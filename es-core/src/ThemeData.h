//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ThemeData.h
//
//  Finds available themes on the file system and loads and parses these.
//  Basic error checking for valid elements and data types is done here,
//  with additional validation handled by the individual components.
//

#ifndef ES_CORE_THEME_DATA_H
#define ES_CORE_THEME_DATA_H

#include "GuiComponent.h"
#include "utils/FileSystemUtil.h"
#include "utils/MathUtil.h"
#include "utils/StringUtil.h"

#include <algorithm>
#include <any>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

namespace ThemeFlags
{
    // clang-format off
    // These are only the most common flags shared accross multiple components, in addition
    // to these there are many component-specific options.
    enum PropertyFlags : unsigned int {
        PATH            = 0x00000001,
        POSITION        = 0x00000002,
        SIZE            = 0x00000004,
        ORIGIN          = 0x00000008,
        COLOR           = 0x00000010,
        FONT_PATH       = 0x00000020,
        FONT_SIZE       = 0x00000040,
        ALIGNMENT       = 0x00000080,
        TEXT            = 0x00000100,
        METADATA        = 0x00000200,
        LETTER_CASE     = 0x00000400,
        LINE_SPACING    = 0x00000800,
        DELAY           = 0x00001000,
        Z_INDEX         = 0x00002000,
        ROTATION        = 0x00004000,
        BRIGHTNESS      = 0x00008000,
        OPACITY         = 0x00010000,
        SATURATION      = 0x00020000,
        VISIBLE         = 0x00040000,
        ALL             = 0xFFFFFFFF
    };
    // clang-format on
} // namespace ThemeFlags

namespace ThemeTriggers
{
    enum class TriggerType {
        NONE,
        NO_VIDEOS,
        NO_MEDIA
    };
} // namespace ThemeTriggers

class ThemeException : public std::exception
{
public:
    std::string message;

    const char* what() const throw() { return message.c_str(); }

    template <typename T> friend ThemeException& operator<<(ThemeException& e, T message)
    {
        std::stringstream ss;
        ss << e.message << message;
        e.message = ss.str();
        return e;
    }

    void setFiles(const std::deque<std::string>& deque)
    {
        // Add all paths to the error message, separated by -> so it's easy to read the log
        // output in case of theme loading errors.
        *this << "\"" << deque.front() << "\"";
        for (auto it = deque.cbegin() + 1; it != deque.cend(); ++it)
            *this << " -> \"" << (*it) << "\"";
    }
};

// Forward declaration para storyboards de tema (evita inclusión circular).
class ThemeStoryboard;

class ThemeData
{
public:
    struct BatoceraTagTranslation {
        std::string canonicalTag;
        std::map<std::string, std::string> attributeAliases;
        bool supported {false};
    };

    class ThemeElement
    {
    public:
        std::string type;

        struct Property {
            enum class PropertyType {
                Unknown,
                Rect,
                Pair,
                String,
                Color,
                Float,
                Bool,
                Int
            };

            void operator=(const glm::vec4& value)
            {
                r = value;
                const glm::vec4 initVector {value};
                v = glm::vec2 {initVector.x, initVector.y};
                type = PropertyType::Rect;
            }
            void operator=(const glm::vec2& value)
            {
                v = value;
                type = PropertyType::Pair;
            }
            void operator=(const std::string& value)
            {
                s = value;
                type = PropertyType::String;
            }
            void operator=(const unsigned int& value)
            {
                i = value;
                type = PropertyType::Color;
            }
            void operator=(const float& value)
            {
                f = value;
                type = PropertyType::Float;
            }
            void operator=(const bool& value)
            {
                b = value;
                type = PropertyType::Bool;
            }

            glm::vec4 r;
            glm::vec2 v;
            std::string s;
            unsigned int i {0};
            float f {0.0f};
            bool b {false};
            PropertyType type {PropertyType::Unknown};
        };

        std::map<std::string, Property> properties;

        template <typename T> const T get(const std::string& prop) const
        {
            if (std::is_same<T, glm::vec2>::value)
                return std::any_cast<const T>(properties.at(prop).v);
            else if (std::is_same<T, std::string>::value)
                return std::any_cast<const T>(properties.at(prop).s);
            else if (std::is_same<T, unsigned int>::value)
                return std::any_cast<const T>(properties.at(prop).i);
            else if (std::is_same<T, float>::value)
                return std::any_cast<const T>(properties.at(prop).f);
            else if (std::is_same<T, bool>::value)
                return std::any_cast<const T>(properties.at(prop).b);
            else if (std::is_same<T, glm::vec4>::value)
                return std::any_cast<const T>(properties.at(prop).r);
            return T();
        }

