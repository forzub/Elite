#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace ui::platform
{
struct ClientPreferences
{
    static constexpr int SchemaVersion = 1;

    std::string lastServerEndpoint;
    std::map<std::string, std::string> lastSuccessfulAccountByServer;
    std::string preferredLocale;

    std::string lastSuccessfulAccountFor(
        const std::string& endpoint) const;

    void rememberSuccessfulMultiplayer(
        const std::string& endpoint,
        const std::string& accountHandle);
};

// Non-secret client UX preferences only. Authentication secrets belong in the
// OS credential store and must never be serialized by this class.
class ClientPreferencesStore
{
public:
    static std::filesystem::path defaultPath();

    static bool load(
        ClientPreferences& outPreferences,
        std::string* outError = nullptr);

    static bool save(
        const ClientPreferences& preferences,
        std::string* outError = nullptr);

    // Explicit-path variants exist for deterministic tests and tooling. Runtime
    // code should normally use load()/save() and the platform default path.
    static bool loadFromPath(
        const std::filesystem::path& path,
        ClientPreferences& outPreferences,
        std::string* outError = nullptr);

    static bool saveToPath(
        const std::filesystem::path& path,
        const ClientPreferences& preferences,
        std::string* outError = nullptr);
};
}
