#include "vehicle_client/domain/runtime_state.h"

namespace vehicle_domain {

void PushMode(RuntimeState* runtime, const std::string& mode) {
  if (runtime->stats.mode_timeline.empty() ||
      runtime->stats.mode_timeline.back() != mode) {
    runtime->stats.mode_timeline.push_back(mode);
  }
}

void OnPeerConnected(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->peer_connected = true;
  runtime->mode = runtime->emergency ? "SAFE_STOP" : "AUTO";
  runtime->speed_mps = 0.0;
  runtime->gear = runtime->emergency ? 0 : runtime->gear;
  PushMode(runtime, runtime->mode);
  runtime->stats.peer_connected_events += 1;
}

void OnPeerDisconnected(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->peer_connected = false;
  runtime->speed_mps = 0.0;
  runtime->gear = 0;
  runtime->mode = runtime->emergency ? "SAFE_STOP" : "AUTO";
  PushMode(runtime, runtime->mode);
  runtime->stats.peer_disconnected_events += 1;
}

void OnNetworkDown(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->peer_connected = false;
  runtime->mode = "SAFE_STOP";
  runtime->emergency = true;
  runtime->speed_mps = 0.0;
  runtime->gear = 0;
  PushMode(runtime, runtime->mode);
  runtime->stats.network_down_events += 1;
  runtime->stats.emergency_commands_received += 1;
}

void OnNetworkUp(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->peer_connected = true;
  runtime->mode = runtime->emergency ? "SAFE_STOP" : "AUTO";
  runtime->speed_mps = 0.0;
  PushMode(runtime, runtime->mode);
  runtime->stats.network_up_events += 1;
}

void OnHeartbeatLost(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->mode = "SAFE_STOP";
  runtime->emergency = true;
  runtime->speed_mps = 0.0;
  runtime->gear = 0;
  PushMode(runtime, runtime->mode);
  runtime->stats.heartbeat_lost_events += 1;
  runtime->stats.emergency_commands_received += 1;
}

void OnTelemetryMessageReceived(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->stats.telemetry_messages_received += 1;
}

void OnWebrtcError(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->stats.webrtc_errors += 1;
  runtime->mode = "SAFE_STOP";
  runtime->emergency = true;
  runtime->speed_mps = 0.0;
  runtime->gear = 0;
  runtime->stats.emergency_commands_received += 1;
  PushMode(runtime, runtime->mode);
}

bool ApplyCommand(RuntimeState* runtime, const std::string& source,
                  const std::string& command_name) {
  std::lock_guard<std::mutex> lock(runtime->mutex);

  if (source == "control_channel") {
    runtime->stats.control_messages_received += 1;
  }
  if (command_name == "EMERGENCY_STOP") {
    runtime->stats.emergency_commands_received += 1;
  }

  if (command_name == "TAKEOVER_REQUEST" && runtime->mode == "AUTO") {
    runtime->mode = "REMOTE_CONTROL";
    runtime->gear = 1;
    PushMode(runtime, runtime->mode);
  } else if (command_name == "FORWARD" && runtime->mode == "REMOTE_CONTROL" &&
             !runtime->emergency) {
    runtime->speed_mps = 6.0;
  } else if (command_name == "STOP" && runtime->mode == "REMOTE_CONTROL") {
    runtime->speed_mps = 0.0;
  } else if (command_name == "EMERGENCY_STOP") {
    runtime->mode = "SAFE_STOP";
    runtime->emergency = true;
    runtime->speed_mps = 0.0;
    runtime->gear = 0;
    PushMode(runtime, runtime->mode);
  } else if (command_name == "RECOVER_AUTO" && runtime->mode == "SAFE_STOP") {
    runtime->mode = "AUTO";
    runtime->emergency = false;
    runtime->speed_mps = 0.0;
    runtime->gear = 0;
    PushMode(runtime, runtime->mode);
  } else {
    return false;
  }

  runtime->stats.command_acks += 1;
  return true;
}

}  // namespace vehicle_domain
