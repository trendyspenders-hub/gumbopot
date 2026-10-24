"""Cook to Order: analyze a reference track, snap the pot to match.
Prints tuneup key, bpm target, loudness target. Feeds stack_brain as seed."""
import argparse, json, sys

def analyze_reference(path):
    import librosa
    import numpy as np
    y, sr = librosa.load(path, sr=22050, mono=True)
    tempo_fn = getattr(librosa.feature, "tempo", None) or librosa.beat.tempo
    bpm = round(float(tempo_fn(y=y, sr=sr)[0]), 1)
    chroma = librosa.feature.chroma_cqt(y=y, sr=sr).mean(axis=1)
    from indexer import detect_key, KS_MAJOR, KS_MINOR, NOTE_NAMES
    key = detect_key((chroma / (chroma.max() + 1e-9)).tolist())
    from coherence import lufs_approx
    return {"bpm": bpm, "key": key, "lufs": round(lufs_approx(y, sr), 1),
            "duration_s": round(len(y)/sr, 2)}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("reference")
    ap.add_argument("--out", default="order.json")
    args = ap.parse_args()
    try:
        info = analyze_reference(args.reference)
    except ImportError:
        sys.exit("needs librosa: pip install librosa soundfile")
    info["tuneup_key"] = info["key"].split()[0] if info["key"] else "A"
    info["tuneup_scale"] = info["key"].split()[1] if info["key"] else "minor"
    json.dump(info, open(args.out, "w"), indent=2)
    print(f"cooked to order: {info['key']}, {info['bpm']} BPM, "
          f"target {info['lufs']} LUFS")

if __name__ == "__main__":
    main()
