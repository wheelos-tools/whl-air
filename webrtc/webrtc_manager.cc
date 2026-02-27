#include "webrtc/webrtc_manager.h"  // Include the implementation header

// Include concrete implementations (linked, not included as .cc)
// #include "signaling/websocket_signaling_client.h" // Concrete signaling
// client #include "webrtc/libwebrtc_peer_connection.h" // Concrete
// PeerConnection implementation

// Include other necessary headers
#include "signaling/signaling_message.h"  // SignalMessage definition
// #include "config/webrtc_config.h" // Include actual config struct if used
// #include "event_loop/event_loop_context.h" // Include event loop context
// header #include "webrtc/rtc_base/thread.h" // For libwebrtc thread management
// #include "webrtc/api/peer_connection_interface.h" // For
// PeerConnectionFactoryInterface

#include <algorithm>  // For std::find_if
#include <chrono>     // For std::chrono
#include <iostream>
#include <string>
#include <thread>   // For std::this_thread::sleep_for (if simulating timers)
#include <utility>  // For std::move
#include <vector>   // For std::vector

// Dummy config placeholder if actual config struct is not included
namespace autodev {
namespace remote {
namespace config {
struct VehicleConfig {};
}  // namespace config
}  // namespace remote
}  // namespace autodev

namespace autodev {
namespace remote {
namespace webrtc {

// --- WebrtcManagerImpl Implementation ---

WebrtcManagerImpl::WebrtcManagerImpl() : state_(AppState::Uninitialized) {
  std::cout << "WebrtcManagerImpl created." << std::endl;
  // libwebrtc initialization and thread creation should ideally happen BEFORE
  // this constructor, managed by the application or a dedicated wrapper,
  // and the factory/context passed into init().
  // TODO: If WebrtcManagerImpl OWNS the libwebrtc factory/threads, initialize
  // them here or in init().
}

WebrtcManagerImpl::~WebrtcManagerImpl() {
  std::cout << "WebrtcManagerImpl destroying..." << std::endl;
  // Ensure stop is called to clean up resources managed by this class
  stop();
  // TODO: If WebrtcManagerImpl OWNS the libwebrtc factory/threads, shut them
  // down here.
  std::cout << "WebrtcManagerImpl destroyed." << std::endl;
}

// Implementation of IWebrtcManager::init
// TODO: Add parameters: const WebrtcConfig& webrtc_config, EventLoopContext*
// event_loop, PeerConnectionFactory* factory
bool WebrtcManagerImpl::init(/* const WebrtcConfig& webrtc_config, EventLoopContext* event_loop, PeerConnectionFactory* factory */) {
  AppState expected = AppState::Uninitialized;
  if (!state_.compare_exchange_strong(expected, AppState::Initializing)) {
    std::cerr
        << "WebrtcManagerImpl: Already initialized or in a different state."
        << std::endl;
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Initializing..." << std::endl;

  // TODO: Store config_ = webrtc_config;
  // TODO: Store event_loop_ = event_loop;
  // TODO: Store factory_ = factory; // Store libwebrtc factory

  // TODO: Create the concrete signaling client implementation
  // It needs config and callbacks, potentially the event loop context
  // signalingClient_ = std::make_unique<WebsocketSignalingClient>(
  //    config_.signaling_uri, config_.signaling_jwt, event_loop_, this); //
  //    Pass config, context, and 'this' as handler sink

  // Skeleton mode: no concrete signaling implementation is wired here yet.
  // Keep a null signaling client until real dependency wiring is complete.
  signalingClient_.reset();

  // TODO: Initialize heartbeat timer if enabled in config
  // if (config_.heartbeat_interval_ms > 0) {
  //     startHeartbeatTimer(); // Start the timer (implementation needs
  //     event_loop_)
  // }

  state_ = AppState::Initialized;
  std::cout << "WebrtcManagerImpl: Initialization successful." << std::endl;
  return true;
}

// Implementation of IWebrtcManager::start
bool WebrtcManagerImpl::start() {
  AppState expected = AppState::Initialized;
  if (!state_.compare_exchange_strong(expected, AppState::Running)) {
    std::cerr << "WebrtcManagerImpl: Cannot start, not in Initialized state."
              << std::endl;
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Starting..." << std::endl;

  if (!signalingClient_) {
    std::cerr << "WebrtcManagerImpl: Signaling client not initialized."
              << std::endl;
    state_ = AppState::Stopped;  // Cannot run without signaling
    return false;
  }

  // Connect the signaling client - this is typically asynchronous
  signalingClient_->connect();
  std::cout << "WebrtcManagerImpl: start completed." << std::endl;
  return true;
}

// Implementation of IWebrtcManager::stop
void WebrtcManagerImpl::stop() {
  AppState expected = AppState::Running;
  if (!state_.compare_exchange_strong(expected, AppState::Stopping)) {
    expected = AppState::Initialized;  // Allow stopping from Initialized state
    if (!state_.compare_exchange_strong(expected, AppState::Stopping)) {
      std::cout << "WebrtcManagerImpl: Already stopping, stopped, or "
                   "uninitialized. Skipping stop."
                << std::endl;
      return;
    }
  }

  // Acquire lock while stopping resources managed by this class
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Stopping..." << std::endl;

  // TODO: Stop heartbeat timer
  // stopHeartbeatTimer(); // Implementation needs event_loop_

  // Close all peer connections managed by this manager
  std::cout << "WebrtcManagerImpl: Closing " << peerConnections_.size()
            << " peer connections." << std::endl;
  for (auto const& [peer_id, pc] : peerConnections_) {
    if (pc) {
      pc->Close();  // This should trigger PeerConnectionState::kClosed/Failed
                    // callbacks
    }
  }
  // Clear the map to release unique_ptrs. This must happen AFTER Close()
  // is called and potential cleanup callbacks finish, or
  // PeerConnection::Close() is blocking until cleanup. Be careful with thread
  // synchronization here. A common pattern is to clear the map only after the
  // WebRTC signaling thread is stopped or PeerConnection callbacks have a safe
  // way to check manager validity. For simplicity in skeleton, clear after
  // signaling disconnect.
  peerConnections_.clear();  // Release unique_ptrs

  // Disconnect signaling client - this is typically asynchronous
  if (signalingClient_) {
    signalingClient_->disconnect();
    // After signaling disconnect, the signaling client should stop calling
    // handlers.
  }

  state_ = AppState::Stopped;  // Final state
  std::cout << "WebrtcManagerImpl: stop completed." << std::endl;
}

// Implementation of IWebrtcManager::connectToPeer
bool WebrtcManagerImpl::connectToPeer(const std::string& peer_id) {
  // This method is called by the application thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Attempting to connect to peer: " << peer_id
            << std::endl;

  // Check if state is Running
  if (state_ != AppState::Running) {
    std::cerr
        << "WebrtcManagerImpl: Cannot connect to peer, manager is not running."
        << std::endl;
    return false;
  }

  // Check if peer connection already exists
  if (peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: Peer connection to " << peer_id
              << " already exists." << std::endl;
    return true;  // Or false, depending on policy
  }

  // Create a new PeerConnection instance
  // This call to getOrCreatePeerConnection also acquires the mutex internally
  // or uses the same mutex if called from within the lock.
  // Let's call createPeerConnection directly and then add to map.
  std::cout << "WebrtcManagerImpl: Creating new PeerConnection for outbound "
               "connection to "
            << peer_id << std::endl;

  // Prepare callbacks for the new PC
  PeerConnectionCallbacks pc_callbacks;
  // Use lambdas capturing peer_id and 'this'. Ensure 'this' is valid when
  // lambda is called. Using a shared_ptr to the manager might be safer if
  // callbacks can outlive the manager temporarily. For now, rely on the
  // manager's stop() ensuring callbacks are not called after destruction.
  pc_callbacks.onLocalSdpGenerated = [this, peer_id](
                                         const std::string& sdp_type,
                                         const std::string& sdp_string) {
    handlePeerLocalSdpGenerated(peer_id, sdp_type,
                                sdp_string);  // This handler ACQUIRES mutex_
  };
  pc_callbacks.onLocalCandidateGenerated =
      [this, peer_id](const std::string& candidate, const std::string& sdp_mid,
                      int sdp_mline_index) {
        handlePeerLocalCandidateGenerated(
            peer_id, candidate, sdp_mid,
            sdp_mline_index);  // This handler ACQUIRES mutex_
      };
  pc_callbacks.onConnectionStateChange = [this,
                                          peer_id](PeerConnectionState state) {
    handlePeerConnectionStateChange(peer_id,
                                    state);  // This handler ACQUIRES mutex_
  };
  pc_callbacks.onIceConnectionStateChange =
      [this, peer_id](IceConnectionState state) {
        handlePeerIceConnectionStateChange(
            peer_id, state);  // This handler ACQUIREs mutex_
      };
  pc_callbacks.onSignalingStateChange = [this, peer_id](SignalingState state) {
    handlePeerSignalingStateChange(
        peer_id, static_cast<int>(state));  // This handler ACQUIREs mutex_
  };
  pc_callbacks.onDataChannelOpened = [this, peer_id](const std::string& label) {
    handlePeerDataChannelOpened(peer_id,
                                label);  // This handler ACQUIREs mutex_
  };
  pc_callbacks.onDataChannelClosed = [this, peer_id](const std::string& label) {
    handlePeerDataChannelClosed(peer_id,
                                label);  // This handler ACQUIRES mutex_
  };
  pc_callbacks.onDataChannelMessage = [this, peer_id](
                                          const std::string& label,
                                          const DataChannelMessage& message) {
    handlePeerDataChannelMessage(peer_id, label,
                                 message);  // This handler ACQUIRES mutex_
  };
  pc_callbacks.onError = [this, peer_id](const std::string& error_msg) {
    handlePeerError(peer_id, error_msg);  // This handler ACQUIRES mutex_
  };
  // TODO: Add VideoTrackReceived callback for Cockpit side

  // Create the PC instance using the factory method
  // This method is called from within the mutex_ lock.
  auto pc =
      createPeerConnection(peer_id, pc_callbacks /*, event_loop_, factory_ */);

  if (!pc) {
    std::cerr << "WebrtcManagerImpl: Failed to create PeerConnection for "
              << peer_id << std::endl;
    return false;
  }

  // Store the new PC in the map
  peerConnections_[peer_id] = std::move(pc);

  // Initiate the offer/answer process for this peer connection
  // Vehicle side typically creates offer, Cockpit side creates answer upon
  // receiving offer. Assuming this WebrtcManager is on the Vehicle side (from
  // VehicleConfig in original code), it might create an offer immediately upon
  // connecting to a peer (if client initiated) or wait for an offer from the
  // peer. Let's assume for this `connectToPeer` method that the LOCAL side
  // initiates the connection. If this is a Vehicle connecting to a Cockpit,
  // Vehicle should create offer. If this is a Cockpit connecting to a Vehicle,
  // Cockpit should create offer. This method should ideally only be called on
  // the initiating side. Let's assume this method is on the OFFERING side.
  peerConnections_[peer_id]
      ->CreateOffer();  // This triggers onLocalSdpGenerated callback
                        // asynchronously

  std::cout << "WebrtcManagerImpl: Initiated connection process for peer "
            << peer_id << std::endl;
  return true;
}

// Implementation of IWebrtcManager::disconnectFromPeer
bool WebrtcManagerImpl::disconnectFromPeer(const std::string& peer_id,
                                           const std::string& reason) {
  // This method is called by the application thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Attempting to disconnect from peer: "
            << peer_id << " Reason: " << reason << std::endl;

  auto it = peerConnections_.find(peer_id);
  if (it != peerConnections_.end()) {
    // Call Close on the PeerConnection. This triggers cleanup callbacks.
    it->second->Close();
    // The PC will be removed from the map later in handlePeerDisconnected after
    // callbacks finish.
    std::cout << "WebrtcManagerImpl: Called Close() on peer connection for "
              << peer_id << std::endl;
    return true;
  } else {
    std::cerr << "WebrtcManagerImpl: Peer " << peer_id
              << " not found for disconnection." << std::endl;
    return false;
  }
}

// Implementation of IWebrtcManager::sendDataChannelMessage
// This method is called by application threads (e.g., CommandHandler,
// TelemetryHandler). MUST BE THREAD-SAFE.
bool WebrtcManagerImpl::sendDataChannelMessage(const std::string& peer_id,
                                               const std::string& channel_label,
                                               const DataChannelMessage& data) {
  // Acquire lock to safely access peerConnections_
  std::lock_guard<std::mutex> lock(mutex_);

  // Check if state is Running
  if (state_ != AppState::Running) {
    // std::cerr << "WebrtcManagerImpl: Cannot send data, manager is not
    // running." << std::endl; Report error via application callback? Or just
    // return false.
    return false;
  }

  auto it = peerConnections_.find(peer_id);
  if (it != peerConnections_.end() &&
      it->second) {  // Check if iterator is valid and unique_ptr is not null
    // Call SendData on the PeerConnection interface
    // This call itself should be thread-safe within the PeerConnection
    // implementation, but accessing the PeerConnection object pointer requires
    // the mutex.
    bool success = it->second->SendData(channel_label, data);
    // std::cout << "WebrtcManagerImpl: Sent data to " << peer_id << " on label
    // " << channel_label << ", size=" << data.size() << (success ? "" : "
    // (failed)") << std::endl;
    return success;
  } else {
    // std::cerr << "WebrtcManagerImpl: Cannot send data, peer " << peer_id << "
    // not found or PC is null." << std::endl; Report error via application
    // callback? invokePeerErrorCallback(peer_id, "Attempted to send data to
    // unknown or invalid peer connection."); // Needs to acquire lock
    // internally
    return false;
  }
}

// Implementation of IWebrtcManager::sendDataChannelMessageToAllPeers
// This method is called by application threads (e.g., TelemetryHandler). MUST
// BE THREAD-SAFE.
bool WebrtcManagerImpl::sendDataChannelMessageToAllPeers(
    const std::string& channel_label, const DataChannelMessage& data) {
  // Acquire lock to safely iterate through peerConnections_
  std::lock_guard<std::mutex> lock(mutex_);

  // Check if state is Running
  if (state_ != AppState::Running) {
    // std::cerr << "WebrtcManagerImpl: Cannot broadcast data, manager is not
    // running." << std::endl;
    return false;
  }

  bool any_sent = false;
  // Iterate through all active peer connections
  for (auto const& [peer_id, pc] : peerConnections_) {
    if (pc) {  // Check if unique_ptr is not null
      // Call SendData on each PeerConnection
      // PeerConnection::SendData should be thread-safe.
      bool sent = pc->SendData(channel_label, data);
      if (sent) any_sent = true;
      // Log or handle individual send failures if needed
      // std::cout << "WebrtcManagerImpl: Broadcasted data to " << peer_id << "
      // on label " << channel_label << (sent ? "" : " (failed)") << std::endl;
    }
  }
  return any_sent;  // Return true if at least one message was sent successfully
}

// Implementation of IWebrtcManager::onSignalingConnected etc. (Callback
// registration) These methods are called by the application thread. They must
// be thread-safe as the handlers might be read from different threads.
void WebrtcManagerImpl::onSignalingConnected(
    OnSignalingConnectedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);  // Protect setting the handler
  onSignalingConnectedHandler_ = handler;
}
void WebrtcManagerImpl::onSignalingDisconnected(
    OnSignalingDisconnectedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onSignalingDisconnectedHandler_ = handler;
}
void WebrtcManagerImpl::onSignalingError(OnSignalingErrorHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onSignalingErrorHandler_ = handler;
}
void WebrtcManagerImpl::onPeerConnected(OnPeerConnectedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onPeerConnectedHandler_ = handler;
}
void WebrtcManagerImpl::onPeerDisconnected(OnPeerDisconnectedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onPeerDisconnectedHandler_ = handler;
}
void WebrtcManagerImpl::onPeerError(OnPeerErrorHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onPeerErrorHandler_ = handler;
}
void WebrtcManagerImpl::onDataChannelMessageReceived(
    OnDataChannelMessageReceivedHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  onDataChannelMessageReceivedHandler_ = handler;
}
// Optional: void WebrtcManagerImpl::onVideoTrackReceived(...) { ... }

// --- Internal Handlers for SignalingClient Events ---
// These methods are called by the SignalingClient's thread. They must acquire
// mutex_.

void WebrtcManagerImpl::handleSignalingConnected() {
  OnSignalingConnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "WebrtcManagerImpl: Signaling connected." << std::endl;
    handler = onSignalingConnectedHandler_;
  }

  if (handler) {
    handler();
  }
}

void WebrtcManagerImpl::handleSignalingDisconnected() {
  OnSignalingDisconnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "WebrtcManagerImpl: Signaling disconnected." << std::endl;
    handler = onSignalingDisconnectedHandler_;
  }

