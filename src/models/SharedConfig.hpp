#pragma once

#include "ProductDetection.hpp"
#include "DownloadLocationConfig.h"
#include "SignatureValidation.hpp"

using json = nlohmann::json;

namespace models
{
    /**
     * \brief Parameters that might be provided by both the server and the local configuration.
     * \remarks Keep in sync with MergedConfig.
     */
    class SharedConfig
    {
    public:
        /** The classic window title, only shown in taskbar in our case */
        std::optional<std::string> windowTitle;
        /** Name of the product shown in UI */
        std::optional<std::string> productName;
        /** The detection method */
        std::optional<ProductVersionDetectionMethod> detectionMethod;
        /** The detection method for the installed software version */
        std::optional<json> detection;
        /** URL of the error article */
        std::optional<std::string> installationErrorUrl;
        /** The setup download location */
        std::optional<DownloadLocationConfig> downloadLocation;
        /** True to run as temporary copy */
        std::optional<bool> runAsTemporaryCopy;
        /** Global Authenticode verification mode (default: WhenPresent) */
        std::optional<SignatureVerificationMode> signatureVerificationMode;
        /** Global Authenticode comparison policy (default: Relaxed) */
        std::optional<SignatureComparisonPolicy> signaturePolicy;
        /** Global Authenticode pin strategy (default: FromUpdaterBinary) */
        std::optional<SignatureVerificationStrategy> signatureStrategy;
        /** Global Authenticode certificate pin (used with FromConfiguration strategy) */
        std::optional<SignatureConfig> signatureConfig;
        /** True to hide the "Remind me tomorrow" button in the UI */
        std::optional<bool> hideRemindButton;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SharedConfig,
                                                    windowTitle,
                                                    productName,
                                                    detectionMethod,
                                                    detection,
                                                    installationErrorUrl,
                                                    downloadLocation,
                                                    runAsTemporaryCopy,
                                                    signatureVerificationMode,
                                                    signaturePolicy,
                                                    signatureStrategy,
                                                    signatureConfig,
                                                    hideRemindButton)
}
