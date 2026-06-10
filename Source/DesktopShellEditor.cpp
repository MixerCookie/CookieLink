// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception

#include "DesktopShellEditor.h"

#include "BinaryData.h"
#include "EyDataAuthClient.h"
#include "RelayProtocol.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <cstring>

namespace
{
struct ShellServerChoice
{
    String id;
    String host;
    bool supportsTcpRelay = false;
    bool supportsUdpP2P = false;
    int latencyProbePort = DEFAULT_RELAY_SERVER_PORT;
};

String shellPingKeyForIndex (int index)
{
    static const char* keys[] = {
        "serverOnePingMs", "serverTwoPingMs", "serverThreePingMs", "serverFourPingMs",
        "serverFivePingMs", "serverSixPingMs", "serverSevenPingMs"
    };
    return index >= 0 && index < (int) (sizeof (keys) / sizeof (keys[0])) ? String (keys[index]) : String();
}

String shellLocationForIndex (int index, const String& one, const String& two, const String& three,
                              const String& four, const String& five, const String& six, const String& seven)
{
    switch (index)
    {
        case 0: return one;
        case 1: return two;
        case 2: return three;
        case 3: return four;
        case 4: return five;
        case 5: return six;
        case 6: return seven;
        default: return {};
    }
}

constexpr const char* kFreeServerId = "free1";
constexpr const char* kFreeServerHost = "aa.menglonghu.cn";
constexpr int kFreeServerPort = DEFAULT_RELAY_SERVER_PORT;
constexpr int kFreeTcpMaxBitrate = 48000;

bool isFreeServerId (const String& id)
{
    return id == kFreeServerId;
}

constexpr int kLatencyNotMeasurableMs = -3;

int defaultServerPortForTransport (int transportMode)
{
    return transportMode == SonobusAudioProcessor::TransportModeTcpRelay ? DEFAULT_RELAY_SERVER_PORT
                                                                         : DEFAULT_SERVER_PORT;
}

bool hostHasPortSafe (const String& hostport) { return hostport.containsChar (':'); }

String encodeRelayCodeForStorage (const String& code)
{
    return Base64::toBase64 (code.toRawUTF8(), code.getNumBytesAsUTF8());
}

String decodeRelayCodeFromStorage (const String& encoded)
{
    MemoryOutputStream decoded;
    if (! Base64::convertFromBase64 (decoded, encoded.trim()))
        return {};

    const auto block = decoded.getMemoryBlock();
    return block.toString().trim();
}

bool isPublicIpv4Address (const String& ip)
{
    StringArray parts;
    parts.addTokens (ip, ".", "");
    if (parts.size() != 4)
        return false;

    int octets[4] {};
    for (int i = 0; i < 4; ++i)
    {
        if (! parts[i].containsOnly ("0123456789"))
            return false;
        octets[i] = parts[i].getIntValue();
        if (octets[i] < 0 || octets[i] > 255)
            return false;
    }

    if (octets[0] == 0 || octets[0] == 10 || octets[0] == 127 || octets[0] >= 224)
        return false;
    if (octets[0] == 100 && octets[1] >= 64 && octets[1] <= 127)
        return false;
    if (octets[0] == 169 && octets[1] == 254)
        return false;
    if (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31)
        return false;
    if (octets[0] == 192 && octets[1] == 168)
        return false;
    if (octets[0] == 198 && (octets[1] == 18 || octets[1] == 19))
        return false;

    return true;
}

void writeLatencyU16 (MemoryOutputStream& out, uint16_t value)
{
    const uint8_t bytes[2] = {
        static_cast<uint8_t> ((value >> 8) & 0xff),
        static_cast<uint8_t> (value & 0xff)
    };
    out.write (bytes, 2);
}

void writeLatencyU32 (MemoryOutputStream& out, uint32_t value)
{
    const uint8_t bytes[4] = {
        static_cast<uint8_t> ((value >> 24) & 0xff),
        static_cast<uint8_t> ((value >> 16) & 0xff),
        static_cast<uint8_t> ((value >> 8) & 0xff),
        static_cast<uint8_t> (value & 0xff)
    };
    out.write (bytes, 4);
}

void writeLatencyString (MemoryOutputStream& out, const String& text)
{
    const auto* bytes = text.toRawUTF8();
    const auto len = static_cast<uint16_t> (jmin ((size_t) std::strlen (bytes), (size_t) 0xffff));
    writeLatencyU16 (out, len);
    out.write (bytes, len);
}

bool readLatencyExact (StreamingSocket& socket, void* dest, int bytes, int timeoutMs)
{
    auto* out = static_cast<char*> (dest);
    int offset = 0;
    const double deadline = Time::getMillisecondCounterHiRes() + timeoutMs;

    while (offset < bytes)
    {
        const int waitMs = roundToInt (deadline - Time::getMillisecondCounterHiRes());
        if (waitMs <= 0)
            return false;

        const int ready = socket.waitUntilReady (true, jmin (100, waitMs));
        if (ready < 0)
            return false;
        if (ready == 0)
            continue;

        const int got = socket.read (out + offset, bytes - offset, false);
        if (got <= 0)
        {
            if (! socket.isConnected())
                return false;
            continue;
        }
        offset += got;
    }

    return true;
}

bool writeLatencyAll (StreamingSocket& socket, const void* data, int bytes, int timeoutMs)
{
    const auto* in = static_cast<const char*> (data);
    int offset = 0;
    const double deadline = Time::getMillisecondCounterHiRes() + timeoutMs;

    while (offset < bytes)
    {
        const int waitMs = roundToInt (deadline - Time::getMillisecondCounterHiRes());
        if (waitMs <= 0)
            return false;

        const int ready = socket.waitUntilReady (false, jmin (100, waitMs));
        if (ready < 0)
            return false;
        if (ready == 0)
            continue;

        const int sent = socket.write (in + offset, bytes - offset);
        if (sent <= 0)
            return false;
        offset += sent;
    }

    return true;
}

/** Map linear RMS/amplitude (0..1) from LevelMeterSource to a bar height 0..1 for clearer low-level motion. */
float shellMeterLinearToDisplay (float linear)
{
    const float x = jlimit (1.0e-7f, 1.0f, linear);
    const float db = juce::Decibels::gainToDecibels (x, -120.0f);
    return jlimit (0.0f, 1.0f, (db + 54.0f) / 54.0f);
}

int shellIndexOfFormatInfo (SonobusAudioProcessor& processor, const SonobusAudioProcessor::AudioCodecFormatInfo& info)
{
    const int nf = processor.getNumberAudioCodecFormats();
    for (int fi = 0; fi < nf; ++fi)
    {
        SonobusAudioProcessor::AudioCodecFormatInfo finfo;
        processor.getAudioCodeFormatInfo (fi, finfo);
        if (finfo.codec != info.codec)
            continue;
        if (finfo.codec == SonobusAudioProcessor::CodecOpus && finfo.bitrate == info.bitrate)
            return fi;
        if (finfo.codec == SonobusAudioProcessor::CodecPCM && finfo.bitdepth == info.bitdepth)
            return fi;
    }
    return -1;
}

int shellFindOpusBitrateFormatIndex (SonobusAudioProcessor& processor, int bitrate)
{
    const int nf = processor.getNumberAudioCodecFormats();
    for (int fi = 0; fi < nf; ++fi)
    {
        SonobusAudioProcessor::AudioCodecFormatInfo finfo;
        processor.getAudioCodeFormatInfo (fi, finfo);
        if (finfo.codec == SonobusAudioProcessor::CodecOpus && finfo.bitrate == bitrate)
            return fi;
    }
    return 0;
}

String makeStablePeersJson (const Array<var>& peers)
{
    Array<var> out;
    for (const auto& p : peers)
    {
        if (! p.isObject())
            continue;
        auto* src = p.getDynamicObject();
        if (src == nullptr)
            continue;
        var dst (new DynamicObject());
        auto* d = dst.getDynamicObject();
        d->setProperty ("index", src->getProperty ("index"));
        d->setProperty ("name", src->getProperty ("name"));
        d->setProperty ("recvMuted", src->getProperty ("recvMuted"));
        d->setProperty ("solo", src->getProperty ("solo"));
        d->setProperty ("volumeDb", src->getProperty ("volumeDb"));
        d->setProperty ("sendQualityIndex", src->getProperty ("sendQualityIndex"));
        d->setProperty ("recvQualityIndex", src->getProperty ("recvQualityIndex"));
        d->setProperty ("bufferMs", src->getProperty ("bufferMs"));
        d->setProperty ("bufferModeIndex", src->getProperty ("bufferModeIndex"));
        out.add (dst);
    }
    return JSON::toString (var (out), true);
}

int parsePingOutputMs (const String& output)
{
    const String lower = output.toLowerCase();
    if (lower.contains ("198.18.") || lower.contains ("198.19."))
        return kLatencyNotMeasurableMs;

    int idx = lower.indexOf ("time=");
    if (idx >= 0)
    {
        const String rest = lower.substring (idx + 5).trimStart();
        const double ms = rest.upToFirstOccurrenceOf (" ", false, false).getDoubleValue();
        return ms > 0.0 ? jmax (1, roundToInt (ms)) : 0;
    }

    idx = lower.indexOf ("time<");
    if (idx >= 0)
        return 1;

    return -1;
}

int measureHostPingMs (const String& host)
{
    ChildProcess ping;
    StringArray args;
#if JUCE_WINDOWS
    args.add ("ping"); args.add ("-n"); args.add ("1"); args.add ("-w"); args.add ("1000"); args.add (host);
#elif JUCE_MAC
    args.add ("ping"); args.add ("-c"); args.add ("1"); args.add ("-W"); args.add ("1000"); args.add (host);
#else
    args.add ("ping"); args.add ("-c"); args.add ("1"); args.add ("-W"); args.add ("1"); args.add (host);
#endif
    if (! ping.start (args))
        return -1;
    if (! ping.waitForProcessToFinish (1800))
    {
        ping.kill();
        return -1;
    }
    return parsePingOutputMs (ping.readAllProcessOutput());
}

int measureTcpConnectMs (const String& host, int port)
{
    StreamingSocket socket;
    const double startMs = Time::getMillisecondCounterHiRes();
    if (! socket.connect (host, port, 1500))
        return -1;
    return jmax (1, roundToInt (Time::getMillisecondCounterHiRes() - startMs));
}

String resolvePublicIpv4WithDoh (const String& host)
{
    if (isPublicIpv4Address (host))
        return host;

    const String escapedHost = URL::addEscapeChars (host.trim(), true);
    const char* endpoints[] = {
        "https://dns.alidns.com/resolve?name=",
        "https://doh.pub/dns-query?name=",
        "https://dns.google/resolve?name="
    };

    for (const auto* endpoint : endpoints)
    {
        int statusCode = 0;
        URL url (String (endpoint) + escapedHost + "&type=A");
        auto stream = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                 .withExtraHeaders ("Accept: application/dns-json\r\n")
                                                 .withConnectionTimeoutMs (2500)
                                                 .withStatusCode (&statusCode));

        if (stream == nullptr || statusCode < 200 || statusCode >= 300)
            continue;

        const String response = stream->readEntireStreamAsString();
        const var parsed = JSON::parse (response);
        const var answer = parsed.getProperty ("Answer", var());
        if (auto* answers = answer.getArray())
        {
            for (const auto& item : *answers)
            {
                const int type = (int) item.getProperty ("type", 0);
                const String data = item.getProperty ("data", {}).toString().trim();
                if (type == 1 && isPublicIpv4Address (data))
                    return data;
            }
        }
    }

    return {};
}

bool isLoopbackHost (const String& host)
{
    const String h = host.trim().toLowerCase();
    return h == "127.0.0.1" || h == "localhost";
}

String makeLocationTextFromParts (String country, String region, String city)
{
    country = country.trim();
    region = region.trim();
    city = city.trim();
    if (country.isNotEmpty() && region.startsWithIgnoreCase (country))
        region = region.substring (country.length()).trim();

    StringArray parts;
    auto addPart = [&parts] (const String& text)
    {
        const String part = text.trim();
        if (part.isNotEmpty() && ! parts.contains (part, true))
            parts.add (part);
    };

    addPart (country);

    StringArray regionParts;
    regionParts.addTokens (region, ",，|", "");
    for (const auto& part : regionParts)
        addPart (part);

    addPart (city);

    return parts.joinIntoString (" · ");
}

String makeIpLocationText (const var& parsed)
{
    if (parsed.getProperty ("status", {}).toString() != "success")
        return {};

    return makeLocationTextFromParts (parsed.getProperty ("country", {}).toString(),
                                      parsed.getProperty ("regionName", {}).toString(),
                                      parsed.getProperty ("city", {}).toString());
}

String makeIpWhoisLocationText (const var& parsed)
{
    if (! (bool) parsed.getProperty ("success", false))
        return {};

    return makeLocationTextFromParts (parsed.getProperty ("country", {}).toString(),
                                      parsed.getProperty ("region", {}).toString(),
                                      parsed.getProperty ("city", {}).toString());
}

