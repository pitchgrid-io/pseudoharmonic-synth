#include "OSCReceiver.h"

#ifdef _WIN32
static void closeSocket(SOCKET s) { closesocket(s); }
#else
static void closeSocket(SOCKET s) { close(s); }
#endif

OSCReceiver::~OSCReceiver()
{
    stop();
}

bool OSCReceiver::start(uint16_t pluginPort)
{
    if (running_.load()) return false;

    pluginPort_ = pluginPort;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) return false;

    // Bind to ephemeral port
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bindAddr.sin_port = 0; // ephemeral

    if (::bind(socket_, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0)
    {
        closeSocket(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }

    // Resolve actual port
    sockaddr_in resolved{};
    socklen_t addrLen = sizeof(resolved);
    getsockname(socket_, reinterpret_cast<sockaddr*>(&resolved), &addrLen);
    myPort_ = ntohs(resolved.sin_port);

    // Set up plugin address for heartbeat sending
    pluginAddr_ = {};
    pluginAddr_.sin_family = AF_INET;
    pluginAddr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    pluginAddr_.sin_port = htons(pluginPort_);

    // Set socket receive timeout so listenLoop can check running_ flag
#ifdef _WIN32
    DWORD tv = 500;
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 500ms
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    running_ = true;
    listenThread_ = std::thread([this]() { listenLoop(); });
    heartbeatThread_ = std::thread([this]() { heartbeatLoop(); });

    return true;
}

void OSCReceiver::stop()
{
    running_ = false;

    if (socket_ != INVALID_SOCKET)
    {
        closeSocket(socket_);
        socket_ = INVALID_SOCKET;
    }

    if (listenThread_.joinable()) listenThread_.join();
    if (heartbeatThread_.joinable()) heartbeatThread_.join();
}

TuningParams OSCReceiver::getTuningParams() const
{
    std::lock_guard<std::mutex> lock(tuningMutex_);
    return tuning_;
}

bool OSCReceiver::isConnected() const
{
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now().time_since_epoch()).count();
    auto last = lastHeartbeatMs_.load();
    return last > 0 && (now - last) < kConnectionTimeoutMs;
}

// ── Listen loop ──────────────────────────────────────────────────────────────

void OSCReceiver::listenLoop()
{
    uint8_t buf[2048];

    while (running_.load())
    {
        sockaddr_in sender{};
        socklen_t senderLen = sizeof(sender);
        auto n = ::recvfrom(socket_, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                            reinterpret_cast<sockaddr*>(&sender), &senderLen);

        if (n > 0)
            parseOSCPacket(buf, static_cast<size_t>(n));
    }
}

// ── Heartbeat loop ───────────────────────────────────────────────────────────

