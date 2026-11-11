#!/usr/bin/env python3
"""music2.py — expanded ORIGINAL soundtrack for "BOT MANAGER: A Developer's Life".

Generates seven looping Impulse Tracker (.it) modules plus offline .wav previews:

  title.it    "CARTRIDGE DAWN"        title screen        (upbeat hook)
  school.it   "PENCIL IN WORLD OUT"   chapters 1-2        (light, bouncy)
  bughunt.it  "OFF BY ONE"            chapters 3-4        (driving, tense)
  botfunk.it  "LET THE BOTS COOK"     chapters 5-7        (groovy, funky)
  threeram.it "USAGE WEEK"            chapters 8-10       (moody night synth)
  boss.it     "OVERCLOCKED"           boss fights         (fast, intense)
  credits.it  "WATERYS"               finale / credits    (triumphant)

Every melody, chord, drum pattern and waveform is composed/synthesized here.
No sampled or transcribed commercial music.

Format notes (matches the shipping game's proven conventions):
  * IT sample mode (no instruments), 8-bit signed samples, C5speed-based tuning
    where IT note 60 == MIDI 60 == middle C.
  * 6 music channels (0-5); channels 6-7 stay free for the engine's SFX.
  * Pattern events use the [0x80|ch+1, mask=7, note, sample, volume] encoding
    that tools/music.py already ships with (proven against smconv/snesmod).
  * Order list ends with 255; snesmod loops back to order 0, so each song is
    arranged intro -> A -> B -> ... -> tag and loops as a whole.
  * Envelopes/vibrato are baked into the waveforms (one-shot decays for drums,
    looped vibrato/tremolo for leads and pads) because sample mode has no
    instrument envelopes.

SPC700/ARAM budget: smconv converts samples to BRR (9 bytes per 16 samples).
Each song is verified individually with the bundled smconv; converted module
size must stay under ~58 KB ARAM.

Usage:  python3 music2.py            (writes *.it into this folder, previews/)
        python3 music2.py --no-wav   (skip .wav rendering)
"""
import math
import random
import struct
import sys
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
WAVDIR = HERE / "previews"
SR = 32000          # sample generation rate == BRR native rate
C5 = 261.626        # MIDI 60 frequency
SEMI = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


def u16(b, o, n):
    struct.pack_into("<H", b, o, n)


def u32(b, o, n):
    struct.pack_into("<I", b, o, n)


def m2f(m):
    return C5 * 2.0 ** ((m - 60) / 12.0)


# --------------------------------------------------------------------------
# Melody DSL:  "d5. b4 a4- r f#5"   '.' = +1 row, '-' = +3 rows, base 1 row.
# One row = one 16th note at speed 6. Returns [(row, midi|None, dur)].
# --------------------------------------------------------------------------
def parse_mel(s):
    out = []
    row = 0
    for tok in s.replace("|", " ").split():
        if not tok:
            continue
        i = 1
        sharp = flat = False
        if i < len(tok) and tok[i] == "#":
            sharp, i = True, i + 1
        elif i < len(tok) and tok[i] == "b":
            flat, i = True, i + 1
        if i < len(tok) and tok[i].isdigit():
            i += 1
        pitch, suf = tok[:i], tok[i:]
        assert set(suf) <= {".", "-"}, f"bad token {tok!r}"
        dur = 1 + suf.count(".") + 3 * suf.count("-")
        if pitch[0].lower() == "r":
            out.append((row, None, dur))
        else:
            semi = SEMI[pitch[0].lower()] + sharp - flat
            octv = int(pitch[-1])
            out.append((row, (octv + 1) * 12 + semi, dur))
        row += dur
    return out


# --------------------------------------------------------------------------
# Waveform synthesis. All deterministic. Loop waves return (bytes, c5speed)
# with an exact integer number of cycles in the loop (seamless, BRR-safe).
# --------------------------------------------------------------------------
def _q8(arr, peak=110):
    # remove DC first: asymmetric pulses otherwise bias the waveform,
    # wasting headroom and thumping on note starts (SPC700 has no DC block)
    mean = sum(arr) / len(arr) if arr else 0.0
    arr = [x - mean for x in arr]
    m = max((abs(x) for x in arr), default=1.0) or 1.0
    s = peak / m
    return bytes(int(max(-127, min(127, round(x * s)))) & 255 for x in arr)


def _frac(ph):
    return (ph / (2 * math.pi)) % 1.0


def shape(kind, ph, t, duty=0.5):
    f = _frac(ph)
    if kind == "pulse":
        return 1.0 if f < duty else -1.0
    if kind == "tri":
        return 4.0 * abs(f - 0.5) - 1.0
    if kind == "saw":
        return 2.0 * f - 1.0
    if kind == "sine":
        return math.sin(ph)
    if kind == "organ":
        return (math.sin(ph) + 0.5 * math.sin(2 * ph + 0.4)
                + 0.33 * math.sin(3 * ph + 1.1) + 0.18 * math.sin(4 * ph + 2.0))
    if kind == "softpad":
        return (0.62 * math.sin(ph) + 0.40 * math.sin(2 * ph + 0.7)
                + 0.30 * math.sin(3 * ph + 2.1) + 0.16 * math.sin(5 * ph + 0.9)
                + 0.10 * math.sin(7 * ph + 1.7))
    if kind == "sawbass":
        x = 0.0
        for n in range(1, 9):
            x += math.sin(n * ph + 0.3 * n) / (n ** 1.35)
        x += 0.35 * (1.0 if f < 0.5 else -1.0)
        return x
    raise ValueError(kind)


def make_loop(kind, L, cycles, vib_cycles=0, vib_depth=0.0,
              trem_cycles=0, trem_depth=0.0, duty=0.5):
    """Looped wave: `cycles` nominal cycles of ~261.6 Hz inside L samples.
    Optional baked vibrato (pitch FM) and tremolo (AM), both loop-seamless."""
    f = cycles * SR / L
    vib_rate = vib_cycles * SR / L if vib_cycles else 0.0
    trem_rate = trem_cycles * SR / L if trem_cycles else 0.0
    k = (f * vib_depth / (2 * math.pi * vib_rate)) if vib_cycles else 0.0
    out = []
    for i in range(L):
        t = i / SR
        if vib_cycles:
            ph = 2 * math.pi * (f * t + k * (1 - math.cos(2 * math.pi * vib_rate * t)))
        else:
            ph = 2 * math.pi * f * t
        x = shape(kind, ph, t, duty)
        if trem_cycles:
            x *= 1.0 - trem_depth * (0.5 + 0.5 * math.sin(2 * math.pi * trem_rate * t))
        out.append(x)
    c5 = round(C5 * L / cycles)
    return _q8(out), c5


def make_kick(L=4200):
    out = []
    for i in range(L):
        t = i / SR
        f = 46 + (152 - 46) * math.exp(-t * 34)
        env = math.exp(-t * 9.5)
        out.append(math.sin(2 * math.pi * f * t) * env)
    return _q8(out), SR


def make_snare(L=4400, tone=190, seed=7):
    rng = random.Random(seed)
    out = []
    for i in range(L):
        t = i / SR
        n = rng.uniform(-1, 1) * math.exp(-t * 26)
        body = math.sin(2 * math.pi * tone * t) * math.exp(-t * 40) * 0.7
        hp = math.sin(2 * math.pi * tone * 2.7 * t) * math.exp(-t * 30) * 0.3
        out.append(n * 0.9 + body + hp)
    return _q8(out), SR


