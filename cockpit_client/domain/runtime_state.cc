#include "cockpit_client/domain/runtime_state.h"

namespace cockpit_domain {

void PushState(RuntimeState* runtime, const std::string& state) {
  if (runtime->stats.state_timeline.empty() ||
      runtime->stats.state_timeline.back() != state) {
    runtime->stats.state_timeline.push_back(state);
  }
}

void OnWsConnected(RuntimeState* runtime, int conn_id) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->ws_clients.insert(conn_id);
  runtime->stats.ws_connected_events += 1;
}

void OnWsDisconnected(RuntimeState* runtime, int conn_id) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->ws_clients.erase(conn_id);
  runtime->stats.ws_disconnected_events += 1;
}

void OnPeerConnected(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->connected = true;
  if (!runtime->emergency) {
    runtime->session_state = "CONNECTED";
    runtime->vehicle_mode = "AUTO";
    runtime->expected_speed_mps = 0.0;
  }
  PushState(runtime, runtime->session_state);
  runtime->stats.peer_connected_events += 1;
}

void OnPeerDisconnected(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->connected = false;
  runtime->session_state = "DISCONNECTED";
  runtime->vehicle_mode = "AUTO";
  runtime->expected_speed_mps = 0.0;
  PushState(runtime, runtime->session_state);
  runtime->stats.peer_disconnected_events += 1;
}

void OnNetworkDown(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->connected = false;
  runtime->session_state = "RECONNECTING";
  runtime->expected_speed_mps = 0.0;
  PushState(runtime, runtime->session_state);
  runtime->stats.network_down_events += 1;
}

void OnNetworkUp(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->connected = true;
  runtime->session_state = runtime->emergency ? "SAFE_STOP" : "CONNECTED";
  runtime->vehicle_mode = runtime->emergency ? "SAFE_STOP" : "AUTO";
  runtime->expected_speed_mps = 0.0;
  PushState(runtime, runtime->session_state);
  runtime->stats.network_up_events += 1;
}

void OnHeartbeatLost(RuntimeState* runtime) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->session_state = "SAFE_STOP";
  runtime->vehicle_mode = "SAFE_STOP";
  runtime->emergency = true;
  runtime->expected_speed_mps = 0.0;
  PushState(runtime, runtime->session_state);
  runtime->stats.heartbeat_lost_events += 1;
}

bool ApplyCommand(RuntimeState* runtime, const std::string& source,
                  const std::string& command_name) {
  std::lock_guard<std::mutex> lock(runtime->mutex);

  const bool allow_when_disconnected = (command_name == "EMERGENCY_STOP");
  if (!runtime->connected && !allow_when_disconnected) {
    return false;
  }

  if (command_name == "TAKEOVER_REQUEST" &&
      runtime->session_state == "CONNECTED") {
    runtime->session_state = "REMOTE_CONTROL";
    runtime->vehicle_mode = "REMOTE_CONTROL";
    PushState(runtime, runtime->session_state);
  } else if (command_name == "FORWARD" &&
             runtime->session_state == "REMOTE_CONTROL" &&
             !runtime->emergency) {
    runtime->expected_speed_mps = 6.0;
  } else if (command_name == "STOP" &&
             runtime->session_state == "REMOTE_CONTROL") {
    runtime->expected_speed_mps = 0.0;
  } else if (command_name == "EMERGENCY_STOP") {
    runtime->session_state = "SAFE_STOP";
    runtime->vehicle_mode = "SAFE_STOP";
    runtime->emergency = true;
    runtime->expected_speed_mps = 0.0;
    PushState(runtime, runtime->session_state);
  } else if (command_name == "RECOVER_AUTO" &&
             runtime->session_state == "SAFE_STOP" && runtime->connected) {
    runtime->session_state = "CONNECTED";
    runtime->vehicle_mode = "AUTO";
    runtime->emergency = false;
    runtime->expected_speed_mps = 0.0;
    PushState(runtime, runtime->session_state);
  } else {
    return false;
  }

  runtime->stats.commands_sent += 1;
  runtime->stats.command_acks += 1;
  if (source == "web") {
    runtime->stats.ws_messages_received += 1;
  } else if (source == "input") {
    runtime->stats.input_commands_received += 1;
  }

  return true;
}

}  // namespace cockpit_domain
