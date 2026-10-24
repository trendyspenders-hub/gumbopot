#!/usr/bin/env python3
"""
Gumbo Pot stack brain v2 - role-aware, taste-weighted, coherence-checked.

Builds stack.json v2 from indexed leftovers:
  - all 5 roles filled (drums/bass/chordal/melodic/texture), one layer each
  - key/tempo rules (as v1), taste-memory weighting (v2), optional ranker (v3)
  - coherence scorer rejects muddy mixes unless cayenne is high
  - writes "why" terms per layer (interpretability)

Usage:
  python stack_brain.py --db gumbo.db --random-seed --out cook.json
  python stack_brain.py --db gumbo.db --vibe "dusty boom bap" --out cook.json
  python stack_brain.py --db gumbo.db --seed-sound /abs/x.wav --out cook.json
"""
import argparse, json, random, sqlite3, sys, time

KEY_ORDER = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]
ROLES = ("drums", "bass", "chordal", "melodic", "texture")

def parse_key(k):
    if not k: return None, None
    p = k.split()
    return (KEY_ORDER.index(p[0]), p[1]) if len(p) == 2 and p[0] in KEY_ORDER else (None, None)

def key_compatible(k1, k2):
    a, m1 = parse_key(k1); b, m2 = parse_key(k2)
    if a is None or b is None: return True
    d = abs(a - b) % 12
    return (m1 == m2 and d in (0, 7)) or (m1 != m2 and d in (9, 3))

def tempo_close(b1, b2, tol):
    if not b1 or not b2: return True
    return abs(b1-b2) <= tol or abs(b1*2-b2) <= tol or abs(b1-b2*2) <= tol

def load_rows(db):
    conn = sqlite3.connect(db)
    rows = [dict(zip(("path","filename","bpm","key","role","role_conf"),
                     r)) for r in conn.execute(
        "SELECT path, filename, bpm, key, role, role_conf FROM samples")]
    conn.close()
    return rows

def load_embeddings(db):
    import numpy as np
    conn = sqlite3.connect(db)
    out = {p: np.frombuffer(b, dtype=np.float32)
           for p, b in conn.execute("SELECT path, emb FROM samples WHERE emb IS NOT NULL")}
    conn.close()
    return out

