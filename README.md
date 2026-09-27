# Semitone - tuner, metronome, and piano for Android

Copyright (C) 2019  Andy Tockman <andy@tck.mn>

Copyright (C) 2026  graue70 <23035329+graue70@users.noreply.github.com>

## License

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

## Fork

This is a maintained fork of [Andy Tockman's Semitone](https://tck.mn/semitone/)
([source code](https://github.com/tckmn/semitone/)), which is no longer
actively developed; its last stable release was in 2021. This fork is developed
and released at [github.com/graue70/semitone](https://github.com/graue70/semitone).

The fork uses a different app ID (`io.github.graue70.semitone`) than the
original (`mn.tck.semitone`), so it installs as a separate app and cannot
update an existing installation of the original.

## Compilation

To compile, first run

```sh
tools/build_ffmpeg.sh
tools/copy_ffmpeg.sh
```

The first command is a custom build script to build ffmpeg with only the
necessary features, and the second command moves the shared object files to the
place where CMake is configured to find them. The script needs the Android NDK
(r28c, 28.2.13676358) via the `ANDROID_NDK` environment variable. Building the
app itself requires a JDK (17 or later) and the Android SDK; see [shell.nix](./shell.nix) for
one way to set these up. Then, to build Semitone, run either of the following
commands:

```sh
./gradlew assembleDebug
./gradlew assembleRelease
```

## Release

To publish a test release of a branch, run

```sh
gh workflow run android.yml --ref <branch>
```

which builds the APK via GitHub Actions and attaches it to a GitHub
prerelease tagged with the app version (e.g. 1.3.2-test.9), so the tag,
APK file name and app version all match. The APKs are signed with a test
keystore stored in the repository secrets (`KEYSTORE_BASE64`,
`KEYSTORE_PASSWORD`, `KEY_ALIAS`), which must be configured before use. The
builds can be installed and kept up to date on a phone using
[Obtainium](https://github.com/ImranR98/Obtainium) by adding this repository
with prereleases enabled.

To publish a stable release, set `versionName` and `versionCode` in
[`build.gradle`](./build.gradle) to the release values, then push a matching
`v<version>` tag (e.g. `v1.4.0`): the workflow checks that tag and
`versionName` match, builds and signs the APK, and creates a GitHub release
with generated notes. Note that `versionCode` needs to be higher than any
previously installed build (test builds use `6 + <run number>`) for the APK
to install as an update.

## Piano samples

The piano samples are cuts of the University of Iowa Musical Instrument Samples
recordings; they can be regenerated with

```sh
tools/make_piano_sample.py <pitch> [--transpose N]
```

which cuts the source recording before the first noise burst in the decaying
tail (some recordings pick up a snare-like noise there, so those samples are
built from a clean neighbouring recording transposed into place) and verifies
the result (no burst, silent tail, correct pitch). Requires ffmpeg on PATH.

## Third-party components

This software uses:

 - libraries from the FFmpeg project, under the GPLv2 or later
 - Material Design icons and the Oboe library, both licensed by Google under
   the Apache 2.0
 - audio provided by the University of Iowa Musical Instrument Samples project
