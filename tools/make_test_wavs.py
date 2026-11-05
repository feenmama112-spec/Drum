#!/usr/bin/env python3
# ===========================================================================
#  make_test_wavs.py  --  generates a complete, playable 4-kit sample pack
# ===========================================================================
#  Writes 4 kits x 12 samples as RIFF/WAVE PCM 16-bit MONO 22050 Hz - exactly
#  what the firmware loads - into:
#
#      sdcard/DRUM/POP/*.wav
#      sdcard/DRUM/ROCK/*.wav
#      sdcard/DRUM/HIPHOP/*.wav
#      sdcard/DRUM/JAZZ/*.wav
#
#  Copy the whole sdcard/ folder onto a FAT32 microSD card.
#
#  These are synthesised placeholders so the module can be tested end to end
#  on day one; replace them with your own recordings (same name, same format)
#  whenever you like - nothing in the firmware needs to change.
#
#  Usage:  python3 tools/make_test_wavs.py [--out DIR]
# ===========================================================================
import argparse
import math
import os
import random
import struct
import wave

RATE = 22050

# duration (s) - kept at or below the cap the firmware applies (kit_defs.cpp)
SAMPLES = [
    ("SNARE.wav",     0.45, "snare"),
    ("SNARE_RIM.wav", 0.30, "rim"),
    ("TOM_HIGH.wav",  0.40, "tom_high"),
    ("TOM_MID.wav",   0.40, "tom_mid"),
    ("TOM_LOW.wav",   0.45, "tom_low"),
    ("FLOOR_TOM.wav", 0.55, "floor_tom"),
    ("HH_CLOSED.wav", 0.18, "hh_closed"),
    ("HH_HALF.wav",   0.28, "hh_half"),
    ("HH_OPEN.wav",   0.75, "hh_open"),
    ("HH_CHICK.wav",  0.12, "hh_chick"),
    ("CRASH.wav",     0.95, "crash"),
    ("RIDE.wav",      1.15, "ride"),
]

# per-kit character
KITS = {
    "POP":    dict(pitch=1.00, tail=1.00, bright=1.05, tom=210, hh=1.00, seed=1, drive=1.00),
    "ROCK":   dict(pitch=0.94, tail=1.25, bright=1.00, tom=190, hh=0.96, seed=2, drive=1.15),
    "HIPHOP": dict(pitch=0.86, tail=0.85, bright=0.85, tom=165, hh=0.90, seed=3, drive=1.30),
    "JAZZ":   dict(pitch=1.06, tail=1.15, bright=0.95, tom=235, hh=1.08, seed=4, drive=0.92),
}