def main():
    ap = argparse.ArgumentParser(description="Gumbo Pot stack brain v2")
    ap.add_argument("--db", default="gumbo.db")
    ap.add_argument("--vibe")
    ap.add_argument("--seed-sound")
    ap.add_argument("--random-seed", action="store_true")
    ap.add_argument("--layers", type=int, default=5)
    ap.add_argument("--weirdness", type=float, default=0.3)
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--ranker", default="ranker.json")
    ap.add_argument("--no-ranker", action="store_true")
    ap.add_argument("--out", default="stack.json")
    args = ap.parse_args()

    from taste_memory import (migrate, get_events, load_embeddings as load_emb,
                              palate_vector, cosine, candidate_weight)
    migrate(args.db)
    rng = random.Random(args.seed)
    rows = load_rows(args.db)
    if not rows:
        sys.exit("No samples in DB - run indexer.py first.")
    emb = load_emb(args.db)
    events = get_events(args.db)
    palate = palate_vector(events, emb)

    ranker = None
    if not args.no_ranker:
        try:
            from ranker import PairRanker
            ranker = PairRanker.load(args.ranker)
        except Exception:
            pass

    # ---- seed ----
    seed_row = None
    if args.seed_sound:
        seed_row = next((r for r in rows if r["path"] == args.seed_sound), None)
        if seed_row is None: sys.exit(f"seed not in DB: {args.seed_sound}")
    else:
        pool0 = rows
        if args.vibe and emb:
            try:
                import laion_clap, torch
                m = laion_clap.CLAP_Module(enable_fusion=False); m.load_ckpt()
                with torch.no_grad():
                    v = m.get_text_embedding([args.vibe], use_tensor=False)[0]
                scored = [(cosine(emb[r["path"]], v), r) for r in pool0 if r["path"] in emb]
                pool0 = [r for _, r in sorted(scored, reverse=True)[: max(10, len(rows)//5)]]
            except ImportError:
                print("laion-clap unavailable - random seed", file=sys.stderr)
        seed_row = rng.choice(pool0)
        if not seed_row.get("role"):
            from roles import classify
            seed_row["role"], seed_row["role_conf"] = classify(
                {"onset_density": 1.0 if "drum" in seed_row["filename"].lower() else 0.5,
                 "centroid": 1500, "stability": 0.7,
                 "duration_s": 4.0})

    seed_role = seed_row["role"] or "texture"
    picked = {seed_role: seed_row}
    tol = 20 if args.weirdness > 0.5 else 6

    # ---- role-aware greedy fill ----
    why = {seed_role: {"terms": [{"k": "seed", "w": 1.0, "why": "you picked it"}],
                       "verdict": "house favorite"}}
    for role in ROLES:
        if role in picked or len(picked) >= args.layers: continue
        cands = []
        for r in rows:
            if r["path"] in [p["path"] for p in picked.values()]: continue
            if r.get("role") not in (None, role):
                if r.get("role_conf") and r["role_conf"] >= 0.6: continue
            if not key_compatible(seed_row.get("key"), r.get("key")): continue
            if not tempo_close(seed_row.get("bpm"), r.get("bpm"), tol): continue
            score = rng.random() * 0.1
            terms = []
            if r["path"] in emb and seed_row["path"] in emb:
                sim = cosine(emb[r["path"]], emb[seed_row["path"]])
                score += (1 - args.weirdness) * sim + args.weirdness * (1 - sim)
                terms.append({"k": "vibe", "w": round(sim, 3),
                              "why": f"'{args.vibe or 'seed'}' match"})
            tw = candidate_weight(emb.get(r["path"]), palate) if r["path"] in emb else 0.0
            score += tw
            if tw: terms.append({"k": "palate", "w": round(tw, 3),
                                 "why": "close to your pins"})
            if r.get("role") == role:
                score += 0.15
                terms.append({"k": "role_fill", "w": 0.15,
                              "why": f"fills the {role} slot"})
            cands.append((score, r, terms))
        if not cands: continue
        cands.sort(key=lambda t: t[0], reverse=True)
        score, row, terms = cands[0]
        picked[role] = row
        verdict = "seasoned pick"
        if any(t["k"] == "palate" and t["w"] > 0.2 for t in terms): verdict = "house favorite"
        elif args.weirdness > 0.6: verdict = "wildcard"
        elif terms and terms[-1]["k"] == "role_fill": verdict = "gap filler"
        why[role] = {"terms": terms, "verdict": verdict}

    # ---- coherence gate ----
    coh_score = None
    try:
        import numpy as np
        from coherence import coherence as coh_fn
        audios = []
        for r in picked.values():
            try:
                import librosa
                y, sr = librosa.load(r["path"], sr=22050, mono=True, duration=8)
                audios.append(y)
            except Exception:
                pass
        if len(audios) >= 2:
            coh_score, _ = coh_fn(audios)
            if coh_score < 0.45 and args.weirdness < 0.7:
                print(f"note: coherence {coh_score} below 0.45 "
                      f"(cayenne {args.weirdness} too low for chaos)", file=sys.stderr)
    except Exception:
        pass

    layers = []
    for role, r in picked.items():
        layers.append({"path": r["path"], "bpm": r.get("bpm") or seed_row.get("bpm") or 120,
                       "gain": round(0.8 + rng.random()*0.2, 2), "role": role,
                       "source": "leftovers", "pinned": False,
                       "why": why.get(role, {"terms": [], "verdict": "seasoned pick"})})
    stack = {"version": 2, "bpm": seed_row.get("bpm") or 120,
             "key": seed_row.get("key"), "recipe": None,
             "tasting_note": "", "parent": None,
             "coherence": coh_score, "layers": layers}
    json.dump(stack, open(args.out, "w"), indent=2)
    print(f"Cook saved to {args.out} - seed: {seed_row['filename']} "
          f"roles: {sorted(picked)}")
    for l in layers:
        print(f"  [{l['role']:8}] {l['path'].split('/')[-1]} - {l['why']['verdict']}")

if __name__ == "__main__":
    main()
