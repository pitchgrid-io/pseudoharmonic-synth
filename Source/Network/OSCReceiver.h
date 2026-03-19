#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define INVALID_SOCKET -1
    typedef int SOCKET;
#endif

struct TuningParams
{
    int mode = 0;
    double rootFreq = 440.0;
    double stretch = 1.0;      // equave (log2 frequency ratio)
    double skew = 0.583333;    // generator
    float modeOffset = 0.0f;
    int steps = 12;
    int mosA = 5;
    int mosB = 2;
};

class OSCReceiver
{
public:
    OSCReceiver() = default;
    ~OSCReceiver();

    bool start(uint16_t pluginPort = 34562);
    void stop();

    TuningParams getTuningParams() const;
    uint64_t getTuningVersion() const { return tuningVersion_.load(); }
    bool isConnected() const;

private:
    void listenLoop();
    void heartbeatLoop();
    void parseOSCPacket(const uint8_t* data, size_t len);

    // OSC binary helpers
    static std::string readOSCString(const uint8_t* data, size_t len, size_t& offset);
    static int32_t readInt32(const uint8_t* data, size_t& offset);
    static float readFloat32(const uint8_t* data, size_t& offset);
    static double readFloat64(const uint8_t* data, size_t& offset);
    static void writeOSCString(std::vector<uint8_t>& buf, const std::string& s);
    static void writeInt32(std::vector<uint8_t>& buf, int32_t v);

    SOCKET socket_ = INVALID_SOCKET;
    uint16_t myPort_ = 0;
    uint16_t pluginPort_ = 34562;
    sockaddr_in pluginAddr_{};

    std::thread listenThread_;
    std::thread heartbeatThread_;
    std::atomic<bool> running_{false};

    mutable std::mutex tuningMutex_;
    TuningParams tuning_;
    std::atomic<uint64_t> tuningVersion_{0};

    using Clock = std::chrono::steady_clock;
    std::atomic<int64_t> lastHeartbeatMs_{0};
    static constexpr int64_t kConnectionTimeoutMs = 2000;
};
