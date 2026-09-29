using System.Diagnostics.CodeAnalysis;

namespace Nefarius.Vicius.Example.Server.Services;

/// <summary>
///     Maps a <c>setup-v</c> GitHub tag onto the numeric version required by the updater manifest.
///     A well-formed SemVer pre-release or build suffix is discarded, so <c>setup-v3.0.0-r6</c> becomes <c>3.0.0</c>.
///     A malformed suffix is rejected.
/// </summary>
internal static class SetupReleaseTagParser
{
    private const string Prefix = "setup-v";

    public static bool TryParse(string tagName, [NotNullWhen(true)] out Version? version)
    {
        version = null;
        if (!tagName.StartsWith(Prefix, StringComparison.Ordinal))
            return false;

        string remainder = tagName[Prefix.Length..];
        int suffixIndex = remainder.IndexOfAny(['-', '+']);
        if (suffixIndex >= 0 && !IsValidSemVerSuffix(remainder.AsSpan(suffixIndex)))
            return false;

        string numeric = suffixIndex >= 0 ? remainder[..suffixIndex] : remainder;
        string[] components = numeric.Split('.');
        if (components.Length is not (3 or 4))
            return false;

        if (components.Any(component => component.Length > 1 && component[0] == '0'))
            return false;

        return Version.TryParse(numeric, out version);
    }

    /// <summary>
    ///     A suffix is either <c>-pre.release+build.meta</c> or <c>+build.meta</c>.
    ///     Identifiers are non-empty and contain only ASCII alphanumerics and hyphens.
    ///     Numeric pre-release identifiers cannot have leading zeroes. Build metadata can.
    /// </summary>
    private static bool IsValidSemVerSuffix(ReadOnlySpan<char> suffix)
    {
        if (suffix.Length < 2 || suffix[0] is not ('-' or '+'))
            return false;

        ReadOnlySpan<char> remaining = suffix[1..];
        if (suffix[0] == '-')
        {
            int buildIndex = remaining.IndexOf('+');
            ReadOnlySpan<char> prerelease = buildIndex >= 0 ? remaining[..buildIndex] : remaining;
            if (!HasValidSemVerIdentifiers(prerelease, allowNumericLeadingZeroes: false))
                return false;

            if (buildIndex < 0)
                return true;

            remaining = remaining[(buildIndex + 1)..];
        }

        return HasValidSemVerIdentifiers(remaining, allowNumericLeadingZeroes: true);
    }

    private static bool HasValidSemVerIdentifiers(ReadOnlySpan<char> value, bool allowNumericLeadingZeroes)
    {
        if (value.IsEmpty)
            return false;

        int start = 0;
        while (true)
        {
            int relativeDot = value[start..].IndexOf('.');
            int end = relativeDot < 0 ? value.Length : start + relativeDot;
            if (!IsValidSemVerIdentifier(value[start..end], allowNumericLeadingZeroes))
                return false;

            if (relativeDot < 0)
                return true;

            start = end + 1;
        }
    }

    private static bool IsValidSemVerIdentifier(ReadOnlySpan<char> identifier, bool allowNumericLeadingZeroes)
    {
        if (identifier.IsEmpty)
            return false;

        bool numeric = true;
        foreach (char character in identifier)
        {
            if (character is >= '0' and <= '9')
                continue;

            numeric = false;
            if (character is (>= 'A' and <= 'Z') or (>= 'a' and <= 'z') or '-')
                continue;

            return false;
        }

        return allowNumericLeadingZeroes || !numeric || identifier.Length == 1 || identifier[0] != '0';
    }
}
