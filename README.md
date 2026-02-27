# whl-air Remote Driving System

This project implements a real-time remote driving system for autonomous vehicles, featuring real-time video transmission from the vehicle to a control center (cockpit) and bidirectional data communication (control commands, telemetry) using the **Google WebRTC C++ library**.

It is built with a **modular architecture**, utilizes **WebRTC Data Channels for reliable and unreliable data transfer**, and is designed for **real-time performance**, **security**, and **extensibility**.

---

## ✨ Features

-   **Real-time Video Streaming:** Low-latency video transmission from the vehicle to the cockpit via WebRTC media channels.
-   **Structured Data Communication:**
    * **Control Commands:** Send vehicle control inputs (throttle, brake, steering, gear) and high-level/emergency commands (e.g., emergency stop, pull over) from the cockpit to the vehicle.
    * **Vehicle Telemetry:** Receive real-time chassis status (speed, gear, etc.) from the vehicle in the cockpit.
    * **WebRTC Data Channels:** Data communication is handled securely and efficiently over dedicated Data Channels established within the WebRTC connection, supporting both reliable/ordered (SCTP) and unreliable/unordered (DCCP over DTLS) modes as configured.
-   **Google Protobuf:** Utilizes Protobuf for defining and serializing/deserializing structured control and telemetry messages, ensuring type safety and efficiency.
-   **Secure Transmission:** Leverages WebRTC's built-in security (DTLS/SRTP for media, DTLS for Data Channels) and TLS for WebSocket signaling.
-   **NAT Traversal:** Supports navigating network address translation using STUN and TURN servers (configurable).
-   **Web-based Cockpit Interface:** Provides a local web server serving an HTML/JavaScript interface for displaying video/status and receiving user input.
-   **Modular and Extensible Architecture:** Designed with clear interfaces for easy replacement or extension of components (e.g., different sensor inputs, control logic, signaling protocols, Web UI frameworks).

---

## 🏗️ Architecture Overview

The system consists of three main components:

1.  **Signaling Server:** A central server (typically runs on a public IP) that helps the Vehicle Client and Cockpit Client discover each other and exchange initial WebRTC signaling messages (SDP offers/answers, ICE candidates). Once signaling is complete, video and data flow directly between the clients.
2.  **Vehicle Client (`vehicle_client_app`):** Runs on the autonomous vehicle. It captures video from cameras, reads chassis status, establishes a WebRTC connection with the Cockpit Client (via the Signaling Server), sends video via a media track, sends telemetry via a Data Channel, and receives/processes control commands via another Data Channel.
3.  **Cockpit Client (`cockpit_client_app`):** Runs on the remote control station. It establishes a WebRTC connection with the Vehicle Client (via the Signaling Server), receives video (displayed in a web browser), receives telemetry via a Data Channel (displayed in the web browser), takes user input (joystick, keyboard, UI), formats control commands, and sends them via a Data Channel to the vehicle. It includes a local HTTP/WebSocket server (`transport_server`) to serve the web-based user interface (`display`) and relay data/commands between the C++ backend and the browser frontend.

```mermaid
graph TD
    subgraph Vehicle
        V[Vehicle Client App]
        VC(Camera Source)
        VS(Chassis Source)
    end

    subgraph Cockpit
        C[Cockpit Client App]
        CD(Command Handler)
        CT(Telemetry Handler)
        CTrans(Transport Server)
        CDisplay(Web Browser Display)
    end

    S[Signaling Server]

    V -->|Signaling| S
    C -->|Signaling| S
    V -->|WebRTC (Video, Data Channels)| C

    VC -->|Video Frames| V
    VS -->|Telemetry Data| V -->|Telemetry DataChannel| C -->|Telemetry Data| CT -->|Telemetry (JSON)| CTrans
    CD -->|Control Data| C -->|Control DataChannel| V -->|Control Data| CD@Vehicle[Controller]

    C -->|Commands (JSON/WS)| CTrans
    CTrans -->|Static Files, WS| CDisplay
    CDisplay -->|User Input (WS)| CTrans
    CTrans -->|Telemetry (WS)| CDisplay
````

-----

## 🚀 Quick Start

### Requirements

  - Ubuntu 22.04+
  - C++17 compatible compiler (e.g., GCC 9+)
  - OpenSSL development libraries
  - Protobuf Compiler (`protoc`)
  - Protobuf C++ runtime libraries
  - A C++ JSON library (e.g., [RapidJSON](https://github.com/Tencent/rapidjson), [nlohmann/json](https://github.com/nlohmann/json))
  - A C++ WebSocket library (e.g., [websocketpp](https://www.google.com/search?q=https://github.com/zaphoyere/websocketpp), [Boost.Beast](https://www.boost.org/doc/libs/release/libs/beast/))
  - A pre-built Google [WebRTC C++ library](https://webrtc.googlesource.com/src/) (libwebrtc) built for your target platform. Integrating libwebrtc requires careful setup and configuration.

### Build

Ensure all requirements are met and dependencies (especially the pre-built WebRTC library) are correctly referenced in your environment or CMake configuration.

```bash
# Navigate to the project root directory
cd whl-air/ # Or wherever your project is located

