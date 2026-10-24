"""PANTRY MAP - the cutting-board scatter. Projects sample embeddings to 2D,
clusters them, and routes neighborhoods into stacks. numpy-only.

UI contract (JUCE): render points on the board, pan/zoom, drag a lasso ->
stack_from_region fills bowls by role; click a point -> solo audition synced;
right-click -> neighbors menu ("replace with similar").
"""
import numpy as np

def _pca2(X, seed=None):
    """Center + SVD -> first two components, normalized to [-1, 1]."""
    X = np.asarray(X, dtype=np.float64)
    X = X - X.mean(axis=0)
    if X.shape[0] < 3:
        pad = np.zeros((3 - X.shape[0], X.shape[1])); X = np.vstack([X, pad])
    U, S, Vt = np.linalg.svd(X, full_matrices=False)
    P = X @ Vt[:2].T
    span = np.abs(P).max() + 1e-9
    return P / span

def project_map(db, seed=None):
    """Returns {path: (x, y)} in [-1,1]^2. Deterministic for same db+seed."""
    import sqlite3
    conn = sqlite3.connect(db)
    items = [(p, np.frombuffer(b, dtype=np.float32))
             for p, b in conn.execute("SELECT path, emb FROM samples WHERE emb IS NOT NULL")]
    metas = {p: (role, key) for p, role, key in conn.execute(
        "SELECT path, role, key FROM samples")}
    conn.close()
    if not items:
        return {}, []
    paths = [p for p, _ in items]
    P = _pca2(np.array([e for _, e in items]), seed)
    pts = {p: (float(P[i, 0]), float(P[i, 1])) for i, p in enumerate(paths)}
    return pts, [(p, pts[p], metas.get(p, (None, None))) for p in paths]

def cluster_points(annotated, cell=None):
    """Grid-cluster 2D points. Returns [{paths, center, label}].
    Label = dominant role + key, e.g. 'drums - G minor'."""
    if not annotated:
        return []
    pts = np.array([a[1] for a in annotated])
    if cell is None:
        cell = max(0.08, 2.0 / max(2, int(np.sqrt(len(annotated)))))
    def key_of(xy):
        return (int((xy[0] + 1) // cell), int((xy[1] + 1) // cell))
    grid = {}
    for p, xy, _ in annotated:
        grid.setdefault(key_of(xy), []).append(p)
    seen, clusters = set(), []
    for k, members in grid.items():
        if k in seen: continue
        # merge adjacent occupied cells (8-neighborhood, two passes)
        stack, group = [k], []
        while stack:
            ck = stack.pop()
            if ck in seen: continue
            seen.add(ck)
            if ck in grid:
                group += grid[ck]
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nk = (ck[0]+dx, ck[1]+dy)
                        if nk not in seen and nk in grid: stack.append(nk)
        if len(group) < 2:
            continue
        meta = {p: m for p, _, m in annotated}
        roles = [meta[g][0] for g in group if meta.get(g, (None,))[0]]
        keys = [meta[g][1] for g in group if meta.get(g, (None, None))[1]]
        dom_role = max(set(roles), key=roles.count) if roles else "mixed"
        dom_key = max(set(keys), key=keys.count) if keys else ""
        cx = float(np.mean([next(a[1][0] for a in annotated if a[0] == g) for g in group]))
        cy = float(np.mean([next(a[1][1] for a in annotated if a[0] == g) for g in group]))
        clusters.append({"paths": group, "center": (cx, cy),
                         "label": f"{dom_role} - {dom_key}".strip(" -")})
    return clusters

def neighbors_of(db, path, k=8):
    """k nearest by embedding cosine. Powers 'replace with similar'."""
    import sqlite3
    from taste_memory import cosine
    conn = sqlite3.connect(db)
    emb = {p: np.frombuffer(b, dtype=np.float32)
           for p, b in conn.execute("SELECT path, emb FROM samples WHERE emb IS NOT NULL")}
    role = dict(conn.execute("SELECT path, role FROM samples"))
    conn.close()
    if path not in emb: return []
    scored = sorted(((cosine(emb[path], e), p) for p, e in emb.items() if p != path),
                    reverse=True)
    same_role = [p for _, p in scored if role.get(p) == role.get(path)]
    other = [p for _, p in scored if role.get(p) != role.get(path)]
    return (same_role + other)[:k]

def stack_from_region(db, center, radius, roles=("drums","bass","chordal","melodic","texture")):
    """Nearest sample per required role within radius of a map point.
    Feeds stack_brain as seed + fills. Returns {role: path}."""
    pts, annotated = project_map(db)
    out = {}
    for role in roles:
        best, best_d = None, 1e9
        for p, xy, (r, _) in annotated:
            if r != role: continue
            d = (xy[0]-center[0])**2 + (xy[1]-center[1])**2
            if d <= radius*radius and d < best_d:
                best, best_d = p, d
        if best: out[role] = best
    return out
