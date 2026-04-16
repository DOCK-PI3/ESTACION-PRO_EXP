//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  GuiComponent.cpp
//
//  Basic GUI component handling such as placement, rotation, Z-order, rendering and animation.
//

#include "GuiComponent.h"

#include "Log.h"
#include "Sound.h"
#include "ThemeData.h"
#include "Window.h"
#include "anim/StoryboardAnimator.h"
#include "animations/Animation.h"
#include "renderers/Renderer.h"

#include <algorithm>

GuiComponent::GuiComponent()
    : mWindow {Window::getInstance()}
    , mParent {nullptr}
    , mThemeGameSelectorEntry {0}
    , mColor {0}
    , mColorShift {0}
    , mColorShiftEnd {0}
    , mColorOriginalValue {0}
    , mColorChangedValue {0}
    , mComponentThemeFlags {0}
    , mPosition {0.0f, 0.0f, 0.0f}
    , mScreenOffset {0.0f, 0.0f}
    , mOrigin {0.0f, 0.0f}
    , mRotationOrigin {0.5f, 0.5f}
    , mScaleOrigin {0.5f, 0.5f}
    , mSize {0.0f, 0.0f}
    , mMinSize {0.0f, 0.0f}
    , mMaxSize {0.0f, 0.0f}
    , mStationary {Stationary::NEVER}
    , mComponentScope {ComponentScope::SHARED}
    , mRenderDuringTransitions {true}
    , mBrightness {0.0f}
    , mOpacity {1.0f}
    , mSaturation {1.0f}
    , mDimming {1.0f}
    , mThemeOpacity {1.0f}
    , mThemeSaturation {1.0f}
    , mRotation {0.0f}
    , mScale {1.0f}
    , mDefaultZIndex {0.0f}
    , mZIndex {0.0f}
    , mIsProcessing {false}
    , mVisible {true}
    , mEnabled {true}
    , mTransform {Renderer::getIdentity()}
    , mStoryboardAnimator {nullptr}
{
    for (unsigned char i {0}; i < MAX_ANIMATIONS; ++i)
        mAnimationMap[i] = nullptr;
}

GuiComponent::~GuiComponent()
{
    mWindow->removeGui(this);

    cancelAllAnimations();

    if (mStoryboardAnimator != nullptr) {
        delete mStoryboardAnimator;
        mStoryboardAnimator = nullptr;
    }

    if (mParent)
        mParent->removeChild(this);

    for (unsigned int i {0}; i < getChildCount(); ++i)
        getChild(i)->setParent(nullptr);
}

void GuiComponent::textInput(const std::string& text, const bool pasting)
{
    for (auto it = mChildren.cbegin(); it != mChildren.cend(); ++it)
        (*it)->textInput(text, pasting);
}

bool GuiComponent::input(InputConfig* config, Input input)
{
    for (unsigned int i {0}; i < getChildCount(); ++i) {
        if (getChild(i)->input(config, input))
            return true;
    }

    return false;
}

void GuiComponent::update(int deltaTime)
{
    updateSelf(deltaTime);
    updateChildren(deltaTime);
}

void GuiComponent::render(const glm::mat4& parentTrans)
{
    if (!isVisible())
        return;

    glm::mat4 trans {parentTrans * getTransform()};
    renderChildren(trans);
}

void GuiComponent::setPosition(float x, float y, float z)
{
    if (mPosition.x == x && mPosition.y == y && mPosition.z == z)
        return;

    mPosition = glm::vec3 {x, y, z};
    onPositionChanged();
}

void GuiComponent::setOrigin(float x, float y)
{
    if (mOrigin.x == x && mOrigin.y == y)
        return;

    mOrigin = glm::vec2 {x, y};
    onOriginChanged();
}

void GuiComponent::setSize(const float w, const float h)
{
    assert(w >= 0.0f && h >= 0.0f);

    glm::vec2 clampedSize {w, h};

    clampedSize.x = std::max(clampedSize.x, mMinSize.x);
    clampedSize.y = std::max(clampedSize.y, mMinSize.y);

    if (mMaxSize.x > 0.0f)
        clampedSize.x = std::min(clampedSize.x, mMaxSize.x);
    if (mMaxSize.y > 0.0f)
        clampedSize.y = std::min(clampedSize.y, mMaxSize.y);

    if (mSize.x == clampedSize.x && mSize.y == clampedSize.y)
        return;

    mSize = clampedSize;
    onSizeChanged();
}

