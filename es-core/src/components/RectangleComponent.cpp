//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  RectangleComponent.cpp
//

#include "components/RectangleComponent.h"

#include "ThemeData.h"
#include "renderers/Renderer.h"

RectangleComponent::RectangleComponent()
    : mBackgroundColor {0x00000000}
    , mBorderColor {0x00000000}
    , mBorderSize {0.0f}
    , mRoundCorners {0.0f}
{
}

void RectangleComponent::render(const glm::mat4& parentTrans)
{
    if (!isVisible() || mThemeOpacity == 0.0f)
        return;

    Renderer* renderer {Renderer::getInstance()};
    glm::mat4 trans {parentTrans * getTransform()};
    renderer->setMatrix(trans);

    if (mBorderSize > 0.0f && (mBorderColor & 0x000000FF) != 0) {
        renderer->drawRect(0.0f, 0.0f, mSize.x, mSize.y, mBorderColor, mBorderColor, false,
                           mOpacity, mDimming,
                           Renderer::BlendFactor::SRC_ALPHA,
                           Renderer::BlendFactor::ONE_MINUS_SRC_ALPHA,
                           mRoundCorners * renderer->getScreenResolutionModifier());
    }

    if ((mBackgroundColor & 0x000000FF) != 0) {
        const float inset {std::max(0.0f, mBorderSize)};
        const float innerWidth {std::max(0.0f, mSize.x - inset * 2.0f)};
        const float innerHeight {std::max(0.0f, mSize.y - inset * 2.0f)};
        const float innerRadius {std::max(0.0f, mRoundCorners - inset)};

        renderer->drawRect(inset, inset, innerWidth, innerHeight, mBackgroundColor,
                   mBackgroundColor, false, mOpacity, mDimming,
                   Renderer::BlendFactor::SRC_ALPHA,
                   Renderer::BlendFactor::ONE_MINUS_SRC_ALPHA,
                   innerRadius * renderer->getScreenResolutionModifier());
    }

    renderChildren(trans);
}

void RectangleComponent::applyTheme(const std::shared_ptr<ThemeData>& theme,
                                    const std::string& view,
                                    const std::string& element,
                                    unsigned int properties)
{
    GuiComponent::applyTheme(theme, view, element, properties);

    const ThemeData::ThemeElement* elem {theme->getElement(view, element, "rectangle")};
    if (elem == nullptr)
        return;

    if (elem->has("backgroundColor"))
        mBackgroundColor = elem->get<unsigned int>("backgroundColor");
    else if (elem->has("color"))
        mBackgroundColor = elem->get<unsigned int>("color");

    if (elem->has("borderColor"))
        mBorderColor = elem->get<unsigned int>("borderColor");

    if (elem->has("borderSize"))
        mBorderSize = elem->get<float>("borderSize");

    if (elem->has("roundCorners"))
        mRoundCorners = elem->get<float>("roundCorners");
}
