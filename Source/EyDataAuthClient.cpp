// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception

#include "EyDataAuthClient.h"

namespace
{
constexpr const char* kEyDataEndpoints[] = {
    "https://vip1.eydata.net/0501E715E85BA066",
    "https://vip2.eydata.net/0501E715E85BA066"
};

constexpr const char* kCardInfoEndpoints[] = {
    "https://vip1.eydata.net/4206B9C2300B57D7",
    "https://vip2.eydata.net/4206B9C2300B57D7"
};

constexpr const char* kExpiredEndpoints[] = {
    "https://vip1.eydata.net/5D2E2C519EC03337",
    "https://vip2.eydata.net/5D2E2C519EC03337"
};

constexpr const char* kBulletinEndpoints[] = {
    "https://vip1.eydata.net/F3214DF9D1A85F20",
    "https://vip2.eydata.net/F3214DF9D1A85F20"
};

constexpr const char* kMacChangeBindEndpoints[] = {
    "https://vip1.eydata.net/017455C3384B0C24",
    "https://vip2.eydata.net/017455C3384B0C24"
};

constexpr const char* kCheckUserStatusEndpoints[] = {
    "https://vip1.eydata.net/2EFF4DFFD4A7D994",
    "https://vip2.eydata.net/2EFF4DFFD4A7D994"
};

constexpr const char* kLogoutEndpoints[] = {
    "https://vip1.eydata.net/B2C73B689A18A97A",
    "https://vip2.eydata.net/B2C73B689A18A97A"
};

constexpr const char* kOnlineCountEndpoints[] = {
    "https://vip1.eydata.net/1C8DAE769BD76254",
    "https://vip2.eydata.net/1C8DAE769BD76254"
};

constexpr const char* kServerListVariableEndpoints[] = {
    "https://vip1.eydata.net/CD717EFAE8218202",
    "https://vip2.eydata.net/CD717EFAE8218202"
};

constexpr int kEyDataEndpointCount = sizeof (kEyDataEndpoints) / sizeof (kEyDataEndpoints[0]);
constexpr int kCardInfoEndpointCount = sizeof (kCardInfoEndpoints) / sizeof (kCardInfoEndpoints[0]);
constexpr int kExpiredEndpointCount = sizeof (kExpiredEndpoints) / sizeof (kExpiredEndpoints[0]);
constexpr int kBulletinEndpointCount = sizeof (kBulletinEndpoints) / sizeof (kBulletinEndpoints[0]);
constexpr int kMacChangeBindEndpointCount = sizeof (kMacChangeBindEndpoints) / sizeof (kMacChangeBindEndpoints[0]);
constexpr int kCheckUserStatusEndpointCount = sizeof (kCheckUserStatusEndpoints) / sizeof (kCheckUserStatusEndpoints[0]);
constexpr int kLogoutEndpointCount = sizeof (kLogoutEndpoints) / sizeof (kLogoutEndpoints[0]);
constexpr int kOnlineCountEndpointCount = sizeof (kOnlineCountEndpoints) / sizeof (kOnlineCountEndpoints[0]);
constexpr int kServerListVariableEndpointCount = sizeof (kServerListVariableEndpoints) / sizeof (kServerListVariableEndpoints[0]);
constexpr const char* kSingleLoginApi = "42977";
constexpr const char* kBulletinApi = "42975";
constexpr const char* kCardInfoApi = "42976";
constexpr const char* kExpiredApi = "42982";
constexpr const char* kMacChangeBindApi = "42978";
constexpr const char* kCheckUserStatusApi = "42979";
constexpr const char* kLogoutApi = "42980";
constexpr const char* kOnlineCountApi = "42981";
constexpr const char* kServerListVariableApi = "43014";
constexpr const char* kServerListVariableIds[] = { "48209", "48210", "48211", "48212", "48213" };
constexpr const char* kServerListVariableNames[] = { "CookieLinkServerList1", "CookieLinkServerList2", "CookieLinkServerList3", "CookieLinkServerList4", "CookieLinkServerList5" };
constexpr int kServerListVariableCount = sizeof (kServerListVariableIds) / sizeof (kServerListVariableIds[0]);
constexpr const char* kAppVersion = "1.0b";
constexpr const char* kAppSecret = "F9979C089DCE428199F489DA63C08637";

constexpr const char* kServerListVariableUpPublicExponent = "010001";
constexpr const char* kServerListVariableUpPublicModulus =
    "aeaf269c6e21c2db8fd9e1ce3ef26e970b9938c9efa64a35fac6efdcfd5aacd50b97fe43841bdaee08300e65f6947fc2f2f60495b30fe3fcfe35424e4f8227ffa00f1fa2785843408cc69d506497b3b591e64a4c311bc9d1d8237655e915610bd0f6d12590eb3dc7b30dd2c37f3f8f4618fcbaefa5c07b49752be049b18e88dd";
constexpr const char* kServerListVariableDownPrivateExponent =
    "3b16d3b0264cf4e14e64d026a459bb85e7f52b7b199e3ed024eb868f12efd341eeedc6a9015e2424962f31b5eb39d3d23f73be84c56d3c40a6ea1257bf281003c4395c0fc0ba866f350b34ad421b174a1f2cf388cfe8df45b3dd38a57f276cf5e9f11b6691fb03db1ac30815dde922b71b654ca23b4441887bf4aa9f92fe56e1";
constexpr const char* kServerListVariableDownPrivateModulus =
    "8ee5cd821dac252a3ce4aabfeb72ade33d6e67c349cf088fec9cd83c65683c054f8967a2ab3cecde477382bfffd8b18e98f04cb5dd0775aef0f2f9c851504992d245b63f5a9a8c4c78fd7dec12fa9ccd04969ddd7a4c9a7acc1880cf0819edbfcff8f594092e130d47be13d27a1f69c72fc026c8f8b66790a836af1b8e1d652f";

constexpr const char* kSingleLoginUpPublicExponent = "010001";
constexpr const char* kSingleLoginUpPublicModulus =
    "91e179361e4059466064240d92d36585a3f9f21e493c29617372e010782cea6e758ebc437a7dbc5c18060ce488b7afd7aa025ca9b5394ddaeb257a371649747642bf4810f5ebfa49c48909a5c546f3bde3328a6d32ef49241cf8186d03e5f255e93cc26f432ff2e89b5f1489c3666c470017279b796cb919fd602794995db2ed";
constexpr const char* kSingleLoginDownPrivateExponent =
    "080320da2491866ae2fd3e1d0aae9df5b223e75b8d2773c75f0a8f883a13c52d9e8f31053f8ea05557c374216701417c95919880903b19143c02553f4ac2c2d1ce77500537f3c6fbdb2acb64f4df7d89e4fc8ace00f0b4e34fd14d5f4a2db11d8e947f16e225bf90ca83a0a59430c637caf461d32bca19b680743935d9e50f61";
constexpr const char* kSingleLoginDownPrivateModulus =
    "8aaea1ad798548b5d5c6276a0bd4dcb493240c50445b05f0195f4db1a2615afc88b86a7ed5541fbaf3dc7965ec1f2272d4ac52fe224759e69b4649ce76bac28a4a608a05fe48f885f009f010d827e355c42fec26bafa2f7abcb84b881a517e5fb53d0a4fed0a6847956bc2a554852dce1470861f6ce0d67db024fcab28467885";

constexpr const char* kBulletinUpPublicExponent = "010001";
constexpr const char* kBulletinUpPublicModulus =
    "96640a2ed58f3406e5dd093de05ac674bb1f6611164db26807bce846dd2a127731c9ce801425d04fd639de55031f30adb5bc8148b6f36cb8617f63f87bfe3e35e95bd2f78e666f59d7b8dad83db42a183e48a9b410cb2d7c061e139d9bee898d32309c0fabccf28aaba416105370242ebf708323b3c01c1bfe29634d89798d59";
constexpr const char* kBulletinDownPrivateExponent =
    "07171be03474db5cb7f07f25f3082649bd24ab8620212ef769a894df34815a7c04f6e928165248591b12bfb19e17e478414ba68931e59a7efbfefe4f4a824cd8d67887319089ab0c01eb6ab2f6a5e39efcf40e740a63dd2bf86e4e9cc300933a7b1dd6dcfa9e5f0441e993eb0942cff2377251584e66fd0b0badf45436e76945";
constexpr const char* kBulletinDownPrivateModulus =
    "be09c4a297e2ce3e030619592af40bb6dcc7100caab8e75f1cbd113c3002682d4f8b618640d88327c6c032b61d4c6f57b189a3fd9a36cc1d5dd2b2d0f475432623b04a9e72a19726129fb0ed7979168ea01b4eb482f9cd4dacf0fa4efbdec3d7f7dd076906b550c7d75dacc8b2668e1bae359ef0a70200307204bd9f9d6b1343";

constexpr const char* kCardInfoUpPublicExponent = "010001";
constexpr const char* kCardInfoUpPublicModulus =
    "aa7f77a35e796192fc0242855e53f095fdff65f4ccc8a83e8cc6bc76fd1da71d34fc6c57f739b25533ffcafc3d2213de734e0c654ea3a70b8db078ec09ee0ab0873c5b4a4b7f297e964dee48459570e8d09791c01e9714165297a48141baf9d9a521fab318480806dd99c42c1fac741d509d2975b3b4c3842bfa7b9e15fd06af";
constexpr const char* kCardInfoDownPrivateExponent =
    "1e4b5fe1e17367a1c62a74bc890a51b1228071cfd21221058d29f2a6ae4fed8a47daf7ac451545491391252ce139542f65cfe5e175d73f319847788bacbac40179eb253722c4230e9a32c6f4df576aa4ab92bdaa99b28727d09f0b7d2e35b025ddf79c0be7e5f31f46310a50a197c0cc20a74ee9106cd02c0b159bc331c91b21";
constexpr const char* kCardInfoDownPrivateModulus =
    "93de94772be43daa0903658fd669d2b28354bb7c310e92c1f4e740c69d394707f8f81f104fba1c0b354f2500ed7505ede097fdda320da4869e2f8ece32f1bf6290c28e7c6f0564919f5a8a987948e47f3f5d60168450e3124968e257a1ff32e5cb7d02eae40621b1683e1b26fd898e8cff0740b30073a08792e3a8232e48cf85";

constexpr const char* kExpiredUpPublicExponent = "010001";
constexpr const char* kExpiredUpPublicModulus =
    "9d5571944662d9ba56a7e9105eb24fd9a4ef8dbd7011024dbae8915552eff7bdb46f9cbced58df23b5bdf08127e411c358d773c27ab61c267fef2dc69f0c95ed736a6279d55651a9823cfdb546429fd1b45d6a96830439c776df79bc332a4652ea95f8008400655625bb7bd3ad46c7889b6cd7eeda069d466ded504b6893f5f1";
constexpr const char* kExpiredDownPrivateExponent =
    "10faf9b85439150ec4d129d767058a9c395aba7f5aa1907a93f58a6d270a8eec5a21572a7a2b3adfab5e58266b850d4076869853f8259fd453ade0c2ae3eeb0bbe5e658ff08075e51812074156988748573b7b8a184f9b10aeef03d69043eddc2121d5fbaee5da6c629909304aa91a93d4f7cd9457b9388911c502cf300a8e61";
constexpr const char* kExpiredDownPrivateModulus =
    "8257088df0cf8661cd816dd121ab7ec37df5ae6319837b3ca3d57eabe8f6c3949202cc1f767c85ef095af4b069e2e10ad8eedffe933511fd1b59e76e21450f9df9fb887ba608b315608e891631bfcad2213d4306d2646d1c1faffdb4d4b2f2e2c4bdf0655b47c70655b61d173e61d936907e45547531992c9819c8ff2faeda7b";

constexpr const char* kMacChangeBindUpPublicExponent = "010001";
constexpr const char* kMacChangeBindUpPublicModulus =
    "8c4f4e571ed3d7f81e2f61bd3516f03c0cd2d24482bb756ebc75d0385c02bd96a9e529e9e60d4e4a35f4d02bd092418f5c1ac3b7f6f3790ba6dd1effd655cb8f66fa83c39ab17a22dec533354d8f171b6bb3027bdaf9c38caaa23c0082953b082c42bdda4c00d4fadd74285866a28887f1b52502f78cb035e605008bd0e9c759";
constexpr const char* kMacChangeBindDownPrivateExponent =
    "065a9580758eeb7b8a24bf01f2c11f9408baded6572987ca01ac10cfcd9e0dc2c9032c588e8b0bc8e9e10d16cf2cda872386f1ba14850c1f1019a67420f18b6bec422d1d026c18c7d292fee57b9092fbd6ee4900dea5d74df1fbe7f1dc6c2adf12af85838b417f321e32ad67fcd01d130a1288f6040d830f95466624b4b04901";
constexpr const char* kMacChangeBindDownPrivateModulus =
    "a7f9c9fedec97aa03b82f56f66a7bd9d83c2686cd805acb7f78a1e58dab4a76141284fb1d0a9995915c9296555292c4d0b0ee590e8e19a308efeebd70b38f026c655468c53edc848678cd5e390d05700caffb34837f33514914847183999bf9f6ca234d9ee37f4e79d4cbec66a9a58411bfc7df9708577d55f6f6ce2ce3dba81";

constexpr const char* kCheckUserStatusUpPublicExponent = "010001";
constexpr const char* kCheckUserStatusUpPublicModulus =
    "af978dab7c01b9603edc713dc6202996c9be79b93d6e7d057cae4686de24df83d40dc36d325ade185fe115c6110ba7521c4a3ccc602839122bfe9769253777fea4f289bbc03983075f3d93a49bac18bb72608b29144a1ac7cce8d7c38a0753c58a518015c22b6333e8fcc8005d0c38b4ef935d90dea494758b2fc34980d26bf7";
constexpr const char* kCheckUserStatusDownPrivateExponent =
    "3442201023c5acc55ddb5fd1ec81af9681e26e8ac3ee34c89f47dff2ecd2aa21167abe1bd2f79c8134c770ec86fb3daf0059b008f902325eea197259e90e331b840df9e30c0d666b75cbf3d586e48398c0ad55dbbba7754313775446b9ae220e985ce3ad112ad7ca03293fa1ebab958f0d569865b342f698483b9dffe2a11689";
constexpr const char* kCheckUserStatusDownPrivateModulus =
    "9b4fc0c7896bee5dba6f4b24d875db89ac4c0b8dad9ef1a1b3067692ec58c06e573db728af6e760a440c88d355221f0eb11b2cffc334640f95c1c4b4548a14e3496a2edff7d0ba5f86d9a65012498ef3e32fd082e0fb5f72d43165544d744dd2fc91eba0336970a1ca49af6ae60fc1df7a50c476c22356edee6ed8f19227e60f";

constexpr const char* kLogoutUpPublicExponent = "010001";
constexpr const char* kLogoutUpPublicModulus =
    "ca54cb3557812a16342f4a29d388ea83efeeb356e08da4632847dded930db7ad019f617cc58f9482e26e017ca9fda3920271abf963703c31167f8561604041dd36c087f0d80518150fedb58b5399e38db7e9189617eb0cd751e7b149d90432b6ee9e3c73a9a3be6f716dc0bdda93508b5dee6a83626e291cb775c942e2516911";
constexpr const char* kLogoutDownPrivateExponent =
    "acfc644707e6b82dd69fe0ab3a72b3929f6a3dc0024b43579a80fae4c85df13060e8c34bcf54aaaf59fdb8971a8d898458161e0fed826fb17d5c899db36f13050f3abddbaaba823e2e3317545bce656b9d84c77f7c59c732e141e68228f579b402281381ad07d6b9d4d32cc3a8f3007e47ae9b09cdc9e6fb3f48b59adaf901";
constexpr const char* kLogoutDownPrivateModulus =
    "a2d0103f74dfc3d71cfdd9b0198594b9b78493eb8792236c1c17233299a8362b989022135cd345a96dd0117d02f54f1fe36432f6a17ab212c0fe24af48847e8d04ac2cefc01e9b20fb10768489bd4d204942421b1922ef9f9670981b4d1212c7a7448fbbd2fca269522b57a77cfad2fdf7f3ff9706e58384fa79d44e37a7e647";

constexpr const char* kOnlineCountUpPublicExponent = "010001";
constexpr const char* kOnlineCountUpPublicModulus =
    "926d2b0b3fb3c77235a38015d994a466fd6be20caef749ba117d96b0d5023ef338fb09969f005fd11d3d82146855f5821d24f80e5892304e359073bd250c85752cd14b50591380308ae02e89141b0b876a5acced74ca7e58c12ce8ebca66b2731e3224b26898e9252b6856c98dad1128c1196e3248e36add3d057477acae31d7";
constexpr const char* kOnlineCountDownPrivateExponent =
    "97eba74b66502e8be60c0840a96b91013bfd1af82801d7af73aef331f83ee3e8c9c323c17a8dd5f5596df5b16538e0145e6147f80da7fdb0d32303494faf9649802f0f66f1397b0568cacd0d97099ca37ce4f9b3bb2fd38862ea998644d0c29c138f6afcfa390a8784ef30cb9414abec379d3f9a5e93f9d3571c2d1396f1acd";
constexpr const char* kOnlineCountDownPrivateModulus =
    "9140c4863405656d2073c7efbb90b185fa1167b1037a71abbc91bb1d591d4d0c9d61ddbf44951cfd15a8b3fb9e6ab6da6ebd38adbb5eccf24b46f083b18db80b314aebe186716bc12d6d7f526ad91706126f04b3240ea83a2bf44a34d02b8094071e7a4452859024cb92a1f610fd9d80a64620816c75bed9bdf8f1adbba98879";

constexpr size_t kRsaBlockSize = 128;

BigInteger bigIntegerFromBigEndian (const uint8* data, size_t size)
{
    MemoryBlock littleEndian (size, true);
    auto* out = static_cast<uint8*> (littleEndian.getData());
    for (size_t i = 0; i < size; ++i)
        out[i] = data[size - 1 - i];

    BigInteger value;
    value.loadFromMemoryBlock (littleEndian);
    return value;
}

MemoryBlock bigEndianFromBigInteger (const BigInteger& value, size_t size)
{
    const auto littleEndian = value.toMemoryBlock();
    MemoryBlock out (size, true);
    auto* dst = static_cast<uint8*> (out.getData());
    const auto* src = static_cast<const uint8*> (littleEndian.getData());
    const size_t bytesToCopy = jmin (littleEndian.getSize(), size);

    for (size_t i = 0; i < bytesToCopy; ++i)
        dst[size - 1 - i] = src[i];

    return out;
}

String rsaPkcs1EncryptBase64 (const MemoryBlock& payload, const String& exponentHex, const String& modulusHex)
{
    if (payload.getSize() > kRsaBlockSize - 11)
        return {};

    MemoryBlock block (kRsaBlockSize, true);
    auto* bytes = static_cast<uint8*> (block.getData());
    bytes[0] = 0x00;
    bytes[1] = 0x02;

    const size_t paddingLen = kRsaBlockSize - payload.getSize() - 3;
    for (size_t i = 0; i < paddingLen; ++i)
    {
        uint8 randomByte = 0;
        while (randomByte == 0)
            randomByte = static_cast<uint8> (Random::getSystemRandom().nextInt (Range<int> (1, 256)));
        bytes[2 + i] = randomByte;
    }

    bytes[2 + paddingLen] = 0x00;
    block.copyFrom (payload.getData(), 3 + paddingLen, payload.getSize());

    BigInteger value = bigIntegerFromBigEndian (bytes, block.getSize());
    RSAKey key (exponentHex + "," + modulusHex);
    if (! key.applyToValue (value))
        return {};

    const auto encrypted = bigEndianFromBigInteger (value, kRsaBlockSize);
    return Base64::toBase64 (encrypted.getData(), encrypted.getSize());
}

String rsaPkcs1EncryptBase64Blocks (const MemoryBlock& payload, const String& exponentHex, const String& modulusHex)
{
    MemoryBlock encryptedTotal;
    const auto* payloadBytes = static_cast<const uint8*> (payload.getData());
    constexpr size_t maxPlainBlock = kRsaBlockSize - 11;
    for (size_t offset = 0; offset < payload.getSize(); offset += maxPlainBlock)
    {
        const size_t chunkSize = jmin (maxPlainBlock, payload.getSize() - offset);
        MemoryBlock chunk (payloadBytes + offset, chunkSize);
        const String encryptedChunkBase64 = rsaPkcs1EncryptBase64 (chunk, exponentHex, modulusHex);
        MemoryOutputStream encryptedChunk;
        if (encryptedChunkBase64.isEmpty() || ! Base64::convertFromBase64 (encryptedChunk, encryptedChunkBase64))
            return {};
        encryptedTotal.append (encryptedChunk.getData(), encryptedChunk.getDataSize());
    }
    return Base64::toBase64 (encryptedTotal.getData(), encryptedTotal.getSize());
}

MemoryBlock rsaPkcs1DecryptBase64 (const String& encryptedText, const String& exponentHex, const String& modulusHex)
{
    MemoryOutputStream binary;
    if (! Base64::convertFromBase64 (binary, encryptedText.trim()))
        return {};

    const auto encrypted = binary.getMemoryBlock();
    if (encrypted.getSize() % kRsaBlockSize != 0)
        return {};

    MemoryBlock plain;
    const auto* encryptedBytes = static_cast<const uint8*> (encrypted.getData());
    for (size_t offset = 0; offset < encrypted.getSize(); offset += kRsaBlockSize)
    {
        BigInteger value = bigIntegerFromBigEndian (encryptedBytes + offset, kRsaBlockSize);
        RSAKey key (exponentHex + "," + modulusHex);
        if (! key.applyToValue (value))
            return {};

        const auto block = bigEndianFromBigInteger (value, kRsaBlockSize);
        const auto* bytes = static_cast<const uint8*> (block.getData());
        if (bytes[0] != 0x00 || bytes[1] != 0x02)
            return {};

        size_t dataOffset = 2;
        while (dataOffset < block.getSize() && bytes[dataOffset] != 0x00)
            ++dataOffset;
        if (dataOffset >= block.getSize())
            return {};

        ++dataOffset;
        plain.append (bytes + dataOffset, block.getSize() - dataOffset);
    }

    return plain;
}

String postForm (const String& endpoint, const String& body)
{
    int statusCode = 0;
    URL url (endpoint);
    url = url.withPOSTData (body);

    auto stream = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inPostData)
                                             .withHttpRequestCmd ("POST")
                                             .withExtraHeaders ("Content-Type: application/x-www-form-urlencoded\r\n")
                                             .withConnectionTimeoutMs (6000)
                                             .withStatusCode (&statusCode));

    if (stream == nullptr || statusCode < 200 || statusCode >= 300)
    {
        DBG ("[CookieLinkAuth] post_failed endpoint=" + endpoint + " http=" + String (statusCode));
        return {};
    }

    return stream->readEntireStreamAsString().trim();
}

