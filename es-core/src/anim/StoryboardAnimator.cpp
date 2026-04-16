// SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  StoryboardAnimator.cpp
//
//  Reproductor de storyboards para propiedades de GuiComponent.
//

#include "anim/StoryboardAnimator.h"

#include <cmath>
#include <iterator>

StoryboardAnimator::StoryboardAnimator(
    ThemeStoryboard* storyboard,
    std::function<ThemeData::ThemeElement::Property(const std::string&)> getter,
    std::function<void(const std::string&, const ThemeData::ThemeElement::Property&)> setter)
{
    mHasInitialProperties = false;
    mPaused = true;
    mGetter = getter;
    mSetter = setter;

    mStoryBoard = new ThemeStoryboard(*storyboard);
    mRepeatCount = 0;
    mCurrentTime = 0;
}

StoryboardAnimator::~StoryboardAnimator()
{
    delete mStoryBoard;

    pause();

    for (auto story : mCurrentStories)
        delete story;

    mCurrentStories.clear();
}

bool StoryAnimation::update(int elapsed)
{
    if (animation == nullptr)
        return false;

    if (mIsReversed)
        mCurrentTime -= elapsed;
    else
        mCurrentTime += elapsed;

    bool ended {false};
    bool pseudoEnd {false};

    if (!mIsReversed && mCurrentTime > animation->duration) {
        if (animation->autoReverse) {
            mIsReversed = true;
            mCurrentTime = animation->duration;
        }
        else {
            pseudoEnd = true;
            mCurrentTime = 0;
        }
    }
    else if (mIsReversed && mCurrentTime < 0) {
        mCurrentTime = 0;
        mIsReversed = false;
        pseudoEnd = true;
    }

    if (pseudoEnd) {
        if (animation->repeat == 1)
            ended = true;
        else if (animation->repeat > 1) {
            mRepeatCount++;
            if (mRepeatCount >= animation->repeat)
                ended = true;
            else
                mCurrentTime = 0;
        }
    }

    if (ended || animation->duration == 0) {
        currentValue = animation->computeValue(animation->autoReverse ? 0.0f : 1.0f);
        return false;
    }

    double value {0.0};

    if (mCurrentTime >= animation->duration) {
        value = 1.0;
    }
    else if (mCurrentTime < animation->duration) {
        double t = static_cast<double>(mCurrentTime) / static_cast<double>(animation->duration);
        if (t > 1.0)
            t = 1.0;

        switch (animation->easingMode) {
            case ThemeAnimation::EasingMode::EaseIn:
                value = (t * t);
                break;
            case ThemeAnimation::EasingMode::EaseInCubic:
                value = t * t * t;
                break;
            case ThemeAnimation::EasingMode::EaseInQuint:
                value = t * t * t * t * t;
                break;
            case ThemeAnimation::EasingMode::EaseOut:
                value = (-t * (t - 2.0));
                break;
            case ThemeAnimation::EasingMode::EaseOutCubic:
                t = t - 1.0;
                value = t * t * t + 1.0;
                break;
            case ThemeAnimation::EasingMode::EaseOutQuint:
                t = t - 1.0;
                value = t * t * t * t * t + 1.0;
                break;
            case ThemeAnimation::EasingMode::EaseInOut: {
                t = static_cast<double>(mCurrentTime) /
                    (static_cast<double>(animation->duration) / 2.0);
                if (t < 1.0)
                    value = t * t / 2.0;
                else {
                    t -= 1.0;
                    value = (-0.5 * (t * (t - 2.0) - 1.0));
                }
                break;
            }
            case ThemeAnimation::EasingMode::Bump:
                value = sin((3.14159265358979323846 / 2.0) * t) +
                        sin(3.14159265358979323846 * t) / 2.0;
                break;
            default:
                value = t;
                break;
        }
    }

    currentValue = animation->computeValue(value);
    return !ended;
}

void StoryboardAnimator::clearStories()
{
    for (auto story : mFinishedStories)
        delete story;

    for (auto story : mCurrentStories)
        delete story;

    mFinishedStories.clear();
    mCurrentStories.clear();
}

