// signaling_server/src/session_manager.js

// Map to store connected clients by their WebSocket connection object
// Using WebSocket object as key (or a unique connection ID managed by server.js)
const connectedClients = new Map(); // Map<WebSocket, ClientInfo { id: string, targetId: string | null }>

// Map to manage active sessions (e.g., VehicleId -> CockpitWebSocket)
// This structure assumes a simple routing logic. More complex routing (1:N, N:M)
// or room-based systems would require a different structure (e.g., Map<string, Set<WebSocket>> for rooms).
const sessions = new Map(); // Map<string, WebSocket> // e.g., 'vehicle_XYZ' -> cockpit WebSocket

/**
 * Adds a newly connected and authenticated client.
 * @param {WebSocket} ws - The client's WebSocket connection.
 * @param {string} clientId - The authenticated client ID.
 */
function addClient(ws, clientId) {
    if (connectedClients.has(ws)) {
        console.warn(`SessionManager: Client ${clientId} already added.`);
        return;
    }
    connectedClients.set(ws, { id: clientId, targetId: null });
    console.log(`SessionManager: Client ${clientId} connected. Total clients: ${connectedClients.size}`);

    // If this is a vehicle, register it in sessions immediately (assuming vehicles are targets)
    // If this is a cockpit, it will send a JOIN message to specify its target.
    // For this simple model, let's register all clients by their ID in the session map initially.
    // This allows lookup by ID. The 'targetId' will be set when they send a JOIN.
    // More complex logic might only add vehicles to the session map.
    sessions.set(clientId, ws); // Register client by ID in the session map
}

/**
 * Removes a client upon disconnection.
 * @param {WebSocket} ws - The disconnecting client's WebSocket connection.
 */
function removeClient(ws) {
    const clientInfo = connectedClients.get(ws);
    if (!clientInfo) {
        console.warn("SessionManager: Unknown client disconnected.");
        return;
    }

    console.log(`SessionManager: Client ${clientInfo.id} disconnected. Total clients before removal: ${connectedClients.size}`);

    // Remove from connected clients map
    connectedClients.delete(ws);

    // Remove from sessions map if present
    // Find the entry where this WebSocket is the value
    let removedFromSessions = false;
    for (const [clientId, clientWs] of sessions.entries()) {
        if (clientWs === ws) {
            sessions.delete(clientId);
            console.log(`SessionManager: Removed client ${clientId} from sessions map.`);
            removedFromSessions = true;

            // If this client was part of a session (had a target or was a target),
            // notify the other peer.
            // This requires tracking session pairs (e.g., Map<string, string> for id -> targetId)
            // or iterating through clients to find who targeted this one.
            // Simple notification: Send a LEAVE message to any peer whose target was clientInfo.id
            connectedClients.forEach((otherClientInfo, otherWs) => {
                if (otherClientInfo.targetId === clientInfo.id) {
                    console.log(`SessionManager: Notifying client ${otherClientInfo.id} that target ${clientInfo.id} left.`);
                    sendMessage(otherWs, {
                        type: 'leave',
                        from: clientInfo.id,
                        to: otherClientInfo.id,
                        reason: 'Peer disconnected'
                    });
                    // Optional: Clear targetId for the other client or manage session state
                }
            });


            break; // Exit loop once found
        }
    }

    if (!removedFromSessions) {
        console.log(`SessionManager: Client ${clientInfo.id} was not found in the sessions map.`);
    }


    console.log(`SessionManager: Client ${clientInfo.id} removed. Total clients now: ${connectedClients.size}`);
}

/**
 * Routes an incoming signaling message to the correct recipient.
 * @param {WebSocket} senderWs - The sender's WebSocket connection.
 * @param {object} message - The parsed signaling message object (JSON).
 * Assumes message structure { type: '...', from: '...', to: '...', ...payload... }
 */