String oneLine (String text)
{
    return text.replaceCharacter ('\n', ' ').replaceCharacter ('\r', ' ');
}

Array<File> getCookieLinkDebugDirs()
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

void appendAuthDebugLine (const String& line)
{
    DBG ("[CookieLinkAuth] " + line.trimEnd());
    for (const auto& dir : getCookieLinkDebugDirs())
    {
        dir.createDirectory();
        appendUtf8LogLine (dir.getChildFile ("eydata_debug.log"), line);
    }
}

void logEndpointResponse (const String& api, const String& endpoint, const String& response)
{
    appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                         + " api=" + api
                         + " endpoint=" + endpoint
                         + " raw_response=" + oneLine (response)
                         + newLine);
}

String postFormWithFallback (const String& body, const String& api, const char* const* endpoints, int endpointCount)
{
    String forcedAlgorithmError;
    for (int i = 0; i < endpointCount; ++i)
    {
        const auto* endpoint = endpoints[i];
        const String response = postForm (endpoint, body);
        logEndpointResponse (api, endpoint, response.isNotEmpty() ? response : String ("<empty>"));
        if (response.isNotEmpty())
        {
            if (response == "-23")
            {
                forcedAlgorithmError = response;
                continue;
            }
            return response;
        }
    }

    return forcedAlgorithmError;
}