def make_hat(L=1500, seed=11, bright=1.0):
    rng = random.Random(seed)
    prev = 0.0
    out = []
    for i in range(L):
        t = i / SR
        n = rng.uniform(-1, 1)
        hp = (n - prev) * bright
        prev = n
        out.append(hp * math.exp(-t * 90))
    return _q8(out), SR


def make_openhat(L=3600, seed=13):
    rng = random.Random(seed)
    prev = 0.0
    out = []
    for i in range(L):
        t = i / SR
        n = rng.uniform(-1, 1)
        hp = (n - prev) * 0.9
        prev = n
        out.append(hp * math.exp(-t * 21))
    return _q8(out), SR


def make_tom(L=3600, f0=240, f1=118):
    out = []
    for i in range(L):
        t = i / SR
        f = f1 + (f0 - f1) * math.exp(-t * 14)
        out.append(math.sin(2 * math.pi * f * t) * math.exp(-t * 11))
    return _q8(out), SR


def make_crash(L=8000, seed=17):
    rng = random.Random(seed)
    prev = 0.0
    out = []
    for i in range(L):
        t = i / SR
        n = rng.uniform(-1, 1)
        hp = (n - prev)
        prev = n
        shimmer = 0.55 + 0.45 * math.sin(2 * math.pi * 3.1 * t)
        taper = min(1.0, (L - i) / 1500.0)   # short fade so the trimmed tail never clicks
        out.append(hp * shimmer * math.exp(-t * 7.0) * taper)
    return _q8(out), SR


def make_pluck(L=6400, ratio=2.0, decay=17.0):
    """FM one-shot: bell/pluck at nominal 261.6 Hz; pitch-shifted by rate."""
    out = []
    for i in range(L):
        t = i / SR
        ph = 2 * math.pi * C5 * t
        mod = math.sin(ratio * ph) * math.exp(-t * decay * 1.6)
        x = math.sin(ph + 1.7 * mod) * math.exp(-t * decay)
        x += 0.4 * math.sin(2 * ph) * math.exp(-t * decay * 2)
        out.append(x)
    return _q8(out), SR


def make_bell(L=11000):
    out = []
    for i in range(L):
        t = i / SR
        ph = 2 * math.pi * C5 * t
        mod = math.sin(3.01 * ph) * math.exp(-t * 9)
        x = math.sin(ph + 2.2 * mod) * math.exp(-t * 5.0)
        x += 0.3 * math.sin(4.07 * 2 * math.pi * C5 * t) * math.exp(-t * 9)
        out.append(x)
    return _q8(out), SR


