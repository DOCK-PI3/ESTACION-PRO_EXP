// SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ThemeAnimation.h
//
//  Clases base y derivadas para las animaciones de storyboard de temas.
//  Adaptado del sistema de storyboards de batocera-emulationstation.
//

#pragma once

#include "ThemeData.h"
#include "utils/MathUtil.h"

#include <cstdint>
#include <string>

// Clase base para todas las animaciones de storyboard.
class ThemeAnimation
{
public:
    enum class EasingMode {
        Linear,
        EaseIn,
        EaseInCubic,
        EaseInQuint,
        EaseOut,
        EaseOutCubic,
        EaseOutQuint,
        EaseInOut,
        Bump
    };

    ThemeAnimation()
    {
        repeat = 1;
        duration = 0;
        begin = 0;
        autoReverse = false;
        easingMode = EasingMode::Linear;
        enabled = true;
    }

    // Nota: from.type y to.type se inicializan a Unknown por defecto gracias
    // al valor por defecto en el struct Property.


    virtual ~ThemeAnimation() = default;

    std::string propertyName;
    int duration;
    int begin;
    bool autoReverse;
    int repeat; // 0 = infinito
    EasingMode easingMode;
    bool enabled;
    std::string enabledExpression;

    ThemeData::ThemeElement::Property from;
    ThemeData::ThemeElement::Property to;

    virtual ThemeData::ThemeElement::Property computeValue(double value) = 0;

    void ensureInitialValue(const ThemeData::ThemeElement::Property& initialValue)
    {
        if (from.type == ThemeData::ThemeElement::Property::PropertyType::Unknown)
            from = initialValue;
        if (to.type == ThemeData::ThemeElement::Property::PropertyType::Unknown)
            to = initialValue;
    }
};

// Anima una propiedad float.
class ThemeFloatAnimation : public ThemeAnimation
{
public:
    ThemeData::ThemeElement::Property computeValue(double value) override
    {
        ThemeData::ThemeElement::Property result;
        result = static_cast<float>(from.f * (1.0 - value) + to.f * value);
        return result;
    }
};

// Anima un color (unsigned int RGBA).
class ThemeColorAnimation : public ThemeAnimation
{
public:
    ThemeData::ThemeElement::Property computeValue(double value) override
    {
        // Mezcla cada canal RGBA por separado.
        unsigned int fromC = from.i;
        unsigned int toC = to.i;

        uint8_t r = static_cast<uint8_t>(((fromC >> 24) & 0xFF) * (1.0 - value) +
                                         ((toC >> 24) & 0xFF) * value);
        uint8_t g = static_cast<uint8_t>(((fromC >> 16) & 0xFF) * (1.0 - value) +
                                         ((toC >> 16) & 0xFF) * value);
        uint8_t b = static_cast<uint8_t>(((fromC >> 8) & 0xFF) * (1.0 - value) +
                                         ((toC >> 8) & 0xFF) * value);
        uint8_t a = static_cast<uint8_t>(((fromC >> 0) & 0xFF) * (1.0 - value) +
                                         ((toC >> 0) & 0xFF) * value);

        ThemeData::ThemeElement::Property result;
        result = static_cast<unsigned int>((r << 24) | (g << 16) | (b << 8) | a);
        return result;
    }
};

// Anima un par de valores (glm::vec2).
class ThemeVector2Animation : public ThemeAnimation
{
public:
    ThemeData::ThemeElement::Property computeValue(double value) override
    {
        ThemeData::ThemeElement::Property result;
        result = glm::vec2 {static_cast<float>(from.v.x * (1.0 - value) + to.v.x * value),
                            static_cast<float>(from.v.y * (1.0 - value) + to.v.y * value)};
        return result;
    }
};

// Anima un rectángulo normalizado (glm::vec4).
class ThemeVector4Animation : public ThemeAnimation
{
public:
    ThemeData::ThemeElement::Property computeValue(double value) override
    {
        ThemeData::ThemeElement::Property result;
        result = glm::vec4 {
            static_cast<float>(from.r.x * (1.0 - value) + to.r.x * value),
            static_cast<float>(from.r.y * (1.0 - value) + to.r.y * value),
            static_cast<float>(from.r.z * (1.0 - value) + to.r.z * value),
            static_cast<float>(from.r.w * (1.0 - value) + to.r.w * value)};
        return result;
    }
};

// Anima un string (el valor 'to' se aplica al 99.99% del recorrido).
class ThemeStringAnimation : public ThemeAnimation
{
public:
    ThemeData::ThemeElement::Property computeValue(double value) override
    {
        ThemeData::ThemeElement::Property result;
        if (value >= 0.9999)
            result = to.s;
        else
            result = from.s;
        return result;
    }
};

// Anima una ruta de archivo.
class ThemePathAnimation : public ThemeAnimation
{
public:
    ThemeData::ThemeElement::Property computeValue(double value) override
    {
        ThemeData::ThemeElement::Property result;
        if (value >= 0.9999)
            result = to.s;
        else
            result = from.s;
        return result;
    }
};

// Anima un valor booleano.
class ThemeBoolAnimation : public ThemeAnimation
{
public:
    ThemeData::ThemeElement::Property computeValue(double value) override
    {
        ThemeData::ThemeElement::Property result;
        if (value >= 0.9999)
            result = to.b;
        else
            result = from.b;
        return result;
    }
};

// Reproduce un sonido en un momento determinado del storyboard.
class ThemeSoundAnimation : public ThemeAnimation
{
public:
    ThemeData::ThemeElement::Property computeValue(double value) override
    {
        ThemeData::ThemeElement::Property result;
        if (value >= 0.9999)
            result = to.s;
        else
            result = from.s;
        return result;
    }
};
