"""Recipe browser DB: FTS5 search over saved cooks. tags/rating/favorite."""
import argparse, json, sqlite3, time

SCHEMA = """
CREATE TABLE IF NOT EXISTS recipes(name TEXT PRIMARY KEY, recipe TEXT,
    tags TEXT, note TEXT, cuisines TEXT, rating INT DEFAULT 0,
    favorite INT DEFAULT 0, created_ts REAL);
CREATE VIRTUAL TABLE IF NOT EXISTS recipes_fts USING fts5(
    name, recipe, tags, note, cuisines);
"""

def migrate(db):
    conn = sqlite3.connect(db)
    conn.executescript(SCHEMA); conn.commit(); conn.close()

def add_recipe(db, name, recipe, tags="", note="", cuisines=""):
    conn = sqlite3.connect(db)
    conn.execute("INSERT OR REPLACE INTO recipes VALUES(?,?,?,?,?,?,?,?)",
                 (name, recipe, tags, note, cuisines, 0, 0, time.time()))
    conn.execute("INSERT INTO recipes_fts(rowid,name,recipe,tags,note,cuisines) "
                 "SELECT rowid,name,recipe,tags,note,cuisines FROM recipes WHERE name=?",
                 (name,))
    conn.commit(); conn.close()

def search(db, query, limit=50):
    conn = sqlite3.connect(db)
    try:
        rows = conn.execute(
            "SELECT r.name, r.recipe, r.tags, r.rating, r.favorite "
            "FROM recipes_fts f JOIN recipes r ON r.rowid=f.rowid "
            "WHERE recipes_fts MATCH ? LIMIT ?", (query, limit)).fetchall()
    except sqlite3.OperationalError:
        rows = []
    conn.close()
    return [{"name": r[0], "recipe": r[1], "tags": r[2],
             "rating": r[3], "favorite": r[4]} for r in rows]

def import_stacks(db, folder):
    import os
    n = 0
    for dp, _d, fs in os.walk(folder):
        for f in fs:
            if f.endswith(".json"):
                try:
                    s = json.load(open(os.path.join(dp, f)))
                    add_recipe(db, f[:-5], s.get("recipe") or "",
                               tags=",".join(l.get("role","") for l in s.get("layers",[])),
                               note=s.get("tasting_note",""),
                               cuisines=(s.get("recipe") or "").split(".")[1] if s.get("recipe") else "")
                    n += 1
                except Exception: pass
    return n

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="recipes.db")
    ap.add_argument("cmd", choices=["init", "import", "search"])
    ap.add_argument("arg", nargs="?", default="")
    a = ap.parse_args()
    if a.cmd == "init": migrate(a.db); print("ready")
    elif a.cmd == "import": print("imported", import_stacks(a.db, a.arg))
    elif a.cmd == "search":
        for r in search(a.db, a.arg): print(f"{r['rating']}* {r['name']}: {r['recipe']} [{r['tags']}]")

if __name__ == "__main__":
    main()
