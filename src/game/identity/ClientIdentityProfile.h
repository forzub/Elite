#pragma once

#include <string>

#include "src/game/identity/AccountHandle.h"
#include "src/game/identity/AuthToken.h"
#include "src/game/network/SessionMessage.h"

namespace game::identity
{
struct ClientIdentityProfile
{
    std::string profileName;
    AuthToken authToken {};

    bool valid() const noexcept
    {
        return isValidAccountHandle(profileName) && authToken.valid();
    }

    game::network::SessionHello sessionHello() const noexcept
    {
        game::network::SessionHello hello;
        hello.accountHandle = profileName;
        hello.authToken = authToken;
        return hello;
    }
};

/*
    Client-side credential slot.

    The client stores only one opaque authentication secret. It does NOT store
    AccountId, PlayerId, ShipInstanceId or EntityId. On Windows the secret is
    kept by Windows Credential Manager. The server hashes the presented token
    and owns every gameplay identity/ownership binding.

    profileName is the same stable account handle sent to the server and is
    also used as the local OS credential-slot selector. The server verifies
    handle + bearer token together; no gameplay identity is derived from the
    string and the client still never stores server-owned gameplay IDs.
*/
class ClientIdentityProfileStore
{
public:
    // Sign-in must never create a credential as a side effect. It only loads
    // a slot that already exists in the OS credential store.
    static bool loadExisting(
        const std::string& profileName,
        ClientIdentityProfile& outProfile,
        std::string* outError = nullptr
    );

    // Registration/developer bootstrap may explicitly create a new local
    // credential slot when one does not already exist.
    static bool loadOrCreate(
        const std::string& profileName,
        ClientIdentityProfile& outProfile,
        std::string* outError = nullptr
    );
};
}
