//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ControllerActivityComponent.cpp
//
//  Batocera-compatible controller activity indicator.
//

#include "components/ControllerActivityComponent.h"

#include "InputManager.h"
#include "ThemeData.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "utils/StringUtil.h"

#include <algorithm>

ControllerActivityComponent::ControllerActivityComponent()
    : mRenderer {Renderer::getInstance()}
    , mSpacing {std::round(0.005f * mRenderer->getScreenWidth())}
    , mHorizontalAlignment {ALIGN_LEFT}
    , mColorShift {0xFFFFFF99}
    , mActivityColor {0xFF3675CA}
    , mHotkeyColor {0x0000FF66}
{
    mSize = glm::vec2 {mRenderer->getScreenHeight() * 0.18f, mRenderer->getScreenHeight() * 0.03f};
    mPosition = glm::vec3 {mRenderer->getScreenWidth() * 0.012f,
                           mRenderer->getScreenHeight() * 0.952f, 0.0f};
}

bool ControllerActivityComponent::input(InputConfig* config, Input input)
{
    if (config == nullptr || config->getDeviceId() < 0)
        return false;

    if (input.type != TYPE_BUTTON && input.type != TYPE_AXIS)
        return false;

    const SDL_JoystickID deviceId {config->getDeviceId()};
    auto it {mPads.find(deviceId)};
    if (it == mPads.end())
        return false;

    it->second.timeoutMs = sActivityTimeoutMs;
    it->second.hotkey = config->isMappedTo("hotkey", input);
    return false;
}

void ControllerActivityComponent::update(int deltaTime)
{
    GuiComponent::update(deltaTime);

    refreshControllers();

    for (auto& pad : mPads) {
        if (pad.second.timeoutMs <= 0)
            continue;

        pad.second.timeoutMs -= deltaTime;
        if (pad.second.timeoutMs <= 0) {
            pad.second.timeoutMs = 0;
            pad.second.hotkey = false;
        }
    }
}

void ControllerActivityComponent::render(const glm::mat4& parentTrans)
{
    if (!isVisible())
        return;

    if (mPads.empty())
        return;

    const glm::mat4 trans {parentTrans * getTransform()};
    mRenderer->setMatrix(trans);

    const float iconSize {mSize.y};
    if (iconSize <= 0.0f)
        return;

    const int count {static_cast<int>(mPads.size())};
    const float totalWidth {
        (count * iconSize) + ((count > 0 ? count - 1 : 0) * mSpacing)};

    float x {0.0f};
    if (mHorizontalAlignment == ALIGN_CENTER)
        x = std::round((mSize.x - totalWidth) * 0.5f);
    else if (mHorizontalAlignment == ALIGN_RIGHT)
        x = std::round(mSize.x - totalWidth);

    for (const auto& pad : mPads) {
        unsigned int color {mColorShift};
        if (pad.second.timeoutMs > 0)
            color = pad.second.hotkey ? mHotkeyColor : mActivityColor;

        if (mPadTexture != nullptr)
            x += renderTexture(x, iconSize, mPadTexture, color);
        else {
            mRenderer->drawRect(x, 0.0f, iconSize, iconSize, color, color, false,
                                mOpacity * mThemeOpacity, mDimming,
                                Renderer::BlendFactor::SRC_ALPHA,
                                Renderer::BlendFactor::ONE_MINUS_SRC_ALPHA,
                                std::round(iconSize * 0.25f));
            x += iconSize + mSpacing;
        }
    }

    renderChildren(trans);
}

