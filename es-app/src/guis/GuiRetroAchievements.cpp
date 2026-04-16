//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  GuiRetroAchievements.cpp
//

#include "guis/GuiRetroAchievements.h"

#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiMsgBox.h"
#include "utils/LocalizationUtil.h"
#include "utils/StringUtil.h"

#include <algorithm>

GuiRetroAchievements::GuiRetroAchievements(const std::string& username, bool forceRefresh)
    : GuiSettings {_("RETROACHIEVEMENTS PROFILE")}
    , mPendingSummary {}
    , mFetchCompleted {false}
    , mSummaryApplied {false}
{
    mStatusValue = makeValueText(_("Cargando perfil..."));
    addWithLabel(_("STATUS"), mStatusValue);

    mFetchThread = std::thread {[this, username, forceRefresh] {
        const RetroAchievementUserSummary summary {
            RetroAchievements::fetchUserSummary(username, forceRefresh)};
        {
            std::lock_guard<std::mutex> lock {mSummaryMutex};
            mPendingSummary = summary;
        }
        mFetchCompleted = true;
    }};
}

GuiRetroAchievements::~GuiRetroAchievements()
{
    if (mFetchThread.joinable())
        mFetchThread.join();
}

void GuiRetroAchievements::update(int deltaTime)
{
    GuiSettings::update(deltaTime);

    if (!mSummaryApplied && mFetchCompleted) {
        if (mFetchThread.joinable())
            mFetchThread.join();

        RetroAchievementUserSummary summary;
        {
            std::lock_guard<std::mutex> lock {mSummaryMutex};
            summary = mPendingSummary;
        }

        applySummary(summary);
        mSummaryApplied = true;
    }
}

std::shared_ptr<TextComponent> GuiRetroAchievements::makeValueText(const std::string& value) const
{
    auto text = std::make_shared<TextComponent>(value, Font::get(FONT_SIZE_MEDIUM),
                                                mMenuColorPrimary, ALIGN_RIGHT);
    text->setSize(0.0f, text->getFont()->getHeight());
    return text;
}

ComponentListRow GuiRetroAchievements::makeRecentGameRow(const RetroAchievementRecentGame& game,
                                                         size_t index) const
{
    ComponentListRow row;

    if (!game.imageIconPath.empty()) {
        auto icon = std::make_shared<ImageComponent>();
        icon->setImage(game.imageIconPath);
        icon->setLinearInterpolation(true);
        icon->setResize(32.0f, 32.0f);
        row.addElement(icon, false, false);
    }

    std::string titleLabel {_("RECENT GAME")};
    titleLabel.append(" ").append(std::to_string(index + 1)).append(": ").append(game.title);
    auto title = std::make_shared<TextComponent>(titleLabel, Font::get(FONT_SIZE_SMALL),
                                                 mMenuColorPrimary, ALIGN_LEFT);
    row.addElement(title, true, true);

    std::string details;
    if (!game.consoleName.empty())
        details.append("[").append(game.consoleName).append("]");
    if (!game.lastPlayed.empty()) {
        if (!details.empty())
            details.append(" ");
        details.append(game.lastPlayed);
    }

    auto info = std::make_shared<TextComponent>(details, Font::get(FONT_SIZE_SMALL),
                                                mMenuColorSecondary, ALIGN_RIGHT);
    info->setSize(0.0f, info->getFont()->getHeight());
    row.addElement(info, false, true);

    return row;
}

void GuiRetroAchievements::applySummary(const RetroAchievementUserSummary& summary)
{
    if (!summary.valid) {
        mStatusValue->setText(summary.error.empty() ? _("No disponible") : summary.error);

        if (!summary.error.empty()) {
            mWindow->pushGui(new GuiMsgBox(
                _("No se pudo cargar el perfil de RetroAchievements:\n") + summary.error));
        }

        return;
    }

    mStatusValue->setText(_("Perfil cargado"));

    if (!summary.userPicPath.empty()) {
        auto avatar = std::make_shared<ImageComponent>();
        avatar->setImage(summary.userPicPath);
        avatar->setLinearInterpolation(true);
        avatar->setResize(72.0f, 72.0f);
        addWithLabel(_("AVATAR"), avatar);
    }
    else {
        addWithLabel(_("AVATAR"), makeValueText(_("UNAVAILABLE")));
    }

    addWithLabel(_("USERNAME"), makeValueText(summary.username));

    addWithLabel(_("DATA SOURCE"),
                 makeValueText(summary.usedCachedFallback ?
                                   _("CACHE (RESPALDO SIN CONEXION)") :
                                   _("PETICION EN VIVO")));

    if (!summary.cacheLastUpdated.empty())
        addWithLabel(_("LAST UPDATED"), makeValueText(summary.cacheLastUpdated));

    if (!summary.cacheAgeDisplay.empty())
        addWithLabel(_("UPDATED AGO"), makeValueText(summary.cacheAgeDisplay));

    const size_t maxRecentGames {std::min<size_t>(summary.recentGames.size(), 3)};
    addWithLabel(_("RECENT ACTIVITY"),
                 makeValueText(Utils::String::format(_("%i ENTRADAS"), maxRecentGames)));

    if (maxRecentGames == 0) {
        addWithLabel(_("RECENT GAMES"), makeValueText(_("NO SE ENCONTRARON JUEGOS RECIENTES")));
    }
    else {
        for (size_t index {0}; index < maxRecentGames; ++index) {
            const RetroAchievementRecentGame& game {summary.recentGames.at(index)};
            addRow(makeRecentGameRow(game, index));
        }
    }

    addWithLabel(_("POINTS"), makeValueText(summary.points));
    addWithLabel(_("TRUE POINTS"), makeValueText(summary.truePoints));
    if (!summary.softcorePoints.empty())
        addWithLabel(_("SOFTCORE POINTS"), makeValueText(summary.softcorePoints));

    std::string rank {summary.rank};
    if (!summary.totalRanked.empty())
        rank.append(" / ").append(summary.totalRanked);
    addWithLabel(_("RANK"), makeValueText(rank));

    if (!summary.memberSince.empty())
        addWithLabel(_("MEMBER SINCE"), makeValueText(summary.memberSince));
    if (!summary.motto.empty())
        addWithLabel(_("MOTTO"), makeValueText(summary.motto));
    if (!summary.richPresence.empty())
        addWithLabel(_("RICH PRESENCE"), makeValueText(summary.richPresence));
    if (!summary.status.empty())
        addWithLabel(_("ACCOUNT STATUS"), makeValueText(summary.status));
}
