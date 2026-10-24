"""Mix-level coherence: masking + holes. numpy-only STFT, no librosa needed.
Masking = two or more layers both substantial (>=25%) in one band - that is
what actually reads as mud/fight. Holes = band energy below 15% of median
AND below an absolute floor (so sparse arrangements aren't punished)."""
import numpy as np

BANDS = [(20,200),(200,500),(500,1000),(1000,2000),(2000,5000),(5000,10000)]

def _stft_mag(y, sr, nfft=1024, hop=256):
    win = np.hanning(nfft)
    frames = [y[i:i+nfft]*win for i in range(0, max(1, len(y)-nfft), hop)]
    if not frames: frames = [np.zeros(nfft)]
    return np.abs(np.fft.rfft(np.array(frames), axis=1)), sr/2.0/(nfft//2)

def band_profile(y, sr):
    S, binhz = _stft_mag(y, sr)
    mean = S.mean(axis=0)
    return np.array([mean[int(lo/binhz):max(int(hi/binhz), int(lo/binhz)+1)].mean()
                     for lo, hi in BANDS])

def coherence(layers, sr=22050):
    """layers: list of mono float arrays. Returns (score 0..1, suggestions)."""
    if not layers:
        return 0.0, []
    profs = [band_profile(y, sr) for y in layers]
    mix = np.sum(profs, axis=0)
    mask_pen = 0.0
    for bi in range(len(BANDS)):
        shares = sorted((p[bi] for p in profs), reverse=True)
        if len(shares) > 1 and shares[1] > 0.25 * (shares[0] + shares[1] + 1e-9) * 2:
            # second layer also substantial -> they fight for the band
            if shares[1] > 0.25 * (sum(shares) + 1e-9) * 2:
                mask_pen += 0.25
    med = np.median(mix) + 1e-9
    peak = mix.max() + 1e-9
    holes = sum(1 for e in mix if e < 0.15 * med and e < 0.02 * peak)
    score = max(0.0, 1.0 - 0.5 * mask_pen - 0.08 * holes)
    suggestions = []
    for p in profs:
        if p[1] / (p.sum()+1e-9) > 0.35:
            suggestions.append({"strain": "lp", "freq": 400})
        elif p[4] / (p.sum()+1e-9) > 0.45:
            suggestions.append({"strain": "hp", "freq": 2500})
        else:
            suggestions.append(None)
    return round(score, 3), suggestions

def lufs_approx(y, sr=22050):
    if len(y) == 0: return -70.0
    return float(-0.691 + 20*np.log10(np.sqrt(np.mean(np.square(y)))+1e-9))
