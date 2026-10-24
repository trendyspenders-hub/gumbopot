"""THE PERFECT CIRCLE - seamless loop-point detection (numpy-only).
Picks a loop region of a given length whose endpoints match in energy and
phase (zero-crossing aligned), and scores how periodic the region really is.
The pot simmers in circles - loops should too."""
import numpy as np

def _rms(x):
    return float(np.sqrt(np.mean(np.square(x))))

def periodicity_score(region):
    """0..1: autocorrelation of the region with itself at half length.
    1.0 = perfectly periodic (sine), ~0 for noise."""
    n = len(region)
    if n < 64: return 0.0
    a, b = region[:n//2], region[n//2:n//2*2]
    if _rms(a) <= 1e-6 or _rms(b) <= 1e-6: return 0.0
    c = np.corrcoef(a, b)[0, 1]
    return float(max(0.0, min(1.0, c)))

def find_loop_points(y, sr, length_s, search_s=2.0):
    """Returns dict(start_s, end_s, score). Searches the loudest onset window,
    aligns the end to the best-matching zero crossing of the same slope."""
    n_target = int(length_s * sr)
    if len(y) < n_target + sr:
        return {"start_s": 0.0, "end_s": len(y)/sr, "score": periodicity_score(y[:min(len(y), n_target)])}
    hop = int(0.01 * sr)
    win = int(0.05 * sr)
    best, best_rms = 0, -1.0
    for i in range(0, min(len(y) - win, int(search_s * sr)), hop):
        r = _rms(y[i:i+win])
        if r > best_rms: best_rms, best = r, i
    start = best
    end_ideal = start + n_target
    if end_ideal >= len(y): end_ideal = len(y) - 1
    # snap end to zero crossing with same slope as start
    def slope_at(idx):
        i = int(np.clip(idx, 1, len(y)-2))
        return 1 if y[i] - y[i-1] >= 0 else -1
    ss = slope_at(start)
    cand = [j for j in range(end_ideal - int(0.02*sr), end_ideal + int(0.02*sr))
            if 1 <= j < len(y)-1 and y[j-1] <= 0 < y[j] and slope_at(j) == ss]
    end = min(cand, key=lambda j: abs(j - end_ideal)) if cand else end_ideal
    region = y[start:end]
    return {"start_s": round(start/sr, 4), "end_s": round(end/sr, 4),
            "score": round(periodicity_score(region), 3)}
