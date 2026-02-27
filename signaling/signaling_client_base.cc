#include "signaling/signaling_client.h"

SignalingClient::SignalingClient(const std::string& uri, const std::string& jwt)
    : uri_(uri), jwt_(jwt) {}

SignalingClient::~SignalingClient() = default;
