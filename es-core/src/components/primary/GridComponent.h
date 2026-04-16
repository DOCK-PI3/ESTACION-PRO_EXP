//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  GridComponent.h
//
//  Grid, usable in both the system and gamelist views.
//

#ifndef ES_CORE_COMPONENTS_PRIMARY_GRID_COMPONENT_H
#define ES_CORE_COMPONENTS_PRIMARY_GRID_COMPONENT_H

#include "components/IList.h"
#include "components/NinePatchComponent.h"
#include "components/primary/PrimaryComponent.h"

#include <algorithm>
#include <cctype>

struct GridEntry {
    std::shared_ptr<GuiComponent> item;
    std::string imagePath;
    std::string defaultImagePath;
};

template <typename T>
class GridComponent : public PrimaryComponent<T>, protected IList<GridEntry, T>
{
protected:
    using List = IList<GridEntry, T>;
    using List::mColumns;
    using List::mCursor;
    using List::mEntries;
    using List::mLastCursor;
    using List::mRows;
    using List::mScrollVelocity;
    using List::mSize;

public:
    using Entry = typename IList<GridEntry, T>::Entry;

    GridComponent();
    ~GridComponent();

    void addEntry(Entry& entry, const std::shared_ptr<ThemeData>& theme);
    void updateEntry(Entry& entry, const std::shared_ptr<ThemeData>& theme);
    void onDemandTextureLoad() override;
    void calculateLayout();
    const int getColumnCount() const { return mColumns; }
    const int getRowCount() const { return mRows; }
    void setScrollVelocity(int velocity) { mScrollVelocity = velocity; }
    void setSuppressTransitions(bool state) { mSuppressTransitions = state; }

    void setCancelTransitionsCallback(const std::function<void()>& func) override
    {
        mCancelTransitionsCallback = func;
    }
    void setCursorChangedCallback(const std::function<void(CursorState state)>& func) override
    {
        mCursorChangedCallback = func;
    }
    int getCursor() override { return mCursor; }
    const size_t getNumEntries() override { return mEntries.size(); }
    const bool getFadeAbovePrimary() const override { return mFadeAbovePrimary; }
    const LetterCase getLetterCase() const override { return mLetterCase; }
    const LetterCase getLetterCaseAutoCollections() const override
    {
        return mLetterCaseAutoCollections;
    }
    const LetterCase getLetterCaseCustomCollections() const override
    {
        return mLetterCaseCustomCollections;
    }
    const bool getSystemNameSuffix() const override { return mSystemNameSuffix; }
    const LetterCase getLetterCaseSystemNameSuffix() const override
    {
        return mLetterCaseSystemNameSuffix;
    }
    const std::string& getDefaultGridImage() const { return mDefaultImagePath; }
    const std::string& getDefaultGridFolderImage() const { return mDefaultFolderImagePath; }
    void setDefaultImage(std::string defaultImage) { mDefaultImagePath = defaultImage; }
    void setDefaultFolderImage(std::string defaultImage) { mDefaultFolderImagePath = defaultImage; }
    bool input(InputConfig* config, Input input) override;
    void update(int deltaTime) override;
    void render(const glm::mat4& parentTrans) override;
    void applyTheme(const std::shared_ptr<ThemeData>& theme,
                    const std::string& view,
                    const std::string& element,
                    unsigned int properties) override;

private:
    void onShowPrimary() override
    {
        if (mCursor >= 0 && mCursor < static_cast<int>(mEntries.size())) {
            auto comp = mEntries.at(mCursor).data.item;
            if (comp) {
                comp->resetComponent();
                if (comp->selectStoryboard("activate") || comp->selectStoryboard("open") ||
                    comp->selectStoryboard() ||
                    comp->selectStoryboard("scroll")) {
                    comp->startStoryboard();
                    comp->update(0);
                }
            }
        }
    }
    void onScroll() override
    {
        if (mGamelistView)
            NavigationSounds::getInstance().playThemeNavigationSound(SCROLLSOUND);
        else
            NavigationSounds::getInstance().playThemeNavigationSound(SYSTEMBROWSESOUND);
    }
    void onCursorChanged(const CursorState& state) override;
    bool isScrolling() const override { return List::isScrolling(); }
    void stopScrolling() override
    {
        List::stopScrolling();
        // Only finish the animation if we're in the gamelist view.
        if (mGamelistView)
            GuiComponent::finishAnimation(0);
    }
    const int getScrollingVelocity() override { return List::getScrollingVelocity(); }
    void clear() override { List::clear(); }
    const T& getSelected() const override { return List::getSelected(); }
    const T& getNext() const override { return List::getNext(); }
    const T& getPrevious() const override { return List::getPrevious(); }
    const T& getFirst() const override { return List::getFirst(); }
    const T& getLast() const override { return List::getLast(); }
    bool setCursor(const T& obj) override
    {
        mLastCursor = mCursor;
        return List::setCursor(obj);
    }
    bool remove(const T& obj) override { return List::remove(obj); }
    int size() const override { return List::size(); }

    enum class ImageFit {
        CONTAIN,
        FILL,
        COVER
    };

    enum class SelectorLayer {
        TOP,
        MIDDLE,
        BOTTOM
    };

    enum class ScrollDirection {
        VERTICAL,
        HORIZONTAL
    };

    struct TileOverlayStyle {
        std::unique_ptr<ImageComponent> image;
        glm::vec2 pos {0.0f, 0.0f};
        glm::vec2 size {0.0f, 0.0f};
        bool enabled {false};
        bool onlySelected {false};
        bool favoriteOnly {false};
        bool completedOnly {false};
        bool dynamicMarquee {false};
        std::string cachedPath;
    };

    Renderer* mRenderer;
    std::function<void()> mCancelTransitionsCallback;
    std::function<void(CursorState state)> mCursorChangedCallback;
    float mEntryOffset;
    float mScrollPos;
    float mTransitionFactor;
    float mVisibleRows;
    int mPreviousScrollVelocity;
    bool mPositiveDirection;
    bool mGamelistView;
    bool mLayoutValid;
    bool mWasScrolling;
    bool mJustCalculatedLayout;
    bool mSuppressTransitions;
    bool mCenterSelection;
    bool mScrollLoop;
    ScrollDirection mScrollDirection;
    int mAutoLayoutColumns;
    int mAutoLayoutRows;
    float mHorizontalMargin;
    float mVerticalMargin;
    glm::vec4 mPadding;
    bool mSelectorImageOnly;
    TileOverlayStyle mOverlayStyle;
    TileOverlayStyle mOverlaySelectedStyle;
    TileOverlayStyle mFavoriteStyle;
    TileOverlayStyle mFavoriteSelectedStyle;
    TileOverlayStyle mMarqueeStyle;
    TileOverlayStyle mMarqueeSelectedStyle;
    TileOverlayStyle mCheevosStyle;
    TileOverlayStyle mCheevosSelectedStyle;

    std::vector<std::string> mImageTypes;
    std::string mDefaultImagePath;
    std::string mDefaultFolderImagePath;
    std::shared_ptr<ImageComponent> mDefaultImage;
    glm::vec2 mItemSize;
    float mItemScale;
    glm::vec2 mItemSpacing;
    bool mScaleInwards;
    bool mFractionalRows;
    bool mInstantItemTransitions;
    bool mInstantRowTransitions;
    float mUnfocusedItemOpacity;
    float mUnfocusedItemSaturation;
    bool mHasUnfocusedItemSaturation;
    float mUnfocusedItemDimming;
    ImageFit mImagefit;
    ImageFit mSelectedImagefit;
    bool mHasSelectedImageFit;
    glm::vec2 mImageCropPos;
    bool mImageLinearInterpolation;
    float mImageRelativeScale;
    float mImageCornerRadius;
    unsigned int mImageColor;
    unsigned int mImageColorEnd;
    bool mImageColorGradientHorizontal;
    unsigned int mImageSelectedColor;
    unsigned int mImageSelectedColorEnd;
    bool mImageSelectedColorGradientHorizontal;
    bool mHasImageSelectedColor;
    float mImageBrightness;
    float mImageSaturation;
    std::unique_ptr<ImageComponent> mBackgroundImage;
    std::unique_ptr<NinePatchComponent> mSelectedBackgroundNinePatch;
    glm::vec2 mSelectedBackgroundNinePatchPadding;
    bool mHasSelectedBackgroundNinePatchPadding;
    std::string mBackgroundImagePath;
    float mBackgroundRelativeScale;
    float mBackgroundCornerRadius;
    float mSelectedBackgroundCornerRadius;
    unsigned int mBackgroundColor;
    unsigned int mBackgroundColorEnd;
    bool mBackgroundColorGradientHorizontal;
    bool mHasBackgroundColor;
    unsigned int mSelectedBackgroundColor;
    unsigned int mSelectedBackgroundColorEnd;
    bool mHasSelectedBackgroundColor;
    std::unique_ptr<ImageComponent> mSelectorImage;
    std::string mSelectorImagePath;
    float mSelectorRelativeScale;
    SelectorLayer mSelectorLayer;
    float mSelectorCornerRadius;
    unsigned int mSelectorColor;
    unsigned int mSelectorColorEnd;
    bool mSelectorColorGradientHorizontal;
    bool mHasSelectorColor;
    float mTextRelativeScale;
    float mTextBackgroundCornerRadius;
    unsigned int mTextColor;
    unsigned int mTextBackgroundColor;
    unsigned int mTextSelectedColor;
    unsigned int mTextSelectedBackgroundColor;
    bool mHasTextSelectedColor;
    bool mTextHorizontalScrolling;
    float mTextHorizontalScrollSpeed;
    float mTextHorizontalScrollDelay;
    float mTextHorizontalScrollGap;
    std::shared_ptr<Font> mFont;
    LetterCase mLetterCase;
    LetterCase mLetterCaseAutoCollections;
    LetterCase mLetterCaseCustomCollections;
    float mLineSpacing;
    bool mSystemNameSuffix;
    LetterCase mLetterCaseSystemNameSuffix;
    bool mFadeAbovePrimary;
};

template <typename T>
GridComponent<T>::GridComponent()
    : IList<GridEntry, T> {IList<GridEntry, T>::LIST_SCROLL_STYLE_SLOW,
                           ListLoopType::LIST_PAUSE_AT_END}
    , mRenderer {Renderer::getInstance()}
    , mEntryOffset {0.0f}
    , mScrollPos {0.0f}
    , mTransitionFactor {1.0f}
    , mVisibleRows {1.0f}
    , mPreviousScrollVelocity {0}
    , mPositiveDirection {false}
    , mGamelistView {std::is_same_v<T, FileData*> ? true : false}
    , mLayoutValid {false}
    , mWasScrolling {false}
    , mJustCalculatedLayout {false}
    , mSuppressTransitions {false}
    , mCenterSelection {false}
    , mScrollLoop {false}
    , mScrollDirection {ScrollDirection::VERTICAL}
    , mAutoLayoutColumns {0}
    , mAutoLayoutRows {0}
    , mHorizontalMargin {0.0f}
    , mVerticalMargin {0.0f}
    , mPadding {0.0f, 0.0f, 0.0f, 0.0f}
    , mSelectorImageOnly {false}
    , mOverlayStyle {}
    , mOverlaySelectedStyle {}
    , mFavoriteStyle {}
    , mFavoriteSelectedStyle {}
    , mMarqueeStyle {}
    , mMarqueeSelectedStyle {}
    , mCheevosStyle {}
    , mCheevosSelectedStyle {}
    , mItemSize {glm::vec2 {mRenderer->getScreenWidth() * 0.15f,
                            mRenderer->getScreenHeight() * 0.25f}}
    , mItemScale {1.05f}
    , mItemSpacing {0.0f, 0.0f}
    , mScaleInwards {false}
    , mFractionalRows {false}
    , mInstantItemTransitions {false}
    , mInstantRowTransitions {false}
    , mUnfocusedItemOpacity {1.0f}
    , mUnfocusedItemSaturation {1.0f}
    , mHasUnfocusedItemSaturation {false}
    , mUnfocusedItemDimming {1.0f}
    , mImagefit {ImageFit::CONTAIN}
    , mSelectedImagefit {ImageFit::CONTAIN}
    , mHasSelectedImageFit {false}
    , mImageCropPos {0.5f, 0.5f}
    , mImageLinearInterpolation {true}
    , mImageRelativeScale {1.0f}
    , mImageCornerRadius {0.0f}
    , mImageColor {0xFFFFFFFF}
    , mImageColorEnd {0xFFFFFFFF}
    , mImageColorGradientHorizontal {true}
    , mImageSelectedColor {0xFFFFFFFF}
    , mImageSelectedColorEnd {0xFFFFFFFF}
    , mImageSelectedColorGradientHorizontal {true}
    , mHasImageSelectedColor {false}
    , mImageBrightness {0.0f}
    , mImageSaturation {1.0f}
    , mSelectedBackgroundNinePatchPadding {0.0f, 0.0f}
    , mHasSelectedBackgroundNinePatchPadding {false}
    , mBackgroundRelativeScale {1.0f}
    , mBackgroundCornerRadius {0.0f}
    , mSelectedBackgroundCornerRadius {0.0f}
    , mBackgroundColor {0xFFFFFFFF}
    , mBackgroundColorEnd {0xFFFFFFFF}
    , mBackgroundColorGradientHorizontal {true}
    , mHasBackgroundColor {false}
    , mSelectedBackgroundColor {0xFFFFFFFF}
    , mSelectedBackgroundColorEnd {0xFFFFFFFF}
    , mHasSelectedBackgroundColor {false}
    , mSelectorRelativeScale {1.0f}
    , mSelectorLayer {SelectorLayer::TOP}
    , mSelectorCornerRadius {0.0f}
    , mSelectorColor {0xFFFFFFFF}
    , mSelectorColorEnd {0xFFFFFFFF}
    , mSelectorColorGradientHorizontal {true}
    , mHasSelectorColor {false}
    , mTextRelativeScale {1.0f}
    , mTextBackgroundCornerRadius {0.0f}
    , mTextColor {0x000000FF}
    , mTextBackgroundColor {0xFFFFFF00}
    , mTextSelectedColor {0x000000FF}
    , mTextSelectedBackgroundColor {0xFFFFFF00}
    , mHasTextSelectedColor {false}
    , mTextHorizontalScrolling {false}
    , mTextHorizontalScrollSpeed {1.0f}
    , mTextHorizontalScrollDelay {3000.0f}
    , mTextHorizontalScrollGap {1.5f}
    , mLetterCase {LetterCase::NONE}
    , mLetterCaseAutoCollections {LetterCase::UNDEFINED}
    , mLetterCaseCustomCollections {LetterCase::UNDEFINED}
    , mLineSpacing {1.5f}
    , mSystemNameSuffix {true}
    , mLetterCaseSystemNameSuffix {LetterCase::UPPERCASE}
    , mFadeAbovePrimary {false}
{
}

