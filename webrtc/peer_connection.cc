#include "webrtc/i_peer_connection.h"  // Should be included by the impl header
#include "webrtc/libwebrtc_peer_connection_impl.h"

// Include libwebrtc headers (requires setting up libwebrtc build)
// Example headers - actual headers vary by libwebrtc version and build config
// #include "api/peer_connection_interface.h"
// #include "api/data_channel_interface.h"
// #include "api/create_session_description_observer.h"
// #include "api/set_session_description_observer.h"
// #include "api/scoped_refptr.h"
// #include "rtc_base/thread.h"
// #include "rtc_base/ref_counted_object.h" // For observer implementation

#include <iostream>
#include <mutex>    // For synchronization
#include <string>   // For std::string
#include <utility>  // For std::move
#include <vector>   // For std::vector

// Forward declarations for libwebrtc types (if not including headers fully)
// namespace webrtc {
//     class PeerConnectionInterface;
//     class DataChannelInterface;
//     class PeerConnectionFactoryInterface;
//     // ... other types (observers, buffers etc.)
// }
// namespace rtc {
//     class Thread; // Signaling thread
//     template <class T> class scoped_refptr; // Reference counting
// }

namespace autodev {
namespace remote {
namespace webrtc {

// Define concrete PeerConnection implementation inheriting from IPeerConnection
// This class will own the actual libwebrtc PeerConnectionInterface instance.
class LibwebrtcPeerConnectionImpl
    : public IPeerConnection,
      public rtc::RefCountedObject<  // Inherit from RefCountedObject if this
                                     // object is also a libwebrtc observer
          webrtc::PeerConnectionObserver,
          webrtc::CreateSessionDescriptionObserver,
          webrtc::SetSessionDescriptionObserver
          // ... other libwebrtc observer interfaces
          > {
 public:
  // Constructor is lightweight. Initialization in init().
  LibwebrtcPeerConnectionImpl();

  // Destructor. MUST ensure libwebrtc PC is closed and released on the correct
  // thread.
  ~LibwebrtcPeerConnectionImpl() override;

  // --- Implementation of IPeerConnection ---

  // TODO: Implement init with dependencies
  // virtual bool init(const RtcConfiguration& config, const
  // PeerConnectionCallbacks& callbacks,
  //                   rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
  //                   factory, rtc::Thread* signaling_thread, rtc::Thread*
  //                   app_thread) override;
  virtual bool init(/* Dependencies */) override;  // Placeholder

  // Implement SetCallbacks. This method MUST BE THREAD-SAFE.
  void SetCallbacks(const PeerConnectionCallbacks& callbacks) override;

  // Implement CreateOffer. Must marshal call to libwebrtc signaling thread.
  bool CreateOffer() override;

  // Implement CreateAnswer. Must marshal call to libwebrtc signaling thread.
  bool CreateAnswer() override;

  // Implement SetRemoteDescription. Must marshal call to libwebrtc signaling
  // thread.
  bool SetRemoteDescription(const std::string& sdp_type,
                            const std::string& sdp_string) override;

  // Implement AddRemoteCandidate. Must marshal call to libwebrtc signaling
  // thread.
  bool AddRemoteCandidate(const std::string& candidate,
                          const std::string& sdp_mid,
                          int sdp_mline_index) override;

  // Implement SendData. This method MUST BE THREAD-SAFE.
  // Must marshal call to the DataChannel thread (usually signaling thread or
  // internal DC thread).
  bool SendData(const std::string& label,
                const DataChannelMessage& data) override;
  bool SendData(const std::string& label, const std::string& data) override;

  // Implement AddLocalTrack (optional). Must marshal call to libwebrtc
  // signaling thread. bool AddLocalTrack(std::shared_ptr<IMediaTrack> track)
  // override; // Example

  // Implement Close. This method MUST BE THREAD-SAFE.
  // Must marshal call to libwebrtc signaling thread.
  void Close() override;

  // Implement State Getters. These might need to be thread-safe depending on
  // how libwebrtc exposes state.
  PeerConnectionState GetConnectionState() const override;
  IceConnectionState GetIceConnectionState() const override;
  SignalingState GetSignalingState() const override;

  // --- Implementation of libwebrtc PeerConnectionObserver ---
  // These methods are called by libwebrtc on the signaling thread.
  // They must translate libwebrtc events to calls to our stored
  // PeerConnectionCallbacks.

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
      override;  // Deprecated
  void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
      override;  // Deprecated
  void OnAddTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
          streams) override;  // For Unified Plan
  void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
      override;  // For Unified Plan
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
      override;  // Called when remote side creates a DataChannel
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnStandardizedIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state)
      override;  // Standardized version
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state)
      override;  // Standardized PC state change
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) override;
  void OnIceCandidatesGatheringDone() override;
  void OnIceCandidateError(const std::string& address, int port,
                           const std::string& url, int error_code,
                           const std::string& error_text) override;
  void OnValidationRemoteCandidateFailed(
      const cricket::Candidate& candidate) override;
  void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>&
                            report) override;  // For getStats
  void OnAudioOrVideoTrack(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
      override;  // Alternative media callback

  // --- Implementation of libwebrtc CreateSessionDescriptionObserver ---
  // Called by libwebrtc on the signaling thread after CreateOffer/Answer
  // completes.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;

  // --- Implementation of libwebrtc SetSessionDescriptionObserver ---
  // Called by libwebrtc on the signaling thread after
  // SetLocal/RemoteDescription completes.
  void OnSetSessionDescriptionComplete(
      webrtc::RTCError error) override;  // Modern API
  // void OnSetSessionDescriptionSuccess() override; // Older API
  // void OnSetSessionDescriptionFailure(const std::string& error) override; //
  // Older API

  // --- Implementation of libwebrtc DataChannelObserver (for each data channel)
  // --- Need a way to store these observers or adapt them. This could be a
  // separate class or internal lambda per channel. Example methods a
  // DataChannelObserver would implement: void OnStateChange() override; //
  // DataChannel state (connecting, open, closed) void OnMessage(const
  // webrtc::DataBuffer& buffer) override; // Message received void
  // OnBufferedAmountChange(uint64_t sent_data_bytes) override; // Sent bytes
  // buffered

 private:
  // Mutex to protect access to members that can be accessed from different
  // threads (App thread calling methods, libwebrtc threads calling observers)
  mutable std::mutex mutex_;

  // Actual libwebrtc PeerConnection instance. MUST be accessed and destroyed on
  // the signaling thread.
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> rtc_peer_connection_
      GUARDED_BY(mutex_);

  // Dependencies received during init
  // rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_
  // GUARDED_BY(mutex_); rtc::Thread* signaling_thread_ GUARDED_BY(mutex_); //
  // Libwebrtc signaling thread rtc::Thread* app_thread_ GUARDED_BY(mutex_); //
  // Application's main thread or event loop thread (for marshalling callbacks)
  // EventLoopContext* app_event_loop_ GUARDED_BY(mutex_); // Alternative to
  // app_thread_

  // Store the application's callbacks
  PeerConnectionCallbacks callbacks_ GUARDED_BY(mutex_);

  // Local SDP is only emitted after SetLocalDescription succeeds.
  bool awaiting_local_set_description_ GUARDED_BY(mutex_) = false;
  std::string pending_local_sdp_type_ GUARDED_BY(mutex_);
  std::string pending_local_sdp_string_ GUARDED_BY(mutex_);

  // Map to store DataChannel instances, potentially keyed by label
  // std::map<std::string, rtc::scoped_refptr<webrtc::DataChannelInterface>>
  // data_channels_ GUARDED_BY(mutex_); Need to manage DataChannelObservers
  // associated with these.

  // Helper to marshal a task to the signaling thread
  // bool PostTaskToSignalingThread(std::function<void()> task);

  // Helper to marshal a task to the application thread
  // bool PostTaskToAppThread(std::function<void()> task);

  // Helper to safely invoke application callbacks, potentially marshalling
  // void InvokeCallback(std::function<void()> app_callback);
  // void InvokeCallbackWithArg(std::function<void(ArgType)> app_callback,
  // ArgType arg);

  // Prevent copying and assignment (already in IPeerConnection, repeat here)
  LibwebrtcPeerConnectionImpl(const LibwebrtcPeerConnectionImpl&) = delete;
  LibwebrtcPeerConnectionImpl& operator=(const LibwebrtcPeerConnectionImpl&) =
      delete;
};