        bool has(const std::string& prop) const
        {
            return (properties.find(prop) != properties.cend());
        }

        // Storyboards definidos para este elemento en el XML del tema.
        std::map<std::string, std::shared_ptr<ThemeStoryboard>> mStoryBoards;
    };

    ThemeData();

    class ThemeView
    {
    public:
        std::map<std::string, ThemeElement> elements;
    };

    struct ThemeVariant {
        std::string name;
        std::vector<std::pair<std::string, std::string>> labels;
        bool selectable;
        bool gamelistViewStyle;
        std::map<ThemeTriggers::TriggerType, std::pair<std::string, std::vector<std::string>>>
            overrides;

        ThemeVariant()
            : selectable {false}
            , gamelistViewStyle {false}
        {
        }
    };

    struct ThemeColorScheme {
        std::string name;
        std::vector<std::pair<std::string, std::string>> labels;
    };

    struct ThemeSubset {
        std::string name;
        std::string displayName;
        std::string ifSubsetCondition;
        std::vector<std::string> appliesTo;
        std::vector<std::pair<std::string, std::string>> options; // option name -> display label
    };

    enum class ThemeSubsetRole {
        SYSTEM_VIEW,
        COLOR_SET,
        LOGO_SET,
        MENU_ICON_SET,
        GENERIC
    };

    struct ThemeConfigurableSubset {
        ThemeSubset subset;
        std::string selectedValue;
        ThemeSubsetRole role {ThemeSubsetRole::GENERIC};
    };

    struct ThemeTransitions {
        std::string name;
        std::vector<std::pair<std::string, std::string>> labels;
        bool selectable;
        std::map<ViewTransition, ViewTransitionAnimation> animations;

        ThemeTransitions()
            : selectable {true}
        {
        }
    };

    struct ThemeCapability {
        std::string themeName;
        std::vector<ThemeVariant> variants;
        std::vector<ThemeColorScheme> colorSchemes;
        std::vector<ThemeSubset> subsets;
        std::vector<std::string> fontSizes;
        std::vector<std::string> aspectRatios;
        std::vector<std::string> languages;
        std::vector<ThemeTransitions> transitions;
        std::vector<std::string> suppressedTransitionProfiles;
        bool validTheme;
        bool batoceraFormat;

        ThemeCapability()
            : validTheme {false}
            , batoceraFormat {false}
        {
        }
    };

    struct Theme {
        std::string path;
        ThemeCapability capabilities;

        std::string getName() const { return Utils::FileSystem::getStem(path); }
        std::string getThemePath(const std::string& system) const
        {
            return path + "/" + system + "/theme.xml";
        }
    };

    struct StringComparator {
        bool operator()(const std::string& a, const std::string& b) const
        {
            return Utils::String::toUpper(a) < Utils::String::toUpper(b);
        }
    };

    void loadFile(const std::map<std::string, std::string>& sysDataMap,
                  const std::string& path,
                  const ThemeTriggers::TriggerType trigger,
                  const bool customCollection);
    bool hasView(const std::string& view);
    ThemeView& getViewElements(std::string view) { return mViews[view]; }
    bool isBatoceraTheme() const { return mIsBatoceraTheme; }
    static BatoceraTagTranslation translateBatoceraTag(const std::string& tagName);
    static std::string translateBatoceraVariable(const std::string& variableName);
    static ThemeSubsetRole classifyThemeSubset(const ThemeSubset& subset);
    std::vector<ThemeConfigurableSubset>
    getConfigurableSubsets(const std::string& gamelistViewStyle = "") const;

    const ThemeElement* getElement(const std::string& view,
                                   const std::string& element,
                                   const std::string& expectedType) const;

    static void populateThemes();
    const static std::map<std::string, Theme, StringComparator>& getThemes() { return sThemes; }
    const static std::string getSystemThemeFile(const std::string& system);
    const static std::string getFontSizeLabel(const std::string& fontSize);
    const static std::string getAspectRatioLabel(const std::string& aspectRatio);
    const static std::string getLanguageLabel(const std::string& language);
    static void setThemeTransitions();

    const std::map<ThemeTriggers::TriggerType, std::pair<std::string, std::vector<std::string>>>
    getCurrentThemeSelectedVariantOverrides();
    const static void themeLoadedLogOutput();

