#ifndef COCKPIT_CONFIG_H
#define COCKPIT_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

// Reuse WebRtcServerConfig and IceServer from VehicleConfig if identical
// struct WebRtcServerConfig { ... };
// struct IceServer { ... };

struct CockpitConfig {
  WebRtcServerConfig signaling;   // Signaling server URI, JWT
  std::string client_id;          // Unique ID for this cockpit client
  std::string target_vehicle_id;  // ID of the vehicle to connect to

  // Local transport server config
  std::string transport_server_address = "127.0.0.1";
  uint16_t transport_server_port = 8899;
  std::string display_files_path =
      "display/public";  // Path to serve static web files

  // DataChannel labels (should match vehicle client)
  std::string control_channel_label = "control";
  std::string telemetry_channel_label = "telemetry";

  std::vector<IceServer> ice_servers;  // WebRTC ICE server config

  // Add other configurations as needed (e.g., input device mapping)
  int heartbeat_interval_ms = 5000;  // milliseconds
};

#endif  // COCKPIT_CONFIG_H