String memoryBlockToHex (const MemoryBlock& block)
{
    String hex;
    const auto* bytes = static_cast<const uint8*> (block.getData());
    for (size_t i = 0; i < block.getSize(); ++i)
    {
        if (i > 0)
            hex << " ";
        hex << String::toHexString ((int) bytes[i]).paddedLeft ('0', 2);
    }
    return hex;
}

void logApiResponseText (const String& api, const String& label, const String& response)
{
    const String line = Time::getCurrentTime().toString (true, true, true, true)
                        + " api=" + api
                        + " " + label + "=" + oneLine (response)
                        + newLine;
    appendAuthDebugLine (line);
}

void logDecryptedApiResponse (const String& api, const MemoryBlock& decrypted)
{
    const String rawText = String::fromUTF8 (static_cast<const char*> (decrypted.getData()), (int) decrypted.getSize());
    const String line = Time::getCurrentTime().toString (true, true, true, true)
                        + " api=" + api
                        + " decrypted_raw_text=" + oneLine (rawText)
                        + " decrypted_trimmed_text=" + oneLine (rawText.trim())
                        + " decrypted_raw_hex=" + memoryBlockToHex (decrypted)
                        + " decrypted_raw_base64=" + Base64::toBase64 (decrypted.getData(), decrypted.getSize())
                        + newLine;
    appendAuthDebugLine (line);
}


