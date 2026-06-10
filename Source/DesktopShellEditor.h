// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception

#pragma once

#include "JuceHeader.h"
#include "SonobusPluginProcessor.h"
#include "SonobusTypes.h"

class DesktopShellEditor : public AudioProcessorEditor,
                           private MultiTimer,
                           private SonobusAudioProcessor::ClientListener
{
public:
    explicit DesktopShellEditor (SonobusAudioProcessor&);
    ~DesktopShellEditor() override;

    void resized() override;
    void paint (Graphics&) override;

    /** Called from the embedded browser when the shell HTML has finished loading. */
    void shellPageLoadFinished();

    std::function<AudioDeviceManager*()> getAudioDeviceManager;
    std::function<bool()> isInterAppAudioConnected;
    std::function<Image(int)> getIAAHostIcon;
    std::function<void()> switchToHostApplication;
    std::function<Value*()> getShouldOverrideSampleRateValue;
    std::function<Value*()> getShouldCheckForNewVersionValue;
    std::function<Value*()> getAllowBluetoothInputValue;
    std::function<StringArray*()> getRecentSetupFiles;
    std::function<String*()> getLastRecentsFolder;
    std::function<void()> saveSettingsIfNeeded;

    void handleURL (const String& urlstr);
    void connectWithInfo (const AooServerConnectionInfo& info, bool allowEmptyGroup = false, bool copyInfoOnly = false);
    bool loadSettingsFromFile (const File& file);
    void prepareForAppExit();
    bool requestedQuit();

private:
    class ShellWebView;

    enum { kUiRefreshTimerId = 1 };

    void timerCallback (int timerId) override;

    void aooClientConnected (SonobusAudioProcessor*, bool success, const String& errmesg = "") override;
    void aooClientDisconnected (SonobusAudioProcessor*, bool success, const String& errmesg = "") override;
    void aooClientLoginResult (SonobusAudioProcessor*, bool success, const String& errmesg = "") override;
    void aooClientGroupJoined (SonobusAudioProcessor*, bool success, const String& group, const String& errmesg = "") override;
    void aooClientGroupLeft (SonobusAudioProcessor*, bool success, const String& group, const String& errmesg = "") override;
    void aooClientPublicGroupModified (SonobusAudioProcessor*, const String& group, int count, const String& errmesg = "") override {}
    void aooClientPublicGroupDeleted (SonobusAudioProcessor*, const String& group, const String& errmesg = "") override {}
    void aooClientPeerJoined (SonobusAudioProcessor*, const String& group, const String& user) override;
    void aooClientPeerPendingJoin (SonobusAudioProcessor*, const String& group, const String& user) override {}
    void aooClientPeerJoinFailed (SonobusAudioProcessor*, const String& group, const String& user) override {}
    void aooClientPeerJoinBlocked (SonobusAudioProcessor*, const String& group, const String& user, const String& address, int port) override {}
    void aooClientPeerLeft (SonobusAudioProcessor*, const String& group, const String& user) override;
    void aooClientError (SonobusAudioProcessor*, const String& errmesg) override;
    void aooClientPeerChangedState (SonobusAudioProcessor*, const String& mesg) override {}
    void sbChatEventReceived (SonobusAudioProcessor*, const SBChatEvent& mesg) override {}
    void peerRequestedLatencyMatch (SonobusAudioProcessor*, const String& username, float latency) override {}
    void peerBlockedInfoChanged (SonobusAudioProcessor*, const String& username, bool blocked) override {}
    void peerSuggestedNewGroup (SonobusAudioProcessor*, const String& username, const String& newgroup, const String& groupPass, bool isPublic, const StringArray& others) override {}

    void handleBridgeFromWebView (const String& url);
    void pushStateToWeb (bool force = false);
    void invokeJs (const String& javascript);
    void applyConnectFromShellState (const var& stateVar, bool copyInfoOnly);
    void shellPerformDisconnect();
    void shellCopyInfo();
    void shellPasteInfo();
    void handlePeerUiAction (const var& detail);
    void applyCookieLinkLaunchURL (const URL& url);
    void applyShellServerSelection (const String& serverId, int transportMode, bool updateProcessorMode);
    void refreshServerListAsync();
    var makeShellServerChoicesVar() const;
    var findShellServerByIdVar (const String& id) const;
    bool shellServerSupportsTransport (const String& serverId, int transportMode) const;
    String firstShellServerIdSupportingTransport (int transportMode) const;
    String shellServerIdForHost (const String& host, int transportMode) const;
    void refreshServerPingsAsync();
    void refreshServerLocationsAsync();
    void loadRelayLicenseState();
    void saveRelayLicenseState() const;
    File getRelayLicenseStateFile() const;
    bool hasRelayLicense() const;
    void activateRelayLicenseAsync (std::function<void(bool)> afterActivation = {}, bool showNotice = true);
    void unbindRelayLicenseAsync();
    void refreshRelayLicenseHeartbeatAsync();
    void refreshOnlineCountAsync();
    void logoutRelayLicenseNow();
    void showRelayLicenseMessage (const String& message, bool isError);
    void showShellNotice (const String& title, const String& message, bool isError);
    void fetchBulletinAsync();

    SonobusAudioProcessor& processor;
    std::unique_ptr<ShellWebView> webView;
    File shellHtmlTempFile;
    std::unique_ptr<FileChooser> shellFileChooser;
    AooServerConnectionInfo currConnectionInfo;
    String currServerUrlText;
    String selectedServerId { "server1" };
    Array<var> remoteServerChoices;
    bool serverListInFlight = false;
    bool serverListLoaded = false;
    String serverListMessage;
    int serverOnePingMs = -2;
    int serverTwoPingMs = -2;
    int serverThreePingMs = -2;
    int serverFourPingMs = -2;
    int serverFivePingMs = -2;
    int serverSixPingMs = -2;
    int serverSevenPingMs = -2;
    String serverOneLocation;
    String serverTwoLocation;
    String serverThreeLocation;
    String serverFourLocation;
    String serverFiveLocation;
    String serverSixLocation;
    String serverSevenLocation;
    bool serverLocationInFlight = false;
    bool serverPingInFlight = false;
    int64 lastServerPingProbeMs = 0;
    String relayLicenseSingleCode;
    String relayLicenseStatusCode;
    String relayLicenseExpiresAt;
    String relayLicenseMessage;
    int relayLicenseOnlineCount = -1;
    bool relayLicenseBusy = false;
    bool relayLicenseHeartbeatInFlight = false;
    bool relayLicenseOnlineCountInFlight = false;
    bool relayLicenseLoggedOut = false;
    int64 lastRelayLicenseHeartbeatMs = 0;
    int64 lastRelayLicenseOnlineCountMs = 0;
    bool shellPageReady = false;
    String pendingShellNoticeTitle;
    String pendingShellNoticeMessage;
    bool pendingShellNoticeIsError = false;
    int64 lastPeerLatencyAutoMs = 0;
    int64 lastStatePushMs = 0;
    String lastStateJson;
    String lastPeersStableJson;

    Array<int64> shellPeerLastRecvBytes;
    Array<int64> shellPeerLastSentBytes;
    int64 shellPeerByteSampleTimeMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DesktopShellEditor)
};