  if (handler) {
    handler("Signaling connection lost");
  }
}

void WebrtcManagerImpl::handleSignalingError(const std::string& msg) {
  OnSignalingErrorHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "WebrtcManagerImpl: Signaling error: " << msg << std::endl;
    handler = onSignalingErrorHandler_;
  }

  if (handler) {
    handler(msg);
  }
}

void WebrtcManagerImpl::handleSignalingMessage(const SignalMessage& message) {
  OnPeerErrorHandler peer_error_handler;
  OnSignalingErrorHandler signaling_error_handler;
  std::string peer_error_message;
  std::string signaling_error_message;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "WebrtcManagerImpl: Received signal message from "
              << message.from
              << " type=" << SignalMessage::TypeToString(message.type)
              << std::endl;

    std::string peer_id = message.from;

    switch (message.type) {
      case SignalMessage::Type::JOIN:
        std::cout << "WebrtcManagerImpl: Peer " << peer_id << " joined."
                  << std::endl;
        break;
      case SignalMessage::Type::LEAVE:
        std::cout << "WebrtcManagerImpl: Peer " << peer_id << " left."
                  << std::endl;
        if (peerConnections_.count(peer_id) && peerConnections_[peer_id]) {
          peerConnections_[peer_id]->Close();
          remoteDescriptionSet_.erase(peer_id);
          pendingRemoteCandidates_.erase(peer_id);
        }
        break;
      case SignalMessage::Type::OFFER: {
        PeerConnection* pc = getOrCreatePeerConnection(peer_id);
        if (pc && message.sdp) {
          if (pc->SetRemoteDescription("offer", *message.sdp)) {
            remoteDescriptionSet_[peer_id] = true;

            auto pending_it = pendingRemoteCandidates_.find(peer_id);
            if (pending_it != pendingRemoteCandidates_.end()) {
              for (const auto& pending : pending_it->second) {
                pc->AddRemoteCandidate(pending.candidate, pending.sdp_mid,
                                       pending.sdp_mline_index);
              }
              pendingRemoteCandidates_.erase(pending_it);
            }

            pc->CreateAnswer();
          } else {
            peer_error_handler = onPeerErrorHandler_;
            peer_error_message =
                "Failed to set remote OFFER before CreateAnswer for " + peer_id;
          }
        } else {
          std::cerr
              << "WebrtcManagerImpl: Received OFFER without SDP or PC not "
                 "created for "
              << peer_id << std::endl;
          peer_error_handler = onPeerErrorHandler_;
          peer_error_message = "Received OFFER with missing SDP or PC";
        }
        break;
      }
      case SignalMessage::Type::ANSWER: {
        PeerConnection* pc = getOrCreatePeerConnection(peer_id);
        if (pc && message.sdp) {
          if (!pc->SetRemoteDescription("answer", *message.sdp)) {
            peer_error_handler = onPeerErrorHandler_;
            peer_error_message = "Failed to set remote ANSWER for " + peer_id;
          } else {
            remoteDescriptionSet_[peer_id] = true;

            auto pending_it = pendingRemoteCandidates_.find(peer_id);
            if (pending_it != pendingRemoteCandidates_.end()) {
              for (const auto& pending : pending_it->second) {
                pc->AddRemoteCandidate(pending.candidate, pending.sdp_mid,
                                       pending.sdp_mline_index);
              }
              pendingRemoteCandidates_.erase(pending_it);
            }
          }
        } else {
          std::cerr
              << "WebrtcManagerImpl: Received ANSWER without SDP or PC not "
                 "created for "
              << peer_id << std::endl;
          peer_error_handler = onPeerErrorHandler_;
          peer_error_message = "Received ANSWER with missing SDP or PC";
        }
        break;
      }
      case SignalMessage::Type::CANDIDATE: {
        PeerConnection* pc = getOrCreatePeerConnection(peer_id);
        if (pc && message.candidate && message.sdpMid &&
            message.sdpMlineIndex) {
          if (!remoteDescriptionSet_[peer_id]) {
            pendingRemoteCandidates_[peer_id].push_back(PendingRemoteCandidate{
                *message.candidate, *message.sdpMid, *message.sdpMlineIndex});
          } else {
            pc->AddRemoteCandidate(*message.candidate, *message.sdpMid,
                                   *message.sdpMlineIndex);
          }
        } else {
          std::cerr << "WebrtcManagerImpl: Received CANDIDATE with missing "
                       "fields or PC not created for "
                    << peer_id << std::endl;
          peer_error_handler = onPeerErrorHandler_;
          peer_error_message = "Received CANDIDATE with missing fields or PC";
        }
        break;
      }
      case SignalMessage::Type::UNKNOWN:
      default:
        std::cerr
            << "WebrtcManagerImpl: Received unknown signal message type from "
            << peer_id << std::endl;
        signaling_error_handler = onSignalingErrorHandler_;
        signaling_error_message =
            "Received unknown signal message type from " + peer_id;
        break;
    }
  }