# Create a build directory
mkdir build && cd build

# Configure the build (adjust -DWEBRTC_ROOT if needed)
cmake .. -DCMAKE_BUILD_TYPE=Release # Add -DWEBRTC_ROOT=/path/to/your/webrtc/build if needed

# Build the project
make -j$(nproc)
```

### Build (Bzlmod, Minimal Smoke Path)

This repository now includes a minimal Bazel + Bzlmod smoke pipeline to verify toolchain and basic test execution first.

```bash
# from repo root
bazel test //:rtc_sanity_smoke_test

# optional: run binary directly
bazel run //:rtc_sanity_smoke
```

What this validates:

- Bzlmod resolution works (`MODULE.bazel`).
- C++ toolchain compile/link path works.
- Basic RTC-like step state machine runs end-to-end.

This is intentionally minimal and should be used as stage-0 stability baseline before full WebRTC/libwebrtc integration build.

### Build (Bzlmod, Stage-3 Header Compile Gate)

Use this stage to catch cross-module include/type regressions early (before linking full runtime dependencies):

```bash
bazel test //:rtc_headers_compile_test
```

Recommended quick regression sequence:

```bash
bazel test //:rtc_sanity_smoke_test //:rtc_headers_compile_test
```

### Build (Bzlmod, Stage-4 Signaling Message Behavior Gate)

Use this stage to validate signaling message serialization/deserialization compatibility (including candidate field variants):

```bash
bazel test //:signaling_message_test
```

Recommended staged regression sequence:

```bash
bazel test //:rtc_sanity_smoke_test //:rtc_headers_compile_test //:signaling_message_test
```

### Build (Bzlmod, Stage-5 Signaling Compatibility Gate)

`signaling_message_test` now also validates cross-implementation candidate compatibility:

- Flat candidate payloads and nested candidate payloads.
- Both `sdpMlineIndex` and `sdpMLineIndex` key variants.

Use the same staged regression command to verify all gates:

```bash
bazel test //:rtc_sanity_smoke_test //:rtc_headers_compile_test //:signaling_message_test
```

### Build/Test (Stage-6 Signaling Server Route Gate)

Validate JavaScript signaling route compatibility without real network I/O:

```bash
node --test signaling_server/tests/session_manager_route.test.js
```

Recommended cross-stack regression sequence:

```bash
bazel test //:rtc_sanity_smoke_test //:rtc_headers_compile_test //:signaling_message_test && node --test signaling_server/tests/session_manager_route.test.js
```

### Build/Test (Stage-7 Signaling Server Full Integration Gate)

Run full signaling server validation against a real WebSocket server process (JWT auth, join/leave, offer/answer/candidate routing, offline recipient error, malformed message handling, and disconnect notification):

```bash
cd signaling_server
npm run test:all
```

For CI from repo root:

```bash
cd signaling_server && npm run test:all
```

### Build/Test (Stage-8 One-Command Full Signaling Verification)

Use a single script to run both C++ signaling compatibility gates and Node signaling server full integration gates:

```bash
./scripts/verify_signaling_full.sh
```

This script verifies:

- `//:signaling_message_test`
- `//:rtc_headers_compile_test`
- `signaling_server` route + integration tests (`npm run test:all`)

