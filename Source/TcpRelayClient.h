#pragma once

#include "JuceHeader.h"
#include "RelayProtocol.h"

#include <deque>

class TcpRelayClientListener
{
public:
    virtual ~TcpRelayClientListener() = default;

    virtual void handleRelayData(uint32_t srcId, const char* data, int size) = 0;
    virtual void handleRelayConnected(bool success, const juce::String& errmesg) = 0;
    virtual void handleRelayDisconnected(const juce::String& errmesg) = 0;
    virtual void handleRelayGroupJoined(bool success, const juce::String& errmesg) = 0;
    virtual void handleRelayPeerJoin(uint32_t peerId, const juce::String& user) = 0;
    virtual void handleRelayPeerLeave(uint32_t peerId) = 0;
    virtual void handleRelayError(const juce::String& errmesg) = 0;
    virtual void handleRelayPublicGroups(const juce::StringArray& groups, const juce::Array<int>& counts) = 0;
};

class TcpRelayClient : public juce::Thread
{
public:
    explicit TcpRelayClient(TcpRelayClientListener& owner);
    ~TcpRelayClient() override;

    bool connectToServer(const juce::String& host, int port, const juce::String& username, const juce::String& password);
    void disconnect();

    bool isConnected() const { return connected.load(); }

    void setTlsOptions(bool useTls, bool verifyPeer, const juce::String& caPath);
    void setWatchPublicGroups(bool flag);

    bool joinGroup(const juce::String& group, const juce::String& password, bool isPublic);
    bool leaveGroup(const juce::String& group);

    bool sendPacket(uint32_t destId, const char* data, int32_t size);

    uint32_t getClientId() const { return clientId; }

    struct RelayStats
    {
        bool connected = false;
        int queuedFrames = 0;
        int queuedBytes = 0;
        double oldestQueuedMs = 0.0;
        int droppedQueuedPackets = 0;
        int droppedStalePackets = 0;
        double lastWriteBlockMs = 0.0;
        double avgWriteBlockMs = 0.0;
        double maxWriteBlockMs = 0.0;
        bool congested = false;
    };

    RelayStats getStats();

private:
    struct TlsState;
    class WriteThread;
    struct QueuedFrame
    {
        juce::MemoryBlock frame;
        double enqueuedMs = 0.0;
    };

    void run() override;

    bool sendHello();
    bool sendJoinGroupMessage(const juce::String& group, const juce::String& password, bool isPublic);
    bool sendLeaveGroupMessage(const juce::String& group);
    bool sendPublicGroupsRequest(bool watch);

    bool sendFrame(const juce::MemoryBlock& frame);
    bool writeWireFrameBlocking(const juce::MemoryBlock& wireFrame);
    bool enqueueDataFrame(const juce::MemoryBlock& frame);
    void runWriter(juce::Thread& thread);
    int dropQueuedDataFramesOlderThan(double nowMs, double maxAgeMs);
    void recordWriteBlockMs(double elapsedMs);
    void resetStats();
    void startWriterThread();
    void stopWriterThread();
    void clearQueuedDataFrames();
    bool readFrame(juce::MemoryBlock& frame, juce::String* err = nullptr);
    bool readExact(void* dest, int bytes, juce::String* err = nullptr);

    void handleFrame(const juce::MemoryBlock& frame);

    TcpRelayClientListener& listener;

    juce::CriticalSection stateLock;
    juce::CriticalSection writeLock;
    juce::CriticalSection socketLock;
    juce::CriticalSection sendQueueLock;
    juce::CriticalSection statsLock;
    juce::WaitableEvent sendQueueEvent;

    std::shared_ptr<juce::StreamingSocket> socket;
    std::unique_ptr<TlsState> tlsState;
    std::unique_ptr<WriteThread> writeThread;
    std::deque<QueuedFrame> sendQueue;
    int queuedDataBytes = 0;

    juce::String pendingHost;
    int pendingPort = 0;
    juce::String pendingUsername;
    juce::String pendingUserPassword;

    juce::String pendingGroup;
    juce::String pendingGroupPassword;
    bool pendingGroupIsPublic = false;
    bool pendingWatchPublicGroups = false;

    bool pendingUseTls = false;
    bool pendingVerifyTls = false;
    juce::String pendingTlsCaPath;

    std::atomic<bool> connected { false };
    std::atomic<bool> connectionRequested { false };
    std::atomic<int> droppedQueuedPackets { 0 };
    std::atomic<int> droppedStalePackets { 0 };

    double lastWriteBlockMs = 0.0;
    double avgWriteBlockMs = 0.0;
    double maxWriteBlockMs = 0.0;
    int writeBlockSamples = 0;

    uint32_t clientId = 0;
    uint32_t sessionId = 0;
};
