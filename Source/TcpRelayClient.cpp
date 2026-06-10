// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2026

#include "TcpRelayClient.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#if defined(__APPLE__)
#include <sys/socket.h>
#elif JUCE_WINDOWS
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#if defined(COOKIELINK_RELAY_USE_OPENSSL)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

using namespace cookielink::relay;

namespace {

static std::atomic<uint32_t> sRelaySessionId { 1 };

static bool envEnabled(const char* name)
{
    if (const char* value = std::getenv(name)) {
        return *value != '\0' && std::strcmp(value, "0") != 0;
    }
    return false;
}

static bool relayDebugEnabled()
{
    return envEnabled("COOKIELINK_RELAY_DEBUG");
}

static bool relayFrameLogEnabled()
{
    return envEnabled("COOKIELINK_RELAY_LOG_FRAMES");
}

static bool relayDataLogEnabled()
{
    return envEnabled("COOKIELINK_RELAY_LOG_DATA");
}

static juce::String relayTimestamp()
{
    return juce::Time::getCurrentTime().toString(true, true, true, true);
}

static void relayDebugLog(uint32_t sessionId, const juce::String& message)
{
    if (!relayDebugEnabled()) {
        return;
    }
    juce::Logger::writeToLog(relayTimestamp() + " [relay-client " + juce::String(sessionId) + "] " + message);
}

static juce::String relayMessageName(MessageType type)
{
    switch (type) {
        case MsgHello: return "hello";
        case MsgWelcome: return "welcome";
        case MsgJoinGroup: return "join-group";
        case MsgJoinResult: return "join-result";
        case MsgPeerJoin: return "peer-join";
        case MsgPeerLeave: return "peer-leave";
        case MsgData: return "data";
        case MsgError: return "error";
        case MsgLeaveGroup: return "leave-group";
        case MsgPublicGroupsRequest: return "public-groups-request";
        case MsgPublicGroupsUpdate: return "public-groups-update";
        default: break;
    }
    return "unknown";
}

static juce::String formatErrno(int errnum)
{
    if (errnum == 0) {
        return {};
    }
    return " errno=" + juce::String(errnum) + " (" + juce::String(std::strerror(errnum)) + ")";
}

static bool isTransientSocketErr(int errnum)
{
    return errnum == EINTR || errnum == EAGAIN || errnum == EWOULDBLOCK || errnum == EINPROGRESS;
}

static constexpr int kMaxSendQueueBytes = 1024 * 1024;
static constexpr double kRelayQueueTargetMs = 120.0;
static constexpr double kRelayQueueHardMs = 150.0;
static constexpr double kRelayCongestedQueueMs = 60.0;
static constexpr double kRelayCongestedWriteMs = 25.0;

static void disableSigPipe(juce::StreamingSocket& sock)
{
#if defined(__APPLE__)
    const int fd = sock.getRawSocketHandle();
    if (fd >= 0) {
        int set = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
    }
#else
    ignoreUnused(sock);
#endif
}

static void tuneRelaySocket(juce::StreamingSocket& sock)
{
    const int fd = sock.getRawSocketHandle();
    if (fd < 0) {
        return;
    }

    int set = 1;

#if defined(__APPLE__)
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
#endif

#if defined(TCP_NODELAY)
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&set), sizeof(set));
#endif

#if defined(SO_SNDBUF)
    int sndbuf = 256 * 1024;
    ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sndbuf), sizeof(sndbuf));
#endif
#if defined(SO_RCVBUF)
    int rcvbuf = 256 * 1024;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf), sizeof(rcvbuf));
#endif
}

static void writeU16(juce::MemoryOutputStream& out, uint16_t value)
{
    const uint8_t bytes[2] = {
        static_cast<uint8_t>((value >> 8) & 0xff),
        static_cast<uint8_t>(value & 0xff)
    };
    out.write(bytes, 2);
}

static void writeU32(juce::MemoryOutputStream& out, uint32_t value)
{
    const uint8_t bytes[4] = {
        static_cast<uint8_t>((value >> 24) & 0xff),
        static_cast<uint8_t>((value >> 16) & 0xff),
        static_cast<uint8_t>((value >> 8) & 0xff),
        static_cast<uint8_t>(value & 0xff)
    };
    out.write(bytes, 4);
}

static bool readU16(const uint8_t* data, int size, int& offset, uint16_t& value)
{
    if (offset + 2 > size) {
        return false;
    }
    value = (static_cast<uint16_t>(data[offset]) << 8)
        | static_cast<uint16_t>(data[offset + 1]);
    offset += 2;
    return true;
}

