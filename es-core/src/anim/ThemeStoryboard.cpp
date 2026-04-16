// SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ThemeStoryboard.cpp
//
//  Parser XML y copia profunda de storyboards.
//

#include "anim/ThemeStoryboard.h"

#include "Log.h"
#include "resources/ResourceManager.h"
#include "utils/FileSystemUtil.h"
#include "utils/LocalizationUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

static unsigned int parseHexColor(const std::string& str)
{
    if (str.size() == 6)
        return static_cast<unsigned int>((strtoul(str.c_str(), nullptr, 16) << 8) | 0xFF);

    return static_cast<unsigned int>(strtoul(str.c_str(), nullptr, 16));
}

ThemeStoryboard::ThemeStoryboard(const ThemeStoryboard& src)
{
    eventName = src.eventName;
    repeat = src.repeat;
    repeatAt = src.repeatAt;

    for (auto anim : src.animations) {
        if (dynamic_cast<ThemeFloatAnimation*>(anim) != nullptr)
            animations.push_back(new ThemeFloatAnimation(
                static_cast<const ThemeFloatAnimation&>(*anim)));
        else if (dynamic_cast<ThemeColorAnimation*>(anim) != nullptr)
            animations.push_back(new ThemeColorAnimation(
                static_cast<const ThemeColorAnimation&>(*anim)));
        else if (dynamic_cast<ThemeVector2Animation*>(anim) != nullptr)
            animations.push_back(new ThemeVector2Animation(
                static_cast<const ThemeVector2Animation&>(*anim)));
        else if (dynamic_cast<ThemeVector4Animation*>(anim) != nullptr)
            animations.push_back(new ThemeVector4Animation(
                static_cast<const ThemeVector4Animation&>(*anim)));
        else if (dynamic_cast<ThemeStringAnimation*>(anim) != nullptr)
            animations.push_back(new ThemeStringAnimation(
                static_cast<const ThemeStringAnimation&>(*anim)));
        else if (dynamic_cast<ThemePathAnimation*>(anim) != nullptr)
            animations.push_back(new ThemePathAnimation(
                static_cast<const ThemePathAnimation&>(*anim)));
        else if (dynamic_cast<ThemeBoolAnimation*>(anim) != nullptr)
            animations.push_back(new ThemeBoolAnimation(
                static_cast<const ThemeBoolAnimation&>(*anim)));
        else if (dynamic_cast<ThemeSoundAnimation*>(anim) != nullptr)
            animations.push_back(new ThemeSoundAnimation(
                static_cast<const ThemeSoundAnimation&>(*anim)));
    }
}

ThemeStoryboard::~ThemeStoryboard()
{
    for (auto anim : animations)
        delete anim;

    animations.clear();
}

static glm::vec2 parseNormalizedPair(const std::string& str)
{
    size_t divider {str.find(' ')};
    if (divider == std::string::npos)
        return glm::vec2 {0.0f, 0.0f};

    std::string first {str.substr(0, divider)};
    std::string second {str.substr(divider, std::string::npos)};

    return glm::vec2 {static_cast<float>(atof(first.c_str())),
                      static_cast<float>(atof(second.c_str()))};
}

