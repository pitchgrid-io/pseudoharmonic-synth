#include "WSBridge.h"
#include <iostream>

WSBridge::WSBridge() = default;

WSBridge::~WSBridge()
{
    stop();
}

bool WSBridge::start(uint16_t port)
{
    bool ok = server_.start("127.0.0.1", port,
        [this](std::shared_ptr<SimpleWS::WebSocketConnection> conn) {
            handleConnection(conn);
        });
    if (ok)
        port_ = server_.getPort();
    return ok;
}

void WSBridge::stop()
{
    server_.stop();
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& c : clients_)
        if (c) c->close();
    clients_.clear();
}

uint16_t WSBridge::getPort() const
{
    return port_.load();
}

void WSBridge::handleConnection(std::shared_ptr<SimpleWS::WebSocketConnection> conn)
{
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.push_back(conn);
    }
    if (connectCallback_)
        connectCallback_();
    clientLoop(conn);
}

void WSBridge::clientLoop(std::shared_ptr<SimpleWS::WebSocketConnection> conn)
{
    while (conn->isConnected())
    {
        std::string msg = conn->receiveMessage();
        if (msg.empty()) break;

        try
        {
            auto j = json::parse(msg);
            if (j.contains("type") && j["type"] == "param" && j.contains("id") && j.contains("value"))
            {
                if (paramCallback_)
                    paramCallback_(j["id"].get<std::string>(), j["value"].get<float>());
            }
        }
        catch (...) {}
    }

    // Remove disconnected client
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [&](const auto& c) { return c.get() == conn.get() || !c->isConnected(); }),
        clients_.end());
}

void WSBridge::broadcast(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& c : clients_)
    {
        if (c && c->isConnected())
            c->sendText(msg);
    }
}

void WSBridge::sendParams(const json& params)
{
    json msg = {{"type", "params"}, {"data", params}};
    broadcast(msg.dump());
}

void WSBridge::sendCurveData(const json& curveData)
{
    json msg = {{"type", "curve"}, {"data", curveData}};
    broadcast(msg.dump());
}

void WSBridge::sendActiveNotes(const json& notes)
{
    json msg = {{"type", "notes"}, {"data", notes}};
    broadcast(msg.dump());
}

void WSBridge::sendIntervals(const json& intervals)
{
    json msg = {{"type", "intervals"}, {"data", intervals}};
    broadcast(msg.dump());
}

void WSBridge::sendScaleDegrees(const json& degrees)
{
    json msg = {{"type", "scaleDegrees"}, {"data", degrees}};
    broadcast(msg.dump());
}

void WSBridge::sendLevel(float peak)
{
    json msg = {{"type", "level"}, {"value", peak}};
    broadcast(msg.dump());
}
