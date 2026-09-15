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

#include "PianoEngine.h"

#include <math.h>

#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "semitone", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "semitone", __VA_ARGS__)

PianoEngine::PianoEngine(AAssetManager &am) : am(am) { init(); }
PianoEngine::~PianoEngine() { deinit(); }

std::atomic<bool> PianoEngine::bluetoothOutput{false};

void PianoEngine::init() {
    bool bluetooth = bluetoothOutput.load(std::memory_order_relaxed);

    oboe::AudioStreamBuilder asb;
    asb.setChannelCount(1);
    if (bluetooth) {
        // use a robust low-performance stream configuration for bluetooth
        // output, where exclusive low-latency streams tend to produce
        // constant underruns
        asb.setSharingMode(oboe::SharingMode::Shared);
    } else {
        asb.setSharingMode(oboe::SharingMode::Exclusive);
    }
    asb.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    asb.setCallback(this);

    oboe::AudioStream *newStream = nullptr;
    oboe::Result res = asb.openStream(&newStream);
    if (res != oboe::Result::OK || newStream == nullptr) {
        sampleRate.store(oboe::DefaultStreamValues::SampleRate, std::memory_order_relaxed);
        LOGE("could not open output stream: %s", oboe::convertToText(res));
        return;
    }

    sampleRate.store(newStream->getSampleRate(), std::memory_order_relaxed);

    int32_t burst = newStream->getFramesPerBurst();
    newStream->setBufferSizeInFrames(bluetooth ? burst * 2 : burst);
    is16bit = newStream->getFormat() == oboe::AudioFormat::I16;
    if (is16bit)
        buf16 = std::make_unique<float[]>(newStream->getBufferCapacityInFrames() *
                                          newStream->getChannelCount());

    LOGI(
        "output stream: api %s, bt %d, sample rate %d, burst %d, buffer %d, 16bit %d, xruns "
        "supported %d",
        oboe::convertToText(newStream->getAudioApi()), bluetooth, newStream->getSampleRate(), burst,
        newStream->getBufferSizeInFrames(), is16bit, newStream->isXRunCountSupported());

    // publish the stream only after it is fully configured, then start it
    stream.store(newStream, std::memory_order_release);
    newStream->requestStart();
}

void PianoEngine::deinit() {
    // taking the current stream out of the slot also keeps ui threads from
    // touching it while we close it; by the time this returns, the audio
    // callback is no longer running
    oboe::AudioStream *s = stream.exchange(nullptr, std::memory_order_acquire);
    if (s != nullptr) {
        s->requestStop();
        s->close();
    }

    {
        std::lock_guard<std::mutex> lock(tonesLock);
        for (int i = 0; i < MAX_TONES; ++i) {
            Tone *tmp = tones[i].exchange(nullptr, std::memory_order_relaxed);
            delete tmp;
        }
    }
    {
        std::lock_guard<std::mutex> lock(soundsLock);
        for (int i = 0; i < MAX_SOUNDS; ++i) {
            Sound *tmp = sounds[i].exchange(nullptr, std::memory_order_relaxed);
            delete tmp;
        }
    }
}

void PianoEngine::pause() {
    oboe::AudioStream *s = stream.load(std::memory_order_relaxed);
    if (s != nullptr) s->requestPause();

    {
        std::lock_guard<std::mutex> lock(tonesLock);
        for (int i = 0; i < MAX_TONES; ++i) {
            Tone *tmp = tones[i].load(std::memory_order_relaxed);
            if (tmp != nullptr) tmp->stopped.store(true, std::memory_order_relaxed);
        }
    }
    {
        std::lock_guard<std::mutex> lock(soundsLock);
        for (int i = 0; i < MAX_SOUNDS; ++i) {
            Sound *tmp = sounds[i].load(std::memory_order_relaxed);
            if (tmp != nullptr) tmp->stopped.store(true, std::memory_order_relaxed);
        }
    }
}

void PianoEngine::resume() {
    oboe::AudioStream *s = stream.load(std::memory_order_relaxed);
    if (s != nullptr) s->requestStart();
}

