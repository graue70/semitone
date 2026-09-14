#!/usr/bin/env python3

"""
Regenerate a piano sample asset from the University of Iowa Musical
Instrument Samples recording.

Some Iowa piano recordings pick up snare-like noise bursts in the
decaying tail, so a fixed-length cut starting at the strike can include
them (most prominent on high notes; see issue #35). This tool instead
cuts the sample shortly before the first burst and applies a short
fade-out.

Some recordings (such as B5) have no usable decay past the first
burst. For those, --transpose borrows the recording of a neighbouring
note and shifts it into place.

Usage: make_piano_sample.py PITCH [--transpose N] [--out PATH]

Requires Python 3.10+ and ffmpeg on PATH. The downloaded source recording
is cached in /tmp/semitone-samples.
"""

from __future__ import annotations

import argparse
import array
import math
import subprocess
import sys
import tempfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path

SR = 44100
BASE_URL = ("https://theremin.music.uiowa.edu/sound%20files/MIS/Piano_Other/"
            "piano/Piano.ff.{}.aiff")
NAMES = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]

LEAD_IN_S = 0.030       # silence kept before the strike (the app ignores the
                        # mp3 encoder delay, so this also keeps the pre-roll
                        # region silent)
BURST_MARGIN_S = 0.040  # cut ends this long before the first burst onset
FADE_S = 0.070          # cosine fade-out length
MAX_DUR_S = 1.000       # matches the other assets
MIN_DUR_S = 0.200


def note_name(pitch: int) -> str:
    return f"{NAMES[pitch % 12]}{pitch // 12 - 1}"


def ffmpeg_decode(path: Path, channels: int) -> bytes:
    try:
        p = subprocess.run(
            ["ffmpeg", "-v", "error", "-i", path,
             "-f", "f32le", "-ar", str(SR), "-ac", str(channels), "-"],
            capture_output=True, check=True)
    except FileNotFoundError:
        sys.exit("error: ffmpeg not found on PATH")
    return p.stdout


