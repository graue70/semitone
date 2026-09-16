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

// smoothstep rolloff applied to extended partials (n > 4) by their own
// frequency: full below 700 Hz, silent above 1200 Hz, so partials fade in
// gradually as notes descend instead of switching on at hard frequency steps
static float knee(float f) {
    float t = (1200.f - f) / 500.f;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return t * t * (3 - 2 * t);
}

Tone::Tone(int pitch, int concert_a, int sampleRate)
    : pitch(pitch),
      stopped(false),
      phase(0),
      pos(0),
      releaseStart(-1),
      attackSamples(sampleRate * 8 / 1000),
      releaseSamples(sampleRate * 50 / 1000) {
    float freq = concert_a * powf(2, (pitch - 69) / 12.0f);
    phaseIncrement = 2 * M_PI * freq / sampleRate;
    totalSamples = (int)((unsigned)-1 >> 1);

    nPartials = freq < 220 ? (int)(1200 / freq) : 4;
    if (nPartials < 4) nPartials = 4;
    if (nPartials > 24) nPartials = 24;

    norm = 0;
    for (int n = 1; n <= nPartials; ++n) {
        float a = n <= 4 ? 1.f / (n * n) : 1.f / n;
        if (n > 4) a *= knee(n * freq);
        amp[n - 1] = a;
        norm += a;
    }

    boost = 250.f / freq;
    if (boost < 1) boost = 1;
    if (boost > 1.6f) boost = 1.6f;
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

    float wave = 0;
    for (int n = 1; n <= nPartials; ++n) wave += amp[n - 1] * sinf(n * phase);

    phase += phaseIncrement;
    if (phase > 2 * M_PI) phase -= 2 * M_PI;

    return env * wave / norm * 0.5f * boost;
}
