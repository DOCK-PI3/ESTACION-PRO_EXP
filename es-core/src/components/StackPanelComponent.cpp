//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  StackPanelComponent.cpp
//

#include "components/StackPanelComponent.h"

#include "ThemeData.h"
#include "renderers/Renderer.h"

#include <cmath>

StackPanelComponent::StackPanelComponent()
    : mHorizontal {true}
    , mReverse {false}
    , mSeparator {0.0f}
    , mPadding {0.0f, 0.0f, 0.0f, 0.0f}
    , mClipChildren {false}
{
}

void StackPanelComponent::render(const glm::mat4& parentTrans)
{
    if (!isVisible())
        return;

    glm::mat4 trans {parentTrans * getTransform()};

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

void StackPanelComponent::applyTheme(const std::shared_ptr<ThemeData>& theme,
                                     const std::string& view,
                                     const std::string& element,
                                     unsigned int properties)
{
    GuiComponent::applyTheme(theme, view, element, properties);

    const ThemeData::ThemeElement* elem {theme->getElement(view, element, "stackpanel")};
    if (elem == nullptr)
        return;

    if (elem->has("orientation"))
        mHorizontal = (elem->get<std::string>("orientation") != "vertical");

    if (elem->has("reverse"))
        mReverse = elem->get<bool>("reverse");

    if (elem->has("clipChildren"))
        mClipChildren = elem->get<bool>("clipChildren");
    else
        mClipChildren = false;

    if (elem->has("separator")) {
        mSeparator = elem->get<float>("separator");
        if (mSeparator > 0.0f && mSeparator < 1.0f) {
            const glm::vec2 parentSize {
                getParent() != nullptr ?
                    getParent()->getSize() :
                    glm::vec2 {Renderer::getScreenWidth(), Renderer::getScreenHeight()}};
            mSeparator *= (mHorizontal ? parentSize.y : parentSize.x);
        }
    }

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

    performLayout();
}

void StackPanelComponent::onSizeChanged()
{
    performLayout();
}

void StackPanelComponent::update(int deltaTime)
{
    GuiComponent::update(deltaTime);
    performLayout();
}

void StackPanelComponent::performLayout()
{
    if (mChildren.empty())
        return;

    const float contentWidth {std::max(0.0f, mSize.x - mPadding.x - mPadding.z)};
    const float contentHeight {std::max(0.0f, mSize.y - mPadding.y - mPadding.w)};

    float cursor {mReverse ?
                      (mHorizontal ? mPadding.x + contentWidth : mPadding.y + contentHeight) :
                      (mHorizontal ? mPadding.x : mPadding.y)};

    for (GuiComponent* child : mChildren) {
        if (child == nullptr || !child->isVisible())
            continue;

        glm::vec2 childSize {child->getSize()};

        if (mHorizontal) {
            if (childSize.y != contentHeight)
                child->setSize(childSize.x, contentHeight);

            childSize = child->getSize();

            const float xPos {mReverse ?
                                  cursor - childSize.x + childSize.x * child->getOrigin().x :
                                  cursor + childSize.x * child->getOrigin().x};
            const float yPos {mPadding.y + childSize.y * child->getOrigin().y};
            child->setPosition(xPos, yPos);

            if (childSize.x > 0.0f)
                cursor += (mReverse ? -1.0f : 1.0f) * (childSize.x + mSeparator);
        }
        else {
            if (childSize.x != contentWidth)
                child->setSize(contentWidth, childSize.y);

            childSize = child->getSize();

            const float yPos {mReverse ?
                                  cursor - childSize.y + childSize.y * child->getOrigin().y :
                                  cursor + childSize.y * child->getOrigin().y};
            const float xPos {mPadding.x + childSize.x * child->getOrigin().x};
            child->setPosition(xPos, yPos);

            if (childSize.y > 0.0f)
                cursor += (mReverse ? -1.0f : 1.0f) * (childSize.y + mSeparator);
        }
    }
}
