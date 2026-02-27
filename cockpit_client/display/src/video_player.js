// display/public/js/video_player.js

/**
 * Manages the WebRTC PeerConnection for receiving video and handling signaling.
 */
class VideoPlayer {
    constructor(videoElementId = 'vehicleVideo', connectionStatusId = 'connectionStatus') {
        this.videoElement = document.getElementById(videoElementId);
        this.connectionStatusElement = document.getElementById(connectionStatusId);
        this.peerConnection = null; // RTCPeerConnection instance
        this.localDataChannel = null; // Optional: if browser creates data channels
        this.signalingCallback = null; // Callback to send signaling messages (SDP, ICE) to the backend
        this.pendingRemoteCandidates = [];
        this.isRemoteDescriptionSet = false;

        if (!this.videoElement || !this.connectionStatusElement) {
            console.error("VideoPlayer: Required DOM elements not found!");
        } else {
            console.log("VideoPlayer initialized.");
        }
    }

    /**
     * Initializes the WebRTC PeerConnection.
     * @param {object} rtcConfig - WebRTC configuration object (e.g., { iceServers: [...] }).
     * @param {function} signalingCallback - Callback function (message) => void, to send signaling messages to the backend.
     */
    init(rtcConfig, signalingCallback) {
        if (this.peerConnection) {
            console.warn("VideoPlayer: PeerConnection already initialized.");
            return;
        }

        this.signalingCallback = signalingCallback;

        // Create RTCPeerConnection
        try {
            this.peerConnection = new RTCPeerConnection(rtcConfig);
            console.log("VideoPlayer: RTCPeerConnection created.");
            this.setConnectionStatus('Connecting...');

            // Set up event handlers
            this.peerConnection.onicecandidate = event => {
                if (event.candidate) {
                    console.log('VideoPlayer: ICE candidate generated:', event.candidate);
                    // Send the candidate to the backend via the signaling callback
                    if (this.signalingCallback) {
                        // Structure candidate message matching C++ SignalMessage format
                        const candidateMsg = {
                            type: 'candidate',
                            candidate: event.candidate.candidate,
                            sdpMid: event.candidate.sdpMid,
                            sdpMLineIndex: event.candidate.sdpMLineIndex
                        };
                        this.signalingCallback(candidateMsg);
                    }
                } else {
                    console.log('VideoPlayer: ICE gathering finished.');
                }
            };

            // Handle incoming tracks (e.g., video from the vehicle)
            this.peerConnection.ontrack = event => {
                console.log('VideoPlayer: Remote track received:', event.track.kind);
                if (event.track.kind === 'video' && this.videoElement) {
                    // Attach the video stream to the video element
                    this.videoElement.srcObject = event.streams[0];
                    this.setConnectionStatus('Receiving Video');
                }
                // Handle other track types (audio, etc.)
            };

            // Monitor ICE connection state (connecting, connected, disconnected, failed, etc.)
            this.peerConnection.oniceconnectionstatechange = () => {
                console.log('VideoPlayer: ICE connection state change:', this.peerConnection.iceConnectionState);
                this.setConnectionStatus(`ICE: ${this.peerConnection.iceConnectionState}`);
                if (this.peerConnection.iceConnectionState === 'connected' || this.peerConnection.iceConnectionState === 'completed') {
                    // ICE connected, media flow should start soon
                    console.log('VideoPlayer: ICE connected.');
                    // Status will update again when video track arrives
                } else if (this.peerConnection.iceConnectionState === 'disconnected' || this.peerConnection.iceConnectionState === 'failed' || this.peerConnection.iceConnectionState === 'closed') {
                    console.error('VideoPlayer: ICE connection failed or disconnected.');
                    this.setConnectionStatus(`ICE: ${this.peerConnection.iceConnectionState}`);
                    this.close(); // Attempt cleanup
                }
            };

            // Monitor overall PeerConnection state
            this.peerConnection.onconnectionstatechange = () => {
                console.log('VideoPlayer: PeerConnection state change:', this.peerConnection.connectionState);
                // this.setConnectionStatus(`PC: ${this.peerConnection.connectionState}`); // Could use this too
            };

            // Handle incoming data channels (if the remote peer creates them)
            this.peerConnection.ondatachannel = event => {
                console.log('VideoPlayer: DataChannel received:', event.channel.label);
                // This typically won't happen if C++ side creates data channels by label
                // We expect data channels to be available by label after SDP negotiation.
            };

            // Create Data Channels explicitly by label (matches C++ side)
            // These will be available after SDP negotiation and ICE connection
            // const controlChannel = this.peerConnection.createDataChannel("control");
            // const telemetryChannel = this.peerConnection.createDataChannel("telemetry");
            // controlChannel.onopen = () => { console.log('Control DataChannel opened'); /* ready to send commands */ };
            // telemetryChannel.onmessage = event => { console.log('Telemetry DataChannel message:', event.data); /* handle telemetry data */ };
            // telemetryChannel.onopen = () => { console.log('Telemetry DataChannel opened'); };
            // ... handle other data channel events (onclose, onerror)

        } catch (e) {
            console.error('VideoPlayer: Failed to create RTCPeerConnection:', e);
            this.setConnectionStatus('Error');
            this.peerConnection = null;
        }
    }