function routeMessage(senderWs, message) {
    const senderInfo = connectedClients.get(senderWs);
    if (!senderInfo) {
        console.warn("SessionManager: Received message from unknown connection.");
        return; // Should not happen if logic is correct
    }

    // Backward-compatible envelope support: { type: 'signaling', payload: {...} }
    let normalizedMessage = message;
    if (message.type === 'signaling' && message.payload && typeof message.payload === 'object') {
        normalizedMessage = {
            ...message.payload,
            from: message.from || senderInfo.id,
            to: message.to || message.targetPeerId || senderInfo.targetId || ''
        };
    }

    // Normalize candidate m-line key for cross-implementation compatibility.
    if (normalizedMessage.type === 'candidate' && normalizedMessage.sdpMLineIndex === undefined && normalizedMessage.sdpMlineIndex !== undefined) {
        normalizedMessage.sdpMLineIndex = normalizedMessage.sdpMlineIndex;
    }

    // Validate 'from' field matches authenticated sender ID
    if (normalizedMessage.from !== senderInfo.id) {
        console.warn(`SessionManager: Received message from ${senderInfo.id} claiming to be from ${message.from}. Ignoring.`);
        // Optional: Send error back to sender
        return;
    }

    console.log(`SessionManager: Routing message type '${normalizedMessage.type}' from ${normalizedMessage.from} to ${normalizedMessage.to || 'server'}.`);

    // Handle specific message types
    switch (normalizedMessage.type) {
        case 'join': {
            // Client requests to join a session or connect to a target peer
            const targetVehicleId = normalizedMessage.data ? normalizedMessage.data.targetVehicleId : null;
            if (!targetVehicleId) {
                console.warn(`SessionManager: JOIN message from ${senderInfo.id} missing targetVehicleId.`);
                // Optional: Send error back to sender
                return;
            }

            // Store the target ID for this sender (this client wants to talk to targetVehicleId)
            senderInfo.targetId = targetVehicleId;
            console.log(`SessionManager: Client ${senderInfo.id} set target to ${targetVehicleId}.`);


            // Check if the target vehicle is currently connected and registered in sessions
            const targetWs = sessions.get(targetVehicleId);

            if (targetWs && connectedClients.has(targetWs)) {
                console.log(`SessionManager: Target vehicle ${targetVehicleId} found for ${senderInfo.id}. Initiating session.`);

                // Notify both peers they are linked (optional messages)
                // You could send a custom 'session_established' message
                // Or, initiate the WebRTC offer/answer exchange directly.
                // A common pattern: the *calling* client (cockpit) sends an Offer,
                // or the *called* client (vehicle) sends back a specific 'join_ack' message.
                // Let's assume the *vehicle* (target) might initiate the offer or respond to JOIN.
                // Send the JOIN message payload (with sender ID) to the target vehicle.
                sendMessage(targetWs, {
                    type: 'join_request', // Custom type for routing JOIN
                    from: senderInfo.id,
                    to: targetVehicleId,
                    data: { clientId: senderInfo.id } // Send initiator's ID
                });

            } else {
                console.warn(`SessionManager: Target vehicle ${targetVehicleId} not found for ${senderInfo.id}.`);
                // Optional: Send error back to sender (e.g., { type: 'error', message: 'Target not found' })
                sendMessage(senderWs, {
                    type: 'error',
                    from: 'server',
                    to: senderInfo.id,
                    message: `Target vehicle ${targetVehicleId} not found or offline.`
                });
                // Optional: Clear targetId or queue the request
                senderInfo.targetId = null; // Clear target if not found
            }
            break;
        }
        case 'leave': {
            // Client requests to leave the session
            const targetId = senderInfo.targetId; // Get the peer they were connected to
            if (targetId) {
                const targetWs = sessions.get(targetId);
                if (targetWs && connectedClients.has(targetWs)) {
                    console.log(`SessionManager: Client ${senderInfo.id} is leaving session with ${targetId}. Notifying target.`);
                    // Send a LEAVE message to the other peer
                    sendMessage(targetWs, {
                        type: 'leave',
                        from: senderInfo.id,
                        to: targetId,
                        reason: normalizedMessage.reason || 'Peer left session'
                    });
                } else {
                    console.warn(`SessionManager: Client ${senderInfo.id} is leaving, but target ${targetId} not found.`);
                }
            }
            // Clear target ID and session state for the sender
            senderInfo.targetId = null;
            console.log(`SessionManager: Client ${senderInfo.id} left session.`);
            break;
        }
        case 'offer':
        case 'answer':
        case 'candidate': {
            // Standard WebRTC signaling messages
            const recipientId = normalizedMessage.to;
            if (!recipientId) {
                console.warn(`SessionManager: Received '${normalizedMessage.type}' message from ${senderInfo.id} with no recipient ('to' field).`);
                // Optional: Send error back to sender
                return;
            }

            // Lookup recipient's WebSocket connection
            const recipientWs = sessions.get(recipientId); // Find recipient by their ID

            if (recipientWs && connectedClients.has(recipientWs)) {
                // Ensure sender is allowed to send to this recipient if implementing stricter access control
                // For 1:1, you might check if sender's targetId is recipientId, and recipient's targetId is senderId.

                console.log(`SessionManager: Routing '${normalizedMessage.type}' from ${normalizedMessage.from} to ${recipientId}.`);
                // Forward the message directly to the recipient
                sendMessage(recipientWs, normalizedMessage);

            } else {
                console.warn(`SessionManager: Recipient ${recipientId} not found or offline for message from ${senderInfo.id}.`);
                // Optional: Send error back to sender
                sendMessage(senderWs, {
                    type: 'error',
                    from: 'server',
                    to: senderInfo.id,
                    message: `Recipient ${recipientId} not found or offline.`
                });
            }
            break;
        }
        // Add other application-specific message types if needed (e.g., chat, status updates)
        default:
            console.warn(`SessionManager: Received unknown message type '${message.type}' from ${message.from}.`);
            // Optional: Send error back to sender
            sendMessage(senderWs, {
                type: 'error',
                from: 'server',
                to: senderInfo.id,
                message: `Unknown message type '${message.type}'.`
            });
            break;
    }
}

/**
 * Sends a JSON message over a WebSocket connection.
 * @param {WebSocket} ws - The WebSocket connection.
 * @param {object} message - The message object to send.
 */
function sendMessage(ws, message) {
    if (ws && ws.readyState === ws.OPEN) {
        try {
            ws.send(JSON.stringify(message));
            // console.log(`Sent message to ${ws.id || 'client'}: ${JSON.stringify(message)}`); // Avoid logging large SDPs
        } catch (e) {
            console.error(`SessionManager: Failed to send message to client:`, e);
            // Handle potential send errors (e.g., connection is closing)
        }
    } else {
        console.warn("SessionManager: Cannot send message, WebSocket not open.");
    }
}

module.exports = {
    addClient,
    removeClient,
    routeMessage
};
