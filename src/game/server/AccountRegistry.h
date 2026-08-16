#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "src/game/identity/AccountId.h"
#include "src/game/identity/AuthToken.h"
#include "src/game/identity/PlayerId.h"

namespace game::server
{
struct AccountBindingRecord
{
    AccountId accountId {};
    game::identity::AuthTokenDigest credentialDigest {};
    PlayerId playerId {};
};

struct AuthTokenDigestHash
{
    std::size_t operator()(
        const game::identity::AuthTokenDigest& digest
    ) const noexcept
    {
        // FNV-1a is only the in-memory unordered_map hash. Authentication
        // security comes from the full SHA-256 digest stored in the key.
        std::size_t hash = static_cast<std::size_t>(1469598103934665603ull);
        for (const auto byte : digest.bytes)
        {
            hash ^= static_cast<std::size_t>(byte);
            hash *= static_cast<std::size_t>(1099511628211ull);
        }
        return hash;
    }
};

/*
    Server-side authentication-token -> persistent player binding.

    Raw bearer tokens never live in this registry. The server hashes a token at
    the admission boundary and stores only SHA-256 digest + server-owned
    AccountId + PlayerId. The client never supplies AccountId, PlayerId,
    ShipInstanceId or EntityId.

    M8E.2 keeps the registry in the live universe runtime. M8E.3 will move
    persistence behind a durable account-store boundary without changing the
    authentication/admission seam.
*/
class AccountRegistry
{
public:
    enum class ResolveResult
    {
        Bound,
        UnknownCredential
    };

    bool bind(
        game::identity::AuthTokenDigest credentialDigest,
        AccountId accountId,
        PlayerId playerId)
    {
        if (!credentialDigest.valid() || !accountId || !playerId)
            return false;

        if (m_accounts.find(credentialDigest) != m_accounts.end())
            return false;

        if (findByAccountId(accountId) || isPlayerBound(playerId))
            return false;

        AccountBindingRecord record;
        record.accountId = accountId;
        record.credentialDigest = credentialDigest;
        record.playerId = playerId;
        m_accounts.emplace(credentialDigest, record);
        return true;
    }

    ResolveResult resolve(
        game::identity::AuthTokenDigest credentialDigest,
        AccountId& outAccountId,
        PlayerId& outPlayerId) const noexcept
    {
        outAccountId = {};
        outPlayerId = {};

        const auto it = m_accounts.find(credentialDigest);
        if (it == m_accounts.end())
            return ResolveResult::UnknownCredential;

        outAccountId = it->second.accountId;
        outPlayerId = it->second.playerId;
        return ResolveResult::Bound;
    }

    bool isPlayerBound(PlayerId playerId) const noexcept
    {
        if (!playerId)
            return false;

        for (const auto& [digest, record] : m_accounts)
        {
            (void)digest;
            if (record.playerId == playerId)
                return true;
        }
        return false;
    }

    const AccountBindingRecord* findByAccountId(
        AccountId accountId
    ) const noexcept
    {
        if (!accountId)
            return nullptr;

        for (const auto& [digest, record] : m_accounts)
        {
            (void)digest;
            if (record.accountId == accountId)
                return &record;
        }
        return nullptr;
    }

    const std::unordered_map<
        game::identity::AuthTokenDigest,
        AccountBindingRecord,
        AuthTokenDigestHash>& all() const noexcept
    {
        return m_accounts;
    }

    std::size_t size() const noexcept
    {
        return m_accounts.size();
    }

private:
    std::unordered_map<
        game::identity::AuthTokenDigest,
        AccountBindingRecord,
        AuthTokenDigestHash> m_accounts;
};
}