  if (peer_error_handler && !peer_error_message.empty()) {
    peer_error_handler(message.from, peer_error_message);
  }
  if (signaling_error_handler && !signaling_error_message.empty()) {
    signaling_error_handler(signaling_error_message);
  }
}

// --- Internal Helper to get or create PeerConnection ---
// This method is called from within the mutex_ lock.
PeerConnection* WebrtcManagerImpl::getOrCreatePeerConnection(
    const std::string& peer_id) {
  // Lock is assumed to be held by the caller (e.g., handleSignalingMessage)
  // std::lock_guard<std::mutex> lock(mutex_); // If not called under lock,
  // acquire here

  auto it = peerConnections_.find(peer_id);
  if (it != peerConnections_.end()) {
    return it->second.get();  // Return raw pointer
  }

  std::cout << "WebrtcManagerImpl: Creating new PeerConnection for peer "
            << peer_id << std::endl;

  // Prepare callbacks for the new PC.
  // Lambdas capture peer_id and 'this'. Ensure 'this' is valid when lambda is
  // called. Using a shared_ptr to the manager might be safer if callbacks can
  // outlive the manager temporarily. For now, rely on the manager's stop()
  // ensuring callbacks are not called after destruction.
  PeerConnectionCallbacks pc_callbacks;
  pc_callbacks.onLocalSdpGenerated = [this, peer_id](
                                         const std::string& sdp_type,
                                         const std::string& sdp_string) {
    handlePeerLocalSdpGenerated(peer_id, sdp_type,
                                sdp_string);  // ACQUIRES mutex_
  };
  pc_callbacks.onLocalCandidateGenerated =
      [this, peer_id](const std::string& candidate, const std::string& sdp_mid,
                      int sdp_mline_index) {
        handlePeerLocalCandidateGenerated(peer_id, candidate, sdp_mid,
                                          sdp_mline_index);  // ACQUIRES mutex_
      };
  pc_callbacks.onConnectionStateChange = [this,
                                          peer_id](PeerConnectionState state) {
    handlePeerConnectionStateChange(peer_id, state);  // ACQUIRES mutex_
  };
  pc_callbacks.onIceConnectionStateChange =
      [this, peer_id](IceConnectionState state) {
        handlePeerIceConnectionStateChange(peer_id, state);  // ACQUIRES mutex_
      };
  pc_callbacks.onSignalingStateChange = [this, peer_id](SignalingState state) {
    handlePeerSignalingStateChange(peer_id,
                                   static_cast<int>(state));  // ACQUIRES mutex_
  };
  pc_callbacks.onDataChannelOpened = [this, peer_id](const std::string& label) {
    handlePeerDataChannelOpened(peer_id, label);  // ACQUIRES mutex_
  };
  pc_callbacks.onDataChannelClosed = [this, peer_id](const std::string& label) {
    handlePeerDataChannelClosed(peer_id, label);  // ACQUIRES mutex_
  };
  pc_callbacks.onDataChannelMessage = [this, peer_id](
                                          const std::string& label,
                                          const DataChannelMessage& message) {
    handlePeerDataChannelMessage(peer_id, label, message);  // ACQUIRES mutex_
  };
  pc_callbacks.onError = [this, peer_id](const std::string& error_msg) {
    handlePeerError(peer_id, error_msg);  // ACQUIRES mutex_
  };
  // TODO: Add OnAddStream/OnRemoveStream for media tracks (Cockpit side)
  // TODO: Add OnDataChannel for receiving incoming DataChannels (Cockpit side)

  // Create the concrete PC instance using the factory method (Implemented
  // below) This method needs access to libwebrtc factory and event loop context
  auto pc =
      createPeerConnection(peer_id, pc_callbacks /*, event_loop_, factory_ */);

  if (!pc) {
    std::cerr << "WebrtcManagerImpl: Failed to create PeerConnection for "
              << peer_id << std::endl;
    return nullptr;
  }

  // Store and return the new PC
  // The map access is protected by the caller's lock
  peerConnections_[peer_id] = std::move(pc);
  remoteDescriptionSet_[peer_id] = false;
  pendingRemoteCandidates_.erase(peer_id);
  return peerConnections_[peer_id].get();
}

