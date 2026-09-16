using Minisign;
using Minisign.Models;

namespace Nefarius.Vicius.Example.Server.Services;

/// <summary>
///     Singleton service that holds a loaded minisign private key and can sign manifest payloads
///     in-memory at request time. Configured via environment variables, in this order:
///     <list type="number">
///         <item><c>MINISIGN_SECKEY</c> / <c>MINISIGN_PASSWORD</c> (production).</item>
///         <item><c>E2E_MINISIGN_SECKEY</c> / <c>E2E_MINISIGN_PASSWORD</c> (E2E / local fallback).</item>
///     </list>
///     When neither pair is complete the service is unconfigured and <see cref="IsConfigured" />
///     returns <c>false</c>.
/// </summary>
internal sealed class MinisignManifestSigner
{
    private readonly MinisignPrivateKey? _privateKey;
    private readonly Lock _lock = new();

    public MinisignManifestSigner(ILogger<MinisignManifestSigner> logger)
    {
        if (!TryReadCredentialPair("MINISIGN_SECKEY", "MINISIGN_PASSWORD", out string? secKeyPath, out string? password)
            && !TryReadCredentialPair("E2E_MINISIGN_SECKEY", "E2E_MINISIGN_PASSWORD", out secKeyPath, out password))
        {
            logger.LogInformation(
                "MinisignManifestSigner: MINISIGN_SECKEY/PASSWORD (or E2E_ fallback) not set; dynamic signing disabled.");
            return;
        }

        if (!File.Exists(secKeyPath))
        {
            logger.LogWarning(
                "MinisignManifestSigner: key file '{Path}' not found; dynamic signing disabled.", secKeyPath);
            return;
        }

        try
        {
            _privateKey = Core.LoadPrivateKeyFromFile(secKeyPath, password);
            logger.LogInformation("MinisignManifestSigner: private key loaded from '{Path}'.", secKeyPath);
        }
        catch (Exception ex)
        {
            logger.LogError(ex, "MinisignManifestSigner: failed to load private key; dynamic signing disabled.");
        }
    }

    /// <summary>Returns <c>true</c> when a private key was successfully loaded.</summary>
    public bool IsConfigured => _privateKey is not null;

    /// <summary>
    ///     Signs <paramref name="manifestBytes" /> with the pre-loaded private key using the prehashed
    ///     ("ED", Ed25519 over BLAKE2b-512) minisign format and returns the raw <c>.minisig</c> sidecar bytes.
    /// </summary>
    /// <exception cref="InvalidOperationException">Thrown when the signer is not configured.</exception>
    public byte[] SignDetached(byte[] manifestBytes)
    {
        if (_privateKey is null)
            throw new InvalidOperationException("MinisignManifestSigner is not configured.");

        // Core.SignHashed requires a real file path; write a temp file, sign it, read the sidecar.
        string tmpFile = Path.Combine(Path.GetTempPath(), $"vicius-manifest-{Guid.NewGuid():N}.json");
        string sigFile = tmpFile + ".minisig";
        try
        {
            lock (_lock)
            {
                File.WriteAllBytes(tmpFile, manifestBytes);
                Core.SignHashed(tmpFile, _privateKey);
                return File.ReadAllBytes(sigFile);
            }
        }
        finally
        {
            if (File.Exists(tmpFile)) File.Delete(tmpFile);
            if (File.Exists(sigFile)) File.Delete(sigFile);
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
