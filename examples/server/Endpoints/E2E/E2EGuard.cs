using System.Security.Cryptography;

namespace Nefarius.Vicius.Example.Server.Endpoints.E2E;

/// <summary>
///     Shared helpers for all E2E test endpoints.
/// </summary>
internal static class E2EGuard
{
    /// <summary>Returns <c>true</c> when <c>VICIUS_E2E=1</c> is set in the environment.</summary>
    public static bool IsEnabled =>
        string.Equals(Environment.GetEnvironmentVariable("VICIUS_E2E"), "1",
            StringComparison.Ordinal);

    /// <summary>Returns the value of the <c>E2E_ARTIFACTS_DIR</c> environment variable.</summary>
    public static string ArtifactsDir =>
        Environment.GetEnvironmentVariable("E2E_ARTIFACTS_DIR") ?? string.Empty;

    /// <summary>
    ///     Computes the lowercase hex SHA-256 digest of a file.
    ///     Returns a string of 64 zero characters if the file does not exist (so the
    ///     endpoint can still return a structurally valid response that will fail the
    ///     checksum check on the client side — useful for the ChecksumMismatch scenario).
    /// </summary>
    public static string ComputeSha256(string filePath)
    {
        if (!File.Exists(filePath))
            return new string('0', 64);

        using SHA256 sha = SHA256.Create();
        using FileStream fs = File.OpenRead(filePath);
        byte[] hash = sha.ComputeHash(fs);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    /// <summary>Builds the base URL (<c>http://host:port</c>) from the current request context.</summary>
    public static string BaseUrl(Microsoft.AspNetCore.Http.HttpContext ctx) =>
        $"{ctx.Request.Scheme}://{ctx.Request.Host}";
}