String readStreamUtf8 (InputStream& stream)
{
    MemoryBlock responseBytes;
    stream.readIntoMemoryBlock (responseBytes);
    return String::fromUTF8 (static_cast<const char*> (responseBytes.getData()),
                             (int) responseBytes.getSize());
}

String lookupServerLocation (const ShellServerChoice& server)
{
    const String host = server.host.trim();
    if (isLoopbackHost (host))
        return "本地";

    String query = resolvePublicIpv4WithDoh (host);
    if (query.isEmpty())
        query = host;

    {
        int statusCode = 0;
        URL url ("https://ipwhois.app/json/" + URL::addEscapeChars (query, true) + "?lang=zh-CN");
        auto stream = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                 .withConnectionTimeoutMs (3500)
                                                 .withStatusCode (&statusCode));
        if (stream != nullptr && statusCode >= 200 && statusCode < 300)
        {
            const String location = makeIpWhoisLocationText (JSON::parse (readStreamUtf8 (*stream)));
            if (location.isNotEmpty())
                return location;
        }
    }

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (query.isEmpty())
            break;

        int statusCode = 0;
        URL url ("http://ip-api.com/json/" + URL::addEscapeChars (query, true)
                 + "?lang=zh-CN&fields=status,country,regionName,city,query");
        auto stream = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                                                 .withConnectionTimeoutMs (2500)
                                                 .withStatusCode (&statusCode));
        if (stream != nullptr && statusCode >= 200 && statusCode < 300)
        {
            const String location = makeIpLocationText (JSON::parse (readStreamUtf8 (*stream)));
            if (location.isNotEmpty())
                return location;
        }

        query = attempt == 0 ? resolvePublicIpv4WithDoh (host) : String();
    }

    return "未知";
}

int measureRelayHandshakeMs (const String& host, int port)
{
    StreamingSocket socket;
    const double startMs = Time::getMillisecondCounterHiRes();
    if (! socket.connect (host, port, 1000))
        return -1;

    MemoryOutputStream payload;
    payload.writeByte (static_cast<char> (cookielink::relay::MsgHello));
    writeLatencyU16 (payload, cookielink::relay::kProtocolVersion);
    writeLatencyString (payload, "测速");
    writeLatencyString (payload, {});

    MemoryOutputStream frame;
    writeLatencyU32 (frame, static_cast<uint32_t> (payload.getDataSize()));
    frame.write (payload.getData(), payload.getDataSize());

    if (! writeLatencyAll (socket, frame.getData(), (int) frame.getDataSize(), 1000))
        return -1;

    uint8_t lenBytes[4];
    if (! readLatencyExact (socket, lenBytes, 4, 1000))
        return -1;

    const uint32_t length = (static_cast<uint32_t> (lenBytes[0]) << 24)
                          | (static_cast<uint32_t> (lenBytes[1]) << 16)
                          | (static_cast<uint32_t> (lenBytes[2]) << 8)
                          | static_cast<uint32_t> (lenBytes[3]);
    if (length == 0 || length > cookielink::relay::kMaxFrameSize)
        return -1;

    MemoryBlock response (length);
    if (! readLatencyExact (socket, response.getData(), (int) length, 1000))
        return -1;

    const auto* data = static_cast<const uint8_t*> (response.getData());
    const auto type = static_cast<cookielink::relay::MessageType> (data[0]);
    if (type != cookielink::relay::MsgWelcome && type != cookielink::relay::MsgError)
        return -1;

    return jmax (1, roundToInt (Time::getMillisecondCounterHiRes() - startMs));
}

int measureServerLatencyMs (const ShellServerChoice& server)
{
    if (server.supportsTcpRelay)
        return measureRelayHandshakeMs (server.host, server.latencyProbePort);

    const String realIp = resolvePublicIpv4WithDoh (server.host);
    if (realIp.isNotEmpty())
    {
        const int connectMs = measureTcpConnectMs (realIp, server.latencyProbePort);
        if (connectMs > 3)
            return connectMs;
    }

    const int icmpMs = measureHostPingMs (server.host);
    if (icmpMs == kLatencyNotMeasurableMs)
    {
        const int realPingMs = realIp.isNotEmpty() ? measureHostPingMs (realIp) : -1;
        return realPingMs > 3 ? realPingMs : -1;
    }

    return icmpMs > 3 ? icmpMs : -1;
}

float paramToUiPercent (RangedAudioParameter* p)
{
    if (p == nullptr) return 0.0f;
    return p->getValue() * 100.0f;
}

void setParamFromUiPercent (RangedAudioParameter* p, float pct01To100)
{
    if (p == nullptr) return;
    p->setValueNotifyingHost (jlimit (0.0f, 100.0f, pct01To100) / 100.0f);
}

void setParamFromUiLinear (RangedAudioParameter* p, float uiVal, float uiMin, float uiMax)
{
    if (p == nullptr) return;
    p->setValueNotifyingHost (p->convertTo0to1 (jlimit (uiMin, uiMax, uiVal)));
}

float paramToUiLinear (RangedAudioParameter* p, float uiMin, float uiMax)
{
    if (p == nullptr) return uiMin;
    return jlimit (uiMin, uiMax, p->convertFrom0to1 (p->getValue()));
}

static constexpr float shellMixerDbMin = -48.0f;
static constexpr float shellMixerDbOff = -48.0f;
static constexpr float shellInDbMax = 12.0f;
static constexpr float shellSendDbMax = 6.0f;
static constexpr float shellMonitorDbMax = 0.0f;

float shellParamGainToDb (RangedAudioParameter* p, float dbMin, float dbMax)
{
    if (p == nullptr) return dbMin;
    const float gain = p->convertFrom0to1 (p->getValue());
    return jlimit (dbMin, dbMax, Decibels::gainToDecibels (gain, dbMin - 1.0f));
}

void setShellParamGainFromDb (RangedAudioParameter* p, float db, float dbMin, float dbMax)
{
    if (p == nullptr) return;
    const float clampedDb = jlimit (dbMin, dbMax, db);
    const float gain = Decibels::decibelsToGain (clampedDb);
    p->setValueNotifyingHost (p->convertTo0to1 (gain));
}

String toBase64Utf8 (const String& text)
{
    return Base64::toBase64 (text.toRawUTF8(), text.getNumBytesAsUTF8());
}

String toJsStringLiteral (const String& text)
{
    return JSON::toString (var (text), true);
}

String makeServerUrlText (const AooServerConnectionInfo& info)
{
    String hostport = info.serverHost;
    if (info.serverPort > 0 && info.serverPort != defaultServerPortForTransport (info.transportMode))
        hostport << ":" << info.serverPort;
    return hostport;
}

void applyServerUrlText (const String& serverUrlText, int transportMode, AooServerConnectionInfo& info)
{
    String text = serverUrlText.trim().removeCharacters ("\"'");
    if (text.contains ("://"))
        text = text.fromFirstOccurrenceOf ("://", false, false).trim();
    text = text.upToFirstOccurrenceOf ("/", false, false).trim();

    if (text.isEmpty())
        text = DEFAULT_SERVER_HOST;

    String host = text.upToFirstOccurrenceOf (":", false, true).trim();
    const String portText = text.fromFirstOccurrenceOf (":", false, true).trim();
    int port = portText.containsOnly ("0123456789") ? portText.getIntValue() : 0;

    if (host.isEmpty())
        host = DEFAULT_SERVER_HOST;

    info.serverHost = host;
    info.serverPort = port > 0 ? port : defaultServerPortForTransport (transportMode);
}

var makeStringOptions (const StringArray& values)
{
    Array<var> list;
    for (const auto& value : values)
    {
        var item (new DynamicObject());
        item.getDynamicObject()->setProperty ("id", value);
        item.getDynamicObject()->setProperty ("name", value);
        list.add (item);
    }
    return var (list);
}

Array<File> getCookieLinkServerDebugDirs()
{
    Array<File> dirs;
    dirs.add (File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("CookieLink"));
#if JUCE_MAC
    dirs.add (File::getSpecialLocation (File::userHomeDirectory)
                  .getChildFile ("Library")
                  .getChildFile ("Application Support")
                  .getChildFile ("CookieLink"));
#endif
    dirs.add (File::getSpecialLocation (File::tempDirectory).getChildFile ("CookieLink"));
    return dirs;
}

void appendUtf8LogLine (const File& file, const String& line)
{
    if (auto stream = std::unique_ptr<FileOutputStream> (file.createOutputStream (8192)))
    {
        stream->setPosition (file.getSize());
        stream->writeText (line, false, false, "\n");
    }
}

void appendServerListDebugLine (const String& line)
{
    DBG ("[CookieLinkServerList] " + line.trimEnd());
    for (const auto& dir : getCookieLinkServerDebugDirs())
    {
        dir.createDirectory();
        appendUtf8LogLine (dir.getChildFile ("server_list_debug.log"), line);
    }
}

var makeIntOptions (const Array<int>& values)
{
    Array<var> list;
    for (const auto value : values)
    {
        var item (new DynamicObject());
        item.getDynamicObject()->setProperty ("id", String (value));
        item.getDynamicObject()->setProperty ("name", String (value));
        list.add (item);
    }
    return var (list);
}

var makeSampleRateOptions (const Array<double>& values)
{
    Array<var> list;
    for (const auto value : values)
    {
        const int rounded = roundToInt (value);
        var item (new DynamicObject());
        item.getDynamicObject()->setProperty ("id", String (rounded));
        item.getDynamicObject()->setProperty ("name", String (rounded));
        list.add (item);
    }
    return var (list);
}

String recordFormatExtension (SonobusAudioProcessor::RecordFileFormat format)
{
    switch (format)
    {
        case SonobusAudioProcessor::FileFormatWAV:
            return ".wav";
        case SonobusAudioProcessor::FileFormatOGG:
            return ".ogg";
        default:
            return ".flac";
    }
}

} // namespace

class DesktopShellEditor::ShellWebView : public WebBrowserComponent
{
public:
    explicit ShellWebView (DesktopShellEditor& o, const WebBrowserComponent::Options& browserOptions)
        : WebBrowserComponent (browserOptions), owner (o)
    {
    }

    bool pageAboutToLoad (const String& newURL) override
    {
        if (newURL.startsWithIgnoreCase ("cookielink://"))
        {
            owner.handleBridgeFromWebView (newURL);
            return false;
        }
        return true;
    }

    void pageFinishedLoading (const String&) override
    {
        MessageManager::callAsync ([safe = Component::SafePointer<DesktopShellEditor> (&owner)]
                                   {
                                       if (safe != nullptr) safe->shellPageLoadFinished();
                                   });
    }

    DesktopShellEditor& owner;
};

DesktopShellEditor::DesktopShellEditor (SonobusAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setSize (1100, 720);

    processor.addClientListener (this);

    appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                               + " server_list_debug_log_ready"
                               + newLine);
    EyDataAuthClient::touchDebugLog();

    WebBrowserComponent::Options opts;
    opts = opts.withKeepPageLoadedWhenBrowserIsHidden();
#if JUCE_WINDOWS
    if (WebBrowserComponent::areOptionsSupported (WebBrowserComponent::Options().withBackend (WebBrowserComponent::Options::Backend::webview2)))
        opts = opts.withBackend (WebBrowserComponent::Options::Backend::webview2)
                  .withWinWebView2Options (WebBrowserComponent::Options::WinWebView2().withBackgroundColour (Colours::transparentBlack));
#endif

    webView = std::make_unique<ShellWebView> (*this, opts);
    addAndMakeVisible (*webView);
    resized();

    shellHtmlTempFile = File::getSpecialLocation (File::tempDirectory)
                            .getNonexistentChildFile ("cookielink_shell", ".html", false);
    shellHtmlTempFile.replaceWithData (BinaryData::CookieLinkShell_html, BinaryData::CookieLinkShell_htmlSize);

    const String shellFileUrl = URL (shellHtmlTempFile).toString (true);
    MessageManager::callAsync ([safe = Component::SafePointer<DesktopShellEditor> (this), shellFileUrl]
                               {
                                   if (safe != nullptr && safe->webView != nullptr)
                                       safe->webView->goToURL (shellFileUrl, nullptr, nullptr);
                               });

    Array<AooServerConnectionInfo> recents;
    processor.getRecentServerConnectionInfos (recents);
    if (recents.size() > 0)
        currConnectionInfo = recents.getReference (0);
    else
    {
        currConnectionInfo.serverHost = DEFAULT_SERVER_HOST;
        currConnectionInfo.transportMode = (int) processor.getTransportMode();
        currConnectionInfo.serverPort = defaultServerPortForTransport ((int) processor.getTransportMode());
        String username = processor.getCurrentUsername().trim();
        if (username.isEmpty())
            username = SystemStats::getFullUserName().trim();
        if (username.isEmpty())
            username = SystemStats::getComputerName();
        currConnectionInfo.userName = username;
    }

    if (processor.getCurrentUsername().trim().isNotEmpty())
        currConnectionInfo.userName = processor.getCurrentUsername().trim();

    selectedServerId.clear();
    if (currConnectionInfo.serverPort == 0)
        currConnectionInfo.serverPort = defaultServerPortForTransport (currConnectionInfo.transportMode);
    currServerUrlText = makeServerUrlText (currConnectionInfo);

    startTimer (kUiRefreshTimerId, 100);
    fetchBulletinAsync();
}

