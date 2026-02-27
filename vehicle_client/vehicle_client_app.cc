#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "vehicle_client/domain/runtime_state.h"

namespace {

struct RuntimeConfig {
  std::string client_id = "vehicle_client_default";
  int camera_fps = 15;
  int telemetry_interval_ms = 200;
  int heartbeat_interval_ms = 1000;
  std::string control_channel_label = "control";
  std::string telemetry_channel_label = "telemetry";
};

using RuntimeState = vehicle_domain::RuntimeState;
using RuntimeStats = vehicle_domain::RuntimeStats;

std::atomic<bool> g_stop_requested{false};

void signal_handler(int signal_number) {
  std::cerr << "vehicle_client_app received signal " << signal_number
            << ", stopping..." << std::endl;
  g_stop_requested = true;
}

std::string trim(const std::string& value) {
  const char* ws = " \t\r\n";
  const auto begin = value.find_first_not_of(ws);
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(ws);
  return value.substr(begin, end - begin + 1);
}

bool to_int(const std::string& value, int* out) {
  try {
    *out = std::stoi(value);
    return true;
  } catch (...) {
    return false;
  }
}

bool load_config(const std::string& config_path, RuntimeConfig* config) {
  std::ifstream input(config_path);
  if (!input.is_open()) {
    std::cerr << "Failed to open config: " << config_path << std::endl;
    return false;
  }

  std::unordered_map<std::string, std::string> kv;
  std::string line;
  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const auto pos = line.find('=');
    if (pos == std::string::npos) {
      std::cerr << "Invalid config line (expected key=value): " << line
                << std::endl;
      return false;
    }
    std::string key = trim(line.substr(0, pos));
    std::string value = trim(line.substr(pos + 1));
    if (!key.empty()) {
      kv[key] = value;
    }
  }

  if (kv.count("client_id")) {
    config->client_id = kv["client_id"];
  }
  if (kv.count("control_channel_label")) {
    config->control_channel_label = kv["control_channel_label"];
  }
  if (kv.count("telemetry_channel_label")) {
    config->telemetry_channel_label = kv["telemetry_channel_label"];
  }

  int parsed = 0;
  if (kv.count("camera_fps") && to_int(kv["camera_fps"], &parsed)) {
    config->camera_fps = std::max(1, parsed);
  }
  if (kv.count("telemetry_interval_ms") &&
      to_int(kv["telemetry_interval_ms"], &parsed)) {
    config->telemetry_interval_ms = std::max(20, parsed);
  }
  if (kv.count("heartbeat_interval_ms") &&
      to_int(kv["heartbeat_interval_ms"], &parsed)) {
    config->heartbeat_interval_ms = std::max(0, parsed);
  }
  return true;
}

void publish_status_update(RuntimeState* runtime, const std::string& category,
                           const std::string& value) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->stats.status_updates += 1;
  std::cout << "[vehicle/status] category=" << category << " value=" << value
            << " mode=" << runtime->mode << " peer_connected="
            << (runtime->peer_connected ? "true" : "false") << std::endl;
}

void handle_peer_connected(RuntimeState* runtime, const RuntimeConfig& config) {
  vehicle_domain::OnPeerConnected(runtime);
  std::cout << "[vehicle/peer] connected peer_id=" << config.client_id
            << " remote=cockpit" << std::endl;
  publish_status_update(runtime, "peer", "connected");
}

void handle_peer_disconnected(RuntimeState* runtime,
                              const std::string& reason) {
  vehicle_domain::OnPeerDisconnected(runtime);
  std::cout << "[vehicle/peer] disconnected reason=" << reason << std::endl;
  publish_status_update(runtime, "peer", "disconnected");
}

void handle_network_down(RuntimeState* runtime, const std::string& reason) {
  vehicle_domain::OnNetworkDown(runtime);
  std::cout << "[vehicle/network] down reason=" << reason << std::endl;
  publish_status_update(runtime, "network", "down");
}

void handle_network_up(RuntimeState* runtime) {
  vehicle_domain::OnNetworkUp(runtime);
  std::cout << "[vehicle/network] up" << std::endl;
  publish_status_update(runtime, "network", "up");
}

void handle_heartbeat_lost(RuntimeState* runtime) {
  vehicle_domain::OnHeartbeatLost(runtime);
  std::cout << "[vehicle/network] heartbeat_lost" << std::endl;
  publish_status_update(runtime, "network", "heartbeat_lost");
}

