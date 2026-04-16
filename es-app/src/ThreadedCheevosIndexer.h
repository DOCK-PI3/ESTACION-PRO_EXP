//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ThreadedCheevosIndexer.h
//
//  Background RetroAchievements indexing similar to ThreadedScraper behavior.
//

#ifndef ES_APP_THREADED_CHEEVOS_INDEXER_H
#define ES_APP_THREADED_CHEEVOS_INDEXER_H

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

class FileData;
class SystemData;

class ThreadedCheevosIndexer
{
public:
    ~ThreadedCheevosIndexer();

    static void start(bool forceAllGames = false,
                      const std::set<std::string>* systems = nullptr,
                      bool showPopup = true);
    static void stop();
    static bool isRunning();
    static void update();

private:
    ThreadedCheevosIndexer(bool forceAllGames,
                           const std::set<std::string>* systems,
                           bool showPopup);

    void run();
    void finish();
    std::string getCurrentLabel();

    static std::unique_ptr<ThreadedCheevosIndexer> sInstance;

    std::vector<FileData*> mSearchQueue;
    bool mForceAllGames;
    bool mShowPopup;

    std::thread mWorker;
    std::atomic<bool> mStopRequested;
    std::atomic<bool> mDone;
    std::atomic<bool> mSuccess;

    std::atomic<int> mTotalGames;
    std::atomic<int> mProcessedGames;
    std::atomic<int> mMatchedGames;

    std::string mCurrentLabel;
    std::string mError;

    std::mutex mLabelMutex;
    std::mutex mChangedSystemsMutex;
    std::set<SystemData*> mChangedSystems;
};

#endif // ES_APP_THREADED_CHEEVOS_INDEXER_H