DesktopShellEditor::~DesktopShellEditor()
{
    stopTimer (kUiRefreshTimerId);
    processor.removeClientListener (this);
    webView.reset();
    shellHtmlTempFile.deleteFile();
}

void DesktopShellEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xffe6e4e1));
}

void DesktopShellEditor::shellPageLoadFinished()
{
    shellPageReady = true;
    pushStateToWeb (true);
    if (pendingShellNoticeMessage.isNotEmpty())
    {
        const auto title = pendingShellNoticeTitle;
        const auto message = pendingShellNoticeMessage;
        const bool isError = pendingShellNoticeIsError;
        pendingShellNoticeTitle.clear();
        pendingShellNoticeMessage.clear();
        pendingShellNoticeIsError = false;
        showShellNotice (title, message, isError);
    }
}

void DesktopShellEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void DesktopShellEditor::invokeJs (const String& js)
{
    if (webView == nullptr) return;
    webView->goToURL ("javascript:" + js, nullptr, nullptr);
}

void DesktopShellEditor::refreshServerListAsync()
{
    if (serverListInFlight)
        return;

    const String status = relayLicenseStatusCode.trim();
    appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                               + " refresh_begin status_len=" + String (status.length())
                               + " loaded=" + String (serverListLoaded ? "yes" : "no")
                               + newLine);

    if (status.isEmpty())
    {
        serverListLoaded = false;
        serverListMessage = "未激活，仅显示免费服务器。";
        remoteServerChoices.clear();
        selectedServerId = firstShellServerIdSupportingTransport (currConnectionInfo.transportMode);
        applyShellServerSelection (selectedServerId, currConnectionInfo.transportMode, false);
        refreshServerPingsAsync();
        refreshServerLocationsAsync();
        pushStateToWeb (true);
        return;
    }

    serverListInFlight = true;
    serverListMessage = "正在获取付费服务器列表...";
    pushStateToWeb (true);
    const SafePointer<DesktopShellEditor> safe (this);
    Thread::launch ([safe, status]
                    {
                        auto result = EyDataAuthClient::fetchServerList (status, safe != nullptr ? safe->relayLicenseSingleCode : String());
                        Array<var> parsedServers;
                        String message = result.message;
                        appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                                                   + " fetch_done ok=" + String (result.ok ? "yes" : "no")
                                                   + " text_len=" + String (result.text.length())
                                                   + " message=" + message
                                                   + " text=" + result.text.substring (0, 1000)
                                                   + newLine);

                        if (result.ok)
                        {
                            String normalizedServerText = result.text.trim();
                            normalizedServerText = normalizedServerText.replace ("\r\n", "\n").replaceCharacter ('\r', '\n');
                            StringArray entries;
                            entries.addLines (normalizedServerText);
                            if (entries.size() <= 1 && normalizedServerText.containsChar (';'))
                            {
                                entries.clear();
                                entries.addTokens (normalizedServerText, ";", "");
                            }
                            for (int i = 0; i < entries.size(); ++i)
                            {
                                const String entry = entries[i].trim();
                                if (entry.isEmpty())
                                    continue;

                                StringArray fields;
                                fields.addTokens (entry, "|", "");
                                if (fields.size() < 3)
                                {
                                    appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                                                               + " skip_invalid_compact_server entry=" + entry
                                                               + newLine);
                                    continue;
                                }

                                String protocol = fields[0].trim().toLowerCase();
                                if (protocol == "t") protocol = "tcp";
                                if (protocol == "u") protocol = "udp";
                                const String host = fields[1].trim();
                                const int port = fields[2].trim().getIntValue();
                                String name = fields.size() > 3 ? fields[3].trim() : String();
                                String description = fields.size() > 4 ? fields[4].trim() : String();
                                if (host.isEmpty() || port <= 0 || (protocol != "tcp" && protocol != "udp"))
                                {
                                    appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                                                               + " skip_invalid_compact_server protocol=" + protocol
                                                               + " host=" + host
                                                               + " port=" + String (port)
                                                               + newLine);
                                    continue;
                                }
                                if (name.isEmpty())
                                    name = host;
                                if (description.isEmpty())
                                    description = protocol == "tcp" ? "付费 TCP 线路，支持全部音质。"
                                                                     : "付费 UDP 线路，支持全部音质。";

                                var dst (new DynamicObject());
                                auto* d = dst.getDynamicObject();
                                d->setProperty ("id", String ("paid") + String (parsedServers.size() + 1));
                                d->setProperty ("name", name);
                                d->setProperty ("description", description);
                                d->setProperty ("host", host);
                                d->setProperty ("port", port);
                                d->setProperty ("protocol", protocol);
                                d->setProperty ("tcp", protocol == "tcp");
                                d->setProperty ("udp", protocol == "udp");
                                const int listIndex = parsedServers.size() + 1;
                                d->setProperty ("pingKey", shellPingKeyForIndex (listIndex));
                                d->setProperty ("locationKey", "server" + String (listIndex + 1));
                                parsedServers.add (dst);
                            }

                            if (parsedServers.isEmpty() && message.isEmpty())
                                message = "服务器变量为空或格式错误。";
                        }

                        appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                                                   + " parsed_count=" + String (parsedServers.size())
                                                   + " final_message=" + message
                                                   + newLine);

                        MessageManager::callAsync ([safe, parsedServers, message]
                                                   {
                                                       if (safe == nullptr)
                                                           return;

                                                       safe->serverListInFlight = false;
                                                       if (! parsedServers.isEmpty())
                                                       {
                                                           safe->remoteServerChoices = parsedServers;
                                                           safe->serverListLoaded = true;
                                                           safe->serverListMessage = "已加载付费服务器列表。";
                                                           appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                                                                                      + " apply_remote_servers count=" + String (parsedServers.size())
                                                                                      + newLine);
                                                           safe->selectedServerId = safe->shellServerIdForHost (safe->currConnectionInfo.serverHost, safe->currConnectionInfo.transportMode);
                                                           if (! safe->shellServerSupportsTransport (safe->selectedServerId, safe->currConnectionInfo.transportMode))
                                                               safe->selectedServerId = safe->firstShellServerIdSupportingTransport (safe->currConnectionInfo.transportMode);
                                                           safe->applyShellServerSelection (safe->selectedServerId, safe->currConnectionInfo.transportMode, false);
                                                           safe->refreshServerPingsAsync();
                                                           safe->refreshServerLocationsAsync();
                                                       }
                                                       else
                                                       {
                                                           safe->serverListMessage = message.isNotEmpty() ? message : "获取服务器列表失败。";
                                                           appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                                                                                      + " apply_remote_servers_failed message=" + safe->serverListMessage
                                                                                      + newLine);
                                                       }
                                                       safe->pushStateToWeb (true);
                                                   });
                    });
}

var DesktopShellEditor::makeShellServerChoicesVar() const
{
    Array<var> list;
    var freeServer (new DynamicObject());
    auto* f = freeServer.getDynamicObject();
    f->setProperty ("id", kFreeServerId);
    f->setProperty ("name", "免费服务器");
    f->setProperty ("description", "免费服务器，TCP 仅支持 48kbps/ch 和以下音质，UDP 不限制。");
    f->setProperty ("host", kFreeServerHost);
    f->setProperty ("port", kFreeServerPort);
    f->setProperty ("protocol", "both");
    f->setProperty ("tcp", true);
    f->setProperty ("udp", true);
    f->setProperty ("free", true);
    f->setProperty ("maxTcpBitrate", kFreeTcpMaxBitrate);
    f->setProperty ("pingKey", "serverOnePingMs");
    f->setProperty ("locationKey", "server1");
    list.add (freeServer);

    for (const auto& item : remoteServerChoices)
        list.add (item);
    return var (list);
}

var DesktopShellEditor::findShellServerByIdVar (const String& id) const
{
    if (isFreeServerId (id))
    {
        auto all = makeShellServerChoicesVar();
        if (auto* arr = all.getArray())
            return arr->getReference (0);
    }

    for (const auto& item : remoteServerChoices)
        if (auto* obj = item.getDynamicObject())
            if (obj->getProperty ("id").toString() == id)
                return item;

    return {};
}

bool DesktopShellEditor::shellServerSupportsTransport (const String& serverId, int transportMode) const
{
    auto item = findShellServerByIdVar (serverId);
    if (auto* obj = item.getDynamicObject())
        return transportMode == SonobusAudioProcessor::TransportModeTcpRelay
            ? (bool) obj->getProperty ("tcp")
            : (bool) obj->getProperty ("udp");

    return false;
}

String DesktopShellEditor::firstShellServerIdSupportingTransport (int transportMode) const
{
    auto choices = makeShellServerChoicesVar();
    if (auto* arr = choices.getArray())
        for (const auto& item : *arr)
            if (auto* obj = item.getDynamicObject())
            {
                const String id = obj->getProperty ("id").toString();
                if (shellServerSupportsTransport (id, transportMode))
                    return id;
            }

    return {};
}

String DesktopShellEditor::shellServerIdForHost (const String& host, int transportMode) const
{
    auto choices = makeShellServerChoicesVar();
    if (auto* arr = choices.getArray())
    {
        for (const auto& item : *arr)
            if (auto* obj = item.getDynamicObject())
            {
                const String id = obj->getProperty ("id").toString();
                if (host.equalsIgnoreCase (obj->getProperty ("host").toString()) && shellServerSupportsTransport (id, transportMode))
                    return id;
            }

        for (const auto& item : *arr)
            if (auto* obj = item.getDynamicObject())
                if (host.equalsIgnoreCase (obj->getProperty ("host").toString()))
                    return obj->getProperty ("id").toString();
    }

    return firstShellServerIdSupportingTransport (transportMode);
}

void DesktopShellEditor::applyShellServerSelection (const String& serverId, int transportMode, bool updateProcessorMode)
{
    int mode = transportMode;
    if (mode != SonobusAudioProcessor::TransportModeUdpP2P && mode != SonobusAudioProcessor::TransportModeTcpRelay)
        mode = SonobusAudioProcessor::TransportModeUdpP2P;
    String resolvedServerId = serverId;
    if (! shellServerSupportsTransport (resolvedServerId, mode))
        resolvedServerId = firstShellServerIdSupportingTransport (mode);

    auto server = findShellServerByIdVar (resolvedServerId);
    if (auto* obj = server.getDynamicObject())
    {
        selectedServerId = obj->getProperty ("id").toString();
        currConnectionInfo.serverHost = obj->getProperty ("host").toString();
        const int port = (int) obj->getProperty ("port");
        currConnectionInfo.serverPort = port > 0 ? port : defaultServerPortForTransport (mode);
    }
    else
    {
        selectedServerId.clear();
    }
    currConnectionInfo.transportMode = mode;
    currServerUrlText = makeServerUrlText (currConnectionInfo);

    if (updateProcessorMode)
        processor.setTransportMode ((SonobusAudioProcessor::TransportMode) mode);
}

void DesktopShellEditor::refreshServerPingsAsync()
{
    if (serverPingInFlight)
        return;

    Array<ShellServerChoice> servers;
    auto choices = makeShellServerChoicesVar();
    if (auto* arr = choices.getArray())
    {
        const int count = jmin (7, arr->size());
        for (int i = 0; i < count; ++i)
        {
            if (auto* obj = arr->getReference (i).getDynamicObject())
            {
                ShellServerChoice server;
                server.id = obj->getProperty ("id").toString();
                server.host = obj->getProperty ("host").toString();
                server.supportsTcpRelay = (bool) obj->getProperty ("tcp");
                server.supportsUdpP2P = (bool) obj->getProperty ("udp");
                const int port = (int) obj->getProperty ("port");
                server.latencyProbePort = port > 0 ? port : (server.supportsTcpRelay ? DEFAULT_RELAY_SERVER_PORT : DEFAULT_SERVER_PORT);
                if (server.host.isNotEmpty())
                    servers.add (server);
            }
        }
    }

    if (servers.isEmpty())
        return;

    appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                               + " ping_begin count=" + String (servers.size())
                               + newLine);

    serverPingInFlight = true;
    lastServerPingProbeMs = Time::getApproximateMillisecondCounter();
    const Component::SafePointer<DesktopShellEditor> safe (this);
    Thread::launch ([safe, servers]
                    {
                        Array<int> pings;
                        for (const auto& server : servers)
                            pings.add (measureServerLatencyMs (server));

                        MessageManager::callAsync ([safe, pings]
                                                   {
                                                       if (safe == nullptr) return;
                                                       safe->serverOnePingMs = pings.size() > 0 ? pings[0] : -2;
                                                       safe->serverTwoPingMs = pings.size() > 1 ? pings[1] : -2;
                                                       safe->serverThreePingMs = pings.size() > 2 ? pings[2] : -2;
                                                       safe->serverFourPingMs = pings.size() > 3 ? pings[3] : -2;
                                                       safe->serverFivePingMs = pings.size() > 4 ? pings[4] : -2;
                                                       safe->serverSixPingMs = pings.size() > 5 ? pings[5] : -2;
                                                       safe->serverSevenPingMs = pings.size() > 6 ? pings[6] : -2;
                                                       safe->serverPingInFlight = false;
                                                       safe->pushStateToWeb (true);
                                                   });
                    });
}