    /**
     * Creates an SDP Offer and sends it via the signaling callback.
     */
    async createOffer() {
        if (!this.peerConnection) {
            console.error("VideoPlayer: PeerConnection not initialized.");
            return;
        }
        try {
            const offer = await this.peerConnection.createOffer();
            await this.peerConnection.setLocalDescription(offer);
            console.log('VideoPlayer: Created and set local Offer:', offer.sdp);
            // Send the offer to the backend via the signaling callback
            if (this.signalingCallback) {
                const offerMsg = {
                    type: 'offer',
                    sdp: offer.sdp
                };
                this.signalingCallback(offerMsg);
            }
        } catch (e) {
            console.error('VideoPlayer: Failed to create offer:', e);
            this.setConnectionStatus('Error');
        }
    }

    /**
     * Sets the remote description (Offer or Answer) received from the remote peer.
     * @param {RTCSessionDescriptionInit} sdp - The SDP object received (e.g., { type: 'offer', sdp: 'v=...' }).
     */
    async setRemoteDescription(sdp) {
        if (!this.peerConnection) {
            console.error("VideoPlayer: PeerConnection not initialized.");
            return;
        }
        try {
            await this.peerConnection.setRemoteDescription(sdp);
            this.isRemoteDescriptionSet = true;
            console.log(`VideoPlayer: Set remote description (${sdp.type}).`);

            // Flush queued candidates that arrived before remote description.
            await this.flushPendingRemoteCandidates();

            // If we received an Offer, create an Answer
            if (sdp.type === 'offer') {
                await this.createAnswer();
            }
        } catch (e) {
            console.error('VideoPlayer: Failed to set remote description:', e);
            this.setConnectionStatus('Error');
        }
    }

    /**
     * Creates an SDP Answer (after receiving an Offer) and sends it via the signaling callback.
     */
    async createAnswer() {
        if (!this.peerConnection) {
            console.error("VideoPlayer: PeerConnection not initialized.");
            return;
        }
        try {
            const answer = await this.peerConnection.createAnswer();
            await this.peerConnection.setLocalDescription(answer);
            console.log('VideoPlayer: Created and set local Answer:', answer.sdp);
            // Send the answer to the backend via the signaling callback
            if (this.signalingCallback) {
                const answerMsg = {
                    type: 'answer',
                    sdp: answer.sdp
                };
                this.signalingCallback(answerMsg);
            }
        } catch (e) {
            console.error('VideoPlayer: Failed to create answer:', e);
            this.setConnectionStatus('Error');
        }
    }

