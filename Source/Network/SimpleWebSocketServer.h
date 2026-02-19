//
// SimpleWebSocketServer.h
// A minimal WebSocket server implementation for the PitchGrid plugin
// Supports binary messages natively without external dependencies
//

#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

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
    #include <fcntl.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    typedef int SOCKET;
#endif

namespace SimpleWS {

class WebSocketConnection {
public:
    WebSocketConnection(SOCKET socket) : socket_(socket), connected_(true) {}
    
    ~WebSocketConnection() {
        close();
    }
    
    bool sendBinary(const uint8_t* data, size_t length) {
        if (!connected_) return false;
        
        std::vector<uint8_t> frame;
        
        // FIN = 1, RSV = 0, Opcode = 0x2 (binary)
        frame.push_back(0x82);
        
        // Payload length
        if (length < 126) {
            frame.push_back(static_cast<uint8_t>(length));
        } else if (length < 65536) {
            frame.push_back(126);
            frame.push_back((length >> 8) & 0xFF);
            frame.push_back(length & 0xFF);
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; --i) {
                frame.push_back((length >> (i * 8)) & 0xFF);
            }
        }
        
        // Payload data
        frame.insert(frame.end(), data, data + length);
        
        // Send frame
        size_t totalSent = 0;
        while (totalSent < frame.size()) {
            int sent = send(socket_, reinterpret_cast<const char*>(frame.data() + totalSent), 
                          static_cast<int>(frame.size() - totalSent), 0);
            if (sent <= 0) {
                connected_ = false;
                return false;
            }
            totalSent += sent;
        }
        
        return true;
    }
    
    bool sendText(const std::string& text) {
        if (!connected_) return false;
        
        std::vector<uint8_t> frame;
        
        // FIN = 1, RSV = 0, Opcode = 0x1 (text)
        frame.push_back(0x81);
        
        // Payload length
        size_t length = text.length();
        if (length < 126) {
            frame.push_back(static_cast<uint8_t>(length));
        } else if (length < 65536) {
            frame.push_back(126);
            frame.push_back((length >> 8) & 0xFF);
            frame.push_back(length & 0xFF);
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; --i) {
                frame.push_back((length >> (i * 8)) & 0xFF);
            }
        }
        
        // Payload data
        frame.insert(frame.end(), text.begin(), text.end());
        
        // Send frame
        size_t totalSent = 0;
        while (totalSent < frame.size()) {
            int sent = send(socket_, reinterpret_cast<const char*>(frame.data() + totalSent), 
                          static_cast<int>(frame.size() - totalSent), 0);
            if (sent <= 0) {
                connected_ = false;
                return false;
            }
            totalSent += sent;
        }
        
        return true;
    }
    
    std::string receiveMessage() {
        if (!connected_) return "";
        
        std::vector<uint8_t> buffer(1024);
        int received = recv(socket_, reinterpret_cast<char*>(buffer.data()), 
                          static_cast<int>(buffer.size()), 0);
        
        if (received <= 0) {
            connected_ = false;
            return "";
        }
        
        // Parse WebSocket frame
        if (received < 2) return "";
        
        size_t pos = 0;
        uint8_t firstByte = buffer[pos++];
        uint8_t secondByte = buffer[pos++];
        
        bool fin = (firstByte & 0x80) != 0;
        uint8_t opcode = firstByte & 0x0F;
        bool masked = (secondByte & 0x80) != 0;
        uint64_t payloadLength = secondByte & 0x7F;
        
        if (payloadLength == 126) {
            if (received < pos + 2) return "";
            payloadLength = (buffer[pos] << 8) | buffer[pos + 1];
            pos += 2;
        } else if (payloadLength == 127) {
            if (received < pos + 8) return "";
            payloadLength = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLength = (payloadLength << 8) | buffer[pos++];
            }
        }
        
        std::vector<uint8_t> maskKey;
        if (masked) {
            if (received < pos + 4) return "";
            maskKey.insert(maskKey.end(), buffer.begin() + pos, buffer.begin() + pos + 4);
            pos += 4;
        }
        
        // Extract payload
        std::string payload;
        for (size_t i = 0; i < payloadLength && pos + i < received; ++i) {
            uint8_t byte = buffer[pos + i];
            if (masked) {
                byte ^= maskKey[i % 4];
            }
            payload += static_cast<char>(byte);
        }
        
        // Handle different opcodes
        if (opcode == 0x8) { // Close frame
            connected_ = false;
            return "";
        } else if (opcode == 0x9) { // Ping frame
            // Send pong
            std::vector<uint8_t> pong = {0x8A, 0x00}; // FIN=1, Opcode=0xA (pong), Length=0
            send(socket_, reinterpret_cast<const char*>(pong.data()), 2, 0);
            return receiveMessage(); // Continue receiving
        }
        
        return payload;
    }
    
    bool isConnected() const { return connected_; }
    
    void close() {
        if (socket_ != INVALID_SOCKET) {
            // Send close frame
            std::vector<uint8_t> closeFrame = {0x88, 0x00}; // FIN=1, Opcode=0x8 (close), Length=0
            send(socket_, reinterpret_cast<const char*>(closeFrame.data()), 2, 0);
            
#ifdef _WIN32
            closesocket(socket_);
#else
            ::close(socket_);
#endif
            socket_ = INVALID_SOCKET;
        }
        connected_ = false;
    }
    