static bool readU32(const uint8_t* data, int size, int& offset, uint32_t& value)
{
    if (offset + 4 > size) {
        return false;
    }
    value = (static_cast<uint32_t>(data[offset]) << 24)
        | (static_cast<uint32_t>(data[offset + 1]) << 16)
        | (static_cast<uint32_t>(data[offset + 2]) << 8)
        | static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    return true;
}

static void writeString(juce::MemoryOutputStream& out, const juce::String& text)
{
    auto bytes = text.toRawUTF8();
    auto len = static_cast<uint16_t>(std::min<size_t>(std::strlen(bytes), 0xffff));
    writeU16(out, len);
    out.write(bytes, len);
}

static juce::MemoryBlock buildWireFrame(const juce::MemoryBlock& frame)
{
    juce::MemoryOutputStream out;
    writeU32(out, static_cast<uint32_t>(frame.getSize()));
    out.write(frame.getData(), frame.getSize());
    return out.getMemoryBlock();
}

static bool readString(const uint8_t* data, int size, int& offset, juce::String& text)
{
    uint16_t len = 0;
    if (!readU16(data, size, offset, len)) {
        return false;
    }
    if (offset + len > size) {
        return false;
    }
    text = juce::String::fromUTF8(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return true;
}

static void setErr(juce::String* err, const juce::String& message)
{
    if (err) {
        *err = message;
    }
}

} // namespace

TcpRelayClient::TcpRelayClient(TcpRelayClientListener& owner)
: Thread("CookieLinkTcpRelayClient"),
  listener(owner)
{
}

class TcpRelayClient::WriteThread : public juce::Thread
{
public:
    explicit WriteThread(TcpRelayClient& ownerIn)
        : Thread("CookieLinkTcpRelayWriter"), owner(ownerIn)
    {}

    void run() override
    {
        setPriority(Priority::high);
        owner.runWriter(*this);
    }

private:
    TcpRelayClient& owner;
};

TcpRelayClient::~TcpRelayClient()
{
    disconnect();
}

struct TcpRelayClient::TlsState {
    bool enabled = false;
    bool verifyPeer = false;
    juce::String caPath;
#if defined(COOKIELINK_RELAY_USE_OPENSSL)
    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
#endif
};

bool TcpRelayClient::connectToServer(const juce::String& host, int port, const juce::String& username, const juce::String& password)
{
    disconnect();
    resetStats();
    sessionId = sRelaySessionId.fetch_add(1, std::memory_order_relaxed);
    relayDebugLog(sessionId, "connect requested host=" + host + " port=" + juce::String(port)
                  + " user=" + username
                  + " tls=" + juce::String(pendingUseTls ? "yes" : "no")
                  + " verify=" + juce::String(pendingVerifyTls ? "yes" : "no"));

    {
        const juce::ScopedLock sl(stateLock);
        pendingHost = host;
        pendingPort = port;
        pendingUsername = username;
        pendingUserPassword = password;
    }

    connectionRequested.store(true);
    startThread();
    return true;
}

void TcpRelayClient::disconnect()
{
    connectionRequested.store(false);
    {
        const juce::ScopedLock sl(stateLock);
        pendingGroup.clear();
        pendingGroupPassword.clear();
        pendingGroupIsPublic = false;
    }

    stopWriterThread();
    clearQueuedDataFrames();

    if (isThreadRunning()) {
        signalThreadShouldExit();
    }

    if (connected.load() || isThreadRunning()) {
        relayDebugLog(sessionId, "disconnect requested");
        if (!relayDebugEnabled()) {
            juce::Logger::writeToLog("Relay disconnect requested");
        }
    }

    std::shared_ptr<juce::StreamingSocket> sock;
    {
        const juce::ScopedLock sl(socketLock);
        sock = socket;
    }
    {
        const juce::ScopedLock sl(writeLock);
        if (sock) {
            sock->close();
        }
    }

    stopThread(800);

    connected.store(false);
    clientId = 0;

    {
        const juce::ScopedLock sl(socketLock);
        socket.reset();
    }

    if (tlsState) {
#if defined(COOKIELINK_RELAY_USE_OPENSSL)
        if (tlsState->ssl) {
            SSL_shutdown(tlsState->ssl);
            SSL_free(tlsState->ssl);
        }
        if (tlsState->ctx) {
            SSL_CTX_free(tlsState->ctx);
        }
#endif
        tlsState.reset();
    }
}

