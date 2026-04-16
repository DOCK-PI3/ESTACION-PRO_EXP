//  SPDX-License-Identifier: MIT
//
//  ESTACION-PRO Frontend
//  WebImageComponent.cpp
//

#include "components/WebImageComponent.h"

#include "HttpReq.h"
#include "utils/StringUtil.h"

WebImageComponent::WebImageComponent()
    : ImageComponent()
    , mRequestedTile {false}
{
}

WebImageComponent::~WebImageComponent()
{
}

void WebImageComponent::setImage(const std::string& path, bool tile)
{
    const std::string lowerPath {Utils::String::toLower(path)};
    const bool isHttp {Utils::String::startsWith(lowerPath, "http://") ||
                       Utils::String::startsWith(lowerPath, "https://")};

    if (!isHttp) {
        mRequest.reset();
        mRequestedPath.clear();
        ImageComponent::setImage(path, tile);
        return;
    }

    if (mRequestedPath == path && mRequest)
        return;

    mRequestedPath = path;
    mRequestedTile = tile;
    mRequest = std::make_unique<HttpReq>(path, false);

    // Keep empty until the network request has completed.
    ImageComponent::setImage("");
}

void WebImageComponent::update(int deltaTime)
{
    GuiComponent::update(deltaTime);

    if (!mRequest)
        return;

    const HttpReq::Status status {mRequest->status()};
    if (status == HttpReq::REQ_IN_PROGRESS)
        return;

    if (status == HttpReq::REQ_SUCCESS) {
        const std::string content {mRequest->getContent()};
        if (!content.empty())
            ImageComponent::setImage(content.data(), content.size(), mRequestedTile);
    }

    mRequest.reset();
}