// --- LibwebrtcPeerConnectionImpl Method Implementations ---

LibwebrtcPeerConnectionImpl::LibwebrtcPeerConnectionImpl() {
  // libwebrtc PC instance is not created here, but in init()
  std::cout << "LibwebrtcPeerConnectionImpl created." << std::endl;
}

LibwebrtcPeerConnectionImpl::~LibwebrtcPeerConnectionImpl() {
  std::cout << "LibwebrtcPeerConnectionImpl destroying." << std::endl;
  // The underlying rtc_peer_connection_ MUST be closed and released
  // on the correct thread (signaling thread) during the Close() call,
  // before the unique_ptr holding THIS object is destroyed.
  // Ensure Close() is called and completes before destructor finishes if
  // this object's lifetime is tied to the PeerConnectionInterface.
  // A common pattern is to call Close() and then rely on the last
  // reference to the underlying PC being released on the signaling thread
  // to trigger its actual destruction.
}

// Implementation of IPeerConnection::init
bool LibwebrtcPeerConnectionImpl::init(/* const RtcConfiguration& config, rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory, rtc::Thread* signaling_thread, rtc::Thread* app_thread */) {
  // This method is called by the WebrtcManager's thread (likely App thread).
  // Acquire mutex before accessing members.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl::init called." << std::endl;

  // TODO: Store dependencies:
  // pc_factory_ = factory;
  // signaling_thread_ = signaling_thread;
  // app_thread_ = app_thread;
  // Store config and apply to rtc_config for PC creation

  // TODO: Use the factory to create the actual libwebrtc
  // PeerConnectionInterface instance This creation must happen on the signaling
  // thread. Option 1: Post task to signaling thread from init (more complex
  // init) Option 2: Rely on PeerConnectionFactory::CreatePeerConnection being
  // callable from current thread and let it handle internal marshalling (check
  // libwebrtc API) Assuming Option 2 for this skeleton structure:
  // webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  // // Populate rtc_config from provided config (ICE servers etc.)
  // rtc_config.ice_servers.push_back(...); // Populate ICE servers from config

  // Create a PeerConnectionObserver adapter if this class doesn't inherit
  // directly or ensure this class inherits from PeerConnectionObserver as shown
  // above. Pass 'this' as the observer to the libwebrtc factory method.

  // rtc_peer_connection_ = pc_factory_->CreatePeerConnection(
  //     rtc_config, nullptr, nullptr, this); // Pass 'this' as
  //     PeerConnectionObserver

  // Dummy creation for skeleton:
  rtc_peer_connection_ = nullptr;  // Simulate creation failure for now
  // rtc_peer_connection_ = new
  // rtc::RefCountedObject<DummyWebrtcPeerConnection>(); // Simulate successful
  // creation

  if (!rtc_peer_connection_) {
    std::cerr << "LibwebrtcPeerConnectionImpl: Failed to create underlying "
                 "libwebrtc PeerConnection."
              << std::endl;
    // TODO: Report error via callbacks? Need callbacks set before init? Or
    // return status.
    return false;
  }

  // TODO: Create DataChannels if needed (e.g., if auto-created by config)
  // rtc::DataChannelInit data_channel_config;
  // rtc::scoped_refptr<webrtc::DataChannelInterface> control_channel =
  // rtc_peer_connection_->CreateDataChannel("control", &data_channel_config);
  // Store data channels and their observers.

  std::cout << "LibwebrtcPeerConnectionImpl: Underlying PC created."
            << std::endl;
  return true;
}

