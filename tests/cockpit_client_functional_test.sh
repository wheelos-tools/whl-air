#!/usr/bin/env bash
set -euo pipefail

cockpit_bin="$1"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

config="$tmpdir/cockpit_client_test.conf"
cat >"$config" <<'EOF'
# cockpit runtime functional test config
client_id=cockpit_test_alpha
target_vehicle_id=vehicle_test_alpha
heartbeat_interval_ms=250
telemetry_interval_ms=120
connect_delay_ms=100
control_channel_label=control
telemetry_channel_label=telemetry
EOF

out_file="$tmpdir/run.log"
"$cockpit_bin" --config "$config" --self-test >"$out_file" 2>&1

if ! grep -q "COCKPIT_SELF_TEST_PASS" "$out_file"; then
  echo "Expected COCKPIT_SELF_TEST_PASS in output" >&2
  cat "$out_file" >&2
  exit 1
fi

if ! grep -q "COCKPIT_LEGACY_FEATURES_PASS" "$out_file"; then
  echo "Expected COCKPIT_LEGACY_FEATURES_PASS in output" >&2
  cat "$out_file" >&2
  exit 1
fi

if ! grep -q "state=CONNECTED" "$out_file"; then
  echo "Expected final state CONNECTED in output" >&2
  cat "$out_file" >&2
  exit 1
fi

echo "cockpit_client_functional_test passed"
