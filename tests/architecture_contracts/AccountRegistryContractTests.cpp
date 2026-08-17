#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "src/game/identity/Sha256.h"
#include "src/game/server/AccountRegistry.h"

namespace
{
[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "[FAIL] account authentication registry: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message)
{
    if (!condition)
        fail(message);
}

game::identity::AuthToken token(std::uint8_t seed)
{
    game::identity::AuthToken value;
    for (std::size_t i = 0; i < value.bytes.size(); ++i)
        value.bytes[i] = static_cast<std::uint8_t>(seed + i * 3u);
    return value;
}

std::string hex(const game::identity::AuthTokenDigest& digest)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : digest.bytes)
        out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}
}

int main()
{
    const std::string abc = "abc";
    const auto abcDigest = game::identity::sha256Digest(
        reinterpret_cast<const std::uint8_t*>(abc.data()), abc.size());
    require(
        hex(abcDigest) ==
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad",
        "SHA-256 implementation failed the standard abc test vector");

    game::server::AccountRegistry accounts;

    const AccountId accountA {101u};
    const AccountId accountB {202u};
    const PlayerId playerA {1u};
    const PlayerId playerB {2u};
    const auto tokenA = token(11u);
    const auto tokenB = token(21u);
    const auto digestA = game::identity::authTokenDigest(tokenA);
    const auto digestB = game::identity::authTokenDigest(tokenB);

    require(accounts.bind("pilot-a", digestA, accountA, playerA),
        "first handle/token/account/player binding failed");
    require(!accounts.bind("pilot-b", digestB, accountB, playerA),
        "two accounts were allowed to own one PlayerId");
    require(!accounts.bind("pilot-a", digestB, accountB, playerB),
        "duplicate account handle was accepted");

    AccountId resolvedAccount {};
    PlayerId resolvedPlayer {};
    require(accounts.resolve("pilot-a", digestA, resolvedAccount, resolvedPlayer) ==
            game::server::AccountRegistry::ResolveResult::Bound &&
            resolvedAccount == accountA &&
            resolvedPlayer == playerA,
        "known token digest did not resolve server-owned account/player identity");

    resolvedAccount = {};
    resolvedPlayer = {};
    require(accounts.resolve("pilot-b", digestB, resolvedAccount, resolvedPlayer) ==
            game::server::AccountRegistry::ResolveResult::UnknownAccount &&
            !resolvedAccount && !resolvedPlayer,
        "unknown account handle was accepted");

    resolvedAccount = {};
    resolvedPlayer = {};
    require(accounts.resolve("pilot-a", digestB, resolvedAccount, resolvedPlayer) ==
            game::server::AccountRegistry::ResolveResult::InvalidCredential &&
            !resolvedAccount && !resolvedPlayer,
        "wrong credential was accepted for a known account handle");

    require(accounts.bind("pilot-b", digestB, accountB, playerB),
        "second independent handle/token/account/player binding failed");
    require(accounts.isPlayerBound(playerA) && accounts.isPlayerBound(playerB),
        "bound PlayerIds were not retained");
    require(accounts.findByAccountId(accountA) != nullptr,
        "server-owned AccountId lookup failed");
    require(accounts.findByHandle("pilot-a") != nullptr,
        "stable account-handle lookup failed");
    require(accounts.size() == 2u,
        "account registry size mismatch");

    require(accounts.findByAccountId(accountA)->credentialDigest == digestA,
        "AccountRegistry did not retain the hashed credential digest");
    require(accounts.findByAccountId(accountA)->accountHandle == "pilot-a",
        "AccountRegistry did not retain the stable account handle");

    accounts.reset();
    require(accounts.size() == 0u && accounts.findByHandle("pilot-a") == nullptr,
        "development auth reset did not clear account bindings");

    std::cout
        << "[PASS] account handle + opaque token digest -> server AccountId/PlayerId binding\n";
    return 0;
}