    /**
     * Adds an ICE candidate received from the remote peer.
     * @param {object} candidate - The ICE candidate object received (matching RTCIceCandidateInit structure,
     * but often received as a plain object matching SignalMessage structure).
     * Assumes { candidate: 'candidate:...', sdpMid: 'video', sdpMLineIndex: 0 }.
     */
    async addIceCandidate(candidate) {
        if (!this.peerConnection) {
            console.error("VideoPlayer: PeerConnection not initialized.");
            return;
        }
        // RTCIceCandidate() constructor takes an object matching RTCIceCandidateInit
        const normalized = {
            candidate: candidate.candidate,
            sdpMid: candidate.sdpMid,
            sdpMLineIndex: candidate.sdpMLineIndex ?? candidate.sdpMlineIndex
        };

        if (!this.isRemoteDescriptionSet) {
            this.pendingRemoteCandidates.push(normalized);
            console.log('VideoPlayer: Queued remote ICE candidate before remote description.');
            return;
        }

        const iceCandidate = new RTCIceCandidate(normalized);

        try {
            await this.peerConnection.addIceCandidate(iceCandidate);
            console.log('VideoPlayer: Added ICE candidate:', iceCandidate);
        } catch (e) {
            console.error('VideoPlayer: Failed to add ICE candidate:', e);
            // Note: Non-fatal errors adding candidates can happen, don't necessarily set status to Error
        }
    }

    async flushPendingRemoteCandidates() {
        if (!this.peerConnection || this.pendingRemoteCandidates.length === 0) {
            return;
        }

        const queued = [...this.pendingRemoteCandidates];
        this.pendingRemoteCandidates = [];

        for (const candidate of queued) {
            try {
                const iceCandidate = new RTCIceCandidate(candidate);
                await this.peerConnection.addIceCandidate(iceCandidate);
            } catch (e) {
                console.error('VideoPlayer: Failed to add queued ICE candidate:', e);
            }
        }
    }

    /**
     * Helper to update the connection status display.
     * @param {string} statusText - The status text.
     */
    setConnectionStatus(statusText) {
        if (this.connectionStatusElement) {
            this.connectionStatusElement.textContent = statusText;
            // Optionally change color based on statusText (e.g., "Connected", "Error")
        }
    }

    /**
     * Closes the PeerConnection and cleans up resources.
     */
    close() {
        if (this.peerConnection) {
            console.log("VideoPlayer: Closing PeerConnection.");
            this.peerConnection.close();
            this.peerConnection = null;
            this.pendingRemoteCandidates = [];
            this.isRemoteDescriptionSet = false;
            this.setConnectionStatus('Disconnected');
            if (this.videoElement) {
                this.videoElement.srcObject = null; // Clear video stream
            }
        }
    }

    /**
     * Gets a DataChannel by its label.
     * Useful for sending data after channels are ready (onopen).
     * Note: Channels created with createDataChannel() are available immediately,
     * but might not be 'open' until ICE/DTLS connects.
     * Channels created by remote peer are available via ondatachannel event.
     * This method *might* need to track created/received channels.
     * For simplicity here, we rely on the caller knowing the channel.
     * A more robust approach would be to get the channel inside the command handler.
     * @param {string} label - The DataChannel label.
     * @returns {RTCDataChannel | null} The DataChannel or null if not found/created/received.
     */
    getDataChannel(label) {
        // This is tricky with RTCPeerConnection. A common pattern is to create channels
        // with specific names, and manage their state (open/closed) internally.
        // For this skeleton, we won't implement a full channel manager here.
        console.warn("VideoPlayer: getDataChannel not fully implemented for retrieval.");
        // You would need to store references to channels created via createDataChannel
        // or received via ondatachannel in a map like this.channels['label'] = channel;
        // return this.channels ? this.channels[label] : null;

        // A simple placeholder:
        return null; // Requires actual channel management
    }

    // Placeholder method to create a data channel - call this AFTER PC is created
    createDataChannel(label, options) {
        if (!this.peerConnection) {
            console.error("VideoPlayer: Cannot create DataChannel, PeerConnection not initialized.");
            return null;
        }
        try {
            const channel = this.peerConnection.createDataChannel(label, options);
            console.log(`VideoPlayer: Created DataChannel: ${label}`);
            // TODO: Add event handlers for this channel (onopen, onmessage, onclose, onerror)
            // And potentially store it in a map
            // this.channels[label] = channel;
            return channel;
        } catch (e) {
            console.error(`VideoPlayer: Failed to create DataChannel ${label}:`, e);
            return null;
        }
    }
}

// Export the class
// window.VideoPlayer = VideoPlayer;
