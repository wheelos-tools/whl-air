#!/usr/bin/env bash
set -euo pipefail

vehicle_bin="$1"
cockpit_bin="$2"
vehicle_launcher="$3"
cockpit_launcher="$4"

[[ -x "$vehicle_bin" ]]
[[ -x "$cockpit_bin" ]]
[[ -x "$vehicle_launcher" ]]
[[ -x "$cockpit_launcher" ]]

"$vehicle_bin" --help >/dev/null
"$cockpit_bin" --help >/dev/null

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

vehicle_cfg="$tmpdir/vehicle.cfg"
cockpit_cfg="$tmpdir/cockpit.cfg"

touch "$vehicle_cfg" "$cockpit_cfg"

"$vehicle_bin" --config "$vehicle_cfg" --run-seconds 1 >/dev/null
"$cockpit_bin" --config "$cockpit_cfg" --run-seconds 1 >/dev/null

echo "client_launchers_smoke_test passed"