void TcpRelayClient::setTlsOptions(bool useTls, bool verifyPeer, const juce::String& caPath)
{
    const juce::ScopedLock sl(stateLock);
    pendingUseTls = useTls;
    pendingVerifyTls = verifyPeer;
    pendingTlsCaPath = caPath;
}

void TcpRelayClient::setWatchPublicGroups(bool flag)
{
    {
        const juce::ScopedLock sl(stateLock);
        pendingWatchPublicGroups = flag;
    }
    if (connected.load()) {
        sendPublicGroupsRequest(flag);
    }
}

TcpRelayClient::RelayStats TcpRelayClient::getStats()
{
    RelayStats stats;
    stats.connected = connected.load();
    stats.droppedQueuedPackets = droppedQueuedPackets.load(std::memory_order_relaxed);
    stats.droppedStalePackets = droppedStalePackets.load(std::memory_order_relaxed);

    {
        const juce::ScopedLock sl(sendQueueLock);
        stats.queuedFrames = static_cast<int>(sendQueue.size());
        stats.queuedBytes = queuedDataBytes;
        if (!sendQueue.empty()) {
            stats.oldestQueuedMs = juce::jmax(0.0, juce::Time::getMillisecondCounterHiRes() - sendQueue.front().enqueuedMs);
        }
    }

    {
        const juce::ScopedLock sl(statsLock);
        stats.lastWriteBlockMs = lastWriteBlockMs;
        stats.avgWriteBlockMs = avgWriteBlockMs;
        stats.maxWriteBlockMs = maxWriteBlockMs;
    }

    stats.congested = stats.oldestQueuedMs >= kRelayCongestedQueueMs
                      || stats.lastWriteBlockMs >= kRelayCongestedWriteMs
                      || stats.queuedFrames >= 3;
    return stats;
}

bool TcpRelayClient::joinGroup(const juce::String& group, const juce::String& password, bool isPublic)
{
    if (connected.load()) {
        return sendJoinGroupMessage(group, password, isPublic);
    }

    {
        const juce::ScopedLock sl(stateLock);
        pendingGroup = group;
        pendingGroupPassword = password;
        pendingGroupIsPublic = isPublic;
    }

    return true;
}

bool TcpRelayClient::leaveGroup(const juce::String& group)
{
    {
        const juce::ScopedLock sl(stateLock);
        if (group.isEmpty() || group == pendingGroup) {
            pendingGroup.clear();
            pendingGroupPassword.clear();
            pendingGroupIsPublic = false;
        }
    }
    if (!connected.load()) {
        return false;
    }
    return sendLeaveGroupMessage(group);
}

bool TcpRelayClient::sendPacket(uint32_t destId, const char* data, int32_t size)
{
    if (!connected.load() || size <= 0 || data == nullptr) {
        return false;
    }

    juce::MemoryOutputStream payload;
    payload.writeByte(static_cast<char>(MsgData));
    writeU32(payload, clientId);
    writeU32(payload, destId);
    writeU32(payload, static_cast<uint32_t>(size));
    payload.write(data, size);

    return sendFrame(payload.getMemoryBlock());
}

