"""Taste Quiz - cold-start palate seeding (the Tinder thing).

Serves 16 sounds one at a time; KEEP/TOSS answers become taste_events so the
palate vector exists before the first STIR. Serving is diversity-aware:
role round-robin + max-min embedding spread, demo pantry first, never repeats.

CLI (works headless; --play tries afplay/ffplay on mac/linux):
    python quiz.py --db gumbo.db                  # terminal quiz
    python quiz.py --db gumbo.db --serve-only     # just print the queue
The JUCE swipe-card UI calls the same functions (quiz.py contract below).
"""
import argparse, os, sqlite3, sys, time
import numpy as np

QUIZ_N = 16

SCHEMA = """CREATE TABLE IF NOT EXISTS quiz_shown(
    path TEXT PRIMARY KEY, ts REAL);"""

def _conn(db):
    c = sqlite3.connect(db); c.execute(SCHEMA); return c

def needs_quiz(db):
    """True if: taste_events empty AND quiz never answered/skipped.
    Robust to a completely fresh DB (no tables yet)."""
    c = _conn(db)
    try:
        has_events = c.execute("SELECT COUNT(*) FROM taste_events").fetchone()[0] > 0
    except sqlite3.OperationalError:
        has_events = False
    answered = c.execute("SELECT COUNT(*) FROM quiz_shown").fetchone()[0] > 0
    c.close()
    return not (has_events or answered)

def mark_shown(db, paths):
    c = _conn(db)
    c.executemany("INSERT OR IGNORE INTO quiz_shown VALUES(?,?)",
                  [(p, time.time()) for p in paths])
    c.commit(); c.close()

def serve_quiz(db, n=QUIZ_N, prefer_demo=True):
    """Returns up to n sample paths: role-diverse, embedding-spread, unrated.
    Contract for the UI layer: display one at a time -> answer() -> next."""
    c = _conn(db)
    rows = c.execute("SELECT path, role, emb FROM samples").fetchall()
    shown = {r[0] for r in c.execute("SELECT path FROM quiz_shown")}
    rated = {r[0] for r in c.execute(
        "SELECT DISTINCT sample_path FROM taste_events")}
    c.close()
    pool = [(p, role, np.frombuffer(b, dtype=np.float32) if b else None)
            for p, role, b in rows if p not in shown and p not in rated]

    # demo-pantry first, then library, then anything left
    def bucket(p):
        return 0 if "/pantry/" in p else (1 if prefer_demo else 1)
    pool.sort(key=lambda r: bucket(r[0]))

    # role round-robin so the quiz covers the whole kitchen
    by_role = {}
    for r in pool:
        by_role.setdefault(r[1] or "texture", []).append(r)

    # max-min spread within each role by embedding distance
    def spread(items):
        with_emb = [i for i in items if i[2] is not None]
        without = [i for i in items if i[2] is None]
        if not with_emb:
            return [i[0] for i in items]
        chosen = [with_emb[0]]
        rest = list(with_emb[1:])
        while rest:
            d = [min(np.linalg.norm(r[2]-c[2]) for c in chosen) for r in rest]
            chosen.append(rest.pop(int(np.argmax(d))))
        return [i[0] for i in chosen + without]

    queues = {role: spread(items) for role, items in by_role.items()}
    order, i = [], 0
    while len(order) < n and any(queues.values()):
        for role in sorted(queues):
            if queues[role] and len(order) < n:
                order.append(queues[role].pop(0))
    order = [p for p in order if "/pantry/" in p] + [p for p in order if "/pantry/" not in p]
    return order[:n]

def answer(db, path, keep, cook_id="quiz"):
    """keep=True -> 'pin' (+1.0), keep=False -> 'trash' (-1.0)."""
    from taste_memory import record_event
    record_event(db, path, "pin" if keep else "trash", cook_id=cook_id)

def skip_quiz(db):
    """User bailed: mark shown-empty so needs_quiz() stays False without
    polluting the palate (no events written)."""
    _conn(db).close()
    c = _conn(db)
    c.execute("INSERT OR IGNORE INTO quiz_shown VALUES(?,?)", ("__skipped__", time.time()))
    c.commit(); c.close()

def _play(path):
    import platform, subprocess
    cmd = (["afplay", path] if platform.system() == "Darwin"
           else ["ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", path])
    try: subprocess.run(cmd, timeout=30)
    except Exception: pass

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="gumbo.db")
    ap.add_argument("--n", type=int, default=QUIZ_N)
    ap.add_argument("--play", action="store_true")
    ap.add_argument("--serve-only", action="store_true")
    a = ap.parse_args()
    from taste_memory import migrate
    migrate(a.db)

    queue = serve_quiz(a.db, a.n)
    if not queue:
        print("pantry too small for a quiz - index more sounds first"); return
    if a.serve_only:
        for p in queue: print(p)
        return

    names = {}
    c = sqlite3.connect(a.db)
    for p, fn, role, key, bpm in c.execute(
            "SELECT path, filename, role, key, bpm FROM samples"):
        names[p] = f"{fn}  [{role or '?'} | {key or '?'} | {bpm or '?'}bpm]"
    c.close()

    print(f"Taste quiz - {len(queue)} sounds. k=keep t=toss s=skip-all q=quit")
    kept = tossed = 0
    for i, p in enumerate(queue, 1):
        mark_shown(a.db, [p])
        if a.play: _play(p)
        ans = input(f"[{i}/{len(queue)}] {names.get(p, p)}  (k/t/q): ").strip().lower()
        if ans == "q": break
        if ans == "s": skip_quiz(a.db); print("skipped - quiz won't nag"); return
        if ans == "k": answer(a.db, p, True); kept += 1
        elif ans == "t": answer(a.db, p, False); tossed += 1
    print(f"palate seeded: {kept} kept, {tossed} tossed - now STIR THE POT")

if __name__ == "__main__":
    main()
