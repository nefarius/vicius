#include "pch.h"
#include "Util.h"
#include "InstanceConfig.hpp"
#include "NAuthenticode.h"

// ============================================================================
// Helpers
// ============================================================================

namespace
{
    /**
     * \brief Hash a file with MD5 and return the hex digest.
     */
    std::string HashFileMD5(const std::string& filePath)
    {
        MD5 alg;
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return {};

        constexpr std::size_t chunkSize = 4 * 1024;
        std::vector<char> buf(chunkSize);
        while (!file.eof())
        {
            file.read(buf.data(), buf.size());
            const std::streamsize n = file.gcount();
            if (n > 0) alg.add(buf.data(), n);
        }
        return alg.getHash();
    }

    std::string HashFileSHA1(const std::string& filePath)
    {
        SHA1 alg;
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return {};

        constexpr std::size_t chunkSize = 4 * 1024;
        std::vector<char> buf(chunkSize);
        while (!file.eof())
        {
            file.read(buf.data(), buf.size());
            const std::streamsize n = file.gcount();
            if (n > 0) alg.add(buf.data(), n);
        }
        return alg.getHash();
    }

    std::string HashFileSHA256(const std::string& filePath)
    {
        SHA256 alg;
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return {};

        constexpr std::size_t chunkSize = 4 * 1024;
        std::vector<char> buf(chunkSize);
        while (!file.eof())
        {
            file.read(buf.data(), buf.size());
            const std::streamsize n = file.gcount();
            if (n > 0) alg.add(buf.data(), n);
        }
        return alg.getHash();
    }

    /**
     * \brief Convert a LPCWSTR (may be nullptr) to a UTF-8 std::string for comparison.
     *        Works for both LPWSTR and LPTSTR (which = LPWSTR when CharacterSet=Unicode).
     */
    std::string WstrToUtf8(LPCWSTR p)
    {
        if (!p) return {};
        const int len = WideCharToMultiByte(CP_UTF8, 0, p, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string result(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, p, -1, result.data(), len, nullptr, nullptr);
        return result;
    }

    /**
     * \brief Match a pin value (optional) against an actual cert field.
     *        If the pin is absent, the field is considered a match (not pinned).
     */
    bool PinMatches(const std::optional<std::string>& pin, const std::string& actual)
    {
        if (!pin.has_value()) return true;           // not pinned, always OK
        if (pin.value().empty()) return true;         // empty pin treated as not set
        return util::icompare(pin.value(), actual);   // case-insensitive compare
    }

    /**
     * \brief Compute the SHA-1 thumbprint of a certificate context and return hex string.
     */
    std::string CertThumbprintSha1(PCCERT_CONTEXT ctx)
    {
        if (!ctx) return {};
        BYTE hash[20] = {};
        DWORD hashLen = sizeof(hash);
        if (!CertGetCertificateContextProperty(ctx, CERT_SHA1_HASH_PROP_ID, hash, &hashLen)) return {};

        std::string result;
        result.reserve(hashLen * 2);
        for (DWORD i = 0; i < hashLen; ++i)
        {
            char buf[3];
            std::snprintf(buf, sizeof(buf), "%02x", hash[i]);
            result += buf;
        }
        return result;
    }

    /**
     * \brief Compute the SHA-256 thumbprint of a certificate context and return hex string.
     */
    std::string CertThumbprintSha256(PCCERT_CONTEXT ctx)
    {
        if (!ctx) return {};
        BYTE hash[32] = {};
        DWORD hashLen = sizeof(hash);
        if (!CertGetCertificateContextProperty(ctx, CERT_SHA256_HASH_PROP_ID, hash, &hashLen)) return {};

        std::string result;
        result.reserve(hashLen * 2);
        for (DWORD i = 0; i < hashLen; ++i)
        {
            char buf[3];
            std::snprintf(buf, sizeof(buf), "%02x", hash[i]);
            result += buf;
        }
        return result;
    }

    /**
     * \brief Look up the leaf signing certificate from the given file and invoke a callback.
     *        Returns false if the file is not signed or the cert can't be located.
     */
    bool WithSigningCert(const std::filesystem::path& filePath,
                         const std::function<void(PCCERT_CONTEXT)>& callback)
    {
        HCERTSTORE hStore = nullptr;
        HCRYPTMSG hMsg = nullptr;
        DWORD dwEncoding = 0, dwContentType = 0, dwFormatType = 0;

        const auto wPath = filePath.wstring();
        const BOOL ok = CryptQueryObject(
            CERT_QUERY_OBJECT_FILE,
            wPath.c_str(),
            CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
            CERT_QUERY_FORMAT_FLAG_BINARY,
            0,
            &dwEncoding, &dwContentType, &dwFormatType,
            &hStore, &hMsg, nullptr
        );

        if (!ok) return false;

        auto cleanupScope = sg::make_scope_guard([&]
        {
            if (hMsg) CryptMsgClose(hMsg);
            if (hStore) CertCloseStore(hStore, 0);
        });

        DWORD dwSignerInfo = 0;
        if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &dwSignerInfo) || dwSignerInfo == 0)
            return false;

