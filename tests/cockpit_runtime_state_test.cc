#include <iostream>
#include <string>

#include "cockpit_client/domain/runtime_state.h"

namespace {

bool Check(bool cond, const std::string& msg) {
  if (!cond) {
    std::cerr << "FAILED: " << msg << std::endl;
    return false;
  }
  return true;
}

}  // namespace

int main() {
  bool ok = true;
  cockpit_domain::RuntimeState runtime;

  ok &=
      Check(!cockpit_domain::ApplyCommand(&runtime, "web", "TAKEOVER_REQUEST"),
            "takeover rejected while disconnected");

  cockpit_domain::OnPeerConnected(&runtime);
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    ok &= Check(runtime.connected, "peer connected sets connected");
    ok &= Check(runtime.session_state == "CONNECTED", "peer connected state");
    ok &= Check(runtime.vehicle_mode == "AUTO", "peer connected vehicle mode");
    ok &=
        Check(!runtime.stats.state_timeline.empty(), "state timeline updated");
    ok &= Check(runtime.stats.state_timeline.back() == "CONNECTED",
                "state timeline connected");
  }

  ok &=
      Check(cockpit_domain::ApplyCommand(&runtime, "input", "TAKEOVER_REQUEST"),
            "takeover accepted while connected");
  ok &= Check(cockpit_domain::ApplyCommand(&runtime, "input", "FORWARD"),
              "forward accepted in remote control");
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    ok &= Check(runtime.session_state == "REMOTE_CONTROL",
                "remote control state");
    ok &=
        Check(runtime.vehicle_mode == "REMOTE_CONTROL", "remote control mode");
    ok &= Check(runtime.expected_speed_mps == 6.0, "forward speed set");
    ok &= Check(runtime.stats.input_commands_received == 2,
                "input command count");
  }

  cockpit_domain::OnHeartbeatLost(&runtime);
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    ok &= Check(runtime.session_state == "SAFE_STOP", "heartbeat lost state");
    ok &= Check(runtime.emergency, "heartbeat lost emergency true");
    ok &= Check(runtime.stats.heartbeat_lost_events == 1,
                "heartbeat lost counter");
  }

  cockpit_domain::OnNetworkUp(&runtime);
  ok &= Check(cockpit_domain::ApplyCommand(&runtime, "web", "RECOVER_AUTO"),
              "recover auto accepted in safe stop when connected");
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    ok &= Check(runtime.session_state == "CONNECTED", "recover to connected");
    ok &= Check(runtime.vehicle_mode == "AUTO", "recover to auto");
    ok &= Check(!runtime.emergency, "recover clears emergency");
    ok &= Check(runtime.stats.ws_messages_received == 1, "web command count");
    ok &= Check(runtime.stats.command_acks == runtime.stats.commands_sent,
                "acks track command sends");
  }

  if (!ok) {
    return 1;
  }

  std::cout << "cockpit_runtime_state_test passed" << std::endl;
  return 0;
}