// Implementation of IPeerConnection::SetCallbacks
void LibwebrtcPeerConnectionImpl::SetCallbacks(
    const PeerConnectionCallbacks& callbacks) {
  // This method is called by the WebrtcManager's thread. Acquire mutex.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl::SetCallbacks called." << std::endl;
  callbacks_ = callbacks;  // Store the provided callbacks
}

// Implementation of IPeerConnection::CreateOffer
bool LibwebrtcPeerConnectionImpl::CreateOffer() {
  // This method is called by the WebrtcManager's thread. Acquire mutex.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl::CreateOffer called." << std::endl;

  if (!rtc_peer_connection_) {
    std::cerr << "LibwebrtcPeerConnectionImpl: Cannot create offer, underlying "
                 "PC not initialized."
              << std::endl;
    // TODO: Report error via callbacks_->onError? Need to marshal to app
    // thread.
    return false;
  }

  // Create a CreateSessionDescriptionObserver adapter if this class doesn't
  // inherit directly Pass 'this' as the observer to the libwebrtc API call. The
  // OnSuccess/OnFailure methods of the observer will be called by libwebrtc on
  // the signaling thread. rtc_peer_connection_->CreateOffer(this,
  // webrtc::PeerConnectionInterface::RTCOfferAnswerOptions()); // Pass 'this'
  // as observer

  // Dummy success for skeleton
  std::cout << "LibwebrtcPeerConnectionImpl: Simulating CreateOffer success."
            << std::endl;
  // Simulate OnSuccess callback (which would happen on signaling thread)
  // For real code, this task must be posted to the signaling thread if called
  // from app thread. PostTaskToSignalingThread([this]() { OnSuccess(nullptr);
  // }); // Conceptual marshalling

  return true;  // Indicate successfully initiating the process
}

