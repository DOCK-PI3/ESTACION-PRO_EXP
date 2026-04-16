//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  RetroAchievements.cpp
//
//  Lightweight RetroAchievements client adapted to the ESTACION-PRO architecture.
//

#include "RetroAchievements.h"

#include "FileData.h"
#include "HttpReq.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "utils/FileSystemUtil.h"
#include "utils/LocalizationUtil.h"
#include "utils/MathUtil.h"
#include "utils/StringUtil.h"

#include <SDL2/SDL_timer.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
    const std::set<std::string> unsupportedCheevosExtensions {".pbp", ".cso"};
}

namespace
{
    constexpr Uint32 REQUEST_TIMEOUT_MS {30000};
    constexpr std::time_t USER_SUMMARY_CACHE_TTL {300};
    constexpr std::time_t GAME_PROGRESS_CACHE_TTL {120};

    std::string sanitizeCacheComponent(const std::string& value)
    {
        std::string sanitized;
        sanitized.reserve(value.size());

        for (const unsigned char character : value) {
            if (std::isalnum(character))
                sanitized.push_back(static_cast<char>(character));
            else
                sanitized.push_back('_');
        }

        return sanitized.empty() ? std::string {"default"} : sanitized;
    }

    std::string getRetroAchievementsCacheDirectory()
    {
        return Utils::FileSystem::getAppDataDirectory() + "/cache/retroachievements";
    }

    std::string getUserSummaryCachePath(const std::string& username)
    {
        return getRetroAchievementsCacheDirectory() + "/user_" +
               sanitizeCacheComponent(Utils::String::toLower(username)) + ".json";
    }

    std::string getGameProgressCachePath(int gameId, const std::string& username)
    {
        return getRetroAchievementsCacheDirectory() + "/game_" + std::to_string(gameId) +
               "_" + sanitizeCacheComponent(Utils::String::toLower(username)) + ".json";
    }

    std::string getRetroAchievementsMediaCacheDirectory()
    {
        return getRetroAchievementsCacheDirectory() + "/media";
    }

    std::string getUrlExtension(const std::string& url)
    {
        const size_t queryPos {url.find_first_of("?#")};
        const std::string trimmedUrl {url.substr(0, queryPos)};
        const size_t dotPos {trimmedUrl.find_last_of('.')};
        if (dotPos == std::string::npos)
            return ".png";

        std::string extension {Utils::String::toLower(trimmedUrl.substr(dotPos))};
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
            extension == ".gif" || extension == ".webp" || extension == ".svg")
            return extension;

