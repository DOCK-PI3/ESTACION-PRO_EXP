//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ThemeData.cpp
//
//  Finds available themes on the file system and loads and parses these.
//  Basic error checking for valid elements and data types is done here,
//  with additional validation handled by the individual components.
//

#include "ThemeData.h"

#include "Log.h"
#include "Settings.h"
#include "SystemStatus.h"
#include "anim/ThemeStoryboard.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "renderers/Renderer.h"
#include "utils/FileSystemUtil.h"
#include "utils/LocalizationUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"

#include <algorithm>
#include <functional>
#include <pugixml.hpp>
#include <set>

#if defined(__IOS__)
#include "utils/PlatformUtilIOS.h"
#endif

#if defined(__ANDROID__)
#include "utils/PlatformUtilAndroid.h"
#endif

namespace
{
    std::string getNodeAttributeValueCompat(const pugi::xml_node& node,
                                            const std::vector<std::string>& attributeNames)
    {
        if (!node || attributeNames.empty())
            return "";

        std::vector<std::string> normalizedNames;
        normalizedNames.reserve(attributeNames.size());
        for (const std::string& name : attributeNames) {
            const std::string trimmedName {Utils::String::trim(name)};
            if (!trimmedName.empty())
                normalizedNames.emplace_back(Utils::String::toLower(trimmedName));
        }

        if (normalizedNames.empty())
            return "";

        for (const std::string& normalizedName : normalizedNames) {
            for (pugi::xml_attribute attr {node.first_attribute()}; attr;
                 attr = attr.next_attribute()) {
                const std::string attrName {Utils::String::toLower(attr.name())};
                if (attrName == normalizedName)
                    return Utils::String::trim(attr.as_string());
            }
        }

        return "";
    }

    std::string getTranslatedAttributeValue(
        const pugi::xml_node& node,
        const ThemeData::BatoceraTagTranslation& translation,
        const std::string& canonicalAttribute,
        const std::vector<std::string>& extraAliases = {})
    {
        std::vector<std::string> candidateNames;
        candidateNames.emplace_back(canonicalAttribute);

        const std::string normalizedCanonical {
            Utils::String::toLower(Utils::String::trim(canonicalAttribute))};

        for (const auto& [alias, canonical] : translation.attributeAliases) {
            if (Utils::String::toLower(Utils::String::trim(canonical)) == normalizedCanonical)
                candidateNames.emplace_back(alias);
        }

        candidateNames.insert(candidateNames.end(), extraAliases.cbegin(), extraAliases.cend());
        return getNodeAttributeValueCompat(node, candidateNames);
    }

    std::string normalizeBatoceraLanguageToken(const std::string& value)
    {
        std::string token {Utils::String::toLower(Utils::String::trim(value))};
        token = Utils::String::replace(token, "-", "_");

        if (token == "jp")
            return "ja";
        if (token == "gr")
            return "el";
        if (token == "br")
            return "pt_br";

        return token;
    }

    bool subsetNameContainsAllTokens(const std::string& subsetName,
                                     const std::vector<std::string>& requiredTokens,
                                     const std::vector<std::string>& rejectedTokens = {})
    {
        const std::string normalizedName {Utils::String::toLower(Utils::String::trim(subsetName))};

        for (const std::string& token : rejectedTokens) {
            if (!token.empty() &&
                normalizedName.find(Utils::String::toLower(token)) != std::string::npos) {
                return false;
            }
        }

        for (const std::string& token : requiredTokens) {
            if (!token.empty() &&
                normalizedName.find(Utils::String::toLower(token)) == std::string::npos) {
                return false;
            }
        }

        return true;
    }

    bool parseBooleanAttribute(const std::string& value, const bool defaultValue = true)
    {
        const std::string normalized {Utils::String::toLower(Utils::String::trim(value))};
        if (normalized.empty())
            return defaultValue;

        if (normalized == "0" || normalized == "false" || normalized == "no" ||
            normalized == "off") {
            return false;
        }

        return true;
    }

    std::vector<std::string> tokenizeConditionValues(const std::string& value)
    {
        std::vector<std::string> tokens;
        const std::string normalizedValue {
            Utils::String::replace(Utils::String::replace(value, "|", ","), ";", ",")};
        auto splitTokens {Utils::String::delimitedStringToVector(normalizedValue, ",")};

        for (auto& token : splitTokens) {
            token = normalizeBatoceraLanguageToken(token);
            if (!token.empty())
                tokens.emplace_back(token);
        }

        return tokens;
    }

    bool matchesAnyConditionToken(const std::string& condition,
                                  const std::vector<std::string>& candidates)
    {
        const auto tokens {tokenizeConditionValues(condition)};
        if (tokens.empty())
            return true;

        for (const auto& candidateRaw : candidates) {
            const std::string candidate {
                Utils::String::toLower(Utils::String::trim(candidateRaw))};
            if (candidate.empty())
                continue;

            if (std::find(tokens.cbegin(), tokens.cend(), candidate) != tokens.cend())
                return true;
        }

        return false;
    }

    std::string getCurrentArchCompat()
    {
#if defined(__x86_64__) || defined(_M_X64)
        return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
        return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
        return "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
        return "arm";
#elif defined(__riscv)
        return "riscv";
#else
        return "unknown";
#endif
    }

    std::vector<std::string> buildLanguageCandidates(const std::string& language)
    {
        std::vector<std::string> candidates;
        const std::string normalizedLocale {normalizeBatoceraLanguageToken(language)};

        if (!normalizedLocale.empty())
            candidates.emplace_back(normalizedLocale);

        if (normalizedLocale.size() >= 2)
            candidates.emplace_back(normalizedLocale.substr(0, 2));

        if (normalizedLocale == "ja" || normalizedLocale == "ja_jp")
            candidates.emplace_back("jp");
        else if (normalizedLocale == "el" || normalizedLocale == "el_gr")
            candidates.emplace_back("gr");
        else if (normalizedLocale == "pt_br")
            candidates.emplace_back("br");

        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        return candidates;
    }

    std::vector<std::string> buildRegionCandidates(const std::string& selectedRegion,
                                                   const std::string& language)
    {
        std::vector<std::string> candidates;

        const std::string normalizedRegion {
            Utils::String::toLower(Utils::String::trim(selectedRegion))};
        if (!normalizedRegion.empty())
            candidates.emplace_back(normalizedRegion);

        const std::string normalizedLocale {Utils::String::toLower(Utils::String::trim(language))};
        if (normalizedLocale.size() >= 5 && normalizedLocale[2] == '_')
            candidates.emplace_back(normalizedLocale.substr(3, 2));
        if (normalizedLocale.size() >= 2)
            candidates.emplace_back(normalizedLocale.substr(0, 2));

        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        return candidates;
    }

    std::string normalizeThemeSubsetName(const std::string& subsetName)
    {
        return Utils::String::toLower(Utils::String::trim(subsetName));
    }

    std::string resolveLegacyBatoceraLocaleTokens(const std::string& input,
                                                  const std::string& language,
                                                  const std::string& region)
    {
        std::string resolved {input};

        if (resolved.find('$') == std::string::npos)
            return resolved;

        resolved = Utils::String::replace(resolved, "$language", language);
        resolved = Utils::String::replace(resolved, "$locale", language);
        resolved = Utils::String::replace(resolved, "$lang", language.size() >= 2 ?
                                                                  Utils::String::toLower(
                                                                      language.substr(0, 2)) :
                                                                  language);
        resolved = Utils::String::replace(resolved, "$region", region);
        resolved = Utils::String::replace(resolved, "$country", region);

        return resolved;
    }

    std::string getCanonicalThemeSubsetSettingKey(const std::string& subsetName)
    {
        const std::string normalizedName {normalizeThemeSubsetName(subsetName)};

        if (normalizedName == "systemview")
            return "ThemeSystemView";
        if (normalizedName == "colorset" || normalizedName == "colorscheme")
            return "ThemeColorSet";
        if (normalizedName == "iconset")
            return "ThemeIconSet";
        if (normalizedName == "menu")
            return "ThemeMenu";
        if (normalizedName == "gamelistview")
            return "ThemeGamelistView";
        if (normalizedName == "region")
            return "ThemeRegionName";

        return "";
    }

    std::vector<std::string> getThemeSubsetSettingKeys(const std::string& subsetName)
    {
        std::vector<std::string> keys;
        const std::string trimmedName {Utils::String::trim(subsetName)};
        const std::string normalizedName {normalizeThemeSubsetName(subsetName)};
        std::string currentThemeName;

        const auto& themes {ThemeData::getThemes()};
        auto currentThemeIt {themes.find(Settings::getInstance()->getString("Theme"))};
        if (currentThemeIt != themes.cend())
            currentThemeName = currentThemeIt->first;

        if (!trimmedName.empty())
            keys.emplace_back("ThemeSubset_" + trimmedName);
        if (!normalizedName.empty() && normalizedName != trimmedName)
            keys.emplace_back("ThemeSubset_" + normalizedName);

        const std::string canonicalKey {getCanonicalThemeSubsetSettingKey(subsetName)};
        if (!canonicalKey.empty())
            keys.emplace_back(canonicalKey);

        if (!normalizedName.empty())
            keys.emplace_back("subset." + normalizedName);

        // Batocera/Retrobat compatibility: theme-scoped subset keys.
        if (!currentThemeName.empty()) {
            if (!trimmedName.empty())
                keys.emplace_back("subset." + currentThemeName + "." + trimmedName);
            if (!normalizedName.empty())
                keys.emplace_back("subset." + currentThemeName + "." + normalizedName);
        }

        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        return keys;
    }

    std::string getPersistedThemeSubsetValue(const std::string& subsetName)
    {
        for (const std::string& key : getThemeSubsetSettingKeys(subsetName)) {
            if (!Settings::getInstance()->hasString(key))
                continue;
            const std::string value {Settings::getInstance()->getString(key)};
            if (!value.empty())
                return value;
        }

        return "";
    }

    std::string getResolvedThemeSubsetSelection(
        const std::string& subsetName,
        const std::string& selectedSystemView,
        const std::map<std::string, std::string>& subsetSelections)
    {
        const std::string normalizedName {normalizeThemeSubsetName(subsetName)};

        if (normalizedName == "systemview")
            return selectedSystemView;

        auto normalizedIt = subsetSelections.find(normalizedName);
        if (normalizedIt != subsetSelections.cend())
            return normalizedIt->second;

        auto originalIt = subsetSelections.find(subsetName);
        if (originalIt != subsetSelections.cend())
            return originalIt->second;

        return getPersistedThemeSubsetValue(subsetName);
    }

    bool detectBatoceraThemeRoot(const pugi::xml_node& root)
    {
        if (!root)
            return false;

        const auto subsetTranslation {ThemeData::translateBatoceraTag("subset")};
        const auto includeTranslation {ThemeData::translateBatoceraTag("include")};

        if (subsetTranslation.supported && root.child(subsetTranslation.canonicalTag.c_str()))
            return true;

        if (root.child("customView"))
            return true;

        if (root.attribute("defaultView"))
            return true;

        for (pugi::xml_node includeNode {root.child(includeTranslation.canonicalTag.c_str())};
             includeNode;
             includeNode = includeNode.next_sibling(includeTranslation.canonicalTag.c_str())) {
            const bool hasSubsetAttribute {
                !getNodeAttributeValueCompat(includeNode, {"subset"}).empty()};
            const bool hasLangAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "lang").empty()};
            const bool hasRegionAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "region").empty()};
            const bool hasIfAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "if").empty()};
            const bool hasIfSubsetAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "ifSubset").empty()};
            const bool hasIfCheevosAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "ifCheevos")
                     .empty()};
            const bool hasIfNetplayAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "ifNetplay")
                     .empty()};
            const bool hasIfArchAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "ifArch").empty()};
            const bool hasIfNotArchAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "ifNotArch")
                     .empty()};
            const bool hasTinyScreenAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "tinyScreen")
                     .empty()};
            const bool hasVerticalScreenAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "verticalScreen")
                     .empty()};
            const bool hasIfHelpPromptsAttribute {
                !getTranslatedAttributeValue(includeNode, includeTranslation, "ifHelpPrompts")
                     .empty()};

            if (hasSubsetAttribute || hasLangAttribute || hasRegionAttribute || hasIfAttribute ||
                hasIfSubsetAttribute || hasIfCheevosAttribute || hasIfNetplayAttribute ||
                hasIfArchAttribute || hasIfNotArchAttribute || hasTinyScreenAttribute ||
                hasVerticalScreenAttribute || hasIfHelpPromptsAttribute) {
                return true;
            }
        }

        return false;
    }

    std::string resolveThemeIncludePathCompat(const std::string& relPath,
                                              const std::string& basePath)
    {
        std::string resolved {Utils::FileSystem::resolveRelativePath(relPath, basePath, true)};

        if (ResourceManager::getInstance().fileExists(resolved))
            return resolved;

        // Some Batocera themes were packaged with include paths under
        // "_arte/multi-options/..." while files actually ship under "_arte/...".
        const std::string fixedRel {Utils::String::replace(relPath, "multi-options/", "")};

        if (fixedRel != relPath) {
            const std::string fixedResolved {
                Utils::FileSystem::resolveRelativePath(fixedRel, basePath, true)};

            if (ResourceManager::getInstance().fileExists(fixedResolved))
                return fixedResolved;
        }

        return resolved;
    }

    std::string getIncludeNodePathCompat(
        const pugi::xml_node& includeNode,
        const ThemeData::BatoceraTagTranslation& includeTranslation)
    {
        std::string includePath {getTranslatedAttributeValue(
            includeNode, includeTranslation, "path", {"file", "filepath", "src", "href"})};

        if (includePath.empty())
            includePath = includeNode.text().as_string();

        return Utils::String::trim(includePath);
    }

    std::string resolveThemeAssetPathCompat(const std::string& relPath,
                                            const std::string& includeBasePath,
                                            const std::string& themeRootPath)
    {
        auto resolveAndCheck = [](const std::string& candidateRelPath,
                                  const std::string& basePath,
                                  std::string& outResolved) -> bool {
            outResolved = Utils::FileSystem::resolveRelativePath(candidateRelPath, basePath, true);
            return ResourceManager::getInstance().fileExists(outResolved);
        };

        std::string resolved;
        if (resolveAndCheck(relPath, includeBasePath, resolved))
            return resolved;

        // Batocera themes often use root-relative paths inside included XML files.
        // If include-relative lookup fails, retry against the main theme root.
        if (!themeRootPath.empty()) {
            std::string rootResolved;
            if (resolveAndCheck(relPath, themeRootPath, rootResolved))
                return rootResolved;

            // Many Batocera themes mix png/svg logo extensions for system assets.
            if (Utils::String::endsWith(Utils::String::toLower(relPath), ".png")) {
                std::string svgPath {relPath.substr(0, relPath.size() - 4) + ".svg"};

                if (resolveAndCheck(svgPath, includeBasePath, rootResolved))
                    return rootResolved;
                if (resolveAndCheck(svgPath, themeRootPath, rootResolved))
                    return rootResolved;
            }

            // ArcadePower-style fallback: if a logo is not found in basicos, retry arcade.
            if (relPath.find("/imgs/basicos/") != std::string::npos) {
                const std::string arcadePath {
                    Utils::String::replace(relPath, "/imgs/basicos/", "/imgs/arcade/")};

                if (resolveAndCheck(arcadePath, includeBasePath, rootResolved))
                    return rootResolved;
                if (resolveAndCheck(arcadePath, themeRootPath, rootResolved))
                    return rootResolved;

                if (Utils::String::endsWith(Utils::String::toLower(arcadePath), ".png")) {
                    std::string arcadeSvgPath {arcadePath.substr(0, arcadePath.size() - 4) +
                                               ".svg"};

                    if (resolveAndCheck(arcadeSvgPath, includeBasePath, rootResolved))
                        return rootResolved;
                    if (resolveAndCheck(arcadeSvgPath, themeRootPath, rootResolved))
                        return rootResolved;
                }
            }

            // Optional Batocera icon fallback used by some themes.
            if (Utils::String::endsWith(Utils::String::toLower(relPath), "/hidden.svg")) {
                const std::string helpRandomPath {
                    Utils::String::replace(relPath, "hidden.svg", "help-random.svg")};

                if (resolveAndCheck(helpRandomPath, includeBasePath, rootResolved))
                    return rootResolved;
                if (resolveAndCheck(helpRandomPath, themeRootPath, rootResolved))
                    return rootResolved;
            }
        }

        return resolved;
    }
}

// clang-format off
std::vector<std::string> ThemeData::sSupportedViews {
    {"all"},
    {"system"},
    {"gamelist"},
    {"menu"},
    {"screen"},
    {"splash"},
    {"basic"},
    {"detailed"},
    {"grid"}};

std::vector<std::string> ThemeData::sSupportedMediaTypes {
    {"miximage"},
    {"marquee"},
    {"screenshot"},
    {"titlescreen"},
    {"cover"},
    {"backcover"},
    {"3dbox"},
    {"physicalmedia"},
    {"fanart"},
    {"video"}};

std::vector<std::string> ThemeData::sSupportedTransitions {
    {"systemToSystem"},
    {"systemToGamelist"},
    {"gamelistToGamelist"},
    {"gamelistToSystem"},
    {"startupToSystem"},
    {"startupToGamelist"}};

std::vector<std::string> ThemeData::sSupportedTransitionAnimations {
    {"builtin-instant"},
    {"builtin-slide"},
    {"builtin-fade"}};

std::vector<std::pair<std::string, std::string>> ThemeData::sSupportedFontSizes {
    {"medium", "medium"},
    {"large", "large"},
    {"small", "small"},
    {"x-large", "extra large"},
    {"x-small", "extra small"}};

std::vector<std::pair<std::string, std::string>> ThemeData::sSupportedAspectRatios {
    {"automatic", "automatic"},
    {"16:9", "16:9"},
    {"16:9_vertical", "16:9 vertical"},
    {"16:10", "16:10"},
    {"16:10_vertical", "16:10 vertical"},
    {"3:2", "3:2"},
    {"3:2_vertical", "3:2 vertical"},
    {"4:3", "4:3"},
    {"4:3_vertical", "4:3 vertical"},
    {"5:3", "5:3"},
    {"5:3_vertical", "5:3 vertical"},
    {"5:4", "5:4"},
    {"5:4_vertical", "5:4 vertical"},
    {"8:7", "8:7"},
    {"8:7_vertical", "8:7 vertical"},
    {"19.5:9", "19.5:9"},
    {"19.5:9_vertical", "19.5:9 vertical"},
    {"20:9", "20:9"},
    {"20:9_vertical", "20:9 vertical"},
    {"21:9", "21:9"},
    {"21:9_vertical", "21:9 vertical"},
    {"32:9", "32:9"},
    {"32:9_vertical", "32:9 vertical"},
    {"1:1", "1:1"}};

std::map<std::string, float> ThemeData::sAspectRatioMap {
    {"16:9", 1.7777f},
    {"16:9_vertical", 0.5625f},
    {"16:10", 1.6f},
    {"16:10_vertical", 0.625f},
    {"3:2", 1.5f},
    {"3:2_vertical", 0.6667f},
    {"4:3", 1.3333f},
    {"4:3_vertical", 0.75f},
    {"5:3", 1.6667f},
    {"5:3_vertical", 0.6f},
    {"5:4", 1.25f},
    {"5:4_vertical", 0.8f},
    {"8:7", 1.1429f},
    {"8:7_vertical", 0.875f},
    {"19.5:9", 2.1667f},
    {"19.5:9_vertical", 0.4615f},
    {"20:9", 2.2222f},
    {"20:9_vertical", 0.45f},
    {"21:9", 2.3703f},
    {"21:9_vertical", 0.4219f},
    {"32:9", 3.5555f},
    {"32:9_vertical", 0.2813f},
    {"1:1", 1.0f}};

std::vector<std::pair<std::string, std::string>> ThemeData::sSupportedLanguages {
    {"automatic", "automatic"},
    {"en_US", "ENGLISH (UNITED STATES)"},
    {"en_GB", "ENGLISH (UNITED KINGDOM)"},
    {"bs_BA", "BOSANSKI"},
    {"ca_ES", "CATALÀ"},
    {"de_DE", "DEUTSCH"},
    {"es_ES", "ESPAÑOL (ESPAÑA)"},
    {"fr_FR", "FRANÇAIS"},
    {"hr_HR", "HRVATSKI"},
    {"it_IT", "ITALIANO"},
    {"nl_NL", "NEDERLANDS"},
    {"pl_PL", "POLSKI"},
    {"pt_BR", "PORTUGUÊS (BRASIL)"},
    {"pt_PT", "PORTUGUÊS (PORTUGAL)"},
    {"ro_RO", "ROMÂNĂ"},
    {"ru_RU", "РУССКИЙ"},
    {"sr_RS", "SRPSKI"},
    {"sv_SE", "SVENSKA"},
    {"vi_VN", "TIẾNG VIỆT"},
    {"ar_SA", "العربية"},
    {"ja_JP", "日本語"},
    {"ko_KR", "한국어"},
    {"zh_CN", "简体中文"},
    {"zh_TW", "繁體中文"}};

std::map<std::string, std::map<std::string, std::string>> ThemeData::sPropertyAttributeMap
    // The data type is defined by the parent property.
    {
     {"badges",
      {{"customBadgeIcon", "badge"},
       {"customControllerIcon", "controller"}}},
     {"helpsystem",
      {{"customButtonIcon", "button"}}},
     {"systemstatus",
      {{"customIcon", "icon"}}},
    };