        std::vector<BYTE> signerBuf(dwSignerInfo);
        if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, signerBuf.data(), &dwSignerInfo))
            return false;

        const auto* pSignerInfo = reinterpret_cast<PCMSG_SIGNER_INFO>(signerBuf.data());

        CERT_INFO ci = {};
        ci.Issuer = pSignerInfo->Issuer;
        ci.SerialNumber = pSignerInfo->SerialNumber;

        PCCERT_CONTEXT pCertCtx = CertFindCertificateInStore(
            hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
            CERT_FIND_SUBJECT_CERT, &ci, nullptr);

        if (!pCertCtx) return false;

        auto certScope = sg::make_scope_guard([pCertCtx] { CertFreeCertificateContext(pCertCtx); });
        callback(pCertCtx);
        return true;
    }

} // namespace

// ============================================================================
// Async wrapper (mirrors InvokeSetupAsync / GetSetupStatus / ResetSetupState)
// ============================================================================

std::expected<void, std::string> models::InstanceConfig::VerifyReleaseIntegrityAsync()
{
    if (verifyTask.has_value())
        return std::unexpected("Verification already in progress");

    try
    {
        verifyTask = std::async(std::launch::async, &InstanceConfig::VerifyReleaseIntegrity, this);
    }
    catch (const std::system_error& e)
    {
        return std::unexpected(std::format("Failed to start verification thread: {}", e.what()));
    }

    return {};
}

std::optional<models::InstanceConfig::VerifyStatus> models::InstanceConfig::GetVerifyStatus() const
{
    if (!verifyTask.has_value())
        return std::nullopt;

    const std::future_status futureStatus = verifyTask->wait_for(std::chrono::seconds(0));

    VerifyStatus status{};
    status.isVerifying = futureStatus == std::future_status::timeout;
    status.hasFinished = futureStatus == std::future_status::ready;

    if (status.hasFinished)
        status.result = verifyTask->get();

    return status;
}