const glm::vec2 GuiComponent::getCenter() const
{
    return glm::vec2 {mPosition.x - (getSize().x * mOrigin.x) + getSize().x / 2.0f,
                      mPosition.y - (getSize().y * mOrigin.y) + getSize().y / 2.0f};
}

void GuiComponent::addChild(GuiComponent* cmp)
{
    mChildren.push_back(cmp);

    if (cmp->getParent())
        cmp->getParent()->removeChild(cmp);

    cmp->setParent(this);
}

void GuiComponent::removeChild(GuiComponent* cmp)
{
    if (!cmp->getParent())
        return;

    if (cmp->getParent() != this) {
        LOG(LogError) << "Tried to remove child from incorrect parent!";
    }

    cmp->setParent(nullptr);

    for (auto i {mChildren.cbegin()}; i != mChildren.cend(); ++i) {
        if (*i == cmp) {
            mChildren.erase(i);
            return;
        }
    }
}

void GuiComponent::sortChildren()
{
    std::stable_sort(mChildren.begin(), mChildren.end(), [](GuiComponent* a, GuiComponent* b) {
        return b->getZIndex() > a->getZIndex();
    });
}

const int GuiComponent::getChildIndex() const
{
    std::vector<GuiComponent*>::iterator it {
        std::find(getParent()->mChildren.begin(), getParent()->mChildren.end(), this)};

    if (it != getParent()->mChildren.end())
        return static_cast<int>(std::distance(getParent()->mChildren.begin(), it));
    else
        return -1;
}

void GuiComponent::setAnimation(Animation* anim,
                                int delay,
                                std::function<void()> finishedCallback,
                                bool reverse,
                                unsigned char slot)
{
    assert(slot < MAX_ANIMATIONS);

    AnimationController* oldAnim {mAnimationMap[slot]};
    mAnimationMap[slot] = new AnimationController(anim, delay, finishedCallback, reverse);

    if (oldAnim)
        delete oldAnim;
}

const bool GuiComponent::stopAnimation(unsigned char slot)
{
    assert(slot < MAX_ANIMATIONS);
    if (mAnimationMap[slot]) {
        delete mAnimationMap[slot];
        mAnimationMap[slot] = nullptr;
        return true;
    }
    else {
        return false;
    }
}

const bool GuiComponent::cancelAnimation(unsigned char slot)
{
    assert(slot < MAX_ANIMATIONS);
    if (mAnimationMap[slot]) {
        mAnimationMap[slot]->removeFinishedCallback();
        delete mAnimationMap[slot];
        mAnimationMap[slot] = nullptr;
        return true;
    }
    else {
        return false;
    }
}

const bool GuiComponent::finishAnimation(unsigned char slot)
{
    assert(slot < MAX_ANIMATIONS);
    AnimationController* anim {mAnimationMap[slot]};
    if (anim) {
        // Skip to animation's end.
        const bool done {anim->update(anim->getAnimation()->getDuration() - anim->getTime())};
        if (done) {
            mAnimationMap[slot] = nullptr;
            delete anim; // Will also call finishedCallback.
        }
        return true;
    }
    else {
        return false;
    }
}

const bool GuiComponent::advanceAnimation(unsigned char slot, unsigned int time)
{
    assert(slot < MAX_ANIMATIONS);
    AnimationController* anim {mAnimationMap[slot]};
    if (anim) {
        bool done {anim->update(time)};
        if (done) {
            mAnimationMap[slot] = nullptr;
            delete anim; // Will also call finishedCallback.
        }
        return true;
    }
    else {
        return false;
    }
}

void GuiComponent::stopAllAnimations()
{
    for (unsigned char i {0}; i < MAX_ANIMATIONS; ++i)
        stopAnimation(i);
}

void GuiComponent::cancelAllAnimations()
{
    for (unsigned char i {0}; i < MAX_ANIMATIONS; ++i)
        cancelAnimation(i);
}

void GuiComponent::setBrightness(float brightness)
{
    if (mBrightness == brightness)
        return;

    mBrightness = brightness;
    for (auto it = mChildren.cbegin(); it != mChildren.cend(); ++it)
        (*it)->setBrightness(brightness);
}

void GuiComponent::setOpacity(float opacity)
{
    if (mOpacity == opacity)
        return;

    mOpacity = opacity;
    for (auto it = mChildren.cbegin(); it != mChildren.cend(); ++it)
        (*it)->setOpacity(opacity);
}

