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

#include "Tone.h"

#include <oboe/Oboe.h>
#include <math.h>

Tone::Tone(int pitch, int concert_a, int sampleRate)
    : pitch(pitch),
      stopped(false),
      phase(0),
      pos(0),
      releaseStart(-1),
      attackSamples(sampleRate * 8 / 1000),
      releaseSamples(sampleRate * 50 / 1000) {
    phaseIncrement = 2 * M_PI * (concert_a * powf(2, (pitch - 69) / 12.0f)) / sampleRate;
    totalSamples = (int)((unsigned)-1 >> 1);
}

bool Tone::finished() const { return pos >= totalSamples; }

float Tone::tick() {
    int p = pos++;
    if (p >= totalSamples) return 0;
    if (releaseStart < 0 && stopped.load(std::memory_order_acquire)) {
        releaseStart = p;
        totalSamples = p + releaseSamples;
    }

    float env;
    if (releaseStart >= 0) {
        env = 1 - (p - releaseStart) / (float)releaseSamples;
        if (env < 0) env = 0;
    } else if (p < attackSamples) {
        env = p / (float)attackSamples;
    } else {
        env = 1;
    }

    float wave =
        sinf(phase) + 0.5f * sinf(2 * phase) + 0.25f * sinf(3 * phase) + 0.125f * sinf(4 * phase);

    phase += phaseIncrement;
    if (phase > 2 * M_PI) phase -= 2 * M_PI;

    return env * wave / (1.f + 0.5f + 0.25f + 0.125f) * 0.35f;
}