        return ".png";
    }

    bool readCacheFile(const std::string& path, std::string& content)
    {
        if (!Utils::FileSystem::isRegularFile(path))
            return false;

        std::ifstream input {path, std::ios::binary};
        if (!input.is_open())
            return false;

        content.assign(std::istreambuf_iterator<char> {input}, std::istreambuf_iterator<char> {});
        return !content.empty();
    }

    bool isCacheFresh(const std::string& path, std::time_t maxAge)
    {
        if (!Utils::FileSystem::isRegularFile(path))
            return false;

        try {
            const auto writeTime {std::filesystem::last_write_time(path)};
            const auto now {decltype(writeTime)::clock::now()};
            const auto age {
                std::chrono::duration_cast<std::chrono::seconds>(now - writeTime).count()};
            return age >= 0 && age <= maxAge;
        }
        catch (...) {
            return false;
        }
    }

    bool getCacheLastWriteTime(const std::string& path, std::time_t& lastWriteTime)
    {
        if (!Utils::FileSystem::isRegularFile(path))
            return false;

        try {
            const auto writeTime {std::filesystem::last_write_time(path)};
            const auto nowFsClock {decltype(writeTime)::clock::now()};
            const auto nowSystemClock {std::chrono::system_clock::now()};
            const auto systemWriteTime {
                nowSystemClock + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                     writeTime - nowFsClock)};

            lastWriteTime = std::chrono::system_clock::to_time_t(systemWriteTime);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool getCacheAgeSeconds(const std::string& path, int& ageSeconds)
    {
        ageSeconds = -1;

        if (!Utils::FileSystem::isRegularFile(path))
            return false;

        try {
            const auto writeTime {std::filesystem::last_write_time(path)};
            const auto now {decltype(writeTime)::clock::now()};
            const auto age {
                std::chrono::duration_cast<std::chrono::seconds>(now - writeTime).count()};
            if (age < 0)
                return false;

            ageSeconds = static_cast<int>(age);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    std::string formatTimestamp(std::time_t timestamp)
    {
        if (timestamp <= 0)
            return "";

        std::tm localTime {};
#if defined(_WIN64)
        localtime_s(&localTime, &timestamp);
#else
        localtime_r(&timestamp, &localTime);
#endif

        std::ostringstream output;
        output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
        return output.str();
    }

    std::string formatCacheAgeDisplay(int cacheAgeSeconds)
    {
        if (cacheAgeSeconds < 0)
            return "";

        if (cacheAgeSeconds < 60)
            return _("just now");

        const int minutes {cacheAgeSeconds / 60};
        if (cacheAgeSeconds < 3600)
            return minutes == 1 ? _("1 minute ago") :
                                  Utils::String::format(_("%i minutes ago"), minutes);

        const int hours {cacheAgeSeconds / 3600};
        if (cacheAgeSeconds < 86400)
            return hours == 1 ? _("1 hour ago") :
                                Utils::String::format(_("%i hours ago"), hours);

        const int days {cacheAgeSeconds / 86400};
        return days == 1 ? _("1 day ago") :
                           Utils::String::format(_("%i days ago"), days);
    }

    void writeCacheFile(const std::string& path, const std::string& content)
    {
        const std::string directory {Utils::FileSystem::getParent(path)};
        if (!directory.empty() && !Utils::FileSystem::exists(directory))
            Utils::FileSystem::createDirectory(directory);

        std::ofstream output {path, std::ios::binary | std::ios::trunc};
        if (!output.is_open())
            return;

        output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    int jsonInt(const rapidjson::Value& value, const char* key)
    {
        if (!value.HasMember(key))
            return 0;

        const rapidjson::Value& member {value[key]};
        if (member.IsInt())
            return member.GetInt();
        if (member.IsUint())
            return static_cast<int>(member.GetUint());
        if (member.IsString()) {
            try {
                return std::stoi(member.GetString());
            }
            catch (...) {
                return 0;
            }
        }

        return 0;
    }

    std::string jsonString(const rapidjson::Value& value, const char* key)
    {
        if (!value.HasMember(key))
            return "";

        const rapidjson::Value& member {value[key]};
        if (member.IsString())
            return member.GetString();
        if (member.IsInt())
            return std::to_string(member.GetInt());
        if (member.IsUint())
            return std::to_string(member.GetUint());

        return "";
    }

    std::string buildRequestError(HttpReq& request)
    {
        if (!request.getErrorMsg().empty())
            return request.getErrorMsg();

        return Utils::String::format(_("Network error (status %i)"), request.status());
    }

    std::string buildImageUrl(const std::string& path)
    {
        if (path.empty())
            return "";
        if (path.find("http://") == 0 || path.find("https://") == 0)
            return path;
        return "https://retroachievements.org" + path;
    }

    std::string appendApiAuthParameters(const std::string& parameters)
    {
        std::string result {parameters};

        const std::string apiUsername {
            Utils::String::trim(Settings::getInstance()->getString("global.retroachievements.username"))};
        std::string apiKey {
            Utils::String::trim(Settings::getInstance()->getString("global.retroachievements.apikey"))};

        if (apiKey.empty()) {
            apiKey = Utils::String::trim(
                Settings::getInstance()->getString("global.retroachievements.token"));
        }

        if (apiKey.empty()) {
            apiKey = Utils::String::trim(
                Settings::getInstance()->getString("global.retroachievements.password"));
        }

        if (!apiUsername.empty() && !apiKey.empty()) {
            result.append("&z=").append(HttpReq::urlEncode(apiUsername));
            result.append("&y=").append(HttpReq::urlEncode(apiKey));
        }

        return result;
    }

    bool isUnauthorizedError(const std::string& error)
    {
        return error.find("401") != std::string::npos;
    }

    bool hasConfiguredApiKey()
    {
        return !Utils::String::trim(
                    Settings::getInstance()->getString("global.retroachievements.apikey"))
                    .empty();
    }

    std::string performBinaryRequest(const std::string& url, std::string& error)
    {
        HttpReq request {url, false};
        const Uint32 startTime {SDL_GetTicks()};

        while (request.status() == HttpReq::REQ_IN_PROGRESS) {
            SDL_Delay(5);

            if (SDL_GetTicks() - startTime > REQUEST_TIMEOUT_MS) {
                error = _("RetroAchievements media request timed out");
                return "";
            }
        }

        if (request.status() != HttpReq::REQ_SUCCESS) {
            error = buildRequestError(request);
            return "";
        }

        return request.getContent();
    }

    std::string cacheImageAsset(const std::string& url, const std::string& cachePrefix)
    {
        if (url.empty())
            return "";

        const std::string cacheDirectory {getRetroAchievementsMediaCacheDirectory()};
        if (!Utils::FileSystem::exists(cacheDirectory))
            Utils::FileSystem::createDirectory(cacheDirectory);

        const std::string cachePath = cacheDirectory + "/" + cachePrefix + "_" +
                          Utils::Math::md5Hash(url, false) +
                          getUrlExtension(url);
        if (Utils::FileSystem::isRegularFile(cachePath))
            return cachePath;

        std::string error;
        const std::string content {performBinaryRequest(url, error)};
        if (content.empty()) {
            LOG(LogWarning) << "RetroAchievements failed to cache media \"" << url
                            << "\": " << error;
            return "";
        }

        writeCacheFile(cachePath, content);
        return Utils::FileSystem::isRegularFile(cachePath) ? cachePath : std::string {};
    }

    bool sortAchievements(const RetroAchievementEntry& lhs, const RetroAchievementEntry& rhs)
    {
        if (lhs.isEarned() != rhs.isEarned())
            return lhs.isEarned() && !rhs.isEarned();

        if (lhs.isEarnedHardcore() != rhs.isEarnedHardcore())
            return lhs.isEarnedHardcore() && !rhs.isEarnedHardcore();

        return lhs.displayOrder < rhs.displayOrder;
    }
} // namespace

bool RetroAchievements::isEnabled()
{
    return Settings::getInstance()->getBool("global.retroachievements");
}

bool RetroAchievements::hasPassword()
{
    return !Settings::getInstance()->getString("global.retroachievements.password").empty();
}

bool RetroAchievements::hasToken()
{
    return !Settings::getInstance()->getString("global.retroachievements.token").empty();
}

bool RetroAchievements::hasCredentials()
{
    return hasUsername() && (hasToken() || hasPassword());
}

bool RetroAchievements::hasSession()
{
    return hasUsername() && hasToken();
}

bool RetroAchievements::hasUsername()
{
    return !Settings::getInstance()->getString("global.retroachievements.username").empty();
}

void RetroAchievements::clearSession(bool clearPassword)
{
    Settings* settings {Settings::getInstance()};
    settings->setString("global.retroachievements.token", "");

    if (clearPassword)
        settings->setString("global.retroachievements.password", "");

    clearCache();
}

bool RetroAchievements::clearCache()
{
    const std::string cacheDirectory {getRetroAchievementsCacheDirectory()};

    if (!Utils::FileSystem::exists(cacheDirectory))
        return true;

    if (!Utils::FileSystem::removeDirectory(cacheDirectory, true)) {
        LOG(LogWarning) << "RetroAchievements failed to clear cache directory \""
                        << cacheDirectory << "\"";
        return false;
    }

    LOG(LogInfo) << "RetroAchievements cache cleared: \"" << cacheDirectory << "\"";
    return true;
}

bool RetroAchievements::supportsSystem(const SystemData* system)
{
    return system != nullptr && system->isCheevosSupported();
}

int RetroAchievements::getGameId(const FileData* file)
{
    return file != nullptr ? file->getCheevosGameId() : 0;
}

std::string RetroAchievements::getApiUrl(const std::string& method, const std::string& parameters)
{
    return "https://retroachievements.org/API/" + method + ".php?" + parameters;
}

std::string RetroAchievements::performRequest(const std::string& url, std::string& error)
{
    HttpReq request {url, false};
    const Uint32 startTime {SDL_GetTicks()};

    while (request.status() == HttpReq::REQ_IN_PROGRESS) {
        SDL_Delay(5);

        if (SDL_GetTicks() - startTime > REQUEST_TIMEOUT_MS) {
            error = _("RetroAchievements request timed out");
            return "";
        }
    }

    if (request.status() != HttpReq::REQ_SUCCESS) {
        error = buildRequestError(request);
        return "";
    }

    return request.getContent();
}

std::string RetroAchievements::performCachedRequest(const std::string& url,
                                                    const std::string& cachePath,
                                                    std::time_t cacheTtl,
                                                    bool forceRefresh,
                                                    bool* usedCachedFallback,
                                                    std::time_t* cacheLastUpdated,
                                                    int* cacheAgeSeconds,
                                                    std::string& error)
{
    std::string cachedContent;

    if (usedCachedFallback != nullptr)
        *usedCachedFallback = false;
    if (cacheLastUpdated != nullptr)
        *cacheLastUpdated = 0;
    if (cacheAgeSeconds != nullptr)
        *cacheAgeSeconds = -1;

    if (forceRefresh) {
        LOG(LogInfo) << "RetroAchievements cache bypassed (forced refresh): \"" << cachePath
                     << "\"";
    }

    if (!forceRefresh) {
        if (isCacheFresh(cachePath, cacheTtl) && readCacheFile(cachePath, cachedContent)) {
            LOG(LogInfo) << "RetroAchievements cache hit: \"" << cachePath << "\"";
            if (cacheLastUpdated != nullptr)
                getCacheLastWriteTime(cachePath, *cacheLastUpdated);
            if (cacheAgeSeconds != nullptr)
                getCacheAgeSeconds(cachePath, *cacheAgeSeconds);
            error.clear();
            return cachedContent;
        }

        LOG(LogDebug) << "RetroAchievements cache miss/stale: \"" << cachePath << "\"";
    }

    std::string networkError;
    const std::string response {performRequest(url, networkError)};
    if (!response.empty()) {
        writeCacheFile(cachePath, response);
        LOG(LogInfo) << "RetroAchievements cache updated from network: \"" << cachePath
                     << "\"";
        if (cacheLastUpdated != nullptr)
            getCacheLastWriteTime(cachePath, *cacheLastUpdated);
        if (cacheAgeSeconds != nullptr)
            getCacheAgeSeconds(cachePath, *cacheAgeSeconds);
        error.clear();
        return response;
    }

    if (readCacheFile(cachePath, cachedContent)) {
        LOG(LogWarning) << "RetroAchievements request failed, using cached data for \""
                        << cachePath << "\": " << networkError;
        if (usedCachedFallback != nullptr)
            *usedCachedFallback = true;
        if (cacheLastUpdated != nullptr)
            getCacheLastWriteTime(cachePath, *cacheLastUpdated);
        if (cacheAgeSeconds != nullptr)
            getCacheAgeSeconds(cachePath, *cacheAgeSeconds);
        error.clear();
        return cachedContent;
    }

    error = networkError;
    return "";
}

bool RetroAchievements::testAccount(const std::string& username,
                                    const std::string& password,
                                    std::string& tokenOrError)
{
    const std::string trimmedUsername {Utils::String::trim(username)};
    const std::string trimmedPassword {Utils::String::trim(password)};

    if (trimmedUsername.empty() || trimmedPassword.empty()) {
        tokenOrError = _("A RetroAchievements username and password are required");
        return false;
    }

    std::string error;
    const std::string content {
        performRequest("https://retroachievements.org/dorequest.php?r=login&u=" +
                           HttpReq::urlEncode(trimmedUsername) +
                           "&p=" + HttpReq::urlEncode(trimmedPassword),
                       error)};
    if (content.empty()) {
        tokenOrError = error;
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError() || !doc.HasMember("Success")) {
        tokenOrError = _("Unable to parse RetroAchievements login response");
        return false;
    }

    if (doc["Success"].IsTrue()) {
        tokenOrError = doc.HasMember("Token") && doc["Token"].IsString() ?
                           doc["Token"].GetString() :
                           "";
        return true;
    }

    tokenOrError = doc.HasMember("Error") && doc["Error"].IsString() ? doc["Error"].GetString() :
                                                                    _("Unknown RetroAchievements error");
    if (trimmedUsername.find('@') != std::string::npos &&
        tokenOrError.find("Invalid user/password") != std::string::npos) {
        tokenOrError += "\n";
        tokenOrError += _("Use your RetroAchievements username, not your email address");
    }

    return false;
}

bool RetroAchievements::fetchCheevosHashes(std::map<std::string, int>& hashToGameId,
                                           std::string& error)
{
    hashToGameId.clear();

    const std::string content {
        performRequest("https://retroachievements.org/dorequest.php?r=hashlibrary", error)};
    if (content.empty())
        return false;

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
        error = rapidjson::GetParseError_En(doc.GetParseError());
        return false;
    }

    if (!doc.HasMember("MD5List") || !doc["MD5List"].IsObject()) {
        error = _("RetroAchievements hash library response is missing MD5List");
        return false;
    }

    const rapidjson::Value& md5List {doc["MD5List"]};
    for (auto it = md5List.MemberBegin(); it != md5List.MemberEnd(); ++it) {
        const std::string md5 {Utils::String::toUpper(it->name.GetString())};
        int gameId {0};

        if (it->value.IsInt())
            gameId = it->value.GetInt();
        else if (it->value.IsUint())
            gameId = static_cast<int>(it->value.GetUint());
        else if (it->value.IsString()) {
            try {
                gameId = std::stoi(it->value.GetString());
            }
            catch (...) {
                gameId = 0;
            }
        }

        if (!md5.empty() && gameId > 0)
            hashToGameId[md5] = gameId;
    }

    return !hashToGameId.empty();
}

bool RetroAchievements::getCheevosHashLibrary(std::map<std::string, int>& hashToGameId,
                                              std::string& error)
{
    return fetchCheevosHashes(hashToGameId, error);
}

bool RetroAchievements::populateCheevosIndexes(bool forceAllGames,
                                               std::string* error,
                                               const std::set<std::string>* systems,
                                               int* indexedGames)
{
    std::map<std::string, int> hashToGameId;
    std::string localError;
    if (!fetchCheevosHashes(hashToGameId, localError)) {
        if (error != nullptr)
            *error = localError;
        return false;
    }

    int updatedCount {0};

    for (SystemData* system : SystemData::sSystemVector) {
        if (system == nullptr || !supportsSystem(system))
            continue;

        if (systems != nullptr && systems->find(system->getName()) == systems->cend())
            continue;

        bool systemChanged {false};
        const std::vector<FileData*> games {system->getRootFolder()->getFilesRecursive(GAME)};

        for (FileData* game : games) {
            if (game == nullptr)
                continue;

            if (!forceAllGames && game->hasCheevos() && !game->getCheevosHash().empty())
                continue;

            const std::string extension {Utils::String::toLower(
                Utils::FileSystem::getExtension(game->getPath()))};
            if (unsupportedCheevosExtensions.find(extension) != unsupportedCheevosExtensions.cend())
                continue;

            const std::string hash {Utils::Math::md5Hash(game->getPath(), true)};
            if (hash.empty())
                continue;

            const std::string normalizedHash {Utils::String::toUpper(hash)};
            const auto match {hashToGameId.find(normalizedHash)};

            const std::string currentHash {Utils::String::toUpper(game->getCheevosHash())};
            const int currentId {game->getCheevosGameId()};
            const int newId {match != hashToGameId.cend() ? match->second : 0};

            if (currentHash != normalizedHash) {
                game->metadata.set("cheevos_hash", normalizedHash);
                systemChanged = true;
            }

            if (currentId != newId) {
                game->metadata.set("cheevos_id", std::to_string(newId));
                systemChanged = true;
            }

            if (newId > 0)
                ++updatedCount;
        }

        if (systemChanged)
            system->onMetaDataSavePoint();
    }

    if (indexedGames != nullptr)
        *indexedGames = updatedCount;

    LOG(LogInfo) << "RetroAchievements indexing completed, " << updatedCount
                 << " games mapped to RetroAchievements ids";

    return true;
}

RetroAchievementUserSummary RetroAchievements::fetchUserSummary(const std::string& username,
                                                                bool forceRefresh)
{
    RetroAchievementUserSummary summary;

    std::string userName {Utils::String::trim(username)};
    if (userName.empty())
        userName = Utils::String::trim(
            Settings::getInstance()->getString("global.retroachievements.username"));

    if (userName.empty()) {
        summary.error = _("RetroAchievements username is not configured");
        return summary;
    }

    std::string error;
    bool usedCachedFallback {false};
    std::time_t cacheLastUpdated {0};
    int cacheAgeSeconds {-1};
    const std::string requestParameters {
        appendApiAuthParameters("u=" + HttpReq::urlEncode(userName) + "&g=10&a=10")};
    const std::string content {performCachedRequest(
        getApiUrl("API_GetUserSummary", requestParameters),
        getUserSummaryCachePath(userName), USER_SUMMARY_CACHE_TTL, forceRefresh,
        &usedCachedFallback, &cacheLastUpdated, &cacheAgeSeconds, error)};
    if (content.empty()) {
        if (isUnauthorizedError(error)) {
            summary.error = hasConfiguredApiKey() ?
                                _("La API key de RetroAchievements es invalida o ha caducado") :
                                _("Configura la API key de RetroAchievements en ajustes");
            return summary;
        }

        summary.error = error;
        return summary;
    }

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
        summary.error = rapidjson::GetParseError_En(doc.GetParseError());
        return summary;
    }

    summary.valid = true;
    summary.usedCachedFallback = usedCachedFallback;
    summary.cacheLastUpdated = formatTimestamp(cacheLastUpdated);
    summary.cacheAgeSeconds = cacheAgeSeconds;
    summary.cacheAgeDisplay = formatCacheAgeDisplay(cacheAgeSeconds);
    summary.username = userName;
    summary.rank = jsonString(doc, "Rank");
    summary.totalRanked = jsonString(doc, "TotalRanked");
    summary.points = jsonString(doc, "TotalPoints");
    summary.truePoints = jsonString(doc, "TotalTruePoints");
    summary.softcorePoints = jsonString(doc, "TotalSoftcorePoints");
    summary.memberSince = jsonString(doc, "MemberSince");
    summary.motto = jsonString(doc, "Motto");
    summary.status = jsonString(doc, "Status");
    summary.richPresence = jsonString(doc, "RichPresenceMsg");
    summary.userPic = buildImageUrl(jsonString(doc, "UserPic"));
    summary.userPicPath = cacheImageAsset(summary.userPic, "userpic");

    if (doc.HasMember("RecentlyPlayed") && doc["RecentlyPlayed"].IsArray()) {
        for (const rapidjson::Value& entry : doc["RecentlyPlayed"].GetArray()) {
            RetroAchievementRecentGame recentGame;
            recentGame.gameId = jsonInt(entry, "GameID");
            recentGame.title = jsonString(entry, "Title");
            recentGame.consoleName = jsonString(entry, "ConsoleName");
            recentGame.lastPlayed = jsonString(entry, "LastPlayed");
            recentGame.imageIconUrl = buildImageUrl(jsonString(entry, "ImageIcon"));
            summary.recentGames.emplace_back(recentGame);
        }

        const size_t maxVisibleRecentGames {std::min<size_t>(summary.recentGames.size(), 5)};
        for (size_t index {0}; index < maxVisibleRecentGames; ++index) {
            RetroAchievementRecentGame& recentGame {summary.recentGames.at(index)};
            recentGame.imageIconPath = cacheImageAsset(recentGame.imageIconUrl, "recentgame");
        }
    }

    return summary;
}

RetroAchievementGameProgress RetroAchievements::fetchGameProgress(int gameId,
                                                                  const std::string& username,
                                                                  bool forceRefresh)
{
    RetroAchievementGameProgress progress;

    if (gameId <= 0) {
        progress.error = _("This game has no RetroAchievements game id");
        return progress;
    }

    std::string parameters {"g=" + std::to_string(gameId)};
    std::string userName {Utils::String::trim(username)};
    if (userName.empty())
        userName = Utils::String::trim(
            Settings::getInstance()->getString("global.retroachievements.username"));
    if (!userName.empty())
        parameters.append("&u=").append(HttpReq::urlEncode(userName));
    parameters = appendApiAuthParameters(parameters);

    std::string error;
    bool usedCachedFallback {false};
    std::time_t cacheLastUpdated {0};
    int cacheAgeSeconds {-1};
    const std::string content {performCachedRequest(
        getApiUrl("API_GetGameInfoAndUserProgress", parameters),
        getGameProgressCachePath(gameId, userName), GAME_PROGRESS_CACHE_TTL, forceRefresh,
        &usedCachedFallback, &cacheLastUpdated, &cacheAgeSeconds, error)};
    if (content.empty()) {
        if (isUnauthorizedError(error)) {
            progress.error = hasConfiguredApiKey() ?
                                 _("La API key de RetroAchievements es invalida o ha caducado") :
                                 _("Configura la API key de RetroAchievements en ajustes");
            return progress;
        }

        progress.error = error;
        return progress;
    }

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
        progress.error = rapidjson::GetParseError_En(doc.GetParseError());
        return progress;
    }

    progress.valid = true;
    progress.usedCachedFallback = usedCachedFallback;
    progress.cacheLastUpdated = formatTimestamp(cacheLastUpdated);
    progress.cacheAgeSeconds = cacheAgeSeconds;
    progress.cacheAgeDisplay = formatCacheAgeDisplay(cacheAgeSeconds);
    progress.gameId = jsonInt(doc, "ID");
    progress.title = jsonString(doc, "Title");
    progress.consoleName = jsonString(doc, "ConsoleName");
    progress.imageIconUrl = buildImageUrl(jsonString(doc, "ImageIcon"));
    progress.imageTitleUrl = buildImageUrl(jsonString(doc, "ImageTitle"));
    progress.imageIngameUrl = buildImageUrl(jsonString(doc, "ImageIngame"));
    progress.imageBoxArtUrl = buildImageUrl(jsonString(doc, "ImageBoxArt"));
    progress.imageBoxArtPath = cacheImageAsset(progress.imageBoxArtUrl, "boxart");
    progress.numAchievements = jsonInt(doc, "NumAchievements");
    progress.numAwardedToUser = jsonInt(doc, "NumAwardedToUser");
    progress.numAwardedToUserHardcore = jsonInt(doc, "NumAwardedToUserHardcore");
    progress.userCompletion = jsonString(doc, "UserCompletion");
    progress.userCompletionHardcore = jsonString(doc, "UserCompletionHardcore");

    if (doc.HasMember("Achievements") && doc["Achievements"].IsObject()) {
        const rapidjson::Value& achievements {doc["Achievements"]};
        for (auto it = achievements.MemberBegin(); it != achievements.MemberEnd(); ++it) {
            const rapidjson::Value& value {it->value};
            RetroAchievementEntry achievement;
            achievement.id = jsonInt(value, "ID");
            achievement.points = jsonInt(value, "Points");
            achievement.displayOrder = jsonInt(value, "DisplayOrder");
            achievement.title = jsonString(value, "Title");
            achievement.description = jsonString(value, "Description");
            achievement.badgeName = jsonString(value, "BadgeName");
            achievement.dateEarned = jsonString(value, "DateEarned");
            achievement.dateEarnedHardcore = jsonString(value, "DateEarnedHardcore");

            if (!achievement.badgeName.empty()) {
                achievement.badgeUrl = std::string {"https://retroachievements.org/Badge/"} +
                                       achievement.badgeName +
                                       (achievement.isEarned() ? ".png" : "_lock.png");
            }

            progress.achievements.emplace_back(achievement);
        }

        std::sort(progress.achievements.begin(), progress.achievements.end(), sortAchievements);

        const size_t maxVisibleBadges {std::min<size_t>(progress.achievements.size(), 20)};
        for (size_t index {0}; index < maxVisibleBadges; ++index) {
            RetroAchievementEntry& achievement {progress.achievements.at(index)};
            achievement.badgePath = cacheImageAsset(achievement.badgeUrl, "badge");
        }
    }

    return progress;
}