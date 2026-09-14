/*
 * Semitone - tuner, metronome, and piano for Android
 * Copyright (C) 2019  Andy Tockman <andy@tck.mn>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Sound.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <oboe/Oboe.h>

extern "C" {
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#define MP3_BLOCKSIZE 1152

#include <android/log.h>
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "semitone", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "semitone", __VA_ARGS__)

// bundle the asset and the original avio buffer, since the avio context
// may swap its internal buffer out from under us
struct AssetIo {
    AAsset *asset;
    uint8_t *origBuf;
};

static int read(void *ptr, uint8_t *buf, int bufsize) {
    return AAsset_read(static_cast<AssetIo *>(ptr)->asset, buf, (size_t)bufsize);
}

static int64_t seek(void *ptr, int64_t offset, int whence) {
    // See https://www.ffmpeg.org/doxygen/3.0/avio_8h.html#a427ff2a881637b47ee7d7f9e368be63f
    AAsset *a = static_cast<AssetIo *>(ptr)->asset;
    if (whence == AVSEEK_SIZE) return AAsset_getLength(a);
    if (AAsset_seek(a, offset, whence) == -1) {
        return -1;
    } else {
        return 0;
    }
}

static void freeAvioContext(AVIOContext *c) {
    AssetIo *io = static_cast<AssetIo *>(c->opaque);
    if (c->buffer != io->origBuf) av_free(io->origBuf);
    av_free(c->buffer);
    avio_context_free(&c);
    delete io;
}

Sound::Sound(std::shared_ptr<const std::vector<float>> pcm)
    : data(std::move(pcm)), offset(0), stopped(false) {}

// on any failure, an empty vector is returned
std::shared_ptr<const std::vector<float>> decodeSound(AAssetManager &am, const char *path,
                                                      int concert_a, int channels, int sampleRate) {
    auto decoded = std::make_shared<std::vector<float>>();
    AAsset *a = AAssetManager_open(&am, path, AASSET_MODE_UNKNOWN);
    if (a == nullptr) {
        LOGW("could not open asset %s", path);
        return decoded;
    }

    // obtain AVIOContext reading straight from the asset (with deleter)
    uint8_t *avioBuf = reinterpret_cast<uint8_t *>(av_malloc(MP3_BLOCKSIZE));
    AssetIo *io = nullptr;
    if (avioBuf != nullptr) {
        io = new (std::nothrow) AssetIo{a, avioBuf};
        if (io == nullptr) av_free(avioBuf);
    }
    if (io == nullptr) {
        LOGE("out of memory reading %s", path);
        AAsset_close(a);
        return decoded;
    }

    std::unique_ptr<AVIOContext, void (*)(AVIOContext *)> ioc{nullptr, &freeAvioContext};
    AVIOContext *iocTmp = avio_alloc_context(avioBuf, MP3_BLOCKSIZE, 0, io, read, nullptr, seek);
    if (iocTmp == nullptr) {
        av_free(avioBuf);
        delete io;
        LOGE("could not allocate avio context for %s", path);
        AAsset_close(a);
        return decoded;
    }
    ioc.reset(iocTmp);

    // obtain AVFormatContext (with deleter)
    std::unique_ptr<AVFormatContext, decltype(&avformat_free_context)> fc{nullptr,
                                                                          &avformat_free_context};
    AVFormatContext *fcTmp = avformat_alloc_context();
    if (fcTmp == nullptr) {
        LOGE("out of memory opening %s", path);
        AAsset_close(a);
        return decoded;
    }
    fcTmp->pb = ioc.get();
    fc.reset(fcTmp);

    // initialize AVFormatContext; on failure, avformat_open_input frees the
    // context itself
    if (avformat_open_input(&fcTmp, "", nullptr, nullptr) != 0) {
        fc.release();
        LOGE("could not open %s", path);
        AAsset_close(a);
        return decoded;
    }
    if (avformat_find_stream_info(fc.get(), nullptr) < 0) {
        LOGE("could not find stream info for %s", path);
        AAsset_close(a);
        return decoded;
    }

    // find stream and codec
    int streamIdx = av_find_best_stream(fc.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIdx < 0) {
        LOGE("no audio stream in %s", path);
        AAsset_close(a);
        return decoded;
    }
    AVStream *stream = fc->streams[streamIdx];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr) {
        LOGE("no decoder for %s", path);
        AAsset_close(a);
        return decoded;
    }

    // obtain AVCodecContext (with deleter)
    std::unique_ptr<AVCodecContext, void (*)(AVCodecContext *)> cc{
        nullptr, [](AVCodecContext *c) { avcodec_free_context(&c); }};
    cc.reset(avcodec_alloc_context3(codec));
    if (!cc || avcodec_parameters_to_context(cc.get(), stream->codecpar) < 0 ||
        avcodec_open2(cc.get(), codec, nullptr) < 0) {
        LOGE("could not open decoder for %s", path);
        AAsset_close(a);
        return decoded;
    }

    // initialize software resampler
    struct SwrDeleter {
        void operator()(SwrContext *s) const { swr_free(&s); }
    };
    std::unique_ptr<SwrContext, SwrDeleter> swr(swr_alloc());
    AVChannelLayout out_chlayout;
    av_channel_layout_default(&out_chlayout, channels);
    if (!swr ||
        av_opt_set_chlayout(swr.get(), "in_chlayout", &stream->codecpar->ch_layout, 0) < 0 ||
        av_opt_set_int(swr.get(), "in_sample_rate",
                       (concert_a / 440.0) * stream->codecpar->sample_rate, 0) < 0 ||
        av_opt_set_int(swr.get(), "in_sample_fmt", stream->codecpar->format, 0) < 0 ||
        av_opt_set_chlayout(swr.get(), "out_chlayout", &out_chlayout, 0) < 0 ||
        av_opt_set_int(swr.get(), "out_sample_rate", sampleRate, 0) < 0 ||
        av_opt_set_sample_fmt(swr.get(), "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0) < 0 ||
        swr_init(swr.get()) < 0) {
        LOGE("could not initialize resampler for %s", path);
        AAsset_close(a);
        return decoded;
    }

    // do the actual decoding; the decoded size isn't known up front, so
    // grow the buffer as needed
    decoded->reserve(AAsset_getLength(a) * 12);
    struct AVPacketDeleter {
        void operator()(AVPacket *p) const { av_packet_free(&p); }
    };
    struct AVFrameDeleter {
        void operator()(AVFrame *f) const { av_frame_free(&f); }
    };
    std::unique_ptr<AVPacket, AVPacketDeleter> packet(av_packet_alloc());
    std::unique_ptr<AVFrame, AVFrameDeleter> frame(av_frame_alloc());
    if (!packet || !frame) {
        LOGE("out of memory decoding %s", path);
        AAsset_close(a);
        return decoded;
    }

    while (av_read_frame(fc.get(), packet.get()) == 0) {
        if (packet->stream_index != streamIdx) {
            av_packet_unref(packet.get());
            continue;
        }
        int ret = avcodec_send_packet(cc.get(), packet.get());
        av_packet_unref(packet.get());
        if (ret < 0) break;
        while (avcodec_receive_frame(cc.get(), frame.get()) == 0) {
            if (frame->sample_rate <= 0) continue;

            // resample
            int32_t samples = (int32_t)av_rescale_rnd(
                swr_get_delay(swr.get(), frame->sample_rate) + frame->nb_samples, sampleRate,
                frame->sample_rate, AV_ROUND_UP);
            if (samples <= 0) continue;
            uint8_t *swrbuf;
            if (av_samples_alloc(&swrbuf, nullptr, channels, samples, AV_SAMPLE_FMT_FLT, 0) < 0)
                break;
            int frame_count = swr_convert(swr.get(), &swrbuf, samples,
                                          (const uint8_t **)frame->data, frame->nb_samples);
            if (frame_count > 0) {
                decoded->insert(decoded->end(), (float *)swrbuf,
                                (float *)swrbuf + frame_count * channels);
            }
            av_freep(&swrbuf);
        }
        av_frame_unref(frame.get());
    }

    if (decoded->empty()) {
        LOGW("no samples decoded from %s", path);
    }
    AAsset_close(a);
    return decoded;
}