String encryptGetVariable43011 (const String& input)
{
    static const int key[] = { 73,34,172,164,197,180,100,190,132,32,227,212,182,166,233,79,73,8,237,81,11,113,86,199 };
    constexpr int keyLen = (int) (sizeof (key) / sizeof (key[0]));
    String encoded;
    auto chars = input.getCharPointer();
    for (int i = 0; ! chars.isEmpty(); ++i, ++chars)
    {
        int code = (int) chars.getAndAdvance();
        code = (code - 78) ^ key[i % keyLen];
        if (code < 0)
        {
            code = -code;
            encoded << "-";
        }
        encoded << String::toHexString (code) << ",";
    }
    return Base64::toBase64 (encoded.toRawUTF8(), encoded.getNumBytesAsUTF8());
}

String decryptGetVariable43011 (const String& input)
{
    static const int key[] = { 20,253,61,134,72,73,128,219,74,29,210,46,74,100,3,50,227,145,100,4,169,177,49,251,133,90 };
    constexpr int keyLen = (int) (sizeof (key) / sizeof (key[0]));
    MemoryOutputStream decoded;
    if (! Base64::convertFromBase64 (decoded, input.trim()))
        return input.trim();

    StringArray parts;
    parts.addTokens (decoded.getMemoryBlock().toString(), ",", "");
    String result;
    for (int i = 0; i < parts.size(); ++i)
    {
        String token = parts[i].trim();
        if (token.isEmpty())
            continue;
        bool negative = token.startsWithChar ('-');
        if (negative)
            token = token.substring (1);
        int value = token.getHexValue32();
        if (negative)
            value = -value;
        const int unicodeChar = (value ^ key[i % keyLen]) + 10;
        result << juce_wchar (unicodeChar);
    }
    return result.trim();
}

