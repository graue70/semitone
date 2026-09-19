/*
 * Semitone - tuner, metronome, and piano for Android
 * Copyright (C) 2019  Andy Tockman <andy@tck.mn>
 * Copyright (C) 2026  graue70 <23035329+graue70@users.noreply.github.com>
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

import android.content.Context;
import android.content.res.AssetManager;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;

public class PianoEngine {

    static long handle = 0;
    static boolean paused = true;

    static {
        System.loadLibrary("avutil");
        System.loadLibrary("swresample");
        System.loadLibrary("avcodec");
        System.loadLibrary("avformat");
        System.loadLibrary("semitone-native");
    }

    public static boolean create(Context context) {
        if (handle != 0) return true;

        // use a robust low-performance stream configuration for bluetooth output,
        // where exclusive low-latency streams tend to produce constant underruns
        boolean bluetooth = false;
        AudioManager am = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            int t = d.getType();
            if (t == AudioDeviceInfo.TYPE_BLUETOOTH_A2DP
                    || t == AudioDeviceInfo.TYPE_BLE_HEADSET
                    || t == AudioDeviceInfo.TYPE_BLE_SPEAKER) {
                bluetooth = true;
                break;
            }
        }
        setBluetoothOutput(bluetooth);

        handle = createPianoEngine(context.getAssets());
        if (handle == 0) return false;

        // keep oboe's defaults if the device doesn't report its output
        // configuration
        try {
            setSampleRate(
                    Integer.parseInt(am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)));
            setFramesPerBurst(
                    Integer.parseInt(
                            am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)));
        } catch (NumberFormatException e) {
        }

        paused = false;
        return true;
    }

    public static void destroy() {
        if (handle == 0) return;
        destroyPianoEngine(handle);
        handle = 0;
        paused = true;
    }

    static void pause() {
        paused = true;
        doPause(handle);
    }

    static void resume() {
        paused = false;
        doResume(handle);
    }

    static void play(int pitch, int concert_a) {
        doPlay(handle, pitch, concert_a);
    }

    static void stop(int pitch) {
        doStop(handle, pitch);
    }

    static void playFile(String path, int concert_a) {
        doPlayFile(handle, path, concert_a);
    }

    private static native long createPianoEngine(AssetManager am);

    private static native void destroyPianoEngine(long handle);

    private static native void doPause(long handle);

    private static native void doResume(long handle);

    private static native void setSampleRate(int val);

    private static native void setFramesPerBurst(int val);

    private static native void setBluetoothOutput(boolean val);

    private static native void doPlay(long handle, int pitch, int concert_a);

    private static native void doStop(long handle, int pitch);

    private static native void doPlayFile(long handle, String path, int concert_a);
}