void PianoEngine::play(int pitch, int concert_a) {
    mode.store(TONE_MODE, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(tonesLock);
    for (int i = 0; i < MAX_TONES; ++i) {
        if (tones[i].load(std::memory_order_relaxed) == nullptr) {
            tones[i].store(new Tone(pitch, concert_a, sampleRate.load(std::memory_order_relaxed)),
                           std::memory_order_release);
            break;
        }
    }
}

void PianoEngine::stop(int pitch) {
    std::lock_guard<std::mutex> lock(tonesLock);
    for (int i = 0; i < MAX_TONES; ++i) {
        Tone *t = tones[i].load(std::memory_order_relaxed);
        if (t != nullptr && t->pitch == pitch) t->stopped.store(true, std::memory_order_relaxed);
    }
}

void PianoEngine::playFile(const char *path, int concert_a) {
    mode.store(SOUND_MODE, std::memory_order_relaxed);
    // decoding happens without holding the sounds lock so the audio
    // callback is never blocked; the resulting pcm data is cached, so
    // repeated plays of the same sound only allocate a small handle
    std::shared_ptr<const std::vector<float>> pcm = loadSound(path, concert_a);
    if (pcm->empty()) return;

    std::lock_guard<std::mutex> lock(soundsLock);
    for (int i = 0; i < MAX_SOUNDS; ++i) {
        if (sounds[i].load(std::memory_order_relaxed) == nullptr) {
            sounds[i].store(new Sound(std::move(pcm)), std::memory_order_release);
            return;
        }
    }
}

std::shared_ptr<const std::vector<float>> PianoEngine::loadSound(const char *path, int concert_a) {
    int sr = sampleRate.load(std::memory_order_relaxed);
    std::string key =
        std::string(path) + "|" + std::to_string(concert_a) + "|" + std::to_string(sr);

    {
        std::lock_guard<std::mutex> lock(cacheLock);
        auto it = decodedCache.find(key);
        if (it != decodedCache.end()) return it->second;
    }

    std::shared_ptr<const std::vector<float>> pcm = decodeSound(am, path, concert_a, 1, sr);

    {
        std::lock_guard<std::mutex> lock(cacheLock);
        decodedCache.emplace(key, pcm);
    }
    return pcm;
}

oboe::DataCallbackResult PianoEngine::onAudioReady(oboe::AudioStream *stream, void *data,
                                                   int32_t frames) {
    if (++logCounter % 50 == 0 && stream->isXRunCountSupported()) {
        auto xruns = stream->getXRunCount();
        LOGI("xruns after %d callbacks: %d", logCounter, xruns ? xruns.value() : -1);
    }

    float *outBuf = is16bit ? buf16.get() : static_cast<float *>(data);
    int channels = stream->getChannelCount();
    int curMode = mode.load(std::memory_order_relaxed);

    // clean up stopped sounds and tones, and anything left over from the
    // other mode; deletions only ever happen here (under the lock) or in
    // deinit (when the callback is no longer running)
    {
        std::lock_guard<std::mutex> lock(tonesLock);
        for (int i = 0; i < MAX_TONES; ++i) {
            Tone *tmp = tones[i].load(std::memory_order_relaxed);
            if (tmp != nullptr && (tmp->finished() || curMode != TONE_MODE)) {
                tones[i].store(nullptr, std::memory_order_relaxed);
                delete tmp;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(soundsLock);
        for (int i = 0; i < MAX_SOUNDS; ++i) {
            Sound *tmp = sounds[i].load(std::memory_order_relaxed);
            if (tmp != nullptr &&
                (tmp->stopped.load(std::memory_order_relaxed) || curMode != SOUND_MODE)) {
                sounds[i].store(nullptr, std::memory_order_relaxed);
                delete tmp;
            }
        }
    }

    if (curMode == TONE_MODE) {
        int nTones = 0;
        for (int i = 0; i < MAX_TONES; ++i) {
            if (tones[i].load(std::memory_order_acquire) != nullptr) ++nTones;
        }

        for (int i = 0; i < frames; ++i) {
            float thing = 0;
            if (nTones) {
                for (int j = 0; j < MAX_TONES; ++j) {
                    Tone *tmp = tones[j].load(std::memory_order_acquire);
                    if (tmp != nullptr) thing += tmp->tick();
                }
                thing /= nTones;
                // if we simply divide by the number of tones, the difference
                // between one tone and two played simultaneously is too dramatic,
                // so scale single tones far down first and gradually bring them
                // back up
                thing *= 1 - expf(-(nTones - 1) * 0.5f) / 2;
            }
            for (int ch = 0; ch < channels; ++ch) outBuf[i * channels + ch] = thing;
        }
    } else if (curMode == SOUND_MODE) {
        for (int i = 0; i < frames; ++i) {
            float thing = 0;
            for (int j = 0; j < MAX_SOUNDS; ++j) {
                Sound *tmp = sounds[j].load(std::memory_order_acquire);
                if (tmp != nullptr && tmp->offset < tmp->data->size()) {
                    thing += (*tmp->data)[tmp->offset];
                    if (++tmp->offset == tmp->data->size()) {
                        // mark for deletion in the next cleanup pass
                        tmp->stopped.store(true, std::memory_order_relaxed);
                    }
                }
            }
            for (int ch = 0; ch < channels; ++ch) outBuf[i * channels + ch] = thing;
        }
    }

    if (is16bit) oboe::convertFloatToPcm16(outBuf, static_cast<int16_t *>(data), frames * channels);
    return oboe::DataCallbackResult::Continue;
}

void PianoEngine::onErrorAfterClose(oboe::AudioStream *stream, oboe::Result err) {
    LOGE("stream error: %s", oboe::convertToText(err));
    if (err == oboe::Result::ErrorDisconnected && restartLock.try_lock()) {
        deinit();
        init();
        restartLock.unlock();
    }
}
