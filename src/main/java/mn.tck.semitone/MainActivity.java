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

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
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
import java.util.ArrayList;

public class MainActivity extends FragmentActivity {

    // live fragments register themselves here (see SemitoneFragment) so
    // settings changes can be pushed to whichever exist
    static final ArrayList<SemitoneFragment> fragments = new ArrayList<>();

    ImageView settings;

    boolean keeptick;

    private final ActivityResultLauncher<Intent> settingsLauncher =
            registerForActivityResult(
                    new ActivityResultContracts.StartActivityForResult(),
                    result -> {
                        for (SemitoneFragment f : fragments) f.onSettingsChanged();
                        keeptick =
                                PreferenceManager.getDefaultSharedPreferences(this)
                                        .getBoolean("keeptick", false);
                    });

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(R.layout.activity_main);

        PianoEngine.create(this);
        RecordEngine.create(this);

        // fill in defaults from settings.xml for anything not set yet
        PreferenceManager.setDefaultValues(this, R.xml.settings, false);

        SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(this);
        keeptick = sp.getBoolean("keeptick", false);

        ViewPager2 pager = (ViewPager2) findViewById(R.id.pager);
        TabLayout tabs = (TabLayout) findViewById(R.id.tabs);
        SemitoneAdapter adapter = new SemitoneAdapter(this);

        pager.setAdapter(adapter);
        new TabLayoutMediator(
                        tabs,
                        pager,
                        (tab, pos) ->
                                tab.setText(
                                        getResources()
                                                .getString(
                                                        pos == 0
                                                                ? R.string.tuner_title
                                                                : pos == 1
                                                                        ? R.string.metronome_title
                                                                        : R.string.piano_title)))
                .attach();
        pager.setCurrentItem(sp.getInt("lastpage", 0), false);

        pager.registerOnPageChangeCallback(
                new ViewPager2.OnPageChangeCallback() {
                    @Override
                    public void onPageSelected(int pos) {
                        if (pos == 0) RecordEngine.resume();
                        else RecordEngine.pause();
                        PreferenceManager.getDefaultSharedPreferences(MainActivity.this)
                                .edit()
                                .putInt("lastpage", pos)
                                .apply();
                    }
                });

        settings = (ImageView) findViewById(R.id.settings);

        settings.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        settingsLauncher.launch(
                                new Intent(MainActivity.this, SettingsActivity.class));
                    }
                });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        PianoEngine.destroy();
        RecordEngine.destroy();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (!keeptick) PianoEngine.pause();
        RecordEngine.pause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (PianoEngine.paused) PianoEngine.resume();
        RecordEngine.resume();
    }

    private static class SemitoneAdapter extends FragmentStateAdapter {
        public SemitoneAdapter(FragmentActivity fa) {
            super(fa);
        }

        @Override
        public int getItemCount() {
            return 3;
        }

        @Override
        public Fragment createFragment(int pos) {
            switch (pos) {
                case 0:
                    return new TunerFragment();
                case 1:
                    return new MetronomeFragment();
                case 2:
                    return new PianoFragment();
                default:
                    return null;
            }
        }
    }
}
