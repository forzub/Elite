#include "src/game/identity/ClientIdentityProfile.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#else
#include <sys/stat.h>
#endif

#include "src/game/identity/SecureRandom.h"

namespace game::identity
{
namespace
{
namespace fs = std::filesystem;

std::string sanitizeProfileName(const std::string& requested)
{
    std::string out;
    out.reserve(requested.size());

    for (const unsigned char c : requested)
    {
        const bool accepted =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_';
        if (accepted)
            out.push_back(static_cast<char>(c));
    }

    if (out.empty())
        out = "default";

    if (out.size() > 64)
        out.resize(64);

    return out;
}

bool generateToken(AuthToken& outToken)
{
    outToken = {};
    if (!fillSecureRandom(outToken.bytes.data(), outToken.bytes.size()))
        return false;

    // An all-zero CSPRNG result is astronomically unlikely, but keep zero as
    // the explicit invalid/sentinel value throughout the protocol.
    return outToken.valid();
}

#ifdef _WIN32
std::wstring credentialTarget(const std::string& profileName)
{
    std::wstring target = L"EliteGame/Auth/";
    target.reserve(target.size() + profileName.size());
    for (const unsigned char c : profileName)
        target.push_back(static_cast<wchar_t>(c));
    return target;
}

bool readWindowsCredential(
    const std::string& profileName,
    AuthToken& outToken,
    std::string* outError)
{
    const std::wstring target = credentialTarget(profileName);
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(
            target.c_str(),
            CRED_TYPE_GENERIC,
            0,
            &credential))
    {
        const DWORD error = GetLastError();
        if (error == ERROR_NOT_FOUND)
            return false;

        if (outError)
            *outError = "Windows Credential Manager read failed: " +
                std::to_string(error);
        return false;
    }

    const bool validBlob =
        credential &&
        credential->CredentialBlobSize == outToken.bytes.size() &&
        credential->CredentialBlob != nullptr;

    if (validBlob)
    {
        std::copy_n(
            credential->CredentialBlob,
            outToken.bytes.size(),
            outToken.bytes.begin()
        );
    }

    CredFree(credential);

    if (!validBlob || !outToken.valid())
    {
        outToken = {};
        if (outError)
            *outError = "Windows Credential Manager contains an invalid EliteGame token";
        return false;
    }

    return true;
}

bool writeWindowsCredential(
    const std::string& profileName,
    const AuthToken& token,
    std::string* outError)
{
    const std::wstring target = credentialTarget(profileName);
    std::wstring userName = L"EliteGame";

    CREDENTIALW credential {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(target.c_str());
    credential.CredentialBlobSize =
        static_cast<DWORD>(token.bytes.size());
    credential.CredentialBlob = const_cast<LPBYTE>(token.bytes.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = userName.data();

    if (!CredWriteW(&credential, 0))
    {
        if (outError)
            *outError = "Windows Credential Manager write failed: " +
                std::to_string(GetLastError());
        return false;
    }

    return true;
}
#else
fs::path userDataRoot()
{
    if (const char* xdgDataHome = std::getenv("XDG_DATA_HOME"))
        return fs::path(xdgDataHome) / "EliteGame";

    if (const char* home = std::getenv("HOME"))
        return fs::path(home) / ".local" / "share" / "EliteGame";

    return fs::current_path() / ".elitegame";
}

fs::path fallbackCredentialPath(const std::string& profileName)
{
    return userDataRoot() / "client" / "credentials" / (profileName + ".token");
}

bool readFallbackCredential(
    const std::string& profileName,
    AuthToken& outToken,
    std::string* outError)
{
    const fs::path path = fallbackCredentialPath(profileName);
    if (!fs::exists(path))
        return false;

    std::ifstream in(path, std::ios::binary);
    in.read(
        reinterpret_cast<char*>(outToken.bytes.data()),
        static_cast<std::streamsize>(outToken.bytes.size())
    );

    if (in.gcount() != static_cast<std::streamsize>(outToken.bytes.size()) ||
        in.peek() != std::char_traits<char>::eof() ||
        !outToken.valid())
    {
        outToken = {};
        if (outError)
            *outError = "client authentication token file is invalid";
        return false;
    }

    return true;
}

bool writeFallbackCredential(
    const std::string& profileName,
    const AuthToken& token,
    std::string* outError)
{
    const fs::path path = fallbackCredentialPath(profileName);
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec)
    {
        if (outError)
            *outError = "cannot create credential directory: " + ec.message();
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(
        reinterpret_cast<const char*>(token.bytes.data()),
        static_cast<std::streamsize>(token.bytes.size())
    );
    out.close();
    if (!out.good())
    {
        if (outError)
            *outError = "cannot write client authentication token";
        return false;
    }

    (void)::chmod(path.c_str(), S_IRUSR | S_IWUSR);
    return true;
}
#endif
}

bool ClientIdentityProfileStore::loadExisting(
    const std::string& requestedProfileName,
    ClientIdentityProfile& outProfile,
    std::string* outError)
{
    if (outError)
        outError->clear();

    const std::string profileName = sanitizeProfileName(requestedProfileName);
    ClientIdentityProfile profile;
    profile.profileName = profileName;

#ifdef _WIN32
    if (!readWindowsCredential(profileName, profile.authToken, outError))
#else
    if (!readFallbackCredential(profileName, profile.authToken, outError))
#endif
    {
        if (outError && outError->empty())
            *outError = "credential slot does not exist: " + profileName;
        return false;
    }

    outProfile = std::move(profile);
    return true;
}

bool ClientIdentityProfileStore::loadOrCreate(
    const std::string& requestedProfileName,
    ClientIdentityProfile& outProfile,
    std::string* outError)
{
    if (outError)
        outError->clear();

    const std::string profileName = sanitizeProfileName(requestedProfileName);

    ClientIdentityProfile profile;
    profile.profileName = profileName;

#ifdef _WIN32
    if (readWindowsCredential(profileName, profile.authToken, outError))
    {
        outProfile = std::move(profile);
        return true;
    }

    if (outError && !outError->empty())
        return false;
#else
    if (readFallbackCredential(profileName, profile.authToken, outError))
    {
        outProfile = std::move(profile);
        return true;
    }

    if (outError && !outError->empty())
        return false;
#endif

    if (!generateToken(profile.authToken))
    {
        if (outError)
            *outError = "secure authentication-token generation failed";
        return false;
    }

#ifdef _WIN32
    if (!writeWindowsCredential(profileName, profile.authToken, outError))
        return false;
#else
    if (!writeFallbackCredential(profileName, profile.authToken, outError))
        return false;
#endif

    outProfile = std::move(profile);
    return true;
}
}
