#pragma once

#include "SimpleWebSocketServer.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>

using json = nlohmann::json;

// Bidirectional WebSocket bridge for Svelte UI communication
// All params synced as JSON: { "type": "param", "id": "stretch3", "value": 3.0 }
// Curve data sent as JSON arrays
// Active notes sent as JSON

class WSBridge
{
public:
    WSBridge();
    ~WSBridge();

    bool start(uint16_t port = 0); // 0 = auto-assign
    void stop();
    uint16_t getPort() const;

    // Called from audio thread (or timer) to push state to UI
    void sendParams(const json& params);
    void sendCurveData(const json& curveData);
    void sendActiveNotes(const json& notes);
    void sendIntervals(const json& intervals);
    void sendScaleDegrees(const json& degrees);
    void sendLevel(float peak);

    // Callback when UI changes a parameter
    using ParamCallback = std::function<void(const std::string& id, float value)>;
    void onParamChange(ParamCallback cb) { paramCallback_ = cb; }

    // Callback when a new client connects
    using ConnectCallback = std::function<void()>;
    void onClientConnect(ConnectCallback cb) { connectCallback_ = cb; }

private:
    void handleConnection(std::shared_ptr<SimpleWS::WebSocketConnection> conn);
    void clientLoop(std::shared_ptr<SimpleWS::WebSocketConnection> conn);
    void broadcast(const std::string& msg);

    SimpleWS::WebSocketServer server_;
    std::vector<std::shared_ptr<SimpleWS::WebSocketConnection>> clients_;
    std::mutex clientsMutex_;
    ParamCallback paramCallback_;
    ConnectCallback connectCallback_;
    std::atomic<uint16_t> port_{0};
};