private:
    SOCKET socket_;
    std::atomic<bool> connected_;
};

class WebSocketServer {
public:
    using ConnectionHandler = std::function<void(std::shared_ptr<WebSocketConnection>)>;
    
    WebSocketServer() : serverSocket_(INVALID_SOCKET), running_(false), port_(0) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    
    ~WebSocketServer() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    bool start(const std::string& host, uint16_t port, ConnectionHandler handler) {
        if (running_) return false;
        
        connectionHandler_ = handler;
        
        // Create socket
        serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket_ == INVALID_SOCKET) {
            return false;
        }
        
        // Configure socket options for address reuse
        int opt = 1;
#ifdef _WIN32
        // On Windows, use SO_EXCLUSIVEADDRUSE to prevent port stealing.
        // SO_REUSEADDR on Windows allows another socket to bind to an already-bound address,
        // which causes issues with multiple plugin instances. SO_EXCLUSIVEADDRUSE ensures
        // that bind() properly fails when another process is already using the port,
        // allowing the retry loop to find an available port.
        setsockopt(serverSocket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                  reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
        // On macOS/Linux, SO_REUSEADDR allows quick socket restarts while still
        // properly failing bind() when another process is actively listening
        setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR,
                  reinterpret_cast<const char*>(&opt), sizeof(opt));
#endif
        
        // Bind to address
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        
        // Handle localhost properly
        if (host == "127.0.0.1" || host == "localhost") {
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        } else {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        addr.sin_port = htons(port);
        
        if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            closesocket(serverSocket_);
            serverSocket_ = INVALID_SOCKET;
            return false;
        }
        
        // Get actual port if 0 was specified
        socklen_t addrLen = sizeof(addr);
        getsockname(serverSocket_, reinterpret_cast<sockaddr*>(&addr), &addrLen);
        port_ = ntohs(addr.sin_port);
        
        // Start listening
        if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(serverSocket_);
            serverSocket_ = INVALID_SOCKET;
            return false;
        }
        
        running_ = true;
        acceptThread_ = std::thread(&WebSocketServer::acceptLoop, this);
        
        return true;
    }
    
    void stop() {
        running_ = false;
        
        if (serverSocket_ != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(serverSocket_);
#else
            ::close(serverSocket_);
#endif
            serverSocket_ = INVALID_SOCKET;
        }
        
        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }
    }
    
    uint16_t getPort() const { return port_; }

    // Broadcast OSC status to all connected clients
    // This method needs to be implemented by the user of this class
    // since the server doesn't maintain the client list directly
    virtual void broadcastOSCStatus() {}

