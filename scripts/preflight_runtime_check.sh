#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ok() { echo "[OK] $*"; }
warn() { echo "[WARN] $*"; }
fail() { echo "[FAIL] $*"; }

has_error=0

check_cmd() {
  local cmd="$1"
  if command -v "$cmd" >/dev/null 2>&1; then
    ok "command available: $cmd"
  else
    fail "missing command: $cmd"
    has_error=1
  fi
}

check_port() {
  local port="$1"
  if ss -ltn "( sport = :$port )" | grep -q ":$port"; then
    warn "port $port is already in use"
  else
    ok "port $port is free"
  fi
}

check_file_exists() {
  local path="$1"
  if [[ -f "$path" ]]; then
    ok "file exists: $path"
  else
    fail "missing file: $path"
    has_error=1
  fi
}

check_exec_exists() {
  local path="$1"
  if [[ -x "$path" ]]; then
    ok "executable exists: $path"
  elif [[ -f "$path" ]]; then
    warn "file exists but not executable: $path"
    has_error=1
  else
    fail "missing executable: $path"
    has_error=1
  fi
}

echo "== whl-air runtime preflight =="
echo "root: $ROOT_DIR"

check_cmd node
check_cmd npm
check_cmd ss

check_port 8898
check_port 8899

check_file_exists "$ROOT_DIR/signaling_server/package.json"
check_file_exists "$ROOT_DIR/signaling_server/src/server.js"

check_exec_exists "$ROOT_DIR/vehicle_client/vehicle_client_app"
check_exec_exists "$ROOT_DIR/cockpit_client/cockpit_client_app"

check_file_exists "$ROOT_DIR/configs/vehicle_client_conf.txt"
check_file_exists "$ROOT_DIR/configs/cockpit_client_conf.txt"

if [[ $has_error -ne 0 ]]; then
  echo
  fail "preflight failed: see missing items above"
  echo "suggested next steps:"
  echo "  1) start signaling server with: cd signaling_server && npm install && npm start"
  echo "  2) build vehicle/cockpit binaries via your internal toolchain (repo currently has no top-level CMakeLists.txt)"
  echo "  3) create runtime config files under configs/*.txt"
  exit 1
fi

echo
ok "preflight passed"
exit 0
