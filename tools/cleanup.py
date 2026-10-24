"""Pantry cleanup: near-dup clusters + hoard stats. Compost = undoable trash."""
import argparse, os, shutil, sqlite3, time
import numpy as np

SIM_THRESHOLD = 0.93

def clusters(db):
    conn = sqlite3.connect(db)
    items = [(p, np.frombuffer(b, dtype=np.float32))
             for p, b in conn.execute("SELECT path, emb FROM samples WHERE emb IS NOT NULL")]
    conn.close()
    seen, out = set(), []
    for i, (pa, ea) in enumerate(items):
        if pa in seen: continue
        group = [pa]
        for pb, eb in items[i+1:]:
            if pb in seen: continue
            sim = float(np.dot(ea, eb) / (np.linalg.norm(ea)*np.linalg.norm(eb) + 1e-9))
            if sim > SIM_THRESHOLD:
                group.append(pb); seen.add(pb)
        if len(group) > 1: out.append(group)
        seen.add(pa)
    return out

def hoard_stats(db, root_folder):
    conn = sqlite3.connect(db)
    n = conn.execute("SELECT COUNT(*), SUM(bpm IS NOT NULL) FROM samples").fetchone()
    never = conn.execute("SELECT COUNT(*) FROM samples s WHERE NOT EXISTS "
                         "(SELECT 1 FROM taste_events t WHERE t.sample_path=s.path)").fetchone()[0]
    conn.close()
    gb = sum(os.path.getsize(os.path.join(dp, f))
             for dp, _d, fs in os.walk(root_folder) for f in fs) / 1e9
    return {"samples": n[0], "analyzed": n[1], "gb": round(gb, 1),
            "never_cooked_pct": round(100 * never / max(1, n[0]), 1)}

def compost(db, keep_path, group, compost_dir):
    os.makedirs(compost_dir, exist_ok=True)
    for p in group:
        if p != keep_path and os.path.exists(p):
            shutil.move(p, os.path.join(compost_dir, os.path.basename(p)))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="gumbo.db")
    ap.add_argument("cmd", choices=["dupes", "stats", "compost"])
    ap.add_argument("--keep", default=None)
    ap.add_argument("--dir", default=".")
    a = ap.parse_args()
    if a.cmd == "dupes":
        for g in clusters(a.db): print(f"{len(g)}x {os.path.basename(g[0])}: {g}")
    elif a.cmd == "stats":
        print(hoard_stats(a.db, a.dir))
    elif a.cmd == "compost":
        for g in clusters(a.db):
            if a.keep in g: compost(a.db, a.keep, g, "compost"); print("composted", g)

if __name__ == "__main__":
    main()