void OSCReceiver::heartbeatLoop()
{
    while (running_.load())
    {
        // Build OSC packet: /pitchgrid/heartbeat ,ii [1, myPort_]
        std::vector<uint8_t> pkt;
        writeOSCString(pkt, "/pitchgrid/heartbeat");
        writeOSCString(pkt, ",ii");
        writeInt32(pkt, 1);
        writeInt32(pkt, static_cast<int32_t>(myPort_));

        if (socket_ != INVALID_SOCKET)
        {
            ::sendto(socket_, reinterpret_cast<const char*>(pkt.data()), pkt.size(), 0,
                     reinterpret_cast<const sockaddr*>(&pluginAddr_), sizeof(pluginAddr_));
        }

        // Sleep 1 second in small increments so we can stop quickly
        for (int i = 0; i < 10 && running_.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ── OSC parsing ──────────────────────────────────────────────────────────────

void OSCReceiver::parseOSCPacket(const uint8_t* data, size_t len)
{
    if (len < 4) return;

    size_t offset = 0;
    std::string address = readOSCString(data, len, offset);
    if (address.empty() || offset >= len) return;

    // Read type tag string
    std::string typeTags = readOSCString(data, len, offset);
    if (typeTags.empty() || typeTags[0] != ',') return;

    if (address == "/pitchgrid/heartbeat")
    {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()).count();
        lastHeartbeatMs_.store(now);
        return;
    }

    if (address == "/pitchgrid/plugin/tuning")
    {
        // Expected: mode, root_freq, stretch, skew, mode_offset, steps, mos_a, mos_b
        // Type tags vary — parse whatever types arrive and coerce
        std::vector<double> args;
        for (size_t i = 1; i < typeTags.size() && offset < len; ++i)
        {
            char tag = typeTags[i];
            if (tag == 'i')
                args.push_back(static_cast<double>(readInt32(data, offset)));
            else if (tag == 'f')
                args.push_back(static_cast<double>(readFloat32(data, offset)));
            else if (tag == 'd')
                args.push_back(readFloat64(data, offset));
            else
                break; // unknown type tag
        }

        if (args.size() >= 8)
        {
            std::lock_guard<std::mutex> lock(tuningMutex_);
            tuning_.mode = static_cast<int>(args[0]);
            tuning_.rootFreq = args[1];
            tuning_.stretch = args[2];
            tuning_.skew = args[3];
            tuning_.modeOffset = static_cast<float>(args[4]);
            tuning_.steps = std::max(1, static_cast<int>(args[5]));
            tuning_.mosA = std::max(1, static_cast<int>(args[6]));
            tuning_.mosB = std::max(1, static_cast<int>(args[7]));
            tuningVersion_++;

            fprintf(stderr, "[OSC recv] tuning: mode=%d rootFreq=%.2f stretch=%.6f skew=%.6f modeOffset=%.6f steps=%d mosA=%d mosB=%d\n",
                tuning_.mode, tuning_.rootFreq, tuning_.stretch, tuning_.skew,
                tuning_.modeOffset, tuning_.steps, tuning_.mosA, tuning_.mosB);
        }

        // Also count as heartbeat
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()).count();
        lastHeartbeatMs_.store(now);
    }
}

// ── OSC binary helpers ───────────────────────────────────────────────────────

std::string OSCReceiver::readOSCString(const uint8_t* data, size_t len, size_t& offset)
{
    // OSC strings are null-terminated and padded to 4-byte boundary
    size_t start = offset;
    while (offset < len && data[offset] != 0) ++offset;
    std::string result(reinterpret_cast<const char*>(data + start), offset - start);
    ++offset; // skip null
    // Pad to 4-byte boundary
    while (offset % 4 != 0 && offset < len) ++offset;
    return result;
}

int32_t OSCReceiver::readInt32(const uint8_t* data, size_t& offset)
{
    int32_t v = (static_cast<int32_t>(data[offset]) << 24)
              | (static_cast<int32_t>(data[offset + 1]) << 16)
              | (static_cast<int32_t>(data[offset + 2]) << 8)
              |  static_cast<int32_t>(data[offset + 3]);
    offset += 4;
    return v;
}

float OSCReceiver::readFloat32(const uint8_t* data, size_t& offset)
{
    uint32_t bits = (static_cast<uint32_t>(data[offset]) << 24)
                  | (static_cast<uint32_t>(data[offset + 1]) << 16)
                  | (static_cast<uint32_t>(data[offset + 2]) << 8)
                  |  static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    float v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

double OSCReceiver::readFloat64(const uint8_t* data, size_t& offset)
{
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i)
        bits = (bits << 8) | data[offset + i];
    offset += 8;
    double v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

void OSCReceiver::writeOSCString(std::vector<uint8_t>& buf, const std::string& s)
{
    for (char c : s) buf.push_back(static_cast<uint8_t>(c));
    buf.push_back(0); // null terminator
    while (buf.size() % 4 != 0) buf.push_back(0); // pad to 4-byte boundary
}

void OSCReceiver::writeInt32(std::vector<uint8_t>& buf, int32_t v)
{
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}
