#ifndef VEHICLE_CLIENT_DOMAIN_RUNTIME_STATE_H
#define VEHICLE_CLIENT_DOMAIN_RUNTIME_STATE_H

#include <mutex>
#include <string>
#include <vector>

namespace vehicle_domain {

struct RuntimeStats {
  int camera_frames = 0;
  int telemetry_messages = 0;
  int telemetry_sent = 0;
  int telemetry_messages_received = 0;
  int heartbeat_messages = 0;
  int command_acks = 0;
  int control_messages_received = 0;
  int emergency_commands_received = 0;
  int webrtc_errors = 0;
  int peer_connected_events = 0;
  int peer_disconnected_events = 0;
  int network_up_events = 0;
  int network_down_events = 0;
  int heartbeat_lost_events = 0;
  int video_frames_sent = 0;
  int status_updates = 0;
  std::vector<std::string> mode_timeline;
};

struct RuntimeState {
  std::mutex mutex;
  std::string mode = "AUTO";
  bool emergency = false;
  double speed_mps = 0.0;
  int gear = 0;
  bool peer_connected = false;
  RuntimeStats stats;
};

void PushMode(RuntimeState* runtime, const std::string& mode);
void OnPeerConnected(RuntimeState* runtime);
void OnPeerDisconnected(RuntimeState* runtime);
void OnNetworkDown(RuntimeState* runtime);
void OnNetworkUp(RuntimeState* runtime);
void OnHeartbeatLost(RuntimeState* runtime);
void OnTelemetryMessageReceived(RuntimeState* runtime);
void OnWebrtcError(RuntimeState* runtime);
bool ApplyCommand(RuntimeState* runtime, const std::string& source,
                  const std::string& command_name);

}  // namespace vehicle_domain

#endif
