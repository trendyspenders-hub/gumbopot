"""Role classifier: drums/bass/chordal/melodic/texture.
analyze_file() needs librosa; classify() is pure logic on a feature dict so
it is testable without audio deps."""
import math

ROLES = ("drums", "bass", "chordal", "melodic", "texture")

def classify(f):
    """f: dict(onset_density, centroid, stability, duration_s) -> (role, conf)"""
    od, cen, stab, dur = f.get("onset_density", 0.0), f.get("centroid", 0.0),                          f.get("stability", 0.0), f.get("duration_s", 0.0)
    if od > 3.5 and dur < 8:
        return "drums", 0.8
    if cen < 1200 and stab > 0.60:
        return "bass", 0.7
    if stab > 0.75 and 400 <= cen <= 2500:
        return "chordal", 0.7
    if cen > 2500 and stab > 0.60:
        return "melodic", 0.6
    return "texture", 0.5

def features_from_audio(path):
    """Full feature extraction via librosa. Returns feature dict."""
    import librosa
    import numpy as np
    y, sr = librosa.load(path, sr=22050, mono=True)
    dur = len(y) / sr
    onset_env = librosa.onset.onset_strength(y=y, sr=sr)
    onsets = librosa.onset.onset_detect(y=y, sr=sr, units="time")
    od = len(onsets) / dur if dur > 0 else 0.0
    cent = float(np.mean(librosa.feature.spectral_centroid(y=y, sr=sr)))
    chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
    stab = float(1.0 - np.mean(np.std(chroma, axis=1)))
    return {"onset_density": od, "centroid": cent,
            "stability": stab, "duration_s": dur}
