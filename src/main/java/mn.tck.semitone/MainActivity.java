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

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.Window;
import android.view.View;
import android.view.ViewGroup;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.ImageView;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.preference.PreferenceManager;
import androidx.viewpager2.adapter.FragmentStateAdapter;
import androidx.viewpager2.widget.ViewPager2;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayoutMediator;

public class MainActivity extends FragmentActivity {

    // ImageView fullscreen;
    ImageView settings;

    static TunerFragment tf;
    static MetronomeFragment mf;
    static PianoFragment pf;
    static String tt, mt, pt;

    boolean keeptick;

    private final ActivityResultLauncher<Intent> settingsLauncher =
        registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
            if (tf != null) tf.onSettingsChanged();
            if (mf != null) mf.onSettingsChanged();
            if (pf != null) pf.onSettingsChanged();
            keeptick = PreferenceManager.getDefaultSharedPreferences(this)
                .getBoolean("keeptick", false);
        });

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.activity_main);

        PianoEngine.create(this);
        RecordEngine.create(this);

        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(this);
        SharedPreferences.Editor e = sp.edit();
        if (!sp.contains("concert_a")) e.putString("concert_a", "440");
        if (!sp.contains("keeptick")) e.putBoolean("keeptick", false);
        if (!sp.contains("sustain")) e.putBoolean("sustain", false);
        if (!sp.contains("labelnotes")) e.putBoolean("labelnotes", true);
        if (!sp.contains("labelc")) e.putBoolean("labelc", true);
        e.apply();

        keeptick = sp.getBoolean("keeptick", false);

        tt = getResources().getString(R.string.tuner_title);
        mt = getResources().getString(R.string.metronome_title);
        pt = getResources().getString(R.string.piano_title);

        ViewPager2 pager = (ViewPager2) findViewById(R.id.pager);
        TabLayout tabs = (TabLayout) findViewById(R.id.tabs);
        SemitoneAdapter adapter = new SemitoneAdapter(this);

        pager.setAdapter(adapter);
        new TabLayoutMediator(tabs, pager, (tab, pos) ->
                tab.setText(pos == 0 ? tt : pos == 1 ? mt : pt)).attach();
        pager.setCurrentItem(sp.getInt("lastpage", 0), false);

        pager.registerOnPageChangeCallback(new ViewPager2.OnPageChangeCallback() {
            @Override public void onPageSelected(int pos) {
                if (pos == 0) RecordEngine.resume();
                else RecordEngine.pause();
                e.putInt("lastpage", pos);
                e.apply();
            }
        });

        // fullscreen = (ImageView) findViewById(R.id.fullscreen);
        settings = (ImageView) findViewById(R.id.settings);

        settings.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                settingsLauncher.launch(new Intent(MainActivity.this, SettingsActivity.class));
            }
        });
    }

    @Override protected void onDestroy() {
        super.onDestroy();
        PianoEngine.destroy();
        RecordEngine.destroy();
    }

    @Override protected void onPause() {
        super.onPause();
        if (!keeptick) PianoEngine.pause();
        RecordEngine.pause();
    }

    @Override protected void onResume() {
        super.onResume();
        if (PianoEngine.paused) PianoEngine.resume();
        RecordEngine.resume();
    }

    private static class SemitoneAdapter extends FragmentStateAdapter {
        public SemitoneAdapter(FragmentActivity fa) { super(fa); }
        @Override public int getItemCount() { return 3; }
        @Override public Fragment createFragment(int pos) {
            switch (pos) {
            case 0: return new TunerFragment();
            case 1: return new MetronomeFragment();
            case 2: return new PianoFragment();
            default: return null;
            }
        }
    }

}