void TcpRelayClient::run()
{
    juce::String host;
    int port = 0;
    juce::String username;
    juce::String password;
    bool useTls = false;
    bool verifyTls = false;
    juce::String caPath;
    bool watchPublicGroups = false;

    {
        const juce::ScopedLock sl(stateLock);
        host = pendingHost;
        port = pendingPort;
        username = pendingUsername;
        password = pendingUserPassword;
        useTls = pendingUseTls;
        verifyTls = pendingVerifyTls;
        caPath = pendingTlsCaPath;
        watchPublicGroups = pendingWatchPublicGroups;
    }

    auto resetSocket = [this]() {
        const juce::ScopedLock sl(socketLock);
        socket.reset();
    };

    if (host.isEmpty() || port == 0) {
        listener.handleRelayConnected(false, "relay host/port invalid");
        return;
    }

    auto newSocket = std::make_shared<juce::StreamingSocket>();
    if (!newSocket->connect(host, port, 5000)) {
        listener.handleRelayConnected(false, "relay connect failed");
        return;
    }
    disableSigPipe(*newSocket);
    tuneRelaySocket(*newSocket);
    {
        const juce::ScopedLock sl(socketLock);
        socket = newSocket;
    }
    if (useTls) {
#if defined(COOKIELINK_RELAY_USE_OPENSSL)
        tlsState = std::make_unique<TlsState>();
        tlsState->enabled = true;
        tlsState->verifyPeer = verifyTls;
        tlsState->caPath = caPath;

        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        tlsState->ctx = SSL_CTX_new(TLS_client_method());
        if (!tlsState->ctx) {
            listener.handleRelayConnected(false, "relay TLS context failed");
            resetSocket();
            tlsState.reset();
            return;
        }

        if (verifyTls) {
            SSL_CTX_set_verify(tlsState->ctx, SSL_VERIFY_PEER, nullptr);
            if (caPath.isNotEmpty()) {
                if (SSL_CTX_load_verify_locations(tlsState->ctx, caPath.toRawUTF8(), nullptr) != 1) {
                    listener.handleRelayConnected(false, "relay TLS CA load failed");
                    resetSocket();
                    tlsState.reset();
                    return;
                }
            }
            else {
                SSL_CTX_set_default_verify_paths(tlsState->ctx);
            }
        }
        else {
            SSL_CTX_set_verify(tlsState->ctx, SSL_VERIFY_NONE, nullptr);
        }

        tlsState->ssl = SSL_new(tlsState->ctx);
        if (!tlsState->ssl) {
            listener.handleRelayConnected(false, "relay TLS session failed");
            resetSocket();
            tlsState.reset();
            return;
        }

        SSL_set_fd(tlsState->ssl, newSocket->getRawSocketHandle());
        SSL_set_tlsext_host_name(tlsState->ssl, host.toRawUTF8());

        if (SSL_connect(tlsState->ssl) != 1) {
            listener.handleRelayConnected(false, "relay TLS handshake failed");
            resetSocket();
            tlsState.reset();
            return;
        }
#else
        listener.handleRelayConnected(false, "relay TLS not supported in this build");
        resetSocket();
        return;
#endif
    }

    if (!sendHello()) {
        listener.handleRelayConnected(false, "relay hello failed");
        resetSocket();
        return;
    }
    relayDebugLog(sessionId, "hello sent");

    juce::MemoryBlock frame;
    juce::String readErr;
    if (!readFrame(frame, &readErr)) {
        if (readErr.isNotEmpty()) {
            listener.handleRelayConnected(false, "relay handshake failed: " + readErr);
        } else {
            listener.handleRelayConnected(false, "relay handshake failed");
        }
        resetSocket();
        return;
    }

    // expect welcome or error
    const auto* data = static_cast<const uint8_t*>(frame.getData());
    const int size = static_cast<int>(frame.getSize());
    if (size < 1) {
        listener.handleRelayConnected(false, "relay handshake invalid");
        resetSocket();
        return;
    }

    const auto type = static_cast<MessageType>(data[0]);
    if (type == MsgWelcome) {
        int offset = 1;
        uint16_t version = 0;
        uint32_t id = 0;
        if (!readU16(data, size, offset, version) || !readU32(data, size, offset, id)) {
            listener.handleRelayConnected(false, "relay handshake invalid");
            resetSocket();
            return;
        }
        if (version != kProtocolVersion) {
            listener.handleRelayConnected(false, "relay protocol mismatch");
            resetSocket();
            return;
        }
        clientId = id;
        connected.store(true);
        startWriterThread();
        relayDebugLog(sessionId, "welcome received clientId=" + juce::String(clientId));
        listener.handleRelayConnected(true, "");

        juce::String group;
        juce::String grouppass;
        bool isPublic = false;
        {
            const juce::ScopedLock sl(stateLock);
            group = pendingGroup;
            grouppass = pendingGroupPassword;
            isPublic = pendingGroupIsPublic;
        }
        if (group.isNotEmpty()) {
            relayDebugLog(sessionId, "auto-joining group=" + group + " public=" + juce::String(isPublic ? "yes" : "no"));
            sendJoinGroupMessage(group, grouppass, isPublic);
        }
        if (watchPublicGroups) {
            relayDebugLog(sessionId, "requesting public groups watch");
            sendPublicGroupsRequest(true);
        }
    }
    else if (type == MsgError) {
        int offset = 1;
        juce::String message;
        if (readString(data, size, offset, message)) {
            listener.handleRelayConnected(false, message);
            relayDebugLog(sessionId, "welcome error=" + message);
        } else {
            listener.handleRelayConnected(false, "relay error");
            relayDebugLog(sessionId, "welcome error=relay error");
        }
        resetSocket();
        return;
    }
    else {
        listener.handleRelayConnected(false, "relay handshake invalid");
        resetSocket();
        return;
    }

    juce::String disconnectReason;
    while (!threadShouldExit()) {
        juce::MemoryBlock msg;
        juce::String readErr;
        if (!readFrame(msg, &readErr)) {
            if (readErr.isNotEmpty()) {
                disconnectReason = readErr;
            }
            break;
        }
        handleFrame(msg);
    }

    connected.store(false);
    stopWriterThread();
    clearQueuedDataFrames();
    resetSocket();

    if (disconnectReason.isEmpty()) {
        disconnectReason = "relay connection lost";
    }
    relayDebugLog(sessionId, "disconnected reason=" + disconnectReason);
    juce::Logger::writeToLog("Relay disconnected: " + disconnectReason);
    listener.handleRelayDisconnected(disconnectReason);
}