template <typename T> GridComponent<T>::~GridComponent()
{
    // Manually flush the background and selector images from the texture cache on destruction
    // when running in debug mode, otherwise a complete system view reload would be needed to
    // get these images updated. This is useful during theme development when using the Ctrl-r
    // keyboard combination to reload the theme configuration.
    if (Settings::getInstance()->getBool("Debug")) {
        TextureResource::manualUnload(mBackgroundImagePath, false);
        TextureResource::manualUnload(mSelectorImagePath, false);
    }
}

template <typename T>
void GridComponent<T>::addEntry(Entry& entry, const std::shared_ptr<ThemeData>& theme)
{
    const bool dynamic {mGamelistView};

    if (entry.data.imagePath != "" &&
        ResourceManager::getInstance().fileExists(entry.data.imagePath)) {
        auto item = std::make_shared<ImageComponent>(false, dynamic);
        item->setLinearInterpolation(mImageLinearInterpolation);
        item->setMipmapping(true);
        if (mImagefit == ImageFit::CONTAIN) {
            item->setMaxSize(glm::round(mItemSize * mImageRelativeScale));
        }
        else if (mImagefit == ImageFit::FILL) {
            item->setResize(glm::round(mItemSize * mImageRelativeScale));
        }
        else if (mImagefit == ImageFit::COVER) {
            item->setCropPos(mImageCropPos);
            item->setCroppedSize(glm::round(mItemSize * mImageRelativeScale));
        }
        item->setCornerRadius(mImageCornerRadius);
        item->setImage(entry.data.imagePath);
        if (mImageBrightness != 0.0)
            item->setBrightness(mImageBrightness);
        if (mImageSaturation != 1.0)
            item->setSaturation(mImageSaturation);
        if (mImageColor != 0xFFFFFFFF)
            item->setColorShift(mImageColor);
        if (mImageColorEnd != mImageColor) {
            item->setColorShiftEnd(mImageColorEnd);
            if (!mImageColorGradientHorizontal)
                item->setColorGradientHorizontal(false);
        }
        item->setOrigin(0.5f, 0.5f);
        item->setRotateByTargetSize(true);
        entry.data.item = item;
    }
    else if (entry.data.defaultImagePath != "" &&
             ResourceManager::getInstance().fileExists(entry.data.defaultImagePath)) {
        if (!mGamelistView)
            entry.data.imagePath = "";

        if (mDefaultImage.get() == nullptr || !mGamelistView) {
            mDefaultImage = std::make_shared<ImageComponent>(false, dynamic);
            mDefaultImage->setLinearInterpolation(mImageLinearInterpolation);
            mDefaultImage->setMipmapping(true);
            if (mImagefit == ImageFit::CONTAIN) {
                mDefaultImage->setMaxSize(glm::round(mItemSize * mImageRelativeScale));
            }
            else if (mImagefit == ImageFit::FILL) {
                mDefaultImage->setResize(glm::round(mItemSize * mImageRelativeScale));
            }
            else if (mImagefit == ImageFit::COVER) {
                mDefaultImage->setCropPos(mImageCropPos);
                mDefaultImage->setCroppedSize(glm::round(mItemSize * mImageRelativeScale));
            }
            mDefaultImage->setCornerRadius(mImageCornerRadius);
            mDefaultImage->setImage(entry.data.defaultImagePath);
            if (mImageBrightness != 0.0)
                mDefaultImage->setBrightness(mImageBrightness);
            if (mImageSaturation != 1.0)
                mDefaultImage->setSaturation(mImageSaturation);
            if (mImageColor != 0xFFFFFFFF)
                mDefaultImage->setColorShift(mImageColor);
            if (mImageColorEnd != mImageColor) {
                mDefaultImage->setColorShiftEnd(mImageColorEnd);
                if (!mImageColorGradientHorizontal)
                    mDefaultImage->setColorGradientHorizontal(false);
            }
            mDefaultImage->setOrigin(0.5f, 0.5f);
            mDefaultImage->setRotateByTargetSize(true);
        }
        // For the gamelist view the default image is applied in onDemandTextureLoad().
        if (!mGamelistView)
            entry.data.item = mDefaultImage;
    }
    else if (!mGamelistView) {
        entry.data.imagePath = "";
    }

    if (!entry.data.item) {
        // Always add the item text as fallback in case there is no image. This is also displayed
        // when quick-jumping as textures are not loaded in this case.
        auto text = std::make_shared<TextComponent>(
            entry.name, mFont, 0x000000FF, Alignment::ALIGN_CENTER, Alignment::ALIGN_CENTER,
            glm::ivec2 {0, 0}, glm::vec3 {0.0f, 0.0f, 0.0f}, mItemSize * mTextRelativeScale,
            0x00000000, mLineSpacing, 1.0f, mTextHorizontalScrolling, mTextHorizontalScrollSpeed,
            mTextHorizontalScrollDelay, mTextHorizontalScrollGap);
        text->setOrigin(0.5f, 0.5f);
        text->setBackgroundCornerRadius(mTextBackgroundCornerRadius);
        text->setColor(mTextColor);
        text->setBackgroundColor(mTextBackgroundColor);
        text->setRenderBackground(true);

        entry.data.item = text;
    }

    List::add(entry);
}

template <typename T>
void GridComponent<T>::updateEntry(Entry& entry, const std::shared_ptr<ThemeData>& theme)
{
    if (entry.data.imagePath != "") {
        const glm::vec3& calculatedItemPos {entry.data.item->getPosition()};
        auto item = std::make_shared<ImageComponent>(false, true);
        item->setLinearInterpolation(mImageLinearInterpolation);
        item->setMipmapping(true);
        if (mImagefit == ImageFit::CONTAIN) {
            item->setMaxSize(glm::round(mItemSize * mImageRelativeScale));
        }
        else if (mImagefit == ImageFit::FILL) {
            item->setResize(glm::round(mItemSize * mImageRelativeScale));
        }
        else if (mImagefit == ImageFit::COVER) {
            item->setCropPos(mImageCropPos);
            item->setCroppedSize(glm::round(mItemSize * mImageRelativeScale));
        }
        item->setCornerRadius(mImageCornerRadius);
        item->setImage(entry.data.imagePath);
        if (mImageBrightness != 0.0)
            item->setBrightness(mImageBrightness);
        if (mImageSaturation != 1.0)
            item->setSaturation(mImageSaturation);
        if (mImageColor != 0xFFFFFFFF)
            item->setColorShift(mImageColor);
        if (mImageColorEnd != mImageColor) {
            item->setColorShiftEnd(mImageColorEnd);
            if (!mImageColorGradientHorizontal)
                item->setColorGradientHorizontal(false);
        }
        item->setOrigin(0.5f, 0.5f);
        item->setRotateByTargetSize(true);
        entry.data.item = item;
        entry.data.item->setPosition(calculatedItemPos);
    }
    else {
        return;
    }
}

template <typename T> void GridComponent<T>::onDemandTextureLoad()
{
    if constexpr (std::is_same_v<T, FileData*>) {
        if (size() == 0)
            return;

        if (mImageTypes.empty())
            mImageTypes.emplace_back("marquee");

        const int visibleRows {static_cast<int>(std::ceil(mVisibleRows))};
        const int columnPos {mCursor % mColumns};
        int loadItems {mColumns * visibleRows};

        const int numEntries {size()};
        int startPos {mCursor};
        int loadedItems {0};

        if (mCursor / mColumns <= visibleRows - 1)
            startPos = 0;
        else
            startPos = mCursor - (mColumns * (visibleRows - 1)) - columnPos;

        if (mItemSpacing.y < mVerticalMargin) {
            loadItems += mColumns;
            if (!mFractionalRows) {
                loadItems += mColumns;
                startPos -= mColumns;
                if (startPos < 0)
                    startPos = 0;
            }
        }

        for (int i {startPos}; i < size(); ++i) {
            if (loadedItems == loadItems)
                break;
            ++loadedItems;
            int cursor {i};

            while (cursor < 0)
                cursor += numEntries;
            while (cursor >= numEntries)
                cursor -= numEntries;

            auto& entry = mEntries.at(cursor);

            if (entry.data.imagePath == "") {
                FileData* game {entry.object};

                for (auto& imageType : mImageTypes) {
                    if (imageType == "marquee")
                        entry.data.imagePath = game->getMarqueePath();
                    else if (imageType == "cover")
                        entry.data.imagePath = game->getCoverPath();
                    else if (imageType == "backcover")
                        entry.data.imagePath = game->getBackCoverPath();
                    else if (imageType == "3dbox")
                        entry.data.imagePath = game->get3DBoxPath();
                    else if (imageType == "physicalmedia")
                        entry.data.imagePath = game->getPhysicalMediaPath();
                    else if (imageType == "screenshot")
                        entry.data.imagePath = game->getScreenshotPath();
                    else if (imageType == "titlescreen")
                        entry.data.imagePath = game->getTitleScreenPath();
                    else if (imageType == "miximage")
                        entry.data.imagePath = game->getMiximagePath();
                    else if (imageType == "fanart")
                        entry.data.imagePath = game->getFanArtPath();
                    else if (imageType == "none") // Display the game name as text.
                        break;

                    if (entry.data.imagePath != "")
                        break;
                }

                if (entry.data.imagePath == "")
                    entry.data.imagePath = entry.data.defaultImagePath;

                auto theme = game->getSystem()->getTheme();
                updateEntry(entry, theme);
            }
        }
    }
}

template <typename T> void GridComponent<T>::calculateLayout()
{
    assert(!mEntries.empty());

    if (mItemScale < 1.0f) {
        mHorizontalMargin = 0.0f;
        mVerticalMargin = 0.0f;
    }
    else {
        mHorizontalMargin =
            ((mItemSize.x * (mScaleInwards ? 1.0f : mItemScale)) - mItemSize.x) / 2.0f;
        mVerticalMargin =
            ((mItemSize.y * (mScaleInwards ? 1.0f : mItemScale)) - mItemSize.y) / 2.0f;
    }

    int columnCount {0};
    mColumns = 0;
    mRows = 0;

    const float availableWidth {std::max(0.0f, mSize.x - (mPadding.x + mPadding.z))};
    const float availableHeight {std::max(0.0f, mSize.y - (mPadding.y + mPadding.w))};
    if (mAutoLayoutColumns > 0) {
        mColumns = std::max(1, mAutoLayoutColumns);
    }
    else {
        float width {mHorizontalMargin * 2.0f};

        while (1) {
            width += mItemSize.x;
            if (mColumns != 0)
                width += mItemSpacing.x;
            if (width > availableWidth)
                break;
            ++mColumns;
        }

        if (mColumns == 0)
            ++mColumns;
    }

    for (auto& entry : mEntries) {
        entry.data.item->setPosition(glm::vec3 {
            mPadding.x + mHorizontalMargin + (mItemSize.x * columnCount) + (mItemSize.x * 0.5f) +
                mItemSpacing.x * columnCount,
            mPadding.y + mVerticalMargin + (mItemSize.y * mRows) + (mItemSize.y * 0.5f) +
                mItemSpacing.y * mRows,
            0.0f});
        if (columnCount == mColumns - 1) {
            ++mRows;
            columnCount = 0;
            continue;
        }

        ++columnCount;
    }

    mVisibleRows = availableHeight / (mItemSize.y + mItemSpacing.y);
    if (availableHeight > 0.0f) {
        mVisibleRows -= (mVerticalMargin / availableHeight) * mVisibleRows * 2.0f;
        mVisibleRows += (mItemSpacing.y / availableHeight) * mVisibleRows;
    }

    if (!mFractionalRows)
        mVisibleRows = std::floor(mVisibleRows);

    if (mAutoLayoutRows > 0)
        mVisibleRows = static_cast<float>(mAutoLayoutRows);

    if (mVisibleRows == 0.0f)
        ++mVisibleRows;

    mLayoutValid = true;
    mJustCalculatedLayout = true;
}

