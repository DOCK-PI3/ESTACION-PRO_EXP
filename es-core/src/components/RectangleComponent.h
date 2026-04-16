//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  RectangleComponent.h
//
//  Basic filled rectangle with optional border and rounded corners.
//

#ifndef ES_CORE_COMPONENTS_RECTANGLE_COMPONENT_H
#define ES_CORE_COMPONENTS_RECTANGLE_COMPONENT_H

#include "GuiComponent.h"

class RectangleComponent : public GuiComponent
{
public:
    RectangleComponent();

    void render(const glm::mat4& parentTrans) override;
    void applyTheme(const std::shared_ptr<ThemeData>& theme,
                    const std::string& view,
                    const std::string& element,
                    unsigned int properties) override;

private:
    unsigned int mBackgroundColor;
    unsigned int mBorderColor;
    float mBorderSize;
    float mRoundCorners;
};

#endif // ES_CORE_COMPONENTS_RECTANGLE_COMPONENT_H