bool TcpRelayClient::sendHello()
{
    juce::String username;
    juce::String password;
    {
        const juce::ScopedLock sl(stateLock);
        username = pendingUsername;
        password = pendingUserPassword;
    }

    juce::MemoryOutputStream payload;
    payload.writeByte(static_cast<char>(MsgHello));
    writeU16(payload, kProtocolVersion);
    writeString(payload, username);
    writeString(payload, password);

    return sendFrame(payload.getMemoryBlock());
}

bool TcpRelayClient::sendJoinGroupMessage(const juce::String& group, const juce::String& password, bool isPublic)
{
    relayDebugLog(sessionId, "send join-group group=" + group + " public=" + juce::String(isPublic ? "yes" : "no"));
    juce::MemoryOutputStream payload;
    payload.writeByte(static_cast<char>(MsgJoinGroup));
    writeString(payload, group);
    writeString(payload, password);
    payload.writeByte(isPublic ? 1 : 0);
    return sendFrame(payload.getMemoryBlock());
}

bool TcpRelayClient::sendLeaveGroupMessage(const juce::String& group)
{
    relayDebugLog(sessionId, "send leave-group group=" + group);
    juce::MemoryOutputStream payload;
    payload.writeByte(static_cast<char>(MsgLeaveGroup));
    writeString(payload, group);
    return sendFrame(payload.getMemoryBlock());
}

bool TcpRelayClient::sendPublicGroupsRequest(bool watch)
{
    relayDebugLog(sessionId, "send public-groups-request watch=" + juce::String(watch ? "yes" : "no"));
    juce::MemoryOutputStream payload;
    payload.writeByte(static_cast<char>(MsgPublicGroupsRequest));
    payload.writeByte(watch ? 1 : 0);
    return sendFrame(payload.getMemoryBlock());
}

bool TcpRelayClient::sendFrame(const juce::MemoryBlock& frame)
{
    if (frame.getSize() == 0 || frame.getSize() > kMaxFrameSize) {
        return false;
    }

    if (relayFrameLogEnabled() && frame.getSize() >= 1) {
        const auto* data = static_cast<const uint8_t*>(frame.getData());
        const auto type = static_cast<MessageType>(data[0]);
        if (type != MsgData || relayDataLogEnabled()) {
            juce::String msg = "send frame type=" + relayMessageName(type)
                               + " size=" + juce::String(static_cast<int64_t>(frame.getSize()));
            if (type == MsgData && frame.getSize() >= 13) {
                int offset = 1;
                uint32_t srcId = 0;
                uint32_t destId = 0;
                uint32_t payloadLen = 0;
                if (readU32(data, static_cast<int>(frame.getSize()), offset, srcId)
                    && readU32(data, static_cast<int>(frame.getSize()), offset, destId)
                    && readU32(data, static_cast<int>(frame.getSize()), offset, payloadLen)) {
                    msg << " src=" << juce::String(static_cast<uint64_t>(srcId))
                        << " dest=" << juce::String(static_cast<uint64_t>(destId))
                        << " payload=" << juce::String(static_cast<uint64_t>(payloadLen));
                }
            }
            relayDebugLog(sessionId, msg);
        }
    }

    const auto wireFrame = buildWireFrame(frame);
    const bool isData = frame.getSize() >= 1
        && static_cast<const uint8_t*>(frame.getData())[0] == static_cast<uint8_t>(MsgData);

    if (isData) {
        if (!connected.load() || writeThread == nullptr) {
            return false;
        }
        return enqueueDataFrame(wireFrame);
    }

    return writeWireFrameBlocking(wireFrame);
}

