#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "signaling/signaling_message.h"

namespace {

struct ContractCase {
  std::string name;
  std::string shape;
  std::string from;
  std::string to;
  std::string candidate;
  std::string sdp_mid;
  int sdp_mline_index = 0;
};

std::string Trim(const std::string& value) {
  const char* ws = " \t\r\n";
  const auto begin = value.find_first_not_of(ws);
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(ws);
  return value.substr(begin, end - begin + 1);
}

bool Check(bool cond, const std::string& msg) {
  if (!cond) {
    std::cerr << "FAILED: " << msg << std::endl;
    return false;
  }
  return true;
}

bool LoadCases(const std::string& path, std::vector<ContractCase>* cases) {
  std::ifstream input(path);
  if (!input.is_open()) {
    std::cerr << "Failed to open contract cases: " << path << std::endl;
    return false;
  }

  std::unordered_map<std::string, std::string> kv;
  auto flush_case = [&]() {
    if (kv.empty()) {
      return;
    }
    ContractCase item;
    item.name = kv["name"];
    item.shape = kv["shape"];
    item.from = kv["from"];
    item.to = kv["to"];
    item.candidate = kv["candidate"];
    item.sdp_mid = kv["sdp_mid"];
    try {
      item.sdp_mline_index = std::stoi(kv["sdp_mline_index"]);
    } catch (...) {
      item.sdp_mline_index = 0;
    }
    cases->push_back(item);
    kv.clear();
  };

  std::string line;
  while (std::getline(input, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (line == "[case]") {
      flush_case();
      continue;
    }
    const auto pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }
    kv[Trim(line.substr(0, pos))] = Trim(line.substr(pos + 1));
  }
  flush_case();
  return !cases->empty();
}

bool FileContainsLine(const std::string& file_contents,
                      const std::string& line) {
  return file_contents.find(line) != std::string::npos;
}

}  // namespace

int main() {
  bool ok = true;

  std::vector<ContractCase> cases;
  ok &= Check(
      LoadCases("contracts/signaling_envelope_contract_cases.txt", &cases),
      "load signaling envelope contract cases");

  for (const auto& item : cases) {
    SignalMessage msg;
    msg.type = SignalMessage::Type::CANDIDATE;
    msg.from = item.from;
    msg.to = item.to;
    msg.candidate = item.candidate;
    msg.sdpMid = item.sdp_mid;
    msg.sdpMlineIndex = item.sdp_mline_index;

    const std::string serialized = SerializeSignalMessage(msg);
    auto parsed = DeserializeSignalMessage(serialized);
    ok &= Check(parsed.has_value(), "deserialize candidate: " + item.name);
    if (!parsed) {
      continue;
    }

    ok &= Check(parsed->type == SignalMessage::Type::CANDIDATE,
                "type candidate: " + item.name);
    ok &= Check(parsed->from == item.from, "from matches: " + item.name);
    ok &= Check(parsed->to == item.to, "to matches: " + item.name);
    ok &= Check(
        parsed->candidate.has_value() && *parsed->candidate == item.candidate,
        "candidate matches: " + item.name);
    ok &= Check(parsed->sdpMid.has_value() && *parsed->sdpMid == item.sdp_mid,
                "sdpMid matches: " + item.name);
    ok &= Check(parsed->sdpMlineIndex.has_value() &&
                    *parsed->sdpMlineIndex == item.sdp_mline_index,
                "sdpMlineIndex matches: " + item.name);
    ok &= Check(serialized.find("\"sdpMlineIndex\"") != std::string::npos,
                "serialized includes sdpMlineIndex: " + item.name);
    ok &= Check(serialized.find("\"sdpMLineIndex\"") != std::string::npos,
                "serialized includes sdpMLineIndex compat: " + item.name);
  }

  std::ifstream proto_input("proto/remote_command.proto");
  ok &= Check(proto_input.is_open(), "open proto/remote_command.proto");
  std::stringstream proto_buffer;
  proto_buffer << proto_input.rdbuf();
  const std::string proto_contents = proto_buffer.str();

  std::ifstream rules_input("contracts/proto_contract_rules.txt");
  ok &= Check(rules_input.is_open(), "open proto contract rules");

  std::string rule;
  while (std::getline(rules_input, rule)) {
    rule = Trim(rule);
    if (rule.empty() || rule[0] == '#') {
      continue;
    }
    ok &= Check(FileContainsLine(proto_contents, rule),
                "proto contains: " + rule);
  }

  if (!ok) {
    return 1;
  }

  std::cout << "signaling_proto_contract_test passed" << std::endl;
  return 0;
}
