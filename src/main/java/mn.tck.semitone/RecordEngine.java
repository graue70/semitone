package mn.tck.semitone;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import androidx.core.content.ContextCompat;

public class RecordEngine {

    static final int SAMPLE_RATE = 44100;

    static int bufsize;
    static AudioRecord ar;
    static Thread recordThread;

    static volatile Callback cb;

    static volatile boolean paused = true, created = false;

    public static void create(Activity a) {
        if (created) return;

        created =
                ContextCompat.checkSelfPermission(a, Manifest.permission.RECORD_AUDIO)
                        == PackageManager.PERMISSION_GRANTED;
        if (!created) return;

        bufsize =
                AudioRecord.getMinBufferSize(
                        SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
        if (bufsize <= 0) {
            created = false;
            return;
        }

        ar =
                new AudioRecord(
                        AudioSource.MIC,
                        SAMPLE_RATE,
                        AudioFormat.CHANNEL_IN_MONO,
                        AudioFormat.ENCODING_PCM_16BIT,
                        bufsize);
        if (ar.getState() != AudioRecord.STATE_INITIALIZED) {
            ar.release();
            ar = null;
            created = false;
            return;
        }

        DSP.init(bufsize);

        resume();
    }

    public static void destroy() {
        if (!created) return;
        created = false;
        pause();
        if (ar != null) {
            ar.release();
            ar = null;
        }
    }

    public static void pause() {
        if (paused || ar == null) return;
        paused = true;
        ar.stop();
        // the thread notices the stopped record either through the
        // interrupt or through a failing read and exits on its own; we
        // don't join it here since it may be blocked in a read
        if (recordThread != null) recordThread.interrupt();
        recordThread = null;
    }

    public static void resume() {
        if (!paused || !created) return;
        paused = false;
        ar.startRecording();
        recordThread = new Thread(new RecordThread());
        recordThread.start();
    }

    static class RecordThread implements Runnable {
        @Override
        public void run() {
            short[] buf = new short[bufsize];
            AudioRecord localAr = ar;
            while (!Thread.interrupted()) {
                int n = localAr == null ? -1 : localAr.read(buf, 0, bufsize);
                if (n <= 0) break; // stopped or errored
                Callback localCb = cb;
                if (localCb != null) localCb.onRecordUpdate(buf);
            }
        }
    }

    public interface Callback {
        public void onRecordUpdate(short[] buf);
    }
}
