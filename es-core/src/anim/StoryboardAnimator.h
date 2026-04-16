// SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  StoryboardAnimator.h
//
//  Runtime player de storyboards de tema.
//

#pragma once

#include "anim/ThemeAnimation.h"
#include "anim/ThemeStoryboard.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

class StoryAnimation
{
public:
    StoryAnimation(ThemeAnimation* anim)
    {
        animation = anim;
        mRepeatCount = 0;
        mCurrentTime = 0;
        mIsReversed = false;
    }

    bool update(int elapsed);

    ThemeData::ThemeElement::Property currentValue;
    ThemeAnimation* animation;

private:
    int mRepeatCount;
    int mCurrentTime;
    bool mIsReversed;
};

class StoryboardAnimator
{
public:
    StoryboardAnimator(
        ThemeStoryboard* storyboard,
        std::function<ThemeData::ThemeElement::Property(const std::string&)> getter,
        std::function<void(const std::string&, const ThemeData::ThemeElement::Property&)> setter);
    ~StoryboardAnimator();

    void reset(int atTime = 0, bool resetInitialProperties = true);
    void stop();
    bool update(int elapsed);
    void pause();

    bool isRunning() const { return !mPaused; }
    void clearInitialProperties();

    std::string getName() const;
    void enableProperty(const std::string& name, bool enable);

private:
    void addNewAnimations();
    void clearStories();

    ThemeStoryboard* mStoryBoard;
    std::function<ThemeData::ThemeElement::Property(const std::string&)> mGetter;
    std::function<void(const std::string&, const ThemeData::ThemeElement::Property&)> mSetter;

    int mRepeatCount;
    int mCurrentTime;

    bool mPaused;

    std::vector<StoryAnimation*> mCurrentStories;
    std::vector<StoryAnimation*> mFinishedStories;
    std::map<std::string, ThemeData::ThemeElement::Property> mInitialProperties;
    std::set<std::string> mDisabledProperties;

    bool mHasInitialProperties;
};
