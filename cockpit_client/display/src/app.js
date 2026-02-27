// display/public/js/app.js

// Define global app configuration (or fetch from a config endpoint on TransportServer)
const APP_CONFIG = {
    WEBSOCKET_URL: `ws://${window.location.hostname}:8080`, // Match TransportServer address and port
    TARGET_VEHICLE_ID: 'vehicle_client_dummy_id', // Match the ID of the vehicle client you want to connect to
    CLIENT_ID: 'cockpit_client_browser_' + Math.random().toString(16).slice(2, 8), // Generate a unique ID for this browser instance
    // WebRTC ICE server configuration (STUN/TURN) - match vehicle_client config
    RTC_CONFIG: {
        iceServers: [
            { urls: 'stun:stun.l.google.com:19302' },
            // Add TURN servers if needed: { urls: 'turn:your.turn.server', username: 'user', credential: 'password' }
        ]
    },
    CONTROL_CHANNEL_LABEL: 'control',
    TELEMETRY_CHANNEL_LABEL: 'telemetry',
    // Add other config like heartbeat interval, etc.
};

// --- Global instances ---
let statusPanel;
let videoPlayer;
let websocket;
let controlDataChannel = null; // Reference to the WebRTC DataChannel for sending commands

// --- Logging Utility ---
const logContentElement = document.getElementById('logContent');
function appendLog(message) {
    const timestamp = new Date().toLocaleTimeString();
    if (logContentElement) {
        logContentElement.textContent += `[${timestamp}] ${message}\n`;
        logContentElement.scrollTop = logContentElement.scrollHeight; // Auto-scroll
    } else {
        console.log(`[${timestamp}] ${message}`);
    }
}
console.log = function (message) { appendLog("LOG: " + message); };
console.info = function (message) { appendLog("INFO: " + message); };
console.warn = function (message) { appendLog("WARN: " + message); };
console.error = function (message) { appendLog("ERROR: " + message); };


// --- WebSocket Handling ---
function connectWebSocket() {
    appendLog(`Attempting to connect WebSocket to ${APP_CONFIG.WEBSOCKET_URL}`);
    websocket = new WebSocket(APP_CONFIG.WEBSOCKET_URL);

    websocket.onopen = () => {
        appendLog('WebSocket connection opened.');
        // WebSocket is ready. Initialize WebRTC and signal backend.
        initWebRTC();

        // Send a JOIN message to the backend/signaling server via WebSocket
        // This tells the backend we are online and want to connect to a specific vehicle.
        const joinMessage = {
            type: 'join',
            from: APP_CONFIG.CLIENT_ID,
            to: '', // To signaling server itself, or empty for broadcast
            data: { // Optional data payload
                targetVehicleId: APP_CONFIG.TARGET_VEHICLE_ID
            }
        };
        sendWebSocketMessage(joinMessage);

        // Cockpit acts as initiator by default to avoid both sides waiting.
        // Delay briefly so join can be processed server-side first.
        setTimeout(async () => {
            if (videoPlayer && videoPlayer.peerConnection) {
                await videoPlayer.createOffer();
            }
        }, 200);
    };

    websocket.onmessage = async (event) => {
        // Messages from backend could be signaling or telemetry
        try {
            const message = JSON.parse(event.data);
            // appendLog(`WebSocket message received: ${JSON.stringify(message)}`); // Log all messages (can be noisy)

            if (message.type === 'signaling' && message.payload) {
                // Backward-compatible signaling envelope
                await handleSignalingMessage(message.payload);
            } else if (message.type === 'offer' || message.type === 'answer' || message.type === 'candidate') {
                // Direct signaling message (expected by signaling_server/session_manager)
                await handleSignalingMessage(message);
            } else if (message.type === 'telemetry') {
                // This is vehicle status/telemetry data
                if (statusPanel) {
                    statusPanel.updateStatus(message.data); // Assume data is the telemetry object
                }
            } else if (message.type === 'leave') {
                appendLog(`Peer left: ${message.from || 'unknown'}`);
                if (videoPlayer) {
                    videoPlayer.close();
                }
            } else if (message.type === 'error') {
                appendLog(`Signaling error: ${message.message || 'unknown error'}`);
            } else if (message.type === 'control_ack') {
                // Optional: Acknowledge for control commands
                appendLog(`Control command acknowledged by backend/vehicle.`);
            }
            // Add handling for other message types if necessary

        } catch (e) {
            console.error('Failed to parse WebSocket message:', e, 'Data:', event.data);
        }
    };

    websocket.onerror = (event) => {
        console.error('WebSocket error observed:', event);
        // Attempt to reconnect after a delay
    };

    websocket.onclose = (event) => {
        appendLog('WebSocket connection closed.');
        // Clean up WebRTC connection if it exists
        if (videoPlayer) {
            videoPlayer.close();
        }
        if (statusPanel) {
            statusPanel.clearStatus();
        }
        // Attempt to reconnect after a delay
        setTimeout(connectWebSocket, 5000); // Retry every 5 seconds
    };
}

