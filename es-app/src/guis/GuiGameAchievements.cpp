//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  GuiGameAchievements.cpp
//

#include "guis/GuiGameAchievements.h"

#include "FileData.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "utils/LocalizationUtil.h"
#include "utils/StringUtil.h"

#include <algorithm>

GuiGameAchievements::GuiGameAchievements(FileData* file)
    : GuiSettings {_("GAME ACHIEVEMENTS")}
    , mPendingProgress {}
    , mFetchCompleted {false}
    , mProgressApplied {false}
{
    mStatusValue = makeValueText(_("Loading achievements..."));
    addWithLabel(_("STATUS"), mStatusValue);

    if (file == nullptr) {
        mStatusValue->setText(_("No game selected"));
        mProgressApplied = true;
        return;
    }

    addWithLabel(_("GAME"), makeValueText(file->getName()));

    const int gameId {file->getCheevosGameId()};
    mFetchThread = std::thread {[this, gameId] {
        const RetroAchievementGameProgress progress {RetroAchievements::fetchGameProgress(gameId)};
        {
            std::lock_guard<std::mutex> lock {mProgressMutex};
            mPendingProgress = progress;
        }
        mFetchCompleted = true;
    }};
}

GuiGameAchievements::~GuiGameAchievements()
{
    if (mFetchThread.joinable())
        mFetchThread.join();
}

void GuiGameAchievements::update(int deltaTime)
{
    GuiSettings::update(deltaTime);

    if (!mProgressApplied && mFetchCompleted) {
        if (mFetchThread.joinable())
            mFetchThread.join();

        RetroAchievementGameProgress progress;
        {
            std::lock_guard<std::mutex> lock {mProgressMutex};
            progress = mPendingProgress;
        }

        applyProgress(progress);
        mProgressApplied = true;
    }
}

std::shared_ptr<TextComponent> GuiGameAchievements::makeValueText(const std::string& value) const
{
    auto text = std::make_shared<TextComponent>(value, Font::get(FONT_SIZE_MEDIUM),
                                                mMenuColorPrimary, ALIGN_RIGHT);
    text->setSize(0.0f, text->getFont()->getHeight());
    return text;
}

ComponentListRow GuiGameAchievements::makeAchievementRow(
    const RetroAchievementEntry& achievement) const
{
    ComponentListRow row;

    if (!achievement.badgePath.empty()) {
        auto badge = std::make_shared<ImageComponent>();
        badge->setImage(achievement.badgePath);
        badge->setLinearInterpolation(true);
        badge->setResize(32.0f, 32.0f);
        row.addElement(badge, false, false);
    }

    const std::string prefix {
        achievement.isEarnedHardcore() ? "[HC]" : (achievement.isEarned() ? "[X]" : "[ ]")};
    auto title = std::make_shared<TextComponent>(prefix + " " + achievement.title,
                                                 Font::get(FONT_SIZE_MEDIUM),
                                                 mMenuColorPrimary, ALIGN_LEFT);
    row.addElement(title, true, true);

    std::string value {std::to_string(achievement.points) + " pts"};
    if (!achievement.description.empty())
        value.append(" - ").append(achievement.description);

    auto details = std::make_shared<TextComponent>(Utils::String::toUpper(value),
                                                   Font::get(FONT_SIZE_SMALL),
                                                   mMenuColorSecondary, ALIGN_RIGHT);
    details->setSize(0.0f, details->getFont()->getHeight());
    row.addElement(details, false, true);

    return row;
}

void GuiGameAchievements::applyProgress(const RetroAchievementGameProgress& progress)
{
    if (!progress.valid) {
        mStatusValue->setText(progress.error.empty() ? _("Unavailable") : progress.error);
        return;
    }

    mStatusValue->setText(_("Data loaded"));

    if (!progress.imageBoxArtPath.empty()) {
        auto boxArt = std::make_shared<ImageComponent>();
        boxArt->setImage(progress.imageBoxArtPath);
        boxArt->setLinearInterpolation(true);
        boxArt->setResize(96.0f, 96.0f);
        addWithLabel(_("BOX ART"), boxArt);
    }

    if (!progress.title.empty())
        addWithLabel(_("RETROACHIEVEMENTS TITLE"), makeValueText(progress.title));
    if (!progress.consoleName.empty())
        addWithLabel(_("CONSOLE"), makeValueText(progress.consoleName));

    if (progress.usedCachedFallback)
        addWithLabel(_("DATA SOURCE"), makeValueText(_("CACHE (RESPALDO SIN CONEXION)")));

    if (!progress.cacheLastUpdated.empty())
        addWithLabel(_("LAST UPDATED"), makeValueText(progress.cacheLastUpdated));

    if (!progress.cacheAgeDisplay.empty())
        addWithLabel(_("UPDATED AGO"), makeValueText(progress.cacheAgeDisplay));

    addWithLabel(_("UNLOCKED"),
                 makeValueText(std::to_string(progress.numAwardedToUser) + "/" +
                               std::to_string(progress.numAchievements)));
    addWithLabel(_("HARDCORE UNLOCKED"),
                 makeValueText(std::to_string(progress.numAwardedToUserHardcore) + "/" +
                               std::to_string(progress.numAchievements)));

    if (!progress.userCompletion.empty())
        addWithLabel(_("COMPLETION"), makeValueText(progress.userCompletion));
    if (!progress.userCompletionHardcore.empty())
        addWithLabel(_("HARDCORE COMPLETION"), makeValueText(progress.userCompletionHardcore));

    const size_t maxAchievements {std::min<size_t>(progress.achievements.size(), 20)};
    for (size_t index {0}; index < maxAchievements; ++index) {
        const RetroAchievementEntry& achievement {progress.achievements.at(index)};
        addRow(makeAchievementRow(achievement));
    }
}