std::map<std::string, std::map<std::string, ThemeData::ElementPropertyType>>
    ThemeData::sElementMap {
     {"splash",
      {{"backgroundColor", COLOR}}},
            {"container",
                {{"pos", NORMALIZED_PAIR},
                 {"size", NORMALIZED_PAIR},
                 {"x", FLOAT},
                 {"y", FLOAT},
                 {"h", FLOAT},
                 {"w", FLOAT},
                 {"origin", NORMALIZED_PAIR},
                 {"rotation", FLOAT},
                 {"rotationOrigin", NORMALIZED_PAIR},
                 {"scale", FLOAT},
                 {"scaleOrigin", NORMALIZED_PAIR},
                 {"padding", NORMALIZED_RECT},
                 {"clipChildren", BOOLEAN},
                 {"opacity", FLOAT},
                 {"visible", BOOLEAN},
                 {"zIndex", FLOAT}}},
         {"stackpanel",
            {{"pos", NORMALIZED_PAIR},
             {"size", NORMALIZED_PAIR},
             {"x", FLOAT},
             {"y", FLOAT},
             {"h", FLOAT},
             {"w", FLOAT},
             {"orientation", STRING},
             {"reverse", BOOLEAN},
             {"separator", FLOAT},
             {"padding", NORMALIZED_RECT},
             {"opacity", FLOAT},
             {"visible", BOOLEAN},
             {"clipChildren", BOOLEAN},
             {"zIndex", FLOAT}}},
         {"rectangle",
            {{"pos", NORMALIZED_PAIR},
             {"size", NORMALIZED_PAIR},
             {"x", FLOAT},
             {"y", FLOAT},
             {"h", FLOAT},
             {"w", FLOAT},
             {"color", COLOR},
             {"backgroundColor", COLOR},
             {"borderColor", COLOR},
             {"borderSize", FLOAT},
             {"roundCorners", FLOAT},
             {"opacity", FLOAT},
             {"visible", BOOLEAN},
             {"padding", NORMALIZED_RECT},
             {"clipChildren", BOOLEAN},
             {"zIndex", FLOAT}}},
         {"screenshader",
            {{"path", PATH},
             {"pos", NORMALIZED_PAIR},
             {"size", NORMALIZED_PAIR},
             {"x", FLOAT},
             {"y", FLOAT},
             {"h", FLOAT},
             {"w", FLOAT},
             {"visible", BOOLEAN},
             {"clipRect", NORMALIZED_RECT},
             {"zIndex", FLOAT},
             {"blur", FLOAT}}},
     {"control",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
             {"x", FLOAT},
             {"y", FLOAT},
             {"w", FLOAT},
             {"h", FLOAT},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"scale", FLOAT},
       {"scaleOrigin", NORMALIZED_PAIR},
       {"opacity", FLOAT},
       {"zIndex", FLOAT},
       {"visible", BOOLEAN},
             {"clipChildren", BOOLEAN},
       {"clipRect", NORMALIZED_RECT},
       {"padding", NORMALIZED_RECT},
       {"offset", NORMALIZED_PAIR},
       {"offsetX", FLOAT},
       {"offsetY", FLOAT},
       {"maxSize", NORMALIZED_PAIR},
       {"minSize", NORMALIZED_PAIR}}},
     {"carousel",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"type", STRING},
       {"staticImage", PATH},
       {"imageType", STRING},
    {"imageSource", STRING},
       {"defaultImage", PATH},
       {"defaultFolderImage", PATH},
       {"maxItemCount", FLOAT},
       {"itemsBeforeCenter", UNSIGNED_INTEGER},
       {"itemsAfterCenter", UNSIGNED_INTEGER},
       {"itemStacking", STRING},
       {"selectedItemMargins", NORMALIZED_PAIR},
       {"selectedItemOffset", NORMALIZED_PAIR},
       {"itemSize", NORMALIZED_PAIR},
       {"itemScale", FLOAT},
       {"itemRotation", FLOAT},
       {"itemRotationOrigin", NORMALIZED_PAIR},
       {"itemAxisHorizontal", BOOLEAN},
       {"itemAxisRotation", FLOAT},
       {"imageFit", STRING},
       {"imageCropPos", NORMALIZED_PAIR},
       {"imageInterpolation", STRING},
       {"imageCornerRadius", FLOAT},
       {"imageColor", COLOR},
       {"imageColorEnd", COLOR},
       {"imageGradientType", STRING},
       {"imageSelectedColor", COLOR},
       {"imageSelectedColorEnd", COLOR},
       {"imageSelectedGradientType", STRING},
       {"imageBrightness", FLOAT},
       {"imageSaturation", FLOAT},
       {"itemTransitions", STRING},
    {"defaultTransition", STRING},
       {"itemDiagonalOffset", FLOAT},
       {"itemHorizontalAlignment", STRING},
       {"itemVerticalAlignment", STRING},
    {"logoSize", NORMALIZED_PAIR},
    {"logoPos", NORMALIZED_PAIR},
    {"maxLogoCount", FLOAT},
    {"minLogoOpacity", FLOAT},
    {"logoScale", FLOAT},
    {"transitionSpeed", FLOAT},
    {"scaledLogoSpacing", FLOAT},
    {"systemInfoDelay", FLOAT},
    {"systemInfoCountOnly", BOOLEAN},
    {"logoAlignment", STRING},
    {"logoRotation", FLOAT},
    {"logoRotationOrigin", NORMALIZED_PAIR},
       {"wheelHorizontalAlignment", STRING},
       {"wheelVerticalAlignment", STRING},
       {"horizontalOffset", FLOAT},
       {"verticalOffset", FLOAT},
       {"reflections", BOOLEAN},
       {"reflectionsOpacity", FLOAT},
       {"reflectionsFalloff", FLOAT},
       {"unfocusedItemOpacity", FLOAT},
       {"unfocusedItemSaturation", FLOAT},
       {"unfocusedItemDimming", FLOAT},
       {"fastScrolling", BOOLEAN},
       {"color", COLOR},
       {"colorEnd", COLOR},
       {"gradientType", STRING},
       {"text", STRING},
       {"textRelativeScale", FLOAT},
       {"textBackgroundCornerRadius", FLOAT},
       {"textColor", COLOR},
       {"textBackgroundColor", COLOR},
       {"textSelectedColor", COLOR},
       {"textSelectedBackgroundColor", COLOR},
       {"textHorizontalScrolling", BOOLEAN},
       {"textHorizontalScrollSpeed", FLOAT},
       {"textHorizontalScrollDelay", FLOAT},
       {"textHorizontalScrollGap", FLOAT},
       {"fontPath", PATH},
       {"fontSize", FLOAT},
       {"letterCase", STRING},
       {"letterCaseAutoCollections", STRING},
       {"letterCaseCustomCollections", STRING},
       {"lineSpacing", FLOAT},
       {"systemNameSuffix", BOOLEAN},
       {"letterCaseSystemNameSuffix", STRING},
       {"fadeAbovePrimary", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"grid",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"staticImage", PATH},
       {"imageType", STRING},
             {"imageSource", STRING},
       {"defaultImage", PATH},
       {"defaultFolderImage", PATH},
       {"itemSize", NORMALIZED_PAIR},
       {"itemScale", FLOAT},
       {"itemSpacing", NORMALIZED_PAIR},
       {"scaleInwards", BOOLEAN},
       {"fractionalRows", BOOLEAN},
       {"itemTransitions", STRING},
       {"rowTransitions", STRING},
       {"unfocusedItemOpacity", FLOAT},
       {"unfocusedItemSaturation", FLOAT},
       {"unfocusedItemDimming", FLOAT},
       {"imageFit", STRING},
       {"imageCropPos", NORMALIZED_PAIR},
       {"imageInterpolation", STRING},
       {"imageRelativeScale", FLOAT},
       {"imageCornerRadius", FLOAT},
       {"imageColor", COLOR},
       {"imageColorEnd", COLOR},
       {"imageGradientType", STRING},
       {"imageSelectedColor", COLOR},
       {"imageSelectedColorEnd", COLOR},
       {"imageSelectedGradientType", STRING},
       {"imageBrightness", FLOAT},
       {"imageSaturation", FLOAT},
       {"backgroundImage", PATH},
       {"backgroundRelativeScale", FLOAT},
       {"backgroundCornerRadius", FLOAT},
       {"backgroundColor", COLOR},
       {"backgroundColorEnd", COLOR},
       {"backgroundGradientType", STRING},
       {"selectorImage", PATH},
       {"selectorRelativeScale", FLOAT},
       {"selectorCornerRadius", FLOAT},
       {"selectorLayer", STRING},
       {"selectorColor", COLOR},
       {"selectorColorEnd", COLOR},
       {"selectorGradientType", STRING},
       {"text", STRING},
       {"textRelativeScale", FLOAT},
       {"textBackgroundCornerRadius", FLOAT},
       {"textColor", COLOR},
       {"textBackgroundColor", COLOR},
       {"textSelectedColor", COLOR},
       {"textSelectedBackgroundColor", COLOR},
       {"textHorizontalScrolling", BOOLEAN},
       {"textHorizontalScrollSpeed", FLOAT},
       {"textHorizontalScrollDelay", FLOAT},
       {"textHorizontalScrollGap", FLOAT},
       {"fontPath", PATH},
       {"fontSize", FLOAT},
       {"letterCase", STRING},
       {"letterCaseAutoCollections", STRING},
       {"letterCaseCustomCollections", STRING},
       {"lineSpacing", FLOAT},
       {"systemNameSuffix", BOOLEAN},
       {"letterCaseSystemNameSuffix", STRING},
       {"fadeAbovePrimary", BOOLEAN},
    {"animateSelection", BOOLEAN},
       {"animateColor", COLOR},
       {"animateColorTime", FLOAT},
       {"logoSize", NORMALIZED_PAIR},
       {"maxLogoCount", FLOAT},
       {"logoScale", FLOAT},
       {"systemInfoDelay", FLOAT},
       {"logoAlignment", STRING},
       {"logoRotation", FLOAT},
       {"logoRotationOrigin", NORMALIZED_PAIR},
    {"margin", NORMALIZED_PAIR},
    {"padding", NORMALIZED_RECT},
    {"selectionMode", STRING},
       {"zIndex", FLOAT}}},
     {"textlist",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
             {"scale", FLOAT},
             {"scaleOrigin", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"selectorWidth", FLOAT},
       {"selectorHeight", FLOAT},
       {"selectorHorizontalOffset", FLOAT},
       {"selectorVerticalOffset", FLOAT},
    {"selectorOffsetY", FLOAT},
       {"selectorColor", COLOR},
       {"selectorColorEnd", COLOR},
       {"selectorGradientType", STRING},
       {"selectorImagePath", PATH},
       {"selectorImageTile", BOOLEAN},
       {"primaryColor", COLOR},
       {"secondaryColor", COLOR},
       {"selectedColor", COLOR},
       {"selectedSecondaryColor", COLOR},
       {"selectedBackgroundColor", COLOR},
       {"selectedSecondaryBackgroundColor", COLOR},
       {"selectedBackgroundMargins", NORMALIZED_PAIR},
       {"selectedBackgroundCornerRadius", FLOAT},
       {"textHorizontalScrolling", BOOLEAN},
       {"textHorizontalScrollSpeed", FLOAT},
       {"textHorizontalScrollDelay", FLOAT},
       {"textHorizontalScrollGap", FLOAT},
       {"fontPath", PATH},
       {"fontSize", FLOAT},
       {"horizontalAlignment", STRING},
       {"horizontalMargin", FLOAT},
       {"letterCase", STRING},
       {"letterCaseAutoCollections", STRING},
       {"letterCaseCustomCollections", STRING},
       {"lineSpacing", FLOAT},
       {"indicators", STRING},
       {"collectionIndicators", STRING},
       {"systemNameSuffix", BOOLEAN},
       {"letterCaseSystemNameSuffix", STRING},
       {"fadeAbovePrimary", BOOLEAN},
       {"glowColor", COLOR},
       {"glowSize", FLOAT},
       {"glowOffset", NORMALIZED_PAIR},
        {"scrollSound", PATH},
    {"forceUppercase", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"image",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
             {"scale", FLOAT},
             {"scaleOrigin", NORMALIZED_PAIR},
       {"maxSize", NORMALIZED_PAIR},
       {"cropSize", NORMALIZED_PAIR},
       {"cropPos", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"stationary", STRING},
       {"renderDuringTransitions", BOOLEAN},
       {"flipHorizontal", BOOLEAN},
       {"flipVertical", BOOLEAN},
       {"path", PATH},
       {"gameOverridePath", PATH},
       {"default", PATH},
       {"imageType", STRING},
       {"metadataElement", BOOLEAN},
       {"gameselector", STRING},
       {"gameselectorEntry", UNSIGNED_INTEGER},
       {"tile", BOOLEAN},
       {"tileSize", NORMALIZED_PAIR},
       {"tileHorizontalAlignment", STRING},
       {"tileVerticalAlignment", STRING},
    {"horizontalAlignment", STRING},
       {"interpolation", STRING},
    {"linearSmooth", BOOLEAN},
       {"mipmap", BOOLEAN},
       {"cornerRadius", FLOAT},
       {"color", COLOR},
       {"colorEnd", COLOR},
       {"gradientType", STRING},
       {"scrollFadeIn", BOOLEAN},
       {"brightness", FLOAT},
       {"opacity", FLOAT},
       {"saturation", FLOAT},
       {"visible", BOOLEAN},
       {"reflexion", NORMALIZED_PAIR},
       {"reflexionOnFrame", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"video",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
             {"scale", FLOAT},
             {"scaleOrigin", NORMALIZED_PAIR},
       {"maxSize", NORMALIZED_PAIR},
       {"cropSize", NORMALIZED_PAIR},
       {"cropPos", NORMALIZED_PAIR},
       {"imageSize", NORMALIZED_PAIR},
       {"imageMaxSize", NORMALIZED_PAIR},
       {"imageCropSize", NORMALIZED_PAIR},
       {"imageCropPos", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"stationary", STRING},
       {"path", PATH},
       {"default", PATH},
       {"defaultImage", PATH},
       {"imageType", STRING},
       {"metadataElement", BOOLEAN},
       {"gameselector", STRING},
       {"gameselectorEntry", UNSIGNED_INTEGER},
       {"iterationCount", UNSIGNED_INTEGER},
       {"onIterationsDone", STRING},
       {"audio", BOOLEAN},
       {"interpolation", STRING},
    {"roundCorners", FLOAT},
       {"imageCornerRadius", FLOAT},
       {"videoCornerRadius", FLOAT},
       {"color", COLOR},
       {"colorEnd", COLOR},
       {"gradientType", STRING},
       {"pillarboxes", BOOLEAN},
       {"pillarboxThreshold", NORMALIZED_PAIR},
       {"scanlines", BOOLEAN},
       {"delay", FLOAT},
       {"fadeInType", STRING},
       {"fadeInTime", FLOAT},
       {"scrollFadeIn", BOOLEAN},
       {"brightness", FLOAT},
       {"opacity", FLOAT},
       {"saturation", FLOAT},
       {"visible", BOOLEAN},
       {"loops", FLOAT},
       {"showSnapshotNoVideo", BOOLEAN},
       {"showSnapshotDelay", BOOLEAN},
    {"showVideoAtDelay", FLOAT},
       {"effect", STRING},
       {"snapshotSource", STRING},
       {"reflexion", NORMALIZED_PAIR},
       {"reflexionOnFrame", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"animation",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"maxSize", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"stationary", STRING},
       {"metadataElement", BOOLEAN},
       {"path", PATH},
       {"speed", FLOAT},
       {"direction", STRING},
       {"iterationCount", UNSIGNED_INTEGER},
       {"interpolation", STRING},
       {"cornerRadius", FLOAT},
       {"color", COLOR},
       {"colorEnd", COLOR},
       {"gradientType", STRING},
       {"brightness", FLOAT},
       {"opacity", FLOAT},
       {"saturation", FLOAT},
       {"visible", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"badges",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"stationary", STRING},
       {"horizontalAlignment", STRING},
       {"direction", STRING},
       {"lines", UNSIGNED_INTEGER},
       {"itemsPerLine", UNSIGNED_INTEGER},
       {"itemMargin", NORMALIZED_PAIR},
       {"slots", STRING},
       {"controllerPos", NORMALIZED_PAIR},
       {"controllerSize", FLOAT},
       {"customBadgeIcon", PATH},
       {"customControllerIcon", PATH},
       {"folderLinkPos", NORMALIZED_PAIR},
       {"folderLinkSize", FLOAT},
       {"customFolderLinkIcon", PATH},
       {"badgeIconColor", COLOR},
       {"badgeIconColorEnd", COLOR},
       {"badgeIconGradientType", STRING},
       {"controllerIconColor", COLOR},
       {"controllerIconColorEnd", COLOR},
       {"controllerIconGradientType", STRING},
       {"folderLinkIconColor", COLOR},
       {"folderLinkIconColorEnd", COLOR},
       {"folderLinkIconGradientType", STRING},
       {"interpolation", STRING},
       {"opacity", FLOAT},
       {"visible", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"text",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
             {"maxSize", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"stationary", STRING},
       {"text", STRING},
       {"systemdata", STRING},
       {"metadata", STRING},
       {"defaultValue", STRING},
       {"systemNameSuffix", BOOLEAN},
       {"letterCaseSystemNameSuffix", STRING},
       {"metadataElement", BOOLEAN},
       {"gameselector", STRING},
       {"gameselectorEntry", UNSIGNED_INTEGER},
       {"container", BOOLEAN},
       {"containerType", STRING},
       {"containerVerticalSnap", BOOLEAN},
       {"containerScrollSpeed", FLOAT},
       {"containerStartDelay", FLOAT},
       {"containerResetDelay", FLOAT},
       {"containerScrollGap", FLOAT},
       {"fontPath", PATH},
       {"fontSize", FLOAT},
       {"horizontalAlignment", STRING},
       {"verticalAlignment", STRING},
       {"color", COLOR},
       {"backgroundColor", COLOR},
       {"backgroundMargins", NORMALIZED_PAIR},
       {"backgroundCornerRadius", FLOAT},
       {"letterCase", STRING},
       {"lineSpacing", FLOAT},
       {"glowColor", COLOR},
       {"glowSize", FLOAT},
             {"glowOffset", NORMALIZED_PAIR},
       {"forceUppercase", BOOLEAN},
       {"opacity", FLOAT},
       {"visible", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"datetime",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"stationary", STRING},
       {"metadata", STRING},
       {"defaultValue", STRING},
       {"gameselector", STRING},
       {"gameselectorEntry", UNSIGNED_INTEGER},
       {"fontPath", PATH},
       {"fontSize", FLOAT},
       {"horizontalAlignment", STRING},
       {"verticalAlignment", STRING},
       {"color", COLOR},
       {"backgroundColor", COLOR},
       {"backgroundMargins", NORMALIZED_PAIR},
       {"backgroundCornerRadius", FLOAT},
       {"letterCase", STRING},
       {"lineSpacing", FLOAT},
    {"glowColor", COLOR},
    {"glowSize", FLOAT},
    {"glowOffset", NORMALIZED_PAIR},
         {"forceUppercase", BOOLEAN},
       {"format", STRING},
       {"displayRelative", BOOLEAN},
       {"opacity", FLOAT},
       {"visible", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"gamelistinfo",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"stationary", STRING},
             {"text", STRING},
             {"systemdata", STRING},
             {"metadata", STRING},
             {"defaultValue", STRING},
             {"systemNameSuffix", BOOLEAN},
             {"letterCaseSystemNameSuffix", STRING},
             {"metadataElement", BOOLEAN},
       {"fontPath", PATH},
       {"fontSize", FLOAT},
       {"horizontalAlignment", STRING},
       {"verticalAlignment", STRING},
       {"color", COLOR},
       {"backgroundColor", COLOR},
             {"backgroundMargins", NORMALIZED_PAIR},
             {"backgroundCornerRadius", FLOAT},
             {"letterCase", STRING},
             {"lineSpacing", FLOAT},
             {"glowColor", COLOR},
             {"glowSize", FLOAT},
             {"glowOffset", NORMALIZED_PAIR},
             {"forceUppercase", BOOLEAN},
       {"opacity", FLOAT},
       {"visible", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"rating",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"stationary", STRING},
       {"hideIfZero", BOOLEAN},
       {"gameselector", STRING},
       {"gameselectorEntry", UNSIGNED_INTEGER},
       {"interpolation", STRING},
       {"color", COLOR},
    {"unfilledColor", COLOR},
       {"filledPath", PATH},
       {"unfilledPath", PATH},
       {"overlay", BOOLEAN},
       {"opacity", FLOAT},
       {"visible", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"gameselector",
      {{"selection", STRING},
       {"gameCount", UNSIGNED_INTEGER},
       {"allowDuplicates", BOOLEAN}}},
     {"helpsystem",
            {{"pos", NORMALIZED_PAIR},
             {"size", NORMALIZED_PAIR},
       {"posDimmed", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"originDimmed", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
             {"alignment", STRING},
       {"textColor", COLOR},
       {"textColorDimmed", COLOR},
       {"iconColor", COLOR},
       {"iconColorDimmed", COLOR},
       {"fontPath", PATH},
       {"fontSize", FLOAT},
       {"fontSizeDimmed", FLOAT},
       {"scope", STRING},
       {"entries", STRING},
       {"entryLayout", STRING},
       {"entryRelativeScale", FLOAT},
       {"entrySpacing", FLOAT},
       {"entrySpacingDimmed", FLOAT},
       {"iconTextSpacing", FLOAT},
       {"iconTextSpacingDimmed", FLOAT},
       {"letterCase", STRING},
       {"backgroundColor", COLOR},
       {"backgroundColorEnd", COLOR},
       {"backgroundGradientType", STRING},
       {"backgroundHorizontalPadding", NORMALIZED_PAIR},
       {"backgroundVerticalPadding", NORMALIZED_PAIR},
       {"backgroundCornerRadius", FLOAT},
       {"opacity", FLOAT},
       {"opacityDimmed", FLOAT},
    {"iconUpDown", PATH},
    {"iconLeftRight", PATH},
    {"iconUpDownLeftRight", PATH},
    {"iconA", PATH},
    {"iconB", PATH},
    {"iconX", PATH},
    {"iconY", PATH},
    {"iconL", PATH},
    {"iconR", PATH},
    {"iconStart", PATH},
    {"iconSelect", PATH},
       {"customButtonIcon", PATH}}},
     {"systemstatus",
      {{"pos", NORMALIZED_PAIR},
       {"height", FLOAT},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
       {"scope", STRING},
       {"fontPath", PATH},
       {"textRelativeScale", FLOAT},
       {"color", COLOR},
       {"colorEnd", COLOR},
       {"backgroundColor", COLOR},
       {"backgroundColorEnd", COLOR},
       {"backgroundGradientType", STRING},
       {"backgroundHorizontalPadding", NORMALIZED_PAIR},
       {"backgroundVerticalPadding", NORMALIZED_PAIR},
       {"backgroundCornerRadius", FLOAT},
       {"entries", STRING},
       {"entrySpacing", FLOAT},
       {"customIcon", PATH},
       {"opacity", FLOAT}}},
     {"clock",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
       {"rotation", FLOAT},
       {"rotationOrigin", NORMALIZED_PAIR},
             {"stationary", STRING},
       {"scope", STRING},
       {"fontPath", PATH},
       {"fontSize", FLOAT},
       {"horizontalAlignment", STRING},
       {"verticalAlignment", STRING},
       {"color", COLOR},
             {"glowColor", COLOR},
             {"glowSize", FLOAT},
             {"glowOffset", NORMALIZED_PAIR},
       {"backgroundColor", COLOR},
       {"backgroundColorEnd", COLOR},
       {"backgroundGradientType", STRING},
       {"backgroundHorizontalPadding", NORMALIZED_PAIR},
       {"backgroundVerticalPadding", NORMALIZED_PAIR},
       {"backgroundCornerRadius", FLOAT},
       {"format", STRING},
       {"opacity", FLOAT}}},
     {"sound",
      {{"path", PATH}}},
     {"imagegrid",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"origin", NORMALIZED_PAIR},
         {"staticImage", PATH},
         {"imageType", STRING},
             {"scaleOrigin", NORMALIZED_PAIR},
       {"defaultImage", PATH},
       {"defaultFolderImage", PATH},
         {"imageSource", STRING},
       {"itemSize", NORMALIZED_PAIR},
       {"itemScale", FLOAT},
       {"itemSpacing", NORMALIZED_PAIR},
         {"scaleInwards", BOOLEAN},
         {"fractionalRows", BOOLEAN},
         {"itemTransitions", STRING},
         {"rowTransitions", STRING},
         {"unfocusedItemOpacity", FLOAT},
         {"unfocusedItemSaturation", FLOAT},
         {"unfocusedItemDimming", FLOAT},
       {"imageFit", STRING},
         {"imageCropPos", NORMALIZED_PAIR},
         {"imageInterpolation", STRING},
         {"imageRelativeScale", FLOAT},
         {"imageCornerRadius", FLOAT},
       {"imageColor", COLOR},
         {"imageColorEnd", COLOR},
         {"imageGradientType", STRING},
       {"imageSelectedColor", COLOR},
         {"imageSelectedColorEnd", COLOR},
         {"imageSelectedGradientType", STRING},
         {"imageBrightness", FLOAT},
         {"imageSaturation", FLOAT},
         {"backgroundImage", PATH},
         {"backgroundRelativeScale", FLOAT},
         {"backgroundCornerRadius", FLOAT},
         {"backgroundColor", COLOR},
         {"backgroundColorEnd", COLOR},
         {"backgroundGradientType", STRING},
         {"selectorImage", PATH},
         {"selectorRelativeScale", FLOAT},
         {"selectorCornerRadius", FLOAT},
         {"selectorLayer", STRING},
         {"selectorColor", COLOR},
         {"selectorColorEnd", COLOR},
         {"selectorGradientType", STRING},
         {"text", STRING},
         {"textRelativeScale", FLOAT},
         {"textBackgroundCornerRadius", FLOAT},
       {"fontPath", PATH},
       {"fontSize", FLOAT},
       {"letterCase", STRING},
         {"letterCaseAutoCollections", STRING},
         {"letterCaseCustomCollections", STRING},
         {"lineSpacing", FLOAT},
         {"systemNameSuffix", BOOLEAN},
         {"letterCaseSystemNameSuffix", STRING},
         {"fadeAbovePrimary", BOOLEAN},
       {"textColor", COLOR},
         {"textBackgroundColor", COLOR},
       {"textSelectedColor", COLOR},
         {"textSelectedBackgroundColor", COLOR},
         {"textHorizontalScrolling", BOOLEAN},
         {"textHorizontalScrollSpeed", FLOAT},
         {"textHorizontalScrollDelay", FLOAT},
         {"textHorizontalScrollGap", FLOAT},
       {"autoLayout", NORMALIZED_PAIR},
       {"autoLayoutSelectedZoom", FLOAT},
       {"scrollDirection", STRING},
       {"centerSelection", BOOLEAN},
       {"scrollLoop", BOOLEAN},
         {"padding", NORMALIZED_RECT},
         {"selectionMode", STRING},
    {"animateSelection", BOOLEAN},
         {"animateColor", COLOR},
         {"animateColorTime", FLOAT},
         {"logoSize", NORMALIZED_PAIR},
         {"maxLogoCount", FLOAT},
         {"logoScale", FLOAT},
         {"systemInfoDelay", FLOAT},
         {"logoAlignment", STRING},
         {"logoRotation", FLOAT},
         {"logoRotationOrigin", NORMALIZED_PAIR},
         {"margin", NORMALIZED_PAIR},
       {"opacity", FLOAT},
         {"showVideoAtDelay", FLOAT},
       {"visible", BOOLEAN},
       {"zIndex", FLOAT}}},
         {"gridtile",
            {{"size", NORMALIZED_PAIR},
         {"maxSize", NORMALIZED_PAIR},
             {"padding", NORMALIZED_PAIR},
             {"imageColor", COLOR},
             {"backgroundColor", COLOR},
         {"backgroundCenterColor", COLOR},
         {"backgroundEdgeColor", COLOR},
             {"textColor", COLOR},
             {"textBackgroundColor", COLOR},
             {"imageCornerRadius", FLOAT},
         {"backgroundCornerSize", NORMALIZED_PAIR},
         {"reflexion", NORMALIZED_PAIR},
             {"selectionMode", STRING},
             {"imageSizeMode", STRING},
             {"visible", BOOLEAN}}},
     {"controllerActivity",
      {{"pos", NORMALIZED_PAIR},
             {"size", NORMALIZED_PAIR},
       {"height", FLOAT},
       {"origin", NORMALIZED_PAIR},
             {"itemSpacing", FLOAT},
         {"horizontalAlignment", STRING},
             {"imagePath", PATH},
         {"gunPath", PATH},
         {"wheelPath", PATH},
         {"networkIcon", PATH},
         {"planemodeIcon", PATH},
         {"incharge", PATH},
         {"full", PATH},
         {"at75", PATH},
         {"at50", PATH},
         {"at25", PATH},
         {"empty", PATH},
             {"color", COLOR},
       {"activityColor", COLOR},
       {"hotkeyColor", COLOR},
       {"visible", BOOLEAN},
       {"zIndex", FLOAT}}},
         {"batteryIndicator",
        {{"pos", NORMALIZED_PAIR},
         {"size", NORMALIZED_PAIR},
         {"itemSpacing", FLOAT},
         {"horizontalAlignment", STRING},
         {"imagePath", PATH},
         {"networkIcon", PATH},
         {"planemodeIcon", PATH},
         {"incharge", PATH},
         {"full", PATH},
         {"at75", PATH},
         {"at50", PATH},
         {"at25", PATH},
         {"empty", PATH},
         {"color", COLOR},
         {"activityColor", COLOR},
         {"hotkeyColor", COLOR},
         {"visible", BOOLEAN},
         {"zIndex", FLOAT}}},
     {"ninepatch",
      {{"pos", NORMALIZED_PAIR},
       {"size", NORMALIZED_PAIR},
       {"x", FLOAT},
       {"y", FLOAT},
       {"w", FLOAT},
       {"h", FLOAT},
       {"origin", NORMALIZED_PAIR},
             {"padding", NORMALIZED_PAIR},
       {"path", PATH},
       {"cornerSize", NORMALIZED_PAIR},
       {"centerColor", COLOR},
       {"edgeColor", COLOR},
       {"color", COLOR},
        {"animateColor", COLOR},
        {"animateColorTime", FLOAT},
       {"visible", BOOLEAN},
       {"zIndex", FLOAT}}},
     {"menuBackground",
      {{"color", COLOR},
             {"centerColor", COLOR},
       {"cornerSize", NORMALIZED_PAIR},
       {"path", PATH},
       {"fadePath", PATH},
             {"scrollbarColor", COLOR},
             {"scrollbarSize", FLOAT},
             {"scrollbarCorner", FLOAT},
             {"scrollbarAlignment", STRING},
       {"opacity", FLOAT}}},
     {"menuIcons",
      {{"iconSystem", PATH},
       {"iconUpdates", PATH},
       {"iconControllers", PATH},
       {"iconGames", PATH},
       {"iconUI", PATH},
       {"iconSound", PATH},
       {"iconNetwork", PATH},
       {"iconScraper", PATH},
       {"iconAdvanced", PATH},
       {"iconQuit", PATH},
       {"iconKodi", PATH},
       {"iconRetroachievements", PATH},
       {"iconFastShutdown", PATH},
       {"iconShutdown", PATH},
       {"iconRestart", PATH}}},
         {"menuGrid",
            {{"separatorColor", COLOR}}},
     {"menuText",
      {{"fontPath", PATH},
       {"fontSize", FLOAT},
       {"color", COLOR},
       {"separatorColor", COLOR},
       {"selectedColor", COLOR},
             {"selectorOffsetY", FLOAT},
             {"selectorColor", COLOR},
             {"selectorColorEnd", COLOR},
             {"selectorGradientType", STRING}}},
     {"menuTextSmall",
      {{"fontPath", PATH},
       {"fontSize", FLOAT},
       {"color", COLOR}}},
     {"menuTextEdit",
      {{"color", COLOR},
       {"inactive", COLOR},
             {"active", COLOR},
             {"inactivePath", PATH},
             {"activePath", PATH}}},
         {"menuSwitch",
            {{"color", COLOR},
             {"inactive", COLOR},
             {"active", COLOR},
             {"pathOn", PATH},
             {"pathOff", PATH}}},
         {"menuSlider",
            {{"color", COLOR},
             {"inactive", COLOR},
             {"active", COLOR},
             {"path", PATH}}},
     {"menuButton",
            {{"path", PATH},
             {"filledPath", PATH},
             {"cornerSize", NORMALIZED_PAIR},
             {"color", COLOR},
       {"colorFocused", COLOR},
       {"textColor", COLOR},
       {"textColorFocused", COLOR}}},
     {"menuGroup",
      {{"color", COLOR},
       {"separatorColor", COLOR},
             {"backgroundColor", COLOR},
       {"fontPath", PATH},
             {"fontSize", FLOAT},
             {"lineSpacing", FLOAT},
         {"alignment", STRING},
         {"visible", BOOLEAN}}}};
// clang-format on

ThemeData::ThemeData()
    : mCustomCollection {false}
{
    sCurrentTheme = sThemes.find(Settings::getInstance()->getString("Theme"));
    sVariantDefinedTransitions = "";
}

ThemeData::BatoceraTagTranslation ThemeData::translateBatoceraTag(const std::string& tagName)
{
    const std::string normalizedTag {Utils::String::toLower(Utils::String::trim(tagName))};

    if (normalizedTag == "subset") {
        return {"subset",
                {{"subsetdisplayname", "displayName"},
                 {"subsetDisplayName", "displayName"},
                 {"appliesto", "appliesTo"},
                 {"ifsubset", "ifSubset"},
                 {"ifsubsetcondition", "ifSubset"},
                 {"if", "if"}},
                true};
    }

    if (normalizedTag == "include") {
        return {"include",
                {{"lang", "lang"},
                 {"language", "lang"},
                 {"locale", "lang"},
                 {"iflang", "lang"},
                 {"region", "region"},
                 {"country", "region"},
                 {"ifsubset", "ifSubset"},
                 {"ifsubsetcondition", "ifSubset"},
                 {"ifarch", "ifArch"},
                 {"ifnotarch", "ifNotArch"},
                 {"ifcheevos", "ifCheevos"},
                 {"ifnetplay", "ifNetplay"},
                 {"ifhelpprompts", "ifHelpPrompts"},
                 {"iflocale", "lang"},
                 {"iflanguage", "lang"},
                 {"ifregion", "region"},
                 {"ifcountry", "region"},
                 {"verticalscreen", "verticalScreen"},
                 {"tinyscreen", "tinyScreen"},
                 {"subSetDisplayName", "displayName"},
                 {"subsetdisplayname", "displayName"},
                 {"appliesto", "appliesTo"},
                 {"path", "path"},
                 {"file", "path"},
                 {"filepath", "path"},
                 {"src", "path"},
                 {"href", "path"}},
                true};
    }

    return {normalizedTag, {}, false};
}

std::string ThemeData::translateBatoceraVariable(const std::string& variableName)
{
    const std::string rawName {Utils::String::trim(variableName)};
    const std::string normalizedName {Utils::String::toLower(rawName)};

    if (normalizedName == "themepath")
        return "themePath";
    if (normalizedName == "currentpath")
        return "currentPath";

    if (normalizedName == "global.locale" || normalizedName == "locale" ||
        normalizedName == "language") {
        return "global.language";
    }

    if (normalizedName == "global.cheevosuser" || normalizedName == "cheevos.username")
        return "global.cheevos.username";

    if (normalizedName == "global.netplayuser" || normalizedName == "netplay.username")
        return "global.netplay.username";

    if (normalizedName == "country" || normalizedName == "global.country" ||
        normalizedName == "global.region") {
        return "region";
    }

    if (normalizedName == "global.screenwidth")
        return "screen.width";
    if (normalizedName == "global.screenheight")
        return "screen.height";
    if (normalizedName == "global.screenratio")
        return "screen.ratio";
    if (normalizedName == "global.vertical")
        return "screen.vertical";

    return rawName;
}

    void ThemeData::setRuntimeVariables(const std::map<std::string, std::string>& variables)
    {
        for (const auto& [key, value] : variables) {
            auto it = mVariables.find(key);
            if (it != mVariables.end())
                it->second = value;
            else
                mVariables.insert(std::make_pair(key, value));
        }
    }

void ThemeData::loadFile(const std::map<std::string, std::string>& sysDataMap,
                         const std::string& path,
                         const ThemeTriggers::TriggerType trigger,
                         const bool customCollection)
{
    mCustomCollection = customCollection;
    mSelectedGamelistView = "";
    mSelectedSystemView = "";
    mSubsetSelections.clear();
    mAutoSelectedGamelistStyle = "";
    mOverrideVariant = "";

    mPaths.push_back(path);

    ThemeException error;
    error << "ThemeData::loadFile(): ";
    error.setFiles(mPaths);

    if (!Utils::FileSystem::exists(path))
        throw error << "File does not exist";

    mViews.clear();
    mVariables.clear();

    mVariables.insert(sysDataMap.cbegin(), sysDataMap.cend());

    pugi::xml_document doc;
#if defined(_WIN64)
    pugi::xml_parse_result res {doc.load_file(Utils::String::stringToWideString(path).c_str())};
#else
    pugi::xml_parse_result res {doc.load_file(path.c_str())};
#endif
    if (!res)
        throw error << ": XML parsing error: " << res.description();

    pugi::xml_node root {doc.child("theme")};
    if (!root)
        throw error << ": Missing <theme> tag";

    // Ignore optional formatVersion to improve compatibility with batocera themes.

    if (sCurrentTheme->second.capabilities.variants.size() > 0) {
        std::vector<std::string> gamelistViews;

        for (auto& variant : sCurrentTheme->second.capabilities.variants) {
            if (variant.gamelistViewStyle)
                gamelistViews.emplace_back(variant.name);
            else
                mVariants.emplace_back(variant.name);
        }

        if (!mVariants.empty()) {
            if (std::find(mVariants.cbegin(), mVariants.cend(),
                          Settings::getInstance()->getString("ThemeVariant")) != mVariants.cend())
                mSelectedVariant = Settings::getInstance()->getString("ThemeVariant");
            else
                mSelectedVariant = mVariants.front();

            // Special shortcut variant to apply configuration to all defined variants.
            mVariants.emplace_back("all");

            if (trigger != ThemeTriggers::TriggerType::NONE) {
                auto overrides = getCurrentThemeSelectedVariantOverrides();
                if (overrides.find(trigger) != overrides.end())
                    mOverrideVariant = overrides.at(trigger).first;
            }
        }

        if (!gamelistViews.empty()) {
            std::string requestedView {Settings::getInstance()->getString("ThemeGamelistView")};

            // Backward compatibility for earlier builds that reused ThemeVariant.
            if (requestedView.empty() &&
                std::find(gamelistViews.cbegin(), gamelistViews.cend(),
                          Settings::getInstance()->getString("ThemeVariant")) !=
                    gamelistViews.cend()) {
                requestedView = Settings::getInstance()->getString("ThemeVariant");
            }

            if (std::find(gamelistViews.cbegin(), gamelistViews.cend(), requestedView) !=
                gamelistViews.cend())
                mSelectedGamelistView = requestedView;
            else
                mSelectedGamelistView = gamelistViews.front();
        }
    }

    // Initialize subset selections from settings.
    if (!sCurrentTheme->second.capabilities.subsets.empty()) {
        for (const auto& subset : sCurrentTheme->second.capabilities.subsets) {
            const std::string normalizedSubsetName {normalizeThemeSubsetName(subset.name)};
            std::string savedVal {getPersistedThemeSubsetValue(subset.name)};

            for (const std::string& settingKey : getThemeSubsetSettingKeys(subset.name)) {
                if (Settings::getInstance()->hasString(settingKey))
                    Settings::getInstance()->setString(settingKey, Settings::getInstance()->getString(settingKey));
            }

            // Validate the saved value.
            bool valid = false;
            for (const auto& opt : subset.options) {
                if (opt.first == savedVal) {
                    valid = true;
                    break;
                }
            }
            const std::string effectiveVal = valid ? savedVal : subset.options.front().first;

            if (normalizedSubsetName == "systemview") {
                mSelectedSystemView = effectiveVal;
            }
            else {
                mSubsetSelections[subset.name] = effectiveVal;
                mSubsetSelections[normalizedSubsetName] = effectiveVal;
            }
        }
    }

    if (sCurrentTheme->second.capabilities.colorSchemes.size() > 0) {
        for (auto& colorScheme : sCurrentTheme->second.capabilities.colorSchemes)
            mColorSchemes.emplace_back(colorScheme.name);

        if (std::find(mColorSchemes.cbegin(), mColorSchemes.cend(),
                      Settings::getInstance()->getString("ThemeColorScheme")) !=
            mColorSchemes.cend())
            mSelectedColorScheme = Settings::getInstance()->getString("ThemeColorScheme");
        else
            mSelectedColorScheme = mColorSchemes.front();
    }

    if (sCurrentTheme->second.capabilities.fontSizes.size() > 0) {
        for (auto& fontSize : sCurrentTheme->second.capabilities.fontSizes)
            mFontSizes.emplace_back(fontSize);

        if (std::find(mFontSizes.cbegin(), mFontSizes.cend(),
                      Settings::getInstance()->getString("ThemeFontSize")) != mFontSizes.cend())
            mSelectedFontSize = Settings::getInstance()->getString("ThemeFontSize");
        else
            mSelectedFontSize = mFontSizes.front();
    }

    sAspectRatioMatch = false;
    sThemeLanguage = "";

    if (sCurrentTheme->second.capabilities.aspectRatios.size() > 0) {
        if (std::find(sCurrentTheme->second.capabilities.aspectRatios.cbegin(),
                      sCurrentTheme->second.capabilities.aspectRatios.cend(),
                      Settings::getInstance()->getString("ThemeAspectRatio")) !=
            sCurrentTheme->second.capabilities.aspectRatios.cend())
            sSelectedAspectRatio = Settings::getInstance()->getString("ThemeAspectRatio");
        else
            sSelectedAspectRatio = sCurrentTheme->second.capabilities.aspectRatios.front();

        if (sSelectedAspectRatio == "automatic") {
            // Auto-detect the closest aspect ratio based on what's available in the theme config.
            sSelectedAspectRatio = "16:9";
            const float screenAspectRatio {Renderer::getScreenAspectRatio()};
            float diff {std::fabs(sAspectRatioMap["16:9"] - screenAspectRatio)};

            for (auto& aspectRatio : sCurrentTheme->second.capabilities.aspectRatios) {
                if (aspectRatio == "automatic")
                    continue;

                if (sAspectRatioMap.find(aspectRatio) != sAspectRatioMap.end()) {
                    const float newDiff {
                        std::fabs(sAspectRatioMap[aspectRatio] - screenAspectRatio)};
                    if (newDiff < 0.01f)
                        sAspectRatioMatch = true;
                    if (newDiff < diff) {
                        diff = newDiff;
                        sSelectedAspectRatio = aspectRatio;
                    }
                }
            }
        }
    }

    if (sCurrentTheme->second.capabilities.languages.size() > 0) {
        for (auto& language : sCurrentTheme->second.capabilities.languages)
            mLanguages.emplace_back(language);

        std::string langSetting {Settings::getInstance()->getString("ThemeLanguage")};
        if (langSetting == "automatic")
            langSetting = Utils::Localization::sCurrentLocale;

        // Check if there is an exact match.
        if (std::find(sCurrentTheme->second.capabilities.languages.cbegin(),
                      sCurrentTheme->second.capabilities.languages.cend(),
                      langSetting) != sCurrentTheme->second.capabilities.languages.cend()) {
            sThemeLanguage = langSetting;
        }
        else {
            // We assume all locales are in the correct format.
            const std::string currLanguage {langSetting.substr(0, 2)};
            // Select the closest matching locale (i.e. same language but possibly for a
            // different country).
            for (const auto& lang : sCurrentTheme->second.capabilities.languages) {
                if (lang.substr(0, 2) == currLanguage) {
                    sThemeLanguage = lang;
                    break;
                }
            }
            // If there is no match then fall back to the default language en_US, which is
            // mandatory for all themes that provide language support.
            if (sThemeLanguage == "")
                sThemeLanguage = "en_US";
        }
    }

    mIsBatoceraTheme = sCurrentTheme->second.capabilities.batoceraFormat ||
                       detectBatoceraThemeRoot(root);

    parseVariables(root);
    // Some Batocera themes define color/icon variables inside <subset> includes and then
    // reference those variables from base <view> entries. Preload subset variables so
    // placeholder resolution works when parsing views below.
    mSubsetVariableVisitedPaths.clear();
    if (mIsBatoceraTheme)
        parseSubsetVariables(root);
    parseColorSchemes(root);
    parseFontSizes(root);
    parseLanguages(root);
    parseIncludes(root);
    parseViews(root);
    parseFeatures(root);
    parseVariants(root);
    parseCustomViews(root);
    if (mIsBatoceraTheme)
        parseSubsets(root);
    parseAspectRatios(root);
}

bool ThemeData::hasView(const std::string& view)
{
    auto viewIt = mViews.find(view);
    return (viewIt != mViews.cend());
}

std::vector<ThemeData::ThemeConfigurableSubset>
ThemeData::getConfigurableSubsets(const std::string& gamelistViewStyle) const
{
    std::vector<ThemeConfigurableSubset> configurableSubsets;

    if (sCurrentTheme == sThemes.end())
        return configurableSubsets;

    const std::string normalizedViewStyle {
        Utils::String::toLower(Utils::String::trim(gamelistViewStyle.empty() ?
                                                       mSelectedGamelistView :
                                                       gamelistViewStyle))};

    for (const ThemeSubset& subset : sCurrentTheme->second.capabilities.subsets) {
        if (subset.options.empty())
            continue;

        if (!subset.ifSubsetCondition.empty() &&
            !evaluateIfSubsetCondition(subset.ifSubsetCondition)) {
            continue;
        }

        if (!subset.appliesTo.empty() && !normalizedViewStyle.empty() &&
            normalizedViewStyle != "none") {
            bool matchesView {false};
            for (const std::string& appliesTo : subset.appliesTo) {
                if (Utils::String::toLower(Utils::String::trim(appliesTo)) == normalizedViewStyle) {
                    matchesView = true;
                    break;
                }
            }

            if (!matchesView)
                continue;
        }

        ThemeConfigurableSubset configurableSubset;
        configurableSubset.subset = subset;
        configurableSubset.selectedValue =
            getResolvedThemeSubsetSelection(subset.name, mSelectedSystemView, mSubsetSelections);
        configurableSubset.role = classifyThemeSubset(subset);
        configurableSubsets.emplace_back(std::move(configurableSubset));
    }

    return configurableSubsets;
}

const ThemeData::ThemeElement* ThemeData::getElement(const std::string& view,
                                                     const std::string& element,
                                                     const std::string& expectedType) const
{
    auto viewIt = mViews.find(view);
    if (viewIt == mViews.cend())
        return nullptr; // Not found.

    auto elemIt = viewIt->second.elements.find(element);

    // Fallback: elements are stored as "type_name" internally (e.g. "menuIcons_menuicons").
    // If the plain-name lookup fails, try the prefixed form.
    if (elemIt == viewIt->second.elements.cend() && !expectedType.empty()) {
        elemIt = viewIt->second.elements.find(expectedType + "_" + element);
    }

    if (elemIt == viewIt->second.elements.cend())
        return nullptr;

    // If expectedType is an empty string, then skip type checking.
    const bool compatibleGridType {
        expectedType == "grid" && elemIt->second.type == "imagegrid"};

    if (elemIt->second.type != expectedType && !expectedType.empty() && !compatibleGridType) {
        LOG(LogWarning) << "ThemeData::getElement(): Requested element \"" << view << "." << element
                        << "\" has the wrong type, expected \"" << expectedType << "\", got \""
                        << elemIt->second.type << "\"";
        return nullptr;
    }

    return &elemIt->second;
}

void ThemeData::populateThemes()
{
    sThemes.clear();
    LOG(LogInfo) << "Checking for available themes...";

    // Check for themes first under the user theme directory (which is in the ES-DE home directory
    // by default), then under the data installation directory (Unix only) and last under the ES-DE
    // binary directory.
#if defined(__ANDROID__)
    const std::string userThemeDirectory {Utils::FileSystem::getInternalAppDataDirectory() +
                                          "/themes"};
#elif defined(__IOS__)
    const std::string userThemeDirectory;
#else
    const std::string defaultUserThemeDir {Utils::FileSystem::getAppDataDirectory() + "/themes"};
    const std::string userThemeDirSetting {Utils::FileSystem::expandHomePath(
        Settings::getInstance()->getString("UserThemeDirectory"))};
    std::string userThemeDirectory;

    if (userThemeDirSetting.empty()) {
        userThemeDirectory = defaultUserThemeDir;
    }
    else if (Utils::FileSystem::isDirectory(userThemeDirSetting) ||
             Utils::FileSystem::isSymlink(userThemeDirSetting)) {
        userThemeDirectory = userThemeDirSetting;
#if defined(_WIN64)
        LOG(LogInfo) << "Setting user theme directory to \""
                     << Utils::String::replace(userThemeDirectory, "/", "\\") << "\"";
#else
        LOG(LogInfo) << "Setting user theme directory to \"" << userThemeDirectory << "\"";
#endif
    }
    else {
        LOG(LogWarning) << "Requested user theme directory \"" << userThemeDirSetting
                        << "\" does not exist or is not a directory, reverting to \""
                        << defaultUserThemeDir << "\"";
        userThemeDirectory = defaultUserThemeDir;
    }
#endif

#if defined(__ANDROID__)
    const std::vector<std::string> themePaths {Utils::FileSystem::getProgramDataPath() + "/themes",
                                               Utils::FileSystem::getAppDataDirectory() + "/themes",
                                               userThemeDirectory};
#elif defined(__IOS__)
    const std::vector<std::string> themePaths {Utils::Platform::iOS::getPackagePath() + "themes",
                                               Utils::FileSystem::getAppDataDirectory() +
                                                   "/themes"};
#elif defined(__APPLE__)
    const std::vector<std::string> themePaths {
        Utils::FileSystem::getExePath() + "/themes",
        Utils::FileSystem::getExePath() + "/../Resources/themes", userThemeDirectory};
#elif defined(_WIN64) || defined(APPIMAGE_BUILD)
    const std::vector<std::string> themePaths {Utils::FileSystem::getExePath() + "/themes",
                                               userThemeDirectory};
#else
    const std::vector<std::string> themePaths {Utils::FileSystem::getProgramDataPath() + "/themes",
                                               userThemeDirectory};
#endif

    for (auto path : themePaths) {
        if (!Utils::FileSystem::isDirectory(path))
            continue;

        Utils::FileSystem::StringList dirContent {Utils::FileSystem::getDirContent(path)};

        for (Utils::FileSystem::StringList::const_iterator it = dirContent.cbegin();
             it != dirContent.cend(); ++it) {
            if (Utils::FileSystem::isDirectory(*it)) {
                const std::string themeDirName {Utils::FileSystem::getFileName(*it)};
                if (themeDirName == "themes-list" ||
                    (themeDirName.length() >= 8 &&
                     Utils::String::toLower(themeDirName.substr(themeDirName.length() - 8, 8)) ==
                         "disabled"))
                    continue;
#if defined(_WIN64)
                LOG(LogDebug) << "Loading theme capabilities for \""
                              << Utils::String::replace(*it, "/", "\\") << "\"...";
#else
                LOG(LogDebug) << "Loading theme capabilities for \"" << *it << "\"...";
#endif
                ThemeCapability capabilities {parseThemeCapabilities((*it))};

                if (!capabilities.validTheme)
                    continue;

                std::string themeName;
                if (capabilities.themeName != "")
                    themeName.append(" (\"").append(capabilities.themeName).append("\")");

#if defined(_WIN64)
                LOG(LogInfo) << "Added theme \"" << Utils::String::replace(*it, "/", "\\") << "\""
                             << themeName;
#else
                LOG(LogInfo) << "Added theme \"" << *it << "\"" << themeName;
#endif
                int aspectRatios {0};
                int languages {0};
                if (capabilities.aspectRatios.size() > 0)
                    aspectRatios = static_cast<int>(capabilities.aspectRatios.size()) - 1;
                if (capabilities.languages.size() > 0)
                    languages = static_cast<int>(capabilities.languages.size()) - 1;
                LOG(LogDebug) << "Theme includes support for " << capabilities.variants.size()
                              << " variant" << (capabilities.variants.size() != 1 ? "s" : "")
                              << ", " << capabilities.colorSchemes.size() << " color scheme"
                              << (capabilities.colorSchemes.size() != 1 ? "s" : "") << ", "
                              << capabilities.fontSizes.size() << " font size"
                              << (capabilities.fontSizes.size() != 1 ? "s" : "") << ", "
                              << aspectRatios << " aspect ratio" << (aspectRatios != 1 ? "s" : "")
                              << ", " << capabilities.transitions.size() << " transition"
                              << (capabilities.transitions.size() != 1 ? "s" : "") << " and "
                              << languages << " language" << (languages != 1 ? "s" : "");

                Theme theme {*it, capabilities};
                sThemes[theme.getName()] = theme;
            }
        }
    }

    if (sThemes.empty()) {
        LOG(LogWarning) << "Couldn't find any themes, creating dummy entry";
        Theme theme {"no-themes", ThemeCapability()};
        sThemes[theme.getName()] = theme;
        sCurrentTheme = sThemes.begin();
    }
}

const std::string ThemeData::getSystemThemeFile(const std::string& system)
{
    if (sThemes.empty())
        getThemes();

    if (sThemes.empty())
        return "";

    std::map<std::string, Theme, StringComparator>::const_iterator theme {
        sThemes.find(Settings::getInstance()->getString("Theme"))};
    if (theme == sThemes.cend()) {
        // Currently configured theme is missing, attempt to load the default theme linear-es-de
        // instead, and if that's also missing then pick the first available one.
        bool defaultSetFound {true};

        theme = sThemes.find("linear-es-de");

        if (theme == sThemes.cend()) {
            theme = sThemes.cbegin();
            defaultSetFound = false;
        }

        LOG(LogWarning) << "Configured theme \"" << Settings::getInstance()->getString("Theme")
                        << "\" does not exist, loading" << (defaultSetFound ? " default " : " ")
                        << "theme \"" << theme->first << "\" instead";

        Settings::getInstance()->setString("Theme", theme->first);
        sCurrentTheme = sThemes.find(Settings::getInstance()->getString("Theme"));
    }

    return theme->second.getThemePath(system);
}

const std::string ThemeData::getFontSizeLabel(const std::string& fontSize)
{
    auto it = std::find_if(sSupportedFontSizes.cbegin(), sSupportedFontSizes.cend(),
                           [&fontSize](const std::pair<std::string, std::string>& entry) {
                               return entry.first == fontSize;
                           });
    if (it != sSupportedFontSizes.cend())
        return it->second;
    else
        return "invalid font size";
}

const std::string ThemeData::getAspectRatioLabel(const std::string& aspectRatio)
{
    auto it = std::find_if(sSupportedAspectRatios.cbegin(), sSupportedAspectRatios.cend(),
                           [&aspectRatio](const std::pair<std::string, std::string>& entry) {
                               return entry.first == aspectRatio;
                           });
    if (it != sSupportedAspectRatios.cend())
        return it->second;
    else
        return "invalid ratio";
}

const std::string ThemeData::getLanguageLabel(const std::string& language)
{
    auto it = std::find_if(sSupportedLanguages.cbegin(), sSupportedLanguages.cend(),
                           [&language](const std::pair<std::string, std::string>& entry) {
                               return entry.first == language;
                           });
    if (it != sSupportedLanguages.cend())
        return it->second;
    else
        return "invalid language";
}

void ThemeData::setThemeTransitions()
{
    auto setTransitionsFunc = [](int transitionAnim) {
        Settings::getInstance()->setInt("TransitionsSystemToSystem", transitionAnim);
        Settings::getInstance()->setInt("TransitionsSystemToGamelist", transitionAnim);
        Settings::getInstance()->setInt("TransitionsGamelistToGamelist", transitionAnim);
        Settings::getInstance()->setInt("TransitionsGamelistToSystem", transitionAnim);
        Settings::getInstance()->setInt("TransitionsStartupToSystem", transitionAnim);
        Settings::getInstance()->setInt("TransitionsStartupToGamelist", transitionAnim);
    };

    int transitionAnim {ViewTransitionAnimation::INSTANT};
    setTransitionsFunc(transitionAnim);

    const std::string& transitionsSetting {Settings::getInstance()->getString("ThemeTransitions")};
    std::string profile;
    size_t profileEntry {0};

    if (transitionsSetting == "automatic") {
        if (sVariantDefinedTransitions != "")
            profile = sVariantDefinedTransitions;
        else if (!sCurrentTheme->second.capabilities.transitions.empty())
            profile = sCurrentTheme->second.capabilities.transitions.front().name;
    }
    else {
        profile = transitionsSetting;
    }

    auto it = std::find_if(
        sCurrentTheme->second.capabilities.transitions.cbegin(),
        sCurrentTheme->second.capabilities.transitions.cend(),
        [&profile](const ThemeTransitions transitions) { return transitions.name == profile; });
    if (it != sCurrentTheme->second.capabilities.transitions.cend())
        profileEntry = static_cast<size_t>(
            std::distance(sCurrentTheme->second.capabilities.transitions.cbegin(), it) + 1);

    if (profileEntry != 0 &&
        sCurrentTheme->second.capabilities.transitions.size() > profileEntry - 1) {
        auto transitionMap =
            sCurrentTheme->second.capabilities.transitions[profileEntry - 1].animations;
        if (transitionMap.find(ViewTransition::SYSTEM_TO_SYSTEM) != transitionMap.end())
            Settings::getInstance()->setInt("TransitionsSystemToSystem",
                                            transitionMap[ViewTransition::SYSTEM_TO_SYSTEM]);
        if (transitionMap.find(ViewTransition::SYSTEM_TO_GAMELIST) != transitionMap.end())
            Settings::getInstance()->setInt("TransitionsSystemToGamelist",
                                            transitionMap[ViewTransition::SYSTEM_TO_GAMELIST]);
        if (transitionMap.find(ViewTransition::GAMELIST_TO_GAMELIST) != transitionMap.end())
            Settings::getInstance()->setInt("TransitionsGamelistToGamelist",
                                            transitionMap[ViewTransition::GAMELIST_TO_GAMELIST]);
        if (transitionMap.find(ViewTransition::GAMELIST_TO_SYSTEM) != transitionMap.end())
            Settings::getInstance()->setInt("TransitionsGamelistToSystem",
                                            transitionMap[ViewTransition::GAMELIST_TO_SYSTEM]);
        if (transitionMap.find(ViewTransition::STARTUP_TO_SYSTEM) != transitionMap.end())
            Settings::getInstance()->setInt("TransitionsStartupToSystem",
                                            transitionMap[ViewTransition::STARTUP_TO_SYSTEM]);
        if (transitionMap.find(ViewTransition::STARTUP_TO_GAMELIST) != transitionMap.end())
            Settings::getInstance()->setInt("TransitionsStartupToGamelist",
                                            transitionMap[ViewTransition::STARTUP_TO_GAMELIST]);
    }
    else if (transitionsSetting == "builtin-slide" || transitionsSetting == "builtin-fade") {
        if (std::find(sCurrentTheme->second.capabilities.suppressedTransitionProfiles.cbegin(),
                      sCurrentTheme->second.capabilities.suppressedTransitionProfiles.cend(),
                      transitionsSetting) ==
            sCurrentTheme->second.capabilities.suppressedTransitionProfiles.cend()) {
            if (transitionsSetting == "builtin-slide") {
                transitionAnim = static_cast<int>(ViewTransitionAnimation::SLIDE);
            }
            else if (transitionsSetting == "builtin-fade") {
                transitionAnim = static_cast<int>(ViewTransitionAnimation::FADE);
            }
            setTransitionsFunc(transitionAnim);
        }
    }
}

const std::map<ThemeTriggers::TriggerType, std::pair<std::string, std::vector<std::string>>>
ThemeData::getCurrentThemeSelectedVariantOverrides()
{
    const auto variantIter = std::find_if(
        sCurrentTheme->second.capabilities.variants.cbegin(),
        sCurrentTheme->second.capabilities.variants.cend(),
        [this](ThemeVariant currVariant) { return currVariant.name == mSelectedVariant; });

    if (variantIter != sCurrentTheme->second.capabilities.variants.cend() &&
        !(*variantIter).overrides.empty())
        return (*variantIter).overrides;
    else
        return ThemeVariant().overrides;
}

const void ThemeData::themeLoadedLogOutput()
{
    LOG(LogInfo) << "Finished loading theme \"" << sCurrentTheme->first << "\"";

    if (sSelectedAspectRatio != "") {
        const bool autoDetect {Settings::getInstance()->getString("ThemeAspectRatio") ==
                               "automatic"};
        const std::string match {sAspectRatioMatch ? "exact match " : "closest match "};

        LOG(LogInfo) << "Aspect ratio " << (autoDetect ? "automatically " : "manually ")
                     << "set to " << (autoDetect ? match : "") << "\""
                     << Utils::String::replace(sSelectedAspectRatio, "_", " ") << "\"";
    }

    if (sThemeLanguage != "") {
        LOG(LogInfo) << "Theme language set to \"" << sThemeLanguage << "\"";
    }
    else {
        LOG(LogInfo) << "Theme does not have multilingual support";
    }
}

unsigned int ThemeData::getHexColor(const std::string& str)
{
    ThemeException error;

    if (str == "")
        throw error << "Empty color property";

    const size_t length {str.size()};
    if (length != 2 && length != 6 && length != 8)
        throw error << "Invalid color property \"" << str
                    << "\" (must be 2, 6 or 8 characters in length)";

    unsigned int value;
    std::stringstream ss;
    ss << str;
    ss >> std::hex >> value;

    // Batocera themes sometimes provide a short alpha-only value (e.g. "FF").
    // Interpret it as white with the provided alpha channel.
    if (length == 2)
        return 0xFFFFFF00U | value;

    if (length == 6)
        value = (value << 8) | 0xFF;

    return value;
}

bool ThemeData::evaluateIfSubsetCondition(const std::string& ifSubsetExpression) const
{
    const std::string trimmedExpression {Utils::String::trim(ifSubsetExpression)};
    if (trimmedExpression.empty())
        return true;

    auto clauses {Utils::String::delimitedStringToVector(trimmedExpression, ",")};
    for (auto& clauseRaw : clauses) {
        const std::string clause {Utils::String::trim(clauseRaw)};
        if (clause.empty())
            continue;

        size_t sepPos {clause.find(':')};
        if (sepPos == std::string::npos)
            sepPos = clause.find('=');

        if (sepPos == std::string::npos)
            return false;

        const std::string subsetName {
            Utils::String::toLower(Utils::String::trim(clause.substr(0, sepPos)))};
        const std::string optionsExpr {Utils::String::trim(clause.substr(sepPos + 1))};

        const std::string selectedValue {
            Utils::String::toLower(getResolvedThemeSubsetSelection(
                subsetName, mSelectedSystemView, mSubsetSelections))};

        bool clauseMatch {false};
        auto allowedOptions {Utils::String::delimitedStringToVector(optionsExpr, "|")};
        for (auto& optionRaw : allowedOptions) {
            const std::string option {Utils::String::toLower(Utils::String::trim(optionRaw))};
            if (!option.empty() && option == selectedValue) {
                clauseMatch = true;
                break;
            }
        }

        if (!clauseMatch)
            return false;
    }

    return true;
}

std::string ThemeData::getThemeRegionCompat() const
{
    std::string region {Settings::getInstance()->getString("ThemeRegionName")};
    region = Utils::String::toLower(Utils::String::trim(region));
    if (!region.empty())
        return region;

    const std::string locale {
        Utils::String::toLower(Utils::String::trim(
            sThemeLanguage.empty() ? Utils::Localization::sCurrentLocale : sThemeLanguage))};

    if (locale.size() >= 5 && locale[2] == '_')
        return locale.substr(3, 2);

    if (locale.size() >= 2)
        return locale.substr(0, 2);

    return "";
}

ThemeData::ThemeSubsetRole ThemeData::classifyThemeSubset(const ThemeSubset& subset)
{
    const std::string normalizedName {normalizeThemeSubsetName(subset.name)};

    if (normalizedName == "systemview")
        return ThemeSubsetRole::SYSTEM_VIEW;

    if (normalizedName == "colorset" || normalizedName == "colorscheme")
        return ThemeSubsetRole::COLOR_SET;

    if (normalizedName == "botones-menu" ||
        subsetNameContainsAllTokens(normalizedName, {"menu"}, {"systemview", "logo"})) {
        return ThemeSubsetRole::MENU_ICON_SET;
    }

    if (normalizedName == "logos arcade power" ||
        subsetNameContainsAllTokens(normalizedName, {"logo"},
                                    {"menu", "systemview", "colorset", "colorscheme"})) {
        return ThemeSubsetRole::LOGO_SET;
    }

    return ThemeSubsetRole::GENERIC;
}

bool ThemeData::passesBatoceraConditionalAttributes(const pugi::xml_node& node,
                                                    const std::string& currentLanguage) const
{
    if (!node)
        return true;

    const auto includeTranslation {ThemeData::translateBatoceraTag("include")};

    const std::string tinyScreenAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "tinyScreen")};
    const std::string verticalScreenAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "verticalScreen")};
    const std::string ifArchAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "ifArch")};
    const std::string ifNotArchAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "ifNotArch")};
    const std::string ifHelpPromptsAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "ifHelpPrompts")};
    const std::string ifExpression {
        getTranslatedAttributeValue(node, includeTranslation, "if")};
    const std::string ifCheevosAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "ifCheevos")};
    const std::string ifNetplayAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "ifNetplay")};
    const std::string langAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "lang", {"locale"})};
    const std::string regionAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "region")};
    const std::string ifSubsetAttribute {
        getTranslatedAttributeValue(node, includeTranslation, "ifSubset")};

    if (!tinyScreenAttribute.empty()) {
        const float shortestSide {
            std::min(Renderer::getScreenWidth(), Renderer::getScreenHeight())};
        const bool isTinyScreen {shortestSide <= 480.0f};
        const bool requiredTinyScreen {parseBooleanAttribute(tinyScreenAttribute)};
        if (isTinyScreen != requiredTinyScreen)
            return false;
    }

    if (!verticalScreenAttribute.empty()) {
        const bool isVerticalScreen {Renderer::getScreenHeight() > Renderer::getScreenWidth()};
        const bool requiredVerticalScreen {parseBooleanAttribute(verticalScreenAttribute)};
        if (isVerticalScreen != requiredVerticalScreen)
            return false;
    }

    if (!ifArchAttribute.empty()) {
        if (!matchesAnyConditionToken(ifArchAttribute, {getCurrentArchCompat()})) {
            return false;
        }
    }

    if (!ifNotArchAttribute.empty()) {
        if (matchesAnyConditionToken(ifNotArchAttribute, {getCurrentArchCompat()})) {
            return false;
        }
    }

    if (!ifHelpPromptsAttribute.empty()) {
        const bool requiredHelpPrompts {parseBooleanAttribute(ifHelpPromptsAttribute)};
        if (Settings::getInstance()->getBool("ShowHelpPrompts") != requiredHelpPrompts)
            return false;
    }

    if (!ifExpression.empty() && !evaluateIfExpression(ifExpression))
        return false;

    if (!ifCheevosAttribute.empty()) {
        const bool requiredCheevos {parseBooleanAttribute(ifCheevosAttribute)};
        if (Settings::getInstance()->getBool("global.retroachievements") != requiredCheevos)
            return false;
    }

    if (!ifNetplayAttribute.empty()) {
        const bool requiredNetplay {parseBooleanAttribute(ifNetplayAttribute)};
        if (Settings::getInstance()->getBool("global.netplay") != requiredNetplay)
            return false;
    }

    if (!langAttribute.empty()) {
        if (!matchesAnyConditionToken(langAttribute,
                                      buildLanguageCandidates(currentLanguage))) {
            return false;
        }
    }

    if (!regionAttribute.empty()) {
        if (!matchesAnyConditionToken(regionAttribute,
                                      buildRegionCandidates(getThemeRegionCompat(),
                                                            currentLanguage))) {
            return false;
        }
    }

    if (!ifSubsetAttribute.empty()) {
        if (!evaluateIfSubsetCondition(ifSubsetAttribute))
            return false;
    }

    return true;
}

