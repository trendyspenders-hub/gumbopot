#!/usr/bin/env python3
"""
Gumbo Pot M2 - Leftovers indexer.

Scans your local Splice downloads folder and builds gumbo.db (SQLite) with
path, filename, duration, BPM and key for every sample.

Usage:
    python indexer.py "/path/to/Splice" --db gumbo.db
    python indexer.py "/path/to/Splice" --db gumbo.db --no-audio   # fast: filenames only

Optional (for BPM/key detection):
    pip install librosa soundfile
"""

import argparse
import os
import sqlite3
import sys

SUPPORTED = (".wav", ".aif", ".aiff", ".flac", ".mp3")

KS_MAJOR = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]
KS_MINOR = [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

SCHEMA = """CREATE TABLE IF NOT EXISTS samples(
    path TEXT PRIMARY KEY,
    filename TEXT,
    duration REAL,
    bpm REAL,
    key TEXT,
    analyzed INTEGER DEFAULT 0,
    role TEXT,
    role_conf REAL
);"""

def analyze_with_role(path):
    "analyze() + role classification. Returns (duration, bpm, key, role, conf)."
    duration, bpm, key = analyze(path)
    role = conf = None
    if duration is not None:
        try:
            from roles import classify, features_from_audio
            role, conf = classify(features_from_audio(path))
        except ImportError:
            pass
    return duration, bpm, key, role, conf


def detect_key(chroma):
    best, best_score = None, -2.0
    for mode, profile in (("major", KS_MAJOR), ("minor", KS_MINOR)):
        for i in range(12):
            score = sum(p * chroma[(i + j) % 12] for j, p in enumerate(profile))
            if score > best_score:
                best_score, best = score, NOTE_NAMES[i] + " " + mode
    return best


def analyze(path):
    """Returns (duration, bpm, key) or (None, None, None). Needs librosa."""
    try:
        import librosa
    except ImportError:
        return None, None, None
    try:
        y, sr = librosa.load(path, sr=22050, mono=True)
        if len(y) < sr // 4:
            return None, None, None
        duration = round(float(len(y) / sr), 3)
        tempo_fn = getattr(librosa.feature, "tempo", None) or librosa.beat.tempo
        bpm = round(float(tempo_fn(y=y, sr=sr)[0]), 1)
        chroma = librosa.feature.chroma_cqt(y=y, sr=sr).mean(axis=1)
        peak = chroma.max()
        chroma = (chroma / peak).tolist() if peak > 0 else [0.0] * 12
        return duration, bpm, detect_key(chroma)
    except Exception:
        return None, None, None


def main():
    ap = argparse.ArgumentParser(description="Gumbo Pot leftovers indexer")
    ap.add_argument("root", help="Folder to scan (e.g. your Splice downloads)")
    ap.add_argument("--db", default="gumbo.db")
    ap.add_argument("--no-audio", action="store_true",
                    help="Skip BPM/key analysis - index filenames only (fast)")
    args = ap.parse_args()

    conn = sqlite3.connect(args.db)
    conn.execute(SCHEMA)

    existing = {row[0] for row in conn.execute("SELECT path FROM samples")}
    found = 0
    added = 0

    for dirpath, _dirs, files in os.walk(args.root):
        for fname in sorted(files):
            if not fname.lower().endswith(SUPPORTED):
                continue
            path = os.path.join(dirpath, fname)
            found += 1
            if path in existing:
                continue
            if args.no_audio:
                duration = bpm = key = role = conf = None
            else:
                duration, bpm, key, role, conf = analyze_with_role(path)
            conn.execute(
                "INSERT OR REPLACE INTO samples VALUES (?,?,?,?,?,?,?,?)",
                (path, fname, duration, bpm, key,
                 0 if args.no_audio else 1, role, conf),
            )
            added += 1

    conn.commit()
    total = conn.execute("SELECT COUNT(*) FROM samples").fetchone()[0]
    analyzed = conn.execute("SELECT COUNT(*) FROM samples WHERE analyzed=1").fetchone()[0]
    conn.close()

    print(f"Scanned {found} audio files, added {added} new.")
    print(f"DB '{args.db}' now holds {total} samples ({analyzed} with BPM/key).")
    if analyzed == 0 and not args.no_audio:
        print("No audio analyzed - install librosa for BPM/key: pip install librosa soundfile",
              file=sys.stderr)


if __name__ == "__main__":
    main()