void GuiComponent::setDimming(float dimming)
{
    if (mDimming == dimming)
        return;

    mDimming = dimming;
    for (auto it = mChildren.cbegin(); it != mChildren.cend(); ++it)
        (*it)->setDimming(dimming);
}

const glm::mat4& GuiComponent::getTransform()
{
    mTransform = Renderer::getIdentity();
    mTransform = glm::translate(
        mTransform, glm::round(glm::vec3 {mPosition.x + mScreenOffset.x, mPosition.y + mScreenOffset.y, mPosition.z}));

    if (mScale != 1.0f) {
        // Calculate offset as difference between origin and scale origin.
        glm::vec2 scaleSize {getRotationSize()};
        float xOff {(mOrigin.x - mScaleOrigin.x) * scaleSize.x};
        float yOff {(mOrigin.y - mScaleOrigin.y) * scaleSize.y};

        // Transform to offset point.
        if (xOff != 0.0f || yOff != 0.0f)
            mTransform = glm::translate(mTransform,
                                        glm::vec3 {xOff * -1.0f, yOff * -1.0f, 0.0f});

        mTransform = glm::scale(mTransform, glm::vec3 {mScale});

        // Transform back to original point.
        if (xOff != 0.0f || yOff != 0.0f)
            mTransform = glm::translate(mTransform, glm::vec3 {xOff, yOff, 0.0f});
    }

    if (mRotation != 0.0f) {
        // Calculate offset as difference between origin and rotation origin.
        glm::vec2 rotationSize {getRotationSize()};
        float xOff {(mOrigin.x - mRotationOrigin.x) * rotationSize.x};
        float yOff {(mOrigin.y - mRotationOrigin.y) * rotationSize.y};

        // Transform to offset point.
        if (xOff != 0.0f || yOff != 0.0f)
            mTransform = glm::translate(mTransform, glm::vec3 {xOff * -1.0f, yOff * -1.0f, 0.0f});

        // Apply rotation transform.
        mTransform = glm::rotate(mTransform, mRotation, glm::vec3 {0.0f, 0.0f, 1.0f});

        // Transform back to original point.
        if (xOff != 0.0f || yOff != 0.0f)
            mTransform = glm::translate(mTransform, glm::vec3 {xOff, yOff, 0.0f});
    }

    mTransform = glm::translate(
        mTransform,
        glm::round(glm::vec3 {mOrigin.x * mSize.x * -1.0f, mOrigin.y * mSize.y * -1.0f, 0.0f}));

    return mTransform;
}

void GuiComponent::onShow()
{
    if (mStoryboardAnimator != nullptr)
        mStoryboardAnimator->reset();

    for (unsigned int i {0}; i < getChildCount(); ++i)
        getChild(i)->onShow();
}

void GuiComponent::onHide()
{
    if (mStoryboardAnimator != nullptr)
        mStoryboardAnimator->pause();

    for (unsigned int i {0}; i < getChildCount(); ++i)
        getChild(i)->onHide();
}