std::string ThemeData::resolvePlaceholders(const std::string& in) const
{
    if (in.empty())
        return in;

    const std::string languageCompat {sThemeLanguage.empty() ? Utils::Localization::sCurrentLocale :
                                                             sThemeLanguage};
    const std::string regionCompat {getThemeRegionCompat()};
    std::string compatInput {resolveLegacyBatoceraLocaleTokens(in, languageCompat, regionCompat)};

    const size_t variableBegin {compatInput.find("${")};
    const size_t variableEnd {compatInput.find("}", variableBegin)};

    if ((variableBegin == std::string::npos) || (variableEnd == std::string::npos))
        return compatInput;

    std::string prefix {compatInput.substr(0, variableBegin)};
    std::string replace {
        compatInput.substr(variableBegin + 2, variableEnd - (variableBegin + 2))};
    replace = translateBatoceraVariable(replace);
    std::string suffix {resolvePlaceholders(compatInput.substr(variableEnd + 1).c_str())};

    std::string value;
    if (replace == "themePath") {
        value = Utils::FileSystem::getParent(mPaths.front());
    }
    else if (replace == "currentPath") {
        value = Utils::FileSystem::getParent(mPaths.back());
    }
    else if (replace == "global.language") {
        value = sThemeLanguage;
    }
    else if (replace == "global.help") {
        value = Settings::getInstance()->getBool("ShowHelpPrompts") ? "true" : "false";
    }
    else if (replace == "global.clock") {
        value = Settings::getInstance()->getBool("DisplayClock") ? "true" : "false";
    }
    else if (replace == "global.cheevos") {
        value = Settings::getInstance()->getBool("global.retroachievements") ? "true" : "false";
    }
    else if (replace == "global.cheevos.username" || replace == "global.cheevosUser") {
        if (Settings::getInstance()->getBool("global.retroachievements"))
            value = Settings::getInstance()->getString("global.retroachievements.username");
        else
            value.clear();
    }
    else if (replace == "global.netplay") {
        value = Settings::getInstance()->getBool("global.netplay") ? "true" : "false";
    }
    else if (replace == "global.netplay.username") {
        if (Settings::getInstance()->getBool("global.netplay")) {
            value = Settings::getInstance()->getString("global.netplay.nickname");
            if (value.empty())
                value = Settings::getInstance()->getString("global.netplay.username");
        }
        else {
            value.clear();
        }
    }
    else if (replace == "global.battery") {
        value = SystemStatus::getInstance().getStatus(false).hasBattery ? "true" : "false";
    }
    else if (replace == "global.batteryLevel") {
        value = std::to_string(SystemStatus::getInstance().getStatus(false).batteryCapacity);
    }
    else if (replace == "global.ip") {
        value.clear();
    }
    else if (replace == "lang") {
        if (sThemeLanguage.size() >= 2)
            value = Utils::String::toLower(sThemeLanguage.substr(0, 2));
    }
    else if (replace == "region") {
        value = getThemeRegionCompat();
    }
    else if (replace == "global.architecture") {
#if defined(__x86_64__) || defined(_M_X64)
        value = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
        value = "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
        value = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
        value = "arm";
#elif defined(__riscv)
        value = "riscv";
#else
        value = "unknown";
#endif
    }
    else if (replace == "screen.width") {
        value = std::to_string(static_cast<int>(Renderer::getScreenWidth()));
    }
    else if (replace == "screen.height") {
        value = std::to_string(static_cast<int>(Renderer::getScreenHeight()));
    }
    else if (replace == "screen.ratio") {
        const float h {Renderer::getScreenHeight()};
        const float ratio {h > 0.0f ? Renderer::getScreenWidth() / h : 0.0f};
        value = std::to_string(ratio);
    }
    else if (replace == "screen.vertical") {
        value = Renderer::getScreenHeight() > Renderer::getScreenWidth() ? "true" : "false";
    }
    else {
        auto varIt = mVariables.find(replace);
        if (varIt != mVariables.end())
            value = varIt->second;
    }

    return prefix + value + suffix;
}

