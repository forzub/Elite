#include "src/ui/platform/ClientPreferencesStore.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <system_error>
#include <chrono>
#include <utility>

#include <nlohmann/json.hpp>

#include "src/game/identity/AccountHandle.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ui::platform
{
namespace
{
namespace fs = std::filesystem;

constexpr std::uintmax_t MaxPreferencesFileBytes = 256u * 1024u;
constexpr std::size_t MaxRememberedServers = 64u;
constexpr std::size_t MaxEndpointLength = 512u;
constexpr std::size_t MaxLocaleLength = 32u;

bool fail(std::string* outError, const std::string& message)
{
    if (outError)
        *outError = message;
    return false;
}

bool validEndpointKey(const std::string& value)
{
    return !value.empty() && value.size() <= MaxEndpointLength &&
           value.find('\0') == std::string::npos;
}

bool validLocale(const std::string& value)
{
    if (value.size() > MaxLocaleLength)
        return false;

    for (const unsigned char c : value)
    {
        const bool valid =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_';
        if (!valid)
            return false;
    }
    return true;
}

fs::path platformPreferencesRoot()
{
#ifdef _WIN32
    wchar_t localAppData[32768] = {};
    constexpr DWORD Capacity =
        static_cast<DWORD>(sizeof(localAppData) / sizeof(localAppData[0]));
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        localAppData,
        Capacity
    );
    if (length > 0 && length < Capacity)
        return fs::path(localAppData) / "EliteGame" / "client";

    std::error_code ec;
    const fs::path temp = fs::temp_directory_path(ec);
    return (ec ? fs::current_path() : temp) / "EliteGame" / "client";
#else
    if (const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME"))
        return fs::path(xdgConfigHome) / "EliteGame" / "client";
    if (const char* home = std::getenv("HOME"))
        return fs::path(home) / ".config" / "EliteGame" / "client";
    return fs::current_path() / ".elitegame" / "client";
#endif
}

bool replaceFileAtomically(
    const fs::path& temporaryPath,
    const fs::path& destinationPath,
    std::string* outError)
{
#ifdef _WIN32
    if (!MoveFileExW(
            temporaryPath.c_str(),
            destinationPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        return fail(
            outError,
            "cannot atomically replace client preferences: " +
            std::to_string(GetLastError())
        );
    }
    return true;
#else
    std::error_code ec;
    fs::rename(temporaryPath, destinationPath, ec);
    if (ec)
        return fail(outError, "cannot atomically replace client preferences: " + ec.message());
    return true;
#endif
}

nlohmann::json toJson(const ClientPreferences& preferences)
{
    nlohmann::json root = nlohmann::json::object();
    root["schema_version"] = ClientPreferences::SchemaVersion;
    root["last_server_endpoint"] = preferences.lastServerEndpoint;
    root["preferred_locale"] = preferences.preferredLocale;
    root["last_successful_account_by_server"] = nlohmann::json::object();

    for (const auto& [endpoint, account] : preferences.lastSuccessfulAccountByServer)
    {
        root["last_successful_account_by_server"][endpoint] = account;
    }
    return root;
}

bool fromJson(
    const nlohmann::json& root,
    ClientPreferences& outPreferences,
    std::string* outError)
{
    if (!root.is_object())
        return fail(outError, "client preferences root must be an object");

    if (root.value("schema_version", 0) != ClientPreferences::SchemaVersion)
        return fail(outError, "unsupported client preferences schema version");

    ClientPreferences parsed;
    if (root.contains("last_server_endpoint"))
    {
        if (!root["last_server_endpoint"].is_string())
            return fail(outError, "last_server_endpoint must be a string");
        parsed.lastServerEndpoint = root["last_server_endpoint"].get<std::string>();
        if (!parsed.lastServerEndpoint.empty() &&
            !validEndpointKey(parsed.lastServerEndpoint))
        {
            return fail(outError, "last_server_endpoint is invalid");
        }
    }

    if (root.contains("preferred_locale"))
    {
        if (!root["preferred_locale"].is_string())
            return fail(outError, "preferred_locale must be a string");
        parsed.preferredLocale = root["preferred_locale"].get<std::string>();
        if (!validLocale(parsed.preferredLocale))
            return fail(outError, "preferred_locale is invalid");
    }

    if (root.contains("last_successful_account_by_server"))
    {
        const auto& accounts = root["last_successful_account_by_server"];
        if (!accounts.is_object())
            return fail(outError, "last_successful_account_by_server must be an object");
        if (accounts.size() > MaxRememberedServers)
            return fail(outError, "too many remembered multiplayer servers");

        for (auto it = accounts.begin(); it != accounts.end(); ++it)
        {
            if (!validEndpointKey(it.key()) || !it.value().is_string())
                return fail(outError, "invalid remembered multiplayer server entry");

            const std::string account = it.value().get<std::string>();
            if (!game::identity::isValidAccountHandle(account))
                return fail(outError, "invalid remembered account handle");
            parsed.lastSuccessfulAccountByServer.emplace(it.key(), account);
        }
    }

    outPreferences = std::move(parsed);
    if (outError)
        outError->clear();
    return true;
}
}

std::string ClientPreferences::lastSuccessfulAccountFor(
    const std::string& endpoint) const
{
    const auto it = lastSuccessfulAccountByServer.find(endpoint);
    return it == lastSuccessfulAccountByServer.end()
        ? std::string()
        : it->second;
}

void ClientPreferences::rememberSuccessfulMultiplayer(
    const std::string& endpoint,
    const std::string& accountHandle)
{
    if (!validEndpointKey(endpoint) ||
        !game::identity::isValidAccountHandle(accountHandle))
    {
        return;
    }

    lastServerEndpoint = endpoint;
    lastSuccessfulAccountByServer[endpoint] = accountHandle;

    while (lastSuccessfulAccountByServer.size() > MaxRememberedServers)
        lastSuccessfulAccountByServer.erase(lastSuccessfulAccountByServer.begin());
}

fs::path ClientPreferencesStore::defaultPath()
{
    return platformPreferencesRoot() / "preferences.json";
}

bool ClientPreferencesStore::load(
    ClientPreferences& outPreferences,
    std::string* outError)
{
    return loadFromPath(defaultPath(), outPreferences, outError);
}

bool ClientPreferencesStore::save(
    const ClientPreferences& preferences,
    std::string* outError)
{
    return saveToPath(defaultPath(), preferences, outError);
}

bool ClientPreferencesStore::loadFromPath(
    const fs::path& path,
    ClientPreferences& outPreferences,
    std::string* outError)
{
    if (outError)
        outError->clear();

    std::error_code ec;
    if (!fs::exists(path, ec))
    {
        if (ec)
            return fail(outError, "cannot inspect client preferences: " + ec.message());
        outPreferences = {};
        return true;
    }

    const std::uintmax_t size = fs::file_size(path, ec);
    if (ec)
        return fail(outError, "cannot inspect client preferences size: " + ec.message());
    if (size > MaxPreferencesFileBytes)
        return fail(outError, "client preferences file exceeds safety limit");

    std::ifstream input(path, std::ios::binary);
    if (!input)
        return fail(outError, "cannot open client preferences");

    try
    {
        const nlohmann::json root = nlohmann::json::parse(input);
        return fromJson(root, outPreferences, outError);
    }
    catch (const nlohmann::json::exception& ex)
    {
        return fail(outError, std::string("invalid client preferences JSON: ") + ex.what());
    }
}

bool ClientPreferencesStore::saveToPath(
    const fs::path& path,
    const ClientPreferences& preferences,
    std::string* outError)
{
    if (outError)
        outError->clear();

    // Validate exactly what would be persisted. This keeps malformed runtime
    // state or future accidental secret-shaped extensions from silently
    // becoming durable preferences.
    ClientPreferences validated;
    if (!fromJson(toJson(preferences), validated, outError))
        return false;

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec)
        return fail(outError, "cannot create client preferences directory: " + ec.message());

    fs::path temporaryPath = path;
#ifdef _WIN32
    temporaryPath += ".tmp." + std::to_string(GetCurrentProcessId());
#else
    temporaryPath += ".tmp." + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif

    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output)
            return fail(outError, "cannot create temporary client preferences");

        output << toJson(validated).dump(2) << '\n';
        output.flush();
        if (!output.good())
        {
            output.close();
            fs::remove(temporaryPath, ec);
            return fail(outError, "cannot write temporary client preferences");
        }
    }

    if (!replaceFileAtomically(temporaryPath, path, outError))
    {
        fs::remove(temporaryPath, ec);
        return false;
    }

    return true;
}
}
