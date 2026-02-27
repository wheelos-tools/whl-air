#include <iostream>
#include <string>

#include "vehicle_client/domain/runtime_state.h"

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
  vehicle_domain::RuntimeState runtime;

  vehicle_domain::PushMode(&runtime, runtime.mode);

  ok &= Check(vehicle_domain::ApplyCommand(&runtime, "control_channel",
                                           "TAKEOVER_REQUEST"),
              "takeover accepted from auto");
  ok &= Check(
      vehicle_domain::ApplyCommand(&runtime, "control_channel", "FORWARD"),
      "forward accepted in remote control");
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    ok &= Check(runtime.mode == "REMOTE_CONTROL", "remote control mode");
    ok &= Check(runtime.gear == 1, "gear set on takeover");
    ok &= Check(runtime.speed_mps == 6.0, "speed set on forward");
    ok &= Check(runtime.stats.control_messages_received == 2,
                "control message count");
  }

  ok &= Check(vehicle_domain::ApplyCommand(&runtime, "control_channel",
                                           "EMERGENCY_STOP"),
              "emergency stop accepted");
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    ok &= Check(runtime.mode == "SAFE_STOP", "emergency mode safe stop");
    ok &= Check(runtime.emergency, "emergency flag set");
    ok &= Check(runtime.speed_mps == 0.0, "speed reset on emergency");
    ok &= Check(runtime.stats.emergency_commands_received >= 1,
                "emergency counter incremented");
  }

  ok &= Check(
      vehicle_domain::ApplyCommand(&runtime, "control_channel", "RECOVER_AUTO"),
      "recover auto accepted from safe stop");
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    ok &= Check(runtime.mode == "AUTO", "recover to auto mode");
    ok &= Check(!runtime.emergency, "recover clears emergency");
  }

  vehicle_domain::OnNetworkDown(&runtime);
  vehicle_domain::OnTelemetryMessageReceived(&runtime);
  vehicle_domain::OnWebrtcError(&runtime);
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    ok &=
        Check(runtime.mode == "SAFE_STOP", "network/webrtc enforce safe stop");
    ok &= Check(runtime.stats.network_down_events == 1, "network down count");
    ok &= Check(runtime.stats.telemetry_messages_received == 1,
                "telemetry receive count");
    ok &= Check(runtime.stats.webrtc_errors == 1, "webrtc error count");
    ok &= Check(!runtime.stats.mode_timeline.empty(), "mode timeline updated");
  }

  if (!ok) {
    return 1;
  }

  std::cout << "vehicle_runtime_state_test passed" << std::endl;
  return 0;
}