std::string ThemeData::resolveDynamicBindings(const std::string& in) const
{
    if (in.empty())
        return in;

    auto resolveTokenValue = [this](const std::string& rawToken, std::string& outValue) -> bool {
        auto lookupVariable = [this](const std::string& key, std::string& value) -> bool {
            auto it = mVariables.find(key);
            if (it != mVariables.end()) {
                value = it->second;
                return true;
            }

            // Fallback case-insensitive lookup for mixed-case Batocera variable names.
            const std::string lowKey {Utils::String::toLower(key)};
            for (const auto& var : mVariables) {
                if (Utils::String::toLower(var.first) == lowKey) {
                    value = var.second;
                    return true;
                }
            }

            return false;
        };

        auto parseBoolString = [](const std::string& value) -> bool {
            const std::string low {Utils::String::toLower(Utils::String::trim(value))};
            return (low == "1" || low == "true" || low == "yes" || low == "on");
        };

        auto isCollectionContext = [&lookupVariable, &parseBoolString]() {
            std::string value;
            if (lookupVariable("system.collection", value))
                return parseBoolString(value);

            // Compatibility fallback for existing system markers used in this codebase.
            if (lookupVariable("system.name.autoCollections", value) && value != "\b")
                return true;
            if (lookupVariable("system.name.customCollections", value) && value != "\b")
                return true;

            return false;
        };

        const std::string token {Utils::String::trim(rawToken)};
        const size_t firstColon {token.find(':')};
        if (firstColon == std::string::npos) {
            // Dot-notation binding without colon (e.g. {system.theme}, {system.name}).
            // Try a direct variable lookup before giving up.
            if (lookupVariable(token, outValue))
                return true;

            const std::string lowToken {Utils::String::toLower(token)};
            if (lowToken == "lang") {
                outValue = (sThemeLanguage.size() >= 2 ?
                                Utils::String::toLower(sThemeLanguage.substr(0, 2)) :
                                "");
                return true;
            }
            if (lowToken == "language" || lowToken == "locale") {
                outValue = sThemeLanguage;
                return true;
            }
            if (lowToken == "region") {
                outValue = getThemeRegionCompat();
                return true;
            }
            if (lowToken == "country") {
                outValue = getThemeRegionCompat();
                return true;
            }
            if (lowToken == "themepath") {
                outValue = Utils::FileSystem::getParent(mPaths.front());
                return true;
            }
            if (lowToken == "currentpath") {
                outValue = Utils::FileSystem::getParent(mPaths.back());
                return true;
            }

            return false;
        }

        const std::string source {Utils::String::toLower(token.substr(0, firstColon))};
        const std::string property {Utils::String::trim(token.substr(firstColon + 1))};
        const std::string lowProperty {Utils::String::toLower(property)};

        if (source == "global") {
            const std::string key {Utils::String::toLower(property)};

            if (key == "help") {
                outValue = Settings::getInstance()->getBool("ShowHelpPrompts") ? "true" : "false";
                return true;
            }
            if (key == "clock") {
                outValue = Settings::getInstance()->getBool("DisplayClock") ? "true" : "false";
                return true;
            }
            if (key == "architecture") {
#if defined(__x86_64__) || defined(_M_X64)
                outValue = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
                outValue = "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
                outValue = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
                outValue = "arm";
#elif defined(__riscv)
                outValue = "riscv";
#else
                outValue = "unknown";
#endif
                return true;
            }
            if (key == "language") {
                outValue = sThemeLanguage;
                return true;
            }
            if (key == "locale") {
                outValue = sThemeLanguage;
                return true;
            }
            if (key == "region") {
                outValue = getThemeRegionCompat();
                return true;
            }
            if (key == "country") {
                outValue = getThemeRegionCompat();
                return true;
            }
            if (key == "cheevos") {
                outValue = Settings::getInstance()->getBool("global.retroachievements") ?
                               "true" :
                               "false";
                return true;
            }
            if (key == "cheevosuser") {
                if (Settings::getInstance()->getBool("global.retroachievements"))
                    outValue =
                        Settings::getInstance()->getString("global.retroachievements.username");
                else
                    outValue.clear();
                return true;
            }
            if (key == "cheevos.username") {
                if (Settings::getInstance()->getBool("global.retroachievements"))
                    outValue =
                        Settings::getInstance()->getString("global.retroachievements.username");
                else
                    outValue.clear();
                return true;
            }
            if (key == "netplay") {
                outValue = Settings::getInstance()->getBool("global.netplay") ? "true" : "false";
                return true;
            }
            if (key == "netplay.username") {
                if (Settings::getInstance()->getBool("global.netplay")) {
                    outValue = Settings::getInstance()->getString("global.netplay.nickname");
                    if (outValue.empty())
                        outValue = Settings::getInstance()->getString("global.netplay.username");
                }
                else {
                    outValue.clear();
                }
                return true;
            }
            if (key == "clock") {
                outValue = Settings::getInstance()->getBool("DisplayClock") ? "true" : "false";
                return true;
            }
            if (key == "help") {
                outValue = Settings::getInstance()->getBool("ShowHelpPrompts") ? "true" : "false";
                return true;
            }
            if (key == "architecture" || key == "arch") {
#if defined(__x86_64__) || defined(_M_X64)
                outValue = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
                outValue = "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
                outValue = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
                outValue = "arm";
#elif defined(__riscv)
                outValue = "riscv";
#else
                outValue = "unknown";
#endif
                return true;
            }
            if (key == "ip") {
                // Batocera themes can reference {global:ip}, but this fork does not
                // expose a runtime IP address string. Return an empty value instead of
                // leaking the raw token or returning an unrelated boolean.
                outValue.clear();
                return true;
            }
            if (key == "battery" || key == "batterylevel" || key == "battery.level") {
                if (key == "battery") {
                    outValue = SystemStatus::getInstance().getStatus(false).hasBattery ? "true" : "false";
                }
                else {
                    outValue = std::to_string(SystemStatus::getInstance().getStatus(false).batteryCapacity);
                }
                return true;
            }
            if (key == "screenwidth" || key == "screen.width") {
                outValue = std::to_string(static_cast<int>(Renderer::getScreenWidth()));
                return true;
            }
            if (key == "screenheight" || key == "screen.height") {
                outValue = std::to_string(static_cast<int>(Renderer::getScreenHeight()));
                return true;
            }
            if (key == "screenratio" || key == "screen.ratio") {
                const float h {Renderer::getScreenHeight()};
                const float ratio {h > 0.0f ? Renderer::getScreenWidth() / h : 0.0f};
                outValue = std::to_string(ratio);
                return true;
            }
            if (key == "vertical" || key == "screen.vertical") {
                outValue = Renderer::getScreenHeight() > Renderer::getScreenWidth() ? "true" : "false";
                return true;
            }

            // Map unknown global keys against already-resolved static variables when possible.
            if (lookupVariable("global." + property, outValue)) {
                return true;
            }

            return false;
        }

        if (source == "cheevos" && lowProperty == "username") {
            if (Settings::getInstance()->getBool("global.retroachievements"))
                outValue = Settings::getInstance()->getString("global.retroachievements.username");
            else
                outValue.clear();
            return true;
        }

        if (source == "netplay" && lowProperty == "username") {
            if (Settings::getInstance()->getBool("global.netplay")) {
                outValue = Settings::getInstance()->getString("global.netplay.nickname");
                if (outValue.empty())
                    outValue = Settings::getInstance()->getString("global.netplay.username");
            }
            else {
                outValue.clear();
            }
            return true;
        }

        if (source == "screen") {
            if (lowProperty == "width") {
                outValue = std::to_string(static_cast<int>(Renderer::getScreenWidth()));
                return true;
            }
            if (lowProperty == "height") {
                outValue = std::to_string(static_cast<int>(Renderer::getScreenHeight()));
                return true;
            }
            if (lowProperty == "ratio") {
                const float h {Renderer::getScreenHeight()};
                const float ratio {h > 0.0f ? Renderer::getScreenWidth() / h : 0.0f};
                outValue = std::to_string(ratio);
                return true;
            }
            if (lowProperty == "vertical") {
                outValue =
                    Renderer::getScreenHeight() > Renderer::getScreenWidth() ? "true" : "false";
                return true;
            }
            return false;
        }

        if (source == "system") {
            if (Utils::String::startsWith(lowProperty, "ascollection:")) {
                const std::string nestedProp {Utils::String::trim(property.substr(13))};
                if (!isCollectionContext()) {
                    outValue.clear();
                    return true;
                }

                if (lookupVariable("collection." + nestedProp, outValue))
                    return true;
                if (lookupVariable("system." + nestedProp, outValue))
                    return true;

                outValue.clear();
                return true;
            }

            if (Utils::String::startsWith(lowProperty, "random:")) {
                const std::string nestedProp {Utils::String::trim(property.substr(7))};
                if (lookupVariable("system.random." + nestedProp, outValue))
                    return true;
                if (lookupVariable("systemrandom." + nestedProp, outValue))
                    return true;
                outValue.clear();
                return true;
            }

            const std::string varName {"system." + property};
            if (lookupVariable(varName, outValue)) {
                return true;
            }

            if (property == "name") {
                if (lookupVariable("system.name", outValue)) {
                    return true;
                }
            }

            return false;
        }

        if (source == "collection") {
            if (!isCollectionContext()) {
                outValue.clear();
                return true;
            }

            if (lookupVariable("collection." + property, outValue))
                return true;
            if (lookupVariable("system." + property, outValue))
                return true;

            outValue.clear();
            return true;
        }

        if (source == "game") {
            if (Utils::String::startsWith(lowProperty, "system:")) {
                const std::string nestedProp {Utils::String::trim(property.substr(7))};
                if (lookupVariable("game.system." + nestedProp, outValue))
                    return true;
                if (lookupVariable("system." + nestedProp, outValue))
                    return true;
                outValue.clear();
                return true;
            }

            if (lookupVariable("game." + property, outValue))
                return true;

            // Keep Batocera-style null-like behavior for unavailable game context.
            outValue.clear();
            return true;
        }

        if (source == "systemrandom") {
            if (lookupVariable("system.random." + property, outValue))
                return true;
            if (lookupVariable("systemrandom." + property, outValue))
                return true;
            outValue.clear();
            return true;
        }

        if (source == "subset") {
            const std::string subsetName {Utils::String::trim(property)};
            if (Utils::String::toLower(subsetName) == "systemview") {
                outValue = mSelectedSystemView;
                return true;
            }

            auto subsetIt = mSubsetSelections.find(subsetName);
            if (subsetIt != mSubsetSelections.end()) {
                outValue = subsetIt->second;
                return true;
            }

            return false;
        }

        return false;
    };

    std::string out;
    out.reserve(in.size());

    size_t pos {0};
    while (pos < in.size()) {
        const size_t open {in.find('{', pos)};
        if (open == std::string::npos) {
            out.append(in.substr(pos));
            break;
        }

        // Preserve static placeholder syntax (${...}).
        if (open > 0 && in[open - 1] == '$') {
            out.append(in.substr(pos, open - pos + 1));
            pos = open + 1;
            continue;
        }

        out.append(in.substr(pos, open - pos));

        const size_t close {in.find('}', open)};
        if (close == std::string::npos) {
            out.append(in.substr(open));
            break;
        }

        const std::string token {in.substr(open + 1, close - open - 1)};
        std::string tokenValue;
        if (resolveTokenValue(token, tokenValue))
            out.append(tokenValue);
        else
            out.append(in.substr(open, close - open + 1));

        pos = close + 1;
    }

    return out;
}