void DesktopShellEditor::refreshServerLocationsAsync()
{
    if (serverLocationInFlight)
        return;

    Array<ShellServerChoice> servers;
    auto choices = makeShellServerChoicesVar();
    if (auto* arr = choices.getArray())
    {
        const int count = jmin (7, arr->size());
        for (int i = 0; i < count; ++i)
        {
            if (auto* obj = arr->getReference (i).getDynamicObject())
            {
                ShellServerChoice server;
                server.id = obj->getProperty ("id").toString();
                server.host = obj->getProperty ("host").toString();
                server.supportsTcpRelay = (bool) obj->getProperty ("tcp");
                server.supportsUdpP2P = (bool) obj->getProperty ("udp");
                const int port = (int) obj->getProperty ("port");
                server.latencyProbePort = port > 0 ? port : (server.supportsTcpRelay ? DEFAULT_RELAY_SERVER_PORT : DEFAULT_SERVER_PORT);
                if (server.host.isNotEmpty())
                    servers.add (server);
            }
        }
    }

    if (servers.isEmpty())
        return;

    appendServerListDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                               + " location_begin count=" + String (servers.size())
                               + newLine);

    serverLocationInFlight = true;
    const Component::SafePointer<DesktopShellEditor> safe (this);
    Thread::launch ([safe, servers]
                    {
                        StringArray locations;
                        for (const auto& server : servers)
                            locations.add (lookupServerLocation (server));

                        MessageManager::callAsync ([safe, locations]
                                                   {
                                                       if (safe == nullptr) return;
                                                       safe->serverOneLocation = locations.size() > 0 ? locations[0] : String();
                                                       safe->serverTwoLocation = locations.size() > 1 ? locations[1] : String();
                                                       safe->serverThreeLocation = locations.size() > 2 ? locations[2] : String();
                                                       safe->serverFourLocation = locations.size() > 3 ? locations[3] : String();
                                                       safe->serverFiveLocation = locations.size() > 4 ? locations[4] : String();
                                                       safe->serverSixLocation = locations.size() > 5 ? locations[5] : String();
                                                       safe->serverSevenLocation = locations.size() > 6 ? locations[6] : String();
                                                       safe->serverLocationInFlight = false;
                                                       safe->pushStateToWeb (true);
                                                   });
                    });
}


File DesktopShellEditor::getRelayLicenseStateFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("CookieLink")
        .getChildFile ("relay_license.xml");
}

void DesktopShellEditor::loadRelayLicenseState()
{
    const auto file = getRelayLicenseStateFile();
    std::unique_ptr<XmlElement> xml = XmlDocument::parse (file);

    relayLicenseSingleCode.clear();
    relayLicenseStatusCode.clear();
    relayLicenseExpiresAt.clear();
    relayLicenseMessage.clear();
    relayLicenseOnlineCount = -1;

    if (xml == nullptr || ! xml->hasTagName ("RelayLicense"))
        return;

    relayLicenseSingleCode = decodeRelayCodeFromStorage (xml->getStringAttribute ("singleCodeB64"));
    if (relayLicenseSingleCode.isEmpty() && xml->hasAttribute ("singleCode"))
        relayLicenseSingleCode = xml->getStringAttribute ("singleCode").trim();
}

void DesktopShellEditor::saveRelayLicenseState() const
{
    const auto file = getRelayLicenseStateFile();
    const String code = relayLicenseSingleCode.trim();
    if (code.isEmpty())
    {
        file.deleteFile();
        return;
    }

    file.getParentDirectory().createDirectory();

    XmlElement xml ("RelayLicense");
    xml.setAttribute ("singleCodeB64", encodeRelayCodeForStorage (code));
    xml.writeTo (file);
}

bool DesktopShellEditor::hasRelayLicense() const
{
    return relayLicenseStatusCode.length() == 32;
}

void DesktopShellEditor::showRelayLicenseMessage (const String& message, bool isError)
{
    relayLicenseMessage = message;
    showShellNotice (TRANS ("TCP服务器中继授权"), message, isError);
    pushStateToWeb (true);
}

void DesktopShellEditor::showShellNotice (const String& title, const String& message, bool isError)
{
    if (message.isEmpty())
        return;

    if (! shellPageReady)
    {
        pendingShellNoticeTitle = title;
        pendingShellNoticeMessage = message;
        pendingShellNoticeIsError = isError;
        return;
    }

    const String js = String ("if(window.CookieLinkShell&&window.CookieLinkShell.showNoticeB64)window.CookieLinkShell.showNoticeB64(")
                      + toJsStringLiteral (toBase64Utf8 (title)) + ","
                      + toJsStringLiteral (toBase64Utf8 (message)) + ","
                      + (isError ? "true" : "false") + ");void(0);";
    invokeJs (js);
}

void DesktopShellEditor::activateRelayLicenseAsync (std::function<void(bool)> afterActivation, bool showNotice)
{
    if (relayLicenseBusy)
        return;

    const String code = relayLicenseSingleCode.trim();
    if (code.isEmpty())
    {
        if (showNotice)
            showRelayLicenseMessage ("请先填写单码授权。", true);
        if (afterActivation)
            afterActivation (false);
        return;
    }

    const bool hadLicense = hasRelayLicense();
    relayLicenseBusy = true;
    relayLicenseMessage = "正在验证授权...";
    pushStateToWeb (true);

    const SafePointer<DesktopShellEditor> safe (this);
    Thread::launch ([safe, code, afterActivation, hadLicense, showNotice]
                    {
                        auto result = EyDataAuthClient::activateSingleCode (code);
                        MessageManager::callAsync ([safe, result, afterActivation, hadLicense, code, showNotice]
                                                   {
                                                       if (safe == nullptr)
                                                           return;

                                                       safe->relayLicenseBusy = false;
                                                       if (result.ok)
                                                       {
                                                           safe->relayLicenseSingleCode = code;
                                                           safe->relayLicenseStatusCode = result.statusCode;
                                                           safe->refreshServerListAsync();
                                                           safe->relayLicenseExpiresAt = result.expiresAt;
                                                           safe->relayLicenseMessage = result.message;
                                                           safe->relayLicenseOnlineCount = -1;
                                                           safe->relayLicenseLoggedOut = false;
                                                           safe->lastRelayLicenseHeartbeatMs = Time::getApproximateMillisecondCounter();
                                                           safe->lastRelayLicenseOnlineCountMs = 0;
                                                           safe->saveRelayLicenseState();
                                                           safe->refreshOnlineCountAsync();
                                                       }
                                                       else
                                                       {
                                                           if (! hadLicense)
                                                           {
                                                               safe->relayLicenseStatusCode.clear();
                                                               safe->relayLicenseExpiresAt.clear();
                                                               safe->relayLicenseOnlineCount = -1;
                                                           safe->remoteServerChoices.clear();
                                                           safe->serverListLoaded = false;
                                                           safe->selectedServerId = kFreeServerId;
                                                           safe->applyShellServerSelection (safe->selectedServerId, safe->currConnectionInfo.transportMode, false);
                                                           }
                                                           safe->relayLicenseMessage = result.message;
                                                           if (! showNotice && ! result.message.startsWith ("无法连接"))
                                                           {
                                                               safe->relayLicenseSingleCode.clear();
                                                               safe->saveRelayLicenseState();
                                                           }
                                                       }

                                                       safe->pushStateToWeb (true);
                                                       if (afterActivation)
                                                           afterActivation (result.ok);
                                                       if (showNotice)
                                                           safe->showRelayLicenseMessage (result.message, ! result.ok);
                                                   });
                    });
}

void DesktopShellEditor::unbindRelayLicenseAsync()
{
    if (relayLicenseBusy)
        return;

    const String code = relayLicenseSingleCode.trim();
    if (code.isEmpty())
    {
        showRelayLicenseMessage ("请先填写单码后再解绑。", true);
        return;
    }

    relayLicenseBusy = true;
    relayLicenseMessage = "正在解绑...";
    pushStateToWeb (true);

    const SafePointer<DesktopShellEditor> safe (this);
    Thread::launch ([safe, code]
                    {
                        auto result = EyDataAuthClient::unbindSingleCode (code);
                        MessageManager::callAsync ([safe, result]
                                                   {
                                                       if (safe == nullptr)
                                                           return;

                                                       safe->relayLicenseBusy = false;
                                                       safe->relayLicenseMessage = result.message;
                                                       if (result.ok)
                                                       {
                                                           safe->relayLicenseSingleCode.clear();
                                                           safe->relayLicenseStatusCode.clear();
                                                           safe->relayLicenseExpiresAt.clear();
                                                           safe->relayLicenseOnlineCount = -1;
                                                           safe->remoteServerChoices.clear();
                                                           safe->serverListLoaded = false;
                                                           safe->selectedServerId = kFreeServerId;
                                                           safe->applyShellServerSelection (safe->selectedServerId, safe->currConnectionInfo.transportMode, false);
                                                           safe->saveRelayLicenseState();
                                                       }

                                                       safe->showRelayLicenseMessage (result.message, ! result.ok);
                                                   });
                    });
}

void DesktopShellEditor::refreshRelayLicenseHeartbeatAsync()
{
    if (relayLicenseHeartbeatInFlight || ! hasRelayLicense())
        return;

    const String code = relayLicenseSingleCode.trim();
    const String status = relayLicenseStatusCode.trim();
    if (code.isEmpty() || status.isEmpty())
        return;

    relayLicenseHeartbeatInFlight = true;
    const SafePointer<DesktopShellEditor> safe (this);
    Thread::launch ([safe, code, status]
                    {
                        auto result = EyDataAuthClient::checkSingleCodeStatus (code, status);
                        MessageManager::callAsync ([safe, result]
                                                   {
                                                       if (safe == nullptr)
                                                           return;

                                                       safe->relayLicenseHeartbeatInFlight = false;
                                                       safe->lastRelayLicenseHeartbeatMs = Time::getApproximateMillisecondCounter();
                                                       if (! result.ok)
                                                       {
                                                           if (result.rawCode == "-112")
                                                           {
                                                               const String message = "用户在别的地方登陆";
                                                               const bool shouldNotify = safe->relayLicenseMessage != message;
                                                               safe->relayLicenseMessage = message;
                                                               safe->pushStateToWeb (true);
                                                               if (shouldNotify)
                                                                   safe->showShellNotice (TRANS ("TCP服务器中继授权"), message, true);
                                                           }
                                                           else if (result.message.startsWith ("无法连接授权服务器"))
                                                           {
                                                               safe->relayLicenseMessage = "授权检查暂时失败，请稍后重试。";
                                                               safe->pushStateToWeb (true);
                                                           }
                                                           else
                                                           {
                                                               safe->relayLicenseStatusCode.clear();
                                                               safe->relayLicenseExpiresAt.clear();
                                                               safe->relayLicenseOnlineCount = -1;
                                                           safe->remoteServerChoices.clear();
                                                           safe->serverListLoaded = false;
                                                           safe->selectedServerId = kFreeServerId;
                                                           safe->applyShellServerSelection (safe->selectedServerId, safe->currConnectionInfo.transportMode, false);
                                                               safe->relayLicenseMessage = result.message;
                                                               if (safe->currConnectionInfo.transportMode == SonobusAudioProcessor::TransportModeTcpRelay)
                                                                   safe->shellPerformDisconnect();
                                                               safe->showRelayLicenseMessage ("授权已失效，请重新授权。", true);
                                                           }
                                                       }
                                                       else
                                                       {
                                                           safe->relayLicenseMessage = "授权有效";
                                                           safe->pushStateToWeb (true);
                                                       }
                                                       if (safe->hasRelayLicense())
                                                           safe->refreshOnlineCountAsync();
                                                   });
                    });
}

void DesktopShellEditor::refreshOnlineCountAsync()
{
    if (relayLicenseOnlineCountInFlight || ! hasRelayLicense())
        return;

    const String code = relayLicenseSingleCode.trim();
    if (code.isEmpty())
        return;

    relayLicenseOnlineCountInFlight = true;
    const SafePointer<DesktopShellEditor> safe (this);
    Thread::launch ([safe, code]
                    {
                        auto result = EyDataAuthClient::fetchOnlineCount (code);
                        MessageManager::callAsync ([safe, result]
                                                   {
                                                       if (safe == nullptr)
                                                           return;

                                                       safe->relayLicenseOnlineCountInFlight = false;
                                                       safe->lastRelayLicenseOnlineCountMs = Time::getApproximateMillisecondCounter();
                                                       if (result.ok)
                                                           safe->relayLicenseOnlineCount = result.text.getIntValue();
                                                       safe->pushStateToWeb (true);
                                                   });
                    });
}