void ControllerActivityComponent::applyTheme(const std::shared_ptr<ThemeData>& theme,
                                             const std::string& view,
                                             const std::string& element,
                                             unsigned int properties)
{
    GuiComponent::applyTheme(theme, view, element, properties);

    const ThemeData::ThemeElement* elem {theme->getElement(view, element, "controllerActivity")};
    if (elem == nullptr)
        return;

    using namespace ThemeFlags;

    if (properties & ThemeFlags::PATH) {
        std::vector<std::string> candidates;
        if (elem->has("imagePath"))
            candidates.emplace_back(elem->get<std::string>("imagePath"));
        if (elem->has("gunPath"))
            candidates.emplace_back(elem->get<std::string>("gunPath"));
        if (elem->has("wheelPath"))
            candidates.emplace_back(elem->get<std::string>("wheelPath"));

        for (const auto& candidate : candidates) {
            if (candidate.empty())
                continue;
            if (!ResourceManager::getInstance().fileExists(candidate))
                continue;

            mPadTexture = TextureResource::get(candidate, false, true, true, true);
            break;
        }
    }

    if (properties & ThemeFlags::COLOR) {
        if (elem->has("color"))
            mColorShift = elem->get<unsigned int>("color");
        if (elem->has("activityColor"))
            mActivityColor = elem->get<unsigned int>("activityColor");
        if (elem->has("hotkeyColor"))
            mHotkeyColor = elem->get<unsigned int>("hotkeyColor");
        if (elem->has("itemSpacing")) {
            mSpacing = std::round(glm::clamp(elem->get<float>("itemSpacing"), 0.0f, 0.04f) *
                                  mRenderer->getScreenWidth());
        }
    }

    if ((properties & ThemeFlags::ALIGNMENT) && elem->has("horizontalAlignment")) {
        const std::string alignment {Utils::String::toLower(
            Utils::String::trim(elem->get<std::string>("horizontalAlignment")))};

        if (alignment == "right")
            mHorizontalAlignment = ALIGN_RIGHT;
        else if (alignment == "center")
            mHorizontalAlignment = ALIGN_CENTER;
        else
            mHorizontalAlignment = ALIGN_LEFT;
    }

    refreshControllers();

    if (mPadTexture != nullptr)
        mPadTexture->rasterizeAt(mSize.y, mSize.y);
}

void ControllerActivityComponent::refreshControllers()
{
    const auto ids {InputManager::getInstance().getConnectedControllerIDs()};

    for (const auto id : ids) {
        auto& pad {mPads[id]};
        const SDL_JoystickPowerLevel power {
            InputManager::getInstance().getControllerPowerLevel(id)};
        pad.batteryLevel = (power == SDL_JOYSTICK_POWER_UNKNOWN || power == SDL_JOYSTICK_POWER_WIRED) ?
                               -1 :
                               100;
    }

    for (auto it = mPads.begin(); it != mPads.end();) {
        if (std::find(ids.cbegin(), ids.cend(), it->first) == ids.cend())
            it = mPads.erase(it);
        else
            ++it;
    }
}

float ControllerActivityComponent::renderTexture(float x,
                                                 float width,
                                                 const std::shared_ptr<TextureResource>& texture,
                                                 unsigned int color) const
{
    if (texture == nullptr || !texture->bind(0))
        return 0.0f;

    const glm::vec2 source {texture->getSourceImageSize()};
    if (source.x <= 0.0f || source.y <= 0.0f)
        return 0.0f;

    float drawW {width};
    float drawH {width};

    if (source.x > source.y)
        drawH = std::round((source.y / source.x) * width);
    else
        drawW = std::round((source.x / source.y) * width);

    const float left {std::round(x + (width - drawW) * 0.5f)};
    const float top {std::round((mSize.y - drawH) * 0.5f)};

    Renderer::Vertex vertices[4];
    vertices[0] = Renderer::Vertex {{left, top}, {0.0f, 0.0f}, color};
    vertices[1] = Renderer::Vertex {{left, top + drawH}, {0.0f, 1.0f}, color};
    vertices[2] = Renderer::Vertex {{left + drawW, top}, {1.0f, 0.0f}, color};
    vertices[3] = Renderer::Vertex {{left + drawW, top + drawH}, {1.0f, 1.0f}, color};
    vertices[0].opacity = mOpacity * mThemeOpacity;
    vertices[0].dimming = mDimming;

    mRenderer->drawTriangleStrips(&vertices[0], 4);
    mRenderer->bindTexture(0, 0);

    return width + mSpacing;
}