template <typename T> bool GridComponent<T>::input(InputConfig* config, Input input)
{
    if (size() > 0) {
        auto movePrevItem = [this]() {
            if (mCancelTransitionsCallback)
                mCancelTransitionsCallback();
            List::listInput(-1);
        };

        auto moveNextItem = [this]() {
            if (mCancelTransitionsCallback)
                mCancelTransitionsCallback();
            List::listInput(1);
        };

        auto moveUpRow = [this]() {
            if (mCursor >= mColumns) {
                if (mCancelTransitionsCallback)
                    mCancelTransitionsCallback();
                List::listInput(-mColumns);
            }
            else if (mScrollLoop && mColumns > 0 && size() > mColumns) {
                if (mCancelTransitionsCallback)
                    mCancelTransitionsCallback();

                const int column {mCursor % mColumns};
                int wrappedCursor {column};
                while (wrappedCursor + mColumns < size())
                    wrappedCursor += mColumns;

                if (wrappedCursor != mCursor) {
                    mLastCursor = mCursor;
                    List::listInput(0);
                    mCursor = wrappedCursor;
                    onScroll();
                    onCursorChanged(CursorState::CURSOR_STOPPED);
                }
            }
        };

        auto moveDownRow = [this]() {
            const int columnModulus {size() % mColumns};
            if (mCursor < size() - (columnModulus == 0 ? mColumns : columnModulus)) {
                if (mCancelTransitionsCallback)
                    mCancelTransitionsCallback();
                List::listInput(mColumns);
            }
            else if (mScrollLoop && mColumns > 0 && size() > mColumns) {
                if (mCancelTransitionsCallback)
                    mCancelTransitionsCallback();

                const int wrappedCursor {mCursor % mColumns};
                if (wrappedCursor != mCursor) {
                    mLastCursor = mCursor;
                    List::listInput(0);
                    mCursor = wrappedCursor;
                    onScroll();
                    onCursorChanged(CursorState::CURSOR_STOPPED);
                }
            }
        };

        if (input.value != 0) {
            if (mScrollDirection == ScrollDirection::HORIZONTAL) {
                if (config->isMappedLike("left", input)) {
                    moveUpRow();
                    return true;
                }
                if (config->isMappedLike("right", input)) {
                    moveDownRow();
                    return true;
                }
                if (config->isMappedLike("up", input)) {
                    movePrevItem();
                    return true;
                }
                if (config->isMappedLike("down", input)) {
                    moveNextItem();
                    return true;
                }
            }
            else {
                if (config->isMappedLike("left", input)) {
                    movePrevItem();
                    return true;
                }
                if (config->isMappedLike("right", input)) {
                    moveNextItem();
                    return true;
                }
                if (config->isMappedLike("up", input)) {
                    moveUpRow();
                    return true;
                }
                if (config->isMappedLike("down", input)) {
                    moveDownRow();
                    return true;
                }
            }
            if (config->isMappedLike("lefttrigger", input)) {
                if (getCursor() == 0)
                    return true;
                if (mCancelTransitionsCallback)
                    mCancelTransitionsCallback();
                return this->listFirstRow();
            }
            if (config->isMappedLike("righttrigger", input)) {
                if (getCursor() == static_cast<int>(mEntries.size()) - 1)
                    return true;
                if (mCancelTransitionsCallback)
                    mCancelTransitionsCallback();
                return this->listLastRow();
            }
        }
        else {
            if (config->isMappedLike("left", input) || config->isMappedLike("right", input) ||
                config->isMappedLike("up", input) || config->isMappedLike("down", input) ||
                config->isMappedLike("lefttrigger", input) ||
                config->isMappedLike("righttrigger", input)) {
                if (isScrolling())
                    onCursorChanged(CursorState::CURSOR_STOPPED);
                List::listInput(0);
            }
        }
    }

    return GuiComponent::input(config, input);
}

template <typename T> void GridComponent<T>::update(int deltaTime)
{
    mEntries.at(mCursor).data.item->update(deltaTime);
    List::listUpdate(deltaTime);
    GuiComponent::update(deltaTime);
}

