#ifndef WEBRTC_MANAGER_IMPL_H
#define WEBRTC_MANAGER_IMPL_H

#include "i_webrtc_manager.h"                  // Include the interface
#include "signaling/signaling_client.h"        // Base SignalingClient interface
#include "webrtc/peer_connection.h"            // Base PeerConnection interface
#include "webrtc/peer_connection_callbacks.h"  // Callbacks struct

// Include configuration relevant to WebRTC/Signaling
// Assuming a specific WebrtcConfig struct exists within config/
// #include "config/webrtc_config.h"
// Assuming a specific SignalingConfig struct exists within config/
// #include "config/signaling_config.h"

#include <atomic>  // For state
#include <chrono>  // For heartbeats
#include <map>
#include <memory>  // unique_ptr, shared_ptr
#include <mutex>   // For synchronization
#include <string>
#include <vector>

#ifndef GUARDED_BY
#define GUARDED_BY(x)
#endif

#ifndef REQUIRES
#define REQUIRES(x)
#endif

#ifndef EXCLUDES
#define EXCLUDES(x)
#endif

// Forward declare concrete implementations (managed by unique_ptr/factories)
// class SignalingClientImpl; // Concrete signaling client
// class LibwebrtcPeerConnection; // Concrete PeerConnection implementation
// using libwebrtc

// Forward declare event loop context if needed
// namespace boost { namespace asio { class io_context; }}
// class TaskQueue; // Another event loop/task queue abstraction

namespace autodev {
namespace remote {
namespace webrtc {

enum class AppState {
  Uninitialized,
  Initializing,
  Initialized,
  Running,
  Stopping,
  Stopped,
};

using PeerConnection = IPeerConnection;

// Configuration struct placeholder (should be defined in config/)
struct WebrtcConfig {
  // Example config members
  std::string signaling_uri;
  std::string client_id;                 // Local client ID
  std::vector<std::string> ice_servers;  // List of ICE servers (STUN/TURN)
  int heartbeat_interval_ms = 5000;      // Heartbeat interval (0 to disable)
  std::string control_channel_label = "control";
  std::string telemetry_channel_label = "telemetry";
  // ... other WebRTC related config
};

// Concrete implementation of the IWebrtcManager interface using specific
// SignalingClient and PeerConnection implementations (e.g., libwebrtc).
class WebrtcManagerImpl : public IWebrtcManager {
 public:
  // Constructor is lightweight, initialization happens in init().
  WebrtcManagerImpl();

  // Destructor. Cleans up all connections and resources.
  ~WebrtcManagerImpl() override;  // Use override

  // --- Implementation of IWebrtcManager Interface ---

  // Initializes internal components and configuration.
  // This replaces the constructor's config role.
  // webrtc_config: Configuration specific to WebRTC and signaling.
  // event_loop: Event loop context or task queue to run WebRTC tasks and
  // callbacks on. CRITICAL for concurrency model. peer_connection_factory: The
  // underlying libwebrtc factory (if managed externally). Returns true if
  // initialization was successful.
  // TODO: Add parameters for config, event loop, libwebrtc factory
  bool init(/* const WebrtcConfig& webrtc_config, EventLoopContext* event_loop, PeerConnectionFactory* factory */) override;

  // Starts all network activity.
  // Must be called after init().
  bool start() override;

  // Stops all network activity.
  void stop() override;

  // Initiates connection to a specific peer.
  bool connectToPeer(const std::string& peer_id) override;

  // Disconnects from a specific peer.
  bool disconnectFromPeer(const std::string& peer_id,
                          const std::string& reason) override;

  // Sends a message over a specific DataChannel to a specific peer.
  // This method MUST BE THREAD-SAFE.
  bool sendDataChannelMessage(const std::string& peer_id,
                              const std::string& channel_label,
                              const DataChannelMessage& data) override;

  // Sends binary data to ALL active PeerConnections on a specific DataChannel.
  // This method MUST BE THREAD-SAFE.
  bool sendDataChannelMessageToAllPeers(
      const std::string& channel_label,
      const DataChannelMessage& data) override;

  // Optional video methods (implement if needed)
  // bool addLocalVideoTrack(...) override;
  // void removeLocalVideoTrack(...) override;

