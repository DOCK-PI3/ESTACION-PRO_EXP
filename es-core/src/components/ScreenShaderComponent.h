//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ScreenShaderComponent.h
//

#ifndef ES_CORE_COMPONENTS_SCREEN_SHADER_COMPONENT_H
#define ES_CORE_COMPONENTS_SCREEN_SHADER_COMPONENT_H

#include "GuiComponent.h"
#include "renderers/Renderer.h"

class ScreenShaderComponent : public GuiComponent
{
public:
    ScreenShaderComponent();

    void render(const glm::mat4& parentTrans) override;
    void applyTheme(const std::shared_ptr<ThemeData>& theme,
                    const std::string& view,
                    const std::string& element,
                    unsigned int properties) override;

private:
    std::string mShaderPath;
    Renderer::postProcessingParams mParameters;
};

#endif // ES_CORE_COMPONENTS_SCREEN_SHADER_COMPONENT_H
