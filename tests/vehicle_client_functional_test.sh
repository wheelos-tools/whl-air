#!/usr/bin/env bash
set -euo pipefail

vehicle_bin="$1"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

config="$tmpdir/vehicle_client_test.conf"
cat >"$config" <<'EOF'
# vehicle bootstrap functional test config
client_id=vehicle_test_alpha
camera_fps=20
telemetry_interval_ms=120
heartbeat_interval_ms=300
control_channel_label=control
telemetry_channel_label=telemetry
EOF

out_file="$tmpdir/run.log"
"$vehicle_bin" --config "$config" --self-test >"$out_file" 2>&1

if ! grep -q "VEHICLE_SELF_TEST_PASS" "$out_file"; then
  echo "Expected VEHICLE_SELF_TEST_PASS in output" >&2
  cat "$out_file" >&2
  exit 1
fi

if ! grep -q "VEHICLE_LEGACY_FEATURES_PASS" "$out_file"; then
  echo "Expected VEHICLE_LEGACY_FEATURES_PASS in output" >&2
  cat "$out_file" >&2
  exit 1
fi

if ! grep -q "mode=AUTO" "$out_file"; then
  echo "Expected final mode AUTO in output" >&2
  cat "$out_file" >&2
  exit 1
fi

echo "vehicle_client_functional_test passed"
