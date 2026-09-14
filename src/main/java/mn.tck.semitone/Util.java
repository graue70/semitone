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

import android.content.SharedPreferences;
import android.text.TextPaint;

public class Util {

    public static final String[][] notenamesAll = {
        {"A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"},
        {"La", "La#", "Si", "Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#"},
        {"A", "Ais", "H", "C", "Cis", "D", "Dis", "E", "F", "Fis", "G", "Gis"},
    };
    public static final String[] notenames = notenamesAll[0];

    public static final int[] WHITE_PCS = {0, 2, 3, 5, 7, 8, 10};

    public static String[] getNotenames(int naming) {
        return notenamesAll[Math.max(0, Math.min(naming, notenamesAll.length - 1))];
    }

    public static int naming(SharedPreferences sp) {
        try {
            return Integer.parseInt(sp.getString("notenames", "0"));
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    public static String widestName(String[] names) {
        String widest = "";
        for (String name : names) if (name.length() > widest.length()) widest = name;
        return widest;
    }

    public static String widestWhiteName(String[] names) {
        String widest = "";
        for (int pc : WHITE_PCS) if (names[pc].length() > widest.length()) widest = names[pc];
        return widest;
    }

    public static int maxTextSize(String text, int maxWidth) {
        TextPaint paint = new TextPaint();
        for (int textSize = 10; ; ++textSize) {
            paint.setTextSize(textSize);
            if (paint.measureText(text) > maxWidth) return textSize - 1;
        }
    }
}