// Implementation of IPeerConnection::CreateAnswer
bool LibwebrtcPeerConnectionImpl::CreateAnswer() {
  // This method is called by the WebrtcManager's thread. Acquire mutex.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl::CreateAnswer called." << std::endl;

  if (!rtc_peer_connection_) {
    std::cerr << "LibwebrtcPeerConnectionImpl: Cannot create answer, "
                 "underlying PC not initialized."
              << std::endl;
    return false;
  }

  // Create a CreateSessionDescriptionObserver adapter if needed. Pass 'this' as
  // observer. rtc_peer_connection_->CreateAnswer(this,
  // webrtc::PeerConnectionInterface::RTCOfferAnswerOptions()); // Pass 'this'
  // as observer

  // Dummy success for skeleton
  std::cout << "LibwebrtcPeerConnectionImpl: Simulating CreateAnswer success."
            << std::endl;
  // Simulate OnSuccess callback (which would happen on signaling thread)
  // For real code, this task must be posted to the signaling thread if called
  // from app thread. PostTaskToSignalingThread([this]() { OnSuccess(nullptr);
  // }); // Conceptual marshalling

  return true;  // Indicate successfully initiating the process
}

// Implementation of IPeerConnection::SetRemoteDescription
bool LibwebrtcPeerConnectionImpl::SetRemoteDescription(
    const std::string& sdp_type, const std::string& sdp_string) {
  // This method is called by the WebrtcManager's thread. Acquire mutex.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl::SetRemoteDescription called."
            << std::endl;

  if (!rtc_peer_connection_) {
    std::cerr << "LibwebrtcPeerConnectionImpl: Cannot set remote description, "
                 "underlying PC not initialized."
              << std::endl;
    return false;
  }

  // Create SessionDescriptionInterface from string
  // std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
  // webrtc::CreateSessionDescription(sdp_type, sdp_string, nullptr); // Last
  // arg is error

  // if (!session_description) {
  //     std::cerr << "LibwebrtcPeerConnectionImpl: Failed to parse remote SDP."
  //     << std::endl;
  //     // TODO: Report error via callbacks_->onError? Need to marshal to app
  //     thread. return false;
  // }

  // Create a SetSessionDescriptionObserver adapter if needed. Pass 'this' as
  // observer.
  // rtc_peer_connection_->SetRemoteDescription(std::move(session_description),
  // this); // Pass observer

  // Dummy success for skeleton
  std::cout
      << "LibwebrtcPeerConnectionImpl: Simulating SetRemoteDescription success."
      << std::endl;
  // Simulate OnSetSessionDescriptionComplete callback (which would happen on
  // signaling thread) For real code, this task must be posted to the signaling
  // thread if called from app thread. PostTaskToSignalingThread([this]() {
  // OnSetSessionDescriptionComplete(webrtc::RTCError::OK()); }); // Conceptual
  // marshalling

  return true;  // Indicate successfully initiating the process
}

// Implementation of IPeerConnection::AddRemoteCandidate
bool LibwebrtcPeerConnectionImpl::AddRemoteCandidate(
    const std::string& candidate, const std::string& sdp_mid,
    int sdp_mline_index) {
  // This method is called by the WebrtcManager's thread. Acquire mutex.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl::AddRemoteCandidate called."
            << std::endl;

  if (!rtc_peer_connection_) {
    std::cerr << "LibwebrtcPeerConnectionImpl: Cannot add remote candidate, "
                 "underlying PC not initialized."
              << std::endl;
    return false;
  }

  // Create IceCandidateInterface from string
  // std::unique_ptr<webrtc::IceCandidateInterface> ice_candidate =
  // webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, candidate, nullptr);
  // // Last arg is error

  // if (!ice_candidate) {
  //     std::cerr << "LibwebrtcPeerConnectionImpl: Failed to parse remote
  //     candidate." << std::endl;
  //     // TODO: Report error via callbacks_->onError? Need to marshal to app
  //     thread. return false;
  // }

  // Add the candidate to the PeerConnection
  // rtc_peer_connection_->AddIceCandidate(ice_candidate.get()); // Check return
  // value

  // Dummy success for skeleton
  std::cout
      << "LibwebrtcPeerConnectionImpl: Simulating AddRemoteCandidate success."
      << std::endl;

  return true;  // Indicate successfully adding the candidate
}

