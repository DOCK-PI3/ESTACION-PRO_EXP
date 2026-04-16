//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  WebImageComponent.h
//

#ifndef ES_CORE_COMPONENTS_WEB_IMAGE_COMPONENT_H
#define ES_CORE_COMPONENTS_WEB_IMAGE_COMPONENT_H

#include "components/ImageComponent.h"

#include <memory>

class HttpReq;

class WebImageComponent : public ImageComponent
{
public:
    WebImageComponent();
    ~WebImageComponent() override;

    void setImage(const std::string& path, bool tile = false) override;
    void update(int deltaTime) override;

private:
    std::unique_ptr<HttpReq> mRequest;
    std::string mRequestedPath;
    bool mRequestedTile;
};

#endif // ES_CORE_COMPONENTS_WEB_IMAGE_COMPONENT_H
