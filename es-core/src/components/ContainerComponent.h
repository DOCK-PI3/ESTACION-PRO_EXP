//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  ContainerComponent.h
//

#ifndef ES_CORE_COMPONENTS_CONTAINER_COMPONENT_H
#define ES_CORE_COMPONENTS_CONTAINER_COMPONENT_H

#include "GuiComponent.h"

class ContainerComponent : public GuiComponent
{
public:
    ContainerComponent();

    void render(const glm::mat4& parentTrans) override;
    void applyTheme(const std::shared_ptr<ThemeData>& theme,
                    const std::string& view,
                    const std::string& element,
                    unsigned int properties) override;

private:
    glm::vec4 mPadding;
    bool mClipChildren;
};

#endif // ES_CORE_COMPONENTS_CONTAINER_COMPONENT_H
