#pragma once
#include "../Common.h"
#include "../ADL.hpp"
#include "SharedConfig.hpp"
#include "UpdateRelease.hpp"
#include "UpdateConfig.hpp"

namespace models
{
    /**
     * \brief An instance returned by the remote update API.
     */
    class UpdateResponse
    {
    public:
        /** The (optional) global settings instance */
        std::optional<UpdateConfig> instance;
        /** The (optional) shared settings */
        std::optional<SharedConfig> shared;
        /** The available releases */
        std::vector<UpdateRelease> releases;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        UpdateResponse,
        instance,
        shared,
        releases
    )
}