String getVariable43011Post (const String& payload, const char* const* endpoints, int endpointCount)
{
    const String encrypted = encryptGetVariable43011 (payload);
    const String body = "p=" + URL::addEscapeChars (encrypted, true)
                      + "&api=" + URL::addEscapeChars (kServerListVariableApi, true);
    const String raw = postFormWithFallback (body, kServerListVariableApi, endpoints, endpointCount);
    if (raw.isEmpty() || raw.startsWithChar ('-'))
        return raw;

    const String decrypted = decryptGetVariable43011 (raw);
    appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                         + " api=" + String (kServerListVariableApi)
                         + " variable_decrypted_text=" + oneLine (decrypted)
                         + newLine);
    return decrypted;
}

String decryptGetVariable43011Response (const String& raw)
{
    if (raw.isEmpty() || raw.startsWithChar ('-'))
        return raw;
    const String decrypted = decryptGetVariable43011 (raw);
    appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                         + " api=" + String (kServerListVariableApi)
                         + " variable_decrypted_text=" + oneLine (decrypted)
                         + newLine);
    return decrypted;
}

String getVariable43011PostBody (const String& label, const String& body, const char* const* endpoints, int endpointCount)
{
    appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                         + " api=" + String (kServerListVariableApi)
                         + " get_variable_try=" + label
                         + " body_len=" + String (body.length())
                         + newLine);
    return decryptGetVariable43011Response (postFormWithFallback (body, kServerListVariableApi, endpoints, endpointCount));
}

bool isLoginRequiredResponse (const String& response)
{
    return response.upToFirstOccurrenceOf ("|", false, false).trim() == "-103";
}


String rsaPkcs1EncryptedPostNoXor (const String& payload,
                                   const String& api,
                                   const String& upExponentHex,
                                   const String& upModulusHex,
                                   const String& downExponentHex,
                                   const String& downModulusHex,
                                   const char* const* endpoints,
                                   int endpointCount)
{
    MemoryBlock plain (payload.toRawUTF8(), payload.getNumBytesAsUTF8());
    const String encrypted = rsaPkcs1EncryptBase64Blocks (plain, upExponentHex, upModulusHex);
    if (encrypted.isEmpty())
    {
        appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                             + " api=" + api
                             + " encrypt_failed_no_xor payload_len=" + String (payload.getNumBytesAsUTF8())
                             + newLine);
        return {};
    }

    const String body = "p=" + URL::addEscapeChars (encrypted, true)
                      + "&api=" + URL::addEscapeChars (api, true);
    const String response = postFormWithFallback (body, api, endpoints, endpointCount);
    auto decrypted = rsaPkcs1DecryptBase64 (response, downExponentHex, downModulusHex);
    if (decrypted.isEmpty())
    {
        logApiResponseText (api, "decrypt_failed_response", response);
        return response;
    }

    logDecryptedApiResponse (api, decrypted);
    return String::fromUTF8 (static_cast<const char*> (decrypted.getData()), (int) decrypted.getSize()).trim();
}

String xorPkcs1EncryptedPost (const String& payload,
                              const String& api,
                              const String& upExponentHex,
                              const String& upModulusHex,
                              const String& downExponentHex,
                              const String& downModulusHex,
                              const char* const* endpoints = kEyDataEndpoints,
                              int endpointCount = kEyDataEndpointCount,
                              bool includeApiParameter = true)
{
    MemoryBlock obfuscated (payload.toRawUTF8(), payload.getNumBytesAsUTF8());
    Array<uint8> keys;
    const int keyLen = Random::getSystemRandom().nextInt (Range<int> (3, 7));
    auto* obfuscatedBytes = static_cast<uint8*> (obfuscated.getData());

    for (int i = 0; i < keyLen; ++i)
    {
        const uint8 key = static_cast<uint8> (Random::getSystemRandom().nextInt (Range<int> (1, 256)));
        keys.add (key);
        for (size_t j = 0; j < obfuscated.getSize(); ++j)
            obfuscatedBytes[j] ^= key;
    }

    MemoryBlock framed;
    const uint8 keyLenByte = static_cast<uint8> (keyLen);
    framed.append (&keyLenByte, 1);
    for (auto key : keys)
        framed.append (&key, 1);
    framed.append (obfuscated.getData(), obfuscated.getSize());

    const String encrypted = rsaPkcs1EncryptBase64 (framed, upExponentHex, upModulusHex);
    if (encrypted.isEmpty())
    {
        appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                             + " api=" + api
                             + " encrypt_failed payload_len=" + String (payload.getNumBytesAsUTF8())
                             + " framed_len=" + String ((int) framed.getSize())
                             + newLine);
        return {};
    }

    String body = "p=" + URL::addEscapeChars (encrypted, true);
    if (includeApiParameter && api.isNotEmpty())
        body << "&api=" << URL::addEscapeChars (api, true);

    const String response = postFormWithFallback (body, api, endpoints, endpointCount);
    auto decrypted = rsaPkcs1DecryptBase64 (response, downExponentHex, downModulusHex);
    if (decrypted.isEmpty())
    {
        logApiResponseText (api, "decrypt_failed_response", response);
        return response;
    }

    auto* decryptedBytes = static_cast<uint8*> (decrypted.getData());
    for (int i = keys.size() - 1; i >= 0; --i)
        for (size_t j = 0; j < decrypted.getSize(); ++j)
            decryptedBytes[j] ^= keys.getReference (i);

    logDecryptedApiResponse (api, decrypted);
    const String decryptedText = String::fromUTF8 (static_cast<const char*> (decrypted.getData()), (int) decrypted.getSize()).trim();
    return decryptedText;
}