void handle_telemetry_message_received(RuntimeState* runtime,
                                       const std::string& peer_id,
                                       const std::string& payload) {
  vehicle_domain::OnTelemetryMessageReceived(runtime);
  std::cout << "[vehicle/telemetry_rx] peer=" << peer_id
            << " payload_size=" << payload.size() << std::endl;
  publish_status_update(runtime, "telemetry_rx", "received");
}

void handle_webrtc_error(RuntimeState* runtime, const std::string& error_msg) {
  vehicle_domain::OnWebrtcError(runtime);
  std::cout << "[vehicle/webrtc] error=" << error_msg << std::endl;
  publish_status_update(runtime, "webrtc", "error");
}

bool process_command(RuntimeState* runtime, const std::string& source,
                     const std::string& command_name,
                     const std::string& command_id) {
  if (!vehicle_domain::ApplyCommand(runtime, source, command_name)) {
    return false;
  }

  std::string mode;
  double speed_mps = 0.0;
  {
    std::lock_guard<std::mutex> lock(runtime->mutex);
    mode = runtime->mode;
    speed_mps = runtime->speed_mps;
  }
  std::cout << "[vehicle/control] source=" << source
            << " ack cmd=" << command_name << " cmd_id=" << command_id
            << " mode=" << mode << " speed=" << speed_mps
            << std::endl;
  return true;
}

void camera_loop(RuntimeState* runtime, const RuntimeConfig& config) {
  const auto interval = std::chrono::milliseconds(1000 / config.camera_fps);
  while (!g_stop_requested) {
    {
      std::lock_guard<std::mutex> lock(runtime->mutex);
      runtime->stats.camera_frames += 1;
      if (runtime->peer_connected) {
        runtime->stats.video_frames_sent += 1;
      }
    }
    std::this_thread::sleep_for(interval);
  }
}

void telemetry_loop(RuntimeState* runtime, const RuntimeConfig& config) {
  const auto interval = std::chrono::milliseconds(config.telemetry_interval_ms);
  while (!g_stop_requested) {
    {
      std::lock_guard<std::mutex> lock(runtime->mutex);
      runtime->stats.telemetry_messages += 1;
      if (runtime->peer_connected) {
        runtime->stats.telemetry_sent += 1;
      }
      std::cout << "[vehicle/telemetry] mode=" << runtime->mode
                << " speed_mps=" << runtime->speed_mps
                << " gear=" << runtime->gear
                << " emergency=" << (runtime->emergency ? "true" : "false")
                << " sent=" << runtime->stats.telemetry_sent << std::endl;
    }
    std::this_thread::sleep_for(interval);
  }
}

void heartbeat_loop(RuntimeState* runtime, const RuntimeConfig& config) {
  const auto interval = std::chrono::milliseconds(config.heartbeat_interval_ms);
  while (!g_stop_requested) {
    {
      std::lock_guard<std::mutex> lock(runtime->mutex);
      runtime->stats.heartbeat_messages += 1;
      std::cout << "[vehicle/heartbeat] client_id=" << config.client_id
                << " status=alive" << std::endl;
    }
    std::this_thread::sleep_for(interval);
  }
}