  // --- Implementation of Callback Registration Methods ---
  // These methods are called by the application to set the callbacks.
  // They must be thread-safe as they might be called from a different thread
  // than the one calling callbacks.
  void onSignalingConnected(OnSignalingConnectedHandler handler) override;
  void onSignalingDisconnected(OnSignalingDisconnectedHandler handler) override;
  void onSignalingError(OnSignalingErrorHandler handler) override;
  void onPeerConnected(OnPeerConnectedHandler handler) override;
  void onPeerDisconnected(OnPeerDisconnectedHandler handler) override;
  void onPeerError(OnPeerErrorHandler handler) override;
  void onDataChannelMessageReceived(
      OnDataChannelMessageReceivedHandler handler) override;
  // Optional: void onVideoTrackReceived(...) override;

 protected:  // Use protected for internal helpers if subclasses might need
             // them, otherwise private
  // Configuration (stored after init)
  WebrtcConfig config_;  // Store loaded config

  // --- Internal State ---
  std::atomic<AppState> state_{AppState::Uninitialized};  // State management

  // Mutex to protect access to shared state members (peerConnections_,
  // handlers, heartbeat state etc.)
  mutable std::mutex mutex_;  // mutable because const methods like
                              // sendDataChannelMessage also need to lock

  // Signaling client instance (concrete type managed here)
  // TODO: Should be created and initialized in init() based on config
  std::unique_ptr<SignalingClient> signalingClient_;

  // Map to manage active peer connections. Key is the remote peer ID.
  std::map<std::string, std::unique_ptr<PeerConnection>>
      peerConnections_;  // Access MUST be protected by mutex_

  // Application-level handlers (Callbacks to the App)
  // Access MUST be protected by mutex_ when setting or getting the
  // std::function object itself.
  OnSignalingConnectedHandler onSignalingConnectedHandler_ GUARDED_BY(mutex_);
  OnSignalingDisconnectedHandler onSignalingDisconnectedHandler_
      GUARDED_BY(mutex_);
  OnSignalingErrorHandler onSignalingErrorHandler_ GUARDED_BY(mutex_);
  OnPeerConnectedHandler onPeerConnectedHandler_ GUARDED_BY(mutex_);
  OnPeerDisconnectedHandler onPeerDisconnectedHandler_ GUARDED_BY(mutex_);
  OnPeerErrorHandler onPeerErrorHandler_ GUARDED_BY(mutex_);
  OnDataChannelMessageReceivedHandler onDataChannelMessageReceivedHandler_
      GUARDED_BY(mutex_);
  // Optional: OnVideoTrackReceivedHandler onVideoTrackReceivedHandler_
  // GUARDED_BY(mutex_);

  // State for heartbeats and reconnection logic (Access MUST be protected by
  // mutex_) Needs a timer mechanism integrated with the event loop.
  // std::unique_ptr<Timer> heartbeatTimer_ GUARDED_BY(mutex_); // Example timer
  std::map<std::string, std::chrono::steady_clock::time_point>
      lastHeartbeatRxTime_
          GUARDED_BY(mutex_);  // Track last received time per peer
  std::map<std::string, int> reconnectionAttemptCount_
      GUARDED_BY(mutex_);  // Track reconnection attempts per peer

  struct PendingRemoteCandidate {
    std::string candidate;
    std::string sdp_mid;
    int sdp_mline_index;
  };

  // Buffer remote ICE candidates that arrive before SetRemoteDescription
  // succeeds for a peer. Access MUST be protected by mutex_.
  std::map<std::string, bool> remoteDescriptionSet_ GUARDED_BY(mutex_);
  std::map<std::string, std::vector<PendingRemoteCandidate>>
      pendingRemoteCandidates_ GUARDED_BY(mutex_);

  // --- Internal Handlers (Called by SignalingClient or PeerConnection
  // Callbacks) --- These methods implement the core logic of the manager. These
  // methods are called from background threads (WebRTC/Signaling); MUST ACQUIRE
  // mutex_.

  // Handlers for SignalingClient events (Called by SignalingClient thread;
  // ACQUIRE mutex_)
  void handleSignalingConnected();
  void
  handleSignalingDisconnected();  // Should trigger peer disconnections/cleanup
  void handleSignalingError(const std::string& msg);
  void handleSignalingMessage(
      const SignalMessage& message);  // Main message routing logic