// Helper to destroy a PeerConnection and clean up state.
// This method is called from within the mutex_ lock.
void WebrtcManagerImpl::destroyPeerConnection(const std::string& peer_id,
                                              const std::string& reason) {
  // Lock is assumed to be held by the caller (e.g.,
  // handlePeerConnectionStateChange, handleSignalingMessage)
  // std::lock_guard<std::mutex> lock(mutex_); // If not called under lock,
  // acquire here

  auto it = peerConnections_.find(peer_id);
  if (it != peerConnections_.end()) {
    std::cout << "WebrtcManagerImpl: Destroying PeerConnection for peer "
              << peer_id << ". Reason: " << reason << std::endl;

    // Call Close() explicitly before erasing if not already done by the state
    // change
    if (it->second) {
      it->second->Close();
    }

    // Remove from heartbeat tracking
    lastHeartbeatRxTime_.erase(peer_id);
    reconnectionAttemptCount_.erase(peer_id);
    remoteDescriptionSet_.erase(peer_id);
    pendingRemoteCandidates_.erase(peer_id);

    // Remove the unique_ptr from the map (this destroys the PeerConnection
    // object)
    peerConnections_.erase(it);

    // Do not invoke application callbacks while manager mutex is held.
    // Callers should notify disconnection after lock release.
  } else {
    // std::cout << "WebrtcManagerImpl: Attempted to destroy non-existent PC for
    // " << peer_id << std::endl;
  }
}