bool readDigitsAt (const String& text, int& pos, int minDigits, int maxDigits, String& value)
{
    const int begin = pos;
    while (pos < text.length()
           && pos - begin < maxDigits
           && CharacterFunctions::isDigit (text[pos]))
        ++pos;

    if (pos - begin < minDigits)
        return false;

    value = text.substring (begin, pos);
    return true;
}

String twoDigit (const String& value)
{
    return value.length() < 2 ? "0" + value : value;
}

String makeDateTimeDisplay (const String& yyyy, const String& mm, const String& dd,
                            const String& hh = {}, const String& mi = {}, const String& ss = {})
{
    String out = yyyy + "年" + twoDigit (mm) + "月" + twoDigit (dd) + "日";
    if (hh.isNotEmpty() && mi.isNotEmpty())
        out << " " << twoDigit (hh) << "时" << twoDigit (mi) << "分" << twoDigit (ss.isNotEmpty() ? ss : "00") << "秒";
    return out;
}

String readAsciiTimeAt (const String& text, int pos)
{
    while (pos < text.length() && (text[pos] == ' ' || text[pos] == 'T'))
        ++pos;

    String hh, mi, ss;
    if (! readDigitsAt (text, pos, 1, 2, hh) || pos >= text.length() || text[pos] != ':')
        return {};

    ++pos;
    if (! readDigitsAt (text, pos, 1, 2, mi))
        return {};

    if (pos < text.length() && text[pos] == ':')
    {
        ++pos;
        readDigitsAt (text, pos, 1, 2, ss);
    }

    return makeDateTimeDisplay ({}, {}, {}, hh, mi, ss).fromFirstOccurrenceOf (" ", false, false);
}

String readChineseTimeAt (const String& text, int pos)
{
    while (pos < text.length() && text[pos] == ' ')
        ++pos;

    String hh, mi, ss;
    if (! readDigitsAt (text, pos, 1, 2, hh))
        return {};

    if (pos < text.length() && text[pos] == ':')
        return readAsciiTimeAt (text, pos - hh.length());

    if (pos >= text.length() || text[pos] != L'时')
        return {};

    ++pos;
    if (! readDigitsAt (text, pos, 1, 2, mi))
        return {};

    if (pos < text.length() && text[pos] == L'分')
    {
        ++pos;
        const int beforeSecond = pos;
        if (readDigitsAt (text, pos, 1, 2, ss))
        {
            if (pos < text.length() && text[pos] == L'秒')
                ++pos;
        }
        else
        {
            pos = beforeSecond;
        }
    }

    return makeDateTimeDisplay ({}, {}, {}, hh, mi, ss).fromFirstOccurrenceOf (" ", false, false);
}

String extractDateTimeAt (const String& text, int start)
{
    int pos = start;
    String yyyy, mm, dd;
    if (! readDigitsAt (text, pos, 4, 4, yyyy) || pos >= text.length())
        return {};

    const auto dateSep = text[pos];
    if (dateSep == '-' || dateSep == '/')
    {
        ++pos;
        if (! readDigitsAt (text, pos, 1, 2, mm) || pos >= text.length() || (text[pos] != '-' && text[pos] != '/'))
            return {};
        ++pos;
        if (! readDigitsAt (text, pos, 1, 2, dd))
            return {};

        const String time = readAsciiTimeAt (text, pos);
        if (time.isNotEmpty())
            return makeDateTimeDisplay (yyyy, mm, dd) + " " + time;
        return makeDateTimeDisplay (yyyy, mm, dd);
    }

    if (dateSep == L'年')
    {
        ++pos;
        if (! readDigitsAt (text, pos, 1, 2, mm) || pos >= text.length() || text[pos] != L'月')
            return {};
        ++pos;
        if (! readDigitsAt (text, pos, 1, 2, dd) || pos >= text.length() || text[pos] != L'日')
            return {};
        ++pos;

        const String time = readChineseTimeAt (text, pos);
        if (time.isNotEmpty())
            return makeDateTimeDisplay (yyyy, mm, dd) + " " + time;
        return makeDateTimeDisplay (yyyy, mm, dd);
    }

    return {};
}

String findFirstDateTimeAfter (const String& text, int start, int maxChars)
{
    const int end = jmin (text.length(), start + maxChars);
    for (int i = jmax (0, start); i < end; ++i)
    {
        const String value = extractDateTimeAt (text, i);
        if (value.isNotEmpty())
            return value;
    }

    return {};
}

String formatDateForDisplay (const String& raw)
{
    const String text = raw.trim();
    const String date = extractDateTimeAt (text, 0);
    return date.isNotEmpty() ? date : text;
}

String findExpiryDateText (const String& text)
{
    if (text.isEmpty())
        return {};

    auto parsed = JSON::parse (text);
    if (parsed.isObject())
    {
        static const char* keys[] = { "Expired", "Expire", "ExpireTime", "EndTime", "EndDate", "expiresAt", "expire_time", "endtime", "end_time", "endDate", "expire" };
        for (const auto* key : keys)
        {
            const String value = parsed.getProperty (key, {}).toString().trim();
            if (value.isNotEmpty())
            {
                const String date = findFirstDateTimeAfter (value, 0, value.length());
                return formatDateForDisplay (date.isNotEmpty() ? date : value);
            }
        }
    }

    const String lower = text.toLowerCase();
    static const char* labels[] = { "到期", "过期", "截止", "结束", "expire", "expired", "endtime", "end time", "enddate", "end date", "expires" };
    for (const auto* label : labels)
    {
        const int index = lower.indexOf (label);
        if (index >= 0)
        {
            const String date = findFirstDateTimeAfter (text, index, 100);
            if (date.isNotEmpty())
                return formatDateForDisplay (date);
        }
    }

    String lastDate;
    for (int i = 0; i < text.length(); ++i)
    {
        const String date = extractDateTimeAt (text, i);
        if (date.isNotEmpty())
        {
            lastDate = date;
            i += date.length() - 1;
        }
    }

    if (lastDate.isNotEmpty())
        return formatDateForDisplay (lastDate);

    return {};
}

bool looksLikeStatusCode (const String& text)
{
    return text.length() == 32 && text.containsOnly ("0123456789abcdefABCDEF");
}

String statusCodeFromLoginResponse (const String& response)
{
    const String statusCode = response.upToFirstOccurrenceOf ("|", false, false).trim();
    return looksLikeStatusCode (statusCode) ? statusCode : String();
}