void GuiComponent::applyTheme(const std::shared_ptr<ThemeData>& theme,
                              const std::string& view,
                              const std::string& element,
                              unsigned int properties)
{
    const glm::vec2 scale {getParent() ?
                               getParent()->getSize() :
                               glm::vec2 {Renderer::getScreenWidth(), Renderer::getScreenHeight()}};

    const ThemeData::ThemeElement* elem {theme->getElement(view, element, "")};
    if (!elem)
        return;

    using namespace ThemeFlags;

    if (properties) {
        if (elem->has("minSize"))
            setMinSize(elem->get<glm::vec2>("minSize") * scale);

        if (elem->has("maxSize"))
            setMaxSize(elem->get<glm::vec2>("maxSize") * scale);
    }

    if (properties & POSITION && elem->has("pos")) {
        glm::vec2 denormalized {elem->get<glm::vec2>("pos") * scale};
        setPosition(glm::vec3 {denormalized.x, denormalized.y, 0.0f});
    }

    // Batocera/Retrobat control overrides frequently target x/y independently.
    if (properties & POSITION) {
        if (elem->has("x"))
            setPosition(elem->get<float>("x") * scale.x, mPosition.y);
        if (elem->has("y"))
            setPosition(mPosition.x, elem->get<float>("y") * scale.y);
    }

    if (properties & ThemeFlags::SIZE && elem->has("size")) {
        glm::vec2 size {(elem->get<glm::vec2>("size") * scale)};
        if (size.x == 0.0f && size.y == 0.0f)
            setAutoCalcExtent(glm::ivec2 {1, 0});
        else if (size.x != 0.0f && size.y == 0.0f)
            setAutoCalcExtent(glm::ivec2 {0, 1});
        else if (size.x != 0.0f && size.y != 0.0f)
            setAutoCalcExtent(glm::ivec2 {0, 0});
        setSize(size);
    }

    // Batocera/Retrobat control overrides frequently target w/h independently.
    if (properties & ThemeFlags::SIZE) {
        if (elem->has("w"))
            setSize(elem->get<float>("w") * scale.x, mSize.y);
        if (elem->has("h"))
            setSize(mSize.x, elem->get<float>("h") * scale.y);
    }

    // Position + size also implies origin.
    if ((properties & ORIGIN || (properties & POSITION && properties & ThemeFlags::SIZE)) &&
        elem->has("origin")) {
        setOrigin(glm::clamp(elem->get<glm::vec2>("origin"), 0.0f, 1.0f));
    }

    if (properties & ThemeFlags::ROTATION) {
        if (elem->has("rotation"))
            setRotationDegrees(elem->get<float>("rotation"));
        if (elem->has("rotationOrigin"))
            setRotationOrigin(glm::clamp(elem->get<glm::vec2>("rotationOrigin"), 0.0f, 1.0f));
    }

    if (properties && elem->has("scale"))
        setScale(elem->get<float>("scale"));

    if (properties && elem->has("scaleOrigin"))
        setScaleOrigin(glm::clamp(elem->get<glm::vec2>("scaleOrigin"), 0.0f, 1.0f));

    if (properties) {
        glm::vec2 offset {mScreenOffset};

        if (elem->has("offset")) {
            const glm::vec2 normalizedOffset {elem->get<glm::vec2>("offset")};
            offset = glm::vec2 {normalizedOffset.x * scale.x, normalizedOffset.y * scale.y};
        }

        if (elem->has("offsetX"))
            offset.x = elem->get<float>("offsetX") * scale.x;

        if (elem->has("offsetY"))
            offset.y = elem->get<float>("offsetY") * scale.y;

        setScreenOffset(offset);
    }

    if (properties & ThemeFlags::Z_INDEX && elem->has("zIndex"))
        setZIndex(elem->get<float>("zIndex"));
    else
        setZIndex(getDefaultZIndex());

    if (properties & ThemeFlags::BRIGHTNESS && elem->has("brightness"))
        mBrightness = glm::clamp(elem->get<float>("brightness"), -2.0f, 2.0f);

    if (properties & ThemeFlags::OPACITY && elem->has("opacity"))
        mThemeOpacity = glm::clamp(elem->get<float>("opacity"), 0.0f, 1.0f);

    if (properties & ThemeFlags::VISIBLE && elem->has("visible") && !elem->get<bool>("visible"))
        mThemeOpacity = 0.0f;
    else
        setVisible(true);

    if (properties & ThemeFlags::SATURATION && elem->has("saturation"))
        mThemeSaturation = glm::clamp(elem->get<float>("saturation"), 0.0f, 1.0f);

    if (properties && elem->has("gameselector"))
        mThemeGameSelector = elem->get<std::string>("gameselector");

    if (properties && elem->has("gameselectorEntry"))
        mThemeGameSelectorEntry = elem->get<unsigned int>("gameselectorEntry");

    mStoryBoards = elem->mStoryBoards;

    if (!mStoryBoards.empty())
        selectStoryboard();
    else
        deselectStoryboard();
}

void GuiComponent::updateHelpPrompts()
{
    if (getParent()) {
        getParent()->updateHelpPrompts();
        return;
    }

    std::vector<HelpPrompt> prompts {getHelpPrompts()};

    if (mWindow->peekGui() == this)
        mWindow->setHelpPrompts(prompts);
}

void GuiComponent::updateSelf(int deltaTime)
{
    for (unsigned char i {0}; i < MAX_ANIMATIONS; ++i)
        advanceAnimation(i, deltaTime);

    if (mStoryboardAnimator != nullptr)
        mStoryboardAnimator->update(deltaTime);
}

void GuiComponent::updateChildren(int deltaTime)
{
    for (unsigned int i {0}; i < getChildCount(); ++i)
        getChild(i)->update(deltaTime);
}

void GuiComponent::renderChildren(const glm::mat4& transform) const
{
    for (unsigned int i {0}; i < getChildCount(); ++i)
        getChild(i)->render(transform);
}