def windows_rms(data: array.array[float], win: int) -> list[float]:
    rms = []
    for i in range(len(data) // win):
        s = 0.0
        for j in range(i * win, (i + 1) * win):
            s += data[j] * data[j]
        rms.append(math.sqrt(s / win))
    return rms


def find_strike(rms10: list[float]) -> int:
    # first window that clearly rises above everything before it
    peak = 0.0
    for i, v in enumerate(rms10):
        if v > 0.04 and v > 2.0 * peak:
            return i
        peak = max(peak, v)
    sys.exit("error: no strike onset found")


def find_burst(env20: list[float], onset10: int, attack_rms: float) -> float | None:
    # first sustained envelope rise (>= 3dB over 100ms earlier, held for
    # 60ms) that is loud enough to be audible in the decayed tail
    o = onset10 // 2
    for i in range(o + 7, min(o + 50, len(env20))):  # 0.15s to 1.0s after the strike
        if env20[i] < 0.126 * attack_rms:
            continue
        if all(env20[j] >= 1.41 * env20[j - 5] for j in range(i, min(i + 3, len(env20)))):
            return i / 50.0
    return None


def cut_and_fade(stereo: array.array[float], start_frame: int, end_frame: int,
                 fade_frames: int) -> array.array[float]:
    seg = array.array("f", stereo[2 * start_frame:2 * end_frame])
    nframes = end_frame - start_frame
    for k in range(fade_frames):
        g = 0.5 * (1.0 + math.cos(math.pi * k / fade_frames))
        j = 2 * (nframes - fade_frames + k)
        seg[j] *= g
        seg[j + 1] *= g
    seg[-1] = 0.0
    seg[-2] = 0.0
    return seg


def transpose(seg: array.array[float], semitones: int) -> array.array[float]:
    # reinterpret the samples at a higher rate, then bring them back to SR
    ratio = 2.0 ** (semitones / 12.0)
    p = subprocess.run(
        ["ffmpeg", "-v", "error",
         "-f", "f32le", "-ar", str(SR), "-ac", "2", "-i", "-",
         "-af", f"asetrate={round(SR * ratio)},aresample={SR}",
         "-f", "f32le", "-ar", str(SR), "-ac", "2", "-"],
        input=seg.tobytes(), capture_output=True, check=True)
    out = array.array("f")
    out.frombytes(p.stdout)
    return out


def encode(seg: array.array[float], out_path: Path) -> None:
    subprocess.run(
        ["ffmpeg", "-v", "error", "-y",
         "-f", "f32le", "-ar", str(SR), "-ac", "2", "-i", "-",
         "-c:a", "libmp3lame", "-b:a", "128k", "-f", "mp3", out_path],
        input=seg.tobytes(), capture_output=True, check=True)


@dataclass
class Verification:
    duration: float
    burst: float | None
    tail: float
    freq: float
    ok: bool


def estimate_freq(data: array.array[float], expected: float) -> float:
    # normalized autocorrelation near the expected pitch; zero crossings are
    # biased high by harmonics
    start = min(int(0.10 * SR), len(data) // 3)
    end = min(int(0.30 * SR), len(data) - 1)
    if end - start < SR // 10:
        return 0.0
    seg = data[start:end]
    n = len(seg)
    lag0 = SR / expected
    best_r, best_lag = 0.0, 0
    for lag in range(int(lag0 * 0.9), int(lag0 * 1.1) + 1):
        s = sum(seg[i] * seg[i + lag] for i in range(n - lag))
        e = sum(v * v for v in seg[lag:])
        r = s / e if e else 0.0
        if r > best_r:
            best_r, best_lag = r, lag
    return SR / best_lag if best_r > 0.5 else 0.0


def verify(path: Path, pitch: int) -> Verification:
    mono = array.array("f")
    mono.frombytes(ffmpeg_decode(path, 1))
    rms10 = windows_rms(mono, SR // 100)
    onset10 = find_strike(rms10)
    env20 = windows_rms(mono, SR // 50)
    attack = max(env20[onset10 // 2:onset10 // 2 + 3])
    burst = find_burst(env20, onset10, attack)
    tail = max(abs(v) for v in mono[-SR // 200:])
    expected = 440.0 * 2.0 ** ((pitch - 69) / 12.0)
    freq = estimate_freq(mono, expected)
    in_tune = abs(freq - expected) < 0.03 * expected
    return Verification(
        duration=len(mono) / SR,
        burst=burst,
        tail=tail,
        freq=freq,
        ok=burst is None and tail < 0.005 and in_tune,
    )


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    ap.add_argument("pitch", type=int, help="midi pitch number (24-108)")
    ap.add_argument("--transpose", type=int, default=0, metavar="N",
                    help="use the recording N semitones lower, transposed up")
    ap.add_argument("--out", help="output mp3 path (default: asset path)")
    args = ap.parse_args()

    root = Path(__file__).resolve().parent.parent
    out = Path(args.out) if args.out else root / "src" / "main" / "assets" / "piano" / f"{args.pitch}.mp3"

    name = note_name(args.pitch - args.transpose)
    cache = Path(tempfile.gettempdir()) / "semitone-samples"
    cache.mkdir(exist_ok=True)
    aiff = cache / f"Piano.ff.{name}.aiff"
    if not aiff.exists():
        url = BASE_URL.format(name)
        print(f"downloading {url}")
        urllib.request.urlretrieve(url, aiff)

    mono = array.array("f")
    mono.frombytes(ffmpeg_decode(aiff, 1))
    stereo = array.array("f")
    stereo.frombytes(ffmpeg_decode(aiff, 2))

    rms10 = windows_rms(mono, SR // 100)
    onset10 = find_strike(rms10)
    onset = onset10 / 100.0

    env20 = windows_rms(mono, SR // 50)
    attack = max(env20[onset10 // 2:onset10 // 2 + 3])
    burst = find_burst(env20, onset10, attack)

    start = max(0, round((onset - LEAD_IN_S) * SR))
    end = round(((burst if burst is not None else onset + MAX_DUR_S) - BURST_MARGIN_S) * SR)
    end = min(end, start + round(MAX_DUR_S * SR), len(stereo) // 2)
    if end - start < MIN_DUR_S * SR:
        sys.exit("error: usable decay after the strike is too short")

    fade = cut_and_fade(stereo, start, end, round(FADE_S * SR))
    if args.transpose:
        fade = transpose(fade, args.transpose)
    encode(fade, out)
    res = verify(out, args.pitch)

    burst_desc = f"{burst:.2f}s" if burst is not None else "none"
    source = f"{note_name(args.pitch - args.transpose)} (transposed {args.transpose:+d})" \
        if args.transpose else note_name(args.pitch)
    print(f"source {source}, strike at {onset:.2f}s, burst at {burst_desc}")
    print(f"cut {start / SR:.2f}s-{end / SR:.2f}s ({(end - start) / SR:.2f}s), "
          f"wrote {out} ({out.stat().st_size} bytes)")
    res_burst = f"{res.burst:.2f}s" if res.burst is not None else "none"
    verdict = "PASS" if res.ok else "FAIL"
    expected = 440.0 * 2.0 ** ((args.pitch - 69) / 12.0)
    print(f"verify: duration {res.duration:.2f}s, tail peak {res.tail:.5f}, "
          f"burst {res_burst}, freq {res.freq:.0f}Hz (expected {expected:.0f}Hz) -> {verdict}")
    sys.exit(0 if res.ok else 1)


if __name__ == "__main__":
    main()