String errorTextForCode (const String& response)
{
    const String code = response.upToFirstOccurrenceOf ("|", false, false).trim();
    if (code.isEmpty())
        return "无法连接授权服务器，请稍后重试。";

    if (! code.startsWithChar ('-'))
        return "激活失败，请稍后重试。";

    if (code == "-3")
        return "接口通信失败，请检查网络或加密配置。";
    if (code == "-20")
        return "获取变量失败（-20），服务端认为参数不完整或变量定位失败，请检查服务器列表变量编号和别名。";
    if (code == "-23")
        return "接口通信失败，请检查接口线路或加密配置。";
    if (code == "-35")
        return "获取变量失败（-35），请检查后台服务器列表变量是否已添加并允许当前账号读取。";
    if (code == "-102")
        return "授权账号不存在。";
    if (code == "-123")
        return "解绑失败，授权状态不允许当前操作。";
    if (code == "-204")
        return "程序版本不存在，请联系管理员更新授权配置。";
    if (code == "-301")
        return "授权码不存在。";
    if (code == "-401")
        return "授权码无效、已过期或已绑定其他设备。";
    if (code == "-112")
        return "用户在别的地方登陆。";

    return "授权服务返回错误代码：" + code;
}

}

void EyDataAuthClient::touchDebugLog()
{
    appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                         + " eydata_debug_log_ready"
                         + newLine);
}

EyDataAuthClient::AuthResult EyDataAuthClient::activateSingleCode (const String& singleCode)
{
    AuthResult result;
    const String code = singleCode.trim();
    if (code.isEmpty())
    {
        result.message = "请先填写单码授权。";
        return result;
    }

    const String payload = "SingleCode=" + code
                           + "&Ver=" + String (kAppVersion)
                           + "&Mac=" + getMachineCode();

    const String response = xorPkcs1EncryptedPost (payload,
                                                   kSingleLoginApi,
                                                   kSingleLoginUpPublicExponent,
                                                   kSingleLoginUpPublicModulus,
                                                   kSingleLoginDownPrivateExponent,
                                                   kSingleLoginDownPrivateModulus);
    result.rawCode = response.upToFirstOccurrenceOf ("|", false, false).trim();
    const String statusCode = statusCodeFromLoginResponse (response);
    if (statusCode.isNotEmpty())
    {
        result.ok = true;
        result.statusCode = statusCode;
        const auto expired = fetchExpired (code);
        const String firstDate = findFirstDateTimeAfter (expired.text, 0, expired.text.length());
        result.expiresAt = firstDate.isNotEmpty() ? formatDateForDisplay (firstDate)
                                                  : findExpiryDateText (expired.text);
        result.message = "激活成功";
        if (result.expiresAt.isNotEmpty())
            result.message << "\n到期时间：" << result.expiresAt;
        else
            result.message << "\n到期时间：未获取";
    }
    else
    {
        result.message = "激活失败";
        const String errorText = errorTextForCode (response);
        if (errorText.isNotEmpty())
            result.message << "\n" << errorText;
    }
    return result;
}

EyDataAuthClient::AuthResult EyDataAuthClient::unbindSingleCode (const String& singleCode)
{
    AuthResult result;
    const String code = singleCode.trim();
    if (code.isEmpty())
    {
        result.message = "本机没有已保存的单码授权。";
        return result;
    }

    const String payload = "UserName=" + code
                           + "&UserPwd="
                           + "&Mac="
                           + "&Type=0";
    const String response = xorPkcs1EncryptedPost (payload,
                                                   kMacChangeBindApi,
                                                   kMacChangeBindUpPublicExponent,
                                                   kMacChangeBindUpPublicModulus,
                                                   kMacChangeBindDownPrivateExponent,
                                                   kMacChangeBindDownPrivateModulus,
                                                   kMacChangeBindEndpoints,
                                                   kMacChangeBindEndpointCount);
    result.rawCode = response.upToFirstOccurrenceOf ("|", false, false).trim();
    const int ret = response.getIntValue();
    if (response.isNotEmpty() && ret >= 0)
    {
        result.ok = true;
        result.message = "解绑成功。";
    }
    else
    {
        result.message = errorTextForCode (response);
    }
    return result;
}

EyDataAuthClient::AuthResult EyDataAuthClient::checkSingleCodeStatus (const String& singleCode, const String& statusCode)
{
    AuthResult result;
    const String code = singleCode.trim();
    const String status = statusCode.trim();
    if (code.isEmpty() || status.isEmpty())
    {
        result.message = "授权状态未就绪。";
        return result;
    }

    const String payload = "UserName=" + code
                           + "&StatusCode=" + status;
    const String response = xorPkcs1EncryptedPost (payload,
                                                   kCheckUserStatusApi,
                                                   kCheckUserStatusUpPublicExponent,
                                                   kCheckUserStatusUpPublicModulus,
                                                   kCheckUserStatusDownPrivateExponent,
                                                   kCheckUserStatusDownPrivateModulus,
                                                   kCheckUserStatusEndpoints,
                                                   kCheckUserStatusEndpointCount);
    result.rawCode = response.upToFirstOccurrenceOf ("|", false, false).trim();
    const int ret = response.getIntValue();
    if (response.isNotEmpty() && ret > 0)
    {
        result.ok = true;
        result.statusCode = status;
        result.message = "授权有效";
    }
    else
    {
        result.message = errorTextForCode (response);
    }
    return result;
}

EyDataAuthClient::TextResult EyDataAuthClient::logoutSingleCode (const String& singleCode, const String& statusCode)
{
    TextResult result;
    const String code = singleCode.trim();
    const String status = statusCode.trim();
    if (code.isEmpty() || status.isEmpty())
    {
        result.message = "授权状态未就绪。";
        return result;
    }

    const String payload = "UserName=" + code
                           + "&StatusCode=" + status;
    const String response = xorPkcs1EncryptedPost (payload,
                                                   kLogoutApi,
                                                   kLogoutUpPublicExponent,
                                                   kLogoutUpPublicModulus,
                                                   kLogoutDownPrivateExponent,
                                                   kLogoutDownPrivateModulus,
                                                   kLogoutEndpoints,
                                                   kLogoutEndpointCount);
    const int ret = response.getIntValue();
    if (response.isNotEmpty() && ret > 0)
    {
        result.ok = true;
        result.text = response;
    }
    else
    {
        result.message = errorTextForCode (response);
    }
    return result;
}

EyDataAuthClient::TextResult EyDataAuthClient::fetchOnlineCount (const String& singleCode)
{
    TextResult result;
    const String code = singleCode.trim();
    if (code.isEmpty())
    {
        result.message = "授权状态未就绪。";
        return result;
    }

    const String response = xorPkcs1EncryptedPost ("UserName=" + code + "&Type=3",
                                                   kOnlineCountApi,
                                                   kOnlineCountUpPublicExponent,
                                                   kOnlineCountUpPublicModulus,
                                                   kOnlineCountDownPrivateExponent,
                                                   kOnlineCountDownPrivateModulus,
                                                   kOnlineCountEndpoints,
                                                   kOnlineCountEndpointCount);
    if (response.isEmpty() || response.startsWithChar ('-'))
    {
        result.message = errorTextForCode (response);
        return result;
    }

    result.ok = true;
    result.text = response;
    return result;
}