private:
    void acceptLoop() {
        while (running_) {
            sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            
            SOCKET clientSocket = accept(serverSocket_, 
                                       reinterpret_cast<sockaddr*>(&clientAddr), 
                                       &clientAddrLen);
            
            if (clientSocket == INVALID_SOCKET) {
                continue;
            }
            
            // Handle in new thread
            std::thread([this, clientSocket]() {
                handleClient(clientSocket);
            }).detach();
        }
    }
    
    void handleClient(SOCKET clientSocket) {
        char buffer[1024];
        int received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (received <= 0) {
            closesocket(clientSocket);
            return;
        }
        
        buffer[received] = '\0';
        std::string request(buffer);
        
        // Check for WebSocket upgrade request
        if (request.find("Upgrade: websocket") != std::string::npos) {
            // Extract Sec-WebSocket-Key
            std::string key;
            size_t keyPos = request.find("Sec-WebSocket-Key: ");
            if (keyPos != std::string::npos) {
                keyPos += 19;
                size_t keyEnd = request.find("\r\n", keyPos);
                key = request.substr(keyPos, keyEnd - keyPos);
            } else {
                closesocket(clientSocket);
                return;
            }
            
            // Generate accept key using SHA1+base64 (RFC 6455)
            std::string acceptKey = generateAcceptKey(key);
            
            // Send handshake response
            std::ostringstream response;
            response << "HTTP/1.1 101 Switching Protocols\r\n";
            response << "Upgrade: websocket\r\n";
            response << "Connection: Upgrade\r\n";
            response << "Sec-WebSocket-Accept: " << acceptKey << "\r\n";
            response << "\r\n";
            
            std::string responseStr = response.str();
            int sent = send(clientSocket, responseStr.c_str(), 
                          static_cast<int>(responseStr.length()), 0);
            if (sent <= 0) {
                closesocket(clientSocket);
                return;
            }
            
            // Create WebSocket connection and handle it
            auto connection = std::make_shared<WebSocketConnection>(clientSocket);
            if (connectionHandler_) {
                connectionHandler_(connection);
            }
        } else {
            closesocket(clientSocket);
        }
    }
    
    void closesocket(SOCKET socket) {
#ifdef _WIN32
        ::closesocket(socket);
#else
        ::close(socket);
#endif
    }
    
    // Simple base64 encoding for WebSocket accept key
    std::string base64Encode(const std::vector<uint8_t>& data) {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        int val = 0, valb = -6;
        for (uint8_t c : data) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                result.push_back(chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
        while (result.size() % 4) result.push_back('=');
        return result;
    }
    
    // Proper SHA-1 implementation for WebSocket handshake
    std::vector<uint8_t> sha1(const std::string& data) {
        // Simple SHA-1 implementation (RFC 3174)
        std::vector<uint8_t> hash(20);
        
        // Prepare message
        std::vector<uint8_t> msg(data.begin(), data.end());
        uint64_t msg_len = msg.size();
        msg.push_back(0x80); // append '1' bit
        
        // Pad to 64 bytes less than multiple of 512 bits
        while ((msg.size() % 64) != 56) {
            msg.push_back(0x00);
        }
        
        // Append length as 64-bit big-endian
        uint64_t bit_len = msg_len * 8;
        for (int i = 7; i >= 0; i--) {
            msg.push_back((bit_len >> (i * 8)) & 0xFF);
        }
        
        // Initialize hash values
        uint32_t h0 = 0x67452301;
        uint32_t h1 = 0xEFCDAB89;
        uint32_t h2 = 0x98BADCFE;
        uint32_t h3 = 0x10325476;
        uint32_t h4 = 0xC3D2E1F0;
        
        // Process message in 512-bit chunks
        for (size_t chunk_start = 0; chunk_start < msg.size(); chunk_start += 64) {
            uint32_t w[80];
            
            // Break chunk into sixteen 32-bit big-endian words
            for (int i = 0; i < 16; i++) {
                w[i] = (msg[chunk_start + i*4] << 24) |
                       (msg[chunk_start + i*4 + 1] << 16) |
                       (msg[chunk_start + i*4 + 2] << 8) |
                       (msg[chunk_start + i*4 + 3]);
            }
            
            // Extend the sixteen 32-bit words into eighty 32-bit words
            for (int i = 16; i < 80; i++) {
                w[i] = rotateLeft(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
            }
            
            // Initialize hash value for this chunk
            uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
            
            // Main loop
            for (int i = 0; i < 80; i++) {
                uint32_t f, k;
                if (i < 20) {
                    f = (b & c) | ((~b) & d);
                    k = 0x5A827999;
                } else if (i < 40) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1;
                } else if (i < 60) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDC;
                } else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6;
                }
                
                uint32_t temp = rotateLeft(a, 5) + f + e + k + w[i];
                e = d; d = c; c = rotateLeft(b, 30); b = a; a = temp;
            }
            
            // Add this chunk's hash to result
            h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
        }
        
        // Produce the final hash value as a 160-bit number (big-endian)
        for (int i = 0; i < 4; i++) { hash[i] = (h0 >> (24 - i*8)) & 0xFF; }
        for (int i = 0; i < 4; i++) { hash[4+i] = (h1 >> (24 - i*8)) & 0xFF; }
        for (int i = 0; i < 4; i++) { hash[8+i] = (h2 >> (24 - i*8)) & 0xFF; }
        for (int i = 0; i < 4; i++) { hash[12+i] = (h3 >> (24 - i*8)) & 0xFF; }
        for (int i = 0; i < 4; i++) { hash[16+i] = (h4 >> (24 - i*8)) & 0xFF; }
        
        return hash;
    }
    
    uint32_t rotateLeft(uint32_t value, int bits) {
        return (value << bits) | (value >> (32 - bits));
    }
    
    std::string generateAcceptKey(const std::string& key) {
        std::string combined = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        auto hash = sha1(combined);
        return base64Encode(hash);
    }
    
    SOCKET serverSocket_;
    std::atomic<bool> running_;
    std::atomic<uint16_t> port_;
    std::thread acceptThread_;
    ConnectionHandler connectionHandler_;
};

} // namespace SimpleWS