bool TcpRelayClient::writeWireFrameBlocking(const juce::MemoryBlock& wireFrame)
{
    std::shared_ptr<juce::StreamingSocket> sock;
    {
        const juce::ScopedLock sl(socketLock);
        sock = socket;
    }
    if (!sock) {
        return false;
    }
    if (wireFrame.getSize() == 0) {
        return false;
    }

    struct WriteTimer {
        TcpRelayClient& owner;
        double startedMs;
        ~WriteTimer()
        {
            owner.recordWriteBlockMs(juce::Time::getMillisecondCounterHiRes() - startedMs);
        }
    } writeTimer { *this, juce::Time::getMillisecondCounterHiRes() };

    const juce::ScopedLock sl(writeLock);
    const auto totalSize = static_cast<int>(wireFrame.getSize());
    const auto* bytes = static_cast<const char*>(wireFrame.getData());
    int offset = 0;
    while (offset < totalSize) {
        int written = 0;
#if defined(COOKIELINK_RELAY_USE_OPENSSL)
        if (tlsState && tlsState->enabled && tlsState->ssl) {
            written = SSL_write(tlsState->ssl, bytes + offset, totalSize - offset);
            if (written <= 0) {
                const int sslErr = SSL_get_error(tlsState->ssl, written);
                if (sslErr == SSL_ERROR_WANT_READ) {
                    sock->waitUntilReady(true, 50);
                    continue;
                }
                if (sslErr == SSL_ERROR_WANT_WRITE) {
                    sock->waitUntilReady(false, 50);
                    continue;
                }
                juce::Logger::writeToLog("Relay TLS write failed: ssl_err=" + juce::String(sslErr));
                return false;
            }
        } else
#endif
        {
            errno = 0;
            written = sock->write(bytes + offset, totalSize - offset);
            if (written <= 0) {
                const int errnum = errno;
                if (isTransientSocketErr(errnum)) {
                    const auto ready = sock->waitUntilReady(false, 50);
                    if (ready < 0 && !sock->isConnected()) {
                        connected.store(false);
                        return false;
                    }
                    continue;
                }
                const bool wasConnected = connected.exchange(false);
                sock->close();
                if (wasConnected) {
                    juce::String message = "Relay write failed";
                    message << formatErrno(errnum)
                            << " connected=" << (sock->isConnected() ? "yes" : "no");
                    juce::Logger::writeToLog(message);
                }
                return false;
            }
        }
        offset += written;
    }
    return true;
}

int TcpRelayClient::dropQueuedDataFramesOlderThan(double nowMs, double maxAgeMs)
{
    int dropped = 0;
    while (!sendQueue.empty()) {
        const double ageMs = nowMs - sendQueue.front().enqueuedMs;
        if (ageMs < maxAgeMs) {
            break;
        }
        queuedDataBytes -= static_cast<int>(sendQueue.front().frame.getSize());
        sendQueue.pop_front();
        ++dropped;
    }
    if (queuedDataBytes < 0) {
        queuedDataBytes = 0;
    }
    if (dropped > 0) {
        droppedStalePackets.fetch_add(dropped, std::memory_order_relaxed);
    }
    return dropped;
}