bool run_self_test(RuntimeState* runtime, const RuntimeConfig& config) {
  handle_peer_connected(runtime, config);
  handle_telemetry_message_received(runtime, config.client_id,
                                    "diag:loopback_payload");

  int command_index = 0;
  auto apply_cmd = [&](const std::string& source, const std::string& cmd,
                       int delay_ms) -> bool {
    if (g_stop_requested) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    std::ostringstream cmd_id;
    cmd_id << source << "-" << (++command_index);
    return process_command(runtime, source, cmd, cmd_id.str());
  };

  if (!apply_cmd("control_channel", "TAKEOVER_REQUEST", 180)) {
    std::cerr << "Self-test command failed: TAKEOVER_REQUEST" << std::endl;
    return false;
  }
  if (!apply_cmd("control_channel", "FORWARD", 160)) {
    std::cerr << "Self-test command failed: FORWARD" << std::endl;
    return false;
  }
  if (!apply_cmd("control_channel", "STOP", 140)) {
    std::cerr << "Self-test command failed: STOP" << std::endl;
    return false;
  }
  if (!apply_cmd("safety_channel", "EMERGENCY_STOP", 120)) {
    std::cerr << "Self-test command failed: EMERGENCY_STOP" << std::endl;
    return false;
  }

  handle_network_down(runtime, "network_drop");
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  handle_network_up(runtime);

  if (!apply_cmd("control_channel", "RECOVER_AUTO", 140)) {
    std::cerr << "Self-test command failed: RECOVER_AUTO" << std::endl;
    return false;
  }

  handle_heartbeat_lost(runtime);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if (!apply_cmd("control_channel", "RECOVER_AUTO", 120)) {
    std::cerr << "Self-test command failed: RECOVER_AUTO after heartbeat_lost"
              << std::endl;
    return false;
  }

  handle_webrtc_error(runtime, "simulated_media_path_failure");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if (!apply_cmd("control_channel", "RECOVER_AUTO", 120)) {
    std::cerr << "Self-test command failed: RECOVER_AUTO after webrtc_error"
              << std::endl;
    return false;
  }

  handle_peer_disconnected(runtime, "peer_reset");
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  handle_peer_connected(runtime, config);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  RuntimeStats stats;
  std::string final_mode;
  bool emergency = false;
  bool peer_connected = false;
  {
    std::lock_guard<std::mutex> lock(runtime->mutex);
    stats = runtime->stats;
    final_mode = runtime->mode;
    emergency = runtime->emergency;
    peer_connected = runtime->peer_connected;
  }

  const std::vector<std::string> expected_modes = {
      "AUTO", "REMOTE_CONTROL", "SAFE_STOP", "AUTO", "SAFE_STOP", "AUTO"};
  size_t cursor = 0;
  for (const auto& mode : stats.mode_timeline) {
    if (cursor < expected_modes.size() && mode == expected_modes[cursor]) {
      ++cursor;
    }
  }

  bool ok = true;
  ok = ok && (stats.camera_frames > 0);
  ok = ok && (stats.video_frames_sent > 0);
  ok = ok && (stats.telemetry_messages >= 3);
  ok = ok && (stats.telemetry_sent > 0);
  ok = ok && (stats.telemetry_messages_received >= 1);
  ok = ok &&
       ((config.heartbeat_interval_ms > 0) ? (stats.heartbeat_messages > 0)
                                           : (stats.heartbeat_messages == 0));
  ok = ok && (stats.command_acks >= 7);
  ok = ok && (stats.control_messages_received >= 5);
  ok = ok && (stats.emergency_commands_received >= 3);
  ok = ok && (stats.webrtc_errors >= 1);
  ok = ok && (stats.peer_connected_events >= 2);
  ok = ok && (stats.peer_disconnected_events >= 1);
  ok = ok && (stats.network_up_events >= 1);
  ok = ok && (stats.network_down_events >= 1);
  ok = ok && (stats.heartbeat_lost_events >= 1);
  ok = ok && (stats.status_updates >= 5);
  ok = ok && (cursor == expected_modes.size());
  ok = ok && (final_mode == "AUTO");
  ok = ok && (!emergency);
  ok = ok && peer_connected;

  if (!ok) {
    std::cerr
        << "Vehicle self-test failed. camera_frames=" << stats.camera_frames
        << " video_frames_sent=" << stats.video_frames_sent
        << " telemetry_messages=" << stats.telemetry_messages
        << " telemetry_sent=" << stats.telemetry_sent
        << " telemetry_messages_received=" << stats.telemetry_messages_received
        << " heartbeat_messages=" << stats.heartbeat_messages
        << " command_acks=" << stats.command_acks
        << " control_messages_received=" << stats.control_messages_received
        << " emergency_commands_received=" << stats.emergency_commands_received
        << " webrtc_errors=" << stats.webrtc_errors
        << " peer_connected_events=" << stats.peer_connected_events
        << " peer_disconnected_events=" << stats.peer_disconnected_events
        << " network_up_events=" << stats.network_up_events
        << " network_down_events=" << stats.network_down_events
        << " heartbeat_lost_events=" << stats.heartbeat_lost_events
        << " status_updates=" << stats.status_updates
        << " final_mode=" << final_mode
        << " emergency=" << (emergency ? "true" : "false")
        << " peer_connected=" << (peer_connected ? "true" : "false")
        << std::endl;
    std::cerr << "mode_timeline:";
    for (const auto& mode : stats.mode_timeline) {
      std::cerr << " " << mode;
    }
    std::cerr << std::endl;
    return false;
  }

  std::cout << "VEHICLE_SELF_TEST_PASS camera_frames=" << stats.camera_frames
            << " telemetry_messages=" << stats.telemetry_messages
            << " command_acks=" << stats.command_acks << std::endl;
  std::cout << "VEHICLE_LEGACY_FEATURES_PASS control_messages="
            << stats.control_messages_received
            << " emergency_commands=" << stats.emergency_commands_received
            << " webrtc_errors=" << stats.webrtc_errors
            << " telemetry_received=" << stats.telemetry_messages_received
            << " network_events="
            << (stats.network_up_events + stats.network_down_events +
                stats.heartbeat_lost_events)
            << " telemetry_sent=" << stats.telemetry_sent
            << " video_frames_sent=" << stats.video_frames_sent << std::endl;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::string config_path;
  bool show_help = false;
  bool self_test = false;
  int run_seconds = 0;

  for (int index = 1; index < argc; ++index) {
    std::string arg = argv[index];
    if (arg == "--help" || arg == "-h") {
      show_help = true;
      break;
    }
    if (arg == "--self-test") {
      self_test = true;
      continue;
    }
    if (arg == "--run-seconds" && index + 1 < argc) {
      int parsed = 0;
      if (!to_int(argv[++index], &parsed)) {
        std::cerr << "Invalid value for --run-seconds" << std::endl;
        return 2;
      }
      run_seconds = std::max(0, parsed);
      continue;
    }
    if (arg == "--config" && index + 1 < argc) {
      config_path = argv[++index];
      continue;
    }
    if (!arg.empty() && arg[0] != '-') {
      config_path = arg;
      continue;
    }
    std::cerr << "Unknown argument: " << arg << std::endl;
    return 2;
  }

  if (show_help) {
    std::cout << "Usage: vehicle_client_app --config <path> [--self-test] "
                 "[--run-seconds N]"
              << std::endl;
    return 0;
  }

  if (config_path.empty()) {
    std::cerr << "Missing required config path." << std::endl;
    return 2;
  }

  if (!std::filesystem::exists(config_path)) {
    std::cerr << "Config file not found: " << config_path << std::endl;
    return 3;
  }

  RuntimeConfig config;
  if (!load_config(config_path, &config)) {
    return 4;
  }

  g_stop_requested = false;
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  RuntimeState runtime;
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    vehicle_domain::PushMode(&runtime, runtime.mode);
  }

  std::cout << "Vehicle client started. role=vehicle client_id="
            << config.client_id << " config=" << config_path << std::endl;

  std::thread camera_thread(camera_loop, &runtime, std::cref(config));
  std::thread telemetry_thread(telemetry_loop, &runtime, std::cref(config));
  std::unique_ptr<std::thread> heartbeat_thread;
  if (config.heartbeat_interval_ms > 0) {
    heartbeat_thread = std::make_unique<std::thread>(heartbeat_loop, &runtime,
                                                     std::cref(config));
  } else {
    std::cout << "[vehicle/heartbeat] disabled interval_ms=0" << std::endl;
  }

  bool success = true;
  if (self_test) {
    success = run_self_test(&runtime, config);
    g_stop_requested = true;
  } else if (run_seconds > 0) {
    std::this_thread::sleep_for(std::chrono::seconds(run_seconds));
    g_stop_requested = true;
  } else {
    while (!g_stop_requested) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  }

  if (camera_thread.joinable()) {
    camera_thread.join();
  }
  if (telemetry_thread.joinable()) {
    telemetry_thread.join();
  }
  if (heartbeat_thread && heartbeat_thread->joinable()) {
    heartbeat_thread->join();
  }

  RuntimeStats final_stats;
  std::string final_mode;
  bool final_emergency = false;
  bool final_peer_connected = false;
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    final_stats = runtime.stats;
    final_mode = runtime.mode;
    final_emergency = runtime.emergency;
    final_peer_connected = runtime.peer_connected;
  }

  std::cout << "Vehicle client stopped. mode=" << final_mode
            << " emergency=" << (final_emergency ? "true" : "false")
            << " peer_connected=" << (final_peer_connected ? "true" : "false")
            << " camera_frames=" << final_stats.camera_frames
            << " video_frames_sent=" << final_stats.video_frames_sent
            << " telemetry_messages=" << final_stats.telemetry_messages
            << " telemetry_sent=" << final_stats.telemetry_sent
            << " command_acks=" << final_stats.command_acks << std::endl;

  return success ? 0 : 5;
}