// Implementation of IPeerConnection::SendData
// This method MUST BE THREAD-SAFE.
bool LibwebrtcPeerConnectionImpl::SendData(const std::string& label,
                                           const DataChannelMessage& data) {
  // This method is called by application threads (e.g., Command/Telemetry
  // Handlers via WebrtcManager). Acquire mutex to safely access the data
  // channel map/pointers and the underlying PC.
  std::lock_guard<std::mutex> lock(mutex_);
  // std::cout << "LibwebrtcPeerConnectionImpl::SendData called for label=" <<
  // label << ", size=" << data.size() << std::endl; // Avoid spamming logs

  if (!rtc_peer_connection_) {
    // std::cerr << "LibwebrtcPeerConnectionImpl: Cannot send data, underlying
    // PC not initialized." << std::endl;
    // TODO: Report error via callbacks_->onError? Need to marshal.
    return false;
  }

  // Find the DataChannel by label
  // rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel =
  // rtc_peer_connection_->LookupDataChannelByLabel(label);

  // If DataChannel is found and its state is OPEN
  // if (data_channel && data_channel->state() ==
  // webrtc::DataChannelInterface::kOpen) { Create a libwebrtc DataBuffer
  // webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(data.data(), data.size()),
  // false); // boolean is binary flag

  // Send the data via the DataChannel
  // bool sent = data_channel->Send(buffer); // Check return value

  // return sent; // Return send status
  // } else {
  // DataChannel not found or not open
  // std::cerr << "LibwebrtcPeerConnectionImpl: DataChannel '" << label << "'
  // not found or not open." << std::endl;
  // TODO: Report error via callbacks_->onError? Need to marshal.
  return false;  // Indicate send failure
                 // }
                 // Dummy success for skeleton
  // std::cout << "LibwebrtcPeerConnectionImpl: Simulating SendData success for
  // label=" << label << std::endl; return true;
}

// Implementation of IPeerConnection::SendData (string overload)
bool LibwebrtcPeerConnectionImpl::SendData(const std::string& label,
                                           const std::string& data) {
  // Convert string to vector<char> and call the vector overload.
  DataChannelMessage binary_data(data.begin(), data.end());
  return SendData(label, binary_data);
}

// Implementation of IPeerConnection::AddLocalTrack (optional)
// bool LibwebrtcPeerConnectionImpl::AddLocalTrack(std::shared_ptr<IMediaTrack>
// track) {
//     // This method is called by the WebrtcManager's thread. Acquire mutex.
//     std::lock_guard<std::mutex> lock(mutex_);
//     std::cout << "LibwebrtcPeerConnectionImpl::AddLocalTrack called." <<
//     std::endl;
//
//     if (!rtc_peer_connection_) {
//         std::cerr << "LibwebrtcPeerConnectionImpl: Cannot add track,
//         underlying PC not initialized." << std::endl; return false;
//     }
//
//     // TODO: Get the underlying rtc::MediaStreamTrackInterface from the
//     IMediaTrack
//     // This depends on the definition of IMediaTrack and how it wraps
//     libwebrtc objects.
//     // auto rtc_track = dynamic_cast<const
//     ConcreteMediaTrackImpl*>(track.get())->rtc_track(); // Example type
//     casting
//
//     // if (!rtc_track) {
//     //     std::cerr << "LibwebrtcPeerConnectionImpl: Invalid media track
//     provided." << std::endl;
//     //     return false;
//     // }
//
//     // Add the track to the PeerConnection
//     // rtc::scoped_refptr<webrtc::SenderInterface> sender =
//     rtc_peer_connection_->AddTrack(rtc_track, {"stream_id"}); // Replace
//     "stream_id" with a meaningful ID
//
//     // if (!sender) {
//     //      std::cerr << "LibwebrtcPeerConnectionImpl: Failed to add track to
//     PeerConnection." << std::endl;
//     //      return false;
//     // }
//
//     // TODO: Store the sender or track info if needed for removeLocalTrack or
//     state tracking
//
//     std::cout << "LibwebrtcPeerConnectionImpl: Simulating AddLocalTrack
//     success." << std::endl; return true;
// }

// Implementation of IPeerConnection::Close
// This method MUST BE THREAD-SAFE.
void LibwebrtcPeerConnectionImpl::Close() {
  // This method is called by the WebrtcManager's thread (e.g., during stop or
  // disconnect). Acquire mutex to safely access the underlying PC pointer.
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl::Close called." << std::endl;

  if (rtc_peer_connection_) {
    // Call Close on the underlying libwebrtc PeerConnection instance.
    // This is an asynchronous operation. It triggers state changes (connecting
    // -> closed). The actual destruction of the libwebrtc PC object happens
    // later on the signaling thread.
    rtc_peer_connection_->Close();
    std::cout << "LibwebrtcPeerConnectionImpl: Called Close() on underlying PC."
              << std::endl;

    // It's crucial that this LibwebrtcPeerConnectionImpl object (which acts as
    // observer) is not destroyed BEFORE the libwebrtc PC object is fully shut
    // down and its observers are detached. The lifetime management is tricky
    // here. If LibwebrtcPeerConnectionImpl owns the rtc_peer_connection_ via
    // scoped_refptr, releasing the scoped_refptr will initiate destruction on
    // the signaling thread. rtc_peer_connection_ = nullptr; // Release the
    // reference (triggers destruction on signaling thread)

    // If using AddRef/Release for observer lifetime, need to ensure Release is
    // called at right time.
  } else {
    std::cout << "LibwebrtcPeerConnectionImpl: Underlying PC is already null "
                 "or not initialized."
              << std::endl;
  }
  // State will transition to Closed via OnConnectionChange callback
  // asynchronously.
}

