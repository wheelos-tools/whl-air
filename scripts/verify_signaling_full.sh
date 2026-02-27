#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ok() { echo "[OK] $*"; }
info() { echo "[INFO] $*"; }
fail() { echo "[FAIL] $*"; }

require_cmd() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    fail "missing required command: $cmd"
    exit 1
  fi
}

run_step() {
  local title="$1"
  shift
  info "$title"
  "$@"
  ok "$title"
}

main() {
  info "whl-air full signaling verification"
  info "repo root: $ROOT_DIR"

  require_cmd bazel
  require_cmd node
  require_cmd npm

  cd "$ROOT_DIR"

  run_step \
    "C++ signaling compatibility gate (Bazel)" \
    bazel test //:signaling_message_test //:rtc_headers_compile_test

  cd "$ROOT_DIR/signaling_server"

  if [[ ! -d node_modules ]]; then
    run_step "Install signaling_server dependencies" npm install --no-audit --no-fund
  else
    info "signaling_server/node_modules already present, skipping npm install"
  fi

  run_step "Node signaling route + integration gate" npm run test:all

  echo
  ok "full signaling verification passed"
}

main "$@"
