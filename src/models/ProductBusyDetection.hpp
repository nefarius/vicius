#pragma once

using json = nlohmann::json;

namespace models
{
    /**
     * \brief Configuration for detecting whether the watched product is currently running,
     *        used to defer the update notification dialog until the product is closed.
     */
    class ProductBusyDetectionConfig
    {
    public:
        /**
         * \brief Process image base names to match case-insensitively against running processes.
         * \example ["HidHide.exe", "HidHideMon.exe"]
         */
        std::vector<std::string> imageNames;

        /**
         * \brief Optional absolute paths (or inja templates) for exact full-path matching
         *        against running processes. Rendered via the inja template engine before comparison.
         */
        std::optional<std::vector<std::string>> executablePaths;

        /**
         * \brief Seconds between successive in-use re-checks. Default: 60.
         */
        std::optional<int> pollIntervalSeconds;

        /**
         * \brief Maximum minutes to wait before giving up and deferring to the next scheduled run.
         *        Default: 180 (3 hours). Hard-clamped to 180 by the agent regardless of this value.
         */
        std::optional<int> maxWaitMinutes;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ProductBusyDetectionConfig,
                                                    imageNames,
                                                    executablePaths,
                                                    pollIntervalSeconds,
                                                    maxWaitMinutes)
}