static glm::vec4 parseNormalizedRect(const std::string& str)
{
    glm::vec4 val {0.0f, 0.0f, 0.0f, 0.0f};
    auto splits {Utils::String::delimitedStringToVector(str, " ")};

    if (splits.size() == 2) {
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

    return val;
}

static bool parseBool(const std::string& str)
{
    std::string v {Utils::String::toLower(str)};
    return v == "1" || v == "true" || v == "yes" || v == "y";
}

static bool tryParseBoolLiteral(const std::string& str, bool& outValue)
{
    const std::string v {Utils::String::toLower(Utils::String::trim(str))};
    if (v == "1" || v == "true" || v == "yes" || v == "y" || v == "on") {
        outValue = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "no" || v == "n" || v == "off") {
        outValue = false;
        return true;
    }
    return false;
}

static bool tryParseNumberLiteral(const std::string& str, double& outValue)
{
    const std::string trimmed {Utils::String::trim(str)};
    if (trimmed.empty())
        return false;

    char* endPtr {nullptr};
    const double value {std::strtod(trimmed.c_str(), &endPtr)};
    if (endPtr == trimmed.c_str())
        return false;

    const std::string rest {Utils::String::trim(std::string(endPtr))};
    if (!rest.empty())
    {
        const size_t slashPos {trimmed.find('/')};
        if (slashPos == std::string::npos || slashPos == 0 || slashPos == trimmed.size() - 1)
            return false;
        if (trimmed.find('/', slashPos + 1) != std::string::npos)
            return false;

        try {
            const std::string numeratorText {Utils::String::trim(trimmed.substr(0, slashPos))};
            const std::string denominatorText {
                Utils::String::trim(trimmed.substr(slashPos + 1))};
            char* numEndPtr {nullptr};
            char* denEndPtr {nullptr};
            const double numerator {std::strtod(numeratorText.c_str(), &numEndPtr)};
            const double denominator {std::strtod(denominatorText.c_str(), &denEndPtr)};

            if (numEndPtr == numeratorText.c_str() || denEndPtr == denominatorText.c_str() ||
                !Utils::String::trim(std::string(numEndPtr)).empty() ||
                !Utils::String::trim(std::string(denEndPtr)).empty() ||
                denominator == 0.0) {
                return false;
            }

            outValue = numerator / denominator;
            return true;
        }
        catch (...) {
            return false;
        }
    }

    outValue = value;
    return true;
}

static std::string stripQuotes(const std::string& str)
{
    const std::string trimmed {Utils::String::trim(str)};
    if (trimmed.size() >= 2 &&
        ((trimmed.front() == '"' && trimmed.back() == '"') ||
         (trimmed.front() == '\'' && trimmed.back() == '\''))) {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

static size_t findTopLevelOperator(const std::string& expr, const std::string& op)
{
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
}

static std::vector<std::string> splitTopLevelCsv(const std::string& input)
{
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
}

static bool tryEvaluateEnabledExpression(const std::string& expression, bool& outValue)
{
    struct EvalResult {
        bool known {false};
        bool value {false};
    };

    std::function<EvalResult(const std::string&)> eval;
    eval = [&eval](const std::string& rawExpr) -> EvalResult {
        std::string expr {Utils::String::trim(rawExpr)};
        if (expr.empty())
            return {true, false};

        // Unresolved dynamic tokens are not handled at this layer.
        if (expr.find('{') != std::string::npos || expr.find('}') != std::string::npos)
            return {false, false};

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

        if (!expr.empty() && expr.front() == '!') {
            const EvalResult inner {eval(expr.substr(1))};
            if (!inner.known)
                return inner;
            return {true, !inner.value};
        }

        auto findTopLevelTernary = [](const std::string& input,
                                      size_t& outQuestion,
                                      size_t& outColon) -> bool {
            outQuestion = std::string::npos;
            outColon = std::string::npos;

            int parenDepth {0};
            int ternaryDepth {0};
            bool inSingleQuote {false};
            bool inDoubleQuote {false};

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

                if (parenDepth != 0)
                    continue;

                if (c == '?') {
                    if (outQuestion == std::string::npos)
                        outQuestion = i;
                    ++ternaryDepth;
                }
                else if (c == ':' && ternaryDepth > 0) {
                    if (ternaryDepth == 1 && outQuestion != std::string::npos) {
                        outColon = i;
                        return true;
                    }
                    --ternaryDepth;
                }
            }

            return false;
        };

        size_t qPos {std::string::npos};
        size_t cPos {std::string::npos};
        if (findTopLevelTernary(expr, qPos, cPos)) {
            const EvalResult cond {eval(expr.substr(0, qPos))};
            if (!cond.known)
                return {false, false};

            const std::string trueExpr {Utils::String::trim(expr.substr(qPos + 1, cPos - qPos - 1))};
            const std::string falseExpr {Utils::String::trim(expr.substr(cPos + 1))};
            return cond.value ? eval(trueExpr) : eval(falseExpr);
        }

        const size_t orPos {findTopLevelOperator(expr, "||")};
        if (orPos != std::string::npos) {
            const EvalResult left {eval(expr.substr(0, orPos))};
            const EvalResult right {eval(expr.substr(orPos + 2))};
            if (!left.known || !right.known)
                return {false, false};
            return {true, left.value || right.value};
        }

        const size_t andPos {findTopLevelOperator(expr, "&&")};
        if (andPos != std::string::npos) {
            const EvalResult left {eval(expr.substr(0, andPos))};
            const EvalResult right {eval(expr.substr(andPos + 2))};
            if (!left.known || !right.known)
                return {false, false};
            return {true, left.value && right.value};
        }

        auto evalMethod = [&expr, &eval]() -> EvalResult {
            auto evaluateMethodArgs = [&eval](const std::vector<std::string>& args,
                                              std::vector<std::string>& outArgs) -> bool {
                outArgs.clear();
                outArgs.reserve(args.size());
                for (const std::string& arg : args) {
                    const EvalResult nested {eval(arg)};
                    if (nested.known)
                        outArgs.emplace_back(nested.value ? "true" : "false");
                    else
                        outArgs.emplace_back(stripQuotes(arg));
                }
                return true;
            };

            const std::string lowerExpr {Utils::String::toLower(expr)};
            auto parseCall = [&expr](const std::string& method,
                                     std::vector<std::string>& args) -> bool {
                if (!Utils::String::startsWith(Utils::String::toLower(expr), method + "(") ||
                    expr.back() != ')') {
                    return false;
                }
                const std::string inner {expr.substr(method.size() + 1,
                                                     expr.size() - method.size() - 2)};
                args = splitTopLevelCsv(inner);
                return true;
            };

            auto parseObjectCall = [&expr](std::string& method,
                                           std::vector<std::string>& args) -> bool {
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

            std::vector<std::string> args;
            std::vector<std::string> evaluatedArgs;
            std::string parsedObjectMethod;
            const bool hasObjectCall {parseObjectCall(parsedObjectMethod, args)};

            auto methodCalled = [&parseCall, &hasObjectCall, &parsedObjectMethod, &args](
                                    const std::string& name) {
                if (parseCall(name, args))
                    return true;
                return hasObjectCall && parsedObjectMethod == name;
            };

            if (methodCalled("exists")) {
                evaluateMethodArgs(args, evaluatedArgs);
                if (evaluatedArgs.empty())
                    return {true, false};
                return {true, Utils::FileSystem::exists(evaluatedArgs.front())};
            }
            if (methodCalled("isdirectory")) {
                evaluateMethodArgs(args, evaluatedArgs);
                if (evaluatedArgs.empty())
                    return {true, false};
                return {true, Utils::FileSystem::isDirectory(evaluatedArgs.front())};
            }
            if (methodCalled("empty")) {
                evaluateMethodArgs(args, evaluatedArgs);
                if (evaluatedArgs.empty())
                    return {true, true};
                return {true, Utils::String::trim(evaluatedArgs.front()).empty()};
            }
            if (methodCalled("contains") && args.size() >= 2) {
                evaluateMethodArgs(args, evaluatedArgs);
                return {true, evaluatedArgs[0].find(evaluatedArgs[1]) != std::string::npos};
            }
            if (methodCalled("startswith") && args.size() >= 2) {
                evaluateMethodArgs(args, evaluatedArgs);
                return {true, Utils::String::startsWith(evaluatedArgs[0], evaluatedArgs[1])};
            }
            if (methodCalled("endswith") && args.size() >= 2) {
                evaluateMethodArgs(args, evaluatedArgs);
                return {true, Utils::String::endsWith(evaluatedArgs[0], evaluatedArgs[1])};
            }
            if (methodCalled("toboolean") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                bool boolValue {false};
                if (tryParseBoolLiteral(evaluatedArgs[0], boolValue))
                    return {true, boolValue};
                double numValue {0.0};
                if (tryParseNumberLiteral(evaluatedArgs[0], numValue))
                    return {true, numValue != 0.0};
                return {true, !Utils::String::trim(evaluatedArgs[0]).empty()};
            }
            if (methodCalled("tointeger") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                double numValue {0.0};
                if (tryParseNumberLiteral(evaluatedArgs[0], numValue))
                    return {true, static_cast<int>(numValue) != 0};
                return {false, false};
            }
            if (methodCalled("lower") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                expr = "'" + Utils::String::toLower(evaluatedArgs[0]) + "'";
                return {false, false};
            }
            if (methodCalled("upper") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                expr = "'" + Utils::String::toUpper(evaluatedArgs[0]) + "'";
                return {false, false};
            }
            if (methodCalled("trim") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                expr = "'" + Utils::String::trim(evaluatedArgs[0]) + "'";
                return {false, false};
            }
            if (methodCalled("proper") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                std::string v {Utils::String::toLower(evaluatedArgs[0])};
                if (!v.empty())
                    v[0] = static_cast<char>(std::toupper(v[0]));
                expr = "'" + v + "'";
                return {false, false};
            }
            if (methodCalled("tostring") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                expr = "'" + evaluatedArgs[0] + "'";
                return {false, false};
            }
            if (methodCalled("translate") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                expr = "'" + std::string {_p("theme", evaluatedArgs[0].c_str())} + "'";
                return {false, false};
            }
            if (methodCalled("date") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                std::string value {Utils::String::trim(evaluatedArgs[0])};
                size_t splitPos {value.find('T')};
                if (splitPos == std::string::npos)
                    splitPos = value.find(' ');
                if (splitPos != std::string::npos)
                    value = value.substr(0, splitPos);
                expr = "'" + value + "'";
                return {false, false};
            }
            if (methodCalled("time") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                std::string value {Utils::String::trim(evaluatedArgs[0])};
                size_t splitPos {value.find('T')};
                if (splitPos == std::string::npos)
                    splitPos = value.find(' ');
                if (splitPos != std::string::npos && splitPos + 1 < value.size())
                    value = value.substr(splitPos + 1);
                else
                    value.clear();
                expr = "'" + value + "'";
                return {false, false};
            }
            if ((methodCalled("year") || methodCalled("month") || methodCalled("day")) &&
                args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                std::string value {Utils::String::trim(evaluatedArgs[0])};
                size_t splitPos {value.find('T')};
                if (splitPos == std::string::npos)
                    splitPos = value.find(' ');
                if (splitPos != std::string::npos)
                    value = value.substr(0, splitPos);

                std::string digits;
                digits.reserve(value.size());
                for (const char c : value) {
                    if (std::isdigit(static_cast<unsigned char>(c)))
                        digits.push_back(c);
                }

                std::string out;
                if (digits.size() >= 8) {
                    if (methodCalled("year"))
                        out = digits.substr(0, 4);
                    else if (methodCalled("month"))
                        out = digits.substr(4, 2);
                    else if (methodCalled("day"))
                        out = digits.substr(6, 2);
                }

                expr = "'" + out + "'";
                return {false, false};
            }
            if (methodCalled("elapsed") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                const std::string value {Utils::String::trim(evaluatedArgs[0])};
                std::string out;
                if (!value.empty()) {
                    const time_t parsedTime {Utils::Time::stringToTime(value)};
                    if (parsedTime > 0) {
                        const time_t nowTime {Utils::Time::now()};
                        const time_t diff {nowTime > parsedTime ? nowTime - parsedTime : 0};
                        Utils::Time::Duration dur {diff};
                        if (dur.getDays() > 0) {
                            out = Utils::String::format(
                                _np("theme", "%i day ago", "%i days ago", dur.getDays()),
                                dur.getDays());
                        }
                        else if (dur.getHours() > 0) {
                            out = Utils::String::format(
                                _np("theme", "%i hour ago", "%i hours ago", dur.getHours()),
                                dur.getHours());
                        }
                        else if (dur.getMinutes() > 0) {
                            out = Utils::String::format(
                                _np("theme", "%i minute ago", "%i minutes ago", dur.getMinutes()),
                                dur.getMinutes());
                        }
                        else {
                            out = Utils::String::format(
                                _np("theme", "%i second ago", "%i seconds ago", dur.getSeconds()),
                                dur.getSeconds());
                        }
                    }
                }
                expr = "'" + out + "'";
                return {false, false};
            }
            if (methodCalled("expandseconds") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                std::string out;
                try {
                    long long sec {std::stoll(Utils::String::trim(evaluatedArgs[0]))};
                    if (sec < 0)
                        sec = 0;
                    Utils::Time::Duration dur {static_cast<time_t>(sec)};
                    if (dur.getDays() > 0) {
                        out = Utils::String::format(
                            _np("theme", "%i day ago", "%i days ago", dur.getDays()),
                            dur.getDays());
                    }
                    else if (dur.getHours() > 0) {
                        out = Utils::String::format(
                            _np("theme", "%i hour ago", "%i hours ago", dur.getHours()),
                            dur.getHours());
                    }
                    else if (dur.getMinutes() > 0) {
                        out = Utils::String::format(
                            _np("theme", "%i minute ago", "%i minutes ago", dur.getMinutes()),
                            dur.getMinutes());
                    }
                    else {
                        out = Utils::String::format(
                            _np("theme", "%i second ago", "%i seconds ago", dur.getSeconds()),
                            dur.getSeconds());
                    }
                }
                catch (...) {
                    out.clear();
                }
                expr = "'" + out + "'";
                return {false, false};
            }
            if (methodCalled("formatseconds") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                std::string out;
                try {
                    long long sec {std::stoll(Utils::String::trim(evaluatedArgs[0]))};
                    if (sec < 0)
                        sec = 0;
                    const long long hours {sec / 3600};
                    const long long minutes {(sec % 3600) / 60};
                    const long long seconds {sec % 60};
                    if (hours > 0)
                        out = Utils::String::format("%02lld:%02lld:%02lld", hours, minutes, seconds);
                    else
                        out = Utils::String::format("%02lld:%02lld", minutes, seconds);
                }
                catch (...) {
                    out.clear();
                }
                expr = "'" + out + "'";
                return {false, false};
            }
            if ((methodCalled("min") || methodCalled("max")) && args.size() >= 2) {
                evaluateMethodArgs(args, evaluatedArgs);
                double a {0.0};
                double b {0.0};
                if (tryParseNumberLiteral(evaluatedArgs[0], a) &&
                    tryParseNumberLiteral(evaluatedArgs[1], b)) {
                    const double out {methodCalled("min") ? std::min(a, b) : std::max(a, b)};
                    expr = std::to_string(out);
                    return {false, false};
                }
            }
            if (methodCalled("clamp") && args.size() >= 3) {
                evaluateMethodArgs(args, evaluatedArgs);
                double v {0.0};
                double lo {0.0};
                double hi {0.0};
                if (tryParseNumberLiteral(evaluatedArgs[0], v) &&
                    tryParseNumberLiteral(evaluatedArgs[1], lo) &&
                    tryParseNumberLiteral(evaluatedArgs[2], hi)) {
                    expr = std::to_string(std::max(lo, std::min(v, hi)));
                    return {false, false};
                }
            }
            if (methodCalled("getextension") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                expr = "'" + Utils::FileSystem::getExtension(evaluatedArgs[0]) + "'";
                return {false, false};
            }
            if (methodCalled("filename") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                expr = "'" + Utils::FileSystem::getFileName(evaluatedArgs[0]) + "'";
                return {false, false};
            }
            if (methodCalled("stem") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                expr = "'" + Utils::FileSystem::getStem(evaluatedArgs[0]) + "'";
                return {false, false};
            }
            if (methodCalled("directory") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                expr = "'" + Utils::FileSystem::getParent(evaluatedArgs[0]) + "'";
                return {false, false};
            }
            if (methodCalled("filesize") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                const long size {Utils::FileSystem::getFileSize(evaluatedArgs[0])};
                expr = std::to_string(size >= 0 ? size : 0);
                return {false, false};
            }
            if (methodCalled("filesizekb") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                const long size {Utils::FileSystem::getFileSize(evaluatedArgs[0])};
                const double sizeKb {(size >= 0 ? static_cast<double>(size) : 0.0) / 1024.0};
                expr = "'" + Utils::String::format("%.1f KB", sizeKb) + "'";
                return {false, false};
            }
            if (methodCalled("filesizemb") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                const long size {Utils::FileSystem::getFileSize(evaluatedArgs[0])};
                const double sizeMb {(size >= 0 ? static_cast<double>(size) : 0.0) /
                                     (1024.0 * 1024.0)};
                expr = "'" + Utils::String::format("%.1f MB", sizeMb) + "'";
                return {false, false};
            }
            if (methodCalled("firstfile") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                for (const auto& p : evaluatedArgs) {
                    if (Utils::FileSystem::exists(p)) {
                        expr = "'" + p + "'";
                        return {false, false};
                    }
                }
                expr = "''";
                return {false, false};
            }
            if (methodCalled("default") && args.size() >= 1) {
                evaluateMethodArgs(args, evaluatedArgs);
                const std::string value {Utils::String::trim(evaluatedArgs[0])};
                if (value.empty())
                    expr = "'Unknown'";
                else {
                    bool boolValue {false};
                    double numValue {0.0};
                    if ((tryParseBoolLiteral(value, boolValue) && !boolValue) ||
                        (tryParseNumberLiteral(value, numValue) && numValue == 0.0)) {
                        expr = "'None'";
                    }
                    else {
                        expr = "'" + value + "'";
                    }
                }
                return {false, false};
            }

            (void)lowerExpr;
            return {false, false};
        };

        const EvalResult methodResult {evalMethod()};
        if (methodResult.known)
            return methodResult;

        const std::vector<std::string> operators {{"!=", "==", ">=", "<=", ">", "<"}};
        for (const std::string& op : operators) {
            const size_t opPos {findTopLevelOperator(expr, op)};
            if (opPos == std::string::npos)
                continue;

            const std::string leftToken {stripQuotes(expr.substr(0, opPos))};
            const std::string rightToken {stripQuotes(expr.substr(opPos + op.size()))};

            bool leftBool {false};
            bool rightBool {false};
            if (tryParseBoolLiteral(leftToken, leftBool) && tryParseBoolLiteral(rightToken, rightBool)) {
                if (op == "==")
                    return {true, leftBool == rightBool};
                if (op == "!=")
                    return {true, leftBool != rightBool};
            }

            double leftNumber {0.0};
            double rightNumber {0.0};
            const bool leftIsNumber {tryParseNumberLiteral(leftToken, leftNumber)};
            const bool rightIsNumber {tryParseNumberLiteral(rightToken, rightNumber)};

            if (leftIsNumber && rightIsNumber) {
                if (op == "==")
                    return {true, leftNumber == rightNumber};
                if (op == "!=")
                    return {true, leftNumber != rightNumber};
                if (op == ">=")
                    return {true, leftNumber >= rightNumber};
                if (op == "<=")
                    return {true, leftNumber <= rightNumber};
                if (op == ">")
                    return {true, leftNumber > rightNumber};
                if (op == "<")
                    return {true, leftNumber < rightNumber};
            }

            if (op == "==")
                return {true, leftToken == rightToken};
            if (op == "!=")
                return {true, leftToken != rightToken};

            return {false, false};
        }

        bool literalBool {false};
        if (tryParseBoolLiteral(expr, literalBool))
            return {true, literalBool};

        double literalNumber {0.0};
        if (tryParseNumberLiteral(expr, literalNumber))
            return {true, literalNumber != 0.0};

        const std::string stripped {stripQuotes(expr)};
        return {true, !Utils::String::trim(stripped).empty()};
    };

    const EvalResult result {eval(expression)};
    if (!result.known)
        return false;

    outValue = result.value;
    return true;
}

static std::string normalizeStoryboardEventName(const std::string& raw)
{
    const std::string trimmed {Utils::String::trim(raw)};
    std::string token {Utils::String::toLower(trimmed)};

    token.erase(std::remove_if(token.begin(), token.end(),
                               [](unsigned char c) {
                                   return std::isspace(c) || c == '-' || c == '_';
                               }),
                token.end());

    if (token == "activate")
        return "activate";
    if (token == "deactivate")
        return "deactivate";
    if (token == "open")
        return "open";
    if (token == "close")
        return "close";
    if (token == "show")
        return "show";
    if (token == "hide")
        return "hide";
    if (token == "scroll")
        return "scroll";

    return trimmed;
}

bool ThemeStoryboard::fromXmlNode(
    const pugi::xml_node& root,
    const std::map<std::string, ThemeData::ElementPropertyType>& typeMap,
    const std::string& relativePath,
    const std::function<std::string(const std::string&)>& resolver)
{
    if (strcmp(root.name(), "storyboard") != 0)
        return false;

    auto resolveValue = [&resolver](const std::string& value) {
        return resolver ? resolver(value) : value;
    };

    // Batocera compatibility: allow conditional storyboard blocks.
    if (root.attribute("if")) {
        const std::string storyboardIfExpr {resolveValue(root.attribute("if").as_string())};
        if (!storyboardIfExpr.empty()) {
            bool storyboardEnabled {false};
            if (tryEvaluateEnabledExpression(storyboardIfExpr, storyboardEnabled)) {
                if (!storyboardEnabled)
                    return false;
            }
            else {
                const bool hasDynamicTokens {storyboardIfExpr.find('{') != std::string::npos ||
                                             storyboardIfExpr.find('}') != std::string::npos};

                if (hasDynamicTokens)
                    return false;

                const std::string low {
                    Utils::String::toLower(Utils::String::trim(storyboardIfExpr))};
                if (low == "false" || low == "0" || low == "no" || low == "off")
                    return false;
            }
        }
    }

    repeat = 1;
    eventName = normalizeStoryboardEventName(
        resolveValue(root.attribute("event").as_string()));

    std::string sbRepeat {resolveValue(root.attribute("repeat").as_string())};
    if (sbRepeat == "forever" || sbRepeat == "infinite")
        repeat = 0;
    else if (!sbRepeat.empty() && sbRepeat != "none")
        repeat = atoi(sbRepeat.c_str());

    sbRepeat = resolveValue(root.attribute("repeatAt").as_string());
    if (sbRepeat.empty())
        sbRepeat = resolveValue(root.attribute("repeatat").as_string());

    if (!sbRepeat.empty()) {
        if (repeat == 1)
            repeat = 0;
        repeatAt = atoi(sbRepeat.c_str());
    }

    for (pugi::xml_node node = root.child("animation"); node;
         node = node.next_sibling("animation")) {
        std::string prop {resolveValue(node.attribute("property").as_string())};
        if (prop.empty())
            continue;

        ThemeData::ElementPropertyType type {
            static_cast<ThemeData::ElementPropertyType>(-1)};

        auto typeIt {typeMap.find(prop)};
        if (typeIt != typeMap.cend()) {
            type = typeIt->second;
        }
        else {
            // Batocera themes frequently animate shorthand component properties that are not
            // always present in the static element property map.
            if (prop == "x" || prop == "y" || prop == "w" || prop == "h" || prop == "scale" ||
                prop == "opacity" || prop == "rotation" || prop == "zIndex" ||
                prop == "brightness" || prop == "saturation" || prop == "dimming") {
                type = ThemeData::ElementPropertyType::FLOAT;
            }
            else if (prop == "pos" || prop == "size" || prop == "origin" ||
                     prop == "rotationOrigin" || prop == "scaleOrigin") {
                type = ThemeData::ElementPropertyType::NORMALIZED_PAIR;
            }
            else if (prop == "padding") {
                type = ThemeData::ElementPropertyType::NORMALIZED_RECT;
            }
            else if (prop == "path") {
                type = ThemeData::ElementPropertyType::PATH;
            }
            else if (prop == "text") {
                type = ThemeData::ElementPropertyType::STRING;
            }
            else if (prop == "visible") {
                type = ThemeData::ElementPropertyType::BOOLEAN;
            }
            else if (prop == "color") {
                type = ThemeData::ElementPropertyType::COLOR;
            }
            else {
                LOG(LogWarning) << "Unknown storyboard property type \"" << prop << "\"";
                continue;
            }
        }

        ThemeAnimation* anim {nullptr};

        switch (type) {
            case ThemeData::ElementPropertyType::NORMALIZED_RECT:
                anim = new ThemeVector4Animation();
                if (node.attribute("from"))
                    anim->from = parseNormalizedRect(
                        resolveValue(node.attribute("from").as_string()));
                if (node.attribute("to"))
                    anim->to =
                        parseNormalizedRect(resolveValue(node.attribute("to").as_string()));
                break;
            case ThemeData::ElementPropertyType::NORMALIZED_PAIR:
                anim = new ThemeVector2Animation();
                if (node.attribute("from"))
                    anim->from = parseNormalizedPair(
                        resolveValue(node.attribute("from").as_string()));
                if (node.attribute("to"))
                    anim->to =
                        parseNormalizedPair(resolveValue(node.attribute("to").as_string()));
                break;
            case ThemeData::ElementPropertyType::COLOR:
                anim = new ThemeColorAnimation();
                if (node.attribute("from"))
                    anim->from = parseHexColor(resolveValue(node.attribute("from").as_string()));
                if (node.attribute("to"))
                    anim->to = parseHexColor(resolveValue(node.attribute("to").as_string()));
                break;
            case ThemeData::ElementPropertyType::FLOAT:
                anim = new ThemeFloatAnimation();
                if (node.attribute("from"))
                    anim->from = static_cast<float>(
                        atof(resolveValue(node.attribute("from").as_string()).c_str()));
                if (node.attribute("to"))
                    anim->to = static_cast<float>(
                        atof(resolveValue(node.attribute("to").as_string()).c_str()));
                break;
            case ThemeData::ElementPropertyType::PATH:
                anim = new ThemePathAnimation();
                if (node.attribute("from"))
                    anim->from = Utils::FileSystem::resolveRelativePath(
                        resolveValue(node.attribute("from").as_string()), relativePath, true);
                if (node.attribute("to"))
                    anim->to = Utils::FileSystem::resolveRelativePath(
                        resolveValue(node.attribute("to").as_string()), relativePath, true);
                break;
            case ThemeData::ElementPropertyType::STRING:
                anim = new ThemeStringAnimation();
                if (node.attribute("from"))
                    anim->from = resolveValue(node.attribute("from").as_string());
                if (node.attribute("to"))
                    anim->to = resolveValue(node.attribute("to").as_string());
                break;
            case ThemeData::ElementPropertyType::BOOLEAN:
                anim = new ThemeBoolAnimation();
                if (node.attribute("from"))
                    anim->from = parseBool(resolveValue(node.attribute("from").as_string()));
                if (node.attribute("to"))
                    anim->to = parseBool(resolveValue(node.attribute("to").as_string()));
                break;
            default:
                LOG(LogWarning) << "Unsupported animation property type \"" << prop << "\"";
                continue;
        }

        if (anim != nullptr) {
            anim->propertyName = prop;

            std::string mode {"linear"};

            for (pugi::xml_attribute xattr : node.attributes()) {
                if (strcmp(xattr.name(), "enabled") == 0) {
                    std::string enabled {resolveValue(xattr.as_string())};
                    anim->enabledExpression = enabled;

                    bool evaluatedEnabled {false};
                    if (tryEvaluateEnabledExpression(enabled, evaluatedEnabled)) {
                        anim->enabled = evaluatedEnabled;
                    }
                    else {
                        // If expression still has unresolved dynamic tokens, keep disabled
                        // until runtime bindings are available to evaluate it.
                        const bool hasDynamicTokens {
                            enabled.find('{') != std::string::npos ||
                            enabled.find('}') != std::string::npos};

                        if (hasDynamicTokens) {
                            anim->enabled = false;
                            continue;
                        }

                        // Conservative fallback for non-dynamic expressions not handled by
                        // evaluator: only explicit false literals disable the animation.
                        std::string low {Utils::String::toLower(Utils::String::trim(enabled))};
                        anim->enabled = !(low == "false" || low == "0" || low == "no" ||
                                          low == "off");
                    }
                }
                else if (strcmp(xattr.name(), "begin") == 0)
                    anim->begin = atoi(resolveValue(xattr.as_string()).c_str());
                else if (strcmp(xattr.name(), "duration") == 0)
                    anim->duration = atoi(resolveValue(xattr.as_string()).c_str());
                else if (strcmp(xattr.name(), "repeat") == 0) {
                    std::string aRepeat {resolveValue(xattr.as_string())};
                    if (aRepeat == "forever" || aRepeat == "infinite")
                        anim->repeat = 0;
                    else if (!aRepeat.empty() && aRepeat != "none")
                        anim->repeat = atoi(aRepeat.c_str());
                }
                else if (strcmp(xattr.name(), "autoreverse") == 0 ||
                         strcmp(xattr.name(), "autoReverse") == 0) {
                    std::string aReverse {resolveValue(xattr.as_string())};
                    anim->autoReverse = (aReverse == "true" || aReverse == "1");
                }
                else if (strcmp(xattr.name(), "mode") == 0 ||
                         strcmp(xattr.name(), "easingMode") == 0 ||
                         strcmp(xattr.name(), "easing") == 0 ||
                         strcmp(xattr.name(), "interpolation") == 0)
                        mode = Utils::String::toLower(resolveValue(xattr.as_string()));
            }

            // Normalize token variants such as "ease-in", "ease_in", "ease in".
            mode = Utils::String::trim(Utils::String::toLower(mode));
            mode.erase(std::remove_if(mode.begin(), mode.end(),
                                      [](const unsigned char c) {
                                          return std::isspace(c) != 0 || c == '-' || c == '_';
                                      }),
                       mode.end());

            if (mode == "easein" || mode == "easeinquad")
                anim->easingMode = ThemeAnimation::EasingMode::EaseIn;
            else if (mode == "easeincubic")
                anim->easingMode = ThemeAnimation::EasingMode::EaseInCubic;
            else if (mode == "easeinquint")
                anim->easingMode = ThemeAnimation::EasingMode::EaseInQuint;
            else if (mode == "easeout" || mode == "easeoutquad")
                anim->easingMode = ThemeAnimation::EasingMode::EaseOut;
            else if (mode == "easeoutcubic")
                anim->easingMode = ThemeAnimation::EasingMode::EaseOutCubic;
            else if (mode == "easeoutquint")
                anim->easingMode = ThemeAnimation::EasingMode::EaseOutQuint;
            else if (mode == "easeinout" || mode == "linearsmooth")
                anim->easingMode = ThemeAnimation::EasingMode::EaseInOut;
            else if (mode == "bump")
                anim->easingMode = ThemeAnimation::EasingMode::Bump;
            else {
                if (mode != "linear") {
                    LOG(LogDebug) << "Unknown storyboard easing mode \"" << mode
                                  << "\" for property \"" << prop
                                  << "\", using linear";
                }
                anim->easingMode = ThemeAnimation::EasingMode::Linear;
            }

            animations.push_back(anim);
        }
    }

    for (pugi::xml_node node = root.child("sound"); node; node = node.next_sibling("sound")) {
        std::string path {resolveValue(node.attribute("path").as_string())};
        if (path.empty())
            continue;

        path = Utils::FileSystem::resolveRelativePath(path, relativePath, true);
        if (!ResourceManager::getInstance().fileExists(path))
            continue;

        auto* sound = new ThemeSoundAnimation();
        sound->propertyName = "sound";
        sound->to = path;

        for (pugi::xml_attribute xattr : node.attributes()) {
            if (strcmp(xattr.name(), "begin") == 0 || strcmp(xattr.name(), "at") == 0)
                sound->begin = atoi(resolveValue(xattr.as_string()).c_str());
            else if (strcmp(xattr.name(), "duration") == 0)
                sound->duration = atoi(resolveValue(xattr.as_string()).c_str());
            else if (strcmp(xattr.name(), "repeat") == 0) {
                std::string aRepeat {resolveValue(xattr.as_string())};
                if (aRepeat == "forever" || aRepeat == "infinite")
                    sound->repeat = 0;
                else if (!aRepeat.empty() && aRepeat != "none")
                    sound->repeat = atoi(aRepeat.c_str());
            }
            else if (strcmp(xattr.name(), "autoreverse") == 0 ||
                     strcmp(xattr.name(), "autoReverse") == 0) {
                std::string aReverse {resolveValue(xattr.as_string())};
                sound->autoReverse = (aReverse == "true" || aReverse == "1");
            }
        }

        animations.push_back(sound);
    }

    return !animations.empty();
}