// --- Implementation of State Getters ---
PeerConnectionState LibwebrtcPeerConnectionImpl::GetConnectionState() const {
  // This method is called by the WebrtcManager's thread. Acquire mutex.
  std::lock_guard<std::mutex> lock(mutex_);
  // TODO: Get state from rtc_peer_connection_ and map to PeerConnectionState
  // enum.
  if (!rtc_peer_connection_)
    return PeerConnectionState::Closed;  // Or Uninitialized/New depending on
                                         // convention
  // return
  // static_cast<PeerConnectionState>(rtc_peer_connection_->peer_connection_state());
  // // Example mapping (incorrect direct cast) Dummy return for skeleton:
  return PeerConnectionState::New;
}

IceConnectionState LibwebrtcPeerConnectionImpl::GetIceConnectionState() const {
  // This method is called by the WebrtcManager's thread. Acquire mutex.
  std::lock_guard<std::mutex> lock(mutex_);
  // TODO: Get state from rtc_peer_connection_ and map to IceConnectionState
  // enum.
  if (!rtc_peer_connection_) return IceConnectionState::Closed;
  // return
  // static_cast<IceConnectionState>(rtc_peer_connection_->ice_connection_state());
  // // Example mapping Dummy return for skeleton:
  return IceConnectionState::New;
}

SignalingState LibwebrtcPeerConnectionImpl::GetSignalingState() const {
  // This method is called by the WebrtcManager's thread. Acquire mutex.
  std::lock_guard<std::mutex> lock(mutex_);
  // TODO: Get state from rtc_peer_connection_ and map to SignalingState enum.
  if (!rtc_peer_connection_) return SignalingState::Closed;
  // return
  // static_cast<SignalingState>(rtc_peer_connection_->signaling_state()); //
  // Example mapping Dummy return for skeleton:
  return SignalingState::Stable;
}

// --- Implementation of libwebrtc PeerConnectionObserver ---
// These methods are called by libwebrtc on the signaling thread.
// They MUST acquire mutex_ before accessing shared state or calling application
// callbacks. Application callbacks should ideally be marshalled to the app
// thread.