def synth(kind, dur, cfg, rng):
    """Returns a list of floats in -1..1."""
    n = int(dur * RATE)
    out = [0.0] * n
    pitch = cfg["pitch"]
    tail = cfg["tail"]
    bright = cfg["bright"]

    def decay(t, tau):
        return math.exp(-t * tau)

    prev_noise = 0.0
    phase = 0.0

    for i in range(n):
        t = i / RATE
        nz = rng.uniform(-1.0, 1.0)
        hp = nz - prev_noise            # first difference ~ +6 dB/oct high pass
        prev_noise = nz
        s = 0.0

        if kind == "snare":
            body = math.sin(2 * math.pi * 190 * pitch * t) * decay(t, 26 / tail)
            s = 0.62 * body + 0.55 * bright * decay(t, 20 / tail) * hp + 0.10 * decay(t, 8) * nz
        elif kind == "rim":
            s = (0.55 * math.sin(2 * math.pi * 470 * pitch * t) * decay(t, 60)
                 + 0.45 * decay(t, 55) * hp)
        elif kind in ("tom_high", "tom_mid", "tom_low", "floor_tom"):
            mult = {"tom_high": 1.34, "tom_mid": 1.0, "tom_low": 0.78, "floor_tom": 0.60}[kind]
            f0 = cfg["tom"] * mult
            f = f0 * (0.72 ** (i / n))       # pitch sweep down
            phase += 2 * math.pi * f / RATE
            s = (0.85 * math.sin(phase) * decay(t, 16 / tail)
                 + 0.12 * decay(t, 45) * hp + 0.06 * decay(t, 10) * nz)
        elif kind in ("hh_closed", "hh_half", "hh_open"):
            tau = {"hh_closed": 55, "hh_half": 26, "hh_open": 11}[kind] / tail
            partials = (205.3, 304.4, 369.6, 522.7, 540.0, 800.0)
            metal = sum(math.sin(2 * math.pi * p * cfg["hh"] * pitch * t) for p in partials) / 6.0
            s = (0.75 * hp + 0.35 * metal) * decay(t, tau)
        elif kind == "hh_chick":
            s = (0.6 * hp * decay(t, 45) + 0.7 * math.sin(2 * math.pi * 120 * t) * decay(t, 55))
        elif kind == "crash":
            partials = (348.0, 471.0, 622.0, 815.0, 1041.0, 1290.0)
            metal = sum(math.sin(2 * math.pi * p * pitch * t + p) for p in partials) / 6.0
            s = (0.62 * hp + 0.30 * metal) * decay(t, 4.2 / tail)
        elif kind == "ride":
            bell = math.sin(2 * math.pi * 1050 * pitch * t) * decay(t, 3.2 / tail)
            ping = math.sin(2 * math.pi * 2360 * pitch * t) * decay(t, 7.0 / tail)
            s = 0.55 * bell + 0.18 * ping + 0.45 * hp * decay(t, 5.5 / tail)
        else:
            s = 0.0

        out[i] = s

    # soft clip + normalise to -1 dBFS
    drive = cfg["drive"]
    out = [math.tanh(v * drive * 1.6) for v in out]
    peak = max(1e-9, max(abs(v) for v in out))
    scale = 0.89 / peak
    # short fade-in/out so the file never starts or ends on a click
    fade = min(int(0.002 * RATE), n // 8)
    for i in range(n):
        g = 1.0
        if i < fade:
            g = i / fade
        if i > n - fade:
            g = min(g, (n - i) / fade)
        out[i] *= scale * g
    return out


def write_wav(path, samples):
    frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767)) for v in samples)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(frames)


def main():
    ap = argparse.ArgumentParser()
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument("--out", default=os.path.join(here, "..", "sdcard", "DRUM"))
    args = ap.parse_args()

    root = os.path.abspath(args.out)
    print(f"generating 4 kits x 12 samples in {root}")
    print(f"{'kit':7} {'file':14} {'ms':>6} {'wav B':>8} {'u-law B':>8}")
    print("-" * 48)

    totals = {}
    for kit, cfg in KITS.items():
        os.makedirs(os.path.join(root, kit), exist_ok=True)
        rng = random.Random(cfg["seed"] * 7919)
        kit_wav = kit_ulaw = 0
        kit_ms = 0
        for name, dur, kind in SAMPLES:
            samples = synth(kind, dur, cfg, rng)
            path = os.path.join(root, kit, name)
            write_wav(path, samples)
            wav_bytes = os.path.getsize(path)
            ulaw_bytes = len(samples)          # 1 byte per frame
            kit_wav += wav_bytes
            kit_ulaw += ulaw_bytes
            kit_ms += len(samples) * 1000 // RATE
            print(f"{kit:7} {name:14} {len(samples)*1000//RATE:6d} {wav_bytes:8d} {ulaw_bytes:8d}")
        totals[kit] = (kit_ms, kit_wav, kit_ulaw)
        print(f"{kit:7} {'TOTAL':14} {kit_ms:6d} {kit_wav:8d} {kit_ulaw:8d}")
        print("-" * 48)

    print("\nfirmware RAM needed per kit (8-bit u-law, 22050 Hz):")
    worst = 0
    for kit, (ms, wav, ulaw) in totals.items():
        print(f"  {kit:7} {ms/1000:5.2f} s  {ulaw/1024:7.1f} kB   (WAV on card {wav/1024:7.1f} kB)")
        worst = max(worst, ulaw)
    print(f"\n  worst kit {worst} B vs AUDIO_POOL_BYTES 150000 B -> "
          f"{'FITS' if worst <= 150000 else 'TOO BIG, shorten the samples'}")
    print("\ncopy the 'sdcard' folder to a FAT32 microSD card (4 GB..32 GB is fine)")


if __name__ == "__main__":
    main()
