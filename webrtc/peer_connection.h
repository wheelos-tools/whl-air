#ifndef I_PEER_CONNECTION_H
#define I_PEER_CONNECTION_H

#include <functional>  // For callbacks if needed in interface methods, though mostly in struct
#include <memory>  // For smart pointers, e.g., shared_ptr for media sources
#include <string>
#include <vector>

#include "webrtc/i_webrtc_manager.h"

// Forward declare potential configuration struct
// In a real system, this would be defined in a config header.
// struct RtcConfiguration; // Configuration for ICE servers, data channels etc.

// Forward declare abstract/interface representation of media sources/tracks
// #include "media/i_media_source.h" // Example interface for a media source

namespace autodev {
namespace remote {
namespace webrtc {

struct PeerConnectionCallbacks;

enum class SignalingState {
  Stable,
  HaveLocalOffer,
  HaveLocalPrAnswer,
  HaveRemoteOffer,
  HaveRemotePrAnswer,
  Closed
};

// Interface for a WebRTC PeerConnection instance.
// Represents a single connection between two peers. Managed by WebrtcManager.
// This interface abstracts the underlying WebRTC library implementation.
class IPeerConnection {
 public:
  virtual ~IPeerConnection() = default;

  // --- Lifecycle and Configuration ---

  // Initializes the PeerConnection instance with configuration and underlying
  // factory/context. This method should be called after object creation and
  // before any operations like CreateOffer/Answer. config: Configuration for
  // this peer connection (ICE servers, DataChannel settings, etc.). callbacks:
  // Struct containing application-level callbacks to report events. Returns
  // true on successful initialization, false otherwise. Note: Real
  // implementations will need access to the libwebrtc PeerConnectionFactory and
  // potentially an event loop context, which should be dependencies passed to
  // this method. virtual bool init(const RtcConfiguration& config, const
  // PeerConnectionCallbacks& callbacks, PeerConnectionFactory* factory,
  // EventLoopContext* event_loop) = 0;
  virtual bool init(/* const RtcConfiguration& config, PeerConnectionFactory* factory, EventLoopContext* event_loop */) = 0; // Simplified for skeleton

  // Sets the callbacks for this peer connection instance.
  // Can be called after init() to update handlers.
  // The implementation should store and use these callbacks to report events.
  // This method should be thread-safe if called from a different thread than
  // the underlying WebRTC signaling thread.
  virtual void SetCallbacks(const PeerConnectionCallbacks& callbacks) = 0;

  // --- Signaling Operations ---

  // Initiates the creation of a local Session Description (Offer).
  // This is an asynchronous operation. The result (SDP string) will be
  // delivered via the onLocalSdpGenerated callback in PeerConnectionCallbacks.
  // Returns true if the offer creation process was successfully initiated.
  virtual bool CreateOffer() = 0;

  // Initiates the creation of a local Session Description (Answer).
  // This is an asynchronous operation, typically called after receiving a
  // remote Offer. The result (SDP string) will be delivered via the
  // onLocalSdpGenerated callback. Returns true if the answer creation process
  // was successfully initiated.
  virtual bool CreateAnswer() = 0;

  // Sets the remote Session Description received from the other peer via
  // signaling. This is an asynchronous operation. sdp_type: "offer" or
  // "answer". sdp_string: The SDP description string. Returns true if the SDP
  // was successfully parsed and set, false on error (e.g., invalid SDP). This
  // method should be thread-safe.
  virtual bool SetRemoteDescription(const std::string& sdp_type,
                                    const std::string& sdp_string) = 0;

  // Adds a remote ICE candidate received from the other peer via signaling.
  // This is an asynchronous operation.
  // candidate: The ICE candidate string.
  // sdp_mid: The media stream identification tag (m= line ID).
  // sdp_mline_index: The index of the m-line in the SDP.
  // Returns true if the candidate was successfully added or queued, false on
  // error (e.g., invalid candidate). This method should be thread-safe.
  virtual bool AddRemoteCandidate(const std::string& candidate,
                                  const std::string& sdp_mid,
                                  int sdp_mline_index) = 0;

  // --- Data Channel Operations ---

  // Creates a new DataChannel associated with this connection.
  // label: The label for the DataChannel.
  // Returns true if the DataChannel creation was successfully initiated.
  // The DataChannel will open asynchronously, reported via onDataChannelOpened
  // callback. virtual bool CreateDataChannel(const std::string& label) = 0; //
  // Optional: If PC creates channels

  // Sends data over a specific DataChannel associated with this connection.
  // Requires the DataChannel with 'label' to be opened (signaled by
  // onDataChannelOpened callback). label: The label of the target DataChannel.
  // data: The data payload (binary bytes).
  // Returns true if message was successfully queued/sent, false if channel is
  // not ready or does not exist. This method MUST BE THREAD-SAFE.
  virtual bool SendData(const std::string& label,
                        const DataChannelMessage& data) = 0;

  // Optional: Overload for sending string data
  virtual bool SendData(const std::string& label, const std::string& data) = 0;

  // --- Media Operations (Optional, if PC manages media tracks directly) ---

  // Adds a local media track (e.g., video or audio source from camera/mic) to
  // this connection. track: Abstract representation of the media track. Returns
  // true if the track was successfully added. The track will be signaled in the
  // SDP. virtual bool AddLocalTrack(std::shared_ptr<IMediaTrack> track) = 0; //
  // Example using an abstract media track interface

  // Removes a local media track from this connection.
  // virtual void RemoveLocalTrack(std::shared_ptr<IMediaTrack> track) = 0;

  // --- Lifecycle Control ---

  // Closes the peer connection, releasing associated resources asynchronously.
  // Triggers onConnectionStateChange callback to a closed state (asynchronous).
  // Safe to call multiple times.
  virtual void Close() = 0;

  // --- State Getters ---

  // Gets the current high-level connection state.
  virtual PeerConnectionState GetConnectionState() const = 0;

  // Gets the current ICE connection state.
  virtual IceConnectionState GetIceConnectionState() const = 0;

  // Gets the current signaling state.
  virtual SignalingState GetSignalingState() const = 0;

  // Optional: Check if a specific DataChannel is open/ready
  // virtual bool IsDataChannelOpen(const std::string& label) const = 0;

 protected:
  // Note: The concrete implementation will store the callbacks provided via
  // SetCallbacks. The interface itself doesn't need a member variable for
  // callbacks.

  // Prevent copying and assignment (PeerConnection instances are unique
  // resources)
  IPeerConnection(const IPeerConnection&) = delete;
  IPeerConnection& operator=(const IPeerConnection&) = delete;
};

}  // namespace webrtc
}  // namespace remote
}  // namespace autodev

#endif  // I_PEER_CONNECTION_H