void LibwebrtcPeerConnectionImpl::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnSignalingChange: " << new_state
            << std::endl;
  // TODO: Map new_state to SignalingState enum
  // TODO: Invoke callbacks_->onSignalingStateChange(mapped_state); // Need to
  // marshal
}
void LibwebrtcPeerConnectionImpl::OnAddStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) { /* Deprecated */
}
void LibwebrtcPeerConnectionImpl::OnRemoveStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) { /* Deprecated */
}
void LibwebrtcPeerConnectionImpl::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
        streams) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnAddTrack" << std::endl;
  // TODO: Get track from receiver (receiver->track())
  // TODO: Check track type (video/audio)
  // TODO: If it's a video track, and this is the Cockpit side,
  //      invoke callbacks_->onVideoTrackReceived(peer_id, video_track); // Need
  //      peer_id context, need to marshal
}
void LibwebrtcPeerConnectionImpl::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnRemoveTrack" << std::endl;
  // TODO: Notify application that a track was removed.
}
void LibwebrtcPeerConnectionImpl::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnDataChannel: "
            << data_channel->label() << std::endl;
  // Called when the remote side creates a DataChannel.
  // Store the data_channel and attach a DataChannelObserver to it.
  // Need to implement DataChannelObserver methods (OnStateChange, OnMessage,
  // etc.) data_channels_[data_channel->label()] = data_channel;
  // data_channel->RegisterObserver(new RtcDataChannelObserverAdapter(this)); //
  // Need adapter or this class implements observer

  // TODO: Notify application that a DataChannel was created/opened?
  // The onDataChannelOpened callback in PeerConnectionCallbacks might be better
  // triggered by the DataChannelObserver's OnStateChange when the state becomes
  // kOpen.
}
void LibwebrtcPeerConnectionImpl::OnRenegotiationNeeded() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnRenegotiationNeeded"
            << std::endl;
  // This indicates the need to create a new offer/answer cycle.
  // Typically handled by the WebRTC Manager or application logic.
  // Manager might call CreateOffer/Answer.
}
void LibwebrtcPeerConnectionImpl::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnIceConnectionChange: "
            << new_state << std::endl;
  // TODO: Map new_state to IceConnectionState enum
  // TODO: Invoke callbacks_->onIceConnectionStateChange(mapped_state); // Need
  // to marshal
}
void LibwebrtcPeerConnectionImpl::OnStandardizedIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout
      << "LibwebrtcPeerConnectionImpl: OnStandardizedIceConnectionChange: "
      << new_state << std::endl;
  // TODO: Map new_state to IceConnectionState enum
  // Use this one if available and preferred over OnIceConnectionChange.
  // TODO: Invoke callbacks_->onIceConnectionStateChange(mapped_state); // Need
  // to marshal
}
void LibwebrtcPeerConnectionImpl::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnConnectionChange: " << new_state
            << std::endl;
  // TODO: Map new_state to PeerConnectionState enum
  // This is the most important state change for application connectivity.
  // TODO: Invoke callbacks_->onConnectionStateChange(mapped_state); // Need to
  // marshal If state is Failed or Closed, might need to trigger cleanup in
  // WebrtcManager.
}
void LibwebrtcPeerConnectionImpl::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnIceGatheringChange: "
            << new_state << std::endl;
  // State of ICE candidate gathering (new, gathering, complete)
}
void LibwebrtcPeerConnectionImpl::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnIceCandidate" << std::endl;
  // A new local ICE candidate is generated. Serialize it to string.
  // std::string sdp;
  // candidate->ToString(&sdp);
  // TODO: Invoke callbacks_->onLocalCandidateGenerated(sdp,
  // candidate->sdp_mid(), candidate->sdp_mline_index()); // Need to marshal
}
void LibwebrtcPeerConnectionImpl::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnIceCandidatesRemoved"
            << std::endl;
  // Optional: Notify application that some candidates were removed.
}
void LibwebrtcPeerConnectionImpl::OnIceCandidatesGatheringDone() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnIceCandidatesGatheringDone"
            << std::endl;
  // Optional: Notify application that ICE gathering is complete.
}
void LibwebrtcPeerConnectionImpl::OnIceCandidateError(
    const std::string& address, int port, const std::string& url,
    int error_code, const std::string& error_text) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << "LibwebrtcPeerConnectionImpl: OnIceCandidateError: "
            << error_text << std::endl;
  // TODO: Invoke callbacks_->onError("ICE Candidate Error: " + error_text); //
  // Need to marshal
}
void LibwebrtcPeerConnectionImpl::OnValidationRemoteCandidateFailed(
    const cricket::Candidate& candidate) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << "LibwebrtcPeerConnectionImpl: OnValidationRemoteCandidateFailed"
            << std::endl;
  // TODO: Invoke callbacks_->onError("Remote Candidate Validation Failed"); //
  // Need to marshal
}
void LibwebrtcPeerConnectionImpl::OnStatsDelivered(
    const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnStatsDelivered" << std::endl;
  // Optional: Process and report stats to the application.
}
void LibwebrtcPeerConnectionImpl::OnAudioOrVideoTrack(
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: OnAudioOrVideoTrack" << std::endl;
  // Called when a remote media track is received (Cockpit side).
  // TODO: Check track type (video/audio). If video, and this is Cockpit,
  //      invoke callbacks_->onVideoTrackReceived(peer_id,
  //      dynamic_cast<webrtc::VideoTrackInterface*>(track.get())); // Need
  //      peer_id, need to marshal
}

// --- Implementation of libwebrtc CreateSessionDescriptionObserver ---
// Called by libwebrtc on the signaling thread after CreateOffer/Answer
// completes.

void LibwebrtcPeerConnectionImpl::OnSuccess(
    webrtc::SessionDescriptionInterface* desc) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << "LibwebrtcPeerConnectionImpl: CreateSdp OnSuccess" << std::endl;

  if (!desc) {
    std::cerr << "LibwebrtcPeerConnectionImpl: CreateSdp OnSuccess with null "
                 "description."
              << std::endl;
    return;
  }

  // Get SDP string
  std::string sdp_string;
  desc->ToString(&sdp_string);
  std::string sdp_type = desc->type();  // "offer" or "answer"

  pending_local_sdp_type_ = sdp_type;
  pending_local_sdp_string_ = sdp_string;
  awaiting_local_set_description_ = true;

  if (!rtc_peer_connection_) {
    std::cerr << "LibwebrtcPeerConnectionImpl: Cannot SetLocalDescription, "
                 "underlying PC not initialized."
              << std::endl;
    awaiting_local_set_description_ = false;
    pending_local_sdp_type_.clear();
    pending_local_sdp_string_.clear();
    return;
  }

  // Set local description (required after creating Offer/Answer)
  rtc_peer_connection_->SetLocalDescription(this, desc);

  // Note: Ownership of `desc` is transferred to SetLocalDescription.
}

void LibwebrtcPeerConnectionImpl::OnFailure(webrtc::RTCError error) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cerr << "LibwebrtcPeerConnectionImpl: CreateSdp OnFailure: "
            << error.message() << std::endl;
  // TODO: Invoke callbacks_->onError("Create SDP failed: " + error.message());
  // // Need to marshal
}