    enum ElementPropertyType {
        NORMALIZED_RECT,
        NORMALIZED_PAIR,
        PATH,
        STRING,
        COLOR,
        UNSIGNED_INTEGER,
        FLOAT,
        BOOLEAN
    };

    // Updates dynamic runtime variables (for example game.* bindings).
    // Existing keys are replaced; new keys are inserted.
    void setRuntimeVariables(const std::map<std::string, std::string>& variables);

    std::map<std::string, std::string> mVariables;

private:
    unsigned int getHexColor(const std::string& str);
    std::string resolvePlaceholders(const std::string& in) const;
    std::string resolveDynamicBindings(const std::string& in) const;
    bool evaluateIfExpression(const std::string& expression) const;
    bool evaluateIfSubsetCondition(const std::string& ifSubsetExpression) const;
    bool passesBatoceraConditionalAttributes(const pugi::xml_node& node,
                                             const std::string& currentLanguage) const;
    std::string getThemeRegionCompat() const;
    // Normalizes Batocera v29+ format (x, y, w, h) to ESTACION-PRO format (pos, size).
    // Returns true if normalization was applied, false otherwise.
    bool normalizeElementFormat(const pugi::xml_node& root, ThemeElement& element);

    static ThemeCapability parseThemeCapabilities(const std::string& path);

    void parseSubsetVariables(const pugi::xml_node& root);
    void parseIncludes(const pugi::xml_node& root);
    void parseSubsets(const pugi::xml_node& root);
    void parseVariants(const pugi::xml_node& root);
    void parseColorSchemes(const pugi::xml_node& root);
    void parseFontSizes(const pugi::xml_node& root);
    void parseLanguages(const pugi::xml_node& root);
    void parseAspectRatios(const pugi::xml_node& root);
    void parseFeatures(const pugi::xml_node& root);
    void parseCustomViews(const pugi::xml_node& root);
    void parseTransitions(const pugi::xml_node& root);
    void parseVariables(const pugi::xml_node& root);
    void parseViews(const pugi::xml_node& root);
    void parseView(const pugi::xml_node& root, ThemeView& view);
    void parseElement(const pugi::xml_node& root,
                      const std::map<std::string, ElementPropertyType>& typeMap,
                      ThemeElement& element);

#if defined(GETTEXT_DUMMY_ENTRIES)
    // This is just to get gettext msgid entries added to the PO message catalog files.
    void gettextMessageCatalogEntries();
#endif

    static std::vector<std::string> sSupportedViews;
    static std::vector<std::string> sSupportedMediaTypes;
    static std::vector<std::string> sSupportedTransitions;
    static std::vector<std::string> sSupportedTransitionAnimations;

    static std::vector<std::pair<std::string, std::string>> sSupportedFontSizes;
    static std::vector<std::pair<std::string, std::string>> sSupportedAspectRatios;
    static std::vector<std::pair<std::string, std::string>> sSupportedLanguages;

    // Prevent deep recursion when subset includes form cyclic include graphs.
    std::set<std::string> mSubsetVariableVisitedPaths;
    static std::map<std::string, float> sAspectRatioMap;

    static std::map<std::string, std::map<std::string, std::string>> sPropertyAttributeMap;
    static std::map<std::string, std::map<std::string, ElementPropertyType>> sElementMap;

    static inline std::map<std::string, Theme, StringComparator> sThemes;
    static inline std::map<std::string, Theme, StringComparator>::iterator sCurrentTheme {};
    static inline std::string sVariantDefinedTransitions;

    std::map<std::string, ThemeView> mViews;
    std::deque<std::string> mPaths;
    std::vector<std::string> mVariants;
    std::vector<std::string> mColorSchemes;
    std::vector<std::string> mFontSizes;
    std::vector<std::string> mLanguages;
    std::string mSelectedVariant;
    std::string mSelectedGamelistView;
    std::string mSelectedSystemView;
    std::map<std::string, std::string> mSubsetSelections;
    std::string mAutoSelectedGamelistStyle;
    std::string mOverrideVariant;
    std::string mSelectedColorScheme;
    std::string mSelectedFontSize;
    static inline std::string sSelectedAspectRatio;
    static inline bool sAspectRatioMatch {false};
    static inline std::string sThemeLanguage;
    bool mIsBatoceraTheme {false};
    bool mCustomCollection;
};

#endif // ES_CORE_THEME_DATA_H