  // Handlers for PeerConnection events (Called by WebRTC Signaling thread;
  // ACQUIRE mutex_) Lambdas capturing peer_id are used to pass the peer
  // context.
  void handlePeerLocalSdpGenerated(const std::string& peer_id,
                                   const std::string& sdp_type,
                                   const std::string& sdp_string);
  void handlePeerLocalCandidateGenerated(const std::string& peer_id,
                                         const std::string& candidate,
                                         const std::string& sdp_mid,
                                         int sdp_mline_index);
  // Need to map int state from libwebrtc to custom enums or strings for clarity
  // if needed
  void handlePeerConnectionStateChange(
      const std::string& peer_id,
      PeerConnectionState state);  // Handles high-level PC state
  void handlePeerIceConnectionStateChange(
      const std::string& peer_id,
      IceConnectionState state);  // Handles ICE connectivity
  void handlePeerSignalingStateChange(const std::string& peer_id,
                                      int state);  // Raw signaling state int
  void handlePeerDataChannelOpened(const std::string& peer_id,
                                   const std::string& label);
  void handlePeerDataChannelClosed(
      const std::string& peer_id,
      const std::string& label);  // Should handle channels closing
  void handlePeerDataChannelMessage(
      const std::string& peer_id, const std::string& label,
      const DataChannelMessage& message);  // Routes messages based on label
  void handlePeerError(
      const std::string& peer_id,
      const std::string& error_msg);  // Handles PC-specific errors

  // --- Internal Manager Logic ---

  // Helper to get or create a PeerConnection for a given peer ID based on
  // received signal. Sets up the callbacks for the PeerConnection. ACQUIRE
  // mutex_.
  PeerConnection* getOrCreatePeerConnection(const std::string& peer_id)
      REQUIRES(mutex_);

  // Method to destroy a PeerConnection and clean up state. ACQUIRE mutex_.
  void destroyPeerConnection(const std::string& peer_id,
                             const std::string& reason) REQUIRES(mutex_);

  // Needs a factory or creation method for concrete PeerConnection instances
  // This factory needs access to the libwebrtc PeerConnectionFactory and
  // EventLoopContext
  // TODO: Add parameters for event loop context and libwebrtc factory
  virtual std::unique_ptr<PeerConnection> createPeerConnection(const std::
                                                                   string&
                                                                       peer_id,
                                                               PeerConnectionCallbacks
                                                                   callbacks /*, EventLoopContext* event_loop, PeerConnectionFactory* factory*/)
      REQUIRES(mutex_);

  // Heartbeat and Reconnection Logic (Access MUST be protected by mutex_)
  // Needs a timer mechanism integrated with the event loop.
  void startHeartbeatTimer()
      EXCLUDES(mutex_);  // Start/stop timer might need external coordination or
                         // run loop access
  void stopHeartbeatTimer() EXCLUDES(mutex_);
  void
  onHeartbeatTimer();  // Called by the timer when it fires (ACQUIRE mutex_)
  void sendHeartbeat(const std::string& peer_id)
      REQUIRES(mutex_);  // Sends a heartbeat message (ACQUIRE mutex_)
  void handleReceivedHeartbeat(
      const std::string&
          peer_id);  // Updates lastHeartbeatRxTime_ (ACQUIRE mutex_)
  void checkForHeartbeatLoss()
      REQUIRES(mutex_);  // Checks last received times (ACQUIRE mutex_)

  void attemptReconnection(
      const std::string&
          peer_id);  // Tries to reconnect to a peer (ACQUIRE mutex_)

  // Helper to safely invoke application callbacks (ACQUIRE mutex_ for getting
  // handler, then release) Need to decide if callbacks are marshalled to a
  // specific thread or called directly from background threads.
  void invokeSignalingConnectedCallback();
  void invokeSignalingDisconnectedCallback(const std::string& reason);
  void invokeSignalingErrorCallback(const std::string& error_msg);
  void invokePeerConnectedCallback(const std::string& peer_id);
  void invokePeerDisconnectedCallback(const std::string& peer_id,
                                      const std::string& reason);
  void invokePeerErrorCallback(const std::string& peer_id,
                               const std::string& error_msg);
  void invokeDataChannelMessageReceivedCallback(
      const std::string& peer_id, const std::string& label,
      const DataChannelMessage& message);
  // Optional: void invokeVideoTrackReceivedCallback(...)

 private:
  // Prevent copying and assignment - already in IWebrtcManager, but repeat here
  // as good practice
  WebrtcManagerImpl(const WebrtcManagerImpl&) = delete;
  WebrtcManagerImpl& operator=(const WebrtcManagerImpl&) = delete;
};

}  // namespace webrtc
}  // namespace remote
}  // namespace autodev

#endif  // WEBRTC_MANAGER_IMPL_H