// --- Factory method for creating PeerConnection instances ---
// This method is called from within the mutex_ lock.
// TODO: Needs EventLoopContext* event_loop and PeerConnectionFactory* factory
// parameters
std::
    unique_ptr<PeerConnection>
    WebrtcManagerImpl::createPeerConnection(const std::string& peer_id,
                                            PeerConnectionCallbacks callbacks /*, EventLoopContext* event_loop, PeerConnectionFactory* factory */) {
  // Lock is assumed to be held by the caller (getOrCreatePeerConnection or
  // connectToPeer) std::lock_guard<std::mutex> lock(mutex_); // If not called
  // under lock, acquire here

  std::cout << "WebrtcManagerImpl: Using factory to create PeerConnection for "
            << peer_id << std::endl;

  // TODO: Use the actual libwebrtc factory and event loop context
  // Example (conceptual):
  // webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  // // Populate rtc_config from config_ (ICE servers etc.)
  // rtc_config.ice_servers.push_back(webrtc::PeerConnectionInterface::IceServer());
  // // Populate ICE servers

  // auto rtc_pc = factory->CreatePeerConnection(rtc_config, nullptr, nullptr,
  // callbacks_adapter); // Use a callbacks adapter to bridge between libwebrtc
  // observer and our callbacks struct

  // if (!rtc_pc) {
  //     std::cerr << "Failed to create libwebrtc PeerConnection." << std::endl;
  //     return nullptr;
  // }

  // TODO: Create the concrete PeerConnection wrapper implementation
  // auto pc_impl = std::make_unique<LibwebrtcPeerConnection>(rtc_pc,
  // callbacks); // Pass libwebrtc PC and our callbacks struct

  // Skeleton mode: no concrete PeerConnection implementation is wired here.
  // Return nullptr so callers can fail gracefully until integration is added.
  (void)callbacks;
  return nullptr;
}

