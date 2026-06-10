// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception

#pragma once

#include "JuceHeader.h"

class EyDataAuthClient
{
public:
    struct AuthResult
    {
        bool ok = false;
        String rawCode;
        String statusCode;
        String expiresAt;
        String message;
    };

    struct TextResult
    {
        bool ok = false;
        String text;
        String message;
    };

    static AuthResult activateSingleCode (const String& singleCode);
    static AuthResult unbindSingleCode (const String& singleCode);
    static AuthResult checkSingleCodeStatus (const String& singleCode, const String& statusCode);
    static TextResult logoutSingleCode (const String& singleCode, const String& statusCode);
    static TextResult fetchOnlineCount (const String& singleCode);
    static TextResult fetchExpired (const String& singleCode);
    static TextResult fetchCardInfo (const String& singleCode);
    static TextResult fetchBulletin();
    static TextResult fetchServerList (const String& statusCode, const String& userName);
    static TextResult fetchServerListVariable (const String& statusCode, const String& userName, const String& variableId, const String& variableName);
    static void touchDebugLog();
    static String getMachineCode();
};
