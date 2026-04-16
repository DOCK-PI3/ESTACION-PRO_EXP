//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ThreadedCheevosIndexer.cpp
//
//  Background RetroAchievements indexing similar to ThreadedScraper behavior.
//

#include "ThreadedCheevosIndexer.h"

#include "FileData.h"
#include "Log.h"
#include "RetroAchievements.h"
#include "SystemData.h"
#include "Window.h"
#include "utils/LocalizationUtil.h"
#include "utils/MathUtil.h"
#include "utils/StringUtil.h"

#include <algorithm>

std::unique_ptr<ThreadedCheevosIndexer> ThreadedCheevosIndexer::sInstance {nullptr};

void ThreadedCheevosIndexer::start(bool forceAllGames,
                                   const std::set<std::string>* systems,
                                   bool showPopup)
{
    if (sInstance)
        return;

    sInstance = std::unique_ptr<ThreadedCheevosIndexer>(
        new ThreadedCheevosIndexer(forceAllGames, systems, showPopup));

    if (sInstance->mTotalGames == 0) {
        LOG(LogInfo) << "ThreadedCheevosIndexer start skipped: no candidate games";
        if (showPopup)
            Window::getInstance()->queueInfoPopup(_("NO GAMES REQUIRE RETROACHIEVEMENTS INDEXING"),
                                                  4000);
        sInstance.reset();
        return;
    }

    LOG(LogInfo) << "ThreadedCheevosIndexer started. total=" << sInstance->mTotalGames.load()
                 << " forceAll=" << (forceAllGames ? 1 : 0)
                 << " showPopup=" << (showPopup ? 1 : 0);
}

void ThreadedCheevosIndexer::stop()
{
    if (!sInstance)
        return;

    sInstance->mStopRequested = true;
    LOG(LogInfo) << "ThreadedCheevosIndexer stop requested";
}

bool ThreadedCheevosIndexer::isRunning()
{
    return static_cast<bool>(sInstance);
}

void ThreadedCheevosIndexer::update()
{
    if (!sInstance)
        return;

    if (!sInstance->mDone) {
        if (sInstance->mShowPopup) {
            const int total {std::max(1, sInstance->mTotalGames.load())};
            const int done {sInstance->mProcessedGames.load()};
            const int percent {std::clamp((done * 100) / total, 0, 100)};

            std::string message {_(("RETROACHIEVEMENTS INDEXING"))};
            message.append(" (")
                .append(std::to_string(done))
                .append("/")
                .append(std::to_string(total))
                .append(")");

            const std::string currentLabel {sInstance->getCurrentLabel()};
            if (!currentLabel.empty())
                message.append(" - ").append(currentLabel);

            Window::getInstance()->setPersistentInfoPopup(message, percent);
        }

        return;
    }

    sInstance->finish();
    sInstance.reset();
}

ThreadedCheevosIndexer::ThreadedCheevosIndexer(bool forceAllGames,
                                               const std::set<std::string>* systems,
                                               bool showPopup)
    : mForceAllGames {forceAllGames}
    , mShowPopup {showPopup}
    , mStopRequested {false}
    , mDone {false}
    , mSuccess {false}
    , mTotalGames {0}
    , mProcessedGames {0}
    , mMatchedGames {0}
{
    for (SystemData* system : SystemData::sSystemVector) {
        if (system == nullptr || !system->isCheevosSupported())
            continue;

        if (systems != nullptr && systems->find(system->getName()) == systems->cend())
            continue;

        for (FileData* game : system->getRootFolder()->getFilesRecursive(GAME)) {
            if (game == nullptr)
                continue;

            if (!mForceAllGames && game->hasCheevos() && !game->getCheevosHash().empty())
                continue;

            const std::string extension {
                Utils::String::toLower(Utils::FileSystem::getExtension(game->getPath()))};
            if (extension == ".pbp" || extension == ".cso")
                continue;

            mSearchQueue.emplace_back(game);
        }
    }

    mTotalGames = static_cast<int>(mSearchQueue.size());

    if (mTotalGames > 0)
        mWorker = std::thread(&ThreadedCheevosIndexer::run, this);
}

ThreadedCheevosIndexer::~ThreadedCheevosIndexer()
{
    mStopRequested = true;
    if (mWorker.joinable())
        mWorker.join();
}

void ThreadedCheevosIndexer::run()
{
    std::map<std::string, int> hashToGameId;
    std::string error;
    if (!RetroAchievements::getCheevosHashLibrary(hashToGameId, error)) {
        mError = error.empty() ? _("Failed to download RetroAchievements hash library") : error;
        mDone = true;
        return;
    }

    for (FileData* game : mSearchQueue) {
        if (mStopRequested)
            break;

        {
            std::unique_lock<std::mutex> lock {mLabelMutex};
            mCurrentLabel = game->getName();
        }

        const std::string hash {Utils::Math::md5Hash(game->getPath(), true)};
        if (!hash.empty()) {
            const std::string normalizedHash {Utils::String::toUpper(hash)};
            const auto match {hashToGameId.find(normalizedHash)};
            const int gameId {match != hashToGameId.cend() ? match->second : 0};

            if (Utils::String::toUpper(game->getCheevosHash()) != normalizedHash)
                game->metadata.set("cheevos_hash", normalizedHash);

            if (game->getCheevosGameId() != gameId)
                game->metadata.set("cheevos_id", std::to_string(gameId));

            if (gameId > 0)
                ++mMatchedGames;

            std::unique_lock<std::mutex> lock {mChangedSystemsMutex};
            mChangedSystems.insert(game->getSystem());
        }

        ++mProcessedGames;
    }

    mSuccess = !mStopRequested;
    mDone = true;
}

void ThreadedCheevosIndexer::finish()
{
    if (mShowPopup)
        Window::getInstance()->clearPersistentInfoPopup();

    {
        std::unique_lock<std::mutex> lock {mChangedSystemsMutex};
        for (SystemData* system : mChangedSystems) {
            if (system != nullptr)
                system->onMetaDataSavePoint();
        }
    }

    if (!mError.empty()) {
        Window::getInstance()->queueInfoPopup(mError, 5000);
        LOG(LogWarning) << "ThreadedCheevosIndexer failed: " << mError;
        return;
    }

    std::string summary;
    if (mStopRequested) {
        summary = Utils::String::format(_("RETROACHIEVEMENTS INDEXING STOPPED (%i/%i)"),
                                        mProcessedGames.load(), mTotalGames.load());
    }
    else {
        summary = Utils::String::format(_("RETROACHIEVEMENTS INDEXED: %i GAMES MAPPED"),
                                        mMatchedGames.load());
    }

    Window::getInstance()->queueInfoPopup(summary, 4000);
    LOG(LogInfo) << "ThreadedCheevosIndexer completed. processed=" << mProcessedGames.load()
                 << " matched=" << mMatchedGames.load();
}

std::string ThreadedCheevosIndexer::getCurrentLabel()
{
    std::unique_lock<std::mutex> lock {mLabelMutex};
    return mCurrentLabel;
}