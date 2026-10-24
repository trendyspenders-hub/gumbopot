"""Pairwise ranker: learns P(candidate beats current) from taste events.
Deliberately boring logistic regression - shippable."""
import json, sqlite3, time
import numpy as np

FEATURE_NAMES = ["cos_a_seed", "cos_b_seed", "cos_a_b",
                 "d_centroid", "d_bpm", "role_match_a", "role_match_b"]

class PairRanker:
    def __init__(self):
        self.w = np.zeros(len(FEATURE_NAMES)); self.b = 0.0

    def _sig(self, z):
        return 1.0 / (1.0 + np.exp(-z))

    def fit(self, X, y, lr=0.1, iters=500):
        X = np.asarray(X, dtype=np.float64); y = np.asarray(y, dtype=np.float64)
        n = max(1, len(y))
        for _ in range(iters):
            p = self._sig(X @ self.w + self.b)
            g = X.T @ (p - y) / n
            self.w -= lr * g
            self.b -= lr * float(np.mean(p - y))
        return self

    def proba(self, feats):
        return float(self._sig(np.asarray(feats, dtype=np.float64) @ self.w + self.b))

    def save(self, path):
        json.dump({"w": self.w.tolist(), "b": self.b}, open(path, "w"))

    @staticmethod
    def load(path):
        d = json.load(open(path))
        r = PairRanker(); r.w = np.asarray(d["w"]); r.b = d["b"]
        return r

def feats_for_pair(emb_a, emb_b, emb_seed, meta_a, meta_b, role_need):
    def c(x, y): return float(np.dot(x, y) / (np.linalg.norm(x)*np.linalg.norm(y) + 1e-9))
    return [c(emb_a, emb_seed), c(emb_b, emb_seed), c(emb_a, emb_b),
            abs((meta_a.get("centroid") or 0) - (meta_b.get("centroid") or 0)) / 3000.0,
            abs((meta_a.get("bpm") or 0) - (meta_b.get("bpm") or 0)) / 60.0,
            1.0 if meta_a.get("role") == role_need else 0.0,
            1.0 if meta_b.get("role") == role_need else 0.0]

def build_dataset(db, embeddings):
    """Pairwise rows from taste_events. Returns (X, y) lists."""
    from taste_memory import WEIGHTS
    conn = sqlite3.connect(db)
    ev = conn.execute("SELECT sample_path, event, ts, cook_id FROM taste_events").fetchall()
    conn.close()
    pos = [p for p, e, *_ in ev if e in ("pin", "ab_win", "solo_keep", "served")]
    neg = [p for p, e, *_ in ev if e in ("trash", "ab_lose")]
    X, y = [], []
    for p in pos:
        for n in neg:
            if p in embeddings and n in embeddings:
                X.append(feats_for_pair(embeddings[p], embeddings[n], embeddings[p],
                                        {}, {}, None))
                y.append(1.0)
                X.append(feats_for_pair(embeddings[n], embeddings[p], embeddings[p],
                                        {}, {}, None))
                y.append(0.0)
    return X, y

def train(db, out_path="ranker.json"):
    from taste_memory import load_embeddings
    emb = load_embeddings(db)
    X, y = build_dataset(db, emb)
    if len(y) < 20:
        return None, len(y)   # cold start guard
    r = PairRanker().fit(X, y)
    r.save(out_path)
    return r, len(y)

def shadow_report(db):
    conn = sqlite3.connect(db)
    rows = conn.execute("SELECT prod_score, cand_score, picked_diff FROM shadow_log").fetchall()
    conn.close()
    if len(rows) < 200:
        return {"n": len(rows), "promote": False, "reason": "under 200 disagreements"}
    win = sum(1 for p, c, d in rows if (c > p) == bool(d)) / len(rows)
    return {"n": len(rows), "win_rate": win, "promote": win >= 0.55}
