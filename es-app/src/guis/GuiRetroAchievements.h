//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  GuiRetroAchievements.h
//

#ifndef ES_APP_GUIS_GUI_RETRO_ACHIEVEMENTS_H
#define ES_APP_GUIS_GUI_RETRO_ACHIEVEMENTS_H

#include "guis/GuiSettings.h"
#include "RetroAchievements.h"

#include <atomic>
#include <mutex>
#include <thread>

class TextComponent;
struct RetroAchievementUserSummary;

class GuiRetroAchievements : public GuiSettings
{
public:
    GuiRetroAchievements(const std::string& username = "", bool forceRefresh = false);
    ~GuiRetroAchievements() override;

    void update(int deltaTime) override;

private:
    std::shared_ptr<TextComponent> makeValueText(const std::string& value) const;
    ComponentListRow makeRecentGameRow(const RetroAchievementRecentGame& game,
                                       size_t index) const;
    void applySummary(const RetroAchievementUserSummary& summary);

    std::shared_ptr<TextComponent> mStatusValue;
    std::thread mFetchThread;
    std::mutex mSummaryMutex;
    RetroAchievementUserSummary mPendingSummary;
    std::atomic<bool> mFetchCompleted;
    bool mSummaryApplied;
};

#endif // ES_APP_GUIS_GUI_RETRO_ACHIEVEMENTS_H