void DesktopShellEditor::logoutRelayLicenseNow()
{
    if (relayLicenseLoggedOut)
        return;

    const String code = relayLicenseSingleCode.trim();
    const String status = relayLicenseStatusCode.trim();
    if (code.isEmpty() || status.isEmpty())
        return;

    relayLicenseLoggedOut = true;
    EyDataAuthClient::logoutSingleCode (code, status);
}

void DesktopShellEditor::fetchBulletinAsync()
{
    const SafePointer<DesktopShellEditor> safe (this);
    Thread::launch ([safe]
                    {
                        auto result = EyDataAuthClient::fetchBulletin();
                        if (! result.ok || result.text.trim().isEmpty())
                            return;

                        MessageManager::callAsync ([safe, result]
                                                   {
                                                       if (safe == nullptr)
                                                           return;
                                                       safe->showShellNotice (TRANS ("公告"), result.text, false);
                                                   });
                    });
}

void DesktopShellEditor::pushStateToWeb (bool force)
{
    if (webView == nullptr) return;
    const int64 now = Time::getApproximateMillisecondCounter();
    if (!force && now - lastStatePushMs < 70)
        return;
    lastStatePushMs = now;

    auto* ing = dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramInGain));
    auto* dry = dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramDry));
    auto* wet = dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramWet));
    auto* metg = dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramMetGain));
    auto* mett = dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramMetTempo));
    auto* mrev = dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramMainReverbLevel));
    auto* jbuf = dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramDefaultNetbufMs));

    var opts (new DynamicObject());
    if (auto* o = opts.getDynamicObject())
    {
        auto* pAuto = dynamic_cast<AudioParameterChoice*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramDefaultAutoNetbuf));
        if (pAuto != nullptr)
            o->setProperty ("bufferModeIndex", String (pAuto->getIndex()));

        auto* pCh = dynamic_cast<AudioParameterChoice*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramSendChannels));
        if (pCh != nullptr)
            o->setProperty ("sendFormatIndex", String (pCh->getIndex()));

        o->setProperty ("sendQualityIndex", String (processor.getDefaultAudioCodecFormat()));

        Array<var> qualList;
        const int nf = processor.getNumberAudioCodecFormats();
        for (int i = 0; i < nf; ++i)
        {
            SonobusAudioProcessor::AudioCodecFormatInfo finfo;
            processor.getAudioCodeFormatInfo (i, finfo);
            String name = finfo.name;
            if (finfo.codec == SonobusAudioProcessor::AudioCodecFormatCodec::CodecOpus && finfo.bitrate < 96000)
                name += " (*)";
            var item (new DynamicObject());
            item.getDynamicObject()->setProperty ("id", i);
            item.getDynamicObject()->setProperty ("name", name);
            qualList.add (item);
        }
        o->setProperty ("sendQualityOptions", var (qualList));

        if (jbuf != nullptr)
            o->setProperty ("jitterMs", roundToInt (jbuf->convertFrom0to1 (jbuf->getValue())));
    }

    const auto relayStats = processor.getRelayRuntimeStats();
    String relayStatsText;
    const int relayDroppedTotal = relayStats.droppedQueuedPackets + relayStats.droppedStalePackets;
    if (relayStats.active)
    {
        relayStatsText << "队列 " << roundToInt (relayStats.oldestQueuedMs)
                       << " ms/" << relayStats.queuedFrames
                       << " · 丢 " << relayDroppedTotal
                       << " · 写 " << roundToInt (relayStats.lastWriteBlockMs) << " ms";
    }

    var root (new DynamicObject());
    if (auto* r = root.getDynamicObject())
    {
        var conn (new DynamicObject());
        conn.getDynamicObject()->setProperty ("displayName", currConnectionInfo.userName);
        conn.getDynamicObject()->setProperty ("groupName", currConnectionInfo.groupName);
        conn.getDynamicObject()->setProperty ("password", currConnectionInfo.groupPassword);
        conn.getDynamicObject()->setProperty ("serverUrl", currServerUrlText);
        r->setProperty ("connect", conn);

        var mix (new DynamicObject());
        const float inDb = shellParamGainToDb (ing, shellMixerDbMin, shellInDbMax);
        const float monitorDb = shellParamGainToDb (dry, shellMixerDbMin, shellMonitorDbMax);
        const float outDb = shellParamGainToDb (wet, shellMixerDbMin, shellSendDbMax);
        mix.getDynamicObject()->setProperty ("inLevel", roundToInt (inDb * 10.0f) / 10.0f);
        mix.getDynamicObject()->setProperty ("monitorLevel", roundToInt (monitorDb * 10.0f) / 10.0f);
        mix.getDynamicObject()->setProperty ("monitorEnabled", dry != nullptr && monitorDb > shellMixerDbOff + 0.5f);
        mix.getDynamicObject()->setProperty ("outLevel", roundToInt (outDb * 10.0f) / 10.0f);
        r->setProperty ("mixer", mix);

        const bool sessionActive = processor.isConnectedToServer()
                                   && processor.getCurrentJoinedGroup().isNotEmpty();
        r->setProperty ("sessionActive", sessionActive);

        processor.getOutputMeterSource().decayIfNeeded();
        processor.getSendMeterSource().decayIfNeeded();
        auto& outMeter = processor.getOutputMeterSource();
        auto& sendMeter = processor.getSendMeterSource();
        var meters (new DynamicObject());
        if (auto* mo = meters.getDynamicObject())
        {
            const int outCh = outMeter.getNumChannels();
            const int sendCh = sendMeter.getNumChannels();
            const float outL = outCh > 0 ? outMeter.getRMSLevel (0) : 0.0f;
            const float outR = outCh > 1 ? outMeter.getRMSLevel (1) : outL;
            const float sendL = sendCh > 0 ? sendMeter.getRMSLevel (0) : 0.0f;
            const float sendR = sendCh > 1 ? sendMeter.getRMSLevel (1) : sendL;
            const float l = jmax (outL, sendL);
            const float r = jmax (outR, sendR);
            mo->setProperty ("masterL", shellMeterLinearToDisplay (l));
            mo->setProperty ("masterR", shellMeterLinearToDisplay (r));
            mo->setProperty ("masterLimiter", processor.isMasterLimiterActive());
            mo->setProperty ("masterClip", processor.isMasterClipRecently());
        }
        r->setProperty ("meters", meters);

        var relay (new DynamicObject());
        relay.getDynamicObject()->setProperty ("active", relayStats.active);
        relay.getDynamicObject()->setProperty ("congested", relayStats.congested);
        relay.getDynamicObject()->setProperty ("queueMs", roundToInt (relayStats.oldestQueuedMs));
        relay.getDynamicObject()->setProperty ("queueFrames", relayStats.queuedFrames);
        relay.getDynamicObject()->setProperty ("queueBytes", relayStats.queuedBytes);
        relay.getDynamicObject()->setProperty ("droppedQueued", relayStats.droppedQueuedPackets);
        relay.getDynamicObject()->setProperty ("droppedStale", relayStats.droppedStalePackets);
        relay.getDynamicObject()->setProperty ("writeMs", roundToInt (relayStats.lastWriteBlockMs));
        relay.getDynamicObject()->setProperty ("avgWriteMs", roundToInt (relayStats.avgWriteBlockMs));
        relay.getDynamicObject()->setProperty ("maxWriteMs", roundToInt (relayStats.maxWriteBlockMs));
        relay.getDynamicObject()->setProperty ("text", relayStatsText);
        r->setProperty ("relay", relay);

        r->setProperty ("options", opts);

        var transport (new DynamicObject());
        transport.getDynamicObject()->setProperty ("mode", String ((int) currConnectionInfo.transportMode));
        r->setProperty ("transport", transport);

        auto* audioDeviceManager = getAudioDeviceManager ? getAudioDeviceManager() : nullptr;
        var audio (new DynamicObject());
        audio.getDynamicObject()->setProperty ("available", audioDeviceManager != nullptr);
        if (audioDeviceManager != nullptr)
        {
            auto* adm = audioDeviceManager;
            auto setup = adm->getAudioDeviceSetup();
            audio.getDynamicObject()->setProperty ("deviceType", adm->getCurrentAudioDeviceType());
            audio.getDynamicObject()->setProperty ("inputDevice", setup.inputDeviceName);
            audio.getDynamicObject()->setProperty ("outputDevice", setup.outputDeviceName);
            audio.getDynamicObject()->setProperty ("sampleRate", String (roundToInt (setup.sampleRate)));
            audio.getDynamicObject()->setProperty ("buffer", String (setup.bufferSize));

            StringArray typeNames;
            const auto& types = adm->getAvailableDeviceTypes();
            for (auto* type : types)
                if (type != nullptr)
                    typeNames.add (type->getTypeName());
            audio.getDynamicObject()->setProperty ("deviceTypeOptions", makeStringOptions (typeNames));

            if (auto* currentType = adm->getCurrentDeviceTypeObject())
            {
                audio.getDynamicObject()->setProperty ("inputDeviceOptions", makeStringOptions (currentType->getDeviceNames (true)));
                audio.getDynamicObject()->setProperty ("outputDeviceOptions", makeStringOptions (currentType->getDeviceNames (false)));
            }

            if (auto* currentDevice = adm->getCurrentAudioDevice())
            {
                audio.getDynamicObject()->setProperty ("sampleRateOptions", makeSampleRateOptions (currentDevice->getAvailableSampleRates()));
                audio.getDynamicObject()->setProperty ("bufferOptions", makeIntOptions (currentDevice->getAvailableBufferSizes()));
            }
        }
        r->setProperty ("audio", audio);

        var metro (new DynamicObject());
        metro.getDynamicObject()->setProperty ("gain", roundToInt (paramToUiPercent (metg)));
        metro.getDynamicObject()->setProperty ("tempo", roundToInt (paramToUiLinear (mett, 10.0f, 400.0f)));
        auto* metOn = processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramMetEnabled);
        metro.getDynamicObject()->setProperty ("on", metOn != nullptr && metOn->getValue() > 0.5f);
        r->setProperty ("metro", metro);

        var rev (new DynamicObject());
        rev.getDynamicObject()->setProperty ("level", roundToInt (paramToUiPercent (mrev)));
        r->setProperty ("reverb", rev);

        var rec (new DynamicObject());
        String rf ("FLAC");
        switch (processor.getDefaultRecordingFormat())
        {
            case SonobusAudioProcessor::FileFormatWAV:
                rf = "WAV";
                break;
            case SonobusAudioProcessor::FileFormatOGG:
                rf = "OGG";
                break;
            default:
                break;
        }
        rec.getDynamicObject()->setProperty ("format", rf);
        rec.getDynamicObject()->setProperty ("isRecording", processor.isRecordingToFile());
        rec.getDynamicObject()->setProperty ("elapsed", roundToInt (processor.getElapsedRecordTime()));
        auto recDir = processor.getDefaultRecordingDirectory();
        String recDirText;
        if (recDir.isLocalFile())
            recDirText = recDir.getLocalFile().getFullPathName();
        else if (! recDir.isEmpty())
            recDirText = recDir.toString (true);
        rec.getDynamicObject()->setProperty ("folder", recDirText);
        r->setProperty ("record", rec);

        String statusKey ("disconnected");
        String statusGroup;
        if (processor.isConnectedToServer())
        {
            const String g = processor.getCurrentJoinedGroup();
            if (g.isNotEmpty())
            {
                statusKey = "connectedGroup";
                statusGroup = g;
            }
            else
                statusKey = "connectedServer";
        }
        r->setProperty ("uiStatusKey", statusKey);
        r->setProperty ("uiStatusGroup", statusGroup);
        r->setProperty ("uiStatusLive", processor.isConnectedToServer());

        var togg (new DynamicObject());
        auto setBool = [&] (const char* key, const String& pid)
        {
            if (auto* pr = processor.getValueTreeState().getParameter (pid))
                togg.getDynamicObject()->setProperty (key, pr->getValue() > 0.5f);
        };
        setBool ("dynResample", SonobusAudioProcessor::paramDynamicResampling);
        setBool ("reconnect", SonobusAudioProcessor::paramAutoReconnectLast);
        setBool ("metroSend", SonobusAudioProcessor::paramSendMetAudio);
        setBool ("metroRecorded", SonobusAudioProcessor::paramMetIsRecorded);
        setBool ("metroSyncHost", SonobusAudioProcessor::paramSyncMetToHost);
        setBool ("reverbOn", SonobusAudioProcessor::paramMainReverbEnabled);
        setBool ("mainSendMuted", SonobusAudioProcessor::paramMainSendMute);

        if (getShouldCheckForNewVersionValue && getShouldCheckForNewVersionValue())
            togg.getDynamicObject()->setProperty ("checkUpdate", getShouldCheckForNewVersionValue()->getValue());
        if (getAllowBluetoothInputValue && getAllowBluetoothInputValue())
            togg.getDynamicObject()->setProperty ("bluetooth", getAllowBluetoothInputValue()->getValue());
        togg.getDynamicObject()->setProperty ("sliderSnap", processor.getSlidersSnapToMousePosition());

        r->setProperty ("toggles", togg);
    }

    const String stateJson = JSON::toString (root, true);
    if (force || stateJson != lastStateJson)
    {
        lastStateJson = stateJson;
        const String statePayload = toBase64Utf8 (stateJson);
        const String js = String ("if(window.CookieLinkShell&&window.CookieLinkShell.setStateB64)window.CookieLinkShell.setStateB64(")
                          + toJsStringLiteral (statePayload) + ");void(0);";
        invokeJs (js);
    }

    Array<var> peers;
    const int n = processor.getNumberRemotePeers();

    const int64 byteNowMs = Time::getMillisecondCounterHiRes();
    const bool haveBytePrev = shellPeerByteSampleTimeMs > 0;
    const float byteDtSec = haveBytePrev ? jmax (0.05f, (float) (byteNowMs - shellPeerByteSampleTimeMs) / 1000.0f)
                                         : 0.25f;
    shellPeerByteSampleTimeMs = byteNowMs;

    while (shellPeerLastRecvBytes.size() < n)
    {
        shellPeerLastRecvBytes.add (-1);
        shellPeerLastSentBytes.add (-1);
    }
    while (shellPeerLastRecvBytes.size() > n)
    {
        shellPeerLastRecvBytes.removeLast();
        shellPeerLastSentBytes.removeLast();
    }

    Array<var> peersLive;

    for (int i = 0; i < n; ++i)
    {
        var peer (new DynamicObject());
        auto* o = peer.getDynamicObject();
        bool initCompleted = false;
        const int bufferMode = (int) processor.getRemotePeerAutoresizeBufferMode (i, initCompleted);
        int sendQuality = processor.getRemotePeerAudioCodecFormat (i);
        if (sendQuality < 0)
            sendQuality = processor.getDefaultAudioCodecFormat();

        const int64 br = processor.getRemotePeerBytesReceived (i);
        const int64 bs = processor.getRemotePeerBytesSent (i);
        float recvKbps = 0.0f;
        float sendKbps = 0.0f;
        if (haveBytePrev && shellPeerLastRecvBytes.getReference (i) >= 0)
        {
            recvKbps = (float) ((br - shellPeerLastRecvBytes.getReference (i)) * 8.0 / (1000.0 * (double) byteDtSec));
            sendKbps = (float) ((bs - shellPeerLastSentBytes.getReference (i)) * 8.0 / (1000.0 * (double) byteDtSec));
        }
        shellPeerLastRecvBytes.set (i, br);
        shellPeerLastSentBytes.set (i, bs);

        String netRatesText;
        netRatesText << "收 " << roundToInt (recvKbps) << " kb/s · 发 " << roundToInt (sendKbps) << " kb/s";

        const float g = processor.getRemotePeerLevelGain (i);
        const float volDb = (g > 1.0e-8f) ? jlimit (-48.0f, 6.0f, juce::Decibels::gainToDecibels (g, -200.0f))
                                         : -48.0f;

        SonobusAudioProcessor::LatencyInfo latinfo;
        processor.getRemotePeerLatencyInfo (i, latinfo);
        String latencyText;
        if (latinfo.pingMs > 0.5f)
            latencyText << "Ping " << roundToInt (latinfo.pingMs) << " ms";
        if (latinfo.incomingMs > 0.5f || latinfo.outgoingMs > 0.5f)
        {
            if (latencyText.isNotEmpty())
                latencyText << " · ";
            latencyText << "收 " << roundToInt (latinfo.incomingMs) << " / 发 " << roundToInt (latinfo.outgoingMs) << " ms";
        }
        else if (latinfo.totalRoundtripMs > 0.5f)
        {
            if (latencyText.isNotEmpty())
                latencyText << " · ";
            latencyText << "往返 " << roundToInt (latinfo.totalRoundtripMs) << " ms";
        }

        if (auto* recvM = processor.getRemotePeerRecvMeterSource (i))
        {
            recvM->decayIfNeeded();
            const int nm = recvM->getNumChannels();
            const float ml = nm > 0 ? recvM->getRMSLevel (0) : 0.0f;
            const float mr = nm > 1 ? recvM->getRMSLevel (1) : ml;
            o->setProperty ("meterL", shellMeterLinearToDisplay (ml));
            o->setProperty ("meterR", shellMeterLinearToDisplay (mr));
        }
        else
        {
            o->setProperty ("meterL", 0.0f);
            o->setProperty ("meterR", 0.0f);
        }

        o->setProperty ("index", i);
        String peerName = processor.getRemotePeerDisplayName (i);
        if (peerName.isEmpty())
            peerName = processor.getRemotePeerUserName (i);
        o->setProperty ("name", peerName);
        o->setProperty ("recvMuted", !processor.getRemotePeerRecvAllow (i, false));
        o->setProperty ("solo", processor.getRemotePeerSoloed (i));
        o->setProperty ("volumeDb", roundToInt (volDb * 10.0f) / 10.0f);
        o->setProperty ("sendQualityIndex", sendQuality);
        int recvQ = processor.getRequestRemotePeerSendAudioCodecFormat (i);
        if (recvQ < 0)
        {
            SonobusAudioProcessor::AudioCodecFormatInfo recvInfo;
            if (processor.getRemotePeerReceiveAudioCodecFormat (i, recvInfo))
                recvQ = shellIndexOfFormatInfo (processor, recvInfo);
        }
        o->setProperty ("recvQualityIndex", recvQ);
        o->setProperty ("latencyText", latencyText);
        o->setProperty ("bufferMs", roundToInt (processor.getRemotePeerBufferTime (i)));
        o->setProperty ("bufferModeIndex", bufferMode);
        o->setProperty ("netRatesText", netRatesText);
        o->setProperty ("relayStatsText", processor.isTcpRelayMode() && relayStats.active ? relayStatsText : String());
        o->setProperty ("relayCongested", processor.isTcpRelayMode() && relayStats.congested);
        peers.add (peer);

        var liveRow (new DynamicObject());
        liveRow.getDynamicObject()->setProperty ("index", i);
        liveRow.getDynamicObject()->setProperty ("netRatesText", netRatesText);
        liveRow.getDynamicObject()->setProperty ("meterL", o->getProperty ("meterL"));
        liveRow.getDynamicObject()->setProperty ("meterR", o->getProperty ("meterR"));
        liveRow.getDynamicObject()->setProperty ("latencyText", latencyText);
        liveRow.getDynamicObject()->setProperty ("relayStatsText", processor.isTcpRelayMode() && relayStats.active ? relayStatsText : String());
        liveRow.getDynamicObject()->setProperty ("relayCongested", processor.isTcpRelayMode() && relayStats.congested);
        liveRow.getDynamicObject()->setProperty ("recvMuted", !processor.getRemotePeerRecvAllow (i, false));
        liveRow.getDynamicObject()->setProperty ("solo", processor.getRemotePeerSoloed (i));
        peersLive.add (liveRow);
    }
    const String peersJson = JSON::toString (var (peers), true);
    const String peersStableJson = makeStablePeersJson (peers);
    const bool fullPeers = force || peersStableJson != lastPeersStableJson;

    if (fullPeers)
    {
        lastPeersStableJson = peersStableJson;
        const String pjs = String ("if(window.CookieLinkShell&&window.CookieLinkShell.renderPeersDataB64)window.CookieLinkShell.renderPeersDataB64(")
                           + toJsStringLiteral (toBase64Utf8 (peersJson)) + ");void(0);";
        invokeJs (pjs);
    }
    else if (n > 0)
    {
        const String liveJson = JSON::toString (var (peersLive), true);
        const String lj = String ("if(window.CookieLinkShell&&window.CookieLinkShell.updatePeersLiveB64)window.CookieLinkShell.updatePeersLiveB64(")
                          + toJsStringLiteral (toBase64Utf8 (liveJson)) + ");void(0);";
        invokeJs (lj);
    }
}

