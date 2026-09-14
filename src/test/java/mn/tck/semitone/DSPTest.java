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

package mn.tck.semitone;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class DSPTest {

    private static void fillSine(double[] buf, double freq, int sr) {
        for (int i = 0; i < buf.length; ++i) buf[i] = Math.sin(2 * Math.PI * freq * i / sr);
    }

    @Test public void initChoosesLargestPowerOfTwoNotOverBufsize() {
        DSP.init(2048);
        assertEquals(2048, DSP.fftlen);
        assertEquals(11, DSP.fftpow);

        // 2560 is not a power of two, so the fft runs on the largest power of
        // two below it
        DSP.init(2560);
        assertEquals(2048, DSP.fftlen);
        assertEquals(11, DSP.fftpow);
    }

    @Test public void freqDetectsPureSines() {
        DSP.init(4096);
        int sr = 44100;
        for (double f : new double[] {220, 330, 440, 880}) {
            double[] buf = new double[DSP.fftlen];
            fillSine(buf, f, sr);
            double detected = DSP.freq(buf, sr);
            assertTrue("detected " + detected + " instead of " + f,
                    Math.abs(detected - f) < 0.01 * f);
        }
    }

    @Test public void freqDetectsNoisySine() {
        DSP.init(4096);
        double[] buf = new double[DSP.fftlen];
        java.util.Random rng = new java.util.Random(42);
        for (int i = 0; i < buf.length; ++i) {
            buf[i] = Math.sin(2 * Math.PI * 440 * i / 44100) + 0.1 * (rng.nextDouble() - 0.5);
        }
        double detected = DSP.freq(buf, 44100);
        assertTrue("detected " + detected, Math.abs(detected - 440) < 0.02 * 440);
    }

    @Test public void freqRejectsSilence() {
        DSP.init(4096);
        double[] buf = new double[DSP.fftlen];
        assertTrue(DSP.freq(buf, 44100) < 0);
    }
}