bool ThemeData::evaluateIfExpression(const std::string& expression) const
{
    std::function<bool(const std::string&, bool&, double&, std::string&)> resolveVariable;
    resolveVariable = [this, &resolveVariable](const std::string& token,
                                               bool& isNumeric,
                                               double& numericValue,
                                               std::string& stringValue) -> bool {
        std::string t {Utils::String::trim(token)};

        // Remove optional quotes.
        if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') ||
                              (t.front() == '\'' && t.back() == '\''))) {
            t = t.substr(1, t.size() - 2);
        }

        // Handle ${...} variables.
        if (t.size() >= 4 && t.front() == '$' && t[1] == '{' && t.back() == '}') {
            const std::string varName {t.substr(2, t.size() - 3)};

            if (varName == "screen.width") {
                isNumeric = true;
                numericValue = static_cast<double>(Renderer::getScreenWidth());
                return true;
            }
            if (varName == "screen.height") {
                isNumeric = true;
                numericValue = static_cast<double>(Renderer::getScreenHeight());
                return true;
            }
            if (varName == "screen.ratio") {
                const double h {static_cast<double>(Renderer::getScreenHeight())};
                isNumeric = true;
                numericValue = (h > 0.0 ? static_cast<double>(Renderer::getScreenWidth()) / h :
                                        0.0);
                return true;
            }
            if (varName == "tinyScreen") {
                const float shortestSide {
                    std::min(Renderer::getScreenWidth(), Renderer::getScreenHeight())};
                isNumeric = true;
                numericValue = (shortestSide <= 480.0f ? 1.0 : 0.0);
                return true;
            }
            if (varName == "arch" || varName == "platform.arch" ||
                varName == "global.architecture") {
                std::string currentArch;
#if defined(__x86_64__) || defined(_M_X64)
                currentArch = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
                currentArch = "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
                currentArch = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
                currentArch = "arm";
#elif defined(__riscv)
                currentArch = "riscv";
#else
                currentArch = "unknown";
#endif
                isNumeric = false;
                stringValue = currentArch;
                return true;
            }
            if (varName == "language" || varName == "theme.language" ||
                varName == "global.language") {
                isNumeric = false;
                stringValue = !sThemeLanguage.empty() ? sThemeLanguage :
                                                        Utils::Localization::sCurrentLocale;
                return true;
            }
            if (varName == "locale" || varName == "theme.locale" ||
                varName == "global.locale") {
                isNumeric = false;
                stringValue = !sThemeLanguage.empty() ? sThemeLanguage :
                                                        Utils::Localization::sCurrentLocale;
                return true;
            }
            if (varName == "global.help") {
                isNumeric = true;
                numericValue = Settings::getInstance()->getBool("ShowHelpPrompts") ? 1.0 : 0.0;
                return true;
            }
            if (varName == "global.clock") {
                isNumeric = true;
                numericValue = Settings::getInstance()->getBool("DisplayClock") ? 1.0 : 0.0;
                return true;
            }
            if (varName == "lang") {
                isNumeric = false;
                const std::string languageCompat {!sThemeLanguage.empty() ? sThemeLanguage :
                                                                          Utils::Localization::sCurrentLocale};
                stringValue = (languageCompat.size() >= 2 ?
                                   Utils::String::toLower(languageCompat.substr(0, 2)) :
                                   "");
                return true;
            }
            if (varName == "region" || varName == "country" || varName == "global.region" ||
                varName == "global.country") {
                isNumeric = false;
                stringValue = getThemeRegionCompat();
                return true;
            }
            if (varName == "themePath") {
                isNumeric = false;
                stringValue = Utils::FileSystem::getParent(mPaths.front());
                return true;
            }
            if (varName == "currentPath") {
                isNumeric = false;
                stringValue = Utils::FileSystem::getParent(mPaths.back());
                return true;
            }
            if (Utils::String::startsWith(varName, "subset.")) {
                const std::string subsetName {varName.substr(7)};
                isNumeric = false;
                stringValue = getResolvedThemeSubsetSelection(
                    subsetName, mSelectedSystemView, mSubsetSelections);
                return true;
            }

            // Generic fallback: expose already-resolved theme/system variables to the
            // if-expression parser, e.g. ${system.name}, ${system.fullName}, etc.
            auto varIt = mVariables.find(varName);
            if (varIt != mVariables.end()) {
                const std::string resolvedValue {Utils::String::trim(varIt->second)};
                const std::string lowResolved {Utils::String::toLower(resolvedValue)};

                if (lowResolved == "true" || lowResolved == "yes" || lowResolved == "on") {
                    isNumeric = true;
                    numericValue = 1.0;
                    return true;
                }
                if (lowResolved == "false" || lowResolved == "no" || lowResolved == "off") {
                    isNumeric = true;
                    numericValue = 0.0;
                    return true;
                }

                try {
                    size_t parsedLen {0};
                    const double numeric {std::stod(resolvedValue, &parsedLen)};
                    if (Utils::String::trim(resolvedValue.substr(parsedLen)).empty()) {
                        isNumeric = true;
                        numericValue = numeric;
                        return true;
                    }
                }
                catch (...) {
                }

                isNumeric = false;
                stringValue = resolvedValue;
                return true;
            }

            return false;
        }

        // Handle Batocera-style {type:property} dynamic bindings (no dollar sign).
        // Resolves the token via resolveDynamicBindings so expressions like
        // {system.theme} == 'snes' work inside if attributes.
        if (t.size() >= 3 && t.front() == '{' && t.back() == '}') {
            const std::string resolved {resolveDynamicBindings(t)};
            if (resolved == t) {
                // Token could not be resolved; use empty string (null-like safe default).
                isNumeric = false;
                stringValue.clear();
                return true;
            }
            const std::string resolvedLow {
                Utils::String::toLower(Utils::String::trim(resolved))};
            if (resolvedLow == "true" || resolvedLow == "yes" || resolvedLow == "on") {
                isNumeric = true;
                numericValue = 1.0;
                return true;
            }
            if (resolvedLow == "false" || resolvedLow == "no" || resolvedLow == "off") {
                isNumeric = true;
                numericValue = 0.0;
                return true;
            }

            const std::string resolvedTrimmed {Utils::String::trim(resolved)};
            try {
                size_t parsedLen {0};
                const double numeric {std::stod(resolvedTrimmed, &parsedLen)};
                if (Utils::String::trim(resolvedTrimmed.substr(parsedLen)).empty()) {
                    isNumeric = true;
                    numericValue = numeric;
                    return true;
                }
            }
            catch (...) {
            }

            isNumeric = false;
            stringValue = resolved;
            return true;
        }

        auto stripQuotes = [](const std::string& inValue) {
            std::string value {Utils::String::trim(inValue)};
            if (value.size() >= 2 &&
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }
            return value;
        };

        auto splitTopLevelCsv = [](const std::string& input) {
            std::vector<std::string> tokens;
            int parenDepth {0};
            bool inSingleQuote {false};
            bool inDoubleQuote {false};
            size_t start {0};

            for (size_t i {0}; i < input.size(); ++i) {
                const char c {input[i]};

                if (c == '\'' && !inDoubleQuote)
                    inSingleQuote = !inSingleQuote;
                else if (c == '"' && !inSingleQuote)
                    inDoubleQuote = !inDoubleQuote;

                if (inSingleQuote || inDoubleQuote)
                    continue;

                if (c == '(')
                    ++parenDepth;
                else if (c == ')' && parenDepth > 0)
                    --parenDepth;

                if (parenDepth == 0 && c == ',') {
                    tokens.emplace_back(Utils::String::trim(input.substr(start, i - start)));
                    start = i + 1;
                }
            }

            tokens.emplace_back(Utils::String::trim(input.substr(start)));
            return tokens;
        };

        auto parseCall = [&splitTopLevelCsv](const std::string& expr,
                                             std::string& method,
                                             std::vector<std::string>& args) {
            if (expr.empty() || expr.back() != ')')
                return false;

            const size_t openParenPos {expr.find('(')};
            if (openParenPos == std::string::npos || openParenPos == 0)
                return false;

            const std::string methodName {
                Utils::String::toLower(Utils::String::trim(expr.substr(0, openParenPos)))};
            if (methodName.empty())
                return false;

            const std::string inner {
                expr.substr(openParenPos + 1, expr.size() - openParenPos - 2)};
            method = methodName;
            args = splitTopLevelCsv(inner);
            return true;
        };

        auto parseObjectCall = [&splitTopLevelCsv](const std::string& expr,
                                                   std::string& method,
                                                   std::vector<std::string>& args) {
            if (expr.empty() || expr.back() != ')')
                return false;

            int parenDepth {0};
            bool inSingleQuote {false};
            bool inDoubleQuote {false};
            size_t openParenPos {std::string::npos};

            for (size_t i {0}; i < expr.size(); ++i) {
                const char c {expr[i]};
                if (c == '\'' && !inDoubleQuote)
                    inSingleQuote = !inSingleQuote;
                else if (c == '"' && !inSingleQuote)
                    inDoubleQuote = !inDoubleQuote;

                if (inSingleQuote || inDoubleQuote)
                    continue;

                if (c == '(') {
                    if (parenDepth == 0)
                        openParenPos = i;
                    ++parenDepth;
                }
                else if (c == ')' && parenDepth > 0) {
                    --parenDepth;
                    if (parenDepth == 0 && i != expr.size() - 1)
                        return false;
                }
            }

            if (openParenPos == std::string::npos || parenDepth != 0)
                return false;

            size_t dotPos {std::string::npos};
            parenDepth = 0;
            inSingleQuote = false;
            inDoubleQuote = false;

            for (size_t i {0}; i < openParenPos; ++i) {
                const char c {expr[i]};
                if (c == '\'' && !inDoubleQuote)
                    inSingleQuote = !inSingleQuote;
                else if (c == '"' && !inSingleQuote)
                    inDoubleQuote = !inDoubleQuote;

                if (inSingleQuote || inDoubleQuote)
                    continue;

                if (c == '(')
                    ++parenDepth;
                else if (c == ')' && parenDepth > 0)
                    --parenDepth;

                if (parenDepth == 0 && c == '.')
                    dotPos = i;
            }

            if (dotPos == std::string::npos)
                return false;

            const std::string subject {Utils::String::trim(expr.substr(0, dotPos))};
            method = Utils::String::toLower(
                Utils::String::trim(expr.substr(dotPos + 1, openParenPos - dotPos - 1)));

            if (subject.empty() || method.empty())
                return false;

            const std::string inner {
                expr.substr(openParenPos + 1, expr.size() - openParenPos - 2)};
            std::vector<std::string> callArgs {splitTopLevelCsv(inner)};

            args.clear();
            args.emplace_back(subject);
            for (const auto& arg : callArgs) {
                if (!arg.empty())
                    args.emplace_back(arg);
            }

            return true;
        };

        auto toLower = [](const std::string& v) {
            return Utils::String::toLower(Utils::String::trim(v));
        };

        auto parseBoolLike = [&toLower](const std::string& v, bool& outBool) {
            const std::string low {toLower(v)};
            if (low == "true" || low == "yes" || low == "on" || low == "1") {
                outBool = true;
                return true;
            }
            if (low == "false" || low == "no" || low == "off" || low == "0") {
                outBool = false;
                return true;
            }
            return false;
        };

        auto parseNumericLiteralCompat = [](const std::string& value,
                                            double& outNumber) -> bool {
            const std::string trimmed {Utils::String::trim(value)};
            if (trimmed.empty())
                return false;

            try {
                size_t parsedLen {0};
                const double numeric {std::stod(trimmed, &parsedLen)};
                if (Utils::String::trim(trimmed.substr(parsedLen)).empty()) {
                    outNumber = numeric;
                    return true;
                }
            }
            catch (...) {
            }

            const size_t slashPos {trimmed.find('/')};
            if (slashPos == std::string::npos || slashPos == 0 || slashPos == trimmed.size() - 1)
                return false;
            if (trimmed.find('/', slashPos + 1) != std::string::npos)
                return false;

            try {
                size_t numParsedLen {0};
                size_t denParsedLen {0};
                const std::string numeratorText {
                    Utils::String::trim(trimmed.substr(0, slashPos))};
                const std::string denominatorText {
                    Utils::String::trim(trimmed.substr(slashPos + 1))};
                const double numerator {std::stod(numeratorText, &numParsedLen)};
                const double denominator {std::stod(denominatorText, &denParsedLen)};

                if (!Utils::String::trim(numeratorText.substr(numParsedLen)).empty() ||
                    !Utils::String::trim(denominatorText.substr(denParsedLen)).empty() ||
                    denominator == 0.0) {
                    return false;
                }

                outNumber = numerator / denominator;
                return true;
            }
            catch (...) {
            }

            return false;
        };

        std::string method;
        std::vector<std::string> args;
        if (parseCall(t, method, args) || parseObjectCall(t, method, args)) {
            std::vector<std::string> evaluatedArgs;
            evaluatedArgs.reserve(args.size());
            for (const auto& arg : args) {
                bool argNumeric {false};
                double argNumber {0.0};
                std::string argString;
                if (resolveVariable(arg, argNumeric, argNumber, argString)) {
                    if (argNumeric)
                        evaluatedArgs.emplace_back(std::to_string(argNumber));
                    else
                        evaluatedArgs.emplace_back(argString);
                }
                else {
                    evaluatedArgs.emplace_back(stripQuotes(arg));
                }
            }

            auto returnBool = [&isNumeric, &numericValue](const bool v) {
                isNumeric = true;
                numericValue = v ? 1.0 : 0.0;
                return true;
            };

            auto returnNumber = [&isNumeric, &numericValue](const double v) {
                isNumeric = true;
                numericValue = v;
                return true;
            };

            auto returnString = [&isNumeric, &stringValue](const std::string& v) {
                isNumeric = false;
                stringValue = v;
                return true;
            };

            auto formatElapsedFromSeconds = [](time_t totalSeconds) {
                if (totalSeconds < 0)
                    totalSeconds = 0;

                Utils::Time::Duration dur {totalSeconds};
                if (dur.getDays() > 0) {
                    return Utils::String::format(
                        _np("theme", "%i day ago", "%i days ago", dur.getDays()),
                        dur.getDays());
                }
                if (dur.getHours() > 0) {
                    return Utils::String::format(
                        _np("theme", "%i hour ago", "%i hours ago", dur.getHours()),
                        dur.getHours());
                }
                if (dur.getMinutes() > 0) {
                    return Utils::String::format(
                        _np("theme", "%i minute ago", "%i minutes ago", dur.getMinutes()),
                        dur.getMinutes());
                }

                return Utils::String::format(
                    _np("theme", "%i second ago", "%i seconds ago", dur.getSeconds()),
                    dur.getSeconds());
            };

            if (method == "exists") {
                if (evaluatedArgs.empty())
                    return returnBool(false);
                return returnBool(Utils::FileSystem::exists(evaluatedArgs.front()));
            }
            if (method == "isdirectory") {
                if (evaluatedArgs.empty())
                    return returnBool(false);
                return returnBool(Utils::FileSystem::isDirectory(evaluatedArgs.front()));
            }
            if (method == "empty") {
                if (evaluatedArgs.empty())
                    return returnBool(true);
                return returnBool(Utils::String::trim(evaluatedArgs.front()).empty());
            }
            if (method == "contains" && evaluatedArgs.size() >= 2)
                return returnBool(evaluatedArgs[0].find(evaluatedArgs[1]) != std::string::npos);
            if (method == "startswith" && evaluatedArgs.size() >= 2)
                return returnBool(Utils::String::startsWith(evaluatedArgs[0], evaluatedArgs[1]));
            if (method == "endswith" && evaluatedArgs.size() >= 2)
                return returnBool(Utils::String::endsWith(evaluatedArgs[0], evaluatedArgs[1]));

            if (method == "lower" && !evaluatedArgs.empty())
                return returnString(Utils::String::toLower(evaluatedArgs[0]));
            if (method == "upper" && !evaluatedArgs.empty())
                return returnString(Utils::String::toUpper(evaluatedArgs[0]));
            if (method == "trim" && !evaluatedArgs.empty())
                return returnString(Utils::String::trim(evaluatedArgs[0]));
            if (method == "proper" && !evaluatedArgs.empty()) {
                std::string val {Utils::String::toLower(evaluatedArgs[0])};
                if (!val.empty())
                    val[0] = static_cast<char>(std::toupper(val[0]));
                return returnString(val);
            }

            if (method == "tointeger" && !evaluatedArgs.empty()) {
                double numeric {0.0};
                if (parseNumericLiteralCompat(evaluatedArgs[0], numeric))
                    return returnNumber(static_cast<int>(numeric));
                bool boolValue {false};
                if (parseBoolLike(evaluatedArgs[0], boolValue))
                    return returnNumber(boolValue ? 1.0 : 0.0);
                return returnNumber(0.0);
            }
            if (method == "toboolean" && !evaluatedArgs.empty()) {
                bool boolValue {false};
                if (parseBoolLike(evaluatedArgs[0], boolValue))
                    return returnBool(boolValue);
                double numeric {0.0};
                if (parseNumericLiteralCompat(evaluatedArgs[0], numeric))
                    return returnBool(numeric != 0.0);
                return returnBool(!Utils::String::trim(evaluatedArgs[0]).empty());
            }
            if (method == "tostring" && !evaluatedArgs.empty())
                return returnString(evaluatedArgs[0]);
            if (method == "translate" && !evaluatedArgs.empty())
                return returnString(_p("theme", evaluatedArgs[0].c_str()));

            if (method == "date" && !evaluatedArgs.empty()) {
                const std::string value {Utils::String::trim(evaluatedArgs[0])};
                size_t splitPos {value.find('T')};
                if (splitPos == std::string::npos)
                    splitPos = value.find(' ');
                if (splitPos == std::string::npos)
                    return returnString(value);
                return returnString(value.substr(0, splitPos));
            }

            if (method == "time" && !evaluatedArgs.empty()) {
                const std::string value {Utils::String::trim(evaluatedArgs[0])};
                size_t splitPos {value.find('T')};
                if (splitPos == std::string::npos)
                    splitPos = value.find(' ');
                if (splitPos == std::string::npos || splitPos + 1 >= value.size())
                    return returnString("");
                return returnString(value.substr(splitPos + 1));
            }

            auto extractDatePart = [](const std::string& value,
                                      const std::string& part) -> std::string {
                std::string dateValue {Utils::String::trim(value)};
                size_t splitPos {dateValue.find('T')};
                if (splitPos == std::string::npos)
                    splitPos = dateValue.find(' ');
                if (splitPos != std::string::npos)
                    dateValue = dateValue.substr(0, splitPos);

                std::string digits;
                digits.reserve(dateValue.size());
                for (const char c : dateValue) {
                    if (std::isdigit(static_cast<unsigned char>(c)))
                        digits.push_back(c);
                }

                if (digits.size() < 8)
                    return "";

                if (part == "year")
                    return digits.substr(0, 4);
                if (part == "month")
                    return digits.substr(4, 2);
                if (part == "day")
                    return digits.substr(6, 2);
                return "";
            };

            if ((method == "year" || method == "month" || method == "day") &&
                !evaluatedArgs.empty()) {
                return returnString(extractDatePart(evaluatedArgs[0], method));
            }

            if (method == "elapsed" && !evaluatedArgs.empty()) {
                const std::string value {Utils::String::trim(evaluatedArgs[0])};
                if (value.empty())
                    return returnString("");

                const time_t parsedTime {Utils::Time::stringToTime(value)};
                if (parsedTime <= 0)
                    return returnString("");

                const time_t nowTime {Utils::Time::now()};
                const time_t diff {nowTime > parsedTime ? nowTime - parsedTime : 0};
                return returnString(formatElapsedFromSeconds(diff));
            }

            if (method == "expandseconds" && !evaluatedArgs.empty()) {
                try {
                    const time_t sec {static_cast<time_t>(std::stoll(Utils::String::trim(
                        evaluatedArgs[0])))};
                    return returnString(formatElapsedFromSeconds(sec));
                }
                catch (...) {
                    return returnString("");
                }
            }

            if (method == "formatseconds" && !evaluatedArgs.empty()) {
                try {
                    long long sec {std::stoll(Utils::String::trim(evaluatedArgs[0]))};
                    if (sec < 0)
                        sec = 0;

                    const long long hours {sec / 3600};
                    const long long minutes {(sec % 3600) / 60};
                    const long long seconds {sec % 60};

                    if (hours > 0) {
                        return returnString(Utils::String::format("%02lld:%02lld:%02lld",
                                                                 hours,
                                                                 minutes,
                                                                 seconds));
                    }
                    return returnString(
                        Utils::String::format("%02lld:%02lld", minutes, seconds));
                }
                catch (...) {
                    return returnString("");
                }
            }

            if ((method == "min" || method == "max") && evaluatedArgs.size() >= 2) {
                double a {0.0};
                double b {0.0};
                if (parseNumericLiteralCompat(evaluatedArgs[0], a) &&
                    parseNumericLiteralCompat(evaluatedArgs[1], b)) {
                    return returnNumber(method == "min" ? std::min(a, b) : std::max(a, b));
                }
            }
            if (method == "clamp" && evaluatedArgs.size() >= 3) {
                double v {0.0};
                double lo {0.0};
                double hi {0.0};
                if (parseNumericLiteralCompat(evaluatedArgs[0], v) &&
                    parseNumericLiteralCompat(evaluatedArgs[1], lo) &&
                    parseNumericLiteralCompat(evaluatedArgs[2], hi)) {
                    return returnNumber(std::max(lo, std::min(v, hi)));
                }
            }

            if (method == "getextension" && !evaluatedArgs.empty())
                return returnString(Utils::FileSystem::getExtension(evaluatedArgs[0]));
            if (method == "filename" && !evaluatedArgs.empty())
                return returnString(Utils::FileSystem::getFileName(evaluatedArgs[0]));
            if (method == "stem" && !evaluatedArgs.empty())
                return returnString(Utils::FileSystem::getStem(evaluatedArgs[0]));
            if (method == "directory" && !evaluatedArgs.empty())
                return returnString(Utils::FileSystem::getParent(evaluatedArgs[0]));
            if (method == "filesize" && !evaluatedArgs.empty()) {
                const long size {Utils::FileSystem::getFileSize(evaluatedArgs[0])};
                return returnNumber(size >= 0 ? static_cast<double>(size) : 0.0);
            }
            if (method == "filesizekb" && !evaluatedArgs.empty()) {
                const long size {Utils::FileSystem::getFileSize(evaluatedArgs[0])};
                const double sizeKb {(size >= 0 ? static_cast<double>(size) : 0.0) / 1024.0};
                return returnString(Utils::String::format("%.1f KB", sizeKb));
            }
            if (method == "filesizemb" && !evaluatedArgs.empty()) {
                const long size {Utils::FileSystem::getFileSize(evaluatedArgs[0])};
                const double sizeMb {
                    (size >= 0 ? static_cast<double>(size) : 0.0) / (1024.0 * 1024.0)};
                return returnString(Utils::String::format("%.1f MB", sizeMb));
            }
            if (method == "firstfile" && !evaluatedArgs.empty()) {
                for (const auto& path : evaluatedArgs) {
                    if (Utils::FileSystem::exists(path))
                        return returnString(path);
                }
                return returnString("");
            }
            if (method == "default" && !evaluatedArgs.empty()) {
                const std::string value {Utils::String::trim(evaluatedArgs[0])};
                if (value.empty())
                    return returnString("Unknown");
                bool boolValue {false};
                if (parseBoolLike(value, boolValue) && !boolValue)
                    return returnString("None");
                double numeric {0.0};
                if (parseNumericLiteralCompat(value, numeric) && numeric == 0.0)
                    return returnString("None");
                return returnString(value);
            }
        }

        // Parse as boolean literals.
        const std::string low {Utils::String::toLower(t)};
        if (low == "true" || low == "yes" || low == "on") {
            isNumeric = true;
            numericValue = 1.0;
            return true;
        }
        if (low == "false" || low == "no" || low == "off") {
            isNumeric = true;
            numericValue = 0.0;
            return true;
        }

        // Parse as numeric literal.
        double numeric {0.0};
        if (parseNumericLiteralCompat(t, numeric)) {
            isNumeric = true;
            numericValue = numeric;
            return true;
        }

        // Fallback to string literal.
        isNumeric = false;
        stringValue = t;
        return true;
    };

    auto evaluateExpression = [&resolveVariable](const std::string& expression) -> bool {
        auto findTopLevelOperator = [](const std::string& expr,
                                       const std::string& op) -> size_t {
            int parenDepth {0};
            bool inSingleQuote {false};
            bool inDoubleQuote {false};

            for (size_t i {0}; i < expr.size(); ++i) {
                const char c {expr[i]};

                if (c == '\'' && !inDoubleQuote)
                    inSingleQuote = !inSingleQuote;
                else if (c == '"' && !inSingleQuote)
                    inDoubleQuote = !inDoubleQuote;

                if (inSingleQuote || inDoubleQuote)
                    continue;

                if (c == '(')
                    ++parenDepth;
                else if (c == ')' && parenDepth > 0)
                    --parenDepth;

                if (parenDepth == 0 && i + op.size() <= expr.size() &&
                    expr.compare(i, op.size(), op) == 0) {
                    return i;
                }
            }

            return std::string::npos;
        };

        auto splitTopLevelCsv = [](const std::string& input) -> std::vector<std::string> {
            std::vector<std::string> tokens;
            int parenDepth {0};
            bool inSingleQuote {false};
            bool inDoubleQuote {false};
            size_t start {0};

            for (size_t i {0}; i < input.size(); ++i) {
                const char c {input[i]};

                if (c == '\'' && !inDoubleQuote)
                    inSingleQuote = !inSingleQuote;
                else if (c == '"' && !inSingleQuote)
                    inDoubleQuote = !inDoubleQuote;

                if (inSingleQuote || inDoubleQuote)
                    continue;

                if (c == '(')
                    ++parenDepth;
                else if (c == ')' && parenDepth > 0)
                    --parenDepth;

                if (parenDepth == 0 && c == ',') {
                    tokens.emplace_back(Utils::String::trim(input.substr(start, i - start)));
                    start = i + 1;
                }
            }

            tokens.emplace_back(Utils::String::trim(input.substr(start)));
            return tokens;
        };

        struct ComparableValue {
            bool numeric {false};
            double number {0.0};
            std::string text;
        };

        auto parseComparableValue = [&resolveVariable](const std::string& token,
                                                       ComparableValue& out) -> bool {
            bool isNumeric {false};
            double numericValue {0.0};
            std::string stringValue;

            if (!resolveVariable(token, isNumeric, numericValue, stringValue))
                return false;

            out.numeric = isNumeric;
            out.number = numericValue;
            out.text = stringValue;
            return true;
        };

        auto valuesEqual = [](const ComparableValue& lhs, const ComparableValue& rhs) -> bool {
            if (lhs.numeric && rhs.numeric)
                return lhs.number == rhs.number;

            const std::string left {lhs.numeric ? std::to_string(lhs.number) : lhs.text};
            const std::string right {rhs.numeric ? std::to_string(rhs.number) : rhs.text};
            return left == right;
        };

        std::function<bool(const std::string&)> eval;
        eval = [&eval,
                &resolveVariable,
                &findTopLevelOperator,
                &splitTopLevelCsv,
                &parseComparableValue,
                &valuesEqual](const std::string& raw) -> bool {
            std::string expr {Utils::String::trim(raw)};
            if (expr.empty())
                return false;

            // Remove enclosing parentheses if they wrap the whole expression.
            while (expr.size() >= 2 && expr.front() == '(' && expr.back() == ')') {
                int depth {0};
                bool wrapsAll {true};
                for (size_t i {0}; i < expr.size(); ++i) {
                    if (expr[i] == '(')
                        ++depth;
                    else if (expr[i] == ')')
                        --depth;

                    if (depth == 0 && i != expr.size() - 1) {
                        wrapsAll = false;
                        break;
                    }
                }
                if (!wrapsAll)
                    break;
                expr = Utils::String::trim(expr.substr(1, expr.size() - 2));
            }

            if (!expr.empty() && expr.front() == '!')
                return !eval(expr.substr(1));

            // OR has lower precedence than AND.
            const size_t orPos {findTopLevelOperator(expr, "||")};
            if (orPos != std::string::npos) {
                return eval(expr.substr(0, orPos)) || eval(expr.substr(orPos + 2));
            }

            const size_t andPos {findTopLevelOperator(expr, "&&")};
            if (andPos != std::string::npos) {
                return eval(expr.substr(0, andPos)) && eval(expr.substr(andPos + 2));
            }

            // Membership operator support:
            // - in(value, a, b, c)
            // - not in(value, a, b, c)
            // - value in (a, b, c)
            // - value not in (a, b, c)
            const std::string exprLower {Utils::String::toLower(expr)};

            auto evaluateMembership = [&parseComparableValue,
                                       &splitTopLevelCsv,
                                       &valuesEqual](const std::string& leftToken,
                                                     const std::string& rightListExpr,
                                                     const bool negate) -> bool {
                ComparableValue lhs;
                if (!parseComparableValue(leftToken, lhs))
                    return false;

                std::string listExpr {Utils::String::trim(rightListExpr)};
                if (listExpr.size() >= 2 && listExpr.front() == '(' && listExpr.back() == ')')
                    listExpr = Utils::String::trim(listExpr.substr(1, listExpr.size() - 2));

                auto candidates {splitTopLevelCsv(listExpr)};
                bool found {false};

                for (const auto& candidateToken : candidates) {
                    if (candidateToken.empty())
                        continue;

                    ComparableValue rhs;
                    if (!parseComparableValue(candidateToken, rhs))
                        continue;

                    if (valuesEqual(lhs, rhs)) {
                        found = true;
                        break;
                    }
                }

                return (negate ? !found : found);
            };

            if (Utils::String::startsWith(exprLower, "in(") && expr.back() == ')') {
                const std::string inner {expr.substr(3, expr.size() - 4)};
                auto args {splitTopLevelCsv(inner)};
                if (args.size() >= 2) {
                    std::string rhsList;
                    for (size_t i {1}; i < args.size(); ++i) {
                        if (!rhsList.empty())
                            rhsList += ",";
                        rhsList += args[i];
                    }
                    return evaluateMembership(args[0], rhsList, false);
                }
            }

            if (Utils::String::startsWith(exprLower, "not in(") && expr.back() == ')') {
                const std::string inner {expr.substr(7, expr.size() - 8)};
                auto args {splitTopLevelCsv(inner)};
                if (args.size() >= 2) {
                    std::string rhsList;
                    for (size_t i {1}; i < args.size(); ++i) {
                        if (!rhsList.empty())
                            rhsList += ",";
                        rhsList += args[i];
                    }
                    return evaluateMembership(args[0], rhsList, true);
                }
            }

            const size_t notInPos {
                findTopLevelOperator(exprLower, " not in ")};
            if (notInPos != std::string::npos) {
                const std::string leftExpr {Utils::String::trim(expr.substr(0, notInPos))};
                const std::string rightExpr {
                    Utils::String::trim(expr.substr(notInPos + std::string(" not in ").size()))};
                return evaluateMembership(leftExpr, rightExpr, true);
            }

            const size_t inPos {findTopLevelOperator(exprLower, " in ")};
            if (inPos != std::string::npos) {
                const std::string leftExpr {Utils::String::trim(expr.substr(0, inPos))};
                const std::string rightExpr {
                    Utils::String::trim(expr.substr(inPos + std::string(" in ").size()))};
                return evaluateMembership(leftExpr, rightExpr, false);
            }

            // Case-insensitive comparison helper.
            auto isCaseInsensitiveEqual = [](const std::string& lhs,
                                             const std::string& rhs) -> bool {
                const std::string lhsLower {Utils::String::toLower(lhs)};
                const std::string rhsLower {Utils::String::toLower(rhs)};
                return lhsLower == rhsLower;
            };

            // Operators ordered by length (longest first) to avoid substring matching issues.
            const std::vector<std::string> operators {
                {"!~=", "==i", "!=i", "!=", "~=", ">=", "<=", "==", ">", "<"}};
            for (const auto& op : operators) {
                const size_t opPos {findTopLevelOperator(expr, op)};
                if (opPos == std::string::npos)
                    continue;

                const std::string leftExpr {Utils::String::trim(expr.substr(0, opPos))};
                const std::string rightExpr {
                    Utils::String::trim(expr.substr(opPos + op.size()))};

                bool leftNumeric {false};
                bool rightNumeric {false};
                double leftNumber {0.0};
                double rightNumber {0.0};
                std::string leftString;
                std::string rightString;

                if (!resolveVariable(leftExpr, leftNumeric, leftNumber, leftString) ||
                    !resolveVariable(rightExpr, rightNumeric, rightNumber, rightString)) {
                    return false;
                }

                // Case-insensitive comparison operators (~=, !=i, ==i).
                if (op == "~=" || op == "==i") {
                    const std::string lhs {leftNumeric ? std::to_string(leftNumber) : leftString};
                    const std::string rhs {rightNumeric ? std::to_string(rightNumber) : rightString};
                    return isCaseInsensitiveEqual(lhs, rhs);
                }

                if (op == "!~=" || op == "!=i") {
                    const std::string lhs {leftNumeric ? std::to_string(leftNumber) : leftString};
                    const std::string rhs {rightNumeric ? std::to_string(rightNumber) : rightString};
                    return !isCaseInsensitiveEqual(lhs, rhs);
                }

                if (leftNumeric && rightNumeric) {
                    if (op == "==")
                        return leftNumber == rightNumber;
                    if (op == "!=")
                        return leftNumber != rightNumber;
                    if (op == ">=")
                        return leftNumber >= rightNumber;
                    if (op == "<=")
                        return leftNumber <= rightNumber;
                    if (op == ">")
                        return leftNumber > rightNumber;
                    if (op == "<")
                        return leftNumber < rightNumber;
                }
                else {
                    const std::string lhs {leftNumeric ? std::to_string(leftNumber) : leftString};
                    const std::string rhs {rightNumeric ? std::to_string(rightNumber) : rightString};
                    if (op == "==")
                        return lhs == rhs;
                    if (op == "!=")
                        return lhs != rhs;
                    return false;
                }
            }

            bool valueNumeric {false};
            double valueNumber {0.0};
            std::string valueString;
            if (!resolveVariable(expr, valueNumeric, valueNumber, valueString))
                return false;

            if (valueNumeric)
                return valueNumber != 0.0;

            const std::string low {Utils::String::toLower(Utils::String::trim(valueString))};
            if (low == "" || low == "false" || low == "0" || low == "no" || low == "off")
                return false;

            return true;
        };

        return eval(expression);
    };

    return evaluateExpression(expression);
}

bool ThemeData::normalizeElementFormat(const pugi::xml_node& root, ThemeElement& element)
{
    // Compatibility with Batocera v29+ format: x, y, w, h instead of pos, size.
    // This function normalizes the legacy format to ESTACION-PRO standard.
    
    float x {-1.0f}, y {-1.0f}, w {-1.0f}, h {-1.0f};
    bool hasLegacyFormat {false};

    // Scan for x, y, w, h child nodes
    for (pugi::xml_node node {root.first_child()}; node; node = node.next_sibling()) {
        const char* nodeName {node.name()};
        std::string text {node.text().as_string()};

        if (strcmp(nodeName, "x") == 0) {
            x = static_cast<float>(atof(text.c_str()));
            hasLegacyFormat = true;
        }
        else if (strcmp(nodeName, "y") == 0) {
            y = static_cast<float>(atof(text.c_str()));
            hasLegacyFormat = true;
        }
        else if (strcmp(nodeName, "w") == 0) {
            w = static_cast<float>(atof(text.c_str()));
            hasLegacyFormat = true;
        }
        else if (strcmp(nodeName, "h") == 0) {
            h = static_cast<float>(atof(text.c_str()));
            hasLegacyFormat = true;
        }
    }

    // If we found legacy format elements, convert them
    if (hasLegacyFormat) {
        if (x >= 0.0f && y >= 0.0f) {
            // Create normalized pos property (x, y are already normalized 0-1)
            element.properties["pos"] = glm::vec2 {x, y};
            LOG(LogDebug) << "ThemeData::normalizeElementFormat(): Converting legacy format "
                             "x=" << x << ", y=" << y << " to pos";
        }
        if (w >= 0.0f && h >= 0.0f) {
            // Create normalized size property (w, h are already normalized 0-1)
            element.properties["size"] = glm::vec2 {w, h};
            LOG(LogDebug) << "ThemeData::normalizeElementFormat(): Converting legacy format "
                             "w=" << w << ", h=" << h << " to size";
        }
        return true;
    }

    return false;
}