/**
* Sends a message over the WebSocket connection.
* Assumes the backend expects a structured JSON message, e.g., { type: '...', data: { ... } }.
* @param {object} message - The JavaScript object to send (will be JSON stringified).
*/
function sendWebSocketMessage(message) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(JSON.stringify(message));
        // appendLog(`WebSocket message sent: ${JSON.stringify(message)}`); // Log sent messages (can be noisy)
    } else {
        console.warn('WebSocket connection not open. Message not sent:', message);
    }
}


// --- WebRTC Handling ---
function initWebRTC() {
    if (videoPlayer && videoPlayer.peerConnection) {
        console.warn("WebRTC already initialized.");
        return;
    }

    // Initialize the VideoPlayer module
    videoPlayer = new VideoPlayer('vehicleVideo', 'connectionStatus');

    // Pass a callback to the VideoPlayer for it to send signaling messages
    // This callback will structure the signaling message payload and send it via our WebSocket
    const signalingCallback = (signalingPayload) => {
        // Send direct signaling message expected by signaling_server/session_manager.
        const signalingMessage = {
            ...signalingPayload,
            from: APP_CONFIG.CLIENT_ID,
            to: APP_CONFIG.TARGET_VEHICLE_ID
        };
        sendWebSocketMessage(signalingMessage);
    };

    videoPlayer.init(APP_CONFIG.RTC_CONFIG, signalingCallback);

    // --- Optional: Create Data Channels here if the browser is initiating them ---
    // Control channel for sending commands
    const controlChannel = videoPlayer.createDataChannel(APP_CONFIG.CONTROL_CHANNEL_LABEL, { ordered: true, negotiated: false }); // ordered/reliable
    if (controlChannel) {
        controlChannel.onopen = () => {
            appendLog(`Control DataChannel '${controlChannel.label}' opened.`);
            controlDataChannel = controlChannel; // Store reference
            document.getElementById('controlStatus').textContent = 'Control Channel: Open';
        };
        controlChannel.onclose = () => {
            appendLog(`Control DataChannel '${controlChannel.label}' closed.`);
            controlDataChannel = null;
            document.getElementById('controlStatus').textContent = 'Control Channel: Closed';
        };
        controlChannel.onerror = (error) => {
            console.error(`Control DataChannel '${controlChannel.label}' error:`, error);
            document.getElementById('controlStatus').textContent = 'Control Channel: Error';
        };
        // controlChannel.onmessage is not needed here as we only send commands on this channel
    }

    // Telemetry channel for receiving data (optional, if browser *creates* it and vehicle sends *to* it)
    // More likely, vehicle creates telemetry channel and browser receives it via ondatachannel event on PC.
    // So, the ondatachannel handler inside video_player.js is more relevant for telemetry.

    // Offer is now created in websocket.onopen after JOIN.
}

/**
* Handles incoming signaling messages received via the WebSocket from the backend.
* These are messages relayed from the vehicle client (Offer, Answer, Candidate).
* @param {object} signalingPayload - The signaling payload (matches RTCSessionDescriptionInit or RTCIceCandidateInit structure).
*/
async function handleSignalingMessage(signalingPayload) {
    if (!videoPlayer || !videoPlayer.peerConnection) {
        console.warn("Received signaling message, but WebRTC not initialized.");
        return;
    }

    if (signalingPayload.type === 'offer' || signalingPayload.type === 'answer') {
        // Set the remote description
        await videoPlayer.setRemoteDescription(signalingPayload);
    } else if (signalingPayload.type === 'candidate') {
        // Add the ICE candidate
        await videoPlayer.addIceCandidate(signalingPayload);
    } else {
        console.warn('Received unknown signaling payload type:', signalingPayload.type);
    }
}

