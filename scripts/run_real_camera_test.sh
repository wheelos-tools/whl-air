#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Starting real-camera integration test sequence"
echo "Repo root: $ROOT_DIR"

cd "$ROOT_DIR"

# Start signaling server on requested port (8898)
echo "Starting signaling_server on port 8898..."
(
  cd "$ROOT_DIR/signaling_server"
  PORT=8898 npm start >/tmp/whl_signaling_8898.log 2>&1 &
  echo $! > /tmp/whl_signaling_8898.pid
)

sleep 1

echo "Starting vehicle client (foreground)..."
"$ROOT_DIR/vehicle_client/vehicle_client_app" --config "$ROOT_DIR/configs/vehicle_client_conf.txt" &
echo $! > /tmp/whl_vehicle.pid

sleep 1

echo "Starting static frontend for cockpit on port 8899 (serves display/public)..."
(
  cd "$ROOT_DIR/cockpit_client/display/public"
  python3 -m http.server 8899 >/tmp/whl_cockpit_static_8899.log 2>&1 &
  echo $! > /tmp/whl_cockpit_static_8899.pid
)

sleep 1

echo
echo "Services started. Open the cockpit UI in your browser: http://localhost:8899/"
echo "Signaling server logs: /tmp/whl_signaling_8898.log"
echo "Vehicle client PID: $(cat /tmp/whl_vehicle.pid)"
echo "Static UI server PID: $(cat /tmp/whl_cockpit_static_8899.pid)"

echo "To stop all test processes:"
echo "  kill $(cat /tmp/whl_signaling_8898.pid) || true"
echo "  kill $(cat /tmp/whl_vehicle.pid) || true"
echo "  kill $(cat /tmp/whl_cockpit_static_8899.pid) || true"

echo "Test notes:"
echo "- The cockpit frontend served at port 8899 will attempt to connect to local transport server (normally provided by cockpit_client_app)."
echo "- For a full end-to-end run using the C++ transport server, ensure the 'cockpit_client_app' is started and its transport server is configured to 8899 (config file: configs/cockpit_client_conf.txt)."

exit 0