bool GuiComponent::hasStoryBoard(const std::string& name, bool compareEmptyName)
{
    if (!name.empty() || compareEmptyName)
        return mStoryboardAnimator != nullptr && mStoryboardAnimator->getName() == name;

    return mStoryboardAnimator != nullptr;
}

bool GuiComponent::currentStoryBoardHasProperty(const std::string& propertyName)
{
    if (mStoryboardAnimator == nullptr)
        return false;

    return storyBoardExists(mStoryboardAnimator->getName(), propertyName);
}

bool GuiComponent::isStoryBoardRunning(const std::string& name)
{
    if (!name.empty()) {
        return mStoryboardAnimator != nullptr && mStoryboardAnimator->getName() == name &&
               mStoryboardAnimator->isRunning();
    }

    return mStoryboardAnimator != nullptr && mStoryboardAnimator->isRunning();
}

bool GuiComponent::storyBoardExists(const std::string& name, const std::string& propertyName)
{
    if (!mStoryBoards.empty()) {
        auto it = mStoryBoards.find(name);
        if (it == mStoryBoards.cend())
            return false;

        if (propertyName.empty())
            return true;

        for (auto animation : it->second->animations) {
            if (animation->propertyName == propertyName)
                return true;
        }
    }

    return false;
}

bool GuiComponent::selectStoryboard(const std::string& name)
{
    if (mStoryboardAnimator != nullptr && mStoryboardAnimator->getName() == name)
        return true;

    auto sb = mStoryBoards.find(name);
    if (sb == mStoryBoards.cend()) {
        if (name.empty() && !mStoryBoards.empty()) {
            // Prioridad de selección por defecto para compatibilidad con themes
            // que usan eventos en storyboards.
            if (mStoryBoards.find("") != mStoryBoards.cend())
                sb = mStoryBoards.find("");
            else if (mStoryBoards.find("activate") != mStoryBoards.cend())
                sb = mStoryBoards.find("activate");
            else if (mStoryBoards.find("open") != mStoryBoards.cend())
                sb = mStoryBoards.find("open");
            else if (mStoryBoards.find("show") != mStoryBoards.cend())
                sb = mStoryBoards.find("show");
            else
                sb = mStoryBoards.begin();
        }
        else {
            return false;
        }
    }

    if (mStoryboardAnimator != nullptr) {
        mStoryboardAnimator->reset();
        delete mStoryboardAnimator;
        mStoryboardAnimator = nullptr;
    }

    auto getter = [this](const std::string& propName) -> ThemeData::ThemeElement::Property {
        ThemeData::ThemeElement::Property value;

        const glm::vec2 scale {getParent() ?
                                   getParent()->getSize() :
                                   glm::vec2 {Renderer::getScreenWidth(), Renderer::getScreenHeight()}};

        if (propName == "pos")
            value = glm::vec2 {mPosition.x / scale.x, mPosition.y / scale.y};
        else if (propName == "x")
            value = mPosition.x / scale.x;
        else if (propName == "y")
            value = mPosition.y / scale.y;
        else if (propName == "size")
            value = glm::vec2 {mSize.x / scale.x, mSize.y / scale.y};
        else if (propName == "w")
            value = mSize.x / scale.x;
        else if (propName == "h")
            value = mSize.y / scale.y;
        else if (propName == "origin")
            value = mOrigin;
        else if (propName == "rotation")
            value = static_cast<float>(glm::degrees(mRotation));
        else if (propName == "rotationOrigin")
            value = mRotationOrigin;
        else if (propName == "scaleOrigin")
            value = mScaleOrigin;
        else if (propName == "opacity")
            value = getOpacity();
        else if (propName == "zIndex")
            value = mZIndex;
        else if (propName == "scale")
            value = mScale;
        else if (propName == "offset")
            value = glm::vec2 {mScreenOffset.x / scale.x, mScreenOffset.y / scale.y};
        else if (propName == "offsetX")
            value = mScreenOffset.x / scale.x;
        else if (propName == "offsetY")
            value = mScreenOffset.y / scale.y;
        else if (propName == "minSize")
            value = glm::vec2 {mMinSize.x / scale.x, mMinSize.y / scale.y};
        else if (propName == "maxSize") {
            const float maxX = (mMaxSize.x > 0.0f ? mMaxSize.x / scale.x : 0.0f);
            const float maxY = (mMaxSize.y > 0.0f ? mMaxSize.y / scale.y : 0.0f);
            value = glm::vec2 {maxX, maxY};
        }
        else if (propName == "color")
            value = getColor();
        else if (propName == "brightness")
            value = getBrightness();
        else if (propName == "saturation")
            value = getSaturation();
        else if (propName == "dimming")
            value = getDimming();
        else if (propName == "visible")
            value = mVisible;
        else if (propName == "sound")
            value = mStoryBoardSound;

        return value;
    };

    auto setter = [this](const std::string& propName,
                         const ThemeData::ThemeElement::Property& value) {
        const glm::vec2 scale {getParent() ?
                                   getParent()->getSize() :
                                   glm::vec2 {Renderer::getScreenWidth(), Renderer::getScreenHeight()}};

        if (propName == "pos" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
            setPosition(value.v.x * scale.x, value.v.y * scale.y);
        else if (propName == "x" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setPosition(value.f * scale.x, mPosition.y);
        else if (propName == "y" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setPosition(mPosition.x, value.f * scale.y);
        else if (propName == "size" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
            setSize(value.v.x * scale.x, value.v.y * scale.y);
        else if (propName == "w" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setSize(value.f * scale.x, mSize.y);
        else if (propName == "h" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setSize(mSize.x, value.f * scale.y);
        else if (propName == "origin" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
            setOrigin(value.v);
        else if (propName == "rotation" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setRotationDegrees(value.f);
        else if (propName == "rotationOrigin" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
            setRotationOrigin(value.v);
           else if (propName == "scaleOrigin" &&
                  value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
              setScaleOrigin(value.v);
        else if (propName == "opacity" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setOpacity(glm::clamp(value.f, 0.0f, 1.0f));
        else if (propName == "zIndex" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setZIndex(value.f);
        else if (propName == "scale" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setScale(value.f);
           else if (propName == "offset" &&
                  value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
              setScreenOffset(glm::vec2 {value.v.x * scale.x, value.v.y * scale.y});
           else if (propName == "offsetX" &&
                  value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
              setScreenOffset(glm::vec2 {value.f * scale.x, mScreenOffset.y});
           else if (propName == "offsetY" &&
                  value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
              setScreenOffset(glm::vec2 {mScreenOffset.x, value.f * scale.y});
           else if (propName == "minSize" &&
                  value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
              setMinSize(glm::vec2 {value.v.x * scale.x, value.v.y * scale.y});
           else if (propName == "maxSize" &&
                  value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
              setMaxSize(glm::vec2 {value.v.x * scale.x, value.v.y * scale.y});
        else if (propName == "color" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Color)
            setColor(value.i);
        else if (propName == "brightness" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setBrightness(value.f);
        else if (propName == "saturation" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setSaturation(value.f);
        else if (propName == "dimming" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Float)
            setDimming(value.f);
        else if (propName == "path" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::String)
            setImage(value.s);
        else if (propName == "text" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::String)
            setValue(value.s);
        else if (propName == "visible" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::Bool)
            setVisible(value.b);
        else if (propName == "sound" &&
                 value.type == ThemeData::ThemeElement::Property::PropertyType::String) {
            if (mStoryBoardSound != value.s) {
                mStoryBoardSound = value.s;
                if (!mStoryBoardSound.empty())
                    Sound::get(mStoryBoardSound)->play();
            }
        }
    };

    mStoryboardAnimator = new StoryboardAnimator(sb->second.get(), getter, setter);
    mStoryboardAnimator->reset();

    return true;
}

void GuiComponent::stopStoryboard()
{
    if (mStoryboardAnimator != nullptr)
        mStoryboardAnimator->stop();
}

void GuiComponent::startStoryboard()
{
    if (mStoryboardAnimator != nullptr)
        mStoryboardAnimator->reset();
}

void GuiComponent::pauseStoryboard()
{
    if (mStoryboardAnimator != nullptr)
        mStoryboardAnimator->pause();
}

void GuiComponent::deselectStoryboard(bool restoreInitialProperties)
{
    if (mStoryboardAnimator != nullptr) {
        if (!restoreInitialProperties)
            mStoryboardAnimator->clearInitialProperties();

        mStoryboardAnimator->stop();
        mStoryboardAnimator->reset();
        delete mStoryboardAnimator;
        mStoryboardAnimator = nullptr;
    }
}

void GuiComponent::enableStoryboardProperty(const std::string& name, bool enable)
{
    if (mStoryboardAnimator != nullptr)
        mStoryboardAnimator->enableProperty(name, enable);
}
