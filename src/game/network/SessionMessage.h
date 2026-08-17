#pragma once

#include <cstdint>
#include <string>

#include "src/scene/EntityID.h"
#include "src/game/identity/AccountHandle.h"
#include "src/game/identity/AuthToken.h"
#include "src/game/identity/PlayerId.h"
#include "src/game/ship/ShipRegistry.h"
#include "src/game/network/ProtocolMetadata.h"

namespace game::network
{

enum class AuthenticationIntent : std::uint8_t
{
    SignIn = 0,
    Register = 1
};

/*
    Client -> server authentication/admission claim.

    accountHandle is the stable, human-entered account identifier. authToken
    is the device-local opaque bearer secret. SignIn is never allowed to create
    an account implicitly. Register is the only first-contact path allowed to
    bind a new server-owned AccountId and PlayerId. The client still does not
    know or choose AccountId, PlayerId, ShipInstanceId or EntityId.

    The handle is intentionally restricted to a conservative ASCII grammar at
    this security boundary. Localized/display player names are a separate
    presentation concept and may use full Unicode later.

    Until TLS is introduced this token must be treated as a development/LAN
    credential: a bearer token sent over plaintext TCP can be captured.
*/
struct SessionHello
{
    std::string accountHandle;
    game::identity::AuthToken authToken {};
    AuthenticationIntent intent = AuthenticationIntent::SignIn;
};

enum class SessionRejectReason : std::uint8_t
{
    InvalidCredential = 1,
    UnknownAccount = 2,
    RegistrationUnavailable = 3,
    AlreadyActive = 4,
    SessionUnavailable = 5,
    BootstrapFailed = 6,
    InvalidAccountHandle = 7,
    AccountHandleTaken = 8
};

struct SessionReject
{
    SessionRejectReason reason = SessionRejectReason::SessionUnavailable;
    bool retryable = true;
};

inline const char* sessionRejectCode(SessionRejectReason reason) noexcept
{
    switch (reason)
    {
        case SessionRejectReason::InvalidCredential:
            return "INVALID_CREDENTIAL";
        case SessionRejectReason::UnknownAccount:
            return "UNKNOWN_ACCOUNT";
        case SessionRejectReason::RegistrationUnavailable:
            return "REGISTRATION_UNAVAILABLE";
        case SessionRejectReason::AlreadyActive:
            return "ALREADY_ACTIVE";
        case SessionRejectReason::BootstrapFailed:
            return "BOOTSTRAP_FAILED";
        case SessionRejectReason::InvalidAccountHandle:
            return "INVALID_ACCOUNT_HANDLE";
        case SessionRejectReason::AccountHandleTaken:
            return "ACCOUNT_HANDLE_TAKEN";
        case SessionRejectReason::SessionUnavailable:
        default:
            return "SESSION_UNAVAILABLE";
    }
}

struct ServerSessionId
{
    std::uint64_t value = 0;

    constexpr explicit operator bool() const noexcept
    {
        return value != 0;
    }

    friend constexpr bool operator==(
        ServerSessionId a,
        ServerSessionId b
    ) noexcept
    {
        return a.value == b.value;
    }

    friend constexpr bool operator!=(
        ServerSessionId a,
        ServerSessionId b
    ) noexcept
    {
        return !(a == b);
    }
};

/*
    One-time server-assigned session bootstrap data.

    The controlled entity is connection authority, not client input. Keep it
    outside recurring simulation snapshots so stable session metadata is not
    resent at replication cadence.
*/
struct SessionWelcome
{
    ServerSessionId sessionId {};
    PlayerId playerId {};
    ShipInstanceId controlledShipInstanceId = 0;
    EntityId controlledEntityId {0};

    // Authoritative fixed simulation cadence used by remote client prediction.
    // Local sessions can obtain this directly from their host, but a process-
    // remote client must receive it as session bootstrap metadata.
    double fixedStepSeconds = 0.0;

    // Static physical catalog is loaded independently at each endpoint. The
    // server sends only a compatibility fence, never the catalog payload.
    CatalogMetadata starAtlasCatalog;
};
}
