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

#ifndef __PIANO_ENGINE_H__
#define __PIANO_ENGINE_H__

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <android/asset_manager.h>
#include <oboe/Oboe.h>

#include "Tone.h"
#include "Sound.h"

#define MAX_TONES  100
#define MAX_SOUNDS 200

#define TONE_MODE  1
#define SOUND_MODE 2

class PianoEngine : oboe::AudioStreamCallback {

public:
    explicit PianoEngine(AAssetManager &am);
    ~PianoEngine();

    static std::atomic<bool> bluetoothOutput;
    void play(int pitch, int concert_a);
    void stop(int pitch);
    void pause();
    void resume();
    void playFile(const char *path, int concert_a);

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *stream, void *data, int32_t frames);
    void onErrorAfterClose(oboe::AudioStream *stream, oboe::Result err);

private:
    void init();
    void deinit();
    std::shared_ptr<const std::vector<float>> loadSound(const char *path, int concert_a);

    AAssetManager &am;

    // the stream is opened on the ui thread but also (re)opened from the
    // oboe error callback, so it can be touched from both
    std::atomic<oboe::AudioStream*> stream {nullptr};
    bool is16bit = false;
    std::unique_ptr<float[]> buf16;
    std::atomic<int> sampleRate {0};
    int logCounter = 0;

    // tone/sound slots are shared between the ui thread (which allocates)
    // and the audio callback (which consumes and frees); all shared state
    // is atomic, and pointer changes additionally happen under the
    // respective lock
    std::atomic<Tone*> tones[MAX_TONES] = {};
    std::atomic<Sound*> sounds[MAX_SOUNDS] = {};
    std::atomic<int> mode {TONE_MODE};

    // decoded samples by path/concert pitch/sample rate; sounds reference
    // the buffers by shared_ptr, so playback survives cache eviction
    std::unordered_map<std::string, std::shared_ptr<const std::vector<float>>> decodedCache;
    std::mutex cacheLock;

    std::mutex restartLock, tonesLock, soundsLock;

};

#endif
