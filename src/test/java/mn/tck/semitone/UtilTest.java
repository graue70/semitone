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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

import org.junit.Test;

public class UtilTest {

    @Test
    public void getNotenamesClamps() {
        assertSame(Util.notenamesAll[0], Util.getNotenames(-3));
        assertSame(Util.notenamesAll[1], Util.getNotenames(1));
        assertSame(Util.notenamesAll[2], Util.getNotenames(2));
        assertSame(Util.notenamesAll[2], Util.getNotenames(17));
    }

    @Test
    public void widestNamePicksFirstLongest() {
        assertEquals("A#", Util.widestName(new String[] {"A", "A#", "B"}));
        assertEquals("Sol#", Util.widestName(Util.notenamesAll[1]));
        assertEquals("Ais", Util.widestName(Util.notenamesAll[2]));
    }

    @Test
    public void widestWhiteNameIgnoresBlackKeys() {
        // "Ais" sits on a black key in the german naming, so among the white
        // keys nothing is wider than "A"
        assertEquals("A", Util.widestWhiteName(Util.notenamesAll[2]));
        assertEquals("Sol", Util.widestWhiteName(Util.notenamesAll[1]));
    }
}
