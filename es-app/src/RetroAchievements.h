//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  RetroAchievements.h
//
//  Lightweight RetroAchievements client adapted to the ESTACION-PRO architecture.
//

#ifndef ES_APP_RETRO_ACHIEVEMENTS_H
#define ES_APP_RETRO_ACHIEVEMENTS_H

#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>

class FileData;
class SystemData;

struct RetroAchievementEntry {
    int id {0};
    int points {0};
    int displayOrder {0};
    std::string title;
    std::string description;
    std::string badgeName;
    std::string badgeUrl;
    std::string badgePath;
    std::string dateEarned;
    std::string dateEarnedHardcore;

    bool isEarned() const { return !dateEarned.empty() || !dateEarnedHardcore.empty(); }
    bool isEarnedHardcore() const { return !dateEarnedHardcore.empty(); }
};

struct RetroAchievementRecentGame {
    int gameId {0};
    std::string title;
    std::string consoleName;
    std::string lastPlayed;
    std::string imageIconUrl;
    std::string imageIconPath;
};

struct RetroAchievementUserSummary {
    bool valid {false};
    bool usedCachedFallback {false};
    std::string cacheLastUpdated;
    int cacheAgeSeconds {-1};
    std::string cacheAgeDisplay;
    std::string username;
    std::string rank;
    std::string totalRanked;
    std::string points;
    std::string truePoints;
    std::string softcorePoints;
    std::string memberSince;
    std::string motto;
    std::string status;
    std::string richPresence;
    std::string userPic;
    std::string userPicPath;
    std::vector<RetroAchievementRecentGame> recentGames;
    std::string error;
};

struct RetroAchievementGameProgress {
    bool valid {false};
    bool usedCachedFallback {false};
    std::string cacheLastUpdated;
    int cacheAgeSeconds {-1};
    std::string cacheAgeDisplay;
    int gameId {0};
    int numAchievements {0};
    int numAwardedToUser {0};
    int numAwardedToUserHardcore {0};
    std::string title;
    std::string consoleName;
    std::string imageIconUrl;
    std::string imageTitleUrl;
    std::string imageIngameUrl;
    std::string imageBoxArtUrl;
    std::string imageBoxArtPath;
    std::string userCompletion;
    std::string userCompletionHardcore;
    std::vector<RetroAchievementEntry> achievements;
    std::string error;
};

class RetroAchievements
{
public:
    static bool isEnabled();
    static bool hasPassword();
    static bool hasToken();
    static bool hasCredentials();
    static bool hasSession();
    static bool hasUsername();
    static bool clearCache();
    static void clearSession(bool clearPassword = true);
    static bool supportsSystem(const SystemData* system);
    static int getGameId(const FileData* file);

    static bool testAccount(const std::string& username,
                            const std::string& password,
                            std::string& tokenOrError);
    static bool getCheevosHashLibrary(std::map<std::string, int>& hashToGameId,
                                      std::string& error);
    static bool populateCheevosIndexes(bool forceAllGames,
                                       std::string* error = nullptr,
                                       const std::set<std::string>* systems = nullptr,
                                       int* indexedGames = nullptr);
        static RetroAchievementUserSummary fetchUserSummary(const std::string& username = "",
                                                                                                                bool forceRefresh = false);
    static RetroAchievementGameProgress fetchGameProgress(int gameId,
                                                                                                                    const std::string& username = "",
                                                                                                                    bool forceRefresh = false);

private:
    static bool fetchCheevosHashes(std::map<std::string, int>& hashToGameId,
                                   std::string& error);
    static std::string getApiUrl(const std::string& method, const std::string& parameters);
    static std::string performRequest(const std::string& url, std::string& error);
    static std::string performCachedRequest(const std::string& url,
                                            const std::string& cachePath,
                                            std::time_t cacheTtl,
                                            bool forceRefresh,
                                            bool* usedCachedFallback,
                                            std::time_t* cacheLastUpdated,
                                            int* cacheAgeSeconds,
                                            std::string& error);
};

#endif // ES_APP_RETRO_ACHIEVEMENTS_H