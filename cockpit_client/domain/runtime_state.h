#ifndef COCKPIT_CLIENT_DOMAIN_RUNTIME_STATE_H
#define COCKPIT_CLIENT_DOMAIN_RUNTIME_STATE_H

#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace cockpit_domain {

struct RuntimeStats {
  int heartbeats = 0;
  int commands_sent = 0;
  int command_acks = 0;
  int telemetry_received = 0;
  int telemetry_broadcasts = 0;
  int ws_connected_events = 0;
  int ws_disconnected_events = 0;
  int ws_messages_received = 0;
  int input_commands_received = 0;
  int peer_connected_events = 0;
  int peer_disconnected_events = 0;
  int network_up_events = 0;
  int network_down_events = 0;
  int heartbeat_lost_events = 0;
  int status_updates = 0;
  std::vector<std::string> state_timeline;
};

struct RuntimeState {
  std::mutex mutex;
  std::string session_state = "DISCONNECTED";
  std::string vehicle_mode = "AUTO";
  bool emergency = false;
  double expected_speed_mps = 0.0;
  bool connected = false;
  std::set<int> ws_clients;
  RuntimeStats stats;
};

void PushState(RuntimeState* runtime, const std::string& state);
void OnWsConnected(RuntimeState* runtime, int conn_id);
void OnWsDisconnected(RuntimeState* runtime, int conn_id);
void OnPeerConnected(RuntimeState* runtime);
void OnPeerDisconnected(RuntimeState* runtime);
void OnNetworkDown(RuntimeState* runtime);
void OnNetworkUp(RuntimeState* runtime);
void OnHeartbeatLost(RuntimeState* runtime);
bool ApplyCommand(RuntimeState* runtime, const std::string& source,
                  const std::string& command_name);

}  // namespace cockpit_domain

#endif
