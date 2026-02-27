#include "signaling/signaling_message.h"

#include <iostream>
#include <string>

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

  ok &=
      Check(SignalMessage::TypeToString(SignalMessage::Type::OFFER) == "offer",
            "TypeToString(OFFER)");
  ok &= Check(
      SignalMessage::StringToType("answer") == SignalMessage::Type::ANSWER,
      "StringToType(answer)");

  SignalMessage message;
  message.type = SignalMessage::Type::CANDIDATE;
  message.from = "peer_a";
  message.to = "peer_b";
  message.candidate = "candidate:1 1 udp 123 127.0.0.1 5000 typ host";
  message.sdpMid = "0";
  message.sdpMlineIndex = 0;
  message.reason = "none";

  std::string serialized = SerializeSignalMessage(message);
  auto parsed = DeserializeSignalMessage(serialized);
  ok &= Check(parsed.has_value(), "Deserialize serialized payload");

  if (parsed) {
    ok &= Check(parsed->type == SignalMessage::Type::CANDIDATE,
                "parsed type candidate");
    ok &= Check(parsed->from == "peer_a", "parsed from");
    ok &= Check(parsed->to == "peer_b", "parsed to");
    ok &= Check(parsed->candidate.has_value(), "parsed candidate present");
    ok &= Check(parsed->sdpMid.has_value(), "parsed sdpMid present");
    ok &= Check(parsed->sdpMlineIndex.has_value(),
                "parsed sdpMlineIndex present");
  }

  {
    const std::string direct_flat =
        "{\"type\":\"candidate\",\"from\":\"v\",\"to\":\"c\","
        "\"candidate\":\"candidate:abc\",\"sdpMid\":\"0\","
        "\"sdpMLineIndex\":2}";
    auto parsed_flat = DeserializeSignalMessage(direct_flat);
    ok &= Check(parsed_flat.has_value(),
                "Deserialize direct flat candidate payload");
    if (parsed_flat) {
      ok &= Check(parsed_flat->candidate.has_value(),
                  "flat candidate present");
      ok &= Check(parsed_flat->sdpMlineIndex.has_value(),
                  "flat sdpMLineIndex compatibility");
      ok &= Check(*parsed_flat->sdpMlineIndex == 2,
                  "flat sdpMLineIndex value");
    }
  }

  {
    const std::string direct_nested =
        "{\"type\":\"candidate\",\"from\":\"v\",\"to\":\"c\","
        "\"candidate\":{\"candidate\":\"candidate:def\",\"sdpMid\":\"1\","
        "\"sdpMLineIndex\":3}}";
    auto parsed_nested = DeserializeSignalMessage(direct_nested);
    ok &= Check(parsed_nested.has_value(),
                "Deserialize direct nested candidate payload");
    if (parsed_nested) {
      ok &= Check(parsed_nested->candidate.has_value(),
                  "nested candidate present");
      ok &= Check(parsed_nested->sdpMlineIndex.has_value(),
                  "nested sdpMLineIndex compatibility");
      ok &= Check(*parsed_nested->sdpMlineIndex == 3,
                  "nested sdpMLineIndex value");
    }
  }

  if (!ok) {
    return 1;
  }

  std::cout << "signaling_message_test passed" << std::endl;
  return 0;
}
