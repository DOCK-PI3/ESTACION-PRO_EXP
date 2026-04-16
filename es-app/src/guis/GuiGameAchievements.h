//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  GuiGameAchievements.h
//

#ifndef ES_APP_GUIS_GUI_GAME_ACHIEVEMENTS_H
#define ES_APP_GUIS_GUI_GAME_ACHIEVEMENTS_H

#include "RetroAchievements.h"
#include "guis/GuiSettings.h"

#include <atomic>
#include <mutex>
#include <thread>

class FileData;
class TextComponent;

class GuiGameAchievements : public GuiSettings
{
public:
    GuiGameAchievements(FileData* file);
    ~GuiGameAchievements() override;

    void update(int deltaTime) override;

private:
    std::shared_ptr<TextComponent> makeValueText(const std::string& value) const;
    ComponentListRow makeAchievementRow(const RetroAchievementEntry& achievement) const;
    void applyProgress(const RetroAchievementGameProgress& progress);

    std::shared_ptr<TextComponent> mStatusValue;
    std::thread mFetchThread;
    std::mutex mProgressMutex;
    RetroAchievementGameProgress mPendingProgress;
    std::atomic<bool> mFetchCompleted;
    bool mProgressApplied;
};

#endif // ES_APP_GUIS_GUI_GAME_ACHIEVEMENTS_H