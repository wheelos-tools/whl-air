#ifndef SIGNALING_CLIENT_H
#define SIGNALING_CLIENT_H

#include <functional>
#include <memory>  // For smart pointers
#include <string>

#include "signaling/signaling_message.h"

class SignalingClient {
 public:
  // --- Callbacks for different signaling events ---
  using OnConnectedHandler = std::function<void()>;
  using OnDisconnectedHandler = std::function<void()>;
  // Error message could be more structured
  using OnErrorHandler = std::function<void(const std::string& error_msg)>;
  // Receives parsed signal message
  using OnMessageReceivedHandler =
      std::function<void(const SignalMessage& message)>;

  // Constructor
  // uri: WebSocket server URI (e.g., "wss://your-signaling-server.com/signal")
  // jwt: Optional JWT for authentication
  SignalingClient(const std::string& uri, const std::string& jwt = "");

  // Destructor. Should ensure resources are cleaned up (e.g., disconnect if
  // connected).
  virtual ~SignalingClient();

  // --- Core Signaling Actions ---

  // Initiates the connection process. This is asynchronous.
  // Connection status and events will be reported via registered handlers.
  virtual void connect() = 0;

  // Gracefully disconnects from the signaling server.
  virtual void disconnect() = 0;

  // Sends a structured signal message to the server.
  // The server is responsible for routing the message based on 'message.to'.
  virtual void sendSignal(const SignalMessage& message) = 0;

  // --- Registering Event Handlers ---

  // Sets the handler for successful connection.
  virtual void onConnected(OnConnectedHandler handler) = 0;

  // Sets the handler for disconnection.
  virtual void onDisconnected(OnDisconnectedHandler handler) = 0;

  // Sets the handler for errors during connection or communication.
  virtual void onError(OnErrorHandler handler) = 0;

  // Sets the handler for incoming signal messages.
  virtual void onMessageReceived(OnMessageReceivedHandler handler) = 0;

  // Optional: Get the client's own ID assigned by the server after connection.
  // The server should send a message containing the assigned ID after a
  // successful JOIN. virtual std::string getClientId() const = 0; // Requires
  // state management in implementation

 protected:
  // Implementations can store handlers here
  OnConnectedHandler onConnectedHandler_;
  OnDisconnectedHandler onDisconnectedHandler_;
  OnErrorHandler onErrorHandler_;
  OnMessageReceivedHandler onMessageReceivedHandler_;

  std::string uri_;
  std::string jwt_;

  // Concrete implementations will need internal state (e.g., websocketpp
  // client, connection handle) and methods to map websocketpp events to calling
  // the handlers_. Using PImpl (Pointer to Implementation) is a good way to
  // hide these details.

  // Prevent copying and assignment
  SignalingClient(const SignalingClient&) = delete;
  SignalingClient& operator=(const SignalingClient&) = delete;
};

#endif  // SIGNALING_CLIENT_H