void DesktopShellEditor::timerCallback (int timerId)
{
    if (timerId == kUiRefreshTimerId)
    {
        const int64 now = Time::getApproximateMillisecondCounter();

        const int nPeers = processor.getNumberRemotePeers();
        if (! processor.isConnectedToServer() || nPeers == 0)
        {
            lastPeerLatencyAutoMs = 0;
        }
        else if (lastPeerLatencyAutoMs == 0 || now - lastPeerLatencyAutoMs >= 5000)
        {
            lastPeerLatencyAutoMs = now;
            const SafePointer<DesktopShellEditor> safe (this);
            for (int i = 0; i < nPeers; ++i)
            {
                processor.stopRemotePeerLatencyTest (i);
                processor.startRemotePeerLatencyTest (i);
            }
            Timer::callAfterDelay (1500, [safe]()
                                   {
                                       if (safe == nullptr)
                                           return;
                                       const int n = safe->processor.getNumberRemotePeers();
                                       for (int i = 0; i < n; ++i)
                                           safe->processor.stopRemotePeerLatencyTest (i);
                                       safe->pushStateToWeb (true);
                                   });
        }

        pushStateToWeb (false);
    }
}

void DesktopShellEditor::handleBridgeFromWebView (const String& url)
{
    URL u (url);
    const int pi = u.getParameterNames().indexOf ("b", false, 0);
    const String b64 = pi >= 0 ? u.getParameterValues()[pi] : String();
    MemoryOutputStream mos;
    if (! Base64::convertFromBase64 (mos, b64))
        return;
    const var parsed = JSON::parse (mos.toString());
    if (! parsed.isObject())
        return;
    const String ev = parsed.getProperty ("ev", {}).toString();
    const var detail = parsed.getProperty ("detail", {});

    if (ev == "change")
    {
        const String key = detail.getProperty ("key", {}).toString();
        const var val = detail.getProperty ("value", {});

        bool shouldSaveSettings = true;
        bool shouldPushState = true;

        if (key == "connect.displayName")
        {
            currConnectionInfo.userName = val.toString();
            processor.setCurrentUsername (currConnectionInfo.userName.trim());
            shouldSaveSettings = false;
            shouldPushState = false;
        }
        else if (key == "connect.groupName")
        {
            currConnectionInfo.groupName = val.toString();
            shouldSaveSettings = false;
            shouldPushState = false;
        }
        else if (key == "connect.password")
        {
            currConnectionInfo.groupPassword = val.toString();
            shouldSaveSettings = false;
            shouldPushState = false;
        }
        else if (key == "connect.serverUrl")
        {
            currServerUrlText = val.toString().trim();
            applyServerUrlText (currServerUrlText, (int) currConnectionInfo.transportMode, currConnectionInfo);
            currServerUrlText = makeServerUrlText (currConnectionInfo);
            shouldSaveSettings = false;
        }
        else if (key == "transport.mode")
        {
            const int mode = val.toString().getIntValue();
            if (mode == SonobusAudioProcessor::TransportModeUdpP2P || mode == SonobusAudioProcessor::TransportModeTcpRelay)
            {
                currConnectionInfo.transportMode = mode;
                applyServerUrlText (currServerUrlText, mode, currConnectionInfo);
                currServerUrlText = makeServerUrlText (currConnectionInfo);
                processor.setTransportMode ((SonobusAudioProcessor::TransportMode) mode);
            }
        }
        else if (key == "audio.deviceType")
        {
            if (getAudioDeviceManager && getAudioDeviceManager() != nullptr)
            {
                auto* adm = getAudioDeviceManager();
                (void) adm->getAvailableDeviceTypes();
                adm->setCurrentAudioDeviceType (val.toString(), true);
            }
        }
        else if (key == "audio.inputDevice")
        {
            if (getAudioDeviceManager && getAudioDeviceManager() != nullptr)
            {
                auto* adm = getAudioDeviceManager();
                auto setup = adm->getAudioDeviceSetup();
                setup.inputDeviceName = val.toString();
                adm->setAudioDeviceSetup (setup, true);
            }
        }
        else if (key == "audio.outputDevice")
        {
            if (getAudioDeviceManager && getAudioDeviceManager() != nullptr)
            {
                auto* adm = getAudioDeviceManager();
                auto setup = adm->getAudioDeviceSetup();
                setup.outputDeviceName = val.toString();
                adm->setAudioDeviceSetup (setup, true);
            }
        }
        else if (key == "audio.sampleRate")
        {
            if (getAudioDeviceManager && getAudioDeviceManager() != nullptr)
            {
                auto* adm = getAudioDeviceManager();
                auto setup = adm->getAudioDeviceSetup();
                setup.sampleRate = val.toString().getDoubleValue();
                adm->setAudioDeviceSetup (setup, true);
            }
        }
        else if (key == "audio.buffer")
        {
            if (getAudioDeviceManager && getAudioDeviceManager() != nullptr)
            {
                auto* adm = getAudioDeviceManager();
                auto setup = adm->getAudioDeviceSetup();
                setup.bufferSize = val.toString().getIntValue();
                adm->setAudioDeviceSetup (setup, true);
            }
        }
        else if (key == "mixer.inLevel")
            setShellParamGainFromDb (dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramInGain)),
                                     (float) (double) val, shellMixerDbMin, shellInDbMax);
        else if (key == "mixer.monitorLevel")
            setShellParamGainFromDb (dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramDry)),
                                     (float) (double) val, shellMixerDbMin, shellMonitorDbMax);
        else if (key == "mixer.outLevel")
            setShellParamGainFromDb (dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramWet)),
                                     (float) (double) val, shellMixerDbMin, shellSendDbMax);
        else if (key == "options.jitterMs")
        {
            if (auto* p = dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramDefaultNetbufMs)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) (double) val));
        }
        else if (key == "options.bufferModeIndex")
        {
            if (auto* p = dynamic_cast<AudioParameterChoice*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramDefaultAutoNetbuf)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) val.toString().getIntValue()));
        }
        else if (key == "options.sendFormatIndex")
        {
            if (auto* p = dynamic_cast<AudioParameterChoice*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramSendChannels)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) val.toString().getIntValue()));
        }
        else if (key == "options.sendQualityIndex")
        {
            int requestedQuality = val.toString().getIntValue();
            if (auto* p = dynamic_cast<AudioParameterInt*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramDefaultSendQual)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) requestedQuality));
        }
        else if (key == "record.format")
        {
            const String fmt = val.toString();
            if (fmt == "FLAC") processor.setDefaultRecordingFormat (SonobusAudioProcessor::FileFormatFLAC);
            else if (fmt == "WAV") processor.setDefaultRecordingFormat (SonobusAudioProcessor::FileFormatWAV);
            else if (fmt == "OGG") processor.setDefaultRecordingFormat (SonobusAudioProcessor::FileFormatOGG);
        }
        else if (key == "metro.gain")
            setParamFromUiPercent (dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramMetGain)),
                                   (float) (double) val);
        else if (key == "metro.tempo")
            setParamFromUiLinear (dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramMetTempo)),
                                  (float) (double) val, 10.0f, 400.0f);
        else if (key == "metro.on")
        {
            if (auto* p = processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramMetEnabled))
                p->setValueNotifyingHost ((bool) val ? 1.0f : 0.0f);
        }
        else if (key == "reverb.level")
            setParamFromUiPercent (dynamic_cast<RangedAudioParameter*> (processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramMainReverbLevel)),
                                   (float) (double) val);
        else if (key.startsWith ("toggle."))
        {
            const String tk = key.fromFirstOccurrenceOf ("toggle.", false, false);
            bool on = (bool) val;
            auto setBoolParam = [&] (const String& pid)
            {
                if (auto* p = processor.getValueTreeState().getParameter (pid))
                    p->setValueNotifyingHost (on ? 1.0f : 0.0f);
            };
            if (tk == "dynResample") setBoolParam (SonobusAudioProcessor::paramDynamicResampling);
            else if (tk == "reconnect") setBoolParam (SonobusAudioProcessor::paramAutoReconnectLast);
            else if (tk == "metroSend") setBoolParam (SonobusAudioProcessor::paramSendMetAudio);
            else if (tk == "metroRecorded") setBoolParam (SonobusAudioProcessor::paramMetIsRecorded);
            else if (tk == "metroSyncHost") setBoolParam (SonobusAudioProcessor::paramSyncMetToHost);
            else if (tk == "reverbOn") setBoolParam (SonobusAudioProcessor::paramMainReverbEnabled);
            else if (tk == "checkUpdate" && getShouldCheckForNewVersionValue && getShouldCheckForNewVersionValue())
                getShouldCheckForNewVersionValue()->setValue (on);
            else if (tk == "bluetooth" && getAllowBluetoothInputValue && getAllowBluetoothInputValue())
                getAllowBluetoothInputValue()->setValue (on);
            else if (tk == "sliderSnap")
                processor.setSlidersSnapToMousePosition (on);
            else if (tk == "mainSendMuted")
                setBoolParam (SonobusAudioProcessor::paramMainSendMute);
        }

        if (shouldSaveSettings && saveSettingsIfNeeded)
            saveSettingsIfNeeded();
        if (shouldPushState)
            pushStateToWeb (true);
    }
    else if (ev == "action")
    {
        const String action = detail.getProperty ("action", {}).toString();
        const var st = detail.getProperty ("state", var());
        if (action == "joinGroup") applyConnectFromShellState (st, false);
        else if (action == "disconnect") shellPerformDisconnect();
        else if (action == "copyInfo") shellCopyInfo();
        else if (action == "pasteInfo") shellPasteInfo();
        else if (action == "chatSend")
        {
            const String msg = st.getProperty ("chat", var()).getProperty ("draft", {}).toString().trim();
            if (msg.isNotEmpty())
            {
                SBChatEvent ev2;
                ev2.type = SBChatEvent::UserType;
                ev2.from = currConnectionInfo.userName;
                ev2.group = currConnectionInfo.groupName;
                ev2.message = msg;
                processor.sendChatEvent (ev2);
            }
        }
        else if (action == "openFile")
        {
            const SafePointer<DesktopShellEditor> safe (this);
            shellFileChooser = std::make_unique<FileChooser> (TRANS ("打开音频文件"), File(), "*.wav;*.flac;*.ogg;*.mp3;*.aif;*.aiff");
            shellFileChooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                           [safe] (const FileChooser& c)
                                           {
                                               if (safe == nullptr) return;
                                               const auto f = c.getResult();
                                               if (f.existsAsFile())
                                                   safe->processor.loadURLIntoTransport (URL (f));
                                               safe->shellFileChooser.reset();
                                           });
        }
        else if (action == "toggleRecord")
        {
            if (processor.isRecordingToFile())
                processor.stopRecordingToFile();
            else
            {
                String filename = (currConnectionInfo.groupName.isEmpty() ? "CookieLinkSession" : currConnectionInfo.groupName)
                                  + "_" + Time::getCurrentTime().formatted ("%Y-%m-%d_%H.%M.%S");
                filename = File::createLegalFileName (filename);
                auto parentDirUrl = processor.getDefaultRecordingDirectory();
                if (parentDirUrl.isEmpty())
                    return;
                if (parentDirUrl.isLocalFile())
                    parentDirUrl.getLocalFile().createDirectory();
                filename += recordFormatExtension (processor.getDefaultRecordingFormat());
                URL ret;
                processor.startRecordingToFile (parentDirUrl, filename, ret);
            }
        }
        else if (action == "chooseRecordFolder")
        {
            const SafePointer<DesktopShellEditor> safe (this);
            File recDir;
            if (processor.getDefaultRecordingDirectory().isLocalFile())
                recDir = processor.getDefaultRecordingDirectory().getLocalFile();

            shellFileChooser = std::make_unique<FileChooser> (TRANS ("选择录音文件夹"), recDir, "", true, false, getTopLevelComponent());
            shellFileChooser->launchAsync (FileBrowserComponent::canSelectDirectories | FileBrowserComponent::openMode,
                                           [safe] (const FileChooser& c)
                                           {
                                               if (safe == nullptr) return;
                                               auto results = c.getURLResults();
                                               if (results.size() > 0)
                                               {
                                                   auto url = results.getReference (0);
                                                   if (url.isLocalFile())
                                                   {
                                                       auto local = url.getLocalFile();
                                                       if (! local.isDirectory())
                                                           local = local.getParentDirectory();
                                                       safe->processor.setDefaultRecordingDirectory (URL (local));
                                                   }
                                                   else if (! url.isEmpty())
                                                   {
                                                       safe->processor.setDefaultRecordingDirectory (url);
                                                   }

                                                   if (safe->saveSettingsIfNeeded)
                                                       safe->saveSettingsIfNeeded();
                                                   safe->pushStateToWeb (true);
                                               }
                                               safe->shellFileChooser.reset();
                                           });
        }
        else if (action == "audioPanel")
        {
            if (getAudioDeviceManager && getAudioDeviceManager() != nullptr)
            {
                auto* adm = getAudioDeviceManager();
                const int maxIn = jmax (1, processor.getMainBusNumInputChannels());
                const int maxOut = jmax (1, processor.getMainBusNumOutputChannels());
                auto* selector = new AudioDeviceSelectorComponent (*adm, 0, maxIn, 0, maxOut, false, false, false, false);
                selector->setSize (520, 420);
                DialogWindow::LaunchOptions o;
                o.dialogTitle = TRANS ("音频设备");
                o.dialogBackgroundColour = Colours::darkgrey;
                o.escapeKeyTriggersCloseButton = true;
                o.useNativeTitleBar = true;
                o.resizable = false;
                o.content.setOwned (selector);
                o.launchAsync();
            }
        }
        else if (action == "audioTest")
        {
            if (getAudioDeviceManager && getAudioDeviceManager() != nullptr)
                getAudioDeviceManager()->playTestSound();
        }
        else if (action == "toggleMainSendMute")
        {
            if (auto* p = processor.getValueTreeState().getParameter (SonobusAudioProcessor::paramMainSendMute))
                p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
        }

        if (saveSettingsIfNeeded)
            saveSettingsIfNeeded();
        pushStateToWeb (true);
    }
    else if (ev == "peer")
        handlePeerUiAction (detail);
}

