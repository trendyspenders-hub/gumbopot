"""Taste memory: events -> weighted palate vector -> candidate weights."""
import math, sqlite3, time
import numpy as np

WEIGHTS = {"pin": 1.0, "solo_keep": 0.7, "ab_win": 0.5,
           "served": 0.3, "ab_lose": -0.5, "trash": -1.0}
HALF_LIFE_DAYS = 30.0
TASTE_W = 0.35

MIGRATION = """
CREATE TABLE IF NOT EXISTS taste_events(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sample_path TEXT NOT NULL,
    event TEXT NOT NULL CHECK(event IN
        ('pin','trash','solo_keep','ab_win','ab_lose','served')),
    cook_id TEXT, ts REAL NOT NULL);
CREATE TABLE IF NOT EXISTS cooks(
    id TEXT PRIMARY KEY, parent_id TEXT, recipe TEXT,
    seed INTEGER, weirdness REAL, created_ts REAL, stack_json TEXT);
CREATE TABLE IF NOT EXISTS grocery(
    path TEXT PRIMARY KEY, vibe TEXT, ts REAL);
CREATE TABLE IF NOT EXISTS shadow_log(
    ts REAL, cook TEXT, prod_score REAL, cand_score REAL, picked_diff INT);
ALTER TABLE samples ADD COLUMN role TEXT;
ALTER TABLE samples ADD COLUMN role_conf REAL;
ALTER TABLE samples ADD COLUMN emb BLOB;
"""

def migrate(db):
    conn = sqlite3.connect(db)
    for stmt in MIGRATION.split(";"):
        stmt = stmt.strip()
        if stmt:
            try:
                conn.execute(stmt)
            except sqlite3.OperationalError:
                pass  # column exists
    conn.commit(); conn.close()

def record_event(db, sample_path, event, cook_id=None, ts=None):
    assert event in WEIGHTS, event
    conn = sqlite3.connect(db)
    conn.execute("INSERT INTO taste_events(sample_path,event,cook_id,ts) VALUES(?,?,?,?)",
                 (sample_path, event, cook_id, ts or time.time()))
    conn.commit(); conn.close()

def load_embeddings(db):
    conn = sqlite3.connect(db)
    out = {}
    for path, blob in conn.execute("SELECT path, emb FROM samples WHERE emb IS NOT NULL"):
        out[path] = np.frombuffer(blob, dtype=np.float32)
    conn.close()
    return out

def _decay(ts, now):
    return 0.5 ** ((now - ts) / (HALF_LIFE_DAYS * 86400.0))

def palate_vector(events, embeddings, now=None):
    """events: iterable of (sample_path, event, ts). Returns 512-vec or None."""
    now = now or time.time()
    v = np.zeros(512, dtype=np.float64); wsum = 0.0
    for path, event, ts in events:
        if path not in embeddings:
            continue
        w = WEIGHTS[event] * _decay(ts, now)
        v += w * embeddings[path].astype(np.float64)
        wsum += abs(w)
    return (v / (wsum + 1e-9)) if wsum > 0 else None

def cosine(a, b):
    return float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-9))

def candidate_weight(cand_emb, palate):
    return TASTE_W * cosine(cand_emb, palate) if palate is not None else 0.0

def get_events(db, since_ts=0.0):
    conn = sqlite3.connect(db)
    rows = conn.execute("SELECT sample_path, event, ts FROM taste_events WHERE ts>=?",
                        (since_ts,)).fetchall()
    conn.close()
    return rows