// --- Internal Handlers for PeerConnection Events ---
// These methods are called by the WebRTC Signaling thread (via PeerConnection
// Callbacks). They must acquire mutex_.

void WebrtcManagerImpl::handlePeerLocalSdpGenerated(
    const std::string& peer_id, const std::string& sdp_type,
    const std::string& sdp_string) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  OnSignalingErrorHandler error_handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "WebrtcManagerImpl: Local SDP generated for " << peer_id
              << ", type=" << sdp_type << std::endl;

    // Check if the peer connection still exists
    if (!peerConnections_.count(peer_id)) {
      std::cout << "WebrtcManagerImpl: Ignoring SDP for non-existent peer "
                << peer_id << std::endl;
      return;
    }

    SignalMessage msg;
    msg.type = (sdp_type == "offer") ? SignalMessage::Type::OFFER
                                     : SignalMessage::Type::ANSWER;
    msg.from = "client_dummy_id";
    msg.to = peer_id;
    msg.sdp = sdp_string;

    if (signalingClient_) {
      signalingClient_->sendSignal(msg);
    } else {
      std::cerr
          << "WebrtcManagerImpl: Signaling client not available to send SDP."
          << std::endl;
      error_handler = onSignalingErrorHandler_;
    }
  }

  if (error_handler) {
    error_handler("Signaling client not available to send SDP");
  }
}

void WebrtcManagerImpl::handlePeerLocalCandidateGenerated(
    const std::string& peer_id, const std::string& candidate,
    const std::string& sdp_mid, int sdp_mline_index) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  OnSignalingErrorHandler error_handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "WebrtcManagerImpl: Local Candidate generated for " << peer_id
              << std::endl;

    if (!peerConnections_.count(peer_id)) {
      std::cout
          << "WebrtcManagerImpl: Ignoring candidate for non-existent peer "
          << peer_id << std::endl;
      return;
    }

    SignalMessage msg;
    msg.type = SignalMessage::Type::CANDIDATE;
    msg.from = "client_dummy_id";
    msg.to = peer_id;
    msg.candidate = candidate;
    msg.sdpMid = sdp_mid;
    msg.sdpMlineIndex = sdp_mline_index;

    if (signalingClient_) {
      signalingClient_->sendSignal(msg);
    } else {
      std::cerr << "WebrtcManagerImpl: Signaling client not available to send "
                   "candidate."
                << std::endl;
      error_handler = onSignalingErrorHandler_;
    }
  }

  if (error_handler) {
    error_handler("Signaling client not available to send candidate");
  }
}

void WebrtcManagerImpl::handlePeerConnectionStateChange(
    const std::string& peer_id, PeerConnectionState state) {
  OnPeerConnectedHandler peer_connected_handler;
  OnPeerDisconnectedHandler peer_disconnected_handler;
  std::string disconnect_reason;
  bool should_notify_disconnected = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // TODO: Map int state to meaningful enum/string from libwebrtc
    std::cout << "WebrtcManagerImpl: PeerConnection state change for "
              << peer_id << ", state=" << static_cast<int>(state) << std::endl;

    if (!peerConnections_.count(peer_id)) {
      std::cout << "WebrtcManagerImpl: State change for non-existent peer "
                << peer_id << std::endl;
      return;
    }

    if (state == PeerConnectionState::Connected) {
      std::cout << "WebrtcManagerImpl: Peer " << peer_id << " connected!"
                << std::endl;
      peer_connected_handler = onPeerConnectedHandler_;

      // TODO: Start heartbeat for this peer if enabled
      // if (config_.heartbeat_interval_ms > 0) {
      //     lastHeartbeatRxTime_[peer_id] = std::chrono::steady_clock::now();
      //     // Ensure timer is running and checks include this peer
      // }
    } else if (state == PeerConnectionState::Disconnected ||
               state == PeerConnectionState::Failed ||
               state == PeerConnectionState::Closed) {
      std::string reason =
          "PC State: " + std::to_string(static_cast<int>(state));
      if (state == PeerConnectionState::Disconnected)
        reason = "PC State: Disconnected";
      else if (state == PeerConnectionState::Failed)
        reason = "PC State: Failed";
      else if (state == PeerConnectionState::Closed)
        reason = "PC State: Closed";

      std::cout << "WebrtcManagerImpl: Peer " << peer_id
                << " disconnected/failed/closed. Reason: " << reason
                << std::endl;
      destroyPeerConnection(peer_id, reason);
      peer_disconnected_handler = onPeerDisconnectedHandler_;
      disconnect_reason = reason;
      should_notify_disconnected = true;
    }
  }

  if (peer_connected_handler) {
    peer_connected_handler(peer_id);
  }
  if (should_notify_disconnected && peer_disconnected_handler) {
    peer_disconnected_handler(peer_id, disconnect_reason);
  }
}

void WebrtcManagerImpl::handlePeerIceConnectionStateChange(
    const std::string& peer_id, IceConnectionState state) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  // TODO: Map int state to meaningful enum/string from libwebrtc
  std::cout << "WebrtcManagerImpl: Peer ICE Connection state change for "
            << peer_id << ", state=" << static_cast<int>(state) << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: ICE state change for non-existent peer "
              << peer_id << std::endl;
    return;
  }

  if (state == IceConnectionState::Connected ||
      state == IceConnectionState::Completed) {  // Use enums
    std::cout << "WebrtcManagerImpl: ICE Connected/Completed for " << peer_id
              << std::endl;
    // This often indicates actual connectivity status better than the overall
    // PC state. Maybe trigger onPeerConnectedHandler_ here if not already done
    // by PC state or update ConnectionMonitor status.
  } else if (state == IceConnectionState::Failed ||
             state == IceConnectionState::Disconnected ||
             state == IceConnectionState::Closed) {  // Use enums
    std::cout << "WebrtcManagerImpl: ICE Failed/Disconnected/Closed for "
              << peer_id << std::endl;
    // This might also indicate disconnection, let the PC StateChange handler
    // handle the main cleanup. But might trigger specific ConnectionMonitor
    // handlers.
  }
}

