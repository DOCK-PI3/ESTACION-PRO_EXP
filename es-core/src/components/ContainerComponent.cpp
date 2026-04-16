//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ContainerComponent.cpp
//

#include "components/ContainerComponent.h"

#include "ThemeData.h"
#include "renderers/Renderer.h"

#include <cmath>

ContainerComponent::ContainerComponent()
    : mPadding {0.0f, 0.0f, 0.0f, 0.0f}
    , mClipChildren {false}
{
}

void ContainerComponent::render(const glm::mat4& parentTrans)
{
    if (!isVisible() || mThemeOpacity == 0.0f)
        return;

    glm::mat4 trans {parentTrans * getTransform()};
    if (mPadding.x != 0.0f || mPadding.y != 0.0f)
        trans = glm::translate(trans, glm::vec3 {mPadding.x, mPadding.y, 0.0f});

    Renderer::getInstance()->setMatrix(trans);

    if (!mClipChildren) {
        renderChildren(trans);
        return;
    }

    const float clipLeft {mPadding.x};
    const float clipTop {mPadding.y};
    const float clipRight {std::max(clipLeft, mSize.x - mPadding.z)};
    const float clipBottom {std::max(clipTop, mSize.y - mPadding.w)};

    for (unsigned int i {0}; i < getChildCount(); ++i) {
        GuiComponent* child {getChild(i)};
        if (child == nullptr || !child->isVisible())
            continue;

        const glm::vec2 childSize {child->getSize()};
        const glm::vec3 childPos {child->getPosition()};
        const glm::vec2 childOrigin {child->getOrigin()};

        const float childLeft {childPos.x - childOrigin.x * childSize.x};
        const float childTop {childPos.y - childOrigin.y * childSize.y};
        const float childRight {childLeft + childSize.x};
        const float childBottom {childTop + childSize.y};

        const bool intersects = !(childRight <= clipLeft || childLeft >= clipRight ||
                                  childBottom <= clipTop || childTop >= clipBottom);

        if (intersects)
            child->render(trans);
    }
}

void ContainerComponent::applyTheme(const std::shared_ptr<ThemeData>& theme,
                                    const std::string& view,
                                    const std::string& element,
                                    unsigned int properties)
{
    GuiComponent::applyTheme(theme, view, element, properties);

    // Ensure the element exists with expected type, while all generic transforms are
    // still handled by GuiComponent::applyTheme.
    const ThemeData::ThemeElement* elem {theme->getElement(view, element, "container")};
    if (elem == nullptr)
        return;

    if (elem->has("clipChildren"))
        mClipChildren = elem->get<bool>("clipChildren");
    else
        mClipChildren = false;

    if (elem->has("padding")) {
        const glm::vec2 parentSize {
            getParent() != nullptr ?
                getParent()->getSize() :
                glm::vec2 {Renderer::getScreenWidth(), Renderer::getScreenHeight()}};

        const glm::vec4 rawPadding {elem->get<glm::vec4>("padding")};

        const auto toPixelsX = [&](float value) {
            return std::abs(value) <= 1.0f ? value * parentSize.x : value;
        };

        const auto toPixelsY = [&](float value) {
            return std::abs(value) <= 1.0f ? value * parentSize.y : value;
        };

        mPadding = glm::vec4 {toPixelsX(rawPadding.x),
                              toPixelsY(rawPadding.y),
                              toPixelsX(rawPadding.z),
                              toPixelsY(rawPadding.w)};
    }
    else {
        mPadding = glm::vec4 {0.0f, 0.0f, 0.0f, 0.0f};
    }
}
