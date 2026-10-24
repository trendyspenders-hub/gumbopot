"""Daily Cook render pipeline (H2): today's recipe -> stack -> 60s preview WAV.
Posts are manual for now; this makes the artifact. Stdlib+numpy only.
Usage: python render_daily.py --db gumbo.db --out today.wav --seconds 60"""
import argparse, json, os, sys, wave
import numpy as np

def _read_wav(path, sr_target=None):
    w = wave.open(path, "rb")
    sr, ch, sw, n = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
    raw = w.readframes(n); w.close()
    y = np.frombuffer(raw, dtype={1: np.int8, 2: np.int16, 4: np.int32}[sw]).astype(np.float64)
    y = y / (2 ** (8 * sw - 1))
    if ch > 1: y = y.reshape(-1, ch).mean(axis=1)
    if sr_target and sr != sr_target:  # linear resample, good enough for preview
        idx = np.linspace(0, len(y)-1, int(len(y) * sr_target / sr))
        y = np.interp(idx, np.arange(len(y)), y)
        sr = sr_target
    return y, sr

def render(stack_path, out, seconds=60, sr=44100):
    stack = json.load(open(stack_path))
    n = int(seconds * sr)
    mix = np.zeros(n)
    used = []
    for layer in stack.get("layers", []):
        p = layer["path"]
        if not os.path.exists(p):
            continue
        try:
            y, _ = _read_wav(p, sr)
        except Exception:
            continue
        g = float(layer.get("gain", 0.9))
        if len(y) == 0: continue
        reps = int(np.ceil(n / len(y)))
        seg = np.tile(y, reps)[:n] * g
        mix += seg
        used.append(os.path.basename(p))
    peak = np.max(np.abs(mix)) + 1e-9
    if peak > 0.98: mix = mix * (0.98 / peak)
    pcm = (mix * 32767).astype(np.int16)
    with wave.open(out, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
        w.writeframes(pcm.tobytes())
    return used

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="gumbo.db")
    ap.add_argument("--out", default="today.wav")
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--stack", default=None, help="skip brain, render this stack.json")
    ap.add_argument("--post", action="store_true", help="print caption text")
    a = ap.parse_args()
    from daily_cook import recipe_for, decode
    recipe = recipe_for()
    if a.stack:
        stack_path = a.stack
    else:
        stack_path = a.out + ".stack.json"
        r = subprocess = __import__("subprocess")
        res = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "stack_brain.py"), "--db", a.db, "--seed", str(decode(recipe)["seed"]),
                            "--weirdness", str(decode(recipe)["weirdness"]),
                            "--layers", str(decode(recipe)["layers"]), "--no-ranker",
                            "--out", stack_path], capture_output=True, text=True,
                            cwd=os.path.dirname(os.path.abspath(__file__)))
        if res.returncode != 0:
            print("stack brain failed (library empty?):", res.stderr.strip()[:200]); sys.exit(1)
    used = render(stack_path, a.out, a.seconds)
    print(f"rendered {a.out} ({len(used)} layers): {used[:5]}")
    if a.post:
        print(f"caption: today{'\''}s gumbo -> {recipe} | cook along #gumbopot")

if __name__ == "__main__":
    main()
