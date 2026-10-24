"""Opt-in telemetry: local JSONL first, privacy-clean (hash paths, never raw).
90-day rolling retention. Upload POST ships disabled."""
import hashlib, json, os, time

RETENTION_DAYS = 90

def _hash_path(p):
    return hashlib.sha256(p.encode()).hexdigest()[:16]

def sanitize(event):
    e = dict(event)
    for k in ("path", "file", "sample_path"):
        if k in e: e[k] = _hash_path(e[k])
    return e

class Telemetry:
    def __init__(self, path, enabled=False):
        self.path = path
        self.enabled = enabled   # upload stays OFF by default; local always ok
        self._roll()

    def _roll(self):
        if not os.path.exists(self.path): return
        now = time.time(); keep = []
        with open(self.path) as f:
            for line in f:
                try:
                    if now - json.loads(line)["ts"] < RETENTION_DAYS * 86400:
                        keep.append(line)
                except Exception: pass
        with open(self.path, "w") as f: f.writelines(keep)

    def emit(self, kind, **kv):
        ev = {"v": 1, "ts": time.time(), "kind": kind, **sanitize(kv)}
        with open(self.path, "a") as f: f.write(json.dumps(ev) + "\n")

    def upload(self, url=None):
        if not self.enabled: return "disabled"
        return "stub - configure endpoint + privacy policy first"