### Build/Test (Client Launchers Availability Gate)

The repo now provides runnable launcher paths expected by the runbook:

- `./vehicle_client/vehicle_client_app`
- `./cockpit_client/cockpit_client_app`

They invoke Bazel-built binaries (`//:vehicle_client_app_bin`, `//:cockpit_client_app_bin`) and support `--help` / `--config` arguments.

Validate both launchers and binary startup parameters with:

```bash
bazel test //:client_launchers_smoke_test
```

### Configuration

Create configuration files based on the examples (not provided in skeleton, but implied by `VehicleConfig` and `CockpitConfig`) for the vehicle and cockpit clients. These files will specify signaling server addresses, peer IDs, sensor settings, DataChannel labels, ICE server configurations, etc.

  * `configs/vehicle_client_conf.txt`
  * `configs/cockpit_client_conf.txt`

Ensure the `client_id` in each config is unique and matches the `target_vehicle_id` in the cockpit config for establishing a PeerConnection. DataChannel labels (`control_channel_label`, `telemetry_channel_label`) must also match between the vehicle and cockpit configurations.

### Run

Before starting services, run a runtime preflight check:

```bash
./scripts/preflight_runtime_check.sh
```

1.  **Start the Signaling Server:**
    This server needs to be accessible from both the vehicle and cockpit clients.

    ```bash
    cd signaling_server
    npm install
    npm start
    ```

    Notes:
    - Default signaling port is `8081` (see `signaling_server/config.js`).
    - JWT authentication is enabled by default; clients must provide a valid token.

2.  **Start the Vehicle Client:**
    Run the vehicle application on the vehicle hardware.

    ```bash
    ./vehicle_client/vehicle_client_app --config ./configs/vehicle_client_conf.txt
    ```

    Note: this binary/config must be generated by your internal C++ build/runtime pipeline before running.

3.  **Start the Cockpit Client:**
    Run the cockpit application on the remote control station. This will start the embedded local web server.

    ```bash
    ./cockpit_client/cockpit_client_app --config ./configs/cockpit_client_conf.txt
    ```

    Note: this binary/config must be generated by your internal C++ build/runtime pipeline before running.

4.  **Access the Cockpit Web Interface:**
    Open a web browser on the cockpit machine and navigate to the address and port configured for the local `transport_server` (e.g., `http://localhost:8080/` or `http://127.0.0.1:8080/`). The JavaScript running in the browser will connect via WebSocket to the local C++ backend, initiate the WebRTC signaling process via this WebSocket connection, and display the video/status streams once the WebRTC connection is established with the vehicle.

### Minimal RTC Smoke Test

For step-by-step stabilization, run a browser-only local loopback test first (no signaling server, no vehicle client required):

1. Start `cockpit_client_app` so static files are served by `transport_server`.
2. Open `http://localhost:8080/rtc_smoke_test.html` (adjust host/port to your config).
3. Click **Run Test**.

Expected pass criteria:

- `pc1/pc2` created successfully.
- Offer/Answer local+remote descriptions are applied.
- ICE state advances beyond `new/checking`.
- DataChannel `ping/pong` succeeds.

If this smoke test fails, prioritize fixing WebRTC runtime/config issues before debugging full vehicle↔cockpit signaling and media pipeline.

### Media Smoke Test (Stage-2)

After the basic loopback smoke test passes, verify media track negotiation (`addTrack` / `ontrack`):

1. Keep `cockpit_client_app` running.
2. Open `http://localhost:8080/rtc_media_smoke_test.html`.
3. Allow camera permission when prompted.
4. Click **Run Media Test**.

Expected pass criteria:

- `getUserMedia` succeeds.
- `pc1.addTrack(...)` executes for local camera track.
- Offer/Answer local+remote descriptions are applied.
- Remote side receives `ontrack` and renders stream.

### WebRTC Integration Test (Stage-7: Media + Command + NAT Traversal)

Run an end-to-end browser integration test that validates all three core capabilities in one pass:

1. Video/image transfer over WebRTC media track.
2. Command transfer over WebRTC DataChannel (command + ack).
3. NAT traversal capability (`srflx`/`relay` ICE candidate presence).

Steps:

1. Keep `cockpit_client_app` running so static files are served.
2. Open `http://localhost:8080/rtc_integration_test.html`.
3. Allow camera permission.
4. Click **Run Integration Test**.

Expected pass criteria:

- Remote side receives video track and inbound video RTP stats become non-zero.
- DataChannel command `EMERGENCY_STOP` gets acked.
- ICE candidate collection contains at least one non-host candidate (`srflx` or `relay`).

If NAT traversal fails in local network, verify outbound UDP policy and provide a valid TURN server for relay candidates.

### WebRTC Local Integration Test (Stage-8: Mock Video + Command)

Keep the Stage-7 NAT traversal version for field validation, and use this local-only version for fast development testing when camera/NAT conditions are unavailable.

Capabilities verified:

1. Video transfer over WebRTC using mock canvas video stream (no physical camera required).
2. Control command transfer over DataChannel with command/ack round-trip.

Steps:

1. Keep `cockpit_client_app` running.
2. Open `http://localhost:8080/rtc_local_integration_test.html`.
3. Click **Run Local Test**.

Expected pass criteria:

- Remote side receives mock video track and inbound video RTP stats become non-zero.
- Control command (`FORWARD`) is acknowledged over DataChannel.
- ICE state reaches `connected` or `completed` in loopback.

### Fully Automated Local E2E Test (Headless)

Run the Stage-8 local integration test fully automatically in a headless browser:

```bash
cd cockpit_client/display
npm install
npm run install:browsers:cn
npm run test:e2e:local
```

What this automation does:

- Starts a temporary local static server for `public/`.
- Opens `rtc_local_integration_test.html` in Chromium via Playwright.
- Clicks **Run Local Test** automatically.
- Waits for `PASS:` status and returns non-zero on failure.

Optional debug mode (visible browser):

```bash
cd cockpit_client/display
npm run test:e2e:local:debug
```

### Intranet Full-Function Validation (Remote Driving)

For intranet acceptance, use the full-function validation page and automation flow to verify core remote-driving capabilities end-to-end:

- Video transmission (mock vehicle camera track over WebRTC).
- Control command transmission (`FORWARD` / `STOP` / `EMERGENCY_STOP`) with `cmdId`-matched ack and latency measurement.
- Telemetry feedback transmission (`speed_mps`, `gear`, etc.).
- Transport observability and thresholds from `getStats()` (`fps`, bitrate, packet loss, jitter, freeze count).

Manual page:

```text
http://localhost:8080/rtc_remote_driving_validation_test.html
```

Automated run (headless):

```bash
cd cockpit_client/display
npm install
npm run install:browsers:cn
npm run test:e2e:remote-driving
```

Stability regression (5 consecutive passes):

```bash
cd cockpit_client/display
npm run test:e2e:remote-driving:loop
```

Degraded-network regression:

```bash
cd cockpit_client/display
npm run test:e2e:remote-driving:degraded
```

Degraded-network stability loop:

```bash
cd cockpit_client/display
npm run test:e2e:remote-driving:degraded:loop
```

Pass criterion for intranet validation:

- Status shows `PASS: video + control + telemetry validation passed`.
- Loop run completes all rounds without failure.

On failure, automated artifacts are generated at:

- `cockpit_client/display/tests/artifacts/*.png` (screenshot)
- `cockpit_client/display/tests/artifacts/*.txt` (status, metrics, logs)

### Takeover Safety Validation (Auto-driving Handover)

This scenario validates autonomous-driving takeover safety state transitions and command closure:

- Mode transition chain: `AUTO -> REMOTE_CONTROL -> SAFE_STOP -> AUTO`.
- Command closure with `cmdId` ack: `TAKEOVER_REQUEST`, `FORWARD`, `EMERGENCY_STOP`, `RECOVER_AUTO`.
- Telemetry consistency checks for mode, speed, and emergency flag.

Automated run:

```bash
cd cockpit_client/display
npm run test:e2e:takeover-safety
```

Stability regression (5 consecutive passes):

```bash
cd cockpit_client/display
npm run test:e2e:takeover-safety:loop
```

Pass criterion:

- Status shows `PASS: takeover + safety state validation passed`.
- Mode timeline includes required ordered transitions.
- Command ack latency p95 remains within threshold.