EyDataAuthClient::TextResult EyDataAuthClient::fetchExpired (const String& singleCode)
{
    TextResult result;
    const String code = singleCode.trim();
    if (code.isEmpty())
    {
        result.message = "请先填写单码授权。";
        return result;
    }

    const String response = xorPkcs1EncryptedPost ("UserName=" + code,
                                                   kExpiredApi,
                                                   kExpiredUpPublicExponent,
                                                   kExpiredUpPublicModulus,
                                                   kExpiredDownPrivateExponent,
                                                   kExpiredDownPrivateModulus,
                                                   kExpiredEndpoints,
                                                   kExpiredEndpointCount);
    if (response.isEmpty() || response.startsWithChar ('-'))
    {
        result.message = errorTextForCode (response);
        return result;
    }

    result.ok = true;
    result.text = response;
    return result;
}

EyDataAuthClient::TextResult EyDataAuthClient::fetchCardInfo (const String& singleCode)
{
    TextResult result;
    const String code = singleCode.trim();
    if (code.isEmpty())
    {
        result.message = "请先填写单码授权。";
        return result;
    }

    const String response = xorPkcs1EncryptedPost ("card=" + code,
                                                   kCardInfoApi,
                                                   kCardInfoUpPublicExponent,
                                                   kCardInfoUpPublicModulus,
                                                   kCardInfoDownPrivateExponent,
                                                   kCardInfoDownPrivateModulus,
                                                   kCardInfoEndpoints,
                                                   kCardInfoEndpointCount);
    if (response.isEmpty() || response.startsWithChar ('-'))
    {
        result.message = errorTextForCode (response);
        return result;
    }

    result.ok = true;
    result.text = response;
    return result;
}

EyDataAuthClient::TextResult EyDataAuthClient::fetchBulletin()
{
    TextResult result;
    const String response = xorPkcs1EncryptedPost ("",
                                                   kBulletinApi,
                                                   kBulletinUpPublicExponent,
                                                   kBulletinUpPublicModulus,
                                                   kBulletinDownPrivateExponent,
                                                   kBulletinDownPrivateModulus,
                                                   kBulletinEndpoints,
                                                   kBulletinEndpointCount);
    if (response.isEmpty() || response.startsWithChar ('-'))
    {
        result.message = errorTextForCode (response);
        return result;
    }

    result.ok = true;
    result.text = response;
    return result;
}

EyDataAuthClient::TextResult EyDataAuthClient::fetchServerListVariable (const String& statusCode, const String& userName, const String& variableId, const String& variableName)
{
    TextResult result;
    const String status = statusCode.trim();
    const String user = userName.trim();
    const String varId = variableId.trim();
    const String varName = variableName.trim();
    appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                         + " api=" + String (kServerListVariableApi)
                         + " fetch_server_list_variable_begin status_len=" + String (status.length())
                         + " user_len=" + String (user.length())
                         + " variable_id=" + varId
                         + " variable=" + varName
                         + newLine);

    const String payload = "StatusCode=" + URL::addEscapeChars (status, true)
                         + "&UserName=" + URL::addEscapeChars (user, true)
                         + "&VariableId=" + URL::addEscapeChars (varId, true)
                         + "&VariableName=" + URL::addEscapeChars (varName, true);
    appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                         + " api=" + String (kServerListVariableApi)
                         + " get_variable_payload=" + oneLine (payload)
                         + newLine);
    String response = rsaPkcs1EncryptedPostNoXor (payload,
                                                   kServerListVariableApi,
                                                   kServerListVariableUpPublicExponent,
                                                   kServerListVariableUpPublicModulus,
                                                   kServerListVariableDownPrivateExponent,
                                                   kServerListVariableDownPrivateModulus,
                                                   kServerListVariableEndpoints,
                                                   kServerListVariableEndpointCount);
    if (isLoginRequiredResponse (response))
    {
        appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                             + " api=" + String (kServerListVariableApi)
                             + " get_variable_retry_after_login_required variable_id=" + varId
                             + newLine);
        Thread::sleep (1200);
        response = rsaPkcs1EncryptedPostNoXor (payload,
                                               kServerListVariableApi,
                                               kServerListVariableUpPublicExponent,
                                               kServerListVariableUpPublicModulus,
                                               kServerListVariableDownPrivateExponent,
                                               kServerListVariableDownPrivateModulus,
                                               kServerListVariableEndpoints,
                                               kServerListVariableEndpointCount);
    }
    appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                         + " api=" + String (kServerListVariableApi)
                         + " fetch_server_list_variable_response_len=" + String (response.length())
                         + " variable_id=" + varId
                         + " response=" + oneLine (response).substring (0, 1000)
                         + newLine);

    if (response.isEmpty() || response.startsWithChar ('-'))
    {
        result.message = errorTextForCode (response);
        appendAuthDebugLine (Time::getCurrentTime().toString (true, true, true, true)
                             + " api=" + String (kServerListVariableApi)
                             + " fetch_server_list_variable_error=" + oneLine (result.message)
                             + " variable_id=" + varId
                             + newLine);
        return result;
    }

    result.ok = true;
    result.text = response;
    return result;
}

EyDataAuthClient::TextResult EyDataAuthClient::fetchServerList (const String& statusCode, const String& userName)
{
    TextResult merged;
    StringArray values;
    StringArray errors;
    for (int i = 0; i < kServerListVariableCount; ++i)
    {
        auto result = fetchServerListVariable (statusCode, userName, kServerListVariableIds[i], kServerListVariableNames[i]);
        if (result.ok && result.text.trim().isNotEmpty())
            values.add (result.text.trim());
        else if (result.message.isNotEmpty())
            errors.add (String (kServerListVariableIds[i]) + ":" + result.message);
    }

    if (! values.isEmpty())
    {
        merged.ok = true;
        merged.text = values.joinIntoString (";");
        return merged;
    }

    merged.message = errors.isEmpty() ? "服务器列表变量为空。" : errors.joinIntoString ("; ");
    return merged;
}

String EyDataAuthClient::getMachineCode()
{
    String machine = SystemStats::getUniqueDeviceID().trim();
    if (machine.isEmpty())
        machine = SystemStats::getComputerName().trim();
    if (machine.isEmpty())
        machine = String (kAppSecret).substring (0, 16);

    return machine.removeCharacters (" \t\r\n-").substring (0, 64);
}
