#pragma once

#include "ProductDetection.hpp"
#include "DownloadLocationConfig.h"

using json = nlohmann::json;

namespace models
{
    /**
     * \brief Parameters that might be provided by both the server and the local configuration.
     * \remarks Keep in sync with SharedConfig.
     */
    class MergedConfig
    {
    public:
        /** The classic window title, only shown in taskbar in our case */
        std::string windowTitle;
        /** Name of the product shown in UI */
        std::string productName;
        /** The detection method */
        ProductVersionDetectionMethod detectionMethod{ProductVersionDetectionMethod::Invalid};
        /** The detection method for the installed software version */
        json detection;
        /** URL of the error article */
        std::string installationErrorUrl;
        /** The setup download location */
        DownloadLocationConfig downloadLocation;
        /** True to run as temporary copy */
        bool runAsTemporaryCopy{false};

        MergedConfig() : windowTitle(NV_WINDOW_TITLE), productName(NV_PRODUCT_NAME) { }

        RegistryValueConfig GetRegistryValueConfig() const { return detection.get<RegistryValueConfig>(); }

        FileVersionConfig GetFileVersionConfig() const { return detection.get<FileVersionConfig>(); }

        FileSizeConfig GetFileSizeConfig() const { return detection.get<FileSizeConfig>(); }

        FileChecksumConfig GetFileChecksumConfig() const { return detection.get<FileChecksumConfig>(); }

        CustomExpressionConfig GetCustomExpressionConfig() const { return detection.get<CustomExpressionConfig>(); }
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MergedConfig,
                                                    windowTitle,
                                                    productName,
                                                    detectionMethod,
                                                    detection,
                                                    installationErrorUrl,
                                                    downloadLocation,
                                                    runAsTemporaryCopy)
}