void models::InstanceConfig::ResetVerifyState()
{
    if (!verifyTask.has_value())
        return;

    // Only destroy the shared_future if the task has already completed; dropping a
    // not-yet-ready shared_future that was created via std::async will block the
    // calling thread (the render thread) until the background work finishes.
    if (verifyTask->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        verifyTask.reset();
    // Still running: leave it.  VerifyReleaseIntegrityAsync guards against re-launch,
    // and GetVerifyStatus will eventually report completion on a later frame.
}

// ============================================================================
// Layer 1: Setup Checksum Verification
// ============================================================================

std::expected<void, std::string> models::InstanceConfig::VerifyReleaseIntegrity()
{
    const auto& release = GetSelectedRelease();
    const auto tempFile = GetLocalReleaseTempFilePath(GetSelectedReleaseId());

    spdlog::debug("VerifyReleaseIntegrity: verifying {}", tempFile.string());

    //
    // --- Checksum layer ---
    //
    if (release.checksum.has_value())
    {
        const auto& hashCfg = release.checksum.value();

        if (!std::filesystem::exists(tempFile))
        {
            spdlog::error("Checksum verification: downloaded file not found at {}", tempFile.string());
            return std::unexpected("Downloaded file not found for checksum verification");
        }

        std::string computed;
        switch (hashCfg.checksumAlg)
        {
            case ChecksumAlgorithm::MD5:
                spdlog::debug("Checksum verification: hashing with MD5");
                computed = HashFileMD5(tempFile.string());
                break;
            case ChecksumAlgorithm::SHA1:
                spdlog::debug("Checksum verification: hashing with SHA1");
                computed = HashFileSHA1(tempFile.string());
                break;
            case ChecksumAlgorithm::SHA256:
                spdlog::debug("Checksum verification: hashing with SHA256");
                computed = HashFileSHA256(tempFile.string());
                break;
            case ChecksumAlgorithm::Invalid:
                spdlog::error("Checksum verification: invalid algorithm specified in manifest");
                return std::unexpected("Invalid checksum algorithm specified in manifest");
        }

        if (computed.empty())
        {
            spdlog::error("Checksum verification: failed to hash file {}", tempFile.string());
            return std::unexpected("Failed to compute file checksum");
        }

        spdlog::debug("Checksum verification: computed={}, expected={}", computed, hashCfg.checksum);

        if (!util::icompare(computed, hashCfg.checksum))
        {
            spdlog::error("Checksum verification FAILED: computed {} does not match expected {}",
                          computed, hashCfg.checksum);
            return std::unexpected(std::format("Checksum mismatch: expected {}, got {}", hashCfg.checksum, computed));
        }

        spdlog::info("Checksum verification passed");
    }
    else
    {
        if (strictVerification)
        {
            spdlog::error("Checksum verification: no checksum in manifest and strict mode is active");
            return std::unexpected("No checksum provided by the update server and strict verification is enabled");
        }
        spdlog::warn("Checksum verification: no checksum provided, skipping (relaxed mode)");
    }

    //
    // --- Authenticode / publisher pin layer ---
    //
    const auto verificationMode = merged.signatureVerificationMode;

    if (verificationMode == SignatureVerificationMode::Disabled)
    {
        spdlog::info("Authenticode verification is disabled by configuration");
        return {};
    }

    // Resolve effective policy and strategy (release-level overrides global)
    const auto effectivePolicy = release.signaturePolicy.value_or(merged.signaturePolicy);
    const auto effectiveStrategy = release.signatureStrategy.value_or(merged.signatureStrategy);

    // Release-level signature config takes precedence (implies FromConfiguration)
    const auto& effectiveSigConfig = release.signature.has_value()
                                         ? release.signature
                                         : merged.signatureConfig;

    const auto sigResult = VerifySetupSignature(tempFile, effectiveSigConfig, effectivePolicy, effectiveStrategy);

    if (!sigResult)
    {
        // If the file is unsigned and we are in WhenPresent mode, accept it
        if (verificationMode == SignatureVerificationMode::WhenPresent &&
            sigResult.error().find("not signed") != std::string::npos)
        {
            spdlog::warn("Authenticode verification: file is unsigned, accepting (WhenPresent mode)");
            return {};
        }
        return std::unexpected(sigResult.error());
    }

    return {};
}

// ============================================================================
// Layer 2 + 3: Authenticode Verification + Publisher Pinning
// ============================================================================

std::expected<void, std::string> models::InstanceConfig::VerifySetupSignature(
    const std::filesystem::path& filePath,
    const std::optional<models::SignatureConfig>& releaseSig,
    models::SignatureComparisonPolicy policy,
    models::SignatureVerificationStrategy strategy) const
{
    spdlog::debug("VerifySetupSignature: checking {}", filePath.string());

    // Run WinVerifyTrust (chain validation + revocation, no lifetime-signing flag).
    // NVerifyFileSignature takes LPCTSTR which is LPCWSTR when CharacterSet=Unicode.
    NSIGINFO sigInfo = {};
    const BOOL chainOk = NVerifyFileSignature(filePath.wstring().c_str(), &sigInfo);

    auto cleanupScope = sg::make_scope_guard([&sigInfo] { NCertFreeSigInfo(&sigInfo); });

    if (!chainOk)
    {
        // Treat "no Authenticode signature could possibly apply" the same as an
        // explicitly unsigned file so WhenPresent mode accepts it (and Required
        // mode still rejects it via the unsigned-error path in the caller):
        //   TRUST_E_NOSIGNATURE          - a signable file that carries no signature
        //   TRUST_E_SUBJECT_FORM_UNKNOWN - non-PE payload (e.g. a .zip archive)
        // Any other status (e.g. TRUST_E_PROVIDER_UNKNOWN, where verification
        // could not even be performed) falls through to the generic failure path
        // below rather than being silently accepted as "unsigned".
        if (sigInfo.lValidationResult == TRUST_E_NOSIGNATURE ||
            sigInfo.lValidationResult == TRUST_E_SUBJECT_FORM_UNKNOWN)
        {
            spdlog::warn("VerifySetupSignature: file is not signed ({:#x}): {}",
                         static_cast<unsigned long>(sigInfo.lValidationResult), filePath.string());
            return std::unexpected(std::string("File is not signed"));
        }

        const LONG result = sigInfo.lValidationResult;
        spdlog::error("VerifySetupSignature: WinVerifyTrust failed with {:#x} for {}", result, filePath.string());
        return std::unexpected(std::format("Authenticode chain validation failed ({:#x})", static_cast<unsigned long>(result)));
    }

    spdlog::info("VerifySetupSignature: Authenticode chain valid for {}", filePath.string());

    // Relaxed with no configuration = chain valid is enough
    if (policy == SignatureComparisonPolicy::Relaxed && !releaseSig.has_value() &&
        strategy == SignatureVerificationStrategy::FromUpdaterBinary && !isUpdaterSigned)
    {
        spdlog::info("VerifySetupSignature: relaxed mode, no pin configured - accepting");
        return {};
    }

    // Extract cert info from the downloaded file for pinning
    crypto::SIGNATURE_INFORMATION fileInfo = {};
    const bool extracted = crypto::ExtractSignatureInformation(filePath.wstring().c_str(), &fileInfo);

    auto certCleanup = sg::make_scope_guard([&fileInfo]
    {
        crypto::FreeSignatureInformation(&fileInfo);
    });

    if (!extracted)
    {
        // Can't extract but chain was valid - only fail in Strict mode
        if (policy == SignatureComparisonPolicy::Strict)
        {
            spdlog::error("VerifySetupSignature: could not extract cert info for pinning in strict mode");
            return std::unexpected("Could not extract certificate information for publisher pin verification");
        }
        spdlog::warn("VerifySetupSignature: could not extract cert info, relaxed mode - accepting");
        return {};
    }

    const std::string fileSubject  = WstrToUtf8(fileInfo.SubjectName);
    const std::string fileIssuer   = WstrToUtf8(fileInfo.IssuerName);
    const std::string fileSerial   = WstrToUtf8(fileInfo.SerialNumber);

    spdlog::debug("VerifySetupSignature: file subject='{}' issuer='{}' serial='{}'",
                  fileSubject, fileIssuer, fileSerial);

    // Determine the effective pin source
    const bool useFromConfig = releaseSig.has_value() ||
                               strategy == SignatureVerificationStrategy::FromConfiguration;

    if (useFromConfig && releaseSig.has_value())
    {
        // FromConfiguration: compare against explicit cert pin from (signed) manifest
        const auto& pin = releaseSig.value();

        // Build thumbprints on demand for comparison
        std::string fileSha1, fileSha256;
        if (pin.thumbprintSha1.has_value() || pin.thumbprintSha256.has_value())
        {
            WithSigningCert(filePath, [&](PCCERT_CONTEXT ctx)
            {
                if (pin.thumbprintSha1.has_value()) fileSha1 = CertThumbprintSha1(ctx);
                if (pin.thumbprintSha256.has_value()) fileSha256 = CertThumbprintSha256(ctx);
            });
        }

        const bool pinMatch = PinMatches(pin.subjectName,     fileSubject)
                           && PinMatches(pin.issuerName,      fileIssuer)
                           && PinMatches(pin.serialNumber,    fileSerial)
                           && PinMatches(pin.thumbprintSha1,  fileSha1)
                           && PinMatches(pin.thumbprintSha256, fileSha256);

        if (!pinMatch)
        {
            spdlog::error("VerifySetupSignature: publisher pin mismatch (FromConfiguration)");
            spdlog::debug("  file  subject='{}' issuer='{}' serial='{}' sha1='{}' sha256='{}'",
                          fileSubject, fileIssuer, fileSerial, fileSha1, fileSha256);
            spdlog::debug("  pin   subject='{}' issuer='{}' serial='{}' sha1='{}' sha256='{}'",
                          pin.subjectName.value_or("<any>"),
                          pin.issuerName.value_or("<any>"),
                          pin.serialNumber.value_or("<any>"),
                          pin.thumbprintSha1.value_or("<any>"),
                          pin.thumbprintSha256.value_or("<any>"));
            return std::unexpected("Publisher certificate does not match the configured pin");
        }

        spdlog::info("VerifySetupSignature: publisher pin matched (FromConfiguration)");
        return {};
    }

    if (strategy == SignatureVerificationStrategy::FromUpdaterBinary && isUpdaterSigned)
    {
        // FromUpdaterBinary: compare subjectName ONLY (stable across cert renewals).
        // Use the extended cert info (appCertInfo.SubjectName) rather than the WinTrust
        // publisher string (appSigInfo.lpszPublisher) for a consistent field.
        const std::string updaterSubject = WstrToUtf8(appCertInfo.SubjectName);

        spdlog::debug("VerifySetupSignature: FromUpdaterBinary: updater subject='{}' file subject='{}'",
                      updaterSubject, fileSubject);

        if (!util::icompare(updaterSubject, fileSubject))
        {
            spdlog::error("VerifySetupSignature: publisher mismatch (FromUpdaterBinary): "
                          "updater='{}' setup='{}'", updaterSubject, fileSubject);
            return std::unexpected(std::format(
                "Setup publisher ('{}') does not match updater publisher ('{}').",
                fileSubject, updaterSubject));
        }

        spdlog::info("VerifySetupSignature: publisher matched (FromUpdaterBinary, subjectName)");
        return {};
    }

    // Reached here because the requested pin source could not be resolved:
    //   - FromConfiguration was requested but no SignatureConfig is present, OR
    //   - FromUpdaterBinary was requested but the updater binary itself is not signed.
    // The Relaxed+FromUpdaterBinary+!isUpdaterSigned case was already accepted above (line 332-338).
    // In Strict mode we must fail closed; accepting on chain-only would defeat the pin requirement.
    if (policy == SignatureComparisonPolicy::Strict)
    {
        spdlog::error("VerifySetupSignature: strict mode requires a resolvable publisher pin "
                      "(strategy={}, isUpdaterSigned={}), but none is available - rejecting",
                      magic_enum::enum_name(strategy), isUpdaterSigned);
        return std::unexpected("Strict verification requires a publisher pin but none could be resolved "
                               "(check signatureStrategy / signatureConfig settings)");
    }

    spdlog::info("VerifySetupSignature: no publisher pin resolvable, accepting on chain validity (relaxed mode)");
    return {};
}

// ============================================================================
// Layer 4 (manifest): Ed25519 / minisign signature verification
// ============================================================================

#if defined(NV_MANIFEST_PUBLIC_KEY)

/**
 * \brief Parse a minisign .minisig file and extract the raw Ed25519 signature bytes.
 *
 * The minisign .minisig format is:
 *   Line 0: comment ("untrusted comment: ...")
 *   Line 1: base64-encoded  "signature_algorithm (2 bytes) || key_id (8 bytes) || signature (64 bytes)"
 *   Line 2: trusted comment ("trusted comment: ...")
 *   Line 3: base64-encoded global signature (covers trusted comment + file signature)
 *
 * We only need line 1 to verify the file body.
 */
static bool ParseMinisigFile(const std::string& minisigBody,
                             std::string& outKeyId,
                             std::vector<unsigned char>& outSig,
                             bool& outPrehashed)
{
    // Split into lines
    std::vector<std::string> lines;
    std::istringstream ss(minisigBody);
    std::string line;
    while (std::getline(ss, line))
    {
        // Strip CR
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }

    if (lines.size() < 2) return false;

    // Decode line 1 (base64)
    const auto& b64 = lines[1];
    const size_t decodedLen = b64.size(); // base64 text is always longer than its decoded form
    std::vector<unsigned char> decoded(decodedLen);
    size_t actualLen = 0;

    if (sodium_base642bin(decoded.data(), decodedLen, b64.c_str(), b64.size(),
                          nullptr, &actualLen, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0)
    {
        return false;
    }

    // Layout: 2 bytes algo + 8 bytes key id + 64 bytes Ed25519 sig = 74 bytes total
    if (actualLen < 74) return false;

    // First 2 bytes: algorithm tag.
    //   "Ed" = legacy   : pure Ed25519 over the raw message
    //   "ED" = prehashed : Ed25519 over BLAKE2b-512 of the message
    // minisign has defaulted to the prehashed ("ED") format since 0.6, so we
    // must accept both and verify accordingly (see VerifyManifestSignature).
    if (decoded[0] != 'E') return false;
    if (decoded[1] == 'd')      outPrehashed = false;
    else if (decoded[1] == 'D') outPrehashed = true;
    else                        return false;

    // Bytes 2-9: key id (we store as hex string for logging)
    std::string keyId;
    keyId.reserve(16);
    for (int i = 2; i < 10; ++i)
    {
        char buf[3];
        std::snprintf(buf, sizeof(buf), "%02x", decoded[i]);
        keyId += buf;
    }
    outKeyId = keyId;

    // Bytes 10-73: Ed25519 signature
    outSig.assign(decoded.begin() + 10, decoded.begin() + 10 + crypto_sign_BYTES);
    return true;
}

/**
 * \brief Decode the base64 public key from the NV_MANIFEST_PUBLIC_KEY string.
 *        The key string is the second line of a minisign .pub file.
 *        Layout: 2 bytes algo + 8 bytes key id + 32 bytes public key = 42 bytes.
 */
static bool ParseMinisignPublicKey(const char* pubKeyStr,
                                   std::vector<unsigned char>& outPubKey)
{
    const size_t maxLen = strlen(pubKeyStr); // base64 text is always longer than its decoded form
    std::vector<unsigned char> decoded(maxLen);
    size_t actualLen = 0;

    if (sodium_base642bin(decoded.data(), maxLen, pubKeyStr, strlen(pubKeyStr),
                          nullptr, &actualLen, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0)
    {
        return false;
    }

    if (actualLen < 42) return false;                    // 2 + 8 + 32
    if (decoded[0] != 'E' || decoded[1] != 'd') return false;

    outPubKey.assign(decoded.begin() + 10, decoded.begin() + 10 + crypto_sign_PUBLICKEYBYTES);
    return true;
}

std::expected<void, std::string> models::InstanceConfig::VerifyManifestSignature(
    const std::string& manifestBody,
    const std::string& minisigBody)
{
    if (sodium_init() < 0)
    {
        return std::unexpected("Failed to initialize libsodium");
    }

    // Parse the .minisig sidecar
    std::string keyId;
    std::vector<unsigned char> sig;
    bool prehashed = false;
    if (!ParseMinisigFile(minisigBody, keyId, sig, prehashed))
    {
        return std::unexpected("Failed to parse manifest signature file (.minisig)");
    }

    spdlog::debug("VerifyManifestSignature: key id from .minisig = {}", keyId);

    if (sig.size() != crypto_sign_BYTES)
    {
        return std::unexpected(std::format("Invalid signature length: expected {}, got {}",
                                           crypto_sign_BYTES, sig.size()));
    }

    // Decode the compiled-in public key
    std::vector<unsigned char> pubKey;
    if (!ParseMinisignPublicKey(NV_MANIFEST_PUBLIC_KEY, pubKey))
    {
        return std::unexpected("Failed to decode the compiled-in manifest public key (NV_MANIFEST_PUBLIC_KEY)");
    }

    if (pubKey.size() != crypto_sign_PUBLICKEYBYTES)
    {
        return std::unexpected("Invalid compiled-in public key length");
    }

    // Verify the detached Ed25519 signature. In prehashed ("ED") mode the
    // signature covers BLAKE2b-512 of the body (minisign's default), so we must
    // hash the manifest first and verify over the 64-byte digest; in legacy
    // ("Ed") mode the signature covers the raw body directly.
    int result;
    if (prehashed)
    {
        unsigned char digest[crypto_generichash_BYTES_MAX];
        crypto_generichash(
            digest, sizeof digest,
            reinterpret_cast<const unsigned char*>(manifestBody.data()),
            manifestBody.size(),
            nullptr, 0
        );
        result = crypto_sign_verify_detached(sig.data(), digest, sizeof digest, pubKey.data());
    }
    else
    {
        result = crypto_sign_verify_detached(
            sig.data(),
            reinterpret_cast<const unsigned char*>(manifestBody.data()),
            manifestBody.size(),
            pubKey.data()
        );
    }

    if (result != 0)
    {
        spdlog::error("VerifyManifestSignature: Ed25519 signature verification FAILED ({} mode)",
                      prehashed ? "prehashed" : "legacy");
        return std::unexpected("Manifest signature is invalid - the update manifest may have been tampered with");
    }

    spdlog::info("VerifyManifestSignature: manifest signature valid");
    return {};
}

bool models::InstanceConfig::CheckAndUpdateManifestVersion(const uint64_t signedVersion) const
{
    //
    // Persist the highest manifest version seen in a volatile registry key.
    // Key: HKCU\Software\<manufacturer>\<product>\Vicius\ManifestVersion
    //
    const auto subKey = std::format(L"Software\\{}\\{}\\Vicius",
                                    ConvertAnsiToWide(manufacturer.empty() ? appFilename : manufacturer),
                                    ConvertAnsiToWide(product.empty() ? appFilename : product));

    winreg::RegKey key;
    constexpr REGSAM flags = KEY_READ | KEY_WRITE;

    if (const auto result = key.TryCreate(HKEY_CURRENT_USER, subKey, flags); !result)
    {
        spdlog::warn("CheckAndUpdateManifestVersion: could not open/create registry key ({})", result.Code());
        return true; // non-fatal; don't block updates over a registry hiccup
    }

    constexpr auto valueName = L"ManifestVersion";
    uint64_t stored = 0;

    if (const auto readResult = key.TryGetQwordValue(valueName); readResult)
    {
        stored = readResult.GetValue();
    }

    spdlog::debug("CheckAndUpdateManifestVersion: stored={} signed={}", stored, signedVersion);

    if (signedVersion < stored)
    {
        spdlog::error("CheckAndUpdateManifestVersion: rollback detected! signed={} < stored={}",
                      signedVersion, stored);
        return false;
    }

    if (signedVersion > stored)
    {
        if (const auto writeResult = key.TrySetQwordValue(valueName, signedVersion); !writeResult)
        {
            spdlog::warn("CheckAndUpdateManifestVersion: could not persist version ({})", writeResult.Code());
        }
    }

    return true;
}

#endif // NV_MANIFEST_PUBLIC_KEY
