#include "signaling/signaling_message.h"
#include "webrtc/i_peer_connection.h"
#include "webrtc/i_webrtc_manager.h"
#include "webrtc/peer_connection_callbacks.h"
#include "webrtc/webrtc_manager.h"

int main() {
  SignalMessage message;
  message.type = SignalMessage::Type::UNKNOWN;

  autodev::remote::webrtc::PeerConnectionCallbacks callbacks;
  (void)callbacks;
  return 0;
}
