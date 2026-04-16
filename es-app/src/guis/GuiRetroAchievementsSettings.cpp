//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  GuiRetroAchievementsSettings.cpp
//

#include "guis/GuiRetroAchievementsSettings.h"

#include "RetroAchievements.h"
#include "Settings.h"
#include "ThreadedCheevosIndexer.h"
#include "components/SwitchComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiRetroAchievements.h"
#include "utils/LocalizationUtil.h"
#include "utils/StringUtil.h"

GuiRetroAchievementsSettings::GuiRetroAchievementsSettings(const std::string& title)
    : GuiSettings {title}
{
    auto makeValueText = [this](const std::string& value) {
        auto text = std::make_shared<TextComponent>(value, Font::get(FONT_SIZE_MEDIUM),
                                                    mMenuColorPrimary, ALIGN_RIGHT);
        text->setSize(0.0f, text->getFont()->getHeight());
        return text;
    };

    auto username = std::make_shared<TextComponent>("", Font::get(FONT_SIZE_MEDIUM),
                                                    mMenuColorPrimary, ALIGN_RIGHT);
    addEditableTextComponent(_("RETROACHIEVEMENTS USERNAME"), username,
                             Settings::getInstance()->getString("global.retroachievements.username"));
    username->setSize(0.0f, username->getFont()->getHeight());
    addSaveFunc([username, this] {
        const std::string trimmedUsername {Utils::String::trim(username->getValue())};
        if (trimmedUsername !=
            Settings::getInstance()->getString("global.retroachievements.username")) {
            Settings::getInstance()->setString("global.retroachievements.username",
                                               trimmedUsername);
            setNeedsSaving();
        }
    });

    auto password = std::make_shared<TextComponent>("", Font::get(FONT_SIZE_MEDIUM),
                                                    mMenuColorPrimary, ALIGN_RIGHT);
    const std::string storedPassword {
        Settings::getInstance()->getString("global.retroachievements.password")};
    if (!storedPassword.empty())
        password->setHiddenValue(storedPassword);
    addEditableTextComponent(_("RETROACHIEVEMENTS PASSWORD"), password,
                             storedPassword.empty() ? "" : "********", "", true);
    password->setSize(0.0f, password->getFont()->getHeight());
    addSaveFunc([password, this] {
        const std::string trimmedPassword {Utils::String::trim(password->getHiddenValue())};
        if (trimmedPassword !=
            Settings::getInstance()->getString("global.retroachievements.password")) {
            Settings::getInstance()->setString("global.retroachievements.password",
                                               trimmedPassword);
            setNeedsSaving();
        }
    });

    auto apiKey = std::make_shared<TextComponent>("", Font::get(FONT_SIZE_MEDIUM),
                                                  mMenuColorPrimary, ALIGN_RIGHT);
    const std::string storedApiKey {
        Settings::getInstance()->getString("global.retroachievements.apikey")};
    if (!storedApiKey.empty())
        apiKey->setHiddenValue(storedApiKey);
    addEditableTextComponent(_("API KEY DE RETROACHIEVEMENTS"), apiKey,
                             storedApiKey.empty() ? "" : "********", "", true);
    apiKey->setSize(0.0f, apiKey->getFont()->getHeight());
    addSaveFunc([apiKey, this] {
        const std::string trimmedApiKey {Utils::String::trim(apiKey->getHiddenValue())};
        if (trimmedApiKey != Settings::getInstance()->getString("global.retroachievements.apikey")) {
            Settings::getInstance()->setString("global.retroachievements.apikey", trimmedApiKey);
            setNeedsSaving();
        }
    });

    auto sessionStatus = makeValueText("");
    auto apiKeyStatus = makeValueText("");
    auto updateSessionStatus = [sessionStatus, apiKeyStatus, username, password, apiKey] {
        const bool hasUser {!Utils::String::trim(username->getValue()).empty()};
        const bool hasApiKey {!Utils::String::trim(apiKey->getHiddenValue()).empty()};
        const bool hasPassword {!Utils::String::trim(password->getHiddenValue()).empty()};

        apiKeyStatus->setValue(hasApiKey ? _("CONFIGURADA") : _("FALTA"));

        if (hasUser && hasApiKey) {
            sessionStatus->setValue(_("API KEY CONFIGURADA"));
        }
        else if (RetroAchievements::hasSession()) {
            sessionStatus->setValue(_("SESION ACTIVA CON TOKEN (LEGACY)"));
        }
        else if (hasUser && hasPassword) {
            sessionStatus->setValue(_("CONTRASENA CONFIGURADA, FALTA INICIO DE SESION"));
        }
        else {
            sessionStatus->setValue(_("NO AUTENTICADO"));
        }
    };
    updateSessionStatus();
    addWithLabel(_("ESTADO DE SESION"), sessionStatus);
    addWithLabel(_("ESTADO DE API KEY"), apiKeyStatus);

    auto addBoolSetting = [this](const std::string& label, const std::string& key) {
        auto setting = std::make_shared<SwitchComponent>();
        setting->setState(Settings::getInstance()->getBool(key));
        addWithLabel(label, setting);
        addSaveFunc([setting, key, this] {
            if (setting->getState() != Settings::getInstance()->getBool(key)) {
                Settings::getInstance()->setBool(key, setting->getState());
                setNeedsSaving();
            }
        });
    };

    addBoolSetting(_("ENABLE RETROACHIEVEMENTS"), "global.retroachievements");

    ComponentListRow logoutRow;
    logoutRow.addElement(std::make_shared<TextComponent>(_("CLEAR STORED SESSION"),
                                                         Font::get(FONT_SIZE_MEDIUM),
                                                         mMenuColorPrimary),
                         true);
    logoutRow.addElement(getMenu().makeArrow(), false);
    logoutRow.makeAcceptInputHandler(
        [this, password, apiKey, sessionStatus, updateSessionStatus] {
            mWindow->pushGui(new GuiMsgBox(
                _("CLEAR THE SAVED RETROACHIEVEMENTS TOKEN, PASSWORD AND API KEY?"), _("YES"),
                [this, password, apiKey, sessionStatus, updateSessionStatus] {
                    RetroAchievements::clearSession();
                    password->setHiddenValue("");
                    password->setValue("");
                    apiKey->setHiddenValue("");
                    apiKey->setValue("");
                    Settings::getInstance()->setString("global.retroachievements.apikey", "");
                    updateSessionStatus();
                    setNeedsSaving();
                },
                _("NO")));
        });
    addRow(logoutRow);

    ComponentListRow clearCacheRow;
    clearCacheRow.addElement(std::make_shared<TextComponent>(
                                 _("CLEAR RETROACHIEVEMENTS CACHE"),
                                 Font::get(FONT_SIZE_MEDIUM), mMenuColorPrimary),
                             true);
    clearCacheRow.addElement(getMenu().makeArrow(), false);
    clearCacheRow.makeAcceptInputHandler([this] {
        if (RetroAchievements::clearCache())
            mWindow->pushGui(new GuiMsgBox(_("RetroAchievements cache cleared")));
        else
            mWindow->pushGui(new GuiMsgBox(_("Failed to clear RetroAchievements cache")));
    });
    addRow(clearCacheRow);

    addWithLabel(_("FUNCIONES EN VIVO EN PARTIDA"), makeValueText(_("NO DISPONIBLE")));

    ComponentListRow runtimeInfoRow;
    runtimeInfoRow.addElement(std::make_shared<TextComponent>(
                                  _("POR QUE LAS FUNCIONES EN VIVO ESTAN DESACTIVADAS"),
                                  Font::get(FONT_SIZE_MEDIUM), mMenuColorPrimary),
                              true);
    runtimeInfoRow.addElement(getMenu().makeArrow(), false);
    runtimeInfoRow.makeAcceptInputHandler([this] {
                mWindow->pushGui(new GuiMsgBox(
                        _("Este frontend solo usa datos de la API web de RetroAchievements "
                            "(perfil, progreso e insignias).\n\n"
                            "Las funciones en vivo durante la partida, como avisos de logros en tiempo real, "
                            "leaderboards en runtime, validaciones hardcore y challenge indicators, "
                            "requieren integracion con el emulador/core.")));
    });
    addRow(runtimeInfoRow);
    addBoolSetting(_("CHECK INDEXES AT STARTUP"), "CheevosCheckIndexesAtStart");
    addBoolSetting(_("SHOW MENU ENTRY"), "RetroachievementsMenuitem");
    addBoolSetting(_("SHOW ACHIEVEMENT ICONS"), "ShowCheevosIcon");

    ComponentListRow testRow;
    testRow.addElement(std::make_shared<TextComponent>(_("TEST ACCOUNT"),
                                                       Font::get(FONT_SIZE_MEDIUM),
                                                       mMenuColorPrimary),
                       true);
    testRow.addElement(getMenu().makeArrow(), false);
    testRow.makeAcceptInputHandler([this, username, password, updateSessionStatus] {
        const std::string trimmedUsername {Utils::String::trim(username->getValue())};
        const std::string trimmedPassword {Utils::String::trim(password->getHiddenValue())};

        std::string tokenOrError;
        if (RetroAchievements::testAccount(trimmedUsername, trimmedPassword, tokenOrError)) {
            Settings::getInstance()->setString("global.retroachievements.token", tokenOrError);
            Settings::getInstance()->setString("global.retroachievements.password", "");
            password->setHiddenValue("");
            password->setValue("");
            Settings::getInstance()->saveFile();
            updateSessionStatus();
            mWindow->pushGui(new GuiMsgBox(_(
                "Inicio de sesion RetroAchievements correcto. Configura la API key para cargar el perfil.")));
        }
        else {
            mWindow->pushGui(new GuiMsgBox(tokenOrError));
        }
    });
    addRow(testRow);

    ComponentListRow profileRow;
    profileRow.addElement(std::make_shared<TextComponent>(_("VIEW PROFILE"),
                                                          Font::get(FONT_SIZE_MEDIUM),
                                                          mMenuColorPrimary),
                          true);
    profileRow.addElement(getMenu().makeArrow(), false);
    profileRow.makeAcceptInputHandler([this, username] {
        if (username->getValue().empty()) {
            mWindow->pushGui(new GuiMsgBox(_("Configure a RetroAchievements username first")));
            return;
        }

        mWindow->pushGui(new GuiRetroAchievements(username->getValue()));
    });
    addRow(profileRow);

    ComponentListRow refreshProfileRow;
    refreshProfileRow.addElement(std::make_shared<TextComponent>(
                                     _("FORCE REFRESH PROFILE NOW"),
                                     Font::get(FONT_SIZE_MEDIUM), mMenuColorPrimary),
                                 true);
    refreshProfileRow.addElement(getMenu().makeArrow(), false);
    refreshProfileRow.makeAcceptInputHandler([this, username] {
        if (username->getValue().empty()) {
            mWindow->pushGui(new GuiMsgBox(_("Configure a RetroAchievements username first")));
            return;
        }

        mWindow->pushGui(new GuiRetroAchievements(username->getValue(), true));
    });
    addRow(refreshProfileRow);

    ComponentListRow indexRow;
    indexRow.addElement(std::make_shared<TextComponent>(_("RUN RETROACHIEVEMENTS INDEX NOW"),
                                                        Font::get(FONT_SIZE_MEDIUM),
                                                        mMenuColorPrimary),
                        true);
    indexRow.addElement(getMenu().makeArrow(), false);
    indexRow.makeAcceptInputHandler([this] {
        if (ThreadedCheevosIndexer::isRunning()) {
            mWindow->pushGui(new GuiMsgBox(_("RETROACHIEVEMENTS INDEXING IS RUNNING. STOP IT?"),
                                           _("YES"),
                                           [] { ThreadedCheevosIndexer::stop(); },
                                           _("NO"), nullptr));
            return;
        }

        ThreadedCheevosIndexer::start(true, nullptr, true);
    });
    addRow(indexRow);

    const std::string currentUsername {
        Settings::getInstance()->getString("global.retroachievements.username")};
    const std::string currentPasswordValue {
        Settings::getInstance()->getString("global.retroachievements.password")};
    const std::string currentApiKey {
        Settings::getInstance()->getString("global.retroachievements.apikey")};
    const std::string currentToken {
        Settings::getInstance()->getString("global.retroachievements.token")};

    addSaveFunc([this, username, password, apiKey, updateSessionStatus, currentUsername,
                 currentPasswordValue, currentApiKey, currentToken] {
        const bool enabled {Settings::getInstance()->getBool("global.retroachievements")};
        const std::string trimmedUsername {Utils::String::trim(username->getValue())};
        const std::string trimmedPassword {Utils::String::trim(password->getHiddenValue())};
        const std::string trimmedApiKey {Utils::String::trim(apiKey->getHiddenValue())};
        const bool usernameChanged {trimmedUsername != currentUsername};
        const bool passwordChanged {trimmedPassword != currentPasswordValue};
        const bool apiKeyChanged {trimmedApiKey != currentApiKey};

        if (!enabled) {
            RetroAchievements::clearSession(false);
            updateSessionStatus();
            return;
        }

        if (!trimmedApiKey.empty()) {
            updateSessionStatus();
            return;
        }

        if (!currentToken.empty() && !usernameChanged && !passwordChanged && !apiKeyChanged) {
            updateSessionStatus();
            return;
        }

        if (trimmedUsername.empty()) {
            RetroAchievements::clearSession(false);
            updateSessionStatus();
            return;
        }

        if (trimmedPassword.empty()) {
            if (RetroAchievements::hasSession() && !usernameChanged && !apiKeyChanged) {
                updateSessionStatus();
                return;
            }

            updateSessionStatus();
            return;
        }

        std::string tokenOrError;
        if (RetroAchievements::testAccount(trimmedUsername, trimmedPassword, tokenOrError)) {
            Settings::getInstance()->setString("global.retroachievements.token", tokenOrError);
            Settings::getInstance()->setString("global.retroachievements.password", "");
            password->setHiddenValue("");
            password->setValue("");
            updateSessionStatus();
        }
        else {
            RetroAchievements::clearSession(false);
            Settings::getInstance()->setBool("global.retroachievements", false);
            updateSessionStatus();
            mWindow->pushGui(new GuiMsgBox(tokenOrError));
        }
    });
}