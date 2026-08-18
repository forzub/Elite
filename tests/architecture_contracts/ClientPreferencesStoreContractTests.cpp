#include "src/ui/platform/ClientPreferencesStore.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
int fail(const std::string& message)
{
    std::cerr << "[FAIL] client preferences contract: " << message << '\n';
    return 1;
}
}

int main()
{
    namespace fs = std::filesystem;
    using ui::platform::ClientPreferences;
    using ui::platform::ClientPreferencesStore;

    const fs::path root = fs::temp_directory_path() / "elite_client_preferences_contract";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec)
        return fail("cannot create temporary test directory");

    const fs::path file = root / "preferences.json";

    ClientPreferences preferences;
    preferences.preferredLocale = "ru";
    preferences.rememberSuccessfulMultiplayer("127.0.0.1:27351", "pilot-a");
    preferences.rememberSuccessfulMultiplayer("example.test:28000", "pilot-b");

    std::string error;
    if (!ClientPreferencesStore::saveToPath(file, preferences, &error))
        return fail("save failed: " + error);

    ClientPreferences loaded;
    if (!ClientPreferencesStore::loadFromPath(file, loaded, &error))
        return fail("load failed: " + error);

    if (loaded.lastServerEndpoint != "example.test:28000")
        return fail("last successful server was not preserved");
    if (loaded.lastSuccessfulAccountFor("127.0.0.1:27351") != "pilot-a")
        return fail("per-server remembered account A was not preserved");
    if (loaded.lastSuccessfulAccountFor("example.test:28000") != "pilot-b")
        return fail("per-server remembered account B was not preserved");
    if (loaded.preferredLocale != "ru")
        return fail("preferred locale was not preserved");

    std::ifstream input(file, std::ios::binary);
    const std::string serialized(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    for (const char* forbidden : {"password", "auth_token", "credential_secret", "recovery_secret"})
    {
        if (serialized.find(forbidden) != std::string::npos)
            return fail(std::string("secret-shaped field leaked into preferences: ") + forbidden);
    }

    {
        std::ofstream corrupt(file, std::ios::binary | std::ios::trunc);
        corrupt << R"({"schema_version":999,"last_server_endpoint":"127.0.0.1:27351"})";
    }
    if (ClientPreferencesStore::loadFromPath(file, loaded, &error))
        return fail("unsupported schema version was accepted");

    fs::remove_all(root, ec);
    std::cout << "[PASS] non-secret client preferences survive restart with per-server account memory\n";
    return 0;
}