// --- Implementation of libwebrtc SetSessionDescriptionObserver ---
// Called by libwebrtc on the signaling thread after SetLocal/RemoteDescription
// completes.

void LibwebrtcPeerConnectionImpl::OnSetSessionDescriptionComplete(
    webrtc::RTCError error) {
  std::string local_sdp_type;
  std::string local_sdp_string;
  std::function<void(const std::string&, const std::string&)> local_sdp_cb;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "LibwebrtcPeerConnectionImpl: SetSdp OnComplete" << std::endl;
    if (!error.ok()) {
      std::cerr << "LibwebrtcPeerConnectionImpl: SetSdp failed: "
                << error.message() << std::endl;
      if (awaiting_local_set_description_) {
        awaiting_local_set_description_ = false;
        pending_local_sdp_type_.clear();
        pending_local_sdp_string_.clear();
      }
      return;
    }

    std::cout << "LibwebrtcPeerConnectionImpl: SetSdp success." << std::endl;

    if (awaiting_local_set_description_) {
      awaiting_local_set_description_ = false;
      local_sdp_type = pending_local_sdp_type_;
      local_sdp_string = pending_local_sdp_string_;
      pending_local_sdp_type_.clear();
      pending_local_sdp_string_.clear();
      local_sdp_cb = callbacks_.onLocalSdpGenerated;
    }
  }

  if (local_sdp_cb) {
    local_sdp_cb(local_sdp_type, local_sdp_string);
  }
}
// Implement older OnSetSessionDescriptionSuccess/Failure if needed based on
// libwebrtc version

// --- Implementation of libwebrtc DataChannelObserver ---
// This would typically be a separate class or implemented internally per data
// channel. Example methods: void
// LibwebrtcPeerConnectionImpl::OnDataChannelStateChange(webrtc::DataChannelInterface::DataState
// new_state) {
//      std::lock_guard<std::mutex> lock(mutex_);
//      std::cout << "LibwebrtcPeerConnectionImpl: DataChannel State Change: "
//      << new_state << std::endl;
//      // TODO: Map state and invoke callbacks_->onDataChannelOpened/Closed
//      based on state
//      // if (new_state == webrtc::DataChannelInterface::kOpen) {
//      callbacks_.onDataChannelOpened(label); } // Need label context
//      // if (new_state == webrtc::DataChannelInterface::kClosed) {
//      callbacks_.onDataChannelClosed(label); } // Need label context
//      // Need to marshal these calls.
// }
//
// void LibwebrtcPeerConnectionImpl::OnDataChannelMessage(const
// webrtc::DataBuffer& buffer) {
//     std::lock_guard<std::mutex> lock(mutex_);
//     std::cout << "LibwebrtcPeerConnectionImpl: DataChannel Message Received.
//     Binary: " << buffer.binary << ", size: " << buffer.data.size() <<
//     std::endl;
//     // TODO: Get label context for this data channel
//     // TODO: Copy data from buffer.data into a DataChannelMessage
//     (std::vector<char>)
//     // TODO: Invoke callbacks_->onDataChannelMessage(peer_id, label,
//     message_data); // Need peer_id and label context, need to marshal
// }

// --- Helper to marshal a task to the signaling thread ---
// bool
// LibwebrtcPeerConnectionImpl::PostTaskToSignalingThread(std::function<void()>
// task) {
//     // Requires access to the signaling thread object/dispatcher
//     // if (signaling_thread_) {
//     //     return signaling_thread_->PostTask(RTC_FROM_HERE,
//     std::move(task));
//     // }
//     // return false;
// }

// Helper to marshal a task to the application thread
// bool LibwebrtcPeerConnectionImpl::PostTaskToAppThread(std::function<void()>
// task) {
//     // Requires access to the application thread object/dispatcher
//     // if (app_thread_) {
//     //     return app_thread_->PostTask(RTC_FROM_HERE, std::move(task));
//     // }
//     // return false;
// }

// Helper to safely invoke application callbacks, marshalling if needed
// This is complex as it needs to know the target thread and have access to its
// dispatcher. Often involves capturing dependencies by value or shared_ptr for
// the lambda. void
// LibwebrtcPeerConnectionImpl::InvokeCallback(std::function<void()>
// app_callback) {
//     if (app_callback) {
//          // Option A: Call directly (simplest, but app callbacks must be
//          thread-safe)
//          // app_callback();
//
//          // Option B: Marshal to app thread
//          // PostTaskToAppThread(app_callback); // Conceptual
//     }
// }
//
// void Libwebrபட்சPeerConnectionImpl::InvokeCallbackWithArg(...) { /* Similar to
// InvokeCallback */ }

}  // namespace webrtc
}  // namespace remote
}  // namespace autodev
