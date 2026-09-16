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

#ifndef __SOUND_H__
#define __SOUND_H__

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <android/asset_manager.h>

// a single playback instance: a shared reference to the decoded pcm data
// plus a play head; the data may be shared with other sounds
class Sound {
   public:
    explicit Sound(std::shared_ptr<const std::vector<float>> pcm);

    std::shared_ptr<const std::vector<float>> data;
    size_t offset;
    std::atomic<bool> stopped;
};

// fully decode an asset into float pcm at the given sample rate; returns
// an empty vector on failure
std::shared_ptr<const std::vector<float>> decodeSound(AAssetManager &am, const char *path,
                                                      int concert_a, int channels, int sampleRate);

#endif
