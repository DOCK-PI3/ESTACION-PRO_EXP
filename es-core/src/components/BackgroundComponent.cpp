//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  BackgroundComponent.cpp
//
//  Displays a background frame with rounded corners.
//  Used by menus, popups etc.
//

#include "components/BackgroundComponent.h"

BackgroundComponent::BackgroundComponent(const glm::vec2 cornerSize)
    : mRenderer {Renderer::getInstance()}
    , mCornerSize {cornerSize}
    , mFrameColor {mMenuColorFrame}
    , mFrameImage {mMenuImagePathBackground}
    , mHasFrameImage {!mMenuImagePathBackground.empty()}
{
    if (mHasFrameImage) {
        mFrameImage.setOrigin(0.0f, 0.0f);
        mFrameImage.setCornerSize(mMenuBackgroundCornerSize);
        mFrameImage.setFrameColor(mFrameColor);
    }
}

void BackgroundComponent::fitTo(glm::vec2 size, glm::vec3 position, glm::vec2 padding)
{
    if (padding != glm::vec2 {0.0f, 0.0f}) {
        size += padding;
        position.x -= padding.x / 2.0f;
        position.y -= padding.y / 2.0f;
    }

    setSize(size);
    setPosition(position);

    if (mHasFrameImage) {
        mFrameImage.setPosition(0.0f, 0.0f, 0.0f);
        mFrameImage.setSize(mSize);
    }
}

void BackgroundComponent::setFrameColor(unsigned int frameColor)
{
    mFrameColor = frameColor;

    if (mHasFrameImage)
        mFrameImage.setFrameColor(frameColor);
}

void BackgroundComponent::render(const glm::mat4& parentTrans)
{
    if (!isVisible())
        return;

    glm::mat4 trans {parentTrans * getTransform()};

    if (mHasFrameImage) {
        mFrameImage.setOpacity(mOpacity);
        mFrameImage.render(trans);
        return;
    }

    mRenderer->setMatrix(trans);
    mRenderer->drawRect(0.0f, 0.0f, mSize.x, mSize.y, mFrameColor, mFrameColor, false, mOpacity,
                        1.0f, Renderer::BlendFactor::SRC_ALPHA,
                        Renderer::BlendFactor::ONE_MINUS_SRC_ALPHA,
                        mCornerSize.x * mRenderer->getScreenResolutionModifier());
}
