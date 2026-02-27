#ifndef PEER_CONNECTION_CALLBACKS_H
#define PEER_CONNECTION_CALLBACKS_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Include definitions for state enums and DataChannelMessage
#include "webrtc/i_peer_connection.h"

namespace rtc {
template <typename T>
class scoped_refptr;
}

namespace webrtc {
class VideoTrackInterface;
}

namespace autodev {
namespace remote {
namespace webrtc {

// Define ICE gathering state enum (example, map from libwebrtc enum in impl)
enum class IceGatheringState { New, Gathering, Complete };

// Struct to bundle all PeerConnection event handlers (callbacks)
// These callbacks are typically called by the underlying PeerConnection
// implementation (e.g., LibwebrtcPeerConnectionImpl) and will likely be invoked
// on background threads. Implementations of these callbacks (e.g., in
// WebrtcManagerImpl) MUST BE THREAD-SAFE, or ensure any logic affecting shared
// state is properly synchronized or marshalled.
struct PeerConnectionCallbacks {
  // Called when a local SDP is successfully generated (either an offer or an
  // answer). This callback is typically invoked on the WebRTC signaling thread.
  // sdp_type: "offer" or "answer".
  // sdp_string: The generated SDP description string.
  std::function<void(const std::string& sdp_type,
                     const std::string& sdp_string)>
      onLocalSdpGenerated;

  // Called when a local ICE candidate is found.
  // This callback is typically invoked on the WebRTC signaling thread.
  // candidate: The ICE candidate string (e.g., "candidate:udp 123 1 ...").
  // sdp_mid: The media stream identification tag.
  // sdp_mline_index: The index of the m-line in the SDP.
  std::function<void(const std::string& candidate, const std::string& sdp_mid,
                     int sdp_mline_index)>
      onLocalCandidateGenerated;

  // Called when the overall PeerConnection state changes (Connecting,
  // Connected, Disconnected, etc.). This callback is typically invoked on the
  // WebRTC signaling thread. state: The new PeerConnection state.
  std::function<void(PeerConnectionState state)> onConnectionStateChange;

  // Called when the ICE connection state changes (Checking, Connected, Failed,
  // etc.). This callback is typically invoked on the WebRTC signaling thread.
  // state: The new ICE connection state.
  std::function<void(IceConnectionState state)> onIceConnectionStateChange;

  // Called when the Signaling state changes (Stable, HaveLocalOffer, etc.).
  // This callback is typically invoked on the WebRTC signaling thread.
  // state: The new Signaling state.
  std::function<void(SignalingState state)> onSignalingStateChange;

  // Called when the ICE gathering state changes (New, Gathering, Complete).
  // This callback is typically invoked on the WebRTC signaling thread.
  // state: The new ICE gathering state.
  std::function<void(IceGatheringState state)>
      onIceGatheringStateChange;  // Added

  // Called when a new DataChannel is successfully opened (can be initiated
  // locally or by remote peer). This callback is typically invoked on the
  // WebRTC signaling thread or DataChannel thread. label: The label of the
  // opened DataChannel.
  std::function<void(const std::string& label)> onDataChannelOpened;

  // Called when a DataChannel is closed.
  // This callback is typically invoked on the WebRTC signaling thread or
  // DataChannel thread. label: The label of the closed DataChannel.
  std::function<void(const std::string& label)> onDataChannelClosed;

  // Called when a message is received on a DataChannel.
  // This callback is typically invoked on the WebRTC signaling thread or
  // DataChannel thread. label: The label of the DataChannel the message arrived
  // on. message: The received data (binary bytes).
  std::function<void(const std::string& label,
                     const DataChannelMessage& message)>
      onDataChannelMessage;

  // Called when a remote media stream and/or track is added to the connection
  // (typically on Cockpit side). This callback is typically invoked on the
  // WebRTC signaling thread or media thread. track: The received video track.
  // Nullptr if only a stream is added. NOTE: Handling media tracks correctly
  // requires careful lifetime management and potentially adding a VideoSink to
  // the track on the application thread. This callback might need to be
  // marshalled.
  std::function<void(rtc::scoped_refptr<::webrtc::VideoTrackInterface> track)>
      onAddVideoTrack;  // Added (Simplified for video)

  // Called when the PeerConnection needs renegotiation (e.g., due to
  // adding/removing tracks). This callback is typically invoked on the WebRTC
  // signaling thread.
  std::function<void()> onRenegotiationNeeded;  // Added

  // Called when a significant error occurs specific to this PeerConnection or
  // its components. This callback is typically invoked on the WebRTC signaling
  // thread. error_msg: Description of the error.
  std::function<void(const std::string& error_msg)> onError;

  // Default constructor to initialize all callbacks to empty functions (no-op)
  // Use {} to ensure std::function is default-constructed (empty).
  PeerConnectionCallbacks()
      : onLocalSdpGenerated({}),
        onLocalCandidateGenerated({}),
        onConnectionStateChange({}),
        onIceConnectionStateChange({}),
        onSignalingStateChange({}),
        onIceGatheringStateChange({}),  // Initialize new member
        onDataChannelOpened({}),
        onDataChannelClosed({}),
        onDataChannelMessage({}),
        onAddVideoTrack({}),        // Initialize new member
        onRenegotiationNeeded({}),  // Initialize new member
        onError({}) {}

  // Copy constructor and assignment operator using default behavior
  // (std::function is copyable)
  PeerConnectionCallbacks(const PeerConnectionCallbacks&) = default;
  PeerConnectionCallbacks& operator=(const PeerConnectionCallbacks&) = default;
  PeerConnectionCallbacks(PeerConnectionCallbacks&&) = default;
  PeerConnectionCallbacks& operator=(PeerConnectionCallbacks&&) = default;
};

}  // namespace webrtc
}  // namespace remote
}  // namespace autodev

#endif  // PEER_CONNECTION_CALLBACKS_H
