"""Watch folder: new audio in Inbox -> analyze -> gumbo.db -> PANTRY flash.
Polling implementation (portable); --once for tests/CI."""
import argparse, os, sqlite3, sys, time

SUPPORTED = (".wav", ".aif", ".aiff", ".flac", ".mp3")

def scan_once(inbox, db, indexer):
    from indexer import analyze, SCHEMA
    conn = sqlite3.connect(db)
    conn.execute(SCHEMA)
    known = {r[0] for r in conn.execute("SELECT path FROM samples")}
    fresh = []
    for f in sorted(os.listdir(inbox)):
        p = os.path.join(inbox, f)
        if not f.lower().endswith(SUPPORTED) or p in known or not os.path.isfile(p):
            continue
        duration, bpm, key = analyze(p)
        role = role_conf = None
        if duration is not None:
            try:
                from roles import classify, features_from_audio
                role, role_conf = classify(features_from_audio(p))
            except ImportError:
                pass
        conn.execute("INSERT OR REPLACE INTO samples(path,filename,duration,bpm,key,analyzed,role,role_conf) "
                     "VALUES(?,?,?,?,?,?,?,?)",
                     (p, f, duration, bpm, key, 1 if duration else 0, role, role_conf))
        fresh.append(f)
    conn.commit(); conn.close()
    return fresh

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inbox")
    ap.add_argument("--db", default="gumbo.db")
    ap.add_argument("--once", action="store_true")
    ap.add_argument("--interval", type=float, default=2.0)
    a = ap.parse_args()
    print(f"watching {a.inbox}")
    while True:
        for f in scan_once(a.inbox, a.db, None):
            print(f"fresh: {f}")
        if a.once: break
        time.sleep(a.interval)

if __name__ == "__main__":
    main()
