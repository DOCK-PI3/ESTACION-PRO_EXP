//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  StackPanelComponent.h
//
//  Lightweight stack layout container used for Batocera/Retrobat theme compatibility.
//

#ifndef ES_CORE_COMPONENTS_STACK_PANEL_COMPONENT_H
#define ES_CORE_COMPONENTS_STACK_PANEL_COMPONENT_H

#include "GuiComponent.h"

class StackPanelComponent : public GuiComponent
{
public:
    StackPanelComponent();

    void render(const glm::mat4& parentTrans) override;
    void applyTheme(const std::shared_ptr<ThemeData>& theme,
                    const std::string& view,
                    const std::string& element,
                    unsigned int properties) override;
    void onSizeChanged() override;
    void update(int deltaTime) override;

private:
    void performLayout();

    bool mHorizontal;
    bool mReverse;
    float mSeparator;
    glm::vec4 mPadding;
    bool mClipChildren;
};

#endif // ES_CORE_COMPONENTS_STACK_PANEL_COMPONENT_H