void DesktopShellEditor::handlePeerUiAction (const var& detail)
{
    const int idx = (int) detail.getProperty ("index", 0);
    const String act = detail.getProperty ("act", {}).toString();
    if (idx < 0 || idx >= processor.getNumberRemotePeers())
        return;
    const String field = detail.getProperty ("field", {}).toString();
    if (field == "volume")
    {
        const float db = detail.getProperty ("value", {}).toString().getFloatValue();
        const float lin = jlimit (0.0f, 2.0f, juce::Decibels::decibelsToGain (db));
        processor.setRemotePeerLevelGain (idx, lin);
    }
    else if (field == "sendQuality")
        processor.setRemotePeerAudioCodecFormat (idx, detail.getProperty ("value", {}).toString().getIntValue());
    else if (field == "recvQuality")
    {
        const int fmt = detail.getProperty ("value", {}).toString().getIntValue();
        if (fmt >= 0)
            processor.setRequestRemotePeerSendAudioCodecFormat (idx, fmt);
    }
    else if (field == "bufferMs")
        processor.setRemotePeerBufferTime (idx, jlimit (0.0f, 500.0f, detail.getProperty ("value", {}).toString().getFloatValue()));
    else if (field == "bufferMode")
        processor.setRemotePeerAutoresizeBufferMode (idx, (SonobusAudioProcessor::AutoNetBufferMode) detail.getProperty ("value", {}).toString().getIntValue());
    else if (act == "mute")
    {
        const bool allowed = processor.getRemotePeerRecvAllow (idx, false);
        if (allowed)
            processor.setRemotePeerRecvAllow (idx, false);
        else
            processor.setRemotePeerRecvActive (idx, true);
    }
    else if (act == "solo")
    {
        const bool cur = processor.getRemotePeerSoloed (idx);
        processor.setRemotePeerSoloed (idx, !cur);
    }
    else if (act == "latency")
    {
        if (processor.isRemotePeerLatencyTestActive (idx))
            processor.stopRemotePeerLatencyTest (idx);
        else
            processor.startRemotePeerLatencyTest (idx);
    }
    pushStateToWeb (true);
}

void DesktopShellEditor::applyConnectFromShellState (const var& stateVar, bool copyInfoOnly)
{
    const var c = stateVar.getProperty ("connect", var());
    currConnectionInfo.userName = c.getProperty ("displayName", {}).toString().trim();
    currConnectionInfo.groupName = c.getProperty ("groupName", {}).toString().trim();
    currConnectionInfo.groupPassword = c.getProperty ("password", {}).toString();
    processor.setCurrentUsername (currConnectionInfo.userName);

    const var t = stateVar.getProperty ("transport", var());
    int mode = t.getProperty ("mode", String ((int) currConnectionInfo.transportMode)).toString().getIntValue();
    if (mode != SonobusAudioProcessor::TransportModeUdpP2P && mode != SonobusAudioProcessor::TransportModeTcpRelay)
        mode = SonobusAudioProcessor::TransportModeUdpP2P;
    currConnectionInfo.transportMode = mode;
    currServerUrlText = c.getProperty ("serverUrl", currServerUrlText).toString().trim();
    applyServerUrlText (currServerUrlText, mode, currConnectionInfo);
    currServerUrlText = makeServerUrlText (currConnectionInfo);
    processor.setTransportMode ((SonobusAudioProcessor::TransportMode) mode);

    if (copyInfoOnly)
    {
        shellCopyInfo();
        return;
    }

    if (currConnectionInfo.groupName.isEmpty())
        return;
    if (currConnectionInfo.userName.isEmpty())
        return;

    connectWithInfo (currConnectionInfo, false, false);
}