bool TcpRelayClient::enqueueDataFrame(const juce::MemoryBlock& wireFrame)
{
    if (!connected.load() || wireFrame.getSize() == 0) {
        return false;
    }

    const size_t frameSize = wireFrame.getSize();
    if (kMaxSendQueueBytes > 0 && frameSize > static_cast<size_t>(kMaxSendQueueBytes)) {
        return false;
    }

    {
        const juce::ScopedLock sl(sendQueueLock);
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        dropQueuedDataFramesOlderThan(nowMs, kRelayQueueTargetMs);

        if (kMaxSendQueueBytes > 0 && queuedDataBytes + static_cast<int>(frameSize) > kMaxSendQueueBytes) {
            while (!sendQueue.empty()
                   && queuedDataBytes + static_cast<int>(frameSize) > kMaxSendQueueBytes) {
                queuedDataBytes -= static_cast<int>(sendQueue.front().frame.getSize());
                sendQueue.pop_front();
                droppedQueuedPackets.fetch_add(1, std::memory_order_relaxed);
            }
            if (queuedDataBytes + static_cast<int>(frameSize) > kMaxSendQueueBytes) {
                droppedQueuedPackets.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        }
        sendQueue.push_back(QueuedFrame { wireFrame, nowMs });
        queuedDataBytes += static_cast<int>(frameSize);
    }

    sendQueueEvent.signal();
    return true;
}

void TcpRelayClient::runWriter(juce::Thread& thread)
{
    while (!thread.threadShouldExit()) {
        QueuedFrame item;
        {
            const juce::ScopedLock sl(sendQueueLock);
            if (!sendQueue.empty()) {
                item = std::move(sendQueue.front());
                sendQueue.pop_front();
                queuedDataBytes -= static_cast<int>(item.frame.getSize());
                if (queuedDataBytes < 0) {
                    queuedDataBytes = 0;
                }
            }
        }

        if (item.frame.getSize() == 0) {
            sendQueueEvent.wait(25);
            continue;
        }

        const double ageMs = juce::Time::getMillisecondCounterHiRes() - item.enqueuedMs;
        if (ageMs >= kRelayQueueHardMs) {
            droppedStalePackets.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        if (!writeWireFrameBlocking(item.frame)) {
            break;
        }
    }
}

void TcpRelayClient::recordWriteBlockMs(double elapsedMs)
{
    const juce::ScopedLock sl(statsLock);
    lastWriteBlockMs = juce::jmax(0.0, elapsedMs);
    maxWriteBlockMs = juce::jmax(maxWriteBlockMs, lastWriteBlockMs);
    if (writeBlockSamples <= 0) {
        avgWriteBlockMs = lastWriteBlockMs;
        writeBlockSamples = 1;
    } else {
        const double weight = writeBlockSamples < 64 ? 1.0 / static_cast<double>(writeBlockSamples + 1) : 0.04;
        avgWriteBlockMs += (lastWriteBlockMs - avgWriteBlockMs) * weight;
        ++writeBlockSamples;
    }
}

void TcpRelayClient::resetStats()
{
    droppedQueuedPackets.store(0, std::memory_order_relaxed);
    droppedStalePackets.store(0, std::memory_order_relaxed);
    const juce::ScopedLock sl(statsLock);
    lastWriteBlockMs = 0.0;
    avgWriteBlockMs = 0.0;
    maxWriteBlockMs = 0.0;
    writeBlockSamples = 0;
}

void TcpRelayClient::startWriterThread()
{
    stopWriterThread();
    writeThread = std::make_unique<WriteThread>(*this);
    writeThread->startThread();
}

void TcpRelayClient::stopWriterThread()
{
    if (writeThread != nullptr) {
        writeThread->signalThreadShouldExit();
        sendQueueEvent.signal();
        writeThread->stopThread(500);
        writeThread.reset();
    }
}

void TcpRelayClient::clearQueuedDataFrames()
{
    const juce::ScopedLock sl(sendQueueLock);
    sendQueue.clear();
    queuedDataBytes = 0;
}

bool TcpRelayClient::readExact(void* dest, int bytes, juce::String* err)
{
    std::shared_ptr<juce::StreamingSocket> sock;
    {
        const juce::ScopedLock sl(socketLock);
        sock = socket;
    }
    if (!sock) {
        setErr(err, "relay socket missing");
        return false;
    }

    int offset = 0;
    auto* out = static_cast<char*>(dest);
    while (offset < bytes && !threadShouldExit()) {
        int got = 0;
#if defined(COOKIELINK_RELAY_USE_OPENSSL)
        if (tlsState && tlsState->enabled && tlsState->ssl) {
            got = SSL_read(tlsState->ssl, out + offset, bytes - offset);
            if (got <= 0) {
                const int sslErr = SSL_get_error(tlsState->ssl, got);
                if (sslErr == SSL_ERROR_WANT_READ) {
                    sock->waitUntilReady(true, 50);
                    continue;
                }
                if (sslErr == SSL_ERROR_WANT_WRITE) {
                    sock->waitUntilReady(false, 50);
                    continue;
                }
                setErr(err, "relay TLS read failed (ssl_err=" + juce::String(sslErr) + ")");
                return false;
            }
        } else
#endif
        {
            errno = 0;
            got = sock->read(out + offset, bytes - offset, true);
            if (got <= 0) {
                const int errnum = errno;
                const auto detail = formatErrno(errnum);
                if (errnum == 0) {
                    setErr(err, "relay connection closed");
                    return false;
                }
                if (isTransientSocketErr(errnum)) {
                    // treat transient errors like a retry to avoid disconnect loops
                    const auto ready = sock->waitUntilReady(true, 100);
                    if (ready < 0 && !sock->isConnected()) {
                        setErr(err, "relay connection closed" + detail);
                        return false;
                    }
                    continue;
                }
                if (!sock->isConnected()) {
                    setErr(err, "relay connection closed" + detail);
                } else {
                    setErr(err, "relay socket read failed" + detail);
                }
                return false;
            }
        }
        offset += got;
    }
    if (threadShouldExit()) {
        setErr(err, "relay read cancelled");
        return false;
    }
    return offset == bytes;
}

bool TcpRelayClient::readFrame(juce::MemoryBlock& frame, juce::String* err)
{
    uint8_t lenBytes[4];
    if (!readExact(lenBytes, 4, err)) {
        return false;
    }

    uint32_t length = (static_cast<uint32_t>(lenBytes[0]) << 24)
        | (static_cast<uint32_t>(lenBytes[1]) << 16)
        | (static_cast<uint32_t>(lenBytes[2]) << 8)
        | static_cast<uint32_t>(lenBytes[3]);

    if (length == 0 || length > kMaxFrameSize) {
        setErr(err, "relay frame length invalid");
        return false;
    }

    frame.setSize(length);
    return readExact(frame.getData(), static_cast<int>(length), err);
}

void TcpRelayClient::handleFrame(const juce::MemoryBlock& frame)
{
    const auto* data = static_cast<const uint8_t*>(frame.getData());
    const int size = static_cast<int>(frame.getSize());
    if (size < 1) {
        return;
    }

    const auto type = static_cast<MessageType>(data[0]);
    int offset = 1;

    switch (type) {
        case MsgJoinResult:
        {
            uint8_t success = 0;
            if (offset >= size) {
                break;
            }
            success = data[offset++];
            juce::String message;
            readString(data, size, offset, message);
            if (success != 0) {
                juce::Logger::writeToLog("Relay group join ok");
                relayDebugLog(sessionId, "join-result success");
            } else {
                juce::Logger::writeToLog("Relay group join failed: " + message);
                relayDebugLog(sessionId, "join-result failed message=" + message);
            }
            listener.handleRelayGroupJoined(success != 0, message);
            break;
        }
        case MsgPeerJoin:
        {
            uint32_t peerId = 0;
            juce::String name;
            if (!readU32(data, size, offset, peerId)) {
                break;
            }
            if (!readString(data, size, offset, name)) {
                break;
            }
            relayDebugLog(sessionId, "peer-join id=" + juce::String(peerId) + " user=" + name);
            listener.handleRelayPeerJoin(peerId, name);
            break;
        }
        case MsgPeerLeave:
        {
            uint32_t peerId = 0;
            if (!readU32(data, size, offset, peerId)) {
                break;
            }
            relayDebugLog(sessionId, "peer-leave id=" + juce::String(peerId));
            listener.handleRelayPeerLeave(peerId);
            break;
        }
        case MsgData:
        {
            uint32_t srcId = 0;
            uint32_t destId = 0;
            uint32_t payloadLen = 0;
            if (!readU32(data, size, offset, srcId) || !readU32(data, size, offset, destId) || !readU32(data, size, offset, payloadLen)) {
                break;
            }
            if (payloadLen == 0 || offset + static_cast<int>(payloadLen) > size) {
                break;
            }
            if (relayFrameLogEnabled() && relayDataLogEnabled()) {
                relayDebugLog(sessionId, "recv data src=" + juce::String(srcId)
                              + " dest=" + juce::String(destId)
                              + " payload=" + juce::String(static_cast<int64_t>(payloadLen)));
            }
            listener.handleRelayData(srcId, reinterpret_cast<const char*>(data + offset), static_cast<int>(payloadLen));
            break;
        }
        case MsgError:
        {
            juce::String message;
            if (readString(data, size, offset, message)) {
                relayDebugLog(sessionId, "relay error=" + message);
                listener.handleRelayError(message);
            }
            break;
        }
        case MsgPublicGroupsUpdate:
        {
            uint16_t count = 0;
            if (!readU16(data, size, offset, count)) {
                break;
            }
            juce::StringArray groups;
            juce::Array<int> counts;
            for (uint16_t i = 0; i < count; ++i) {
                juce::String name;
                uint32_t gcount = 0;
                if (!readString(data, size, offset, name) || !readU32(data, size, offset, gcount)) {
                    break;
                }
                groups.add(name);
                counts.add(static_cast<int>(gcount));
            }
            relayDebugLog(sessionId, "public groups update count=" + juce::String(count));
            listener.handleRelayPublicGroups(groups, counts);
            break;
        }
        default:
            break;
    }
}
