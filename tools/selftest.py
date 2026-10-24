"""Gumbo Pot toolchain self-test. Synthetic data, no audio deps required.
Run: python selftest.py   (exit 0 = all pass)"""
import json, os, sqlite3, sys, tempfile, time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
FAILS = []
def check(ok, what):
    print(("  PASS  " if ok else "  FAIL  ") + what)
    if not ok: FAILS.append(what)

def main():
    tmp = tempfile.mkdtemp()
    db = os.path.join(tmp, "gumbo.db")

    # --- build synthetic DB: 5 roles x 3 samples with embeddings ---
    from taste_memory import migrate, record_event, get_events, palate_vector, candidate_weight
    from indexer import SCHEMA as SAMPLES_SCHEMA
    from roles import classify

    def populate(path_db):
        sqlite3.connect(path_db).execute(SAMPLES_SCHEMA)
        migrate(path_db)
        rng = np.random.default_rng(7)
        conn = sqlite3.connect(path_db)
        for role_i, role in enumerate(["drums","bass","chordal","melodic","texture"]):
            base = rng.normal(size=512).astype(np.float32)
            for j in range(3):
                emb = (base + 0.05*rng.normal(size=512)).astype(np.float32)
                emb /= np.linalg.norm(emb)
                key = ["G minor","G minor","C major","C major","A minor"][(role_i+j) % 5]
                bpm = 90 + 10*j if role != "texture" else 120
                p = f"/fake/{role}_{j}.wav"
                conn.execute("INSERT INTO samples (path,filename,duration,bpm,key,analyzed,role,role_conf) VALUES (?,?,?,?,?,?,?,?)",
                             (p, f"{role}_{j}.wav", 4.0, bpm, key, 1, role, 0.7))
                conn.execute("UPDATE samples SET emb=? WHERE path=?", (emb.tobytes(), p))
        conn.commit(); conn.close()

    populate(db)

    # --- taste memory ---
    emb_map = __import__("taste_memory").load_embeddings(db)
    record_event(db, "/fake/drums_0.wav", "pin")
    record_event(db, "/fake/drums_1.wav", "pin")
    record_event(db, "/fake/bass_2.wav", "trash")
    events = get_events(db)
    check(len(events) == 3, "taste events recorded")
    pal = palate_vector(events, emb_map, now=time.time())
    check(pal is not None, "palate vector computed")
    w_pos = candidate_weight(emb_map["/fake/drums_0.wav"], pal)
    w_neg = candidate_weight(emb_map["/fake/bass_2.wav"], pal)
    check(w_pos > w_neg, "palate prefers pins over trash")

    # --- role classifier rules ---
    check(classify({"onset_density": 6.0, "centroid": 3000, "stability": 0.4, "duration_s": 2.0})[0] == "drums", "classifier: drums")
    check(classify({"onset_density": 0.5, "centroid": 600, "stability": 0.8, "duration_s": 8.0})[0] == "bass", "classifier: bass")
    check(classify({"onset_density": 0.5, "centroid": 1200, "stability": 0.9, "duration_s": 8.0})[0] == "chordal", "classifier: chordal")
    check(classify({"onset_density": 2.0, "centroid": 4000, "stability": 0.8, "duration_s": 6.0})[0] == "melodic", "classifier: melodic")
    check(classify({"onset_density": 2.0, "centroid": 2000, "stability": 0.3, "duration_s": 6.0})[0] == "texture", "classifier: texture")

    # --- ranker: train on synthetic pairwise data ---
    from ranker import PairRanker, feats_for_pair, build_dataset
    X = [[1,0,0, 0.1,0.05, 1,0], [0,1,0, 0.1,0.05, 0,1], [1,0,0.2, 0.0,0.0, 1,0],
         [0,1,0.2, 0.0,0.0, 0,1], [1,0,0.1, 0.2,0.1, 1,0], [0,1,0.1, 0.2,0.1, 0,1]] * 20
    y = [1,0,1,0,1,0] * 20
    r = PairRanker().fit(X, y)
    check(r.proba([1,0,0,0.1,0.05,1,0]) > 0.7, "ranker learns positive class")
    r.save(os.path.join(tmp, "ranker.json"))
    r2 = PairRanker.load(os.path.join(tmp, "ranker.json"))
    check(abs(r2.proba(X[0]) - r.proba(X[0])) < 1e-6, "ranker save/load roundtrip")

    # --- stack brain v2 end-to-end (random seed, no CLAP) ---
    import subprocess
    out = os.path.join(tmp, "cook.json")
    res = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "stack_brain.py"), "--db", db, "--random-seed",
                         "--seed", "42", "--out", out, "--no-ranker"],
                         capture_output=True, text=True, cwd=os.path.dirname(os.path.abspath(__file__)))
    check(res.returncode == 0, f"stack brain runs ({res.stderr.strip()[:80]})")
    stack = json.load(open(out))
    roles_out = sorted(l["role"] for l in stack["layers"])
    check(stack["version"] == 2, "stack.json v2")
    check(len(set(roles_out)) == len(stack["layers"]), "no duplicate roles")
    check(all("why" in l and l["why"]["verdict"] for l in stack["layers"]), "why-terms present")

    # --- coherence: mud vs clean (broadband, realistic) ---
    from coherence import coherence, lufs_approx
    sr = 22050; t = np.arange(sr*4)/sr
    noise = np.random.default_rng(3).normal(0, 0.2, sr*4)
    def bp(y, lo, hi):
        F = np.fft.rfft(y); f = np.fft.rfftfreq(len(y), 1/sr)
        F[(f<lo)|(f>hi)] = 0; return np.fft.irfft(F, len(y))
    pad1 = bp(noise, 200, 900)      # mid-heavy pad
    pad2 = bp(np.roll(noise, 500), 250, 800)  # another mid-heavy layer -> mud
    drums = bp(np.abs(noise), 2000, 9000)     # bright transients
    bassl = bp(noise, 30, 120)
    s_mud, _ = coherence([pad1, pad2, bassl])
    s_mix, sug = coherence([drums, bassl, pad1])
    check(s_mix > s_mud, f"coherence prefers balanced mix (mud={s_mud} clean={s_mix})")
    check(any(x is not None for x in sug), "strain suggestions emitted")
    check(-60 < lufs_approx(0.5*np.sin(2*np.pi*440*t)) < 0, "lufs approx sane")

    # --- daily cook ---
    import datetime
    from daily_cook import recipe_for, decode
    d = datetime.date(2026, 10, 24)
    r1, r2 = recipe_for(d), recipe_for(d)
    check(r1 == r2, f"daily cook deterministic ({r1})")
    dec = decode(r1)
    check(dec["seed"] == d.timetuple().tm_yday*37 + 26, "daily cook seed math")
    try:
        decode("gumbo://v1.kansas.1.1.5"); check(False, "bad cuisine rejected")
    except ValueError as e:
        check("menu" in str(e), "bad cuisine rejected")

    # --- telemetry privacy ---
    from telemetry import Telemetry
    tel = Telemetry(os.path.join(tmp, "tel.jsonl"))
    tel.emit("served", path="/Users/john_doe/secret/kick.wav", cpu_pct=5.0)
    line = json.loads(open(os.path.join(tmp, "tel.jsonl")).readline())
    check("john_doe" not in json.dumps(line), "telemetry hashes paths")
    check(tel.upload() == "disabled", "upload ships disabled")

    # --- recipes FTS ---
    from recipes import migrate as rmig, add_recipe, search, import_stacks
    rdb = os.path.join(tmp, "recipes.db"); rmig(rdb)
    add_recipe(rdb, "cajun_bass_test", "gumbo://v1.cajun.42.30.5", "bass,drums", "smoky")
    add_recipe(rdb, "tokyo_pad", "gumbo://v1.tokyo.7.50.5", "chordal,texture", "")
    hits = search(rdb, "cajun AND bass")
    check(len(hits) == 1 and hits[0]["name"] == "cajun_bass_test", f"FTS search works ({len(hits)} hits)")

    # --- cleanup dedup ---
    from cleanup import clusters, hoard_stats
    cls = clusters(db)
    check(len(cls) >= 3, f"dedup clusters found ({len(cls)} role clusters)")

    # --- watcher ---
    inbox = os.path.join(tmp, "inbox"); os.makedirs(inbox)
    open(os.path.join(inbox, "new_snare.wav"), "wb").write(b"RIFF")
    from watcher import scan_once
    fresh = scan_once(inbox, db, None)
    check("new_snare.wav" in fresh, "watch folder ingests new file")

    # --- touchosc + manifest ---
    from make_touchosc import main as tmain
    from make_demo_manifest import main as mmain
    tmain(os.path.join(tmp, "g.touchosc")); mmain(os.path.join(tmp, "man.json"))
    check(os.path.getsize(os.path.join(tmp, "g.touchosc")) > 500, "touchosc generated")
    check(len(json.load(open(os.path.join(tmp, "man.json")))["slots"]) == 20, "manifest 20 slots")


    # --- taste quiz ---
    from quiz import serve_quiz, answer as qanswer, needs_quiz, mark_shown, skip_quiz
    db2 = os.path.join(tmp, "gumbo2.db")
    db3 = os.path.join(tmp, "gumbo3.db")
    populate(db3)
    check(needs_quiz(db3), "quiz: needed on fresh db")
    check(not needs_quiz(db), "quiz: not needed once events exist")
    q = serve_quiz(db3, 16)
    check(len(q) == 15, f"quiz serves full pool ({len(q)}/15)")
    roles_q = {r.split("/")[-1].split("_")[0] for r in q}
    check(len(roles_q) == 5, f"quiz covers all roles {sorted(roles_q)}")
    for p in q[:4]:
        qanswer(db3, p, keep=not p.endswith("1.wav"))
    check(not needs_quiz(db3), "quiz: satisfied after answers")
    q2 = serve_quiz(db3, 16)
    check(len(q2) == 11 and not any(p in q2 for p in q[:4]), "quiz never repeats")
    skip_quiz(db2)
    check(not needs_quiz(db2), "quiz: skip suppresses without events")

    # --- taste memory: decay + filters ---
    from taste_memory import _decay
    check(abs(_decay(time.time() - 100*86400, time.time()) - 0.5**(100/30)) < 1e-9,
          "decay: 100-day-old event weighs ~0.099")
    recent = get_events(db, since_ts=time.time() - 3600)
    check(len(recent) == 3, "get_events since-filter works")

    # --- ranker cold start + shadow promotion gates ---
    from ranker import train, shadow_report
    from taste_memory import migrate as tmig
    db_cold = os.path.join(tmp, "cold.db")
    sqlite3.connect(db_cold).execute(SAMPLES_SCHEMA)
    tmig(db_cold)
    model, n = train(db_cold)
    check(model is None and n == 0, "ranker cold start refuses to train")
    rep = shadow_report(db)
    check(rep["promote"] is False and rep["n"] < 200, "shadow gate: under 200 disagreements")
    connx = sqlite3.connect(db)
    nowx = time.time()
    for i in range(250):
        win = 1 if i % 5 != 0 else 0     # 80% candidate win rate, all disagreed
        connx.execute("INSERT INTO shadow_log VALUES(?,?,?,?,?)",
                      (nowx, "cook", 0.5, 0.9 if win else 0.1, 1))
    connx.commit(); connx.close()
    rep = shadow_report(db)
    check(rep["promote"] is True and abs(rep["win_rate"] - 0.8) < 0.02,
          "shadow gate: promotes at 80% win rate")

    # --- daily cook: flip + bad input ---
    check(decode("gumbo://v1.flip.42.30.5")["cuisine"] == "flip", "flip cuisine decodes")
    try:
        decode("gumbo://v1.cajun.abc.30.5"); check(False, "non-int seed rejected")
    except ValueError:
        check(True, "non-int seed rejected")

    # --- stack brain: seed-sound mode + vibe fallback ---
    out2 = os.path.join(tmp, "cook2.json")
    res2 = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "stack_brain.py"), "--db", db, "--seed-sound", "/fake/drums_0.wav",
                          "--no-ranker", "--out", out2],
                          capture_output=True, text=True, cwd=os.path.dirname(os.path.abspath(__file__)))
    check(res2.returncode == 0, "stack brain seed-sound mode runs")
    st2 = json.load(open(out2))
    check(st2["layers"][0]["role"] == "drums" and
          st2["layers"][0]["path"] == "/fake/drums_0.wav", "seed-sound anchors the cook")
    res3 = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "stack_brain.py"), "--db", db, "--vibe", "dusty boom bap",
                          "--no-ranker", "--out", os.path.join(tmp, "c3.json")],
                          capture_output=True, text=True, cwd=os.path.dirname(os.path.abspath(__file__)))
    check(res3.returncode == 0 and "laion-clap unavailable" in res3.stderr,
          "vibe mode degrades gracefully without CLAP")

    # --- indexer fallback without librosa ---
    from indexer import analyze_with_role
    check(all(v is None for v in analyze_with_role("/nonexistent/x.wav")),
          "indexer analyze degrades to None without librosa")

    # --- quiz: demo pantry served first ---
    connq = sqlite3.connect(db3)
    connq.execute("INSERT INTO samples (path,filename,duration,bpm,key,analyzed,role,role_conf) "
                  "VALUES (?,?,?,?,?,?,?,?)",
                  ("/fake/pantry/zeta.wav", "zeta.wav", 4.0, 95, "G minor", 1, "drums", 0.7))
    connq.commit(); connq.close()
    q3 = serve_quiz(db3, 16)
    check(q3[0] == "/fake/pantry/zeta.wav", "quiz serves demo pantry first")

    # --- telemetry retention roll ---
    telpath = os.path.join(tmp, "tel_old.jsonl")
    with open(telpath, "w") as f:
        f.write(json.dumps({"v":1,"ts":time.time()-200*86400,"kind":"stir"}) + "\n")
    Telemetry(telpath)
    check(open(telpath).read().strip() == "", "telemetry: >90-day events rolled away")

    # --- recipes: import_stacks ---
    stacks_dir = os.path.join(tmp, "stacks"); os.makedirs(stacks_dir)
    json.dump({"recipe": "gumbo://v1.lagos.9.40.5", "tasting_note": "okra heavy",
               "layers": [{"role": "drums"}]}, open(os.path.join(stacks_dir, "lagos1.json"), "w"))
    rdb2 = os.path.join(tmp, "recipes2.db")
    rmig(rdb2)
    nimp = import_stacks(rdb2, stacks_dir)
    hits2 = search(rdb2, "lagos")
    check(nimp == 1 and len(hits2) == 1 and hits2[0]["recipe"].endswith(".5"),
          "import_stacks -> FTS roundtrip")

    # --- cleanup: compost restores ---
    from cleanup import hoard_stats
    stats = hoard_stats(db, tmp)
    check(stats["samples"] >= 15 and stats["never_cooked_pct"] >= 0,
          f"hoard stats computed ({stats['samples']} samples)")
    d1 = os.path.join(tmp, "dup_a.wav"); d2 = os.path.join(tmp, "dup_b.wav")
    open(d1, "wb").write(b"RIFF"); open(d2, "wb").write(b"RIFF")
    rng2 = np.random.default_rng(11)
    twin = rng2.normal(size=512).astype(np.float32); twin /= np.linalg.norm(twin)
    connd = sqlite3.connect(db)
    connd.execute("INSERT INTO samples (path,filename,duration,bpm,key,analyzed,role,role_conf) "
                  "VALUES (?,?,?,?,?,?,?,?)", (d1, "dup_a.wav", 1.0, 100, None, 1, None, None))
    connd.execute("INSERT INTO samples (path,filename,duration,bpm,key,analyzed,role,role_conf) "
                  "VALUES (?,?,?,?,?,?,?,?)", (d2, "dup_b.wav", 1.0, 100, None, 1, None, None))
    connd.execute("UPDATE samples SET emb=? WHERE path=?", (twin.tobytes(), d1))
    connd.execute("UPDATE samples SET emb=? WHERE path=?", (twin.tobytes(), d2))
    connd.commit(); connd.close()
    from cleanup import compost as do_compost
    grp = [g for g in __import__("cleanup").clusters(db) if d1 in g][0]
    do_compost(db, d1, grp, os.path.join(tmp, "compost"))
    check(os.path.exists(d1) and not os.path.exists(d2)
          and os.path.exists(os.path.join(tmp, "compost", "dup_b.wav")),
          "compost moves duplicates, keeps winner")

    # --- watcher idempotent ---
    fresh2 = scan_once(inbox, db, None)
    check(fresh2 == [], "watch folder: second scan ingests nothing")

    # --- touchosc XML valid ---
    import xml.etree.ElementTree as ET
    troot = ET.parse(os.path.join(tmp, "g.touchosc")).getroot()
    check(len(troot.find("tab")) == 21, "touchosc: 21 well-formed controls")

    # --- THE PERFECT CIRCLE: loop points ---
    from loop_points import find_loop_points, periodicity_score
    sr2 = 22050
    t4 = np.arange(sr2*4)/sr2
    sine = 0.5*np.sin(2*np.pi*220*t4)
    lp = find_loop_points(sine, sr2, length_s=1.0)
    check(lp["score"] > 0.95, f"loop: sine scores periodic ({lp['score']})")
    check(abs((lp["end_s"] - lp["start_s"]) - 1.0) < 0.05, "loop: length honored")
    nz = np.random.default_rng(5).normal(0, .3, sr2*4)
    noise_lp = find_loop_points(nz, sr2, 1.0)
    check(noise_lp["score"] < 0.3, f"loop: noise scores low ({noise_lp['score']})")
    check(periodicity_score(np.zeros(100)) == 0.0, "loop: silence is 0")

    # --- menu board: JS decoder matches Python ---
    import shutil as _sh
    if _sh.which("node"):
        jsrc = "\n".join([
            "const g = require('" + os.path.dirname(os.path.abspath(__file__)) + "/gumbo_decode.js');",
            "const a = g.decode('gumbo://v1.cajun.42.30.5');",
            "let bad;",
            "try { g.decode('gumbo://v1.kansas.1.1.5'); bad = 'no'; }",
            "catch(e) { bad = e.message; }",
            "console.log(JSON.stringify({cuisine: a.cuisine, seed: a.seed, w: a.weirdness, bad: bad}));",
        ])
        jp = os.path.join(tmp, "jcheck.js")
        open(jp, "w").write(jsrc)
        jr = subprocess.run(["node", jp], capture_output=True, text=True)
        j = json.loads(jr.stdout)
        check(j["cuisine"] == "cajun" and j["seed"] == 42 and j["w"] == 0.3,
              "JS decoder: matches python decode")
        check("menu" in j["bad"], "JS decoder: rejects unknown cuisine")
    else:
        check(True, "JS decoder: node unavailable, skipped")

    # --- PANTRY MAP (V7: where Atlas/XO beat us -> now ours) ---
    from pantry_map import project_map, cluster_points, neighbors_of, stack_from_region
    emb_now = __import__("taste_memory").load_embeddings(db)
    pts, annotated = project_map(db)
    check(len(pts) == len(emb_now), f"map projects all embedded samples ({len(pts)})")
    check(all(-1.01 <= xy[0] <= 1.01 and -1.01 <= xy[1] <= 1.01 for xy in pts.values()),
          "map coords normalized to [-1,1]")
    pts_again, _ = project_map(db)
    check(all(abs(pts[p][0]-pts_again[p][0]) < 1e-9 for p in pts), "map deterministic")
    clusters = cluster_points(annotated, cell=0.2)
    check(len(clusters) >= 3, f"clusters found ({len(clusters)})")
    lbl = clusters[0]["label"]
    check(any(r in lbl for r in ["drums","bass","chordal","melodic","texture","mixed"]),
          f"cluster labeled by dominant role ('{lbl}')")
    nbrs = neighbors_of(db, "/fake/drums_0.wav", k=5)
    check(nbrs[0].startswith("/fake/drums"), "neighbors: same role first (replace-with-similar)")
    reg = stack_from_region(db, (0.0, 0.0), radius=2.0)
    check(len(reg) == 5 and set(reg) == {"drums","bass","chordal","melodic","texture"},
          f"region lasso -> role-complete stack ({sorted(reg)})")

    # --- render_daily: preview mix ---
    import wave as _wave
    def _mk_wav(path, freq, sr=22050, secs=2.0):
        tt = np.arange(int(sr*secs))/sr
        pcm = (0.4*np.sin(2*np.pi*freq*tt)*32767).astype(np.int16)
        with _wave.open(path, "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
            w.writeframes(pcm.tobytes())
    w1 = os.path.join(tmp, "l1.wav"); w2 = os.path.join(tmp, "l2.wav")
    _mk_wav(w1, 220); _mk_wav(w2, 330)
    json.dump({"layers": [{"path": w1, "gain": 0.8}, {"path": w2, "gain": 0.8}]},
              open(os.path.join(tmp, "rd.json"), "w"))
    from render_daily import render
    used = render(os.path.join(tmp, "rd.json"), os.path.join(tmp, "rd_out.wav"), seconds=3)
    with _wave.open(os.path.join(tmp, "rd_out.wav")) as w:
        check(w.getnframes() == 3*44100 and w.getnchannels() == 1,
              "daily render: correct duration/format")
    check(set(used) == {"l1.wav", "l2.wav"}, "daily render: mixes all layers")
    check(used == ["l1.wav", "l2.wav"], "daily render: layer order stable")

    print("\n" + ("ALL PASS" if not FAILS else f"{len(FAILS)} FAILURES: {FAILS}"))
    return 1 if FAILS else 0

if __name__ == "__main__":
    sys.exit(main())