void DesktopShellEditor::connectWithInfo (const AooServerConnectionInfo& info, bool allowEmptyGroup, bool copyInfoOnly)
{
    const AooServerConnectionInfo prevInfo = currConnectionInfo;
    currConnectionInfo = info;
    selectedServerId.clear();
    currServerUrlText = makeServerUrlText (currConnectionInfo);

    if (copyInfoOnly)
    {
        URL url2 ("cookielink://join");
        if (currConnectionInfo.groupName.isNotEmpty())
        {
            url2 = url2.withParameter ("s", makeServerUrlText (currConnectionInfo))
                       .withParameter ("m", String (currConnectionInfo.transportMode))
                       .withParameter ("g", currConnectionInfo.groupName);
            if (currConnectionInfo.groupPassword.isNotEmpty())
                url2 = url2.withParameter ("p", currConnectionInfo.groupPassword);
            if (processor.isConnectedToServer() && currConnectionInfo.groupIsPublic)
                url2 = url2.withParameter ("public", "1");
            SystemClipboard::copyTextToClipboard (url2.toString (true));
        }
        return;
    }

    const bool wasConnected = processor.isConnectedToServer();
    AooServerConnectionInfo normalized = currConnectionInfo;
    normalized.userName = normalized.userName.trim();
    normalized.groupName = normalized.groupName.trim();
    normalized.serverHost = normalized.serverHost.trim();
    currConnectionInfo = normalized;
    processor.setTransportMode ((SonobusAudioProcessor::TransportMode) currConnectionInfo.transportMode);

    if (wasConnected && currConnectionInfo == prevInfo && processor.getCurrentJoinedGroup() == currConnectionInfo.groupName)
        return;

    if (currConnectionInfo.groupName.isEmpty() && !allowEmptyGroup)
        return;
    if (currConnectionInfo.userName.trim().isEmpty())
        return;

    const bool sameServerAndUser = wasConnected && currConnectionInfo.serverHost == prevInfo.serverHost
                                   && currConnectionInfo.serverPort == prevInfo.serverPort
                                   && currConnectionInfo.transportMode == prevInfo.transportMode
                                   && currConnectionInfo.userName == prevInfo.userName
                                   && currConnectionInfo.userPassword == prevInfo.userPassword;

    if (sameServerAndUser && currConnectionInfo.groupName.isNotEmpty())
    {
        const auto currentGroup = processor.getCurrentJoinedGroup();
        const bool groupChanged = (currentGroup != currConnectionInfo.groupName);
        const bool passwordChanged = (currConnectionInfo.groupPassword != prevInfo.groupPassword)
                                     || (currConnectionInfo.groupIsPublic != prevInfo.groupIsPublic);
        if (currentGroup.isEmpty() || groupChanged || passwordChanged)
        {
            currConnectionInfo.timestamp = Time::getCurrentTime().toMilliseconds();
            processor.addRecentServerConnectionInfo (currConnectionInfo);
            processor.setWatchPublicGroups (false);
            if (!currentGroup.isEmpty() && groupChanged)
                processor.leaveServerGroup (currentGroup);
            processor.joinServerGroup (currConnectionInfo.groupName, currConnectionInfo.groupPassword, currConnectionInfo.groupIsPublic);
        }
        return;
    }

    if (currConnectionInfo.serverHost.isNotEmpty() && currConnectionInfo.serverPort != 0)
    {
        if (wasConnected)
            processor.disconnectFromServer ("DesktopShellEditor::connectWithInfo");

        Timer::callAfterDelay (100, [this]
                               { processor.connectToServer (currConnectionInfo.serverHost, currConnectionInfo.serverPort, currConnectionInfo.userName, currConnectionInfo.userPassword); });
    }
}

void DesktopShellEditor::shellPerformDisconnect()
{
    if (processor.isConnectedToServer() && processor.getCurrentJoinedGroup().isNotEmpty())
    {
        if (processor.getWatchPublicGroups())
            processor.leaveServerGroup (processor.getCurrentJoinedGroup());
        else
            processor.disconnectFromServer ("DesktopShellEditor::disconnect");
    }
    else if (processor.isConnectedToServer())
    {
        processor.disconnectFromServer ("DesktopShellEditor::disconnect");
    }
}

void DesktopShellEditor::shellCopyInfo() { connectWithInfo (currConnectionInfo, false, true); }

void DesktopShellEditor::shellPasteInfo()
{
    const auto clip = SystemClipboard::getTextFromClipboard();
    if (clip.isEmpty()) return;
    String urlpart = clip.fromFirstOccurrenceOf ("cookielink://", true, true);
    if (urlpart.isNotEmpty())
    {
        urlpart = urlpart.upToFirstOccurrenceOf ("\n", false, true).trim();
        urlpart = urlpart.upToFirstOccurrenceOf (" ", false, true).trim();
        applyCookieLinkLaunchURL (URL (urlpart));
        return;
    }
    urlpart = clip.fromFirstOccurrenceOf ("https://go.cookielink.local/sblaunch?", true, false);
    if (urlpart.isEmpty())
        urlpart = clip.fromFirstOccurrenceOf ("http://go.cookielink.local/sblaunch?", true, false);
    if (urlpart.isNotEmpty())
    {
        urlpart = urlpart.upToFirstOccurrenceOf ("\n", false, true).trim();
        urlpart = urlpart.upToFirstOccurrenceOf (" ", false, true).trim();
        applyCookieLinkLaunchURL (URL (urlpart));
    }
}

void DesktopShellEditor::applyCookieLinkLaunchURL (const URL& url)
{
    auto& pnames = url.getParameterNames();
    auto& pvals = url.getParameterValues();
    int ind;
    if ((ind = pnames.indexOf ("g", true)) >= 0)
    {
        currConnectionInfo.groupName = pvals[ind];
        if ((ind = pnames.indexOf ("p", true)) >= 0)
            currConnectionInfo.groupPassword = pvals[ind];
        else
            currConnectionInfo.groupPassword.clear();
        if ((ind = pnames.indexOf ("public", true)) >= 0)
            currConnectionInfo.groupIsPublic = pvals[ind].getIntValue() > 0;
        else
            currConnectionInfo.groupIsPublic = false;
    }

    int mode = (int) processor.getTransportMode();
    bool modeProvided = false;
    if ((ind = pnames.indexOf ("m", true)) >= 0)
    {
        mode = pvals[ind].getIntValue();
        modeProvided = true;
    }
    if (mode != SonobusAudioProcessor::TransportModeUdpP2P && mode != SonobusAudioProcessor::TransportModeTcpRelay)
        mode = (int) processor.getTransportMode();

    if ((ind = pnames.indexOf ("sid", true)) >= 0)
    {
        selectedServerId = pvals[ind].trim();
        if (! shellServerSupportsTransport (selectedServerId, mode))
            selectedServerId = firstShellServerIdSupportingTransport (mode);
        applyShellServerSelection (selectedServerId, mode, true);
    }
    else if ((ind = pnames.indexOf ("s", true)) >= 0)
    {
        applyServerUrlText (pvals[ind], mode, currConnectionInfo);
        if (! modeProvided)
            mode = currConnectionInfo.serverPort == DEFAULT_RELAY_SERVER_PORT ? SonobusAudioProcessor::TransportModeTcpRelay
                                                                              : SonobusAudioProcessor::TransportModeUdpP2P;
        currConnectionInfo.transportMode = mode;
        processor.setTransportMode ((SonobusAudioProcessor::TransportMode) mode);
        selectedServerId.clear();
    }
    else if (url.getScheme() == "cookielink" && url.getDomain().isNotEmpty() && ! url.getDomain().equalsIgnoreCase ("join"))
    {
        String hostpart = url.getDomain();
        currConnectionInfo.serverHost = hostpart.upToFirstOccurrenceOf (":", false, true);
        int port = url.getPort();
        currConnectionInfo.serverPort = port > 0 ? port : defaultServerPortForTransport (mode);
        if (! modeProvided)
            mode = currConnectionInfo.serverPort == DEFAULT_RELAY_SERVER_PORT ? SonobusAudioProcessor::TransportModeTcpRelay
                                                                              : SonobusAudioProcessor::TransportModeUdpP2P;
        currConnectionInfo.transportMode = mode;
        processor.setTransportMode ((SonobusAudioProcessor::TransportMode) mode);
        selectedServerId.clear();
    }
    else
    {
        currConnectionInfo.transportMode = mode;
        if (currConnectionInfo.serverPort <= 0)
            currConnectionInfo.serverPort = defaultServerPortForTransport (mode);
        processor.setTransportMode ((SonobusAudioProcessor::TransportMode) mode);
        selectedServerId.clear();
    }
    currServerUrlText = makeServerUrlText (currConnectionInfo);
    pushStateToWeb (true);
}

void DesktopShellEditor::handleURL (const String& urlstr)
{
    URL url (urlstr);
    if (url.isWellFormed())
        applyCookieLinkLaunchURL (url);
}

bool DesktopShellEditor::loadSettingsFromFile (const File& file)
{
    if (!getAudioDeviceManager || getAudioDeviceManager() == nullptr) return false;
    bool retval = true;
    PropertiesFile::Options opts;
    PropertiesFile propfile (file, opts);
    if (!propfile.isValidFile())
        return false;

    MemoryBlock data;
    if (propfile.containsKey ("filterStateXML"))
    {
        String filtxml = propfile.getValue ("filterStateXML");
        data.replaceAll (filtxml.toUTF8(), (size_t) filtxml.getNumBytesAsUTF8());
        if (data.getSize() > 0)
            processor.setStateInformationWithOptions (data.getData(), (int) data.getSize(), false, true, true);
        else
            retval = false;
    }
    else
    {
        if (data.fromBase64Encoding (propfile.getValue ("filterState")) && data.getSize() > 0)
            processor.setStateInformationWithOptions (data.getData(), (int) data.getSize(), false, true);
        else
            retval = false;
    }

    auto deviceManager = getAudioDeviceManager();
    auto savedAudioState = propfile.getXmlValue ("audioSetup");
    if (savedAudioState.get())
    {
        if (getShouldOverrideSampleRateValue && !((bool) getShouldOverrideSampleRateValue()->getValue()))
        {
            if (savedAudioState->hasAttribute ("audioDeviceRate"))
                savedAudioState->removeAttribute ("audioDeviceRate");
        }
        auto totalInChannels = processor.getMainBusNumInputChannels();
        auto totalOutChannels = processor.getMainBusNumOutputChannels();
        deviceManager->initialise (totalInChannels, totalOutChannels, savedAudioState.get(), true, {}, nullptr);
    }

    pushStateToWeb (true);
    return retval;
}

void DesktopShellEditor::prepareForAppExit()
{
}

bool DesktopShellEditor::requestedQuit()
{
    prepareForAppExit();
    return true;
}

void DesktopShellEditor::aooClientConnected (SonobusAudioProcessor*, bool success, const String&)
{
    if (!success) return;
    if (processor.getCurrentJoinedGroup().isEmpty())
    {
        if (currConnectionInfo.groupName.isNotEmpty())
        {
            currConnectionInfo.timestamp = Time::getCurrentTime().toMilliseconds();
            processor.addRecentServerConnectionInfo (currConnectionInfo);
            const bool relayJoinAlreadyPending = processor.isTcpRelayMode() && processor.isRelayGroupJoinPending();
            if (!relayJoinAlreadyPending)
            {
                processor.setWatchPublicGroups (false);
                processor.joinServerGroup (currConnectionInfo.groupName, currConnectionInfo.groupPassword, currConnectionInfo.groupIsPublic);
            }
        }
        else
            processor.setWatchPublicGroups (true);
    }
    pushStateToWeb (true);
}

void DesktopShellEditor::aooClientDisconnected (SonobusAudioProcessor*, bool, const String&) { pushStateToWeb (true); }
void DesktopShellEditor::aooClientLoginResult (SonobusAudioProcessor*, bool, const String&) { pushStateToWeb (true); }
void DesktopShellEditor::aooClientGroupJoined (SonobusAudioProcessor*, bool, const String&, const String&) { pushStateToWeb (true); }
void DesktopShellEditor::aooClientGroupLeft (SonobusAudioProcessor*, bool, const String&, const String&) { pushStateToWeb (true); }
void DesktopShellEditor::aooClientPeerJoined (SonobusAudioProcessor*, const String&, const String&) { pushStateToWeb (true); }
void DesktopShellEditor::aooClientPeerLeft (SonobusAudioProcessor*, const String&, const String&) { pushStateToWeb (true); }
void DesktopShellEditor::aooClientError (SonobusAudioProcessor*, const String&) { pushStateToWeb (true); }
