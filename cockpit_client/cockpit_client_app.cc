#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cockpit_client/domain/runtime_state.h"

namespace {

struct RuntimeConfig {
  std::string client_id = "cockpit_client_default";
  std::string target_vehicle_id = "vehicle_client_default";
  int heartbeat_interval_ms = 1000;
  int telemetry_interval_ms = 200;
  int connect_delay_ms = 300;
  std::string control_channel_label = "control";
  std::string telemetry_channel_label = "telemetry";
};

using RuntimeState = cockpit_domain::RuntimeState;
using RuntimeStats = cockpit_domain::RuntimeStats;

std::atomic<bool> g_stop_requested{false};

void signal_handler(int signal_number) {
  std::cerr << "cockpit_client_app received signal " << signal_number
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
  if (kv.count("target_vehicle_id")) {
    config->target_vehicle_id = kv["target_vehicle_id"];
  }
  if (kv.count("control_channel_label")) {
    config->control_channel_label = kv["control_channel_label"];
  }
  if (kv.count("telemetry_channel_label")) {
    config->telemetry_channel_label = kv["telemetry_channel_label"];
  }

  int parsed = 0;
  if (kv.count("heartbeat_interval_ms") &&
      to_int(kv["heartbeat_interval_ms"], &parsed)) {
    config->heartbeat_interval_ms = std::max(100, parsed);
  }
  if (kv.count("telemetry_interval_ms") &&
      to_int(kv["telemetry_interval_ms"], &parsed)) {
    config->telemetry_interval_ms = std::max(20, parsed);
  }
  if (kv.count("connect_delay_ms") && to_int(kv["connect_delay_ms"], &parsed)) {
    config->connect_delay_ms = std::max(10, parsed);
  }
  return true;
}

void publish_status_update(RuntimeState* runtime, const std::string& category,
                           const std::string& value) {
  std::lock_guard<std::mutex> lock(runtime->mutex);
  runtime->stats.status_updates += 1;
  std::cout << "[cockpit/status] category=" << category << " value=" << value
            << " session_state=" << runtime->session_state
            << " ws_clients=" << runtime->ws_clients.size() << std::endl;
}

void handle_ws_connected(RuntimeState* runtime, int conn_id) {
  cockpit_domain::OnWsConnected(runtime, conn_id);
  std::cout << "[cockpit/ws] connected conn_id=" << conn_id << std::endl;
  publish_status_update(runtime, "ws_connected", std::to_string(conn_id));
}

void handle_ws_disconnected(RuntimeState* runtime, int conn_id) {
  cockpit_domain::OnWsDisconnected(runtime, conn_id);
  std::cout << "[cockpit/ws] disconnected conn_id=" << conn_id << std::endl;
  publish_status_update(runtime, "ws_disconnected", std::to_string(conn_id));
}

void handle_peer_connected(RuntimeState* runtime, const RuntimeConfig& config) {
  cockpit_domain::OnPeerConnected(runtime);

  std::cout << "[cockpit/peer] connected peer_id=" << config.target_vehicle_id
            << std::endl;
  publish_status_update(runtime, "peer", "connected");
}

void handle_peer_disconnected(RuntimeState* runtime,
                              const std::string& reason) {
  cockpit_domain::OnPeerDisconnected(runtime);

  std::cout << "[cockpit/peer] disconnected reason=" << reason << std::endl;
  publish_status_update(runtime, "peer", "disconnected");
}

void handle_network_down(RuntimeState* runtime, const std::string& reason) {
  cockpit_domain::OnNetworkDown(runtime);

  std::cout << "[cockpit/network] down reason=" << reason << std::endl;
  publish_status_update(runtime, "network", "down");
}

void handle_network_up(RuntimeState* runtime, const RuntimeConfig& config) {
  cockpit_domain::OnNetworkUp(runtime);

  std::cout << "[cockpit/network] up target=" << config.target_vehicle_id
            << std::endl;
  publish_status_update(runtime, "network", "up");
}

void handle_heartbeat_lost(RuntimeState* runtime) {
  cockpit_domain::OnHeartbeatLost(runtime);

  std::cout << "[cockpit/network] heartbeat_lost" << std::endl;
  publish_status_update(runtime, "network", "heartbeat_lost");
}

bool process_command(RuntimeState* runtime, const std::string& source,
                     const std::string& command_name,
                     const std::string& command_id) {
  if (!cockpit_domain::ApplyCommand(runtime, source, command_name)) {
    return false;
  }

  std::string session_state;
  double expected_speed_mps = 0.0;
  {
    std::lock_guard<std::mutex> lock(runtime->mutex);
    session_state = runtime->session_state;
    expected_speed_mps = runtime->expected_speed_mps;
  }
  std::cout << "[cockpit/control] source=" << source << " cmd=" << command_name
            << " cmd_id=" << command_id << " state=" << session_state
            << " expected_speed_mps=" << expected_speed_mps << std::endl;
  return true;
}

std::string parse_web_command_name(const std::string& raw_message) {
  const auto key_pos = raw_message.find("cmd=");
  if (key_pos != std::string::npos) {
    std::string candidate = raw_message.substr(key_pos + 4);
    const auto sep = candidate.find_first_of(", }\"]");
    return trim(candidate.substr(0, sep));
  }

  const auto quote_key = raw_message.find("\"cmd\"");
  if (quote_key != std::string::npos) {
    const auto colon = raw_message.find(':', quote_key);
    if (colon != std::string::npos) {
      auto start = raw_message.find_first_not_of(" \t\r\n\"", colon + 1);
      if (start != std::string::npos) {
        auto end = raw_message.find_first_of("\" ,}\r\n\t", start);
        return trim(raw_message.substr(start, end - start));
      }
    }
  }

  return trim(raw_message);
}

bool handle_ws_message(RuntimeState* runtime, int conn_id,
                       const std::string& raw_message,
                       const std::string& command_id_prefix,
                       int* command_counter) {
  const std::string command_name = parse_web_command_name(raw_message);

  std::ostringstream command_id;
  command_id << command_id_prefix << "-" << (++(*command_counter));
  std::cout << "[cockpit/ws] message conn_id=" << conn_id << " raw=\""
            << raw_message << "\" parsed_cmd=" << command_name << std::endl;
  return process_command(runtime, "web", command_name, command_id.str());
}

bool handle_input_command(RuntimeState* runtime,
                          const std::string& command_name,
                          const std::string& command_id_prefix,
                          int* command_counter) {
  std::ostringstream command_id;
  command_id << command_id_prefix << "-" << (++(*command_counter));
  return process_command(runtime, "input", command_name, command_id.str());
}

void connection_loop(RuntimeState* runtime, const RuntimeConfig& config) {
  {
    std::lock_guard<std::mutex> lock(runtime->mutex);
    runtime->session_state = "CONNECTING";
    cockpit_domain::PushState(runtime, runtime->session_state);
  }
  std::cout << "[cockpit/session] connecting target="
            << config.target_vehicle_id << std::endl;

  std::this_thread::sleep_for(
      std::chrono::milliseconds(config.connect_delay_ms));
  if (g_stop_requested) {
    return;
  }

  handle_peer_connected(runtime, config);

  while (!g_stop_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void heartbeat_loop(RuntimeState* runtime, const RuntimeConfig& config) {
  const auto interval = std::chrono::milliseconds(config.heartbeat_interval_ms);
  while (!g_stop_requested) {
    {
      std::lock_guard<std::mutex> lock(runtime->mutex);
      runtime->stats.heartbeats += 1;
      std::cout << "[cockpit/heartbeat] client_id=" << config.client_id
                << " target=" << config.target_vehicle_id
                << " state=" << runtime->session_state << std::endl;
    }
    std::this_thread::sleep_for(interval);
  }
}

void telemetry_loop(RuntimeState* runtime, const RuntimeConfig& config) {
  const auto interval = std::chrono::milliseconds(config.telemetry_interval_ms);
  while (!g_stop_requested) {
    {
      std::lock_guard<std::mutex> lock(runtime->mutex);
      if (runtime->connected) {
        runtime->stats.telemetry_received += 1;
        runtime->stats.telemetry_broadcasts +=
            static_cast<int>(runtime->ws_clients.size());

        std::cout << "[cockpit/telemetry] vehicle_mode="
                  << runtime->vehicle_mode
                  << " speed_mps=" << runtime->expected_speed_mps
                  << " emergency=" << (runtime->emergency ? "true" : "false")
                  << " ws_clients=" << runtime->ws_clients.size() << std::endl;
      }
    }
    std::this_thread::sleep_for(interval);
  }
}

bool run_self_test(RuntimeState* runtime, const RuntimeConfig& config) {
  while (!g_stop_requested) {
    bool ready = false;
    {
      std::lock_guard<std::mutex> lock(runtime->mutex);
      ready = runtime->connected;
    }
    if (ready) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if (g_stop_requested) {
    return false;
  }

  int command_counter = 0;
  const int ws_conn = 101;

  handle_ws_connected(runtime, ws_conn);
  std::this_thread::sleep_for(std::chrono::milliseconds(120));

  if (!handle_ws_message(runtime, ws_conn, "cmd=TAKEOVER_REQUEST", "web",
                         &command_counter)) {
    std::cerr << "Self-test failed: web TAKEOVER_REQUEST" << std::endl;
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  if (!handle_input_command(runtime, "FORWARD", "input", &command_counter)) {
    std::cerr << "Self-test failed: input FORWARD" << std::endl;
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  if (!handle_ws_message(runtime, ws_conn, "{\"cmd\":\"STOP\"}", "web",
                         &command_counter)) {
    std::cerr << "Self-test failed: web STOP" << std::endl;
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  if (!handle_ws_message(runtime, ws_conn, "cmd=EMERGENCY_STOP", "web",
                         &command_counter)) {
    std::cerr << "Self-test failed: web EMERGENCY_STOP" << std::endl;
    return false;
  }

  handle_network_down(runtime, "link_lost");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  handle_network_up(runtime, config);

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  if (!handle_input_command(runtime, "RECOVER_AUTO", "input",
                            &command_counter)) {
    std::cerr << "Self-test failed: input RECOVER_AUTO" << std::endl;
    return false;
  }

  handle_heartbeat_lost(runtime);
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  if (!handle_ws_message(runtime, ws_conn, "cmd=RECOVER_AUTO", "web",
                         &command_counter)) {
    std::cerr << "Self-test failed: web RECOVER_AUTO after heartbeat_lost"
              << std::endl;
    return false;
  }

  handle_peer_disconnected(runtime, "peer_reset");
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  handle_peer_connected(runtime, config);

  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  handle_ws_disconnected(runtime, ws_conn);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  RuntimeStats stats;
  std::string final_state;
  std::string vehicle_mode;
  bool emergency = false;
  bool connected = false;
  size_t ws_clients = 0;
  {
    std::lock_guard<std::mutex> lock(runtime->mutex);
    stats = runtime->stats;
    final_state = runtime->session_state;
    vehicle_mode = runtime->vehicle_mode;
    emergency = runtime->emergency;
    connected = runtime->connected;
    ws_clients = runtime->ws_clients.size();
  }

  const std::vector<std::string> expected_states = {
      "DISCONNECTED", "CONNECTING",   "CONNECTED",    "REMOTE_CONTROL",
      "SAFE_STOP",    "RECONNECTING", "SAFE_STOP",    "CONNECTED",
      "SAFE_STOP",    "CONNECTED",    "DISCONNECTED", "CONNECTED"};

  size_t cursor = 0;
  for (const auto& state : stats.state_timeline) {
    if (cursor < expected_states.size() && state == expected_states[cursor]) {
      ++cursor;
    }
  }

  bool ok = true;
  ok = ok && (stats.heartbeats > 0);
  ok = ok && (stats.telemetry_received >= 3);
  ok = ok && (stats.telemetry_broadcasts > 0);
  ok = ok && (stats.commands_sent >= 6);
  ok = ok && (stats.command_acks >= 6);
  ok = ok && (stats.ws_connected_events >= 1);
  ok = ok && (stats.ws_disconnected_events >= 1);
  ok = ok && (stats.ws_messages_received >= 4);
  ok = ok && (stats.input_commands_received >= 2);
  ok = ok && (stats.peer_connected_events >= 2);
  ok = ok && (stats.peer_disconnected_events >= 1);
  ok = ok && (stats.network_up_events >= 1);
  ok = ok && (stats.network_down_events >= 1);
  ok = ok && (stats.heartbeat_lost_events >= 1);
  ok = ok && (stats.status_updates >= 5);
  ok = ok && (cursor == expected_states.size());
  ok = ok && connected;
  ok = ok && (final_state == "CONNECTED");
  ok = ok && (vehicle_mode == "AUTO");
  ok = ok && (!emergency);
  ok = ok && (ws_clients == 0);

  if (!ok) {
    std::cerr << "Cockpit self-test failed. heartbeats=" << stats.heartbeats
              << " telemetry_received=" << stats.telemetry_received
              << " telemetry_broadcasts=" << stats.telemetry_broadcasts
              << " commands_sent=" << stats.commands_sent
              << " command_acks=" << stats.command_acks
              << " ws_connected_events=" << stats.ws_connected_events
              << " ws_disconnected_events=" << stats.ws_disconnected_events
              << " ws_messages_received=" << stats.ws_messages_received
              << " input_commands_received=" << stats.input_commands_received
              << " peer_connected_events=" << stats.peer_connected_events
              << " peer_disconnected_events=" << stats.peer_disconnected_events
              << " network_up_events=" << stats.network_up_events
              << " network_down_events=" << stats.network_down_events
              << " heartbeat_lost_events=" << stats.heartbeat_lost_events
              << " status_updates=" << stats.status_updates
              << " connected=" << (connected ? "true" : "false")
              << " final_state=" << final_state
              << " vehicle_mode=" << vehicle_mode
              << " emergency=" << (emergency ? "true" : "false")
              << " ws_clients=" << ws_clients << std::endl;
    std::cerr << "state_timeline:";
    for (const auto& state : stats.state_timeline) {
      std::cerr << " " << state;
    }
    std::cerr << std::endl;
    return false;
  }

  std::cout << "COCKPIT_SELF_TEST_PASS heartbeats=" << stats.heartbeats
            << " telemetry_received=" << stats.telemetry_received
            << " command_acks=" << stats.command_acks << std::endl;
  std::cout << "COCKPIT_LEGACY_FEATURES_PASS ws_messages="
            << stats.ws_messages_received
            << " input_commands=" << stats.input_commands_received
            << " network_events="
            << (stats.network_up_events + stats.network_down_events +
                stats.heartbeat_lost_events)
            << " telemetry_broadcasts=" << stats.telemetry_broadcasts
            << std::endl;
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
    std::cout << "Usage: cockpit_client_app --config <path> [--self-test] "
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
    cockpit_domain::PushState(&runtime, runtime.session_state);
  }

  std::cout << "Cockpit client started. role=cockpit client_id="
            << config.client_id << " target=" << config.target_vehicle_id
            << " config=" << config_path << std::endl;

  std::thread session_thread(connection_loop, &runtime, std::cref(config));
  std::thread heartbeat_thread(heartbeat_loop, &runtime, std::cref(config));
  std::thread telemetry_thread(telemetry_loop, &runtime, std::cref(config));

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

  session_thread.join();
  heartbeat_thread.join();
  telemetry_thread.join();

  RuntimeStats stats;
  std::string final_state;
  std::string final_mode;
  bool emergency = false;
  size_t ws_clients = 0;
  {
    std::lock_guard<std::mutex> lock(runtime.mutex);
    stats = runtime.stats;
    final_state = runtime.session_state;
    final_mode = runtime.vehicle_mode;
    emergency = runtime.emergency;
    ws_clients = runtime.ws_clients.size();
  }

  std::cout << "Cockpit client stopped. state=" << final_state
            << " vehicle_mode=" << final_mode
            << " emergency=" << (emergency ? "true" : "false")
            << " ws_clients=" << ws_clients
            << " heartbeats=" << stats.heartbeats
            << " telemetry_received=" << stats.telemetry_received
            << " telemetry_broadcasts=" << stats.telemetry_broadcasts
            << " command_acks=" << stats.command_acks << std::endl;

  return success ? 0 : 5;
}
