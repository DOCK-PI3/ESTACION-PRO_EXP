//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ControllerActivityComponent.h
//
//  Batocera-compatible controller activity indicator.
//

#ifndef ES_CORE_COMPONENTS_CONTROLLER_ACTIVITY_COMPONENT_H
#define ES_CORE_COMPONENTS_CONTROLLER_ACTIVITY_COMPONENT_H

#include "GuiComponent.h"
#include "resources/TextureResource.h"

#include <SDL2/SDL_joystick.h>

#include <map>

class Renderer;

class ControllerActivityComponent : public GuiComponent
{
public:
    ControllerActivityComponent();

    bool input(InputConfig* config, Input input) override;
    void update(int deltaTime) override;
    void render(const glm::mat4& parentTrans) override;

    void applyTheme(const std::shared_ptr<ThemeData>& theme,
                    const std::string& view,
                    const std::string& element,
                    unsigned int properties) override;

private:
    struct PadState {
        int timeoutMs {0};
        bool hotkey {false};
        int batteryLevel {-1};
    };

    static constexpr int sActivityTimeoutMs {150};

    void refreshControllers();
    float renderTexture(float x, float width, const std::shared_ptr<TextureResource>& texture,
                        unsigned int color) const;

    Renderer* mRenderer;
    std::map<SDL_JoystickID, PadState> mPads;

    std::shared_ptr<TextureResource> mPadTexture;

    float mSpacing;
    Alignment mHorizontalAlignment;
    unsigned int mColorShift;
    unsigned int mActivityColor;
    unsigned int mHotkeyColor;
};

#endif // ES_CORE_COMPONENTS_CONTROLLER_ACTIVITY_COMPONENT_H