/**
* Sends a control command via the WebRTC DataChannel.
* Assumes the control DataChannel is open.
* Assumes commandData is an object ready to be stringified (or a binary buffer).
* TODO: Convert commandData object to protobuf binary data before sending.
* @param {object} commandData - The control command data (e.g., { acceleration: 1.0, braking: 0.0 }).
*/
function sendControlCommand(commandData) {
    if (controlDataChannel && controlDataChannel.readyState === 'open') {
        // TODO: Serialize commandData object into Protobuf binary data
        // This requires a protobuf.js setup in the browser to compile .proto files
        // Example:
        // const CommandProto = protobuf.lookup("autodev.remote.control.ControlCommand");
        // const errMsg = CommandProto.verify(commandData);
        // if (errMsg) throw Error(errMsg);
        // const message = CommandProto.create(commandData);
        // const buffer = CommandProto.encode(message).finish(); // This is the binary data

        // --- Skeleton: Send as JSON string for simplicity ---
        const commandMessage = {
            type: 'control', // Indicate control type to C++ handler
            data: commandData
        };
        const dataToSend = JSON.stringify(commandMessage); // Send JSON string

        controlDataChannel.send(dataToSend);
        // console.log('Sent command via DataChannel:', commandData);
        document.getElementById('controlStatus').textContent = 'Control Channel: Sending';
    } else {
        console.warn('Control DataChannel not open. Command not sent:', commandData);
        document.getElementById('controlStatus').textContent = 'Control Channel: Not Open';
    }
}

/**
* Sends an emergency command via the WebRTC DataChannel.
* Similar to sendControlCommand but for emergency actions.
* @param {object} commandData - The emergency command data (e.g., { type: 1, reason: "User pressed button" }).
*/
function sendEmergencyCommand(commandData) {
    if (controlDataChannel && controlDataChannel.readyState === 'open') {
        // TODO: Serialize commandData object into EmergencyCommand Protobuf binary data
        const commandMessage = {
            type: 'emergency', // Indicate emergency type to C++ handler
            data: commandData
        };
        const dataToSend = JSON.stringify(commandMessage); // Send JSON string

        controlDataChannel.send(dataToSend);
        console.log('Sent emergency command via DataChannel:', commandData);
        document.getElementById('controlStatus').textContent = 'Control Channel: Sending Emergency';
    } else {
        console.warn('Control DataChannel not open. Emergency command not sent:', commandData);
        document.getElementById('controlStatus').textContent = 'Control Channel: Not Open';
    }
}


// --- Initialize App ---
document.addEventListener('DOMContentLoaded', () => {
    // 1. Initialize Status Panel
    statusPanel = new StatusPanel('statusPanel', 'telemetryStatus');

    // 2. Connect WebSocket to local server
    connectWebSocket();

    // 3. Set up control button listeners (skeleton)
    const btnForward = document.getElementById('btnForward');
    const btnStop = document.getElementById('btnStop');
    const btnEmergencyStop = document.getElementById('btnEmergencyStop');

    if (btnForward) {
        btnForward.addEventListener('click', () => {
            // Simulate sending a forward command (adjust values as per protobuf definition)
            const command = { acceleration: 1.0, braking: 0.0, steering_angle: 0.0, gear: 1, hand_brake: false }; // Drive gear
            sendControlCommand(command);
        });
    }

    if (btnStop) {
        btnStop.addEventListener('click', () => {
            // Simulate sending a stop command
            const command = { acceleration: 0.0, braking: 1.0, steering_angle: 0.0, gear: 0, hand_brake: true }; // Brake, Neutral, Handbrake
            sendControlCommand(command);
        });
    }

    if (btnEmergencyStop) {
        btnEmergencyStop.addEventListener('click', () => {
            // Simulate sending an emergency stop command (matches emergency_command.proto)
            const emergencyCommand = { type: 1, reason: "Manual Emergency Stop Button" }; // 1 corresponds to EMERGENCY_STOP enum
            sendEmergencyCommand(emergencyCommand);
        });
    }

    // TODO: Implement joystick/keyboard input listeners and map them to sendControlCommand
});

// Attach classes to window for simple script tag usage
// window.StatusPanel = StatusPanel;
// window.VideoPlayer = VideoPlayer;
