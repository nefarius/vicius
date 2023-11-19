#pragma once

using json = nlohmann::json;

namespace models
{
    class DownloadLocationConfig
    {
    public:
        /** The path or inja template */
        std::string input;
        /** The optional inja data */
        std::optional<json> data;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        DownloadLocationConfig,
        input,
        data
    )
}