template <typename T> void GridComponent<T>::render(const glm::mat4& parentTrans)
{
    if (mEntries.empty())
        return;

    glm::mat4 trans {parentTrans * List::getTransform()};
    mRenderer->setMatrix(trans);

    // In image debug mode, draw a green rectangle covering the entire grid area.
    if (Settings::getInstance()->getBool("DebugImage"))
        mRenderer->drawRect(0.0f, 0.0f, mSize.x, mSize.y, 0x00FF0033, 0x00FF0033);

    // Clip to element boundaries.
    glm::vec3 dim {mSize.x, mSize.y, 0.0f};

    if (!mFractionalRows && mSize.y > mItemSize.y)
        dim.y = mVisibleRows * (mItemSize.y + mItemSpacing.y) + (mVerticalMargin * 2.0f) -
                mItemSpacing.y;

    dim.x = (trans[0].x * dim.x + trans[3].x) - trans[3].x;
    dim.y = (trans[1].y * dim.y + trans[3].y) - trans[3].y;

    mRenderer->pushClipRect(glm::ivec2 {static_cast<int>(trans[3].x), static_cast<int>(trans[3].y)},
                            glm::ivec2 {static_cast<int>(dim.x), static_cast<int>(dim.y)});

    // We want to render the currently selected item last and before that the last selected
    // item to avoid incorrect overlapping in case the element has been configured with for
    // example large scaling or small or no margins between items.
    std::vector<size_t> renderEntries;

    const int currRow {static_cast<int>(std::ceil(mScrollPos))};
    const int visibleRows {static_cast<int>(std::ceil(mVisibleRows))};
    int startPos {0};
    int loadItems {mColumns * visibleRows};
    int loadedItems {0};

    if (currRow > 0) {
        if (GuiComponent::isAnimationPlaying(0) || mItemSpacing.y <= mVerticalMargin) {
            loadItems += mColumns;
            startPos = (currRow - 1) * mColumns;
        }
        else {
            if (mFractionalRows)
                startPos = (currRow - 1) * mColumns;
            else
                startPos = currRow * mColumns;
        }

        if (mItemSpacing.y < mVerticalMargin) {
            if (GuiComponent::isAnimationPlaying(0)) {
                loadItems += mColumns;
                startPos -= mColumns;
                if (startPos < 0)
                    startPos = 0;
            }
        }
    }

    if (!mFractionalRows && mItemSpacing.y < mVerticalMargin)
        loadItems += mColumns;

    for (int i {startPos}; i < size(); ++i) {
        if (loadedItems == loadItems)
            break;
        ++loadedItems;
        if (i == mCursor || i == mLastCursor)
            continue;
        renderEntries.emplace_back(i);
    }

    if (mLastCursor >= startPos && mLastCursor < startPos + loadItems)
        renderEntries.emplace_back(mLastCursor);
    if (mLastCursor != mCursor)
        renderEntries.emplace_back(mCursor);

    float scale {1.0f};
    float opacity {1.0f};
    float saturation {1.0f};
    float dimming {1.0f};

    trans[3].y -= (mItemSize.y + mItemSpacing.y) * mScrollPos;

    auto calculateOffsetPos = [this](const glm::vec3& itemPos, const glm::vec2& origin,
                                     const float scale,
                                     const float relativeScale) -> const glm::vec2 {
        const float sizeX {mItemSize.x * scale * relativeScale};
        const float sizeY {mItemSize.y * scale * relativeScale};
        glm::vec2 position {0.0f, 0.0f};

        position.x = itemPos.x - (mItemSize.x / 2.0f);
        position.x -= ((mItemSize.x * scale) - mItemSize.x) * origin.x;
        position.x += ((mItemSize.x * scale) - sizeX) / 2.0f;

        position.y = itemPos.y - (mItemSize.y / 2.0f);
        position.y -= ((mItemSize.y * scale) - mItemSize.y) * origin.y;
        position.y += ((mItemSize.y * scale) - sizeY) / 2.0f;

        return position;
    };

    auto selectorRenderFunc = [this, &trans, calculateOffsetPos](
                                  std::vector<size_t>::const_iterator it, const glm::vec3& itemPos,
                                  const float scale, glm::vec2 origin, glm::vec2 offset,
                                  const float opacity) {
        if (mSelectorImage == nullptr && !mHasSelectorColor)
            return;

        float selectorSizeX {mItemSize.x * scale * mSelectorRelativeScale};
        float selectorSizeY {mItemSize.y * scale * mSelectorRelativeScale};
        glm::vec2 position {calculateOffsetPos(itemPos, origin, scale, mSelectorRelativeScale)};

        if (mSelectorImageOnly) {
            const auto imageItem {std::dynamic_pointer_cast<ImageComponent>(mEntries.at(*it).data.item)};
            if (!imageItem)
                return;

            const glm::vec2 imageSize {imageItem->getSize()};
            if (imageSize.x <= 0.0f || imageSize.y <= 0.0f)
                return;

            selectorSizeX = imageSize.x * scale * mSelectorRelativeScale;
            selectorSizeY = imageSize.y * scale * mSelectorRelativeScale;
            position = glm::vec2 {itemPos.x - selectorSizeX / 2.0f,
                                  itemPos.y - selectorSizeY / 2.0f};
        }

        if (mSelectorImage != nullptr) {
            mSelectorImage->setOrigin(0.0f, 0.0f);
            mSelectorImage->setResize(selectorSizeX, selectorSizeY);
            mSelectorImage->setPosition(position.x, position.y);
            mSelectorImage->setScale(scale);
            mSelectorImage->setOpacity(opacity);
            mSelectorImage->render(trans);
        }
        else if (mHasSelectorColor) {
            // If a selector color is set but no selector image, then render a rectangle.
            const glm::mat4 drawTrans {
                glm::translate(trans, glm::round(glm::vec3 {position.x, position.y, 0.0f}))};
            mRenderer->setMatrix(drawTrans);
            mRenderer->drawRect(0.0f, 0.0f, selectorSizeX, selectorSizeY, mSelectorColor,
                                mSelectorColorEnd,
                                mSelectorColorGradientHorizontal, opacity, 1.0f,
                                Renderer::BlendFactor::SRC_ALPHA,
                                Renderer::BlendFactor::ONE_MINUS_SRC_ALPHA, mSelectorCornerRadius);
        }
    };

    auto renderTileOverlay = [this, &trans](TileOverlayStyle& style,
                                            const glm::vec3& itemPos,
                                            const float scale,
                                            const std::string& dynamicPath,
                                            const float opacity) {
        if (!style.enabled || style.image == nullptr)
            return;

        if (style.dynamicMarquee) {
            if (dynamicPath.empty())
                return;

            if (style.cachedPath != dynamicPath) {
                if (Utils::FileSystem::exists(dynamicPath) &&
                    !Utils::FileSystem::isDirectory(dynamicPath)) {
                    style.image->setImage(dynamicPath);
                    style.cachedPath = dynamicPath;
                }
                else {
                    return;
                }
            }
        }

        const glm::vec2 tileSizePx {mItemSize.x * scale, mItemSize.y * scale};
        glm::vec2 overlaySize {
            (std::abs(style.size.x) <= 1.0f ? style.size.x * tileSizePx.x : style.size.x),
            (std::abs(style.size.y) <= 1.0f ? style.size.y * tileSizePx.y : style.size.y)};

        overlaySize = glm::max(overlaySize, glm::vec2 {1.0f, 1.0f});

        const glm::vec2 tileTopLeft {itemPos.x - tileSizePx.x / 2.0f,
                                     itemPos.y - tileSizePx.y / 2.0f};
        const glm::vec2 overlayPos {
            tileTopLeft.x + (std::abs(style.pos.x) <= 1.0f ? style.pos.x * tileSizePx.x : style.pos.x),
            tileTopLeft.y + (std::abs(style.pos.y) <= 1.0f ? style.pos.y * tileSizePx.y : style.pos.y)};

        style.image->setOrigin(0.0f, 0.0f);
        style.image->setResize(overlaySize.x, overlaySize.y);
        style.image->setPosition(overlayPos.x, overlayPos.y);
        style.image->setOpacity(opacity);
        style.image->render(trans);
    };

    auto applyImageFitToComponent = [this](const std::shared_ptr<GuiComponent>& component,
                                           const ImageFit fitMode) {
        auto imageItem = std::dynamic_pointer_cast<ImageComponent>(component);
        if (!imageItem)
            return;

        if (fitMode == ImageFit::CONTAIN) {
            imageItem->setMaxSize(glm::round(mItemSize * mImageRelativeScale));
        }
        else if (fitMode == ImageFit::FILL) {
            imageItem->setResize(glm::round(mItemSize * mImageRelativeScale));
        }
        else if (fitMode == ImageFit::COVER) {
            imageItem->setCropPos(mImageCropPos);
            imageItem->setCroppedSize(glm::round(mItemSize * mImageRelativeScale));
        }
    };

    for (auto it = renderEntries.cbegin(); it != renderEntries.cend(); ++it) {
        float metadataOpacity {1.0f};
        bool cursorEntry {false};

        if constexpr (std::is_same_v<T, FileData*>) {
            // If a game is marked as hidden, lower the opacity a lot.
            // If a game is marked to not be counted, lower the opacity a moderate amount.
            if (mEntries.at(*it).object->getHidden())
                metadataOpacity = 0.4f;
            else if (!mEntries.at(*it).object->getCountAsGame())
                metadataOpacity = 0.7f;
        }

        opacity = mUnfocusedItemOpacity * metadataOpacity;
        if (mHasUnfocusedItemSaturation)
            saturation = mUnfocusedItemSaturation;
        dimming = mUnfocusedItemDimming;

        if (*it == static_cast<size_t>(mCursor)) {
            cursorEntry = true;
            scale = glm::mix(1.0f, mItemScale, mTransitionFactor);
            opacity = glm::mix(mUnfocusedItemOpacity * metadataOpacity, 1.0f * metadataOpacity,
                               mTransitionFactor);
            if (mHasUnfocusedItemSaturation)
                saturation =
                    glm::mix(mUnfocusedItemSaturation, mImageSaturation, mTransitionFactor);
            dimming = glm::mix(mUnfocusedItemDimming, 1.0f, mTransitionFactor);
        }
        else if (*it == static_cast<size_t>(mLastCursor)) {
            scale = glm::mix(mItemScale, 1.0f, mTransitionFactor);
            opacity = glm::mix(1.0f * metadataOpacity, mUnfocusedItemOpacity * metadataOpacity,
                               mTransitionFactor);
            if (mHasUnfocusedItemSaturation)
                saturation =
                    glm::mix(mImageSaturation, mUnfocusedItemSaturation, mTransitionFactor);
            dimming = glm::mix(1.0f, mUnfocusedItemDimming, mTransitionFactor);
        }

        const glm::vec3 itemPos {mEntries.at(*it).data.item->getPosition()};
        glm::vec2 originInwards {0.5f, 0.5f};
        glm::vec2 offsetInwards {0.0f, 0.0f};

        const ImageFit activeFit {
            cursorEntry && mHasSelectedImageFit ? mSelectedImagefit : mImagefit};
        applyImageFitToComponent(mEntries.at(*it).data.item, activeFit);

        if (mScaleInwards && scale != 1.0f) {
            if (static_cast<int>(*it) < mColumns) {
                // First row.
                originInwards.y = 0.0f;
                offsetInwards.y = mItemSize.y / 2.0f;
            }
            if ((itemPos.y + (mItemSize.y / 2.0f) * mItemScale) > mSize.y) {
                // Scaled image won't fit vertically at the bottom.
                originInwards.y = 1.0f;
                offsetInwards.y = -(mItemSize.y / 2.0f);
            }
            if (static_cast<int>(*it) % mColumns == 0) {
                // Leftmost column.
                originInwards.x = 0.0f;
                offsetInwards.x = mItemSize.x / 2.0f;
            }
            if (static_cast<int>(*it) % mColumns == mColumns - 1) {
                // Rightmost column.
                originInwards.x = 1.0f;
                offsetInwards.x = -(mItemSize.x / 2.0f);
            }

            const bool textEntry {mEntries.at(*it).data.imagePath.empty() &&
                                  mEntries.at(*it).data.defaultImagePath.empty()};
            const glm::vec2 position {
                calculateOffsetPos(itemPos, originInwards, scale,
                                   (textEntry ? mTextRelativeScale : mImageRelativeScale))};
            const glm::vec2 offset {textEntry ? glm::vec2 {0.0f, 0.0f} :
                                                mItemSize * mImageRelativeScale * scale};
            if (textEntry)
                mEntries.at(*it).data.item->setOrigin(0.0f, 0.0f);
            mEntries.at(*it).data.item->setPosition(position.x, position.y);
            mEntries.at(*it).data.item->setPosition(position.x + (offset.x / 2.0f),
                                                    position.y + (offset.y / 2.0f));
        }

        if (cursorEntry && mSelectorLayer == SelectorLayer::BOTTOM)
            selectorRenderFunc(it, itemPos, scale, originInwards, offsetInwards, opacity);

        glm::vec2 backgroundPos {
            calculateOffsetPos(itemPos, originInwards, scale, mBackgroundRelativeScale)};

        if (mBackgroundImage != nullptr) {
            if (mScaleInwards && scale != 1.0f) {
                mBackgroundImage->setOrigin(0.0f, 0.0f);
                mBackgroundImage->setPosition(backgroundPos.x, backgroundPos.y);
            }
            else {
                mBackgroundImage->setPosition(mEntries.at(*it).data.item->getPosition());
            }
            mBackgroundImage->setScale(scale);
            mBackgroundImage->setOpacity(opacity);
            if (mHasUnfocusedItemSaturation)
                mBackgroundImage->setSaturation(saturation);
            if (mUnfocusedItemDimming != 1.0f)
                mBackgroundImage->setDimming(dimming);
            mBackgroundImage->render(trans);
            if (mScaleInwards && scale != 1.0f)
                mBackgroundImage->setOrigin(0.5f, 0.5f);
        }
        else if (mHasBackgroundColor) {
            // If a background color is set but no background image, then render a rectangle.
            const float sizeX {mItemSize.x * scale * mBackgroundRelativeScale};
            const float sizeY {mItemSize.y * scale * mBackgroundRelativeScale};
            const glm::mat4 drawTrans {glm::translate(
                trans, glm::round(glm::vec3 {backgroundPos.x, backgroundPos.y, 0.0f}))};
            mRenderer->setMatrix(drawTrans);
            // Batocera compatibility: use selected background color for the cursor tile.
            const unsigned int bgColor {(cursorEntry && mHasSelectedBackgroundColor) ?
                                            mSelectedBackgroundColor :
                                            mBackgroundColor};
            const unsigned int bgColorEnd {(cursorEntry && mHasSelectedBackgroundColor) ?
                                               mSelectedBackgroundColorEnd :
                                               mBackgroundColorEnd};
            const float bgCornerRadius {
                (cursorEntry && mSelectedBackgroundCornerRadius > 0.0f) ?
                    mSelectedBackgroundCornerRadius :
                    mBackgroundCornerRadius};
            mRenderer->drawRect(
                0.0f, 0.0f, sizeX, sizeY, bgColor, bgColorEnd,
                mBackgroundColorGradientHorizontal, opacity, 1.0f, Renderer::BlendFactor::SRC_ALPHA,
                Renderer::BlendFactor::ONE_MINUS_SRC_ALPHA, bgCornerRadius);
        }

        if (cursorEntry && mSelectedBackgroundNinePatch != nullptr) {
            const glm::vec2 patchSize {mItemSize * scale * mBackgroundRelativeScale};
            const glm::vec2 cornerSize {mSelectedBackgroundNinePatch->getCornerSize()};
            glm::vec2 patchPadding {-cornerSize.x * 2.0f, -cornerSize.y * 2.0f};
            if (mHasSelectedBackgroundNinePatchPadding) {
                patchPadding.x = (std::abs(mSelectedBackgroundNinePatchPadding.x) <= 1.0f) ?
                                     (mSelectedBackgroundNinePatchPadding.x * patchSize.x) :
                                     mSelectedBackgroundNinePatchPadding.x;
                patchPadding.y = (std::abs(mSelectedBackgroundNinePatchPadding.y) <= 1.0f) ?
                                     (mSelectedBackgroundNinePatchPadding.y * patchSize.y) :
                                     mSelectedBackgroundNinePatchPadding.y;
            }
            mSelectedBackgroundNinePatch->fitTo(
                patchSize, glm::vec3 {backgroundPos.x, backgroundPos.y, 0.0f},
                patchPadding);
            mSelectedBackgroundNinePatch->setOpacity(opacity);
            mSelectedBackgroundNinePatch->render(trans);
        }

        if (cursorEntry && mSelectorLayer == SelectorLayer::MIDDLE)
            selectorRenderFunc(it, itemPos, scale, originInwards, offsetInwards, opacity);

        mEntries.at(*it).data.item->setScale(scale);
        mEntries.at(*it).data.item->setOpacity(opacity);
        if (mHasUnfocusedItemSaturation)
            mEntries.at(*it).data.item->setSaturation(saturation);
        if (mUnfocusedItemDimming != 1.0f)
            mEntries.at(*it).data.item->setDimming(dimming);
        if (cursorEntry && (mHasTextSelectedColor || mHasImageSelectedColor)) {
            if (mHasTextSelectedColor && mEntries.at(*it).data.imagePath == "" &&
                mEntries.at(*it).data.defaultImagePath == "") {
                mEntries.at(*it).data.item->setColor(mTextSelectedColor);
                if (mTextSelectedBackgroundColor != mTextBackgroundColor)
                    mEntries.at(*it).data.item->setBackgroundColor(mTextSelectedBackgroundColor);
                mEntries.at(*it).data.item->render(trans);
                mEntries.at(*it).data.item->setColor(mTextColor);
                if (mTextSelectedBackgroundColor != mTextBackgroundColor)
                    mEntries.at(*it).data.item->setBackgroundColor(mTextBackgroundColor);
            }
            else if (mHasImageSelectedColor) {
                mEntries.at(*it).data.item->setColorShift(mImageSelectedColor);
                if (mImageSelectedColorEnd != mImageSelectedColor)
                    mEntries.at(*it).data.item->setColorShiftEnd(mImageSelectedColorEnd);
                if (mImageSelectedColorGradientHorizontal != mImageColorGradientHorizontal)
                    mEntries.at(*it).data.item->setColorGradientHorizontal(
                        mImageSelectedColorGradientHorizontal);
                mEntries.at(*it).data.item->render(trans);
                if (mImageSelectedColorGradientHorizontal != mImageColorGradientHorizontal)
                    mEntries.at(*it).data.item->setColorGradientHorizontal(
                        mImageColorGradientHorizontal);
                mEntries.at(*it).data.item->setColorShift(mImageColor);
                if (mImageColorEnd != mImageColor)
                    mEntries.at(*it).data.item->setColorShiftEnd(mImageColorEnd);
            }
            else {
                mEntries.at(*it).data.item->render(trans);
            }
        }
        else {
            mEntries.at(*it).data.item->render(trans);
        }
        mEntries.at(*it).data.item->setScale(1.0f);
        mEntries.at(*it).data.item->setOpacity(1.0f);

        if constexpr (std::is_same_v<T, FileData*>) {
            const bool isFavorite {mEntries.at(*it).object != nullptr &&
                                   mEntries.at(*it).object->getFavorite()};
            const bool isCompleted {
                mEntries.at(*it).object != nullptr &&
                mEntries.at(*it).object->metadata.get("completed") == "true"};
            const std::string marqueePath {
                mEntries.at(*it).object != nullptr ? mEntries.at(*it).object->getMarqueePath() :
                                                    std::string {}};

            struct OverlayRenderCandidate {
                TileOverlayStyle* style;
                std::string dynamicPath;
            };

            std::vector<OverlayRenderCandidate> overlaysToRender;
            overlaysToRender.reserve(4);

            auto queuePreferredStyle =
                [&overlaysToRender, cursorEntry, isFavorite, isCompleted](
                    TileOverlayStyle& defaultStyle, TileOverlayStyle& selectedStyle,
                    const std::string& dynamicPath) {
                    auto styleEligible = [cursorEntry, isFavorite, isCompleted](
                                             const TileOverlayStyle& style) {
                        if (!style.enabled)
                            return false;
                        if (style.onlySelected && !cursorEntry)
                            return false;
                        if (style.favoriteOnly && !isFavorite)
                            return false;
                        if (style.completedOnly && !isCompleted)
                            return false;
                        return true;
                    };

                    if (cursorEntry && styleEligible(selectedStyle)) {
                        overlaysToRender.emplace_back(
                            OverlayRenderCandidate {&selectedStyle, dynamicPath});
                    }
                    else if (styleEligible(defaultStyle)) {
                        overlaysToRender.emplace_back(
                            OverlayRenderCandidate {&defaultStyle, dynamicPath});
                    }
                };

            queuePreferredStyle(mOverlayStyle, mOverlaySelectedStyle, "");
            queuePreferredStyle(mFavoriteStyle, mFavoriteSelectedStyle, "");
            queuePreferredStyle(mMarqueeStyle, mMarqueeSelectedStyle, marqueePath);
            queuePreferredStyle(mCheevosStyle, mCheevosSelectedStyle, "");

            std::stable_sort(overlaysToRender.begin(), overlaysToRender.end(),
                             [](const OverlayRenderCandidate& a,
                                const OverlayRenderCandidate& b) {
                                 const float az {a.style != nullptr && a.style->image != nullptr ?
                                                     a.style->image->getZIndex() :
                                                     0.0f};
                                 const float bz {b.style != nullptr && b.style->image != nullptr ?
                                                     b.style->image->getZIndex() :
                                                     0.0f};
                                 return az < bz;
                             });

            for (auto& candidate : overlaysToRender) {
                renderTileOverlay(*candidate.style, itemPos, scale, candidate.dynamicPath,
                                  opacity);
            }
        }

        if (cursorEntry && mSelectorLayer == SelectorLayer::TOP)
            selectorRenderFunc(it, itemPos, scale, originInwards, offsetInwards, opacity);

        if (mScaleInwards && scale != 1.0f) {
            mEntries.at(*it).data.item->setOrigin(0.5f, 0.5f);
            mEntries.at(*it).data.item->setPosition(itemPos);
        }
    }

    mRenderer->popClipRect();
    GuiComponent::renderChildren(trans);
}