void WebrtcManagerImpl::handlePeerSignalingStateChange(
    const std::string& peer_id, int state) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  // TODO: Map int state to meaningful enum/string from libwebrtc
  std::cout << "WebrtcManagerImpl: Peer Signaling state change for " << peer_id
            << ", state=" << state << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout
        << "WebrtcManagerImpl: Signaling state change for non-existent peer "
        << peer_id << std::endl;
    return;
  }
}

void WebrtcManagerImpl::handlePeerDataChannelOpened(const std::string& peer_id,
                                                    const std::string& label) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: DataChannel opened for " << peer_id
            << ", label=" << label << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: DataChannel opened for non-existent peer "
              << peer_id << std::endl;
    return;
  }

  // TODO: Check label against config (control_channel_label,
  // telemetry_channel_label) Could keep track of opened channels per peer here
  // if needed for sendDataChannelMessage logic. Notify application if
  // DataChannel readiness is important for sending/receiving specific data.
  // Example: TelemetryHandler might need to know telemetry channel is open
  // before sending updates.
}

void WebrtcManagerImpl::handlePeerDataChannelClosed(const std::string& peer_id,
                                                    const std::string& label) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: DataChannel closed for " << peer_id
            << ", label=" << label << std::endl;

  // Check if the peer connection still exists
  if (!peerConnections_.count(peer_id)) {
    std::cout << "WebrtcManagerImpl: DataChannel closed for non-existent peer "
              << peer_id << std::endl;
    return;
  }
  // TODO: Update internal state if tracking DataChannel status.
}

void WebrtcManagerImpl::handlePeerDataChannelMessage(
    const std::string& peer_id, const std::string& label,
    const DataChannelMessage& message) {
  // Called by WebRTC signaling thread. ACQUIRE mutex_.
  OnDataChannelMessageReceivedHandler handler;
  bool should_invoke = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // std::cout << "WebrtcManagerImpl: DataChannel message received for " <<
    // peer_id << ", label=" << label << ", size=" << message.size() <<
    // std::endl;

    if (!peerConnections_.count(peer_id)) {
      std::cout
          << "WebrtcManagerImpl: DataChannel message for non-existent peer "
          << peer_id << std::endl;
      return;
    }

    if (label == "control") {
      handler = onDataChannelMessageReceivedHandler_;
      should_invoke = true;
    } else if (label == "telemetry") {
      handler = onDataChannelMessageReceivedHandler_;
      should_invoke = true;
    } else {
      std::cout << "WebrtcManagerImpl: Received message on unknown DataChannel "
                   "label: "
                << label << " from " << peer_id << std::endl;
    }
  }

  if (should_invoke && handler) {
    handler(peer_id, label, message);
  }
}

void WebrtcManagerImpl::handlePeerError(const std::string& peer_id,
                                        const std::string& error_msg) {
  OnPeerErrorHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "WebrtcManagerImpl: PeerConnection error for " << peer_id
              << ": " << error_msg << std::endl;

    if (!peerConnections_.count(peer_id)) {
      std::cout << "WebrtcManagerImpl: Error for non-existent peer " << peer_id
                << std::endl;
      return;
    }

    handler = onPeerErrorHandler_;
  }

  if (handler) {
    handler(peer_id, error_msg);
  }
  // Error might mean the connection is going down, StateChange handler should
  // catch closure and perform cleanup.
}

// --- Heartbeat and Reconnection Logic ---
// These methods need synchronization. onHeartbeatTimer is called by the timer
// thread.

// TODO: Implement timer integration with event_loop_
void WebrtcManagerImpl::startHeartbeatTimer() {
  // Acquire mutex briefly if accessing shared timer object or config
  // Timer mechanism itself might need to be created/configured outside the
  // mutex.
  std::lock_guard<std::mutex> lock(
      mutex_);  // Protect timer object if it's a member

  // if (config_.heartbeat_interval_ms > 0 && !heartbeatTimer_) {
  //     heartbeatTimer_ = event_loop_->createTimer(...); // Example using event
  //     loop timer heartbeatTimer_->start(config_.heartbeat_interval_ms, true,
  //     [this](){ onHeartbeatTimer(); });
  //      std::cout << "WebrtcManagerImpl: Heartbeat timer started." <<
  //      std::endl;
  // }
}

// TODO: Implement timer integration with event_loop_
void WebrtcManagerImpl::stopHeartbeatTimer() {
  std::lock_guard<std::mutex> lock(mutex_);  // Protect timer object

  // if (heartbeatTimer_) {
  //     heartbeatTimer_->stop();
  //     heartbeatTimer_.reset();
  //      std::cout << "WebrtcManagerImpl: Heartbeat timer stopped." <<
  //      std::endl;
  // }
}

// Called by the timer thread when it fires. ACQUIRE mutex_.
void WebrtcManagerImpl::onHeartbeatTimer() {
  std::lock_guard<std::mutex> lock(mutex_);
  // std::cout << "WebrtcManagerImpl: Heartbeat timer fired." << std::endl;

  checkForHeartbeatLoss();  // Check for lost heartbeats

  // Send heartbeats to connected peers
  for (auto const& [peer_id, pc] : peerConnections_) {
    if (pc &&
        pc->GetConnectionState() ==
            PeerConnectionState::Connected) {  // Only send to connected peers
      // Option 1: Send heartbeat via signaling (Simpler if signaling supports
      // it) heartbeat_msg.to = peer_id; if (signalingClient_)
      // signalingClient_->sendSignal(heartbeat_msg);

      // Option 2: Send heartbeat via DataChannel (More common for peer-to-peer
      // checks) Needs a dedicated heartbeat DataChannel
      const std::string heartbeat_channel_label =
          "heartbeat";  // Define a heartbeat channel label
      DataChannelMessage ping_data = {'p', 'i', 'n', 'g'};  // Example raw data
      // Ensure heartbeat channel is open before sending
      // pc->IsDataChannelOpen(heartbeat_channel_label) // Check channel state
      // if available in PC interface
      pc->SendData(heartbeat_channel_label,
                   ping_data);  // Send via PC DataChannel
    }
  }
}