# --------------------------------------------------------------------------
# Song container: samples, patterns (event lists), orders; IT + WAV writers.
# --------------------------------------------------------------------------
class Song:
    def __init__(self, filename, songname, tempo, speed=6, pans=None, vols=None):
        self.filename = filename
        self.songname = songname
        self.tempo = tempo
        self.speed = speed
        self.pans = pans or [32, 46, 32, 20, 32, 32]
        self.vols = vols or [64] * 6
        self.samples = []          # (name, data, c5, loop, vol)
        self.smap = {}             # name -> 1-based index
        self.patterns = []         # list of list of (ch,row,note,vol,sidx)
        self.orders = []

    # -- samples -----------------------------------------------------------
    def samp(self, name, data_c5, loop, vol=64):
        data, c5 = data_c5
        self.samples.append((name, data, c5, loop, vol))
        self.smap[name] = len(self.samples)
        return name

    # -- patterns ----------------------------------------------------------
    def pat(self):
        self.patterns.append([])
        return len(self.patterns) - 1

    def ev(self, p, ch, row, note, vol, sname):
        self.patterns[p].append((ch, row, note, vol, self.smap[sname]))

    def order(self, plist):
        self.orders = list(plist)

    # -- timing ------------------------------------------------------------
    def rows_per_sec(self):
        return self.tempo * 0.4 / self.speed

    def seconds(self):
        return 64 * len(self.orders) / self.rows_per_sec()

    # -- IT serialization --------------------------------------------------
    def build_it(self):
        pats = []
        for evs in self.patterns:
            byrow = {}
            last = {}
            for ch, row, note, vol, sidx in evs:
                last[(row, ch)] = (ch, note, vol, sidx)     # IT allows ONE event per channel per row: later wins
            self.dropped_dupes = getattr(self, "dropped_dupes", 0) + len(evs) - len(last)
            for (row, ch), ev in last.items():
                byrow.setdefault(row, []).append(ev)
            data = bytearray()
            for row in range(64):
                for ch, note, vol, sidx in sorted(byrow.get(row, [])):
                    # snesmod/SPC limit: playback rate must stay under 128 kHz, else the SPC misbehaves.
                    # Fold too-high notes down an octave (keeps the tune, loses a little brilliance).
                    c5 = self.samples[sidx - 1][2]
                    while note < 120 and c5 * 2 ** ((note - 60) / 12.0) > 124000:
                        note -= 12
                    data += bytes([0x80 | (ch + 1), 7, note, sidx, vol])
                data.append(0)
            pats.append(struct.pack("<HHI", len(data), 64, 0) + bytes(data))
        ns, npat = len(self.samples), len(pats)
        header = bytearray(192)
        header[:4] = b"IMPM"
        name = self.songname.encode()[:25]
        header[4:4 + len(name)] = name
        header[30:32] = bytes([16, 7])
        u16(header, 32, len(self.orders) + 1)
        u16(header, 34, len(self.samples))     # one instrument per sample (snesmod needs instrument mode)
        u16(header, 36, ns)
        u16(header, 38, npat)
        u16(header, 40, 0x0214)
        u16(header, 42, 0x0214)
        u16(header, 44, 1 | 4 | 8)            # flags: stereo, instrument mode, linear slides
        u16(header, 46, 0)                    # special
        header[48:54] = bytes([128, 128, self.speed, self.tempo, 128, 0])  # GV, MV, speed, tempo, sep, PWD (canonical IT offsets)
        pan = bytearray(64)
        for i in range(64):
            pan[i] = self.pans[i] if i < 6 else 128   # 128 = channel off
        header[64:128] = bytes(pan)
        vol = bytearray(64)
        for i in range(6):
            vol[i] = self.vols[i]
        header[128:192] = bytes(vol)
        orders = bytes(self.orders + [255])
        pos = 192 + len(orders) + (ns + ns + npat) * 4
        instrument_offsets = [pos + i * 554 for i in range(ns)]
        pos += ns * 554
        sample_offsets = [pos + i * 80 for i in range(ns)]
        pos += ns * 80
        pattern_offsets = []
        for p in pats:
            pattern_offsets.append(pos)
            pos += len(p)
        sheads, sdatas, iheads = [], [], []
        for idx, (nm, data, c5, loop, svol) in enumerate(self.samples):
            if loop and len(data) % 16:
                # snesmod/BRR: loop length must be a multiple of 16 -> resample, keep pitch via c5speed
                n0 = len(data)
                n1 = max(16, round(n0 / 16) * 16)
                sg = [d - 256 if d > 127 else d for d in data]
                out = []
                for i in range(n1):
                    x = i * n0 / n1
                    a = int(x)
                    fr = x - a
                    out.append(int(round(sg[a % n0] * (1 - fr) + sg[(a + 1) % n0] * fr)) & 255)
                data = bytes(out)
                c5 = int(round(c5 * n1 / n0))
            ih = bytearray(554)
            ih[:4] = b"IMPI"
            dos = nm.upper().replace(" ", "")[:12].encode()
            ih[4:4 + len(dos)] = dos
            ih[17] = 0                       # NNA: note cut
            ih[18] = 0
            ih[19] = 0
            u16(ih, 20, 0)                   # fadeout
            ih[24] = 128                     # global volume
            ih[25] = 32 | 128                # default pan (ignored)
            ih[26] = 0
            ih[27] = 0
            u16(ih, 28, 0x0214)
            ih[30] = 1
            nm25 = nm.upper().encode()[:25]
            ih[32:32 + len(nm25)] = nm25
            ih[60] = 0
            ih[61] = 0xff
            u16(ih, 62, 0xffff)
            for note in range(120):
                ih[64 + note * 2] = note
                ih[64 + note * 2 + 1] = idx + 1
            iheads.append(bytes(ih))
            h = bytearray(80)
            h[:4] = b"IMPS"
            dos = nm.upper().replace(" ", "")[:12].encode()
            h[4:4 + len(dos)] = dos
            h[17:20] = bytes([64, 0x11 if loop else 0x01, svol])
            sname = nm.upper().encode()[:25]
            h[20:20 + len(sname)] = sname
            h[46] = 1
            h[47] = 32
            u32(h, 48, len(data))
            u32(h, 52, 0)
            u32(h, 56, len(data) if loop else 0)
            u32(h, 60, c5)
            u32(h, 72, pos)
            sheads.append(bytes(h))
            sdatas.append(data)
            pos += len(data)
        return (header + orders
                + b"".join(struct.pack("<I", o) for o in instrument_offsets + sample_offsets + pattern_offsets)
                + b"".join(iheads) + b"".join(sheads) + b"".join(pats) + b"".join(sdatas))

    def brr_estimate(self):
        return sum((len(d) + 15) // 16 * 9 for (_, d, _, _, _) in self.samples)

    # -- offline WAV render (preview mixer; monophonic channels) -----------
    def render_wav(self, path):
        from array import array
        rps = self.rows_per_sec()
        row_sec = 1.0 / rps
        nframes = int(round(64 * len(self.orders) * row_sec * SR))
        # global row for each order entry
        order_start = [64 * i for i in range(len(self.orders))]
        # channel event list: (start_frame, end_frame_or_None, sname, note, vol)
        sdata = {nm: (d, c5, loop) for (nm, d, c5, loop, _) in self.samples}
        chevs = [[] for _ in range(6)]
        for oi, pidx in enumerate(self.orders):
            base = order_start[oi]
            for ch, row, note, vol, sidx in self.patterns[pidx]:
                nm = [k for k, v in self.smap.items() if v == sidx][0]
                f0 = int(round((base + row) * row_sec * SR))
                chevs[ch].append((f0, nm, note, vol))
        for ch in chevs:
            ch.sort()
        Lmix = array("f", bytes(4 * nframes))
        Rmix = array("f", bytes(4 * nframes))
        period_cache = {}

        def period(sname, note):
            key = (sname, note)
            hit = period_cache.get(key)
            if hit is not None:
                return hit
            data, c5, loop = sdata[sname]
            rate = c5 * 2.0 ** ((note - 60) / 12.0) / SR
            plen = max(2, int(round(len(data) / rate)))
            arr = array("f", bytes(4 * plen))
            for i in range(plen):
                idx = int(i * rate)
                if idx >= len(data):
                    break
                v = data[idx]
                arr[i] = (v - 256 if v > 127 else v) / 128.0
            period_cache[key] = (arr, loop, rate)
            return period_cache[key]

        for ch in range(6):
            evs = chevs[ch]
            gl = min(1.0, (64 - self.pans[ch]) / 32.0)
            gr = min(1.0, self.pans[ch] / 32.0)
            cv = self.vols[ch] / 64.0
            for ei, (f0, nm, note, vol) in enumerate(evs):
                arr, loop, rate = period(nm, note)
                plen = len(arr)
                f1 = evs[ei + 1][0] if ei + 1 < len(evs) else nframes
                if not loop:
                    f1 = min(f1, f0 + int(plen))
                f1 = min(f1, nframes)
                if f1 <= f0:
                    continue
                g = vol / 64.0 * cv
                s = f0
                reps = (f1 - f0 + plen - 1) // plen
                chunk = arr * reps if reps > 1 else arr
                chunk = chunk[: f1 - f0]
                if abs(g - 1.0) > 1e-9:
                    chunk = array("f", [x * g for x in chunk])
                seg_l = array("f", [a + b * gl for a, b in zip(Lmix[s:f1], chunk)])
                seg_r = array("f", [a + b * gr for a, b in zip(Rmix[s:f1], chunk)])
                Lmix[s:f1] = seg_l
                Rmix[s:f1] = seg_r
        peak = max(max(abs(x) for x in Lmix), max(abs(x) for x in Rmix), 1e-6)
        gn = 0.92 / peak
        with wave.open(str(path), "wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(SR)
            frames = bytearray()
            for i in range(nframes):
                l = max(-1.0, min(1.0, Lmix[i] * gn))
                r = max(-1.0, min(1.0, Rmix[i] * gn))
                frames += struct.pack("<hh", int(l * 32767), int(r * 32767))
            w.writeframes(bytes(frames))


# --------------------------------------------------------------------------
# Arrangement helpers. All write into pattern `p` starting at bar `b0`
# (row b0*16). `chords` = list of (voicing, bass_root) per bar.
# --------------------------------------------------------------------------
def put_melody(S, p, ch, b0, mel, sname, vol, bar=0):
    evs = parse_mel(mel)
    total = sum(d for (_, _, d) in evs)
    assert total == 64, f"melody must be exactly 4 bars/64 rows, got {total}: {mel}"
    for (r, m, dur) in evs:
        if m is not None:
            v = vol + (4 if r % 16 == 0 else 0) - (4 if r % 4 else 0)
            S.ev(p, ch, b0 * 16 + r, m, max(8, min(64, v)), sname)


def put_melody_at(S, p, ch, row0, mel, sname, vol):
    """Same as put_melody but free row offset (for echoes/counter lines)."""
    for (r, m, dur) in parse_mel(mel):
        if m is not None and row0 + r < 64:
            S.ev(p, ch, row0 + r, m, vol, sname)


def put_pad(S, p, ch, b0, chords, sname, vol):
    for bi, (voic, _) in enumerate(chords):
        for j, n in enumerate(sorted(set(voic))[:4]):
            S.ev(p, ch, b0 * 16 + bi * 16 + j, n, vol - j, sname)


def put_bass(S, p, ch, b0, chords, sname, vol, style):
    for bi, (_, root) in enumerate(chords):
        R, F, O = root, root + 7, root + 12
        base = b0 * 16 + bi * 16
        rows = []
        if style == "drive8":
            rows = [(0, R, vol), (2, R, vol - 8), (4, R, vol - 4), (6, F, vol - 6),
                    (8, R, vol), (10, R, vol - 8), (12, F, vol - 4), (14, R, vol - 6)]
        elif style == "bounce8":
            rows = [(0, R, vol), (2, O, vol - 12), (4, R, vol - 4), (6, F, vol - 10),
                    (8, R, vol), (10, O, vol - 12), (12, F, vol - 4), (14, R, vol - 8)]
        elif style == "gallop16":
            for r in range(16):
                if r % 16 in (7, 15):
                    continue
                rows.append((r, R if r % 2 == 0 else O, vol if r % 4 == 0 else vol - 8))
        elif style == "funk":
            rows = [(0, R, vol), (3, R, vol - 6), (6, F, vol - 4), (10, R, vol),
                    (11, O, vol - 10), (14, F, vol - 4)]
        elif style == "night":
            rows = [(0, R, vol), (8, F, vol - 6)]
        elif style == "walk8":
            rows = [(0, R, vol), (2, R, vol - 6), (4, F, vol - 2), (6, R, vol - 6),
                    (8, O, vol - 2), (10, R, vol - 6), (12, F, vol - 2), (14, R + 2, vol - 6)]
        elif style == "pulse16":
            for r in range(16):
                rows.append((r, R if r % 2 == 0 else O, vol if r % 4 == 0 else vol - 10))
        else:
            raise ValueError(style)
        for (r, n, v) in rows:
            S.ev(p, ch, base + r, n, v, sname)


def put_arp(S, p, ch, b0, chords, sname, vol, style):
    for bi, (voic, _) in enumerate(chords):
        v4 = sorted(set(voic))[:4]
        base = b0 * 16 + bi * 16
        if style == "16up":
            seq = v4 + [v4[1] + 12, v4[2] + 12, v4[3] + 12, v4[2], v4[1]]
            for r in range(16):
                S.ev(p, ch, base + r, seq[r % len(seq)], vol if r % 4 == 0 else vol - 8, sname)
        elif style == "updown":
            pat = [0, 1, 2, 3, 2, 1]
            for r in range(16):
                n = v4[pat[r % 6]] + (12 if (r % 12) >= 6 else 0)
                S.ev(p, ch, base + r, n, vol if r % 4 == 0 else vol - 6, sname)
        elif style == "stab":
            for i, r in enumerate((2, 6, 10, 14)):
                S.ev(p, ch, base + r, v4[(bi * 2 + i) % len(v4)] + 12, vol, sname)
        elif style == "8th":
            pat = [0, 1, 2, 3, 2, 1]
            for i, r in enumerate(range(0, 16, 2)):
                S.ev(p, ch, base + r, v4[pat[i % 6]], vol if r % 4 == 0 else vol - 6, sname)
        elif style == "shimmer":
            seq = [v4[0] + 24, v4[1] + 24, v4[2] + 24, v4[3] + 24]
            for r in range(0, 16, 2):
                S.ev(p, ch, base + r, seq[(r // 2) % 4], vol, sname)
        else:
            raise ValueError(style)


DRUM_MAP = {
    "K": ("kick", 58, 60, 4),
    "S": ("snare", 52, 60, 5),
    "s": ("snare", 24, 60, 5),
    "h": ("hat", 30, 60, 5),
    "H": ("hat", 44, 60, 5),
    "o": ("openhat", 40, 60, 5),
    "T": ("tom", 46, 60, 4),
    "t": ("tom", 38, 55, 4),
    "C": ("crash", 44, 60, 5),
}


def put_drums(S, p, b0, bars, volmul=1.0):
    """bars: list of 16-char strings; chars per DRUM_MAP; '.' = rest."""
    for bi, bs in enumerate(bars):
        assert len(bs) == 16, bs
        for r, c in enumerate(bs):
            if c == "." or c not in DRUM_MAP:
                continue
            sname, vol, note, ch = DRUM_MAP[c]
            if sname in S.smap:
                S.ev(p, ch, b0 * 16 + bi * 16 + r, note,
                     max(8, min(64, int(vol * volmul))), sname)


# --------------------------------------------------------------------------
# Standard drum kit per song (varied tuning/seed so songs feel distinct).
# --------------------------------------------------------------------------
def kit(S, snare_tone=190, seed=7, crash=False, openhat=True, tom=True):
    S.samp("kick", make_kick(), False)
    S.samp("snare", make_snare(tone=snare_tone, seed=seed), False)
    S.samp("hat", make_hat(seed=seed + 4), False)
    if openhat:
        S.samp("openhat", make_openhat(seed=seed + 6), False)
    if tom:
        S.samp("tom", make_tom(), False)
    if crash:
        S.samp("crash", make_crash(seed=seed + 10), False)


# --------------------------------------------------------------------------
# 1. TITLE — "CARTRIDGE DAWN" — D major, 150 BPM
# --------------------------------------------------------------------------
def build_title():
    S = Song("title.it", "CARTRIDGE DAWN", 150,
             pans=[32, 48, 32, 18, 32, 32])
    S.samp("lead", make_loop("pulse", 11300, 92, vib_cycles=2, vib_depth=0.006, duty=0.45), True)
    S.samp("spark", make_pluck(ratio=2.0, decay=16), False)
    S.samp("bass", make_loop("sawbass", 731, 6), True)
    S.samp("pad", make_loop("softpad", 974, 8), True)
    kit(S, snare_tone=200, seed=21, crash=True)
    D = ([50, 57, 62, 66], 38)
    A = ([52, 57, 61, 64], 33)
    Bm = ([47, 54, 59, 62], 35)
    G = ([43, 50, 55, 59], 31)
    mels = [
        # A1 (D A Bm G)
        "d5. b4 a4 f#4 a4. d5. r a4 b4 r- |"
        "c#5. a4 c#5 e5. c#5. a4 e5. f#5 r... |"
        "b4. f#5 d5. b4 f#4. b4 d5. f#5 r- |"
        "g4. b4 d5 g5 f#5. d5 b4. a4 r.-",
        # A2 variation (same head, different tail)
        "d5. b4 a4 f#4 a4. d5. r a4 b4 r- |"
        "c#5. a4 c#5 e5. c#5. e5 a5. g5 r- |"
        "f#5. d5 b4. f#5 a5. f#5 d5. b4 r- |"
        "a4. b4 d5 f#5 e5. d5 b4. r. a4 b4 c#5.",
        # B section (Bm G A A - lift)
        "b4. d5 f#5 b5. f#5 d5. b4 d5. r- |"
        "g4. b4 d5 g5. d5 b4. g4 b4. r- |"
        "a4. c#5 e5 a5. e5 c#5. a4 e5. r- |"
        "c#5. e5 a5. c#6 b5. a5 f#5. e5 f#5. r.",
        # tag (D A Bm D) - big finish
        "d5. f#5 a5. d6 c#6. b5 a5. f#5 r- |"
        "e5. f#5 a5. c#6 b5. a5 f#5. e5 r- |"
        "d5. b4 d5 f#5 b5. a5 f#5. d5 r.- |"
        "a5 f#5 d5.----",
    ]
    # intro: pad+arp build, drums enter bar 3
    pi = S.pat()
    put_pad(S, pi, 3, 0, [D, A, Bm, G], "pad", 30)
    put_arp(S, pi, 1, 0, [D, A, Bm, G], "spark", 34, "updown")
    put_bass(S, pi, 2, 0, [D, A], "bass", 44, "drive8")
    put_drums(S, pi, 2, ["K...K...K...K...", "K.S.K.S.K.S.TtTt"])
    # A patterns
    progA = [D, A, Bm, G]
    pa = []
    for mi in (0, 1):
        p = S.pat()
        pa.append(p)
        put_melody(S, p, 0, 0, mels[mi], "lead", 50)
        put_pad(S, p, 3, 0, progA, "pad", 26)
        put_arp(S, p, 1, 0, progA, "spark", 30, "updown")
        put_bass(S, p, 2, 0, progA, "bass", 50, "drive8")
        put_drums(S, p, 0, ["Ch..h.S.h...h.h.", "K...h.S.h.h.h.ho",
                            "K...h.S.h...h.h.", "K...h.S.h.h.TtTo"])
    # B patterns
    progB = [Bm, G, A, A]
    pb = []
    for half in (0, 1):
        p = S.pat()
        pb.append(p)
        put_melody(S, p, 0, 0, mels[2], "lead", 48)
        put_pad(S, p, 3, 0, progB, "pad", 28)
        if half == 0:
            put_arp(S, p, 1, 0, progB, "spark", 28, "16up")
        else:
            put_arp(S, p, 1, 0, progB, "spark", 34, "updown")
        put_bass(S, p, 2, 0, progB, "bass", 50, "drive8")
        put_drums(S, p, 0, ["K...h.S.h...h.h.", "K...h.S.h.h.h.ho",
                            "K...h.S.h...h.h.", "K...h.S.h.h.TtTo"], volmul=1.0)
    # tag
    pt = S.pat()
    put_melody(S, pt, 0, 0, mels[3], "lead", 52)
    put_pad(S, pt, 3, 0, [D, A, Bm, D], "pad", 30)
    put_arp(S, pt, 1, 0, [D, A, Bm, D], "spark", 30, "updown")
    put_bass(S, pt, 2, 0, [D, A, Bm, D], "bass", 50, "drive8")
    put_drums(S, pt, 0, ["Ch..h.S.h...h.h.", "K...h.S.h.h.h.ho",
                         "K...h.S.h...h.h.", "K...T.t.TtTt...."])
    S.order([pi, pa[0], pa[1], pb[0], pb[1], pa[0], pa[1], pt])
    return S


# --------------------------------------------------------------------------
# 2. SCHOOL (ch 1-2) — "PENCIL IN WORLD OUT" — C major, 128 BPM
# --------------------------------------------------------------------------
def build_school():
    S = Song("school.it", "PENCIL IN WORLD OUT", 128,
             pans=[32, 44, 32, 20, 32, 32])
    S.samp("lead", make_pluck(ratio=2.0, decay=11), False)
    S.samp("keys", make_loop("pulse", 611, 5, duty=0.30), True)
    S.samp("bass", make_loop("tri", 489, 4), True)
    S.samp("pad", make_loop("softpad", 853, 7), True)
    kit(S, snare_tone=210, seed=31, crash=False)
    C = ([48, 55, 60, 64], 36)
    Am = ([52, 57, 60, 64], 33)
    F = ([53, 57, 60, 65], 29)
    Gc = ([50, 55, 59, 62], 31)
    mels = [
        "e5 c5 g4. c5 e5. g5. e5 c5 d5 b4 c5 r. |"
        "c5 a4 e4. a4 c5. e5. c5 a4 b4 g4 a4 r. |"
        "f5. a5 f5. c5 f5. a5 c6. a5 f5. c5. |"
        "g4 b4 d5. g5 f#5. d5 b4. g4 b4 d5 c5 r.",
        "e5 c5 g4. c5 e5. g5. a5 g5 e5 c5 d5. r |"
        "c5 a4 e4. a4 c5. e5. d5 c5 a4 g4 a4. r |"
        "f5. a5 f5. c5 f5. a5 c6. a5 f5. c5. |"
        "d5. b4 c5----",
        "g5. e5 c5. e5 g5. c6 b5. g5 e5. c5. |"
        "a5. f5 c5. f5 a5. c6 d6. b5 g5. e5. |"
        "c6. g5 e5. g5 c6. e6 d6. c6 b5. g5. |"
        "c6. d6 c6. g5 c5---",
    ]
    pi = S.pat()
    put_pad(S, pi, 3, 0, [C, Am, F, Gc], "pad", 26)
    put_arp(S, pi, 1, 0, [C, Am, F, Gc], "keys", 30, "stab")
    put_bass(S, pi, 2, 0, [C, Am, F, Gc], "bass", 44, "bounce8")
    put_drums(S, pi, 0, ["................", "................",
                         "K...h.S.h...h.h.", "K...h.S.h.h.TtTo"])
    pa = []
    for mi, prog in ((0, [C, Am, F, Gc]), (1, [C, Am, F, Gc])):
        p = S.pat()
        pa.append(p)
        put_melody(S, p, 0, 0, mels[mi], "lead", 50)
        put_pad(S, p, 3, 0, prog, "pad", 22)
        put_arp(S, p, 1, 0, prog, "keys", 32, "stab")
        put_bass(S, p, 2, 0, prog, "bass", 48, "bounce8")
        put_drums(S, p, 0, ["K...h.S.h...h.h.", "K...h.S.h.h.h.ho",
                            "K...h.S.h...h.h.", "K...h.S.h.h.TtTo"])
    pb = S.pat()
    put_melody(S, pb, 0, 0, mels[1], "lead", 50)
    put_pad(S, pb, 3, 0, [F, Gc, C, Am], "pad", 24)
    put_arp(S, pb, 1, 0, [F, Gc, C, Am], "keys", 30, "8th")
    put_bass(S, pb, 2, 0, [F, Gc, C, Am], "bass", 48, "bounce8")
    put_drums(S, pb, 0, ["K...h.S.h...h.h.", "K...h.S.h.h.h.ho",
                         "K...h.S.h...h.h.", "K...h.S.h.h.TtTo"])
    pc = S.pat()
    put_melody(S, pc, 0, 0, mels[2], "lead", 52)
    put_pad(S, pc, 3, 0, [F, Gc, C, C], "pad", 26)
    put_arp(S, pc, 1, 0, [F, Gc, C, C], "keys", 32, "stab")
    put_bass(S, pc, 2, 0, [F, Gc, C, C], "bass", 48, "bounce8")
    put_drums(S, pc, 0, ["K...h.S.h...h.h.", "K...h.S.h.h.h.ho",
                         "K...h.S.h...h.h.", "K.h.T.T.t.TtTt.."])
    S.order([pi, pa[0], pa[1], pb, pa[0], pa[1], pc])
    return S


# --------------------------------------------------------------------------
# 3. BUG HUNT (ch 3-4) — "OFF BY ONE" — E minor, 152 BPM
# --------------------------------------------------------------------------
def build_bughunt():
    S = Song("bughunt.it", "OFF BY ONE", 152,
             pans=[32, 46, 32, 18, 32, 32])
    S.samp("lead", make_loop("pulse", 9740, 79, vib_cycles=2, vib_depth=0.008, duty=0.35), True)
    S.samp("buzz", make_loop("saw", 366, 3), True)
    S.samp("bass", make_loop("sawbass", 610, 5), True)
    S.samp("pad", make_loop("softpad", 1097, 9), True)
    kit(S, snare_tone=175, seed=41, crash=True)
    Em = ([47, 52, 55, 59], 28)
    Cc = ([48, 55, 60, 64], 36)
    Dd = ([50, 57, 62, 65], 38)
    Am = ([52, 57, 60, 64], 33)
    Bt = ([47, 54, 59, 63], 35)
    mels = [
        "e5. e5 b4. e5 g5 f#5 e5 d5 e5. r b4 d5. |"
        "c5. c5 g4. c5 e5 d5 c5 b4 c5. r g4 b4. |"
        "e5. e5 b4. e5 g5 f#5 e5 d5 e5 g5 b5. g5. |"
        "f#5. d5 b4. f#5 a4 b4 d5 f#5 b4. r e5 f#5.",
        "e5 g5 b5. g5 e5 d5 b4. e5 g5 a5 b5. g5. |"
        "c5 e5 g5. e5 c5 b4 g4. c5 e5 f#5 g5. e5. |"
        "d5 f#5 a5. f#5 d5 c#5 a4. c#5 e5 f#5 g5. e5. |"
        "b4 d5 e5.----",
        "a4. c5 e5 a5 g5. e5 c5. a4 e5. c5.. |"
        "e5. g5 b5 e6 d6. b5 g5. e5 b5. g5.. |"
        "b5. f#5 d#5 f#5 b5. d#6 b5. f#5 d#5. b4.. |"
        "f#5 g5 a5 b5----",
    ]
    progA = [Em, Cc, Em, Dd]
    progB = [Am, Cc, Bt, Em]
    pi = S.pat()
    put_pad(S, pi, 3, 0, [Em, Em, Cc, Dd], "pad", 26)
    put_arp(S, pi, 1, 0, [Em, Em, Cc, Dd], "buzz", 24, "16up")
    put_bass(S, pi, 2, 0, [Em, Em], "bass", 46, "gallop16")
    put_drums(S, pi, 2, ["K..hK.h.SK..hK.h", "K..hK.h.SK.hTtTt"])
    pa = []
    for mi in (0, 1):
        p = S.pat()
        pa.append(p)
        put_melody(S, p, 0, 0, mels[mi], "lead", 48)
        put_pad(S, p, 3, 0, progA, "pad", 20)
        put_arp(S, p, 1, 0, progA, "buzz", 26, "16up")
        put_bass(S, p, 2, 0, progA, "bass", 52, "gallop16")
        put_drums(S, p, 0, ["Ch.hK.h.S.h.hK.h", "K.h.K.h.S.h.h.ho",
                            "K.h.K.h.S.h.hK.h", "K.h.K.h.S.h.hTtT"])
    pb = []
    for mi in (2, 0):
        p = S.pat()
        pb.append(p)
        put_melody(S, p, 0, 0, mels[2] if mi == 2 else mels[0], "lead", 46)
        put_pad(S, p, 3, 0, progB, "pad", 22)
        put_arp(S, p, 1, 0, progB, "buzz", 28, "updown")
        put_bass(S, p, 2, 0, progB, "bass", 52, "gallop16")
        put_drums(S, p, 0, ["K.h.K.h.S.h.hK.h", "K.h.K.h.S.h.h.ho",
                            "K.h.K.h.S.h.hK.h", "K.h.K.h.S.T.TtTt"])
    pt = S.pat()
    put_melody(S, pt, 0, 0, mels[1], "lead", 50)
    put_pad(S, pt, 3, 0, [Em, Cc, Bt, Em], "pad", 24)
    put_arp(S, pt, 1, 0, [Em, Cc, Bt, Em], "buzz", 30, "16up")
    put_bass(S, pt, 2, 0, [Em, Cc, Bt, Em], "bass", 54, "pulse16")
    put_drums(S, pt, 0, ["Ch.hK.h.S.h.hK.h", "K.h.K.h.S.h.h.ho",
                         "K.h.K.h.S.h.hK.h", "K.T.K.T.t.TtTt.."])
    S.order([pi, pa[0], pa[1], pb[0], pb[1], pa[0], pa[1], pt])
    return S


# --------------------------------------------------------------------------
# 4. BOT FUNK (ch 5-7) — "LET THE BOTS COOK" — A minor, 108 BPM
# --------------------------------------------------------------------------
def build_botfunk():
    S = Song("botfunk.it", "LET THE BOTS COOK", 108,
             pans=[32, 50, 32, 16, 32, 32])
    S.samp("clav", make_loop("pulse", 611, 5, trem_cycles=1, trem_depth=0.25, duty=0.22), True)
    S.samp("wobble", make_loop("pulse", 7940, 65, trem_cycles=2, trem_depth=0.35, duty=0.5), True)
    S.samp("bass", make_loop("sawbass", 853, 7), True)
    S.samp("pad", make_loop("softpad", 1220, 10), True)
    kit(S, snare_tone=165, seed=57, crash=False)
    Am7 = ([52, 55, 60, 64], 33)
    Em7 = ([52, 55, 59, 64], 28)
    D9 = ([50, 55, 57, 62], 38)
    Fmaj = ([53, 57, 60, 64], 29)
    E7 = ([52, 56, 59, 62], 28)
    mels = [
        "a4 c5. b4 a4. r c5 b4. a4 r. e4. g4 |"
        "e5. r d5 c5. r b4 a4. r c5. b4 a4. |"
        "a4 c5. b4 a4. r c5 b4. a4 r. e4. g4 |"
        "c5 d5 e5. c5 a4.---",
        "e5. e5 d5 c5 d5. e5 g5. e5 d5 c5 r.. |"
        "a4. c5 e5 a5 g5. e5 d5. c5 b4 a4 r.. |"
        "c5. e5 g5 e5 c5. e5 a5. g5 e5 d5 r.. |"
        "e5 d5 c5 a4----",
        "a5. g5 e5. c5 d5. e5 c5. a4 e5. g5. |"
        "c6 a5. g5 e5. a5--- |"
        "e5. g5 a5 c6 b5. a5 g5. e5 d5. e5.. |"
        "e5 g5 a5.----",
    ]
    progA = [Am7, Em7, Am7, Fmaj]
    progB = [Am7, D9, Am7, E7]
    pi = S.pat()
    put_bass(S, pi, 2, 0, [Am7, Am7], "bass", 48, "funk")
    put_drums(S, pi, 0, ["K.....K...K.....", "K.....K...K...s.",
                         "K..hK.h.S.h.hK.h", "K..hK.h.S.h.hoTo"])
    put_arp(S, pi, 1, 2, [Am7, Em7], "clav", 28, "stab")
    pa = []
    for mi in (0, 1):
        p = S.pat()
        pa.append(p)
        put_melody(S, p, 0, 0, mels[mi], "clav", 48)
        put_arp(S, p, 1, 0, progA, "wobble", 30, "stab")
        put_bass(S, p, 2, 0, progA, "bass", 52, "funk")
        put_pad(S, p, 3, 0, progA, "pad", 18)
        put_drums(S, p, 0, ["Kh.hK.h.S.h.hK.h", "Kh.hK.h.S.h.h.ho",
                            "Kh.hK.h.S.h.hK.h", "Kh.hK.h.S.h.hoTo"])
    pb = []
    for mi in (2, 1):
        p = S.pat()
        pb.append(p)
        put_melody(S, p, 0, 0, mels[2] if mi == 2 else mels[1], "clav", 46)
        put_arp(S, p, 1, 0, progB, "wobble", 32, "stab")
        put_bass(S, p, 2, 0, progB, "bass", 52, "funk")
        put_pad(S, p, 3, 0, progB, "pad", 20)
        put_drums(S, p, 0, ["Kh.hK.h.S.h.hK.h", "Kh.hK.h.S.h.h.ho",
                            "Kh.hK.h.S.h.hK.h", "Kh.hK.h.S.h.ToTo"])
    pt = S.pat()
    put_melody(S, pt, 0, 0, mels[0], "clav", 50)
    put_arp(S, pt, 1, 0, [Am7, D9, Am7, Am7], "wobble", 32, "stab")
    put_bass(S, pt, 2, 0, [Am7, D9, Am7, Am7], "bass", 52, "funk")
    put_pad(S, pt, 3, 0, [Am7, D9, Am7, Am7], "pad", 20)
    put_drums(S, pt, 0, ["Kh.hK.h.S.h.hK.h", "Kh.hK.h.S.h.h.ho",
                         "K..hK.h.S.h.hK.h", "K.h.T.t.S.TtTt.."])
    S.order([pi, pa[0], pa[1], pb[0], pb[1], pa[0], pa[1], pt])
    return S


# --------------------------------------------------------------------------
# 5. 3AM / USAGE WEEK (ch 8-10) — F# minor, 88 BPM
# --------------------------------------------------------------------------
def build_threeram():
    S = Song("threeram.it", "USAGE WEEK", 88,
             pans=[32, 44, 32, 20, 32, 32])
    S.samp("lead", make_loop("pulse", 8530, 70, vib_cycles=2, vib_depth=0.005, duty=0.5), True)
    S.samp("echo", make_loop("tri", 610, 5), True)
    S.samp("shim", make_loop("sine", 489, 4), True)
    S.samp("bass", make_loop("tri", 974, 8), True)
    S.samp("pad", make_loop("softpad", 12200, 100, trem_cycles=2, trem_depth=0.22), True)
    kit(S, snare_tone=150, seed=67, crash=False, tom=True, openhat=True)
    Fsm = ([49, 54, 57, 61], 30)
    Dd = ([50, 57, 62, 66], 26)
    Aa = ([52, 57, 61, 64], 33)
    Ee = ([52, 59, 64, 68], 28)
    Bm = ([47, 54, 59, 62], 35)
    Csm = ([49, 56, 61, 64], 37)
    mels = [
        "c#5..- a4- f#4- r. |"
        "d5-- a4- f#4- r |"
        "e5..- c#5- a4- r. |"
        "c#5-- b4- a4- b4",
        "f#5..- e5- c#5- r. |"
        "d5-- c#5- b4- r |"
        "c#5-- e5 b4-- r |"
        "c#5- b4- a4- g#4-",
    ]
    progA = [Fsm, Dd, Aa, Ee]
    progB = [Bm, Csm, Fsm, Csm]
    pi = S.pat()
    put_pad(S, pi, 3, 0, [Fsm, Fsm, Dd, Aa], "pad", 26)
    put_arp(S, pi, 1, 0, [Fsm, Fsm, Dd, Aa], "shim", 18, "shimmer")
    put_bass(S, pi, 2, 0, [Fsm, Fsm], "bass", 40, "night")
    put_melody(S, pi, 0, 0, mels[0], "lead", 40)
    pa = []
    for rep in (0, 1):
        p = S.pat()
        pa.append(p)
        put_melody(S, p, 0, 0, mels[rep], "lead", 44)
        put_melody_at(S, p, 1, 16, mels[rep].split("|")[0], "echo", 20)
        put_pad(S, p, 3, 0, progA, "pad", 26)
        put_arp(S, p, 1, 0, progA, "shim", 16, "shimmer")
        put_bass(S, p, 2, 0, progA, "bass", 44, "night")
        put_drums(S, p, 0, ["K.......K.......", "K...h...S...h...",
                            "K.......K.......", "K...h...S...h.o."], volmul=0.8)
    pb = S.pat()
    put_melody(S, pb, 0, 0, mels[1], "lead", 44)
    put_pad(S, pb, 3, 0, progB, "pad", 28)
    put_arp(S, pb, 1, 0, progB, "shim", 18, "shimmer")
    put_bass(S, pb, 2, 0, progB, "bass", 44, "night")
    put_drums(S, pb, 0, ["K.......K.......", "K...h...S...h...",
                         "K.......K...t...", "K...h...S...h.o."], volmul=0.8)
    pt = S.pat()
    put_pad(S, pt, 3, 0, [Fsm, Dd, Aa, Fsm], "pad", 26)
    put_arp(S, pt, 1, 0, [Fsm, Dd, Aa, Fsm], "shim", 16, "shimmer")
    put_bass(S, pt, 2, 0, [Fsm, Dd], "bass", 40, "night")
    put_melody_at(S, pt, 0, 0, "c#5---- a4-- f#4-- r c#5-- b4-- a4----", "lead", 42)
    put_melody_at(S, pt, 0, 48, "a4------------- f#5-------------", "lead", 40)
    S.order([pi, pa[0], pa[1], pb, pa[0], pt])
    return S


# --------------------------------------------------------------------------
# 6. BOSS — "OVERCLOCKED" — C minor, 168 BPM
# --------------------------------------------------------------------------
def build_boss():
    S = Song("boss.it", "OVERCLOCKED", 168,
             pans=[32, 46, 32, 18, 32, 32])
    S.samp("lead", make_loop("saw", 9130, 75, vib_cycles=3, vib_depth=0.010), True)
    S.samp("grit", make_loop("pulse", 489, 4, duty=0.25), True)
    S.samp("bass", make_loop("sawbass", 489, 4), True)
    S.samp("pad", make_loop("softpad", 732, 6), True)
    kit(S, snare_tone=185, seed=77, crash=True)
    Cm = ([48, 51, 55, 60], 36)
    Ab = ([48, 51, 56, 60], 32)
    Bb = ([50, 53, 58, 62], 34)
    Fm = ([48, 53, 56, 60], 29)
    G7 = ([50, 55, 59, 65], 31)
    mels = [
        "c6 c5 g5 c5 eb6 c6 g5 eb5 c5. g5. c6. r. |"
        "c6 c5 g5 c5 eb6 c6 g5 eb5 c5. g5. c6. r. |"
        "ab5. g5 f5. eb5 f5 g5 ab5. c6 b5. g5.. |"
        "f5. g5 ab5. c6 b5. ab5 g5 eb5 c5. r..",
        "c5 eb5 g5 c6 g5. eb5 c5 eb5 g5 c6 eb6. c6.. |"
        "ab5 c6 eb6. c6 ab5 eb5 ab5. c6 f5. ab5... |"
        "b5. d6 b5. g5 d5. g5 b5. d6 f6. d6. |"
        "c6 b5 ab5 g5 f5 eb5 d5.. c5--",
        "g5. ab5 b5. d6 c6. b5 ab5. g5 eb5. g5. |"
        "ab5. c6 eb6. ab6 g6. eb6 c6. ab5 f5. ab5. |"
        "b5. d6 f6. b6 ab6. f6 d6. b5 g5. b5. |"
        "eb6 f6 g5. ab5 b5 c6---",
    ]
    progA = [Cm, Cm, Ab, Bb]
    progB = [Cm, Fm, G7, Cm]
    pi = S.pat()
    put_pad(S, pi, 3, 0, [Cm, Cm, Ab, Bb], "pad", 24)
    put_bass(S, pi, 2, 0, [Cm, Cm], "bass", 50, "pulse16")
    put_drums(S, pi, 2, ["K..K..K.S...K..K", "K..K..K.S.K.TtTt"])
    pa = []
    for mi in (0, 1):
        p = S.pat()
        pa.append(p)
        put_melody(S, p, 0, 0, mels[mi], "lead", 50)
        put_arp(S, p, 1, 0, progA, "grit", 26, "16up")
        put_pad(S, p, 3, 0, progA, "pad", 18)
        put_bass(S, p, 2, 0, progA, "bass", 54, "pulse16")
        put_drums(S, p, 0, ["CK.K..K.S.h.K..h", "K..K.hK.S.h.K.ho",
                            "K..K..K.S.h.K..h", "K..K.hK.S.hKTtTt"])
    pb = []
    for mi in (2, 0):
        p = S.pat()
        pb.append(p)
        put_melody(S, p, 0, 0, mels[2] if mi == 2 else mels[0], "lead", 52)
        put_arp(S, p, 1, 0, progB, "grit", 28, "updown")
        put_pad(S, p, 3, 0, progB, "pad", 18)
        put_bass(S, p, 2, 0, progB, "bass", 54, "pulse16")
        put_drums(S, p, 0, ["K..K.hK.S.h.K..h", "K..K.hK.S.h.K.ho",
                            "K..K.hK.S.h.K..h", "K..K.hK.S.TKTtTt"])
    pt = S.pat()
    put_melody(S, pt, 0, 0, mels[1], "lead", 54)
    put_arp(S, pt, 1, 0, [Cm, Ab, Bb, Cm], "grit", 30, "16up")
    put_pad(S, pt, 3, 0, [Cm, Ab, Bb, Cm], "pad", 20)
    put_bass(S, pt, 2, 0, [Cm, Ab, Bb, Cm], "bass", 56, "pulse16")
    put_drums(S, pt, 0, ["CK.K..K.S.h.K..h", "K..K.hK.S.h.K.ho",
                         "K..K.hK.S.h.K..h", "K.T.K.T.t.TtTt.."])
    S.order([pi, pa[0], pa[1], pb[0], pb[1], pa[0], pb[0], pt])
    return S


# --------------------------------------------------------------------------
# 7. CREDITS — "WATERYS" — Bb major, 96 BPM
# --------------------------------------------------------------------------
def build_credits():
    S = Song("credits.it", "WATERYS", 96,
             pans=[32, 44, 32, 20, 32, 32])
    S.samp("bell", make_bell(), False)
    S.samp("organ", make_loop("organ", 1463, 12), True)
    S.samp("bass", make_loop("tri", 732, 6), True)
    S.samp("pad", make_loop("softpad", 11600, 95, trem_cycles=2, trem_depth=0.18), True)
    kit(S, snare_tone=195, seed=87, crash=True)
    Bb = ([50, 53, 58, 62], 34)
    Gm = ([50, 55, 58, 62], 31)
    Eb = ([51, 55, 58, 63], 27)
    Fc = ([53, 57, 60, 64], 29)
    mels = [
        "f5. g5 a5- d6-- c6 a5 |"
        "g5. f5 g5 a5--- r. |"
        "d5. g5 b5- d6-- b5 g5 |"
        "d5 f5 a5 c6----",
        "d#6-- c6 a#5- g5- |"
        "a5-- f5-- d5 f5 |"
        "g5-- d5-- a#4 d5 |"
        "d5 f5 a5 c6----",
        "f5..- d5. a5.-- |"
        "a5. c6 b5. a5 g5. f5 g5. a5 b5 c6.. |"
        "d6----- |"
        "a#5-----",
    ]
    progA = [Bb, Gm, Eb, Fc]
    progB = [Eb, Fc, Gm, Fc]
    pi = S.pat()
    put_pad(S, pi, 3, 0, [Bb, Bb, Gm, Eb], "pad", 28)
    put_arp(S, pi, 1, 0, [Bb, Bb, Gm, Eb], "organ", 24, "8th")
    put_bass(S, pi, 2, 0, [Bb, Gm], "bass", 42, "walk8")
    put_melody_at(S, pi, 0, 0, "d5----- f5----- a#5----- g5-----", "bell", 40)
    pa = []
    for rep in (0, 1):
        p = S.pat()
        pa.append(p)
        put_melody(S, p, 0, 0, mels[0] if rep == 0 else mels[1], "bell", 48)
        put_pad(S, p, 3, 0, progA if rep == 0 else progB, "pad", 26)
        put_arp(S, p, 1, 0, progA if rep == 0 else progB, "organ", 26, "8th")
        put_bass(S, p, 2, 0, progA if rep == 0 else progB, "bass", 46, "walk8")
        put_drums(S, p, 0, ["K...h...K...h...", "K...h.S.h...h.h.",
                            "K...h...K...h...", "K...h.S.h...TtT."], volmul=0.9)
    pb = S.pat()
    put_melody(S, pb, 0, 0, mels[1], "bell", 50)
    put_pad(S, pb, 3, 0, progB, "pad", 30)
    put_arp(S, pb, 1, 0, progB, "organ", 28, "8th")
    put_bass(S, pb, 2, 0, progB, "bass", 46, "walk8")
    put_drums(S, pb, 0, ["Ch..h...K...h...", "K...h.S.h...h.h.",
                         "K...h...K...h...", "K...h.S.h...TtT."], volmul=0.9)
    pt = S.pat()
    put_melody(S, pt, 0, 0, mels[2], "bell", 50)
    put_pad(S, pt, 3, 0, [Eb, Fc, Bb, Bb], "pad", 30)
    put_arp(S, pt, 1, 0, [Eb, Fc, Bb, Bb], "organ", 26, "8th")
    put_bass(S, pt, 2, 0, [Eb, Fc, Bb, Bb], "bass", 46, "walk8")
    put_drums(S, pt, 0, ["K...h...K...h...", "K...h.S.h...h.h.",
                         "K...h...K...h...", "K.T.K.T.t.TtT..."])
    S.order([pi, pa[0], pa[1], pb, pa[0], pb, pt])
    return S


BUILDERS = [
    ("title.it", "Title screen", build_title),
    ("school.it", "Chapters 1-2 (Hello World / Stack Overflow)", build_school),
    ("bughunt.it", "Chapters 3-4 (Bug Hunt / The Deploy)", build_bughunt),
    ("botfunk.it", "Chapters 5-7 (AI Bot Manager arc)", build_botfunk),
    ("threeram.it", "Chapters 8-10 (late-night / usage / 3am)", build_threeram),
    ("boss.it", "Boss fights (Twin Tyrants etc.)", build_boss),
    ("credits.it", "Finale / credits roll", build_credits),
]


def main():
    nowav = "--no-wav" in sys.argv
    WAVDIR.mkdir(exist_ok=True)
    print(f"{'file':12} {'use':42} {'tempo':>5} {'pats':>4} {'orders':>6} "
          f"{'sec':>5} {'it B':>7} {'BRR est':>7}")
    for fname, use, build in BUILDERS:
        S = build()
        data = S.build_it()
        (HERE / fname).write_bytes(data)
        if not nowav:
            S.render_wav(WAVDIR / (fname[:-3] + ".wav"))
        print(f"{fname:12} {use[:42]:42} {S.tempo:5} {len(S.patterns):4} "
              f"{len(S.orders):6} {S.seconds():5.0f} {len(data):7} {S.brr_estimate():7}")
    print("Wrote 7 original .it modules" + ("" if nowav else " + previews/*.wav"))


if __name__ == "__main__":
    main()
