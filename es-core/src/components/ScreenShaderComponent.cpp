//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ScreenShaderComponent.cpp
//

#include "components/ScreenShaderComponent.h"

#include "ThemeData.h"
#include "renderers/Renderer.h"
#include "utils/StringUtil.h"

ScreenShaderComponent::ScreenShaderComponent()
{
}

void ScreenShaderComponent::render(const glm::mat4& parentTrans)
{
    if (!isVisible() || mThemeOpacity == 0.0f)
        return;

    glm::mat4 trans {parentTrans * getTransform()};

    if (getOpacity() == 0.0f || getSize().x == 0.0f || getSize().y == 0.0f)
        return;

    unsigned int shaders {Renderer::Shader::CORE};
    const std::string loweredPath {Utils::String::toLower(mShaderPath)};

    if (loweredPath.find("scanline") != std::string::npos)
        shaders |= Renderer::Shader::SCANLINES;

    if (loweredPath.find("blur") != std::string::npos || mParameters.blurStrength > 0.0f)
        shaders |= Renderer::Shader::BLUR_HORIZONTAL | Renderer::Shader::BLUR_VERTICAL;

    Renderer::postProcessingParams parameters;
    parameters.opacity = mParameters.opacity;
    parameters.saturation = mParameters.saturation;
    parameters.dimming = mParameters.dimming;
    parameters.blurStrength = mParameters.blurStrength;
    parameters.blurPasses = mParameters.blurPasses;
    parameters.shaders = mParameters.shaders;
    parameters.opacity *= (getOpacity() * mThemeOpacity);

    if ((shaders & (Renderer::Shader::BLUR_HORIZONTAL | Renderer::Shader::BLUR_VERTICAL)) != 0 &&
        parameters.blurStrength <= 0.0f) {
        parameters.blurStrength = 1.75f;
    }

    Renderer::getInstance()->setMatrix(trans);
    Renderer::getInstance()->shaderPostprocessing(shaders, parameters, nullptr);

    renderChildren(trans);
}

void ScreenShaderComponent::applyTheme(const std::shared_ptr<ThemeData>& theme,
                                       const std::string& view,
                                       const std::string& element,
                                       unsigned int properties)
{
    GuiComponent::applyTheme(theme, view, element, properties);

    const ThemeData::ThemeElement* elem {theme->getElement(view, element, "screenshader")};
    if (elem == nullptr)
        return;

    if (elem->has("path"))
        mShaderPath = elem->get<std::string>("path");

    if (elem->has("blur"))
        mParameters.blurStrength = elem->get<float>("blur");

    if (elem->has("blurPasses"))
        mParameters.blurPasses = static_cast<unsigned int>(elem->get<float>("blurPasses"));

    if (elem->has("opacity"))
        mParameters.opacity = elem->get<float>("opacity");

    if (elem->has("saturation"))
        mParameters.saturation = elem->get<float>("saturation");

    if (elem->has("dimming"))
        mParameters.dimming = elem->get<float>("dimming");
}
