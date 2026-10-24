#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
command -v node >/dev/null || { echo 'Install Node.js 20 or newer from https://nodejs.org then run this again.'; exit 1; }
node -e 'if(Number(process.versions.node.split(".")[0])<20)process.exit(1)' || { echo 'Node.js 20+ is required.'; exit 1; }
command -v npm >/dev/null || { echo 'npm is required. Reinstall Node.js with npm included.'; exit 1; }
npm ci --ignore-scripts --no-fund --no-audit
npm run build
node browser/server.mjs &
gumbo_server_pid=$!
trap 'kill "$gumbo_server_pid" 2>/dev/null || true' EXIT INT TERM
for attempt in {1..50}; do
  kill -0 "$gumbo_server_pid" 2>/dev/null || { echo 'Could not start the kitchen. Is port 5173 already in use?'; exit 1; }
  if curl -fsS http://127.0.0.1:5173/api/status >/dev/null 2>&1; then
    open http://127.0.0.1:5173
    wait "$gumbo_server_pid"
    exit $?
  fi
  sleep 0.2
done
echo 'The kitchen did not start in time. See the error above.'
exit 1