template <typename T>
void GridComponent<T>::applyTheme(const std::shared_ptr<ThemeData>& theme,
                                  const std::string& view,
                                  const std::string& element,
                                  unsigned int properties)
{
    mSize.x = Renderer::getScreenWidth();
    mSize.y = Renderer::getScreenHeight() * 0.8f;
    GuiComponent::mPosition.x = 0.0f;
    GuiComponent::mPosition.y = Renderer::getScreenHeight() * 0.1f;

    // Reset to baseline defaults so omitted theme properties do not inherit stale values.
    mItemSize = glm::vec2 {mRenderer->getScreenWidth() * 0.15f,
                           mRenderer->getScreenHeight() * 0.25f};
    mItemScale = 1.05f;
    mImageRelativeScale = 1.0f;
    mInstantItemTransitions = false;
    mInstantRowTransitions = false;
    mImagefit = ImageFit::CONTAIN;
    mImageCropPos = glm::vec2 {0.5f, 0.5f};
    mImageLinearInterpolation = true;
    mImageCornerRadius = 0.0f;
    mImageColor = 0xFFFFFFFF;
    mImageColorEnd = 0xFFFFFFFF;
    mImageColorGradientHorizontal = true;
    mImageSelectedColor = 0xFFFFFFFF;
    mImageSelectedColorEnd = 0xFFFFFFFF;
    mImageSelectedColorGradientHorizontal = true;
    mImageBrightness = 0.0f;
    mImageSaturation = 1.0f;
    mBackgroundRelativeScale = 1.0f;
    mBackgroundCornerRadius = 0.0f;
    mSelectedBackgroundCornerRadius = 0.0f;
    mBackgroundColor = 0xFFFFFFFF;
    mBackgroundColorEnd = 0xFFFFFFFF;
    mBackgroundColorGradientHorizontal = true;
    mSelectedBackgroundColor = 0xFFFFFFFF;
    mSelectedBackgroundColorEnd = 0xFFFFFFFF;
    mSelectorRelativeScale = 1.0f;
    mSelectorLayer = SelectorLayer::TOP;
    mSelectorCornerRadius = 0.0f;
    mSelectorColor = 0xFFFFFFFF;
    mSelectorColorEnd = 0xFFFFFFFF;
    mSelectorColorGradientHorizontal = true;
    mTextRelativeScale = 1.0f;
    mTextBackgroundCornerRadius = 0.0f;
    mTextColor = 0x000000FF;
    mTextBackgroundColor = 0xFFFFFF00;
    mTextSelectedColor = 0x000000FF;
    mTextSelectedBackgroundColor = 0xFFFFFF00;
    mTextHorizontalScrolling = false;
    mTextHorizontalScrollSpeed = 1.0f;
    mTextHorizontalScrollDelay = 3000.0f;
    mTextHorizontalScrollGap = 1.5f;
    mLineSpacing = 1.5f;
    mLetterCase = LetterCase::NONE;
    mLetterCaseAutoCollections = LetterCase::UNDEFINED;
    mLetterCaseCustomCollections = LetterCase::UNDEFINED;
    mSystemNameSuffix = true;
    mLetterCaseSystemNameSuffix = LetterCase::UPPERCASE;
    mFadeAbovePrimary = false;
    mUnfocusedItemOpacity = 1.0f;
    mUnfocusedItemSaturation = 1.0f;
    mUnfocusedItemDimming = 1.0f;
    mImageTypes.clear();
    mCenterSelection = false;
    mScrollLoop = false;
    mScrollDirection = ScrollDirection::VERTICAL;
    mAutoLayoutColumns = 0;
    mAutoLayoutRows = 0;

    mItemSpacing.x = ((mItemSize.x * mItemScale) - mItemSize.x) / 2.0f;
    mItemSpacing.y = ((mItemSize.y * mItemScale) - mItemSize.y) / 2.0f;
    mHorizontalMargin = ((mItemSize.x * mItemScale) - mItemSize.x) / 2.0f;
    mVerticalMargin = ((mItemSize.y * mItemScale) - mItemSize.y) / 2.0f;
    mPadding = {0.0f, 0.0f, 0.0f, 0.0f};
    GuiComponent::mThemeOpacity = 1.0f;
    mSelectorImageOnly = false;
    mHasUnfocusedItemSaturation = false;
    mHasImageSelectedColor = false;
    mHasTextSelectedColor = false;
    mHasSelectedImageFit = false;
    mSelectedImagefit = mImagefit;
    mSelectedBackgroundNinePatchPadding = {0.0f, 0.0f};
    mHasSelectedBackgroundNinePatchPadding = false;
    mBackgroundImage.reset();
    mSelectedBackgroundNinePatch.reset();
    mSelectorImage.reset();
    mBackgroundImagePath.clear();
    mSelectorImagePath.clear();
    mOverlayStyle = {};
    mOverlaySelectedStyle = {};
    mFavoriteStyle = {};
    mFavoriteSelectedStyle = {};
    mMarqueeStyle = {};
    mMarqueeSelectedStyle = {};
    mCheevosStyle = {};
    mCheevosSelectedStyle = {};

    GuiComponent::applyTheme(theme, view, element, properties);

    using namespace ThemeFlags;
    const ThemeData::ThemeElement* elem {theme->getElement(view, element, "grid")};

    if (!elem)
        return;

    if (mGamelistView && properties && (elem->has("imageType") || elem->has("imageSource"))) {
        const std::vector<std::string> supportedImageTypes {
            "marquee",    "cover",       "backcover", "3dbox",  "physicalmedia",
            "screenshot", "titlescreen", "miximage",  "fanart", "none"};
        const bool useImageSource {elem->has("imageSource")};
        const std::string propertyName {useImageSource ? "imageSource" : "imageType"};
        std::string imageTypesString {elem->get<std::string>(propertyName)};

        for (auto& character : imageTypesString) {
            if (std::isspace(character))
                character = ',';
        }
        imageTypesString = Utils::String::replace(imageTypesString, ",,", ",");
        mImageTypes = Utils::String::delimitedStringToVector(imageTypesString, ",");

        // Batocera compatibility aliases for logo-centric image source names.
        for (std::string& type : mImageTypes) {
            if (type == "logo" || type == "wheel" || type == "logos" || type == "wheels")
                type = "marquee";
        }

        // Only allow two imageType entries due to performance reasons.
        if (mImageTypes.size() > 2)
            mImageTypes.erase(mImageTypes.begin() + 2, mImageTypes.end());

        if (mImageTypes.empty()) {
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property \""
                            << propertyName << "\" "
                               "for element \""
                            << element.substr(5) << "\" contains no values";
        }

        for (std::string& type : mImageTypes) {
            if (std::find(supportedImageTypes.cbegin(), supportedImageTypes.cend(), type) ==
                supportedImageTypes.cend()) {
                LOG(LogWarning)
                    << "GridComponent: Invalid theme configuration, property \""
                    << propertyName << "\" "
                       "for element \""
                    << element.substr(5) << "\" defined as \"" << type << "\"";
                mImageTypes.clear();
                break;
            }
        }

        if (mImageTypes.size() == 2 && mImageTypes.front() == mImageTypes.back()) {
            LOG(LogError) << "GridComponent: Invalid theme configuration, property \""
                          << propertyName << "\" "
                             "for element \""
                          << element.substr(5) << "\" contains duplicate values";
            mImageTypes.clear();
        }
    }

    mFractionalRows = (elem->has("fractionalRows") && elem->get<bool>("fractionalRows"));

    if (elem->has("itemSize")) {
        const glm::vec2& itemSize {elem->get<glm::vec2>("itemSize")};
        if (!(itemSize.x == -1 && itemSize.y == -1)) {
            if (itemSize.x == -1) {
                mItemSize.y =
                    glm::clamp(itemSize.y, 0.05f, 1.0f) * (mRenderer->getIsVerticalOrientation() ?
                                                               mRenderer->getScreenWidth() :
                                                               mRenderer->getScreenHeight());
                mItemSize.x = mItemSize.y;
            }
            else if (itemSize.y == -1) {
                mItemSize.x = glm::clamp(itemSize.x, 0.05f, 1.0f) * mRenderer->getScreenWidth();
                mItemSize.y = mItemSize.x;
            }
            else {
                mItemSize = glm::clamp(itemSize, 0.05f, 1.0f) *
                            glm::vec2(mRenderer->getScreenWidth(), mRenderer->getScreenHeight());
            }
        }
    }

    if (elem->has("itemScale"))
        mItemScale = glm::clamp(elem->get<float>("itemScale"), 0.5f, 2.0f);
    else if (elem->has("autoLayoutSelectedZoom"))
        mItemScale = glm::clamp(elem->get<float>("autoLayoutSelectedZoom"), 0.5f, 3.0f);

    if (elem->has("centerSelection"))
        mCenterSelection = elem->get<bool>("centerSelection");

    if (elem->has("scrollLoop"))
        mScrollLoop = elem->get<bool>("scrollLoop");

    auto normalizeToken = [](const std::string& raw) {
        std::string normalized {raw};
        normalized.erase(
            std::remove_if(normalized.begin(), normalized.end(),
                           [](unsigned char c) { return std::isspace(c) != 0; }),
            normalized.end());
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return normalized;
    };

    if (elem->has("scrollDirection")) {
        const std::string scrollDirection {
            normalizeToken(elem->get<std::string>("scrollDirection"))};
        if (scrollDirection == "horizontal")
            mScrollDirection = ScrollDirection::HORIZONTAL;
        else if (scrollDirection == "vertical")
            mScrollDirection = ScrollDirection::VERTICAL;
        else
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"scrollDirection\" for element \""
                            << element.substr(5) << "\" defined as \"" << scrollDirection
                            << "\"";
    }

    if (elem->has("autoLayout")) {
        const glm::vec2 autoLayout {
            glm::max(elem->get<glm::vec2>("autoLayout"), glm::vec2 {1.0f, 1.0f})};
        mAutoLayoutColumns = static_cast<int>(std::round(autoLayout.x));
        mAutoLayoutRows = static_cast<int>(std::round(autoLayout.y));
    }

    if (elem->has("imageRelativeScale"))
        mImageRelativeScale = glm::clamp(elem->get<float>("imageRelativeScale"), 0.2f, 1.0f);

    mScaleInwards =
        (mItemScale > 1.0f && elem->has("scaleInwards") && elem->get<bool>("scaleInwards"));

    if (elem->has("imageFit")) {
        const std::string& imageFit {elem->get<std::string>("imageFit")};
        const std::string normalizedImageFit {normalizeToken(imageFit)};
        if (normalizedImageFit == "contain") {
            mImagefit = ImageFit::CONTAIN;
        }
        else if (normalizedImageFit == "fill") {
            mImagefit = ImageFit::FILL;
        }
        else if (normalizedImageFit == "cover") {
            mImagefit = ImageFit::COVER;
            if (elem->has("imageCropPos"))
                mImageCropPos = glm::clamp(elem->get<glm::vec2>("imageCropPos"), 0.0f, 1.0f);
        }
        else {
            mImagefit = ImageFit::CONTAIN;
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"imageFit\" for element \""
                            << element.substr(5) << "\" defined as \"" << imageFit << "\"";
        }
    }

    if (elem->has("imageInterpolation")) {
        const std::string& imageInterpolation {elem->get<std::string>("imageInterpolation")};
        const std::string normalizedInterpolation {normalizeToken(imageInterpolation)};
        if (normalizedInterpolation == "linear") {
            mImageLinearInterpolation = true;
        }
        else if (normalizedInterpolation == "nearest") {
            mImageLinearInterpolation = false;
        }
        else {
            mImageLinearInterpolation = true;
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"imageInterpolation\" for element \""
                            << element.substr(5) << "\" defined as \"" << imageInterpolation
                            << "\"";
        }
    }

    if (elem->has("backgroundRelativeScale"))
        mBackgroundRelativeScale =
            glm::clamp(elem->get<float>("backgroundRelativeScale"), 0.2f, 1.0f);

    mHasBackgroundColor = false;
    mHasSelectedBackgroundColor = false;

    if (elem->has("backgroundColor")) {
        mHasBackgroundColor = true;
        mBackgroundColor = elem->get<unsigned int>("backgroundColor");
        mBackgroundColorEnd = mBackgroundColor;
    }
    if (elem->has("backgroundColorEnd"))
        mBackgroundColorEnd = elem->get<unsigned int>("backgroundColorEnd");

    if (elem->has("backgroundGradientType")) {
        const std::string& gradientType {elem->get<std::string>("backgroundGradientType")};
        const std::string normalizedGradientType {normalizeToken(gradientType)};
        if (normalizedGradientType == "horizontal") {
            mBackgroundColorGradientHorizontal = true;
        }
        else if (normalizedGradientType == "vertical") {
            mBackgroundColorGradientHorizontal = false;
        }
        else {
            mBackgroundColorGradientHorizontal = true;
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"backgroundGradientType\" for element \""
                            << element.substr(5) << "\" defined as \"" << gradientType << "\"";
        }
    }

    if (elem->has("selectorRelativeScale"))
        mSelectorRelativeScale = glm::clamp(elem->get<float>("selectorRelativeScale"), 0.2f, 1.0f);

    mHasSelectorColor = false;

    if (elem->has("selectorColor")) {
        mHasSelectorColor = true;
        mSelectorColor = elem->get<unsigned int>("selectorColor");
        mSelectorColorEnd = mSelectorColor;
    }
    if (elem->has("selectorColorEnd"))
        mSelectorColorEnd = elem->get<unsigned int>("selectorColorEnd");

    if (elem->has("selectorGradientType")) {
        const std::string& gradientType {elem->get<std::string>("selectorGradientType")};
        const std::string normalizedGradientType {normalizeToken(gradientType)};
        if (normalizedGradientType == "horizontal") {
            mSelectorColorGradientHorizontal = true;
        }
        else if (normalizedGradientType == "vertical") {
            mSelectorColorGradientHorizontal = false;
        }
        else {
            mSelectorColorGradientHorizontal = true;
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"selectorGradientType\" for element \""
                            << element.substr(5) << "\" defined as \"" << gradientType << "\"";
        }
    }

    if (elem->has("backgroundImage")) {
        const std::string& path {elem->get<std::string>("backgroundImage")};
        if (Utils::FileSystem::exists(path) && !Utils::FileSystem::isDirectory(path)) {
            mBackgroundImage = std::make_unique<ImageComponent>(false, false);
            mBackgroundImage->setLinearInterpolation(true);
            mBackgroundImage->setMipmapping(true);
            mBackgroundImage->setResize(mItemSize * mBackgroundRelativeScale);
            mBackgroundImage->setOrigin(0.5f, 0.5f);
            if (mHasBackgroundColor) {
                mBackgroundImage->setColorShift(mBackgroundColor);
                if (mBackgroundColor != mBackgroundColorEnd) {
                    mBackgroundImage->setColorShiftEnd(mBackgroundColorEnd);
                    if (!mBackgroundColorGradientHorizontal)
                        mBackgroundImage->setColorGradientHorizontal(false);
                }
            }
            float backgroundCornerRadius {0.0f};
            if (elem->has("backgroundCornerRadius")) {
                backgroundCornerRadius =
                    glm::clamp(elem->get<float>("backgroundCornerRadius"), 0.0f, 0.5f) *
                    (mItemScale >= 1.0f ? mItemScale : 1.0f) * mRenderer->getScreenWidth();
            }
            mBackgroundImage->setCornerRadius(backgroundCornerRadius);
            mBackgroundImage->setImage(elem->get<std::string>("backgroundImage"));
            mBackgroundImagePath = path;
        }
        else {
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"backgroundImage\" for element \""
                            << element.substr(5) << "\", image does not exist: \"" << path << "\"";
        }
    }
    else if (elem->has("backgroundCornerRadius")) {
        mBackgroundCornerRadius =
            glm::clamp(elem->get<float>("backgroundCornerRadius"), 0.0f, 0.5f) *
            (mItemScale >= 1.0f ? mItemScale : 1.0f) * mRenderer->getScreenWidth();
    }

    if (elem->has("selectorImage")) {
        const std::string& path {elem->get<std::string>("selectorImage")};
        if (Utils::FileSystem::exists(path) && !Utils::FileSystem::isDirectory(path)) {
            mSelectorImage = std::make_unique<ImageComponent>(false, false);
            mSelectorImage->setLinearInterpolation(true);
            mSelectorImage->setMipmapping(true);
            mSelectorImage->setResize(mItemSize * mSelectorRelativeScale);
            mSelectorImage->setOrigin(0.5f, 0.5f);
            if (mHasSelectorColor) {
                mSelectorImage->setColorShift(mSelectorColor);
                if (mBackgroundColor != mBackgroundColorEnd) {
                    mSelectorImage->setColorShiftEnd(mSelectorColorEnd);
                    if (!mSelectorColorGradientHorizontal)
                        mSelectorImage->setColorGradientHorizontal(false);
                }
            }
            float selectorCornerRadius {0.0f};
            if (elem->has("selectorCornerRadius")) {
                selectorCornerRadius =
                    glm::clamp(elem->get<float>("selectorCornerRadius"), 0.0f, 0.5f) *
                    (mItemScale >= 1.0f ? mItemScale : 1.0f) * mRenderer->getScreenWidth();
            }
            mSelectorImage->setCornerRadius(selectorCornerRadius);
            mSelectorImage->setImage(elem->get<std::string>("selectorImage"));
            mSelectorImagePath = path;
        }
        else {
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"selectorImage\" for element \""
                            << element.substr(5) << "\", image does not exist: \"" << path << "\"";
        }
    }
    else if (elem->has("selectorCornerRadius")) {
        mSelectorCornerRadius = glm::clamp(elem->get<float>("selectorCornerRadius"), 0.0f, 0.5f) *
                                (mItemScale >= 1.0f ? mItemScale : 1.0f) *
                                mRenderer->getScreenWidth();
    }

    if (elem->has("selectorLayer")) {
        const std::string& selectorLayer {elem->get<std::string>("selectorLayer")};
        const std::string normalizedSelectorLayer {normalizeToken(selectorLayer)};
        if (normalizedSelectorLayer == "top") {
            mSelectorLayer = SelectorLayer::TOP;
        }
        else if (normalizedSelectorLayer == "middle") {
            mSelectorLayer = SelectorLayer::MIDDLE;
        }
        else if (normalizedSelectorLayer == "bottom") {
            mSelectorLayer = SelectorLayer::BOTTOM;
        }
        else {
            mSelectorLayer = SelectorLayer::TOP;
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"selectorLayer\" for element \""
                            << element.substr(5) << "\" defined as \"" << selectorLayer << "\"";
        }
    }

    if (elem->has("itemTransitions")) {
        const std::string& itemTransitions {elem->get<std::string>("itemTransitions")};
        const std::string normalizedItemTransitions {normalizeToken(itemTransitions)};
        if (normalizedItemTransitions == "animate") {
            mInstantItemTransitions = false;
        }
        else if (normalizedItemTransitions == "instant") {
            mInstantItemTransitions = true;
        }
        else {
            mInstantItemTransitions = false;
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"itemTransitions\" for element \""
                            << element.substr(5) << "\" defined as \"" << itemTransitions << "\"";
        }
    }

    if (elem->has("rowTransitions")) {
        const std::string& rowTransitions {elem->get<std::string>("rowTransitions")};
        const std::string normalizedRowTransitions {normalizeToken(rowTransitions)};
        if (normalizedRowTransitions == "animate") {
            mInstantRowTransitions = false;
        }
        else if (normalizedRowTransitions == "instant") {
            mInstantRowTransitions = true;
        }
        else {
            mInstantRowTransitions = false;
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"rowTransitions\" for element \""
                            << element.substr(5) << "\" defined as \"" << rowTransitions << "\"";
        }
    }

    if (!elem->has("itemTransitions") && !elem->has("rowTransitions") &&
        elem->has("animateSelection") && !elem->get<bool>("animateSelection")) {
        mInstantItemTransitions = true;
        mInstantRowTransitions = true;
    }

    // If itemSpacing is not defined, then it's automatically calculated so that scaled items
    // don't overlap. If the property is present but one axis is defined as -1 then set this
    // axis to the same pixel value as the other axis.
    if (elem->has("itemSpacing")) {
        const glm::vec2& itemSpacing {elem->get<glm::vec2>("itemSpacing")};
        if (itemSpacing.x == -1 && itemSpacing.y == -1) {
            mItemSpacing = {0.0f, 0.0f};
        }
        else if (itemSpacing.x == -1) {
            mItemSpacing.y = glm::clamp(itemSpacing.y, 0.0f, 0.1f) * mRenderer->getScreenHeight();
            mItemSpacing.x = mItemSpacing.y;
        }
        else if (itemSpacing.y == -1) {
            mItemSpacing.x = glm::clamp(itemSpacing.x, 0.0f, 0.1f) * mRenderer->getScreenWidth();
            mItemSpacing.y = mItemSpacing.x;
        }
        else {
            mItemSpacing = glm::clamp(itemSpacing, 0.0f, 0.1f) *
                           glm::vec2(Renderer::getScreenWidth(), Renderer::getScreenHeight());
        }
    }
    else if (mItemScale < 1.0f) {
        mItemSpacing = glm::vec2 {0.0f, 0.0f};
    }
    else if (elem->has("margin")) {
        // Batocera compatibility: imagegrid margin acts as inter-item spacing fallback.
        const glm::vec2 margin {
            glm::clamp(elem->get<glm::vec2>("margin"), 0.0f, 0.1f)};
        mItemSpacing = glm::vec2 {margin.x * mRenderer->getScreenWidth(),
                                  margin.y * mRenderer->getScreenHeight()};
    }
    else {
        mItemSpacing.x = ((mItemSize.x * mItemScale) - mItemSize.x) / 2.0f;
        mItemSpacing.y = ((mItemSize.y * mItemScale) - mItemSize.y) / 2.0f;
    }

    auto applyGridPadding = [this](const glm::vec4& padding) {
        auto axisToPixels = [this](const float value, const bool horizontal) {
            if (std::abs(value) <= 1.0f)
                return value * (horizontal ? mRenderer->getScreenWidth() :
                                             mRenderer->getScreenHeight());
            return value;
        };

        mPadding.x = axisToPixels(padding.x, true);
        mPadding.y = axisToPixels(padding.y, false);
        mPadding.z = axisToPixels(padding.z, true);
        mPadding.w = axisToPixels(padding.w, false);
        mPadding = glm::max(mPadding, glm::vec4 {0.0f, 0.0f, 0.0f, 0.0f});
    };

    auto applyGridTileSize = [this](const glm::vec2& tileSize) {
        // Accept both normalized values (<= 1.0) and absolute pixel values (> 1.0).
        auto axisToPixels = [this](const float value, const float reference) {
            if (value <= 0.0f)
                return -1.0f;
            if (value <= 1.0f)
                return value * reference;
            return value;
        };

        const float sizeX {axisToPixels(tileSize.x, mRenderer->getScreenWidth())};
        const float sizeY {axisToPixels(tileSize.y, mRenderer->getScreenHeight())};

        if (sizeX > 0.0f && sizeY > 0.0f)
            mItemSize = glm::vec2 {sizeX, sizeY};
        else if (sizeX > 0.0f)
            mItemSize = glm::vec2 {sizeX, sizeX};
        else if (sizeY > 0.0f)
            mItemSize = glm::vec2 {sizeY, sizeY};
    };

    auto gridCornerSizeToRadius = [this](const glm::vec2& cornerSize) {
        auto axisToPixels = [this](const float value) {
            if (value <= 0.0f)
                return 0.0f;
            if (value <= 1.0f)
                return value * mRenderer->getScreenWidth();
            return value;
        };

        const float radius {std::max(axisToPixels(cornerSize.x), axisToPixels(cornerSize.y))};
        return glm::clamp(radius, 0.0f, mRenderer->getScreenWidth() * 0.5f);
    };

    auto applySelectionMode = [this, &element](const std::string& selectionMode,
                                                const std::string& propertySource) {
        std::string normalizedMode {selectionMode};
        normalizedMode.erase(
            std::remove_if(normalizedMode.begin(), normalizedMode.end(),
                           [](unsigned char c) { return std::isspace(c) != 0; }),
            normalizedMode.end());
        std::transform(normalizedMode.begin(), normalizedMode.end(), normalizedMode.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (normalizedMode == "full" || normalizedMode == "tile") {
            mSelectorImageOnly = false;
        }
        else if (normalizedMode == "image") {
            mSelectorImageOnly = true;
        }
        else {
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property \""
                            << propertySource << "\" for element \"" << element.substr(5)
                            << "\" defined as \"" << selectionMode << "\"";
        }
    };

    if (elem->has("padding"))
        applyGridPadding(elem->get<glm::vec4>("padding"));

    if (elem->has("selectionMode"))
        applySelectionMode(elem->get<std::string>("selectionMode"), "selectionMode");

    // Batocera compatibility: read default/selected gridtile fallback configuration.
    const ThemeData::ThemeElement* defaultGridTile {theme->getElement(view, "default", "gridtile")};
    const ThemeData::ThemeElement* selectedGridTile {
        theme->getElement(view, "selected", "gridtile")};
    const ThemeData::ThemeElement* selectedGridTileBackground {
        theme->getElement(view, "gridtile.background:selected", "ninepatch")};
    const bool useDefaultGridTile {
        defaultGridTile != nullptr &&
        (!defaultGridTile->has("visible") || defaultGridTile->get<bool>("visible"))};
    const bool useSelectedGridTile {
        selectedGridTile != nullptr &&
        (!selectedGridTile->has("visible") || selectedGridTile->get<bool>("visible"))};
    bool appliedGridTileDefaultSize {false};
    bool derivedGridTileItemScale {false};

    // Deferred gridtile.selected overrides — must be applied AFTER native reset assignments.
    unsigned int gridTileSelectedImageColor {0};
    bool hasGridTileSelectedImageColorFallback {false};
    unsigned int gridTileSelectedTextColor {0};
    bool hasGridTileSelectedTextColorFallback {false};
    unsigned int gridTileSelectedTextBackgroundColor {0};
    bool hasGridTileSelectedTextBackgroundColorFallback {false};

    auto parseGridTileImageSizeMode = [this, &element](const std::string& imageSizeMode,
                                                       const std::string& propertySource,
                                                       ImageFit& outFit) {
        std::string normalizedMode {imageSizeMode};
        normalizedMode.erase(
            std::remove_if(normalizedMode.begin(), normalizedMode.end(),
                           [](unsigned char c) { return std::isspace(c) != 0; }),
            normalizedMode.end());
        std::transform(normalizedMode.begin(), normalizedMode.end(), normalizedMode.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (normalizedMode == "size") {
            outFit = ImageFit::FILL;
            return true;
        }
        if (normalizedMode == "minsize") {
            outFit = ImageFit::CONTAIN;
            return true;
        }
        if (normalizedMode == "maxsize") {
            outFit = ImageFit::COVER;
            return true;
        }

        LOG(LogWarning) << "GridComponent: Invalid theme configuration, property \""
                        << propertySource << "\" for element \"" << element.substr(5)
                        << "\" defined as \"" << imageSizeMode << "\"";
        return false;
    };

    if (useDefaultGridTile) {
        if (!elem->has("padding") && defaultGridTile->has("padding"))
            applyGridPadding(glm::vec4 {defaultGridTile->get<glm::vec2>("padding").x,
                                        defaultGridTile->get<glm::vec2>("padding").y,
                                        defaultGridTile->get<glm::vec2>("padding").x,
                                        defaultGridTile->get<glm::vec2>("padding").y});

        if (!elem->has("itemSize") && defaultGridTile->has("size")) {
            applyGridTileSize(defaultGridTile->get<glm::vec2>("size"));
            appliedGridTileDefaultSize = true;
        }

        if (!elem->has("selectionMode") && defaultGridTile->has("selectionMode")) {
            applySelectionMode(defaultGridTile->get<std::string>("selectionMode"),
                               "gridtile.selectionMode");
        }

        // The closest equivalent in ESTACION-PRO grid rendering is applying these as item colors.
        if (!elem->has("imageColor") && defaultGridTile->has("imageColor")) {
            mImageColor = defaultGridTile->get<unsigned int>("imageColor");
            mImageColorEnd = mImageColor;
        }

        if (!elem->has("imageFit") && defaultGridTile->has("imageSizeMode")) {
            parseGridTileImageSizeMode(defaultGridTile->get<std::string>("imageSizeMode"),
                                       "gridtile.imageSizeMode", mImagefit);
        }

        // Batocera compatibility: gridtile.default.backgroundColor sets tile background color.
        if (!elem->has("backgroundColor") && defaultGridTile->has("backgroundColor")) {
            mHasBackgroundColor = true;
            mBackgroundColor = defaultGridTile->get<unsigned int>("backgroundColor");
            mBackgroundColorEnd = mBackgroundColor;
        }

        if (!elem->has("backgroundColor") && !defaultGridTile->has("backgroundColor") &&
            defaultGridTile->has("backgroundCenterColor")) {
            mHasBackgroundColor = true;
            mBackgroundColor = defaultGridTile->get<unsigned int>("backgroundCenterColor");
            mBackgroundColorEnd = mBackgroundColor;
            if (defaultGridTile->has("backgroundEdgeColor"))
                mBackgroundColorEnd = defaultGridTile->get<unsigned int>("backgroundEdgeColor");
        }

        // Batocera compatibility: gridtile.default.textColor sets tile game-name text color.
        if (!elem->has("textColor") && defaultGridTile->has("textColor")) {
            mTextColor = defaultGridTile->get<unsigned int>("textColor");
        }

        // Batocera compatibility: gridtile.default.textBackgroundColor sets tile name background.
        if (!elem->has("textBackgroundColor") && defaultGridTile->has("textBackgroundColor")) {
            mTextBackgroundColor = defaultGridTile->get<unsigned int>("textBackgroundColor");
        }

        // Batocera compatibility: gridtile.default.imageCornerRadius rounds tile images.
        if (!elem->has("imageCornerRadius") && defaultGridTile->has("imageCornerRadius")) {
            mImageCornerRadius =
                glm::clamp(defaultGridTile->get<float>("imageCornerRadius"), 0.0f, 0.5f) *
                (mItemScale >= 1.0f ? mItemScale : 1.0f) * mRenderer->getScreenWidth();
        }

        if (!elem->has("backgroundCornerRadius") &&
            defaultGridTile->has("backgroundCornerSize")) {
            mBackgroundCornerRadius =
                gridCornerSizeToRadius(defaultGridTile->get<glm::vec2>("backgroundCornerSize"));
        }

        // Batocera compatibility: derive itemScale from gridtile.default.size vs selected.size.
        if (!elem->has("itemScale") && defaultGridTile->has("size") && useSelectedGridTile &&
            selectedGridTile->has("size")) {
            const glm::vec2 defSize {defaultGridTile->get<glm::vec2>("size")};
            const glm::vec2 selSize {selectedGridTile->get<glm::vec2>("size")};
            float scaleSum {0.0f};
            float scaleCount {0.0f};

            if (defSize.x > 0.0f && selSize.x > 0.0f) {
                scaleSum += (selSize.x / defSize.x);
                scaleCount += 1.0f;
            }

            if (defSize.y > 0.0f && selSize.y > 0.0f) {
                scaleSum += (selSize.y / defSize.y);
                scaleCount += 1.0f;
            }

            if (scaleCount > 0.0f)
                mItemScale = glm::clamp(scaleSum / scaleCount, 0.5f, 2.0f);
            if (scaleCount > 0.0f)
                derivedGridTileItemScale = true;
        }
    }

    // Batocera compatibility: if only selected size exists, use it as base tile size.
    if (!appliedGridTileDefaultSize && !elem->has("itemSize") && useSelectedGridTile &&
        selectedGridTile->has("size")) {
        applyGridTileSize(selectedGridTile->get<glm::vec2>("size"));
        appliedGridTileDefaultSize = true;
    }

    if (appliedGridTileDefaultSize || derivedGridTileItemScale) {
        mHorizontalMargin = ((mItemSize.x * mItemScale) - mItemSize.x) / 2.0f;
        mVerticalMargin = ((mItemSize.y * mItemScale) - mItemSize.y) / 2.0f;

        if (!elem->has("itemSpacing")) {
            if (mItemScale < 1.0f) {
                mItemSpacing = glm::vec2 {0.0f, 0.0f};
            }
            else {
                mItemSpacing.x = ((mItemSize.x * mItemScale) - mItemSize.x) / 2.0f;
                mItemSpacing.y = ((mItemSize.y * mItemScale) - mItemSize.y) / 2.0f;
            }
        }
    }

    if (useSelectedGridTile) {
        if (!elem->has("selectionMode") && selectedGridTile->has("selectionMode")) {
            applySelectionMode(selectedGridTile->get<std::string>("selectionMode"),
                               "gridtile.selected.selectionMode");
        }

        // Deferred: gridtile.selected.imageColor — applied after mImageSelectedColor = mImageColor.
        if (!elem->has("imageSelectedColor") && selectedGridTile->has("imageColor")) {
            gridTileSelectedImageColor = selectedGridTile->get<unsigned int>("imageColor");
            hasGridTileSelectedImageColorFallback = true;
            mHasImageSelectedColor = true;
        }

        if (selectedGridTile->has("imageSizeMode")) {
            ImageFit parsedFit {mSelectedImagefit};
            if (parseGridTileImageSizeMode(selectedGridTile->get<std::string>("imageSizeMode"),
                                           "gridtile.selected.imageSizeMode", parsedFit)) {
                mSelectedImagefit = parsedFit;
                mHasSelectedImageFit = true;
            }
        }

        // Batocera compatibility: gridtile.selected.backgroundColor for the highlighted tile.
        if (selectedGridTile->has("backgroundColor")) {
            mSelectedBackgroundColor = selectedGridTile->get<unsigned int>("backgroundColor");
            mSelectedBackgroundColorEnd = mSelectedBackgroundColor;
            mHasSelectedBackgroundColor = true;
            // Ensure default background is active so the rect is drawn for all tiles.
            if (!mHasBackgroundColor) {
                mHasBackgroundColor = true;
                mBackgroundColor = mSelectedBackgroundColor;
                mBackgroundColorEnd = mSelectedBackgroundColorEnd;
            }
        }

        if (!selectedGridTile->has("backgroundColor") &&
            selectedGridTile->has("backgroundCenterColor")) {
            mSelectedBackgroundColor =
                selectedGridTile->get<unsigned int>("backgroundCenterColor");
            mSelectedBackgroundColorEnd = mSelectedBackgroundColor;
            if (selectedGridTile->has("backgroundEdgeColor")) {
                mSelectedBackgroundColorEnd =
                    selectedGridTile->get<unsigned int>("backgroundEdgeColor");
            }
            mHasSelectedBackgroundColor = true;
            if (!mHasBackgroundColor) {
                mHasBackgroundColor = true;
                mBackgroundColor = mSelectedBackgroundColor;
                mBackgroundColorEnd = mSelectedBackgroundColorEnd;
            }
        }

        if (selectedGridTile->has("backgroundCornerSize")) {
            mSelectedBackgroundCornerRadius =
                gridCornerSizeToRadius(selectedGridTile->get<glm::vec2>("backgroundCornerSize"));
        }

        // Deferred: gridtile.selected.textColor — applied after mTextSelectedColor = mTextColor.
        if (!elem->has("textSelectedColor") && selectedGridTile->has("textColor")) {
            gridTileSelectedTextColor = selectedGridTile->get<unsigned int>("textColor");
            hasGridTileSelectedTextColorFallback = true;
            mHasTextSelectedColor = true;
        }

        // Deferred: gridtile.selected.textBackgroundColor — applied after reset.
        if (!elem->has("textSelectedBackgroundColor") &&
            selectedGridTile->has("textBackgroundColor")) {
            gridTileSelectedTextBackgroundColor =
                selectedGridTile->get<unsigned int>("textBackgroundColor");
            hasGridTileSelectedTextBackgroundColorFallback = true;
            mHasTextSelectedColor = true;
        }
    }

    if (useSelectedGridTile && selectedGridTileBackground != nullptr &&
        selectedGridTileBackground->has("path")) {
        mSelectedBackgroundNinePatch = std::make_unique<NinePatchComponent>(
            selectedGridTileBackground->get<std::string>("path"));

        if (selectedGridTileBackground->has("cornerSize")) {
            const glm::vec2 cornerSize {
                selectedGridTileBackground->get<glm::vec2>("cornerSize") *
                Renderer::getScreenWidth()};
            mSelectedBackgroundNinePatch->setCornerSize(cornerSize);
        }

        if (selectedGridTileBackground->has("color")) {
            mSelectedBackgroundNinePatch->setFrameColor(
                selectedGridTileBackground->get<unsigned int>("color"));
        }
        else if (selectedGridTileBackground->has("edgeColor")) {
            mSelectedBackgroundNinePatch->setFrameColor(
                selectedGridTileBackground->get<unsigned int>("edgeColor"));
        }

        if (selectedGridTileBackground->has("padding")) {
            mSelectedBackgroundNinePatchPadding =
                selectedGridTileBackground->get<glm::vec2>("padding");
            mHasSelectedBackgroundNinePatchPadding = true;
        }
        else {
            mSelectedBackgroundNinePatchPadding = {0.0f, 0.0f};
            mHasSelectedBackgroundNinePatchPadding = false;
        }
    }

    auto configureOverlayStyle = [this, &theme, &view](TileOverlayStyle& style,
                                                        const std::string& elementName,
                                                        const bool onlySelected,
                                                        const bool favoriteOnly,
                                                        const bool completedOnly) {
        const ThemeData::ThemeElement* overlayElem {theme->getElement(view, elementName, "image")};
        if (overlayElem == nullptr)
            return;

        const bool isMarqueeElement {
            elementName == "gridtile.marquee" || elementName == "gridtile.marquee:selected"};

        if (!(overlayElem->has("path") || overlayElem->has("default") || isMarqueeElement))
            return;

        style.image = std::make_unique<ImageComponent>(false, false);
        style.image->setMipmapping(true);
        style.image->setLinearInterpolation(true);

        if (overlayElem->has("path")) {
            const std::string path {overlayElem->get<std::string>("path")};
            if (Utils::FileSystem::exists(path) && !Utils::FileSystem::isDirectory(path)) {
                style.image->setImage(path);
                style.cachedPath = path;
            }
            else {
                style.image.reset();
                return;
            }
        }
        else if (overlayElem->has("default")) {
            const std::string path {overlayElem->get<std::string>("default")};
            if (Utils::FileSystem::exists(path) && !Utils::FileSystem::isDirectory(path)) {
                style.image->setImage(path);
                style.cachedPath = path;
            }
            else {
                style.image.reset();
                return;
            }
        }
        else if (isMarqueeElement) {
            style.dynamicMarquee = true;
        }

        unsigned int imageFlags {ThemeFlags::ALL ^ ThemeFlags::POSITION ^ ThemeFlags::SIZE};
        if (style.dynamicMarquee)
            imageFlags ^= ThemeFlags::PATH;

        style.image->applyTheme(theme, view, elementName, imageFlags);

        style.pos = overlayElem->has("pos") ? overlayElem->get<glm::vec2>("pos") :
                                               glm::vec2 {0.0f, 0.0f};
        style.size = overlayElem->has("size") ? overlayElem->get<glm::vec2>("size") :
                                                 glm::vec2 {0.18f, 0.18f};
        style.onlySelected = onlySelected;
        style.favoriteOnly = favoriteOnly;
        style.completedOnly = completedOnly;
        style.enabled = true;
    };

    configureOverlayStyle(mOverlayStyle, "gridtile.overlay", false, false, false);
    configureOverlayStyle(mOverlaySelectedStyle, "gridtile.overlay:selected", true, false,
                          false);
    configureOverlayStyle(mFavoriteStyle, "gridtile.favorite", false, true, false);
    configureOverlayStyle(mFavoriteSelectedStyle, "gridtile.favorite:selected", true, true,
                          false);
    configureOverlayStyle(mMarqueeStyle, "gridtile.marquee", false, false, false);
    configureOverlayStyle(mMarqueeSelectedStyle, "gridtile.marquee:selected", true, false,
                          false);
    configureOverlayStyle(mCheevosStyle, "gridtile.cheevos", false, false, true);
    configureOverlayStyle(mCheevosSelectedStyle, "gridtile.cheevos:selected", true, false,
                          true);

    if (!useDefaultGridTile) {
        mOverlayStyle.enabled = false;
        mFavoriteStyle.enabled = false;
        mMarqueeStyle.enabled = false;
        mCheevosStyle.enabled = false;
    }

    if (!useSelectedGridTile) {
        mOverlaySelectedStyle.enabled = false;
        mFavoriteSelectedStyle.enabled = false;
        mMarqueeSelectedStyle.enabled = false;
        mCheevosSelectedStyle.enabled = false;
    }

    if (elem->has("imageCornerRadius"))
        mImageCornerRadius = glm::clamp(elem->get<float>("imageCornerRadius"), 0.0f, 0.5f) *
                             (mItemScale >= 1.0f ? mItemScale : 1.0f) * mRenderer->getScreenWidth();

    if (elem->has("imageColor")) {
        mImageColor = elem->get<unsigned int>("imageColor");
        mImageColorEnd = mImageColor;
    }
    if (elem->has("imageColorEnd"))
        mImageColorEnd = elem->get<unsigned int>("imageColorEnd");

    if (elem->has("imageGradientType")) {
        const std::string& gradientType {elem->get<std::string>("imageGradientType")};
        const std::string normalizedGradientType {normalizeToken(gradientType)};
        if (normalizedGradientType == "horizontal") {
            mImageColorGradientHorizontal = true;
        }
        else if (normalizedGradientType == "vertical") {
            mImageColorGradientHorizontal = false;
        }
        else {
            mImageColorGradientHorizontal = true;
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"imageGradientType\" for element \""
                            << element.substr(5) << "\" defined as \"" << gradientType << "\"";
        }
    }

    mImageSelectedColor = mImageColor;
    mImageSelectedColorEnd = mImageColorEnd;

    if (elem->has("imageSelectedColor")) {
        mImageSelectedColor = elem->get<unsigned int>("imageSelectedColor");
        mImageSelectedColorEnd = mImageSelectedColor;
        mHasImageSelectedColor = true;
    }
    if (elem->has("imageSelectedColorEnd")) {
        mImageSelectedColorEnd = elem->get<unsigned int>("imageSelectedColorEnd");
        mHasImageSelectedColor = true;
    }

    // Apply deferred selected fallback from gridtile.selected.imageColor after native resets.
    if (hasGridTileSelectedImageColorFallback && !elem->has("imageSelectedColor")) {
        mImageSelectedColor = gridTileSelectedImageColor;
        mImageSelectedColorEnd = mImageSelectedColor;
        mHasImageSelectedColor = true;
    }

    if (elem->has("imageSelectedGradientType")) {
        const std::string& gradientType {elem->get<std::string>("imageSelectedGradientType")};
        const std::string normalizedGradientType {normalizeToken(gradientType)};
        if (normalizedGradientType == "horizontal") {
            mImageSelectedColorGradientHorizontal = true;
        }
        else if (normalizedGradientType == "vertical") {
            mImageSelectedColorGradientHorizontal = false;
        }
        else {
            mImageSelectedColorGradientHorizontal = true;
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"imageSelectedGradientType\" for element \""
                            << element.substr(5) << "\" defined as \"" << gradientType << "\"";
        }
    }

    if (elem->has("imageBrightness"))
        mImageBrightness = glm::clamp(elem->get<float>("imageBrightness"), -2.0f, 2.0f);

    if (elem->has("imageSaturation"))
        mImageSaturation = glm::clamp(elem->get<float>("imageSaturation"), 0.0f, 1.0f);

    if (elem->has("unfocusedItemOpacity"))
        mUnfocusedItemOpacity = glm::clamp(elem->get<float>("unfocusedItemOpacity"), 0.1f, 1.0f);

    if (elem->has("unfocusedItemSaturation")) {
        mUnfocusedItemSaturation =
            glm::clamp(elem->get<float>("unfocusedItemSaturation"), 0.0f, 1.0f);
        mHasUnfocusedItemSaturation = true;
    }

    if (elem->has("unfocusedItemDimming"))
        mUnfocusedItemDimming = glm::clamp(elem->get<float>("unfocusedItemDimming"), 0.0f, 1.0f);

    mFont = Font::getFromTheme(elem, properties, mFont);

    if (elem->has("textRelativeScale"))
        mTextRelativeScale = glm::clamp(elem->get<float>("textRelativeScale"), 0.2f, 1.0f);

    if (elem->has("textBackgroundCornerRadius")) {
        mTextBackgroundCornerRadius =
            glm::clamp(elem->get<float>("textBackgroundCornerRadius"), 0.0f, 0.5f) *
            (mItemScale >= 1.0f ? mItemScale : 1.0f) * mRenderer->getScreenWidth();
    }

    if (elem->has("textColor"))
        mTextColor = elem->get<unsigned int>("textColor");
    if (elem->has("textBackgroundColor"))
        mTextBackgroundColor = elem->get<unsigned int>("textBackgroundColor");

    mTextSelectedColor = mTextColor;
    mTextSelectedBackgroundColor = mTextBackgroundColor;

    if (elem->has("textSelectedColor")) {
        mTextSelectedColor = elem->get<unsigned int>("textSelectedColor");
        mHasTextSelectedColor = true;
    }
    if (elem->has("textSelectedBackgroundColor")) {
        mTextSelectedBackgroundColor = elem->get<unsigned int>("textSelectedBackgroundColor");
        mHasTextSelectedColor = true;
    }

    if (hasGridTileSelectedTextColorFallback && !elem->has("textSelectedColor")) {
        mTextSelectedColor = gridTileSelectedTextColor;
        mHasTextSelectedColor = true;
    }

    if (hasGridTileSelectedTextBackgroundColorFallback &&
        !elem->has("textSelectedBackgroundColor")) {
        mTextSelectedBackgroundColor = gridTileSelectedTextBackgroundColor;
        mHasTextSelectedColor = true;
    }

    if (elem->has("textHorizontalScrolling"))
        mTextHorizontalScrolling = elem->get<bool>("textHorizontalScrolling");

    if (elem->has("textHorizontalScrollSpeed")) {
        mTextHorizontalScrollSpeed =
            glm::clamp(elem->get<float>("textHorizontalScrollSpeed"), 0.1f, 10.0f);
    }

    if (elem->has("textHorizontalScrollDelay")) {
        mTextHorizontalScrollDelay =
            glm::clamp(elem->get<float>("textHorizontalScrollDelay"), 0.0f, 10.0f) * 1000.0f;
    }

    if (elem->has("textHorizontalScrollGap")) {
        mTextHorizontalScrollGap =
            glm::clamp(elem->get<float>("textHorizontalScrollGap"), 0.1f, 5.0f);
    }

    if (elem->has("lineSpacing"))
        mLineSpacing = glm::clamp(elem->get<float>("lineSpacing"), 0.5f, 3.0f);

    if (elem->has("letterCase")) {
        const std::string& letterCase {elem->get<std::string>("letterCase")};
        const std::string normalizedLetterCase {normalizeToken(letterCase)};

        if (normalizedLetterCase == "uppercase") {
            mLetterCase = LetterCase::UPPERCASE;
        }
        else if (normalizedLetterCase == "lowercase") {
            mLetterCase = LetterCase::LOWERCASE;
        }
        else if (normalizedLetterCase == "capitalize") {
            mLetterCase = LetterCase::CAPITALIZE;
        }
        else if (normalizedLetterCase != "none") {
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"letterCase\" for element \""
                            << element.substr(5) << "\" defined as \"" << letterCase << "\"";
        }
    }

    if (elem->has("letterCaseAutoCollections")) {
        const std::string& letterCase {elem->get<std::string>("letterCaseAutoCollections")};
        const std::string normalizedLetterCase {normalizeToken(letterCase)};

        if (normalizedLetterCase == "uppercase") {
            mLetterCaseAutoCollections = LetterCase::UPPERCASE;
        }
        else if (normalizedLetterCase == "lowercase") {
            mLetterCaseAutoCollections = LetterCase::LOWERCASE;
        }
        else if (normalizedLetterCase == "capitalize") {
            mLetterCaseAutoCollections = LetterCase::CAPITALIZE;
        }
        else if (normalizedLetterCase == "none") {
            mLetterCaseAutoCollections = LetterCase::NONE;
        }
        else {
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"letterCaseAutoCollections\" for element \""
                            << element.substr(5) << "\" defined as \"" << letterCase << "\"";
        }
    }

    if (elem->has("letterCaseCustomCollections")) {
        const std::string& letterCase {elem->get<std::string>("letterCaseCustomCollections")};
        const std::string normalizedLetterCase {normalizeToken(letterCase)};

        if (normalizedLetterCase == "uppercase") {
            mLetterCaseCustomCollections = LetterCase::UPPERCASE;
        }
        else if (normalizedLetterCase == "lowercase") {
            mLetterCaseCustomCollections = LetterCase::LOWERCASE;
        }
        else if (normalizedLetterCase == "capitalize") {
            mLetterCaseCustomCollections = LetterCase::CAPITALIZE;
        }
        else if (normalizedLetterCase == "none") {
            mLetterCaseCustomCollections = LetterCase::NONE;
        }
        else {
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"letterCaseCustomCollections\" for element \""
                            << element.substr(5) << "\" defined as \"" << letterCase << "\"";
        }
    }

    if (mGamelistView && elem->has("systemNameSuffix"))
        mSystemNameSuffix = elem->get<bool>("systemNameSuffix");

    if (mGamelistView && properties & LETTER_CASE && elem->has("letterCaseSystemNameSuffix")) {
        const std::string& letterCase {elem->get<std::string>("letterCaseSystemNameSuffix")};
        const std::string normalizedLetterCase {normalizeToken(letterCase)};
        if (normalizedLetterCase == "uppercase") {
            mLetterCaseSystemNameSuffix = LetterCase::UPPERCASE;
        }
        else if (normalizedLetterCase == "lowercase") {
            mLetterCaseSystemNameSuffix = LetterCase::LOWERCASE;
        }
        else if (normalizedLetterCase == "capitalize") {
            mLetterCaseSystemNameSuffix = LetterCase::CAPITALIZE;
        }
        else {
            LOG(LogWarning) << "GridComponent: Invalid theme configuration, property "
                               "\"letterCaseSystemNameSuffix\" for element \""
                            << element.substr(5) << "\" defined as \"" << letterCase << "\"";
        }
    }

    if (elem->has("fadeAbovePrimary"))
        mFadeAbovePrimary = elem->get<bool>("fadeAbovePrimary");

    mSize.x = glm::clamp(mSize.x, mRenderer->getScreenWidth() * 0.05f,
                         mRenderer->getScreenWidth() * 1.0f);
    mSize.y = glm::clamp(mSize.y, mRenderer->getScreenHeight() * 0.05f,
                         mRenderer->getScreenHeight() * 1.0f);
}

template <typename T> void GridComponent<T>::onCursorChanged(const CursorState& state)
{
    if (mLastCursor >= 0 && mLastCursor < static_cast<int>(mEntries.size())) {
        auto comp = mEntries.at(mLastCursor).data.item;
        if (comp) {
            if (comp->isStoryBoardRunning("activate") || comp->isStoryBoardRunning("deactivate"))
                comp->deselectStoryboard(false);

            if (comp->selectStoryboard("deactivate") || comp->selectStoryboard("close") ||
                comp->selectStoryboard()) {
                comp->startStoryboard();
                comp->update(0);
            }
            else {
                comp->deselectStoryboard();
            }
        }
    }

    if (mCursor >= 0 && mCursor < static_cast<int>(mEntries.size())) {
        auto comp = mEntries.at(mCursor).data.item;
        if (comp) {
            if (comp->isStoryBoardRunning("activate") || comp->isStoryBoardRunning("deactivate"))
                comp->deselectStoryboard(false);

            if (comp->selectStoryboard("activate") || comp->selectStoryboard("open") ||
                comp->selectStoryboard()) {
                comp->startStoryboard();
                comp->update(0);
            }
            else if (comp->selectStoryboard("scroll")) {
                comp->startStoryboard();
                comp->update(0);
            }
            else {
                comp->deselectStoryboard();
            }
        }
    }

    if (mEntries.size() > static_cast<size_t>(mLastCursor))
        mEntries.at(mLastCursor).data.item->resetComponent();

    if (mColumns == 0)
        return;

    if (mWasScrolling && state == CursorState::CURSOR_STOPPED && mScrollVelocity != 0) {
        mWasScrolling = false;
        if (mCursorChangedCallback)
            mCursorChangedCallback(state);
        return;
    }

    if (mCursor == mLastCursor && !mJustCalculatedLayout) {
        mWasScrolling = false;
        if (mCursorChangedCallback)
            mCursorChangedCallback(state);
        return;
    }
    else {
        mJustCalculatedLayout = false;
    }

    float startPos {mEntryOffset};
    float posMax {static_cast<float>(mEntries.size())};
    float target {static_cast<float>(mCursor)};

    // Find the shortest path to the target.
    float endPos {target}; // Directly.

    if (mPreviousScrollVelocity > 0 && mScrollVelocity == 0 && mEntryOffset > posMax - 1.0f)
        startPos = 0.0f;

    float dist {std::fabs(endPos - startPos)};

    if (std::fabs(target + posMax - startPos - mScrollVelocity) < dist)
        endPos = target + posMax; // Loop around the end (0 -> max).
    if (std::fabs(target - posMax - startPos - mScrollVelocity) < dist)
        endPos = target - posMax; // Loop around the start (max - 1 -> -1).

    // Make sure there are no reverse jumps between items.
    bool changedDirection {false};
    if (mPreviousScrollVelocity != 0 && mPreviousScrollVelocity != mScrollVelocity)
        changedDirection = true;

    if (!changedDirection && mScrollVelocity > 0 && endPos < startPos)
        endPos = endPos + posMax;

    if (!changedDirection && mScrollVelocity < 0 && endPos > startPos)
        endPos = endPos - posMax;

    if (mScrollVelocity != 0)
        mPreviousScrollVelocity = mScrollVelocity;

    // Needed to make sure that overlapping items are renderered correctly.
    if (startPos > endPos)
        mPositiveDirection = true;
    else
        mPositiveDirection = false;

    float animTime {250.0f};

    // If startPos is inbetween two positions then reduce the time slightly as the distance will
    // be shorter meaning the animation would play for too long if not compensated for.
    //    float timeDiff {1.0f};
    //    if (mScrollVelocity == 1)
    //        timeDiff = endPos - startPos;
    //    else if (mScrollVelocity == -1)
    //        timeDiff = startPos - endPos;
    //    if (timeDiff != 1.0f)
    //        animTime =
    //            glm::clamp(std::fabs(glm::mix(0.0f, animTime, timeDiff * 1.5f)), 180.0f,
    //            animTime);

    if (mSuppressTransitions)
        animTime = 0.0f;

    const float visibleRows {mVisibleRows - 1.0f};
    const float startRow {static_cast<float>(mScrollPos)};
    float endRow {static_cast<float>(mCursor / mColumns)};
    const float maxStartRow {std::max(0.0f, static_cast<float>(mRows) - mVisibleRows)};

    if (mCenterSelection && mVisibleRows > 1.0f) {
        const float centeredOffset {std::floor(visibleRows / 2.0f)};
        if (endRow <= centeredOffset)
            endRow = 0.0f;
        else
            endRow -= centeredOffset;
        endRow = std::min(endRow, maxStartRow);
    }
    else {
        if (endRow <= visibleRows)
            endRow = 0.0f;
        else
            endRow -= visibleRows;
    }

    Animation* anim {new LambdaAnimation(
        [this, startPos, endPos, posMax, startRow, endRow](float t) {
            // Non-linear interpolation.
            t = 1.0f - (1.0f - t) * (1.0f - t);
            float f {(endPos * t) + (startPos * (1.0f - t))};

            if (f < 0)
                f += posMax;
            if (f >= posMax)
                f -= posMax;

            mEntryOffset = f;

            if (mInstantRowTransitions)
                mScrollPos = endRow;
            else
                mScrollPos = {(endRow * t) + (startRow * (1.0f - t))};

            if (mInstantItemTransitions) {
                mTransitionFactor = 1.0f;
            }
            else {
                // Linear interpolation.
                mTransitionFactor = t;
                // Non-linear interpolation doesn't seem to be a good match for this component.
                // mTransitionFactor = {(1.0f * t) + (0.0f * (1.0f - t))};
            }
        },
        static_cast<int>(animTime))};

    GuiComponent::setAnimation(anim, 0, nullptr, false, 0);

    if (mCursorChangedCallback)
        mCursorChangedCallback(state);

    mWasScrolling = (state == CursorState::CURSOR_SCROLLING);
}

#endif // ES_CORE_COMPONENTS_PRIMARY_GRID_COMPONENT_H
