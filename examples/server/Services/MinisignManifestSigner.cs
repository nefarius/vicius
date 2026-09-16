using Minisign;
using Minisign.Models;

namespace Nefarius.Vicius.Example.Server.Services;

/// <summary>
///     Singleton service that can hold two independently loaded minisign private keys:
///     <list type="bullet">
///         <item>
///             Production (<c>MINISIGN_SECKEY</c> / <c>MINISIGN_PASSWORD</c>) — used by release
///             product routes such as BthPS3.
///         </item>
///         <item>
///             E2E (<c>E2E_MINISIGN_SECKEY</c> / <c>E2E_MINISIGN_PASSWORD</c>) — used only by the
///             E2E dynamic-signing endpoint. Never applied to production routes.
///         </item>
///     </list>
///     A pair is ignored unless both values are set. Missing or unloadable credentials disable
///     that scope only.
/// </summary>
internal sealed class MinisignManifestSigner
{
    private readonly MinisignPrivateKey? _productionKey;
    private readonly MinisignPrivateKey? _e2eKey;
    private readonly Lock _lock = new();

    public MinisignManifestSigner(ILogger<MinisignManifestSigner> logger)
    {
        _productionKey = TryLoadKey(logger, "MINISIGN_SECKEY", "MINISIGN_PASSWORD", "production");
        _e2eKey = TryLoadKey(logger, "E2E_MINISIGN_SECKEY", "E2E_MINISIGN_PASSWORD", "E2E");
    }

    /// <summary>Returns <c>true</c> when the production private key was successfully loaded.</summary>
    public bool IsConfigured => _productionKey is not null;

    /// <summary>Returns <c>true</c> when the E2E private key was successfully loaded.</summary>
    public bool IsE2EConfigured => _e2eKey is not null;

    /// <summary>
    ///     Signs <paramref name="manifestBytes" /> with the production private key using the prehashed
    ///     ("ED", Ed25519 over BLAKE2b-512) minisign format and returns the raw <c>.minisig</c> sidecar bytes.
    /// </summary>
    /// <exception cref="InvalidOperationException">Thrown when the production signer is not configured.</exception>
    public byte[] SignDetached(byte[] manifestBytes) =>
        SignWith(_productionKey, manifestBytes, "production");

    /// <summary>
    ///     Signs <paramref name="manifestBytes" /> with the E2E private key. Used only by E2E routes.
    /// </summary>
    /// <exception cref="InvalidOperationException">Thrown when the E2E signer is not configured.</exception>
    public byte[] SignE2EDetached(byte[] manifestBytes) =>
        SignWith(_e2eKey, manifestBytes, "E2E");

    private byte[] SignWith(MinisignPrivateKey? key, byte[] manifestBytes, string scope)
    {
        if (key is null)
            throw new InvalidOperationException($"MinisignManifestSigner {scope} key is not configured.");

        // Core.SignHashed requires a real file path; write a temp file, sign it, read the sidecar.
        string tmpFile = Path.Combine(Path.GetTempPath(), $"vicius-manifest-{Guid.NewGuid():N}.json");
        string sigFile = tmpFile + ".minisig";
        try
        {
            lock (_lock)
            {
                File.WriteAllBytes(tmpFile, manifestBytes);
                Core.SignHashed(tmpFile, key);
                return File.ReadAllBytes(sigFile);
            }
        }
        finally
        {
            if (File.Exists(tmpFile)) File.Delete(tmpFile);
            if (File.Exists(sigFile)) File.Delete(sigFile);
        }
    }

    private static MinisignPrivateKey? TryLoadKey(
        ILogger logger, string keyName, string passwordName, string scope)
    {
        if (!TryReadCredentialPair(keyName, passwordName, out string? secKeyPath, out string? password))
        {
            logger.LogInformation(
                "MinisignManifestSigner: {KeyName}/{PasswordName} not set; {Scope} signing disabled.",
                keyName, passwordName, scope);
            return null;
        }

        if (!File.Exists(secKeyPath))
        {
            logger.LogWarning(
                "MinisignManifestSigner: {Scope} key file '{Path}' not found; {Scope} signing disabled.",
                scope, secKeyPath, scope);
            return null;
        }

        try
        {
            MinisignPrivateKey key = Core.LoadPrivateKeyFromFile(secKeyPath, password);
            logger.LogInformation(
                "MinisignManifestSigner: {Scope} private key loaded from '{Path}'.", scope, secKeyPath);
            return key;
        }
        catch (Exception ex)
        {
            logger.LogError(ex,
                "MinisignManifestSigner: failed to load {Scope} private key; {Scope} signing disabled.",
                scope, scope);
            return null;
        }
    }

    /// <summary>
    ///     Returns <c>true</c> only when both values of a pair are non-empty, so a production key
    ///     is never combined with an E2E password (or the reverse).
    /// </summary>
    private static bool TryReadCredentialPair(
        string keyName, string passwordName, out string? secKeyPath, out string? password)
    {
        secKeyPath = Environment.GetEnvironmentVariable(keyName);
        password = Environment.GetEnvironmentVariable(passwordName);
        return !string.IsNullOrEmpty(secKeyPath) && !string.IsNullOrEmpty(password);
    }
}
