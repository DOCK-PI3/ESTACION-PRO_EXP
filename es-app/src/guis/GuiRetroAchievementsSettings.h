//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  GuiRetroAchievementsSettings.h
//

#ifndef ES_APP_GUIS_GUI_RETRO_ACHIEVEMENTS_SETTINGS_H
#define ES_APP_GUIS_GUI_RETRO_ACHIEVEMENTS_SETTINGS_H

#include "guis/GuiSettings.h"

class GuiRetroAchievementsSettings : public GuiSettings
{
public:
    GuiRetroAchievementsSettings(const std::string& title);
    ~GuiRetroAchievementsSettings() override = default;
};

#endif // ES_APP_GUIS_GUI_RETRO_ACHIEVEMENTS_SETTINGS_H