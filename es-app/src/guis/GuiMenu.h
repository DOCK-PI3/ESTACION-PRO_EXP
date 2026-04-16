//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  GuiMenu.h
//
//  Main menu.
//  Some submenus are covered in separate source files.
//

#ifndef ES_APP_GUIS_GUI_MENU_H
#define ES_APP_GUIS_GUI_MENU_H

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"

#include <map>

class GuiMenu : public GuiComponent
{
public:
    GuiMenu();
    ~GuiMenu();

    bool input(InputConfig* config, Input input) override;
    void onSizeChanged() override;
    std::vector<HelpPrompt> getHelpPrompts() override;

private:
    void close(bool closeAllWindows);
    void addEntry(const std::string& name,
                  unsigned int color,
                  bool add_arrow,
                  const std::function<void()>& func,
                  const std::string& iconProperty = "");
    void addVersionInfo();

    void openScraperOptions();
    void openUIOptions();
    void openThemeDownloader(GuiSettings* settings);
    void openSystemStatusOptions();
    void openMediaViewerOptions();
    void openScreensaverOptions();
    void openRetroAchievementsOptions();
    void openSoundOptions();
    void openInputDeviceOptions();
    void openConfigInput(GuiSettings* settings);
    void openCollectionSystemOptions();
    void openOtherOptions();
    void openUtilities();
    void openQuitMenu();

    Renderer* mRenderer;
    MenuComponent mMenu;
    TextComponent mVersion;
    int mThemeDownloaderReloadCounter;
    std::map<std::string, std::string> mMenuIconPaths;
};

#endif // ES_APP_GUIS_GUI_MENU_H