void StoryboardAnimator::reset(int atTime, bool resetInitialProperties)
{
    mPaused = false;
    mCurrentTime = atTime;

    clearStories();

    if (atTime > 0) {
        for (auto anim : mStoryBoard->animations) {
            if (anim->enabled && (anim->begin + anim->duration <= atTime))
                mFinishedStories.push_back(new StoryAnimation(anim));
        }

        addNewAnimations();
    }
    else {
        ThemeData::ThemeElement::Property emptySound;
        emptySound = std::string();
        mSetter("sound", emptySound);

        if (mHasInitialProperties && resetInitialProperties) {
            for (auto prop : mInitialProperties) {
                if (mDisabledProperties.find(prop.first) != mDisabledProperties.cend())
                    continue;

                bool hasAssignationAtZero {false};

                if (atTime == 0) {
                    for (auto anim : mStoryBoard->animations) {
                        if (anim->enabled && anim->begin == 0 && anim->propertyName == prop.first)
                            hasAssignationAtZero = true;
                    }
                }

                if (!hasAssignationAtZero)
                    mSetter(prop.first, prop.second);
            }
        }

        for (auto anim : mStoryBoard->animations) {
            if (anim->enabled && anim->begin == 0)
                mCurrentStories.push_back(new StoryAnimation(anim));
        }
    }
}

void StoryboardAnimator::clearInitialProperties()
{
    mInitialProperties.clear();
}

void StoryboardAnimator::stop()
{
    pause();

    for (auto prop : mInitialProperties) {
        if (mDisabledProperties.find(prop.first) == mDisabledProperties.cend())
            mSetter(prop.first, prop.second);
    }

    clearStories();
}

void StoryboardAnimator::pause()
{
    mPaused = true;
}

void StoryboardAnimator::addNewAnimations()
{
    for (auto anim : mStoryBoard->animations) {
        if (!anim->enabled)
            continue;

        bool exists {false};

        for (auto story : mCurrentStories) {
            if (story->animation == anim) {
                exists = true;
                break;
            }
        }

        if (!exists && !mFinishedStories.empty()) {
            for (auto story : mFinishedStories) {
                if (story->animation == anim) {
                    exists = true;
                    break;
                }
            }
        }

        if (exists)
            continue;

        if (mCurrentTime >= anim->begin) {
            anim->ensureInitialValue(mGetter(anim->propertyName));
            mCurrentStories.push_back(new StoryAnimation(anim));
        }
    }
}

bool StoryboardAnimator::update(int elapsed)
{
    if (mPaused || elapsed > 500)
        return true;

    if (!mHasInitialProperties) {
        mHasInitialProperties = true;

        for (auto anim : mStoryBoard->animations) {
            if (anim->enabled && anim->propertyName != "sound")
                mInitialProperties[anim->propertyName] =
                    mGetter(anim->propertyName);
        }

        for (auto anim : mStoryBoard->animations) {
            if (anim->begin == 0 && anim->enabled) {
                ThemeData::ThemeElement::Property emptySound;
                emptySound = std::string();

                if (anim->to.type == ThemeData::ThemeElement::Property::PropertyType::Unknown)
                    anim->to = anim->propertyName == "sound"
                                   ? emptySound
                                   : mGetter(anim->propertyName);
                if (anim->from.type == ThemeData::ThemeElement::Property::PropertyType::Unknown)
                    anim->from = anim->propertyName == "sound"
                                     ? emptySound
                                     : mGetter(anim->propertyName);
                else if (mDisabledProperties.find(anim->propertyName) == mDisabledProperties.cend())
                    mSetter(anim->propertyName, anim->from);
            }
        }
    }

    mCurrentTime += elapsed;

    addNewAnimations();

    for (int i = static_cast<int>(mCurrentStories.size()) - 1; i >= 0; i--) {
        auto story = mCurrentStories.at(i);
        bool ended = !story->update(elapsed);

        if (mDisabledProperties.find(story->animation->propertyName) ==
            mDisabledProperties.cend()) {
            mSetter(story->animation->propertyName, story->currentValue);
        }

        if (ended) {
            if (story->animation->propertyName == "sound")
            {
                ThemeData::ThemeElement::Property emptySound;
                emptySound = std::string();
                mSetter("sound", emptySound);
            }

            mFinishedStories.push_back(story);
            auto it = mCurrentStories.begin();
            std::advance(it, i);
            mCurrentStories.erase(it);
            continue;
        }
    }

    if (mFinishedStories.size() == mStoryBoard->animations.size()) {
        if (mStoryBoard->repeat == 1) {
            clearStories();
            pause();
            return false;
        }
        else if (mStoryBoard->repeat > 0) {
            mRepeatCount++;
            if (mRepeatCount >= mStoryBoard->repeat) {
                clearStories();
                pause();
                return false;
            }
        }

        reset(mStoryBoard->repeatAt, mStoryBoard->repeatAt == 0);
        return true;
    }

    return true;
}

std::string StoryboardAnimator::getName() const
{
    if (mStoryBoard != nullptr)
        return mStoryBoard->eventName;

    return "";
}

void StoryboardAnimator::enableProperty(const std::string& name, bool enable)
{
    mDisabledProperties.erase(name);

    if (!enable)
        mDisabledProperties.insert(name);
}
