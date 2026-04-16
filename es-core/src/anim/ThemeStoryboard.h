// SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ThemeStoryboard.h
//
//  Estructura y parser XML de storyboards de tema.
//

#pragma once

#include "ThemeAnimation.h"

#include <pugixml.hpp>

#include <map>
#include <functional>
#include <string>
#include <vector>

class ThemeStoryboard
{
public:
    ThemeStoryboard()
    {
        repeatAt = 0;
        repeat = 1;
    }

    ThemeStoryboard(const ThemeStoryboard& src);
    ~ThemeStoryboard();

    std::string eventName;
    int repeat; // 0 = infinito
    int repeatAt;

    std::vector<ThemeAnimation*> animations;

    bool fromXmlNode(const pugi::xml_node& root,
                     const std::map<std::string, ThemeData::ElementPropertyType>& typeMap,
                     const std::string& relativePath,
                     const std::function<std::string(const std::string&)>& resolver = nullptr);
};
