using Minisign;
using Minisign.Models;

namespace Nefarius.Vicius.Example.Server.Services;

/// <summary>
///     Singleton service that holds a loaded minisign private key and can sign manifest payloads
///     in-memory at request time. Configured via environment variables:
///     <list type="bullet">
///         <item><c>E2E_MINISIGN_SECKEY</c> — path to the <c>.key</c> file.</item>
///         <item><c>E2E_MINISIGN_PASSWORD</c> — password used to decrypt the key.</item>
///     </list>
///     When either variable is absent the service is unconfigured and <see cref="IsConfigured" />
///     returns <c>false</c>.
/// </summary>
internal sealed class MinisignManifestSigner
{
    private readonly MinisignPrivateKey? _privateKey;
    private readonly Lock _lock = new();

    public MinisignManifestSigner(ILogger<MinisignManifestSigner> logger)
    {
        string? secKeyPath = Environment.GetEnvironmentVariable("E2E_MINISIGN_SECKEY");
        string? password = Environment.GetEnvironmentVariable("E2E_MINISIGN_PASSWORD");

        if (string.IsNullOrEmpty(secKeyPath) || string.IsNullOrEmpty(password))
        {
            logger.LogInformation(
                "MinisignManifestSigner: E2E_MINISIGN_SECKEY or E2E_MINISIGN_PASSWORD not set; dynamic signing disabled.");
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
}