ThemeData::ThemeCapability ThemeData::parseThemeCapabilities(const std::string& path)
{
    ThemeCapability capabilities;
    std::vector<std::string> aspectRatiosTemp;
    std::vector<std::string> fontSizesTemp;
    std::vector<std::string> languagesTemp;
    bool hasTriggers {false};

    const std::string capFile {path + "/capabilities.xml"};

    if (Utils::FileSystem::isRegularFile(capFile) || Utils::FileSystem::isSymlink(capFile)) {
        capabilities.validTheme = true;

        pugi::xml_document doc;
#if defined(_WIN64)
        const pugi::xml_parse_result& res {
            doc.load_file(Utils::String::stringToWideString(capFile).c_str())};
#else
        const pugi::xml_parse_result& res {doc.load_file(capFile.c_str())};
#endif
        if (res.status == pugi::status_no_document_element) {
            LOG(LogDebug) << "Found a capabilities.xml file with no configuration";
        }
        else if (!res) {
            LOG(LogError) << "Couldn't parse capabilities.xml: " << res.description();
            return capabilities;
        }
        const pugi::xml_node& themeCapabilities {doc.child("themeCapabilities")};
        if (!themeCapabilities) {
            LOG(LogError) << "Missing <themeCapabilities> tag in capabilities.xml";
            return capabilities;
        }

        const pugi::xml_node& themeName {themeCapabilities.child("themeName")};
        if (themeName != nullptr)
            capabilities.themeName = themeName.text().get();

        const std::string themeFile {path + "/theme.xml"};
        if (Utils::FileSystem::isRegularFile(themeFile) || Utils::FileSystem::isSymlink(themeFile)) {
            pugi::xml_document themeDoc;
#if defined(_WIN64)
            const pugi::xml_parse_result& themeRes {
                themeDoc.load_file(Utils::String::stringToWideString(themeFile).c_str())};
#else
            const pugi::xml_parse_result& themeRes {themeDoc.load_file(themeFile.c_str())};
#endif
            if (themeRes)
                capabilities.batoceraFormat = detectBatoceraThemeRoot(themeDoc.child("theme"));
        }

        for (pugi::xml_node aspectRatio {themeCapabilities.child("aspectRatio")}; aspectRatio;
             aspectRatio = aspectRatio.next_sibling("aspectRatio")) {
            const std::string& value {aspectRatio.text().get()};
            if (std::find_if(sSupportedAspectRatios.cbegin(), sSupportedAspectRatios.cend(),
                             [&value](const std::pair<std::string, std::string>& entry) {
                                 return entry.first == value;
                             }) == sSupportedAspectRatios.cend()) {
                LOG(LogWarning) << "Declared aspect ratio \"" << value
                                << "\" is not supported, ignoring entry in \"" << capFile << "\"";
            }
            else {
                if (std::find(aspectRatiosTemp.cbegin(), aspectRatiosTemp.cend(), value) !=
                    aspectRatiosTemp.cend()) {
                    LOG(LogWarning)
                        << "Aspect ratio \"" << value
                        << "\" is declared multiple times, ignoring entry in \"" << capFile << "\"";
                }
                else {
                    aspectRatiosTemp.emplace_back(value);
                }
            }
        }

        for (pugi::xml_node fontSize {themeCapabilities.child("fontSize")}; fontSize;
             fontSize = fontSize.next_sibling("fontSize")) {
            const std::string& value {fontSize.text().get()};
            if (std::find_if(sSupportedFontSizes.cbegin(), sSupportedFontSizes.cend(),
                             [&value](const std::pair<std::string, std::string>& entry) {
                                 return entry.first == value;
                             }) == sSupportedFontSizes.cend()) {
                LOG(LogWarning) << "Declared font size \"" << value
                                << "\" is not supported, ignoring entry in \"" << capFile << "\"";
            }
            else {
                if (std::find(fontSizesTemp.cbegin(), fontSizesTemp.cend(), value) !=
                    fontSizesTemp.cend()) {
                    LOG(LogWarning)
                        << "Font size \"" << value
                        << "\" is declared multiple times, ignoring entry in \"" << capFile << "\"";
                }
                else {
                    fontSizesTemp.emplace_back(value);
                }
            }
        }

        for (pugi::xml_node variant {themeCapabilities.child("variant")}; variant;
             variant = variant.next_sibling("variant")) {
            ThemeVariant readVariant;
            const std::string& name {variant.attribute("name").as_string()};
            if (name.empty()) {
                LOG(LogWarning)
                    << "Found <variant> tag without name attribute, ignoring entry in \"" << capFile
                    << "\"";
            }
            else if (name == "all") {
                LOG(LogWarning)
                    << "Found <variant> tag using reserved name \"all\", ignoring entry in \""
                    << capFile << "\"";
            }
            else {
                readVariant.name = name;
            }

            if (variant.child("label") == nullptr) {
                LOG(LogDebug)
                    << "No variant <label> tags found, setting label value to the variant name \""
                    << name << "\" for \"" << capFile << "\"";
                readVariant.labels.emplace_back(std::make_pair("en_US", name));
            }
            else {
                std::vector<std::pair<std::string, std::string>> labels;
                for (pugi::xml_node labelTag {variant.child("label")}; labelTag;
                     labelTag = labelTag.next_sibling("label")) {
                    std::string language {labelTag.attribute("language").as_string()};
                    if (language == "")
                        language = "en_US";
                    else if (std::find_if(sSupportedLanguages.cbegin(), sSupportedLanguages.cend(),
                                          [language](std::pair<std::string, std::string> currLang) {
                                              return currLang.first == language;
                                          }) == sSupportedLanguages.cend()) {
                        LOG(LogWarning) << "Declared language \"" << language
                                        << "\" not supported, setting label language to \"en_US\""
                                           " for variant name \""
                                        << name << "\" in \"" << capFile << "\"";
                        language = "en_US";
                    }
                    std::string labelValue {labelTag.text().as_string()};
                    if (labelValue == "") {
                        LOG(LogWarning) << "No variant <label> value defined, setting value to "
                                           "the variant name \""
                                        << name << "\" for \"" << capFile << "\"";
                        labelValue = name;
                    }
                    labels.emplace_back(std::make_pair(language, labelValue));
                }
                if (!labels.empty()) {
                    // Add the label languages in the order they are defined in sSupportedLanguages.
                    for (auto& language : sSupportedLanguages) {
                        if (language.first == "automatic")
                            continue;
                        const auto it =
                            std::find_if(labels.cbegin(), labels.cend(),
                                         [language](std::pair<std::string, std::string> currLabel) {
                                             return currLabel.first == language.first;
                                         });
                        if (it != labels.cend()) {
                            readVariant.labels.emplace_back(
                                std::make_pair((*it).first, (*it).second));
                        }
                    }
                }
            }

            const pugi::xml_node& selectableTag {variant.child("selectable")};
            if (selectableTag != nullptr) {
                const std::string& value {selectableTag.text().as_string()};
                if (value.front() == '0' || value.front() == 'f' || value.front() == 'F' ||
                    value.front() == 'n' || value.front() == 'N')
                    readVariant.selectable = false;
                else
                    readVariant.selectable = true;
            }

            for (pugi::xml_node overrideTag {variant.child("override")}; overrideTag;
                 overrideTag = overrideTag.next_sibling("override")) {
                if (overrideTag != nullptr) {
                    std::vector<std::string> mediaTypes;
                    const pugi::xml_node& mediaTypeTag {overrideTag.child("mediaType")};
                    if (mediaTypeTag != nullptr) {
                        std::string mediaTypeValue {mediaTypeTag.text().as_string()};
                        for (auto& character : mediaTypeValue) {
                            if (std::isspace(character))
                                character = ',';
                        }
                        mediaTypeValue = Utils::String::replace(mediaTypeValue, ",,", ",");
                        mediaTypes = Utils::String::delimitedStringToVector(mediaTypeValue, ",");

                        for (std::string& type : mediaTypes) {
                            if (std::find(sSupportedMediaTypes.cbegin(),
                                          sSupportedMediaTypes.cend(),
                                          type) == sSupportedMediaTypes.cend()) {
                                LOG(LogError) << "ThemeData::parseThemeCapabilities(): Invalid "
                                                 "override configuration, unsupported "
                                                 "\"mediaType\" value \""
                                              << type << "\"";
                                mediaTypes.clear();
                                break;
                            }
                        }
                    }

                    const pugi::xml_node& triggerTag {overrideTag.child("trigger")};
                    if (triggerTag != nullptr) {
                        const std::string& triggerValue {triggerTag.text().as_string()};
                        if (triggerValue == "") {
                            LOG(LogWarning) << "No <trigger> tag value defined for variant \""
                                            << readVariant.name << "\", ignoring entry in \""
                                            << capFile << "\"";
                        }
                        else if (triggerValue != "noVideos" && triggerValue != "noMedia") {
                            LOG(LogWarning) << "Invalid <useVariant> tag value \"" << triggerValue
                                            << "\" defined for variant \"" << readVariant.name
                                            << "\", ignoring entry in \"" << capFile << "\"";
                        }
                        else {
                            const pugi::xml_node& useVariantTag {overrideTag.child("useVariant")};
                            if (useVariantTag != nullptr) {
                                const std::string& useVariantValue {
                                    useVariantTag.text().as_string()};
                                if (useVariantValue == "") {
                                    LOG(LogWarning)
                                        << "No <useVariant> tag value defined for variant \""
                                        << readVariant.name << "\", ignoring entry in \"" << capFile
                                        << "\"";
                                }
                                else {
                                    hasTriggers = true;
                                    if (triggerValue == "noVideos") {
                                        readVariant
                                            .overrides[ThemeTriggers::TriggerType::NO_VIDEOS] =
                                            std::make_pair(useVariantValue,
                                                           std::vector<std::string>());
                                    }
                                    else if (triggerValue == "noMedia") {
                                        if (mediaTypes.empty())
                                            mediaTypes.emplace_back("miximage");
                                        readVariant
                                            .overrides[ThemeTriggers::TriggerType::NO_MEDIA] =
                                            std::make_pair(useVariantValue, mediaTypes);
                                    }
                                }
                            }
                            else {
                                LOG(LogWarning)
                                    << "Found an <override> tag without a corresponding "
                                       "<useVariant> tag, "
                                    << "ignoring entry for variant \"" << readVariant.name
                                    << "\" in \"" << capFile << "\"";
                            }
                        }
                    }
                }
                else {
                    LOG(LogWarning)
                        << "Found an <override> tag without a corresponding <trigger> tag, "
                        << "ignoring entry for variant \"" << readVariant.name << "\" in \""
                        << capFile << "\"";
                }
            }

            if (readVariant.name != "") {
                bool duplicate {false};
                for (auto& variant : capabilities.variants) {
                    if (variant.name == readVariant.name) {
                        LOG(LogWarning) << "Variant \"" << readVariant.name
                                        << "\" is declared multiple times, ignoring entry in \""
                                        << capFile << "\"";
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                    capabilities.variants.emplace_back(readVariant);
            }
        }

        for (pugi::xml_node colorScheme {themeCapabilities.child("colorScheme")}; colorScheme;
             colorScheme = colorScheme.next_sibling("colorScheme")) {
            ThemeColorScheme readColorScheme;
            const std::string& name {colorScheme.attribute("name").as_string()};
            if (name.empty()) {
                LOG(LogWarning)
                    << "Found <colorScheme> tag without name attribute, ignoring entry in \""
                    << capFile << "\"";
            }
            else {
                readColorScheme.name = name;
            }

            if (colorScheme.child("label") == nullptr) {
                LOG(LogDebug) << "No colorScheme <label> tag found, setting label value to the "
                                 "color scheme name \""
                              << name << "\" for \"" << capFile << "\"";
                readColorScheme.labels.emplace_back(std::make_pair("en_US", name));
            }
            else {
                std::vector<std::pair<std::string, std::string>> labels;
                for (pugi::xml_node labelTag {colorScheme.child("label")}; labelTag;
                     labelTag = labelTag.next_sibling("label")) {
                    std::string language {labelTag.attribute("language").as_string()};
                    if (language == "")
                        language = "en_US";
                    else if (std::find_if(sSupportedLanguages.cbegin(), sSupportedLanguages.cend(),
                                          [language](std::pair<std::string, std::string> currLang) {
                                              return currLang.first == language;
                                          }) == sSupportedLanguages.cend()) {
                        LOG(LogWarning) << "Declared language \"" << language
                                        << "\" not supported, setting label language to \"en_US\""
                                           " for color scheme name \""
                                        << name << "\" in \"" << capFile << "\"";
                        language = "en_US";
                    }
                    std::string labelValue {labelTag.text().as_string()};
                    if (labelValue == "") {
                        LOG(LogWarning) << "No colorScheme <label> value defined, setting value to "
                                           "the color scheme name \""
                                        << name << "\" for \"" << capFile << "\"";
                        labelValue = name;
                    }
                    labels.emplace_back(std::make_pair(language, labelValue));
                }
                if (!labels.empty()) {
                    // Add the label languages in the order they are defined in sSupportedLanguages.
                    for (auto& language : sSupportedLanguages) {
                        if (language.first == "automatic")
                            continue;
                        const auto it =
                            std::find_if(labels.cbegin(), labels.cend(),
                                         [language](std::pair<std::string, std::string> currLabel) {
                                             return currLabel.first == language.first;
                                         });
                        if (it != labels.cend()) {
                            readColorScheme.labels.emplace_back(
                                std::make_pair((*it).first, (*it).second));
                        }
                    }
                }
            }

            if (readColorScheme.name != "") {
                bool duplicate {false};
                for (auto& colorScheme : capabilities.colorSchemes) {
                    if (colorScheme.name == readColorScheme.name) {
                        LOG(LogWarning) << "Color scheme \"" << readColorScheme.name
                                        << "\" is declared multiple times, ignoring entry in \""
                                        << capFile << "\"";
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                    capabilities.colorSchemes.emplace_back(readColorScheme);
            }
        }

        for (pugi::xml_node language {themeCapabilities.child("language")}; language;
             language = language.next_sibling("language")) {
            const std::string& value {language.text().get()};
            if (std::find_if(sSupportedLanguages.cbegin(), sSupportedLanguages.cend(),
                             [&value](const std::pair<std::string, std::string>& entry) {
                                 return entry.first == value;
                             }) == sSupportedLanguages.cend()) {
                LOG(LogWarning) << "Declared language \"" << value
                                << "\" is not supported, ignoring entry in \"" << capFile << "\"";
            }
            else {
                if (std::find(languagesTemp.cbegin(), languagesTemp.cend(), value) !=
                    languagesTemp.cend()) {
                    LOG(LogWarning)
                        << "Language \"" << value
                        << "\" is declared multiple times, ignoring entry in \"" << capFile << "\"";
                }
                else {
                    languagesTemp.emplace_back(value);
                }
            }
        }

        if (languagesTemp.size() > 0 && std::find(languagesTemp.cbegin(), languagesTemp.cend(),
                                                  "en_US") == languagesTemp.cend()) {
            LOG(LogError) << "Theme has declared language support but is missing mandatory "
                          << "\"en_US\" entry in \"" << capFile << "\"";
            languagesTemp.clear();
        }

        for (pugi::xml_node transitions {themeCapabilities.child("transitions")}; transitions;
             transitions = transitions.next_sibling("transitions")) {
            std::map<ViewTransition, ViewTransitionAnimation> readTransitions;
            std::string name {transitions.attribute("name").as_string()};
            std::string label;
            bool selectable {true};

            if (name.empty()) {
                LOG(LogWarning)
                    << "Found <transitions> tag without name attribute, ignoring entry in \""
                    << capFile << "\"";
                name.clear();
            }
            else {
                if (std::find(sSupportedTransitionAnimations.cbegin(),
                              sSupportedTransitionAnimations.cend(),
                              name) != sSupportedTransitionAnimations.cend()) {
                    LOG(LogWarning)
                        << "Found <transitions> tag using reserved name attribute value \"" << name
                        << "\", ignoring entry in \"" << capFile << "\"";
                    name.clear();
                }
                else {
                    for (auto& transitionEntry : capabilities.transitions) {
                        if (transitionEntry.name == name) {
                            LOG(LogWarning)
                                << "Found <transitions> tag with previously used name attribute "
                                   "value \""
                                << name << "\", ignoring entry in \"" << capFile << "\"";
                            name.clear();
                        }
                    }
                }
            }

            if (name == "")
                continue;

            const pugi::xml_node& selectableTag {transitions.child("selectable")};
            if (selectableTag != nullptr) {
                const std::string& value {selectableTag.text().as_string()};
                if (value.front() == '0' || value.front() == 'f' || value.front() == 'F' ||
                    value.front() == 'n' || value.front() == 'N')
                    selectable = false;
            }

            for (auto& currTransition : sSupportedTransitions) {
                const pugi::xml_node& transitionTag {transitions.child(currTransition.c_str())};
                if (transitionTag != nullptr) {
                    const std::string& transitionValue {transitionTag.text().as_string()};
                    if (transitionValue.empty()) {
                        LOG(LogWarning) << "Found <" << currTransition
                                        << "> transition tag without any value, "
                                           "ignoring entry in \""
                                        << capFile << "\"";
                    }
                    else if (std::find(sSupportedTransitionAnimations.cbegin(),
                                       sSupportedTransitionAnimations.cend(),
                                       currTransition) != sSupportedTransitionAnimations.cend()) {
                        LOG(LogWarning)
                            << "Invalid <" << currTransition << "> transition tag value \""
                            << transitionValue << "\", ignoring entry in \"" << capFile << "\"";
                    }
                    else {
                        ViewTransitionAnimation transitionAnim {ViewTransitionAnimation::INSTANT};
                        if (transitionValue == "slide")
                            transitionAnim = ViewTransitionAnimation::SLIDE;
                        else if (transitionValue == "fade")
                            transitionAnim = ViewTransitionAnimation::FADE;

                        if (currTransition == "systemToSystem")
                            readTransitions[ViewTransition::SYSTEM_TO_SYSTEM] = transitionAnim;
                        else if (currTransition == "systemToGamelist")
                            readTransitions[ViewTransition::SYSTEM_TO_GAMELIST] = transitionAnim;
                        else if (currTransition == "gamelistToGamelist")
                            readTransitions[ViewTransition::GAMELIST_TO_GAMELIST] = transitionAnim;
                        else if (currTransition == "gamelistToSystem")
                            readTransitions[ViewTransition::GAMELIST_TO_SYSTEM] = transitionAnim;
                        else if (currTransition == "startupToSystem")
                            readTransitions[ViewTransition::STARTUP_TO_SYSTEM] = transitionAnim;
                        else if (currTransition == "startupToGamelist")
                            readTransitions[ViewTransition::STARTUP_TO_GAMELIST] = transitionAnim;
                    }
                }
            }

            if (!readTransitions.empty()) {
                // If startupToSystem and startupToGamelist are not defined, then set them
                // to the same values as systemToSystem and gamelistToGamelist respectively,
                // assuming those transitions have been defined.
                if (readTransitions.find(ViewTransition::STARTUP_TO_SYSTEM) ==
                    readTransitions.cend()) {
                    if (readTransitions.find(ViewTransition::SYSTEM_TO_SYSTEM) !=
                        readTransitions.cend())
                        readTransitions[ViewTransition::STARTUP_TO_SYSTEM] =
                            readTransitions[ViewTransition::SYSTEM_TO_SYSTEM];
                }
                if (readTransitions.find(ViewTransition::STARTUP_TO_GAMELIST) ==
                    readTransitions.cend()) {
                    if (readTransitions.find(ViewTransition::GAMELIST_TO_GAMELIST) !=
                        readTransitions.cend())
                        readTransitions[ViewTransition::STARTUP_TO_GAMELIST] =
                            readTransitions[ViewTransition::GAMELIST_TO_GAMELIST];
                }

                ThemeTransitions transition;
                transition.name = name;

                std::vector<std::pair<std::string, std::string>> labels;
                for (pugi::xml_node labelTag {transitions.child("label")}; labelTag;
                     labelTag = labelTag.next_sibling("label")) {
                    std::string language {labelTag.attribute("language").as_string()};
                    if (language == "")
                        language = "en_US";
                    else if (std::find_if(sSupportedLanguages.cbegin(), sSupportedLanguages.cend(),
                                          [language](std::pair<std::string, std::string> currLang) {
                                              return currLang.first == language;
                                          }) == sSupportedLanguages.cend()) {
                        LOG(LogWarning) << "Declared language \"" << language
                                        << "\" not supported, setting label language to \"en_US\""
                                           " for transition name \""
                                        << name << "\" in \"" << capFile << "\"";
                        language = "en_US";
                    }
                    std::string labelValue {labelTag.text().as_string()};
                    if (labelValue != "")
                        labels.emplace_back(std::make_pair(language, labelValue));
                }
                if (!labels.empty()) {
                    // Add the label languages in the order they are defined in sSupportedLanguages.
                    for (auto& language : sSupportedLanguages) {
                        if (language.first == "automatic")
                            continue;
                        const auto it =
                            std::find_if(labels.cbegin(), labels.cend(),
                                         [language](std::pair<std::string, std::string> currLabel) {
                                             return currLabel.first == language.first;
                                         });
                        if (it != labels.cend()) {
                            transition.labels.emplace_back(
                                std::make_pair((*it).first, (*it).second));
                        }
                    }
                }

                transition.selectable = selectable;
                transition.animations = std::move(readTransitions);
                capabilities.transitions.emplace_back(std::move(transition));
            }
        }

        for (pugi::xml_node suppressTransitionProfiles {
                 themeCapabilities.child("suppressTransitionProfiles")};
             suppressTransitionProfiles;
             suppressTransitionProfiles =
                 suppressTransitionProfiles.next_sibling("suppressTransitionProfiles")) {
            std::vector<std::string> readSuppressProfiles;

            for (pugi::xml_node entries {suppressTransitionProfiles.child("entry")}; entries;
                 entries = entries.next_sibling("entry")) {
                const std::string& entryValue {entries.text().as_string()};

                if (std::find(sSupportedTransitionAnimations.cbegin(),
                              sSupportedTransitionAnimations.cend(),
                              entryValue) != sSupportedTransitionAnimations.cend()) {
                    capabilities.suppressedTransitionProfiles.emplace_back(entryValue);
                }
                else {
                    LOG(LogWarning)
                        << "Found suppressTransitionProfiles <entry> tag with invalid value \""
                        << entryValue << "\", ignoring entry in \"" << capFile << "\"";
                }
            }

            // Sort and remove any duplicates.
            if (capabilities.suppressedTransitionProfiles.size() > 1) {
                std::sort(capabilities.suppressedTransitionProfiles.begin(),
                          capabilities.suppressedTransitionProfiles.end());
                auto last = std::unique(capabilities.suppressedTransitionProfiles.begin(),
                                        capabilities.suppressedTransitionProfiles.end());
                capabilities.suppressedTransitionProfiles.erase(
                    last, capabilities.suppressedTransitionProfiles.end());
            }
        }
    }
    else {
        capabilities.validTheme = false;
        LOG(LogWarning)
            << "No capabilities.xml file found, this does not appear to be a valid theme: \""
#if defined(_WIN64)
            << Utils::String::replace(path, "/", "\\") << "\"";
#else
            << path << "\"";
#endif
    }

    // Add the aspect ratios in the order they are defined in sSupportedAspectRatios so they
    // always show up in the same order in the UI Settings menu.
    if (!aspectRatiosTemp.empty()) {
        // Add the "automatic" aspect ratio if there is at least one entry.
        capabilities.aspectRatios.emplace_back(sSupportedAspectRatios.front().first);
        for (auto& aspectRatio : sSupportedAspectRatios) {
            if (std::find(aspectRatiosTemp.cbegin(), aspectRatiosTemp.cend(), aspectRatio.first) !=
                aspectRatiosTemp.cend()) {
                capabilities.aspectRatios.emplace_back(aspectRatio.first);
            }
        }
    }

    // Add the languages in the order they are defined in sSupportedLanguages so they always
    // show up in the same order in the UI Settings menu.
    if (!languagesTemp.empty()) {
        // Add the "automatic" language if there is at least one entry.
        capabilities.languages.emplace_back(sSupportedLanguages.front().first);
        for (auto& language : sSupportedLanguages) {
            if (std::find(languagesTemp.cbegin(), languagesTemp.cend(), language.first) !=
                languagesTemp.cend()) {
                capabilities.languages.emplace_back(language.first);
            }
        }
    }

    // Add the font sizes in the order they are defined in sSupportedFontSizes so they always
    // show up in the same order in the UI Settings menu.
    if (!fontSizesTemp.empty()) {
        for (auto& fontSize : sSupportedFontSizes) {
            if (std::find(fontSizesTemp.cbegin(), fontSizesTemp.cend(), fontSize.first) !=
                fontSizesTemp.cend()) {
                capabilities.fontSizes.emplace_back(fontSize.first);
            }
        }
    }

    if (hasTriggers) {
        for (auto& variant : capabilities.variants) {
            for (auto it = variant.overrides.begin(); it != variant.overrides.end();) {
                const auto variantIter =
                    std::find_if(capabilities.variants.begin(), capabilities.variants.end(),
                                 [it](ThemeVariant currVariant) {
                                     return currVariant.name == (*it).second.first;
                                 });
                if (variantIter == capabilities.variants.end()) {
                    LOG(LogWarning)
                        << "The <useVariant> tag value \"" << (*it).second.first
                        << "\" does not match any defined variants, ignoring entry in \"" << capFile
                        << "\"";
                    it = variant.overrides.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
    }

    // Some batocera themes don't provide <variant> entries in capabilities.xml and instead use
    // multiple gamelist view styles and/or <customView> blocks in theme.xml. Expose these through
    // the existing Theme Variant setting so they can be selected from the menu.
    if (capabilities.validTheme && capabilities.variants.empty()) {
        const std::string themeFile {path + "/theme.xml"};
        if (Utils::FileSystem::isRegularFile(themeFile) || Utils::FileSystem::isSymlink(themeFile)) {
            pugi::xml_document themeDoc;
#if defined(_WIN64)
            const pugi::xml_parse_result& themeRes {
                themeDoc.load_file(Utils::String::stringToWideString(themeFile).c_str())};
#else
            const pugi::xml_parse_result& themeRes {themeDoc.load_file(themeFile.c_str())};
#endif
            if (themeRes) {
                const pugi::xml_node root {themeDoc.child("theme")};

                const std::set<std::string> gamelistStyles {{"basic", "detailed", "video",
                                                             "grid", "gamecarousel"}};

                auto variantExists = [&capabilities](const std::string& name) {
                    return std::find_if(capabilities.variants.cbegin(), capabilities.variants.cend(),
                                        [&name](const ThemeVariant& variant) {
                                            return variant.name == name;
                                        }) != capabilities.variants.cend();
                };

                auto addVariant = [&capabilities, &variantExists](const std::string& name,
                                                                  const std::string& label) {
                    if (name.empty() || name == "all" || variantExists(name))
                        return;

                    ThemeVariant variant;
                    variant.name = name;
                    variant.selectable = true;
                    variant.gamelistViewStyle = true;
                    variant.labels.emplace_back(std::make_pair("en_US", label.empty() ? name : label));
                    capabilities.variants.emplace_back(std::move(variant));
                };

                for (pugi::xml_node view {root.child("view")}; view;
                     view = view.next_sibling("view")) {
                    const std::string names {view.attribute("name").as_string()};
                    const std::string delim {" \t\r\n,"};
                    size_t prevOff {names.find_first_not_of(delim, 0)};
                    size_t off {names.find_first_of(delim, prevOff)};

                    while (off != std::string::npos || prevOff != std::string::npos) {
                        const std::string viewName {names.substr(prevOff, off - prevOff)};
                        prevOff = names.find_first_not_of(delim, off);
                        off = names.find_first_of(delim, prevOff);

                        if (gamelistStyles.find(viewName) != gamelistStyles.end())
                            addVariant(viewName, Utils::String::toUpper(viewName));
                    }
                }

                for (pugi::xml_node customView {root.child("customView")}; customView;
                     customView = customView.next_sibling("customView")) {
                    const std::string namesAttr {customView.attribute("name").as_string()};
                    const std::string label {customView.attribute("displayName").as_string()};

                    // Batocera customView name may be comma-separated (e.g. "Tiles View,Icons").
                    // Only split by comma, preserve spaces inside names.
                    std::vector<std::string> customViewNames = Utils::String::delimitedStringToVector(namesAttr, ",");
                    for (std::string& singleName : customViewNames) {
                        singleName = Utils::String::trim(singleName);
                        if (!singleName.empty())
                            addVariant(singleName, label.empty() ? singleName : label);
                    }
                }
            }
        }
    }

    // Detect batocera <subset> elements from theme.xml and add them to capabilities.
    // Each <subset name="X"> creates a selectable group of includes.
    {
        const std::string themeFile {path + "/theme.xml"};
        if (capabilities.subsets.empty() &&
            (Utils::FileSystem::isRegularFile(themeFile) ||
             Utils::FileSystem::isSymlink(themeFile))) {
            std::set<std::string> visitedSubsetFiles;
            const auto subsetTranslation {translateBatoceraTag("subset")};
            const auto includeTranslation {translateBatoceraTag("include")};

            auto appendSubsetCapability = [&capabilities, &subsetTranslation,
                                           &includeTranslation](const pugi::xml_node& subsetNode) {
                const std::string subsetName {
                    getTranslatedAttributeValue(subsetNode, subsetTranslation, "name")};
                if (subsetName.empty())
                    return;

                ThemeSubset parsedSubset;
                parsedSubset.name = subsetName;
                parsedSubset.displayName =
                    getTranslatedAttributeValue(subsetNode, subsetTranslation, "displayName");
                if (parsedSubset.displayName.empty())
                    parsedSubset.displayName = subsetName;
                parsedSubset.ifSubsetCondition =
                    getTranslatedAttributeValue(subsetNode, subsetTranslation, "ifSubset");

                const std::string appliesToValue {
                    getTranslatedAttributeValue(subsetNode, subsetTranslation, "appliesTo")};
                if (!appliesToValue.empty()) {
                    std::vector<std::string> appliesToValues {
                        Utils::String::delimitedStringToVector(appliesToValue, ",")};
                    for (std::string& entry : appliesToValues) {
                        entry = Utils::String::toLower(Utils::String::trim(entry));
                        if (!entry.empty())
                            parsedSubset.appliesTo.emplace_back(entry);
                    }
                }

                for (pugi::xml_node incNode {subsetNode.child("include")}; incNode;
                     incNode = incNode.next_sibling("include")) {
                    const std::string optName {
                        getTranslatedAttributeValue(incNode, includeTranslation, "name")};
                    if (optName.empty())
                        continue;

                    std::string optLabel {
                        getTranslatedAttributeValue(incNode, includeTranslation, "displayName")};
                    if (optLabel.empty())
                        optLabel = optName;
                    parsedSubset.options.emplace_back(std::make_pair(optName, optLabel));
                }

                if (parsedSubset.options.empty())
                    return;

                const std::string normalizedSubsetName {normalizeThemeSubsetName(subsetName)};
                auto existingIt = std::find_if(capabilities.subsets.begin(), capabilities.subsets.end(),
                                               [&normalizedSubsetName](const ThemeSubset& subset) {
                                                   return normalizeThemeSubsetName(subset.name) ==
                                                          normalizedSubsetName;
                                               });

                if (existingIt == capabilities.subsets.end()) {
                    capabilities.subsets.emplace_back(std::move(parsedSubset));
                    return;
                }

                const bool existingDisplayIsPlaceholder {
                    existingIt->displayName.empty() ||
                    existingIt->displayName.find("${") != std::string::npos};
                const bool parsedDisplayIsPlaceholder {
                    parsedSubset.displayName.empty() ||
                    parsedSubset.displayName.find("${") != std::string::npos};
                if (existingDisplayIsPlaceholder && !parsedDisplayIsPlaceholder)
                    existingIt->displayName = parsedSubset.displayName;

                if (existingIt->ifSubsetCondition.empty() && !parsedSubset.ifSubsetCondition.empty())
                    existingIt->ifSubsetCondition = parsedSubset.ifSubsetCondition;

                for (const std::string& appliesTo : parsedSubset.appliesTo) {
                    if (std::find(existingIt->appliesTo.cbegin(), existingIt->appliesTo.cend(),
                                  appliesTo) == existingIt->appliesTo.cend()) {
                        existingIt->appliesTo.emplace_back(appliesTo);
                    }
                }

                for (const auto& option : parsedSubset.options) {
                    auto existingOption = std::find_if(
                        existingIt->options.begin(), existingIt->options.end(),
                        [&option](const std::pair<std::string, std::string>& candidate) {
                            return candidate.first == option.first;
                        });

                    if (existingOption == existingIt->options.end()) {
                        existingIt->options.emplace_back(option);
                        continue;
                    }

                    const bool existingLabelIsPlaceholder {
                        existingOption->second.empty() ||
                        existingOption->second.find("${") != std::string::npos};
                    const bool parsedLabelIsPlaceholder {
                        option.second.empty() || option.second.find("${") != std::string::npos};
                    if (existingLabelIsPlaceholder && !parsedLabelIsPlaceholder)
                        existingOption->second = option.second;
                }
            };

            auto appendInlineSubsetCapability = [&capabilities, &includeTranslation](
                                                  const pugi::xml_node& includeNode) {
                const std::string subsetName {
                    getTranslatedAttributeValue(includeNode, includeTranslation, "subset")};
                const std::string optionName {
                    getTranslatedAttributeValue(includeNode, includeTranslation, "name")};

                if (subsetName.empty() || optionName.empty())
                    return;

                const std::string normalizedSubsetName {normalizeThemeSubsetName(subsetName)};

                auto subsetIt =
                    std::find_if(capabilities.subsets.begin(), capabilities.subsets.end(),
                                 [&normalizedSubsetName](const ThemeSubset& subset) {
                                     return normalizeThemeSubsetName(subset.name) ==
                                            normalizedSubsetName;
                                 });

                if (subsetIt == capabilities.subsets.end()) {
                    ThemeSubset newSubset;
                    newSubset.name = subsetName;

                    std::string subsetDisplayName {
                        getTranslatedAttributeValue(includeNode, includeTranslation, "displayName",
                                                   {"subSetDisplayName",
                                                    "subsetDisplayName"})};
                    if (subsetDisplayName.empty())
                        subsetDisplayName = subsetName;

                    newSubset.displayName = subsetDisplayName;
                    newSubset.ifSubsetCondition =
                        getTranslatedAttributeValue(includeNode, includeTranslation, "ifSubset");

                    const std::string appliesToValue {
                        getTranslatedAttributeValue(includeNode, includeTranslation,
                                                   "appliesTo")};
                    if (!appliesToValue.empty()) {
                        std::vector<std::string> appliesToValues {
                            Utils::String::delimitedStringToVector(appliesToValue, ",")};
                        for (std::string& entry : appliesToValues) {
                            entry = Utils::String::toLower(Utils::String::trim(entry));
                            if (!entry.empty())
                                newSubset.appliesTo.emplace_back(entry);
                        }
                    }

                    capabilities.subsets.emplace_back(std::move(newSubset));
                    subsetIt = capabilities.subsets.end() - 1;
                }

                std::string optionLabel {
                    getTranslatedAttributeValue(includeNode, includeTranslation, "displayName")};
                if (optionLabel.empty())
                    optionLabel = optionName;

                auto optionIt = std::find_if(
                    subsetIt->options.begin(), subsetIt->options.end(),
                    [&optionName](const std::pair<std::string, std::string>& option) {
                        return option.first == optionName;
                    });

                if (optionIt == subsetIt->options.end())
                    subsetIt->options.emplace_back(std::make_pair(optionName, optionLabel));
            };

            std::function<void(const std::string&)> scanSubsetCapabilities =
                [&](const std::string& xmlPath) {
                    if (xmlPath.empty())
                        return;

                    const std::string canonicalPath {Utils::FileSystem::getCanonicalPath(xmlPath)};
                    const std::string visitedPath {!canonicalPath.empty() ? canonicalPath :
                                                       xmlPath};
                    if (visitedSubsetFiles.find(visitedPath) != visitedSubsetFiles.cend())
                        return;
                    visitedSubsetFiles.insert(visitedPath);

                    pugi::xml_document themeDoc;
#if defined(_WIN64)
                    const pugi::xml_parse_result& themeRes {
                        themeDoc.load_file(Utils::String::stringToWideString(xmlPath).c_str())};
#else
                    const pugi::xml_parse_result& themeRes {themeDoc.load_file(xmlPath.c_str())};
#endif
                    if (!themeRes)
                        return;

                    const pugi::xml_node root {themeDoc.child("theme")};
                    if (!root)
                        return;

                    for (pugi::xml_node subsetNode {root.child("subset")}; subsetNode;
                         subsetNode = subsetNode.next_sibling("subset")) {
                        appendSubsetCapability(subsetNode);

                        for (pugi::xml_node incNode {subsetNode.child("include")}; incNode;
                             incNode = incNode.next_sibling("include")) {
                            const std::string relPath {
                                getIncludeNodePathCompat(incNode, includeTranslation)};
                            if (relPath.empty())
                                continue;

                            const std::string includePath {
                                resolveThemeIncludePathCompat(relPath, xmlPath)};
                            if (ResourceManager::getInstance().fileExists(includePath))
                                scanSubsetCapabilities(includePath);
                        }
                    }

                    for (pugi::xml_node includeNode {root.child("include")}; includeNode;
                         includeNode = includeNode.next_sibling("include")) {
                        appendInlineSubsetCapability(includeNode);

                        const std::string relPath {
                            getIncludeNodePathCompat(includeNode, includeTranslation)};
                        if (relPath.empty())
                            continue;

                        const std::string includePath {
                            resolveThemeIncludePathCompat(relPath, xmlPath)};
                        if (ResourceManager::getInstance().fileExists(includePath))
                            scanSubsetCapabilities(includePath);
                    }
                };

            scanSubsetCapabilities(themeFile);
        }
    }

    return capabilities;
}

void ThemeData::parseSubsets(const pugi::xml_node& root)
{
    const std::string currentLanguage {
        !sThemeLanguage.empty() ? sThemeLanguage : Utils::Localization::sCurrentLocale};
    const auto subsetTranslation {translateBatoceraTag("subset")};
    const auto includeTranslation {translateBatoceraTag("include")};

    for (pugi::xml_node node {root.child("subset")}; node; node = node.next_sibling("subset")) {
        ThemeException error;
        error << "ThemeData::parseSubsets(): ";
        error.setFiles(mPaths);

        if (!passesBatoceraConditionalAttributes(node, currentLanguage))
            continue;

        const std::string subsetName {
            getTranslatedAttributeValue(node, subsetTranslation, "name")};
        if (subsetName.empty()) {
            LOG(LogDebug) << error.message << ": <subset> tag missing \"name\" attribute, skipping";
            continue;
        }

        // Determine which option is selected for this subset.
        const std::string selectedOption {
            getResolvedThemeSubsetSelection(subsetName, mSelectedSystemView, mSubsetSelections)};

        // Find the matching include, falling back to the first one if nothing is selected.
        bool loaded = false;
        pugi::xml_node firstInc;

        for (pugi::xml_node inc {node.child("include")}; inc; inc = inc.next_sibling("include")) {
            if (!passesBatoceraConditionalAttributes(inc, currentLanguage))
                continue;

            const std::string optName {
                getTranslatedAttributeValue(inc, includeTranslation, "name")};
            if (optName.empty())
                continue;

            if (!firstInc)
                firstInc = inc;

            if (optName == selectedOption) {
                // Load this include file.
                std::string relPath {
                    resolvePlaceholders(getIncludeNodePathCompat(inc, includeTranslation))};
                if (relPath.empty())
                    continue;
                std::string path {resolveThemeIncludePathCompat(relPath, mPaths.back())};

                if (!ResourceManager::getInstance().fileExists(path)) {
                    LOG(LogDebug) << error.message << ": Couldn't find file \"" << relPath
                                  << "\" (subset \"" << subsetName << "\", option \"" << optName
                                  << "\")";
                    continue;
                }

                mPaths.push_back(path);
                pugi::xml_document incDoc;
#if defined(_WIN64)
                pugi::xml_parse_result result {
                    incDoc.load_file(Utils::String::stringToWideString(path).c_str())};
#else
                pugi::xml_parse_result result {incDoc.load_file(path.c_str())};
#endif
                if (!result) {
                    mPaths.pop_back();
                    LOG(LogWarning) << error.message << ": Error parsing file \"" << path
                                    << "\": " << result.description();
                    continue;
                }

                pugi::xml_node incTheme {incDoc.child("theme")};
                if (!incTheme) {
                    mPaths.pop_back();
                    LOG(LogWarning) << error.message << ": Missing <theme> tag in \"" << path
                                    << "\"";
                    continue;
                }

                parseTransitions(incTheme);
                parseVariables(incTheme);
                parseColorSchemes(incTheme);
                parseFontSizes(incTheme);
                parseLanguages(incTheme);
                parseIncludes(incTheme);
                parseViews(incTheme);
                parseFeatures(incTheme);
                parseVariants(incTheme);
                parseCustomViews(incTheme);
                parseSubsets(incTheme);
                parseAspectRatios(incTheme);
                mPaths.pop_back();
                loaded = true;
                break;
            }
        }

        // If no matching option was found and options exist, load the first one as default.
        if (!loaded && firstInc) {
            std::string relPath {
                resolvePlaceholders(getIncludeNodePathCompat(firstInc, includeTranslation))};
            if (!relPath.empty()) {
                std::string path {resolveThemeIncludePathCompat(relPath, mPaths.back())};

                if (ResourceManager::getInstance().fileExists(path)) {
                    mPaths.push_back(path);
                    pugi::xml_document incDoc;
#if defined(_WIN64)
                    pugi::xml_parse_result result {
                        incDoc.load_file(Utils::String::stringToWideString(path).c_str())};
#else
                    pugi::xml_parse_result result {incDoc.load_file(path.c_str())};
#endif
                    if (result) {
                        pugi::xml_node incTheme {incDoc.child("theme")};
                        if (incTheme) {
                            parseTransitions(incTheme);
                            parseVariables(incTheme);
                            parseColorSchemes(incTheme);
                            parseFontSizes(incTheme);
                            parseLanguages(incTheme);
                            parseIncludes(incTheme);
                            parseViews(incTheme);
                            parseFeatures(incTheme);
                            parseVariants(incTheme);
                            parseCustomViews(incTheme);
                            parseSubsets(incTheme);
                            parseAspectRatios(incTheme);
                        }
                    }
                    mPaths.pop_back();
                }
            }
        }
    }
}

void ThemeData::parseSubsetVariables(const pugi::xml_node& root)
{
    const std::string currentLanguage {
        !sThemeLanguage.empty() ? sThemeLanguage : Utils::Localization::sCurrentLocale};
    const auto subsetTranslation {translateBatoceraTag("subset")};
    const auto includeTranslation {translateBatoceraTag("include")};

    for (pugi::xml_node node {root.child("subset")}; node; node = node.next_sibling("subset")) {
        ThemeException error;
        error << "ThemeData::parseSubsetVariables(): ";
        error.setFiles(mPaths);

        if (!passesBatoceraConditionalAttributes(node, currentLanguage))
            continue;

        const std::string subsetName {
            getTranslatedAttributeValue(node, subsetTranslation, "name")};
        if (subsetName.empty())
            continue;

        const std::string selectedOption {
            getResolvedThemeSubsetSelection(subsetName, mSelectedSystemView, mSubsetSelections)};

        pugi::xml_node includeNode;
        pugi::xml_node firstInc;

        for (pugi::xml_node inc {node.child("include")}; inc; inc = inc.next_sibling("include")) {
            if (!passesBatoceraConditionalAttributes(inc, currentLanguage))
                continue;

            const std::string optName {
                getTranslatedAttributeValue(inc, includeTranslation, "name")};
            if (optName.empty())
                continue;

            if (!firstInc)
                firstInc = inc;

            if (!selectedOption.empty() && optName == selectedOption) {
                includeNode = inc;
                break;
            }
        }

        if (!includeNode)
            includeNode = firstInc;

        if (!includeNode)
            continue;

        std::string relPath {
            resolvePlaceholders(getIncludeNodePathCompat(includeNode, includeTranslation))};
        if (relPath.empty())
            continue;

        std::string path {resolveThemeIncludePathCompat(relPath, mPaths.back())};
        if (!ResourceManager::getInstance().fileExists(path)) {
            LOG(LogDebug) << error.message << ": Couldn't find file \"" << relPath
                          << "\" (subset \"" << subsetName << "\")";
            continue;
        }

        if (mSubsetVariableVisitedPaths.find(path) != mSubsetVariableVisitedPaths.end())
            continue;

        mSubsetVariableVisitedPaths.insert(path);

        mPaths.push_back(path);
        pugi::xml_document incDoc;
#if defined(_WIN64)
        pugi::xml_parse_result result {
            incDoc.load_file(Utils::String::stringToWideString(path).c_str())};
#else
        pugi::xml_parse_result result {incDoc.load_file(path.c_str())};
#endif
        if (!result) {
            mPaths.pop_back();
            continue;
        }

        pugi::xml_node incTheme {incDoc.child("theme")};
        if (!incTheme) {
            mPaths.pop_back();
            continue;
        }

        parseVariables(incTheme);
        parseSubsetVariables(incTheme);
        mPaths.pop_back();
    }
}

void ThemeData::parseIncludes(const pugi::xml_node& root)
{
    const std::string currentLanguage {
        !sThemeLanguage.empty() ? sThemeLanguage : Utils::Localization::sCurrentLocale};
    const auto includeTranslation {translateBatoceraTag("include")};

    for (pugi::xml_node node {root.child("include")}; node; node = node.next_sibling("include")) {
        ThemeException error;
        error << "ThemeData::parseIncludes(): ";
        error.setFiles(mPaths);

        // Ignore optional formatVersion to improve compatibility with batocera themes.

        // Skip includes that belong to a batocera-style subset (handled by parseSubsets).
        // These have a 'subset' attribute on the <include> tag itself.
        const std::string inlineSubset {
            getTranslatedAttributeValue(node, includeTranslation, "subset")};
        if (!inlineSubset.empty()) {
            // Inline subset include: only load if the option name matches the selection.
            const std::string optName {
                getTranslatedAttributeValue(node, includeTranslation, "name")};
            const std::string selectedOption {getResolvedThemeSubsetSelection(
                inlineSubset, mSelectedSystemView, mSubsetSelections)};
            // If we have a selection and it doesn't match, skip this include.
            if (!selectedOption.empty() &&
                Utils::String::toLower(Utils::String::trim(optName)) !=
                    Utils::String::toLower(Utils::String::trim(selectedOption)))
                continue;

            // If no selection is stored, only load the first declared option.
            if (selectedOption.empty() && !optName.empty()) {
                bool firstOptionForSubset {true};
                for (pugi::xml_node previousNode {root.child("include")}; previousNode &&
                     previousNode != node;
                     previousNode = previousNode.next_sibling("include")) {
                    if (normalizeThemeSubsetName(
                            getTranslatedAttributeValue(previousNode, includeTranslation,
                                                       "subset")) ==
                        normalizeThemeSubsetName(inlineSubset)) {
                        const std::string previousName {
                            getTranslatedAttributeValue(previousNode, includeTranslation,
                                                       "name")};
                        if (!previousName.empty()) {
                            firstOptionForSubset = false;
                            break;
                        }
                    }
                }

                if (!firstOptionForSubset)
                    continue;
            }
        }

        if (!passesBatoceraConditionalAttributes(node, currentLanguage))
            continue;

        const std::string rawIncludePath {getIncludeNodePathCompat(node, includeTranslation)};
        std::string relPath {resolvePlaceholders(rawIncludePath)};
        if (relPath.empty())
            continue;
        std::string path {resolveThemeIncludePathCompat(relPath, mPaths.back())};

        if (!ResourceManager::getInstance().fileExists(path)) {
            // For explicit paths, throw an error if the file couldn't be found, but only
            // print a debug message if it was set using a variable.
            if (relPath == rawIncludePath) {
                throw error << " -> \"" << relPath << "\" not found (resolved to \"" << path
                            << "\")";
            }
            else {
                if (!(Settings::getInstance()->getBool("DebugSkipMissingThemeFiles") ||
                      (mCustomCollection && Settings::getInstance()->getBool(
                                                "DebugSkipMissingThemeFilesCustomCollections")))) {
#if defined(_WIN64)
                    LOG(LogDebug) << Utils::String::replace(error.message, "/", "\\")
                                  << ": Couldn't find file \"" << rawIncludePath << "\" "
                                  << ((rawIncludePath != path) ?
                                          "which resolves to \"" +
                                              Utils::String::replace(path, "/", "\\") + "\"" :
#else
                    LOG(LogDebug) << error.message << ": Couldn't find file \"" << rawIncludePath
                                  << "\" "
                                  << ((rawIncludePath != path) ?
                                          "which resolves to \"" + path + "\"" :
#endif
                                          "");
                }
                continue;
            }
        }

        error << " -> \"" << relPath << "\"";

        mPaths.push_back(path);

        pugi::xml_document includeDoc;
#if defined(_WIN64)
        pugi::xml_parse_result result {
            includeDoc.load_file(Utils::String::stringToWideString(path).c_str())};
#else
        pugi::xml_parse_result result {includeDoc.load_file(path.c_str())};
#endif
        if (!result)
            throw error << ": Error parsing file: " << result.description();

        pugi::xml_node theme {includeDoc.child("theme")};
        if (!theme)
            throw error << ": Missing <theme> tag";

        parseTransitions(theme);
        parseVariables(theme);
        parseColorSchemes(theme);
        parseFontSizes(theme);
        parseLanguages(theme);
        parseIncludes(theme);
        parseViews(theme);
        parseFeatures(theme);
        parseVariants(theme);
        parseCustomViews(theme);
        parseSubsets(theme);
        parseAspectRatios(theme);

        mPaths.pop_back();
    }
}

void ThemeData::parseVariants(const pugi::xml_node& root)
{
    if (sCurrentTheme == sThemes.end())
        return;

    if (mSelectedVariant == "")
        return;

    ThemeException error;
    error << "ThemeData::parseVariants(): ";
    error.setFiles(mPaths);

    for (pugi::xml_node node {root.child("variant")}; node; node = node.next_sibling("variant")) {
        if (!node.attribute("name")) {
            LOG(LogDebug) << error.message << ": <variant> tag missing \"name\" attribute, skipping";
            continue;
        }

        const std::string delim {" \t\r\n,"};
        const std::string nameAttr {node.attribute("name").as_string()};
        size_t prevOff {nameAttr.find_first_not_of(delim, 0)};
        size_t off {nameAttr.find_first_of(delim, prevOff)};
        std::string viewKey;
        while (off != std::string::npos || prevOff != std::string::npos) {
            viewKey = nameAttr.substr(prevOff, off - prevOff);
            prevOff = nameAttr.find_first_not_of(delim, off);
            off = nameAttr.find_first_of(delim, prevOff);

            if (std::find(mVariants.cbegin(), mVariants.cend(), viewKey) == mVariants.cend()) {
                LOG(LogDebug) << error.message << ": <variant> value \"" << viewKey
                              << "\" is not defined in capabilities.xml, skipping";
                continue;
            }

            const std::string variant {mOverrideVariant.empty() ? mSelectedVariant :
                                                                  mOverrideVariant};

            if (variant == viewKey || viewKey == "all") {
                parseTransitions(node);
                parseVariables(node);
                parseColorSchemes(node);
                parseFontSizes(node);
                parseLanguages(node);
                parseIncludes(node);
                parseViews(node);
                parseAspectRatios(node);
            }
        }
    }
}

void ThemeData::parseColorSchemes(const pugi::xml_node& root)
{
    if (sCurrentTheme == sThemes.end())
        return;

    if (mSelectedColorScheme == "")
        return;

    ThemeException error;
    error << "ThemeData::parseColorSchemes(): ";
    error.setFiles(mPaths);

    for (pugi::xml_node node {root.child("colorScheme")}; node;
         node = node.next_sibling("colorScheme")) {
        if (!node.attribute("name")) {
            LOG(LogDebug)
                << error.message << ": <colorScheme> tag missing \"name\" attribute, skipping";
            continue;
        }

        const std::string delim {" \t\r\n,"};
        const std::string nameAttr {node.attribute("name").as_string()};
        size_t prevOff {nameAttr.find_first_not_of(delim, 0)};
        size_t off {nameAttr.find_first_of(delim, prevOff)};
        std::string viewKey;
        while (off != std::string::npos || prevOff != std::string::npos) {
            viewKey = nameAttr.substr(prevOff, off - prevOff);
            prevOff = nameAttr.find_first_not_of(delim, off);
            off = nameAttr.find_first_of(delim, prevOff);

            if (std::find(mColorSchemes.cbegin(), mColorSchemes.cend(), viewKey) ==
                mColorSchemes.cend()) {
                LOG(LogDebug) << error.message << ": <colorScheme> value \"" << viewKey
                              << "\" is not defined in capabilities.xml, skipping";
                continue;
            }

            if (mSelectedColorScheme == viewKey) {
                parseVariables(node);
                parseIncludes(node);
            }
        }
    }
}

void ThemeData::parseFontSizes(const pugi::xml_node& root)
{
    if (sCurrentTheme == sThemes.end())
        return;

    if (mSelectedFontSize == "")
        return;

    ThemeException error;
    error << "ThemeData::parseFontSizes(): ";
    error.setFiles(mPaths);

    for (pugi::xml_node node {root.child("fontSize")}; node; node = node.next_sibling("fontSize")) {
        if (!node.attribute("name")) {
            LOG(LogDebug)
                << error.message << ": <fontSize> tag missing \"name\" attribute, skipping";
            continue;
        }

        const std::string delim {" \t\r\n,"};
        const std::string nameAttr {node.attribute("name").as_string()};
        size_t prevOff {nameAttr.find_first_not_of(delim, 0)};
        size_t off {nameAttr.find_first_of(delim, prevOff)};
        std::string viewKey;
        while (off != std::string::npos || prevOff != std::string::npos) {
            viewKey = nameAttr.substr(prevOff, off - prevOff);
            prevOff = nameAttr.find_first_not_of(delim, off);
            off = nameAttr.find_first_of(delim, prevOff);

            if (std::find(mFontSizes.cbegin(), mFontSizes.cend(), viewKey) == mFontSizes.cend()) {
                LOG(LogDebug) << error.message << ": <fontSize> value \"" << viewKey
                              << "\" is not defined in capabilities.xml, skipping";
                continue;
            }

            if (mSelectedFontSize == viewKey) {
                parseVariables(node);
                parseIncludes(node);
            }
        }
    }
}

void ThemeData::parseAspectRatios(const pugi::xml_node& root)
{
    if (sCurrentTheme == sThemes.end())
        return;

    if (sSelectedAspectRatio == "")
        return;

    ThemeException error;
    error << "ThemeData::parseAspectRatios(): ";
    error.setFiles(mPaths);

    for (pugi::xml_node node {root.child("aspectRatio")}; node;
         node = node.next_sibling("aspectRatio")) {
        if (!node.attribute("name")) {
            LOG(LogDebug)
                << error.message << ": <aspectRatio> tag missing \"name\" attribute, skipping";
            continue;
        }

        const std::string delim {" \t\r\n,"};
        const std::string nameAttr {node.attribute("name").as_string()};
        size_t prevOff {nameAttr.find_first_not_of(delim, 0)};
        size_t off {nameAttr.find_first_of(delim, prevOff)};
        std::string viewKey;
        while (off != std::string::npos || prevOff != std::string::npos) {
            viewKey = nameAttr.substr(prevOff, off - prevOff);
            prevOff = nameAttr.find_first_not_of(delim, off);
            off = nameAttr.find_first_of(delim, prevOff);

            if (std::find(sCurrentTheme->second.capabilities.aspectRatios.cbegin(),
                          sCurrentTheme->second.capabilities.aspectRatios.cend(),
                          viewKey) == sCurrentTheme->second.capabilities.aspectRatios.cend()) {
                LOG(LogDebug) << error.message << ": <aspectRatio> value \"" << viewKey
                              << "\" is not defined in capabilities.xml, skipping";
                continue;
            }

            if (sSelectedAspectRatio == viewKey) {
                parseVariables(node);
                parseColorSchemes(node);
                parseFontSizes(node);
                parseLanguages(node);
                parseIncludes(node);
                parseViews(node);
            }
        }
    }
}

void ThemeData::parseTransitions(const pugi::xml_node& root)
{
    ThemeException error;
    error << "ThemeData::parseTransitions(): ";
    error.setFiles(mPaths);

    const pugi::xml_node& transitions {root.child("transitions")};
    if (transitions != nullptr) {
        const std::string& transitionsValue {transitions.text().as_string()};
        if (std::find_if(sCurrentTheme->second.capabilities.transitions.cbegin(),
                         sCurrentTheme->second.capabilities.transitions.cend(),
                         [&transitionsValue](const ThemeTransitions transitions) {
                             return transitions.name == transitionsValue;
                         }) == sCurrentTheme->second.capabilities.transitions.cend()) {
            throw error << ": <transitions> value \"" << transitionsValue
                        << "\" is not matching any defined transitions";
        }
        sVariantDefinedTransitions = transitionsValue;
    }
}

void ThemeData::parseLanguages(const pugi::xml_node& root)
{
    if (sCurrentTheme == sThemes.end())
        return;

    if (sThemeLanguage == "")
        return;

    ThemeException error;
    error << "ThemeData::parseLanguages(): ";
    error.setFiles(mPaths);

    for (pugi::xml_node node {root.child("language")}; node; node = node.next_sibling("language")) {
        if (!node.attribute("name")) {
            LOG(LogDebug)
                << error.message << ": <language> tag missing \"name\" attribute, skipping";
            continue;
        }

        const std::string delim {" \t\r\n,"};
        const std::string nameAttr {node.attribute("name").as_string()};
        size_t prevOff {nameAttr.find_first_not_of(delim, 0)};
        size_t off {nameAttr.find_first_of(delim, prevOff)};
        std::string viewKey;
        while (off != std::string::npos || prevOff != std::string::npos) {
            viewKey = nameAttr.substr(prevOff, off - prevOff);
            prevOff = nameAttr.find_first_not_of(delim, off);
            off = nameAttr.find_first_of(delim, prevOff);

            if (std::find(mLanguages.cbegin(), mLanguages.cend(), viewKey) == mLanguages.cend()) {
                LOG(LogDebug) << error.message << ": <language> value \"" << viewKey
                              << "\" is not defined in capabilities.xml, skipping";
                continue;
            }

            if (sThemeLanguage == viewKey) {
                parseVariables(node);
                parseIncludes(node);
            }
        }
    }
}

void ThemeData::parseVariables(const pugi::xml_node& root)
{
    ThemeException error;
    error.setFiles(mPaths);

    for (pugi::xml_node node {root.child("variables")}; node;
         node = node.next_sibling("variables")) {

        for (pugi::xml_node_iterator it = node.begin(); it != node.end(); ++it) {
            const std::string key {it->name()};
            const std::string val {resolvePlaceholders(it->text().as_string())};

            if (!val.empty()) {
                if (mVariables.find(key) != mVariables.end())
                    mVariables[key] = val;
                else
                    mVariables.insert(std::pair<std::string, std::string>(key, val));
            }
        }
    }
}

void ThemeData::parseViews(const pugi::xml_node& root)
{
    ThemeException error;
    error << "ThemeData::parseViews(): ";
    error.setFiles(mPaths);

    const std::set<std::string> gamelistStyles {{"basic", "detailed", "video", "grid",
                                                 "gamecarousel"}};

    auto customViewInheritsStyle = [&root, &gamelistStyles](const std::string& customViewName,
                                                             const std::string& styleName) {
        std::map<std::string, std::string> customViewInheritance;
        for (pugi::xml_node customView {root.child("customView")}; customView;
             customView = customView.next_sibling("customView")) {
            const std::string nameAttr {customView.attribute("name").as_string()};
            if (nameAttr.empty())
                continue;
            const std::string inherits {customView.attribute("inherits").as_string()};
            // Batocera customView name may be comma-separated (e.g. "Tiles View,Icons").
            std::vector<std::string> customViewNames {
                Utils::String::delimitedStringToVector(nameAttr, ",")};
            for (std::string& singleName : customViewNames) {
                singleName = Utils::String::trim(singleName);
                if (!singleName.empty())
                    customViewInheritance[singleName] = inherits;
            }
        }

        std::set<std::string> visited;
        std::function<bool(const std::string&)> recurse = [&](const std::string& name) {
            if (!visited.insert(name).second)
                return false;

            auto it = customViewInheritance.find(name);
            if (it == customViewInheritance.end())
                return false;

            const std::string& parent {it->second};
            if (parent.empty())
                return false;
            if (parent == styleName)
                return true;
            if (gamelistStyles.find(parent) != gamelistStyles.end())
                return false;

            return recurse(parent);
        };

        return recurse(customViewName);
    };

    // Build set of all customView names (Batocera PascalCase names like Tiles View, Icons, Boxes…).
    // These are gamelist variants: when a view block names them, remap to "gamelist".
    std::set<std::string> customViewNames;
    {
        for (pugi::xml_node cv {root.child("customView")}; cv; cv = cv.next_sibling("customView")) {
            const std::string nameAttr {cv.attribute("name").as_string()};
            if (nameAttr.empty())
                continue;
            std::vector<std::string> names {
                Utils::String::delimitedStringToVector(nameAttr, ",")};
            for (std::string& singleName : names) {
                singleName = Utils::String::trim(singleName);
                if (!singleName.empty())
                    customViewNames.insert(singleName);
            }
        }
    }

    // Parse views.
    for (pugi::xml_node node {root.child("view")}; node; node = node.next_sibling("view")) {
        if (!node.attribute("name")) {
            LOG(LogDebug) << error.message << ": View missing \"name\" attribute, skipping";
            continue;
        }

        const std::string delim {" \t\r\n,"};
        const std::string nameAttr {node.attribute("name").as_string()};
        size_t prevOff {nameAttr.find_first_not_of(delim, 0)};
        size_t off {nameAttr.find_first_of(delim, prevOff)};
        std::string viewKey;
        while (off != std::string::npos || prevOff != std::string::npos) {
            viewKey = nameAttr.substr(prevOff, off - prevOff);
            prevOff = nameAttr.find_first_not_of(delim, off);
            off = nameAttr.find_first_of(delim, prevOff);

            if (gamelistStyles.find(viewKey) != gamelistStyles.end()) {
                const std::string selectedViewStyle {
                    !mSelectedGamelistView.empty() ? mSelectedGamelistView : mSelectedVariant};

                if (!selectedViewStyle.empty()) {
                    if (gamelistStyles.find(selectedViewStyle) != gamelistStyles.end()) {
                        if (selectedViewStyle != viewKey)
                            continue;
                    }
                    else if (!customViewInheritsStyle(selectedViewStyle, viewKey)) {
                        continue;
                    }
                }
                else {
                    if (mAutoSelectedGamelistStyle.empty())
                        mAutoSelectedGamelistStyle = viewKey;
                    if (mAutoSelectedGamelistStyle != viewKey)
                        continue;
                }

                viewKey = "gamelist";
            }
            else if (customViewNames.find(viewKey) != customViewNames.end()) {
                // Batocera PascalCase customView name (e.g. Tiles, Icons, Boxes, Content,
                // Carousel). These are gamelist variants — only apply when it matches the
                // currently selected custom view.
                const std::string selectedViewStyle {
                    !mSelectedGamelistView.empty() ? mSelectedGamelistView : mSelectedVariant};
                if (selectedViewStyle != viewKey)
                    continue;
                viewKey = "gamelist";
            }

            if (std::find(sSupportedViews.cbegin(), sSupportedViews.cend(), viewKey) !=
                sSupportedViews.cend()) {
                ThemeView& view {
                    mViews.insert(std::pair<std::string, ThemeView>(viewKey, ThemeView()))
                        .first->second};
                parseView(node, view);
            }
            else {
                LOG(LogDebug) << error.message << ": Unsupported view style \"" << viewKey
                              << "\", skipping";
            }
        }
    }
}

void ThemeData::parseCustomViews(const pugi::xml_node& root)
{
    const std::string selectedCustomView {
        !mSelectedGamelistView.empty() ? mSelectedGamelistView : mSelectedVariant};

    if (selectedCustomView.empty())
        return;

    std::map<std::string, pugi::xml_node> customViews;
    for (pugi::xml_node node {root.child("customView")}; node;
         node = node.next_sibling("customView")) {
        const std::string namesAttr {node.attribute("name").as_string()};
        if (namesAttr.empty())
            continue;
        // Batocera customView name may be comma-separated (e.g. "Tiles View,Icons").
        // Register every individual name so lookup by selected view works.
        std::vector<std::string> names {
            Utils::String::delimitedStringToVector(namesAttr, ",")};
        for (std::string& singleName : names) {
            singleName = Utils::String::trim(singleName);
            if (!singleName.empty())
                customViews[singleName] = node;
        }
    }

    if (customViews.find(selectedCustomView) == customViews.end())
        return;

    std::set<std::string> applied;
    std::map<std::string, std::string> resolvedViews;


    std::function<void(const std::string&)> applyCustomView = [&](const std::string& name) {
        if (applied.find(name) != applied.end())
            return;

        auto cvIt = customViews.find(name);
        if (cvIt == customViews.end())
            return;

        pugi::xml_node node {cvIt->second};
        const std::string inherits {node.attribute("inherits").as_string()};

        std::string targetView {"gamelist"};
        ThemeView* parentViewPtr = nullptr;

        if (!inherits.empty()) {
            auto parentIt = customViews.find(inherits);
            if (parentIt != customViews.end()) {
                applyCustomView(inherits);
                if (resolvedViews.find(inherits) != resolvedViews.end())
                    targetView = resolvedViews[inherits];
                // Inherit from parent customView
                parentViewPtr = &mViews[targetView];
            } else {
                // Inherit from a base view (e.g., gamecarousel)
                targetView = inherits;
                if (targetView == "basic" || targetView == "detailed" || targetView == "video" ||
                    targetView == "grid" || targetView == "gamecarousel") {
                    targetView = "gamelist";
                }
                parentViewPtr = &mViews[targetView];
            }
        } else {
            // No inherits, use gamelist as base
            parentViewPtr = &mViews[targetView];
        }

        // Create or get the target view
        ThemeView& view = mViews[targetView];

        // Batocera-style inheritance: copy all elements from parent view before parsing customView node
        if (parentViewPtr && parentViewPtr != &view) {
            for (const auto& elemPair : parentViewPtr->elements) {
                // Only insert if not already present (customView can override)
                if (view.elements.find(elemPair.first) == view.elements.end()) {
                    view.elements[elemPair.first] = elemPair.second;
                }
            }
        }

        // Now parse the customView node, which can override/add elements
        parseView(node, view);

        resolvedViews[name] = targetView;
        applied.insert(name);
    };

    applyCustomView(selectedCustomView);
}

void ThemeData::parseView(const pugi::xml_node& root, ThemeView& view)
{
    ThemeException error;
    error << "ThemeData::parseView(): ";
    error.setFiles(mPaths);

    struct DeferredControl {
        std::string names;
        pugi::xml_node node;
    };

    std::vector<DeferredControl> deferredControls;

    for (pugi::xml_node node {root.first_child()}; node; node = node.next_sibling()) {
        const std::string originalElementType {node.name()};

        // Batocera/Retrobat compatibility fallback: layout wrappers like stackpanel/container
        // can be nested and unnamed. We flatten and parse their children as regular view
        // elements so existing runtime components remain usable.
        if ((originalElementType == "stackpanel" || originalElementType == "container") &&
            !node.attribute("name")) {
            parseView(node, view);
            continue;
        }

        if (!node.attribute("name")) {
            LOG(LogDebug) << error.message << ": Element of type \"" << node.name()
                          << "\" missing \"name\" attribute, skipping";
            continue;
        }


        std::string resolvedElementType {originalElementType};
        std::string parserElementType {resolvedElementType};

        // Batocera compatibility: gamecarousel is equivalent to carousel.
        static bool loggedGamecarouselAlias = false;
        if (resolvedElementType == "gamecarousel") {
            if (!loggedGamecarouselAlias) {
                LOG(LogWarning) << error.message << ": Alias Batocera <gamecarousel> detectado, mapeando a <carousel> (solo se muestra una vez por sesión)";
                loggedGamecarouselAlias = true;
            }
            resolvedElementType = "carousel";
            parserElementType = "carousel";
        } else if (resolvedElementType == "carousel") {
            // No log, es el estándar
        } else if (resolvedElementType == "networkIcon" || resolvedElementType == "batteryIcon") {
            LOG(LogWarning) << error.message << ": Alias Batocera <" << resolvedElementType << "> detectado, mapeando a <image>";
            resolvedElementType = "image";
            parserElementType = "image";
        } else if (resolvedElementType == "webimage" || resolvedElementType == "webImage") {
            LOG(LogWarning) << error.message << ": Alias Batocera <" << resolvedElementType << "> detectado, mapeando a <image> (webimage)";
            resolvedElementType = "webimage";
            parserElementType = "image";
        } else if (resolvedElementType == "batteryText") {
            LOG(LogWarning) << error.message << ": Alias Batocera <batteryText> detectado, mapeando a <text>";
            resolvedElementType = "text";
            parserElementType = "text";
        }

        auto elemTypeIt = sElementMap.find(parserElementType);
        if (elemTypeIt == sElementMap.cend()) {
            LOG(LogDebug) << error.message << ": Unknown element type \"" << node.name()
                          << "\", skipping";
            continue;
        }

        const std::string delim {" \t\r\n,"};
        const std::string nameAttr {node.attribute("name").as_string()};

        if (resolvedElementType == "control") {
            deferredControls.emplace_back(DeferredControl {nameAttr, node});
            continue;
        }

        size_t prevOff {nameAttr.find_first_not_of(delim, 0)};
        size_t off {nameAttr.find_first_of(delim, prevOff)};
        while (off != std::string::npos || prevOff != std::string::npos) {
            std::string elemName {nameAttr.substr(prevOff, off - prevOff)};
            prevOff = nameAttr.find_first_not_of(delim, off);
            off = nameAttr.find_first_of(delim, prevOff);

            // Add the element type as a prefix to avoid name collisions between different
            // component types.
            std::string elemKey {resolvedElementType + "_" + elemName};

            auto insertIt = view.elements.insert(
                std::pair<std::string, ThemeElement>(elemKey, ThemeElement()));
            parseElement(node, elemTypeIt->second, insertIt.first->second);

            // Normalize stored type so runtime dispatch in views can use existing branches.
            // This is especially important for Batocera derived tags mapped to image/text.
            insertIt.first->second.type = resolvedElementType;

            // Batocera rectangle fallback: map fill color to text background color so existing
            // runtime can display simple solid rectangles without introducing a new component.
            if (originalElementType == "rectangle") {
                ThemeElement& parsedElem {insertIt.first->second};
                if (parsedElem.has("color") && !parsedElem.has("backgroundColor"))
                    parsedElem.properties["backgroundColor"] =
                        parsedElem.get<unsigned int>("color");
            }

            // Batocera compatibility: inject 'metadata' property for elements named with
            // the 'md_' prefix when no explicit metadata property was already defined.
            ThemeElement& parsedElem {insertIt.first->second};
            if (!parsedElem.has("metadata")) {
                static const std::map<std::string, std::string> mdNameToMetadata {
                    {"md_description", "description"},
                    {"md_name", "name"},
                    {"md_developer", "developer"},
                    {"md_publisher", "publisher"},
                    {"md_genre", "genre"},
                    {"md_players", "players"},
                    {"md_playcount", "playcount"},
                    {"md_gametime", "playtime"},
                    {"md_releasedate", "releasedate"},
                    {"md_lastplayed", "lastplayed"},
                    {"md_favorite", "favorite"},
                    {"md_completed", "completed"},
                    {"md_kidgame", "kidgame"},
                    {"md_broken", "broken"},
                    {"md_manual", "manual"},
                };
                // Strip trailing comma-separated part if element name was composed (e.g.
                // "md_developer, md_publisher"). We use the first token already split above.
                auto mdIt = mdNameToMetadata.find(elemName);
                if (mdIt != mdNameToMetadata.cend() &&
                    (parsedElem.type == "text" || parsedElem.type == "datetime")) {
                    parsedElem.properties["metadata"] = mdIt->second;
                }
            }
        }
    }

    // Batocera compatibility: <control> does not create a new component, it overrides
    // properties/storyboards of existing elements listed by name.
    for (const DeferredControl& controlDef : deferredControls) {
        ThemeElement controlElement;
        parseElement(controlDef.node, sElementMap.at("control"), controlElement);

        // Batocera compatibility: <control .../> often uses inline attributes instead of
        // child nodes. Parse these and merge into the same override payload.
        float inlineX {-1.0f};
        float inlineY {-1.0f};
        float inlineW {-1.0f};
        float inlineH {-1.0f};
        bool hasInlinePosLegacy {false};
        bool hasInlineSizeLegacy {false};

        for (pugi::xml_attribute attr {controlDef.node.first_attribute()}; attr;
             attr = attr.next_attribute()) {
            const std::string attrName {attr.name()};
            if (attrName.empty() || attrName == "name")
                continue;

            auto controlTypeIt = sElementMap.at("control").find(attrName);
            if (controlTypeIt == sElementMap.at("control").cend())
                continue;

            const std::string rawValue {
                resolveDynamicBindings(resolvePlaceholders(attr.as_string()))};
            if (rawValue.empty())
                continue;

            switch (controlTypeIt->second) {
                case NORMALIZED_RECT: {
                    glm::vec4 val {0.0f, 0.0f, 0.0f, 0.0f};
                    auto splits = Utils::String::delimitedStringToVector(rawValue, " ");
                    if (splits.size() == 1) {
                        const float scalar {static_cast<float>(atof(splits.at(0).c_str()))};
                        val = glm::vec4 {scalar, scalar, scalar, scalar};
                    }
                    else if (splits.size() == 2) {
                        val = glm::vec4 {static_cast<float>(atof(splits.at(0).c_str())),
                                         static_cast<float>(atof(splits.at(1).c_str())),
                                         static_cast<float>(atof(splits.at(0).c_str())),
                                         static_cast<float>(atof(splits.at(1).c_str()))};
                    }
                    else if (splits.size() == 4) {
                        val = glm::vec4 {static_cast<float>(atof(splits.at(0).c_str())),
                                         static_cast<float>(atof(splits.at(1).c_str())),
                                         static_cast<float>(atof(splits.at(2).c_str())),
                                         static_cast<float>(atof(splits.at(3).c_str()))};
                    }
                    controlElement.properties[attrName] = val;
                    break;
                }
                case NORMALIZED_PAIR: {
                    size_t divider = rawValue.find(' ');
                    if (divider == std::string::npos) {
                        const float scalar {static_cast<float>(atof(rawValue.c_str()))};
                        controlElement.properties[attrName] = glm::vec2 {scalar, scalar};
                    }
                    else {
                        const std::string first {rawValue.substr(0, divider)};
                        const std::string second {rawValue.substr(divider, std::string::npos)};
                        controlElement.properties[attrName] =
                            glm::vec2 {static_cast<float>(atof(first.c_str())),
                                       static_cast<float>(atof(second.c_str()))};
                    }
                    break;
                }
                case STRING: {
                    controlElement.properties[attrName] = rawValue;
                    break;
                }
                case FLOAT: {
                    const float floatValue {static_cast<float>(atof(rawValue.c_str()))};
                    controlElement.properties[attrName] = floatValue;

                    if (attrName == "x") {
                        inlineX = floatValue;
                        hasInlinePosLegacy = true;
                    }
                    else if (attrName == "y") {
                        inlineY = floatValue;
                        hasInlinePosLegacy = true;
                    }
                    else if (attrName == "w") {
                        inlineW = floatValue;
                        hasInlineSizeLegacy = true;
                    }
                    else if (attrName == "h") {
                        inlineH = floatValue;
                        hasInlineSizeLegacy = true;
                    }
                    break;
                }
                case BOOLEAN: {
                    const char c {(rawValue.empty() ? '\0' : rawValue.front())};
                    const bool boolValue {
                        c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y'};
                    controlElement.properties[attrName] = boolValue;
                    break;
                }
                case PATH:
                case COLOR:
                case UNSIGNED_INTEGER:
                default:
                    // No current control-inline use case for these types.
                    break;
            }
        }

        if (!controlElement.has("pos") && hasInlinePosLegacy && inlineX >= 0.0f && inlineY >= 0.0f)
            controlElement.properties["pos"] = glm::vec2 {inlineX, inlineY};

        if (!controlElement.has("size") && hasInlineSizeLegacy && inlineW >= 0.0f &&
            inlineH >= 0.0f) {
            controlElement.properties["size"] = glm::vec2 {inlineW, inlineH};
        }

        const std::string delim {" \t\r\n,"};
        size_t prevOff {controlDef.names.find_first_not_of(delim, 0)};
        size_t off {controlDef.names.find_first_of(delim, prevOff)};

        while (off != std::string::npos || prevOff != std::string::npos) {
            const std::string targetName {controlDef.names.substr(prevOff, off - prevOff)};
            prevOff = controlDef.names.find_first_not_of(delim, off);
            off = controlDef.names.find_first_of(delim, prevOff);

            if (targetName.empty())
                continue;

            std::vector<ThemeElement*> matchedElements;

            auto directIt = view.elements.find(targetName);
            if (directIt != view.elements.end())
                matchedElements.emplace_back(&directIt->second);

            const std::string suffixedName {"_" + targetName};
            for (auto& viewElement : view.elements) {
                if (viewElement.first.size() > suffixedName.size() &&
                    viewElement.first.compare(viewElement.first.size() - suffixedName.size(),
                                              suffixedName.size(), suffixedName) == 0) {
                    matchedElements.emplace_back(&viewElement.second);
                }
            }

            // Remove duplicates when direct and suffixed lookup resolve to the same element.
            std::sort(matchedElements.begin(), matchedElements.end());
            matchedElements.erase(std::unique(matchedElements.begin(), matchedElements.end()),
                                  matchedElements.end());

            if (matchedElements.empty()) {
                LOG(LogDebug) << error.message << ": <control> target \"" << targetName
                              << "\" not found in current view, skipping";
                continue;
            }

            for (ThemeElement* targetElement : matchedElements) {
                for (const auto& property : controlElement.properties)
                    targetElement->properties[property.first] = property.second;

                for (const auto& storyboard : controlElement.mStoryBoards)
                    targetElement->mStoryBoards[storyboard.first] = storyboard.second;
            }
        }
    }
}

void ThemeData::parseElement(const pugi::xml_node& root,
                             const std::map<std::string, ElementPropertyType>& typeMap,
                             ThemeElement& element)
{
    ThemeException error;
    error << "ThemeData::parseElement(): ";
    error.setFiles(mPaths);
    element.type = root.name();
    if (element.type == "gamecarousel") {
        LOG(LogWarning) << error.message << ": Alias Batocera <gamecarousel> detectado en elemento, mapeando a <carousel>";
        element.type = "carousel";
    }

    // Ignore optional extra attribute used by some batocera themes.

    const auto includeTranslation {translateBatoceraTag("include")};

    // Check if attribute (Batocera compatibility)
    // Supported expressions:
    // - true/false/1/0
    // - ${screen.width|screen.height|screen.ratio} <op> number
    // - ${language|theme.language|subset.<name>} ==|!= value
    const std::string ifAttribute {
        getTranslatedAttributeValue(root, includeTranslation, "if")};
    if (!ifAttribute.empty() && !evaluateIfExpression(ifAttribute)) {
        LOG(LogDebug) << error.message << ": Element \"" << element.type
                      << "\" skipped due to if condition (\"" << ifAttribute << "\")";
        return;
    }

    // Check tinyScreen attribute (Batocera compatibility)
    // Uses a conservative heuristic where tiny screen means min(width, height) <= 480.
    const std::string tinyScreenAttribute {
        Utils::String::toLower(
            getTranslatedAttributeValue(root, includeTranslation, "tinyScreen"))};
    if (!tinyScreenAttribute.empty()) {
        bool requiredTinyScreen {true};
        if (tinyScreenAttribute == "0" || tinyScreenAttribute == "false" ||
            tinyScreenAttribute == "no" || tinyScreenAttribute == "off") {
            requiredTinyScreen = false;
        }

        const float screenWidth {Renderer::getScreenWidth()};
        const float screenHeight {Renderer::getScreenHeight()};
        const float shortestSide {std::min(screenWidth, screenHeight)};
        const bool isTinyScreen {shortestSide <= 480.0f};

        if (isTinyScreen != requiredTinyScreen) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to tinyScreen condition (required: "
                          << (requiredTinyScreen ? "true" : "false") << ", current: "
                          << (isTinyScreen ? "true" : "false") << ")";
            return;
        }
    }

    // Check verticalScreen attribute (Batocera compatibility)
    const std::string verticalScreenAttribute {
        Utils::String::toLower(
            getTranslatedAttributeValue(root, includeTranslation, "verticalScreen"))};
    if (!verticalScreenAttribute.empty()) {
        const bool requiredVerticalScreen {parseBooleanAttribute(verticalScreenAttribute)};
        const bool isVerticalScreen {Renderer::getScreenHeight() > Renderer::getScreenWidth()};

        if (isVerticalScreen != requiredVerticalScreen) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to verticalScreen condition (required: "
                          << (requiredVerticalScreen ? "true" : "false") << ", current: "
                          << (isVerticalScreen ? "true" : "false") << ")";
            return;
        }
    }

    // Check ifArch attribute (Batocera compatibility)
    // Supports comma-separated values, e.g. ifArch="x86_64,aarch64".
    const std::string ifArchAttribute {
        getTranslatedAttributeValue(root, includeTranslation, "ifArch")};
    if (!ifArchAttribute.empty()) {
        std::string currentArch;
#if defined(__x86_64__) || defined(_M_X64)
        currentArch = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
        currentArch = "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
        currentArch = "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
        currentArch = "arm";
#elif defined(__riscv)
        currentArch = "riscv";
#else
        currentArch = "unknown";
#endif

        auto requestedArches {
            Utils::String::delimitedStringToVector(Utils::String::toLower(ifArchAttribute), ",")};
        bool archMatch {false};

        for (auto& requested : requestedArches) {
            requested = Utils::String::trim(requested);
            if (requested.empty())
                continue;

            // Alias normalization for common batocera values.
            if (requested == "x64" || requested == "amd64")
                requested = "x86_64";
            else if (requested == "x86" || requested == "i386" || requested == "i686")
                requested = "x86";
            else if (requested == "arm64")
                requested = "aarch64";
            else if (requested == "armhf" || requested == "armv7" || requested == "armv7l")
                requested = "arm";

            if (requested == currentArch) {
                archMatch = true;
                break;
            }
        }

        if (!archMatch) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to ifArch condition (current: \""
                          << currentArch << "\", required: \"" << ifArchAttribute << "\")";
            return;
        }
    }

    // Check ifNotArch attribute (Batocera compatibility)
    const std::string ifNotArchAttribute {
        getTranslatedAttributeValue(root, includeTranslation, "ifNotArch")};
    if (!ifNotArchAttribute.empty()) {
        const std::string currentArch {getCurrentArchCompat()};

        if (matchesAnyConditionToken(ifNotArchAttribute, {currentArch})) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to ifNotArch condition (current: \""
                          << currentArch << "\", disallowed: \"" << ifNotArchAttribute
                          << "\")";
            return;
        }
    }

    // Check ifHelpPrompts attribute (Batocera compatibility)
    // If present, element is only included when ShowHelpPrompts matches.
    const std::string ifHelpPromptsAttribute {
        Utils::String::toLower(
            getTranslatedAttributeValue(root, includeTranslation, "ifHelpPrompts"))};
    if (!ifHelpPromptsAttribute.empty()) {
        bool requiredHelpPrompts {true};
        if (ifHelpPromptsAttribute == "0" || ifHelpPromptsAttribute == "false" ||
            ifHelpPromptsAttribute == "no" || ifHelpPromptsAttribute == "off") {
            requiredHelpPrompts = false;
        }

        const bool showHelpPrompts {Settings::getInstance()->getBool("ShowHelpPrompts")};
        if (showHelpPrompts != requiredHelpPrompts) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to ifHelpPrompts condition (required: "
                          << (requiredHelpPrompts ? "true" : "false") << ")";
            return;
        }
    }

    // Check ifCheevos attribute (Batocera compatibility)
    const std::string ifCheevosAttribute {
        Utils::String::toLower(
            getTranslatedAttributeValue(root, includeTranslation, "ifCheevos"))};
    if (!ifCheevosAttribute.empty()) {
        const bool requiredCheevos {parseBooleanAttribute(ifCheevosAttribute)};
        const bool cheevosEnabled {Settings::getInstance()->getBool("global.retroachievements")};

        if (cheevosEnabled != requiredCheevos) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to ifCheevos condition (required: "
                          << (requiredCheevos ? "true" : "false") << ")";
            return;
        }
    }

    // Check ifNetplay attribute (Batocera compatibility)
    const std::string ifNetplayAttribute {
        Utils::String::toLower(
            getTranslatedAttributeValue(root, includeTranslation, "ifNetplay"))};
    if (!ifNetplayAttribute.empty()) {
        const bool requiredNetplay {parseBooleanAttribute(ifNetplayAttribute)};
        const bool netplayEnabled {Settings::getInstance()->getBool("global.netplay")};

        if (netplayEnabled != requiredNetplay) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to ifNetplay condition (required: "
                          << (requiredNetplay ? "true" : "false") << ")";
            return;
        }
    }

    // Check language attribute (Batocera compatibility)
    // If lang attribute is present and doesn't match current theme language, skip this element
    std::string langAttribute {
        getTranslatedAttributeValue(root, includeTranslation, "lang", {"locale"})};

    if (!langAttribute.empty()) {
        const bool langMatch {
            matchesAnyConditionToken(langAttribute, buildLanguageCandidates(sThemeLanguage))};
        if (!langMatch) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to lang attribute mismatch (expected: \""
                          << sThemeLanguage << "\", got: \"" << langAttribute << "\")";
            return;
        }
    }

    // Check region attribute (Batocera compatibility)
    const std::string regionAttribute {
        getTranslatedAttributeValue(root, includeTranslation, "region")};
    if (!regionAttribute.empty()) {
        const bool regionMatch {matchesAnyConditionToken(
            regionAttribute,
            buildRegionCandidates(Settings::getInstance()->getString("ThemeRegionName"),
                                  sThemeLanguage))};

        if (!regionMatch) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to region attribute mismatch (got: \""
                          << regionAttribute << "\")";
            return;
        }
    }

    // Check ifSubset attribute (Batocera compatibility)
    // Supported formats include:
    // - ifSubset="subsetName:option"
    // - ifSubset="subsetName=option"
    // - ifSubset="subsetName:optA|optB, otherSubset=optC"
    const std::string ifSubsetAttribute {
        getTranslatedAttributeValue(root, includeTranslation, "ifSubset")};
    if (!ifSubsetAttribute.empty()) {
        if (!evaluateIfSubsetCondition(ifSubsetAttribute)) {
            LOG(LogDebug) << error.message << ": Element \"" << element.type
                          << "\" skipped due to ifSubset condition (\"" << ifSubsetAttribute
                          << "\")";
            return;
        }
    }

    // Normalize Batocera v29+ format (x, y, w, h) to ESTACION-PRO format (pos, size)
    normalizeElementFormat(root, element);

    for (pugi::xml_node node {root.first_child()}; node; node = node.next_sibling()) {
        if (strcmp(node.name(), "storyboard") == 0) {
            if (Settings::getInstance()->getBool("ThemeAnimations")) {
                auto storyboard = std::make_shared<ThemeStoryboard>();
                if (storyboard->fromXmlNode(
                        node, typeMap, mPaths.back(),
                        [this](const std::string& value) {
                            return resolveDynamicBindings(resolvePlaceholders(value));
                        })) {
                    std::string sbName {storyboard->eventName};
                    element.mStoryBoards[sbName] = storyboard;
                }
            }

            continue;
        }

        std::string propertyName {node.name()};

        // Batocera compatibility aliases.
        if (propertyName == "alignment") {
            if (element.type == "text" || element.type == "textlist" || element.type == "datetime" ||
                element.type == "clock" || element.type == "gamelistinfo")
                propertyName = "horizontalAlignment";
        }
        else if (propertyName == "textColor") {
            if (element.type == "text" || element.type == "gamelistinfo")
                propertyName = "color";
        }
        else if (element.type == "image" && propertyName == "flipX") {
            propertyName = "flipHorizontal";
        }
        else if (element.type == "image" && propertyName == "flipY") {
            propertyName = "flipVertical";
        }
        else if (element.type == "imagegrid" && propertyName == "gameImage") {
            propertyName = "defaultImage";
        }
        else if (element.type == "imagegrid" && propertyName == "folderImage") {
            propertyName = "defaultFolderImage";
        }

        if (element.type == "menuTextEdit" && (propertyName == "inactive" || propertyName == "active")) {
            const std::string rawValue {Utils::String::trim(node.text().as_string())};
            const std::string lowerValue {Utils::String::toLower(rawValue)};
            const bool looksLikePath {
                rawValue.find('/') != std::string::npos ||
                rawValue.find('\\') != std::string::npos ||
                Utils::String::startsWith(rawValue, "./") ||
                Utils::String::startsWith(rawValue, "../") ||
                Utils::String::startsWith(rawValue, ":/") ||
                Utils::String::endsWith(lowerValue, ".png") ||
                Utils::String::endsWith(lowerValue, ".jpg") ||
                Utils::String::endsWith(lowerValue, ".jpeg") ||
                Utils::String::endsWith(lowerValue, ".svg") ||
                Utils::String::endsWith(lowerValue, ".webp")};

            if (looksLikePath)
                propertyName += "Path";
        }

        auto typeIt = typeMap.find(propertyName);
        if (typeIt == typeMap.cend()) {
            LOG(LogDebug) << error.message << ": Unknown property type \"" << node.name()
                          << "\" for element of type \"" << root.name()
                          << "\", skipping";
            continue;
        }

        std::string str {resolveDynamicBindings(resolvePlaceholders(node.text().as_string()))};

        // Handle the special case with mutually exclusive system variables, for example
        // system.fullName.autoCollections and system.fullName.noCollections which can never
        // exist at the same time. A backspace is assigned in SystemData to flag the
        // variables that do not apply and if it's encountered here we simply skip the
        // property.
        if (str == "\b")
            continue;

        // Strictly enforce that there are no blank values in the theme configuration.
        if (str == "") {
            LOG(LogDebug) << error.message << ": Property \"" << typeIt->first
                          << "\" for element \"" << element.type
                          << "\" has no value defined, skipping";
            continue;
        }

        std::string nodeName {propertyName};

        // Convert value aliases after placeholder resolution.
        if (element.type == "image" && propertyName == "linearSmooth") {
            const char c {(str.empty() ? '\0' : str.front())};
            const bool linear {c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y'};
            str = linear ? "linear" : "nearest";
            nodeName = "interpolation";
            typeIt = typeMap.find(nodeName);
            if (typeIt == typeMap.cend())
                continue;
        }
        else if (propertyName == "forceUppercase") {
            const char c {(str.empty() ? '\0' : str.front())};
            const bool forceUppercase {c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y'};
            str = forceUppercase ? "uppercase" : "none";
            nodeName = "letterCase";
            typeIt = typeMap.find(nodeName);
            if (typeIt == typeMap.cend())
                continue;
        }
        else if (element.type == "video" && propertyName == "roundCorners") {
            nodeName = "videoCornerRadius";
            typeIt = typeMap.find(nodeName);
            if (typeIt == typeMap.cend())
                continue;
        }

        // If an attribute exists, then replace nodeName with its name.
        auto attributeEntry = sPropertyAttributeMap.find(element.type);
        if (attributeEntry != sPropertyAttributeMap.end()) {
            auto attribute = attributeEntry->second.find(typeIt->first);
            if (attribute != attributeEntry->second.end()) {
                if (node.attribute(attribute->second.c_str()) == nullptr) {
                    LOG(LogDebug) << error.message << ": Missing expected attribute \""
                                  << attribute->second << "\" for property \""
                                  << typeIt->first << "\" (element \""
                                  << attributeEntry->first << "\"), skipping";
                    continue;
                }
                else {
                    // Add the attribute name as a prefix to avoid potential name collisions.
                    nodeName = attribute->second + "_" +
                               node.attribute(attribute->second.c_str()).as_string("");
                }
            }
        }

        switch (typeIt->second) {
            case NORMALIZED_RECT: {
                glm::vec4 val {0.0f, 0.0f, 0.0f, 0.0f};

                auto splits = Utils::String::delimitedStringToVector(str, " ");
                if (splits.size() == 1) {
                    const float scalar {static_cast<float>(atof(splits.at(0).c_str()))};
                    val = glm::vec4 {scalar, scalar, scalar, scalar};
                }
                else if (splits.size() == 2) {
                    val = glm::vec4 {static_cast<float>(atof(splits.at(0).c_str())),
                                     static_cast<float>(atof(splits.at(1).c_str())),
                                     static_cast<float>(atof(splits.at(0).c_str())),
                                     static_cast<float>(atof(splits.at(1).c_str()))};
                }
                else if (splits.size() == 4) {
                    val = glm::vec4 {static_cast<float>(atof(splits.at(0).c_str())),
                                     static_cast<float>(atof(splits.at(1).c_str())),
                                     static_cast<float>(atof(splits.at(2).c_str())),
                                     static_cast<float>(atof(splits.at(3).c_str()))};
                }

                element.properties[nodeName] = val;
                break;
            }
            case NORMALIZED_PAIR: {
                size_t divider = str.find(' ');
                if (divider == std::string::npos) {
                    // Batocera themes sometimes provide a single scalar for pair values.
                    const float scalar {static_cast<float>(atof(str.c_str()))};
                    element.properties[nodeName] = glm::vec2 {scalar, scalar};
                    break;
                }

                std::string first {str.substr(0, divider)};
                std::string second {str.substr(divider, std::string::npos)};

                glm::vec2 val {static_cast<float>(atof(first.c_str())),
                               static_cast<float>(atof(second.c_str()))};

                element.properties[nodeName] = val;
                break;
            }
            case STRING: {
                element.properties[nodeName] = str;
                break;
            }
            case PATH: {
                std::string path;

                if (!str.empty() && str.front() == ':')
                    path = ResourceManager::getInstance().getResourcePath(str);
                else
                    path = resolveThemeAssetPathCompat(
                        str, mPaths.back(), (mPaths.empty() ? std::string {} : mPaths.front()));

                const bool fileExists {ResourceManager::getInstance().fileExists(path)};
                const std::string rawPathText {node.text().as_string()};
                const std::string rawPathTextLower {Utils::String::toLower(rawPathText)};
                const bool isKnownOptionalArcadePowerAsset {
                    Utils::String::endsWith(rawPathTextLower, "/centerfade_v.png") ||
                    Utils::String::endsWith(rawPathTextLower, "/favorito.png") ||
                    Utils::String::endsWith(rawPathTextLower, "/marq_default.png")};
                const bool suppressMissingLog {
                    rawPathText.find("${system.theme}") != std::string::npos ||
                    rawPathText.find("${system.name}") != std::string::npos ||
                    isKnownOptionalArcadePowerAsset};

                if (!fileExists) {
                    std::stringstream ss;
                    // For explicit paths, print a warning if the file couldn't be found, but
                    // only print a debug message if it was set using a variable.
                    if (str == rawPathText && !suppressMissingLog) {
#if defined(_WIN64)
                        LOG(LogWarning) << Utils::String::replace(error.message, "/", "\\")
                                        << ": Couldn't find file \"" << node.text().get() << "\" "
                                        << ((node.text().get() != path) ?
                                                "which resolves to \"" +
                                                    Utils::String::replace(path, "/", "\\") + "\"" :
#else
                        LOG(LogWarning)
                            << error.message << ": Couldn't find file \"" << node.text().get()
                            << "\" "
                            << ((node.text().get() != path) ? "which resolves to \"" + path + "\"" :
#endif
                                                "")
                                        << " (element type \"" << element.type << "\", name \""
                                        << root.attribute("name").as_string() << "\", property \""
                                        << nodeName << "\")";
                    }
                    else if (!suppressMissingLog &&
                             !(Settings::getInstance()->getBool("DebugSkipMissingThemeFiles") ||
                               (mCustomCollection &&
                                Settings::getInstance()->getBool(
                                    "DebugSkipMissingThemeFilesCustomCollections")))) {
#if defined(_WIN64)
                        LOG(LogDebug) << Utils::String::replace(error.message, "/", "\\")
                                      << ": Couldn't find file \"" << node.text().get() << "\" "
                                      << ((node.text().get() != path) ?
                                              "which resolves to \"" +
                                                  Utils::String::replace(path, "/", "\\") + "\"" :
#else
                        LOG(LogDebug)
                            << error.message << ": Couldn't find file \"" << node.text().get()
                            << "\" "
                            << ((node.text().get() != path) ? "which resolves to \"" + path + "\"" :
#endif
                                              "")
                                      << " (element type \"" << element.type << "\", name \""
                                      << root.attribute("name").as_string() << "\", property \""
                                      << nodeName << "\")";
                    }
                }
                // Keep the latest valid path if multiple entries are declared (themes often
                // define default first and system-specific override later).
                // If this entry doesn't resolve to a file, leave the property unchanged.
                if (fileExists)
                    element.properties[nodeName] = path;
                break;
            }
            case COLOR: {
                try {
                    element.properties[nodeName] = getHexColor(str);
                }
                catch (ThemeException& e) {
                    throw error << ": " << e.what();
                }
                break;
            }
            case UNSIGNED_INTEGER: {
                unsigned int integerVal {static_cast<unsigned int>(strtoul(str.c_str(), 0, 0))};
                element.properties[nodeName] = integerVal;
                break;
            }
            case FLOAT: {
                float floatVal {static_cast<float>(strtod(str.c_str(), 0))};
                element.properties[nodeName] = floatVal;
                break;
            }
            case BOOLEAN: {
                bool boolVal = false;
                // Only look at the first character.
                if (str.size() > 0) {
                    if (str.front() == '1' || str.front() == 't' || str.front() == 'T' ||
                        str.front() == 'y' || str.front() == 'Y')
                        boolVal = true;
                }

                element.properties[nodeName] = boolVal;
                break;
            }
            default: {
                throw error << ": Unknown ElementPropertyType for \""
                            << root.attribute("name").as_string() << "\", property " << node.name();
            }
        }
    }
}

void ThemeData::parseFeatures(const pugi::xml_node& root)
{
    for (pugi::xml_node node {root.child("feature")}; node; node = node.next_sibling("feature")) {
        parseTransitions(node);
        parseVariables(node);
        parseColorSchemes(node);
        parseFontSizes(node);
        parseLanguages(node);
        parseIncludes(node);
        parseViews(node);
        parseVariants(node);
        parseAspectRatios(node);
    }
}

#if defined(GETTEXT_DUMMY_ENTRIES)
void ThemeData::gettextMessageCatalogEntries()
{
    // sSupportedFontSizes
    _("medium");
    _("large");
    _("small");
    _("extra small");
    _("extra large");

    // sSupportedAspectRatios
    _("automatic");
    _("16:9 vertical");
    _("16:10 vertical");
    _("3:2 vertical");
    _("4:3 vertical");
    _("5:3 vertical");
    _("5:4 vertical");
    _("8:7 vertical");
    _("19.5:9 vertical");
    _("20:9 vertical");
    _("21:9 vertical");
    _("32:9 vertical");
}
#endif