// Checks for lost heartbeats. Called by onHeartbeatTimer(). ACQUIRE mutex_.
void WebrtcManagerImpl::checkForHeartbeatLoss() {
  // Lock is assumed to be held by onHeartbeatTimer.
  // std::lock_guard<std::mutex> lock(mutex_); // If called elsewhere, acquire
  // here

  if (config_.heartbeat_interval_ms <= 0) return;  // Heartbeat disabled

  auto now = std::chrono::steady_clock::now();
  // Define a threshold, e.g., 2 or 3 times the interval
  auto timeout = std::chrono::milliseconds(config_.heartbeat_interval_ms * 3);

  std::vector<std::string> peers_to_disconnect;

  for (auto const& [peer_id, last_rx_time] : lastHeartbeatRxTime_) {
    if (peerConnections_.count(peer_id) && peerConnections_[peer_id] &&
        peerConnections_[peer_id]->GetConnectionState() ==
            PeerConnectionState::Connected) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          now - last_rx_time);
      if (elapsed > timeout) {
        std::cerr << "WebrtcManagerImpl: Heartbeat lost from peer " << peer_id
                  << std::endl;
        peers_to_disconnect.push_back(peer_id);
        // TODO: Maybe increment a heartbeat loss counter before disconnecting
        // attemptReconnection(peer_id); // Or trigger reconnection attempt here
      }
    }
  }

  // Disconnect peers outside the iteration loop to avoid invalidating iterators
  for (const auto& peer_id : peers_to_disconnect) {
    // Calling destroyPeerConnection here will acquire the mutex again or rely
    // on current lock.
    destroyPeerConnection(peer_id,
                          "Heartbeat lost");  // This method ACQUIRES mutex_ or
                                              // relies on current lock.
  }
}

// Updates lastHeartbeatRxTime_. Called by handleSignalingMessage or
// handlePeerDataChannelMessage. ACQUIRE mutex_.
void WebrtcManagerImpl::handleReceivedHeartbeat(const std::string& peer_id) {
  // Lock is assumed to be held by the caller handler.
  // std::lock_guard<std::mutex> lock(mutex_); // If called elsewhere, acquire
  // here

  if (config_.heartbeat_interval_ms > 0) {
    lastHeartbeatRxTime_[peer_id] = std::chrono::steady_clock::now();
    // std::cout << "WebrtcManagerImpl: Updated heartbeat for " << peer_id <<
    // std::endl;
  }
}

// Tries to reconnect to a peer. ACQUIRE mutex_.
void WebrtcManagerImpl::attemptReconnection(const std::string& peer_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "WebrtcManagerImpl: Attempting reconnection for peer " << peer_id
            << std::endl;

  // TODO: Implement reconnection logic
  // Increment attempt counter
  // If attempts < max_attempts:
  //    Try reconnecting signaling?
  //    Try creating a new PeerConnection and sending offer/answer?
  // else:
  //    Report permanent failure
}

// --- Helper to safely invoke application callbacks ---
// These helpers acquire the mutex briefly to read the handler function,
// then release the mutex before invoking the handler.
// This prevents application callbacks from blocking the manager's threads while
// holding the mutex. If callbacks need to be marshalled to a specific
// application thread, that logic goes here. For now, callbacks are called
// directly from the manager/webrtc/signaling threads.

void WebrtcManagerImpl::invokeSignalingConnectedCallback() {
  OnSignalingConnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);  // Safely get the handler
    handler = onSignalingConnectedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler();  // Invoke the application's handler
  }
}

void WebrtcManagerImpl::invokeSignalingDisconnectedCallback(
    const std::string& reason) {
  OnSignalingDisconnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onSignalingDisconnectedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(reason);
  }
}

void WebrtcManagerImpl::invokeSignalingErrorCallback(
    const std::string& error_msg) {
  OnSignalingErrorHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onSignalingErrorHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(error_msg);
  }
}

void WebrtcManagerImpl::invokePeerConnectedCallback(
    const std::string& peer_id) {
  OnPeerConnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onPeerConnectedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(peer_id);
  }
}

void WebrtcManagerImpl::invokePeerDisconnectedCallback(
    const std::string& peer_id, const std::string& reason) {
  OnPeerDisconnectedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onPeerDisconnectedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(peer_id, reason);
  }
}

void WebrtcManagerImpl::invokePeerErrorCallback(const std::string& peer_id,
                                                const std::string& error_msg) {
  OnPeerErrorHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onPeerErrorHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    handler(peer_id, error_msg);
  }
}

void WebrtcManagerImpl::invokeDataChannelMessageReceivedCallback(
    const std::string& peer_id, const std::string& label,
    const DataChannelMessage& message) {
  OnDataChannelMessageReceivedHandler handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    handler = onDataChannelMessageReceivedHandler_;
  }
  if (handler) {
    // TODO: Marshal to application thread if needed
    // If the application's handler does significant work or interacts with UI
    // (Cockpit), it MUST be marshalled to the application's event loop thread.
    handler(peer_id, label, message);
  }
}

// Optional: void WebrtcManagerImpl::invokeVideoTrackReceivedCallback(...) { ...
// }

// --- Other Internal Logic ---
// ... Heartbeat and Reconnection implementation details ...

}  // namespace webrtc
}  // namespace remote
}  // namespace autodev
