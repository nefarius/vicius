<#
.SYNOPSIS
    Maps a Git tag ref to the four-part numeric version stamped into resources.

.DESCRIPTION
    Windows VERSIONINFO components are 16-bit integers, so the tag cannot be
    passed through as-is. Stable tags vX.Y.Z become X.Y.Z.0. Prerelease tags
    vX.Y.Z-preNNN become X.Y.Z.NNN after converting NNN to an integer
    (pre001 → 1). Sequence values outside 1..65535 are rejected.
#>

Set-StrictMode -Version Latest

function Get-ReleaseBuildVersion
{
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Ref
    )

    if ($Ref -notmatch '^refs/tags/(?<tag>.+)$')
    {
        throw "Ref '$Ref' is not a tag ref."
    }

    $tag = $Matches['tag']

    if ($tag -match '^v(?<major>\d+)\.(?<minor>\d+)\.(?<patch>\d+)$')
    {
        return "$($Matches['major']).$($Matches['minor']).$($Matches['patch']).0"
    }

    if ($tag -match '^v(?<major>\d+)\.(?<minor>\d+)\.(?<patch>\d+)-pre(?<pre>\d+)$')
    {
        try
        {
            $pre = [int64]$Matches['pre']
        }
        catch
        {
            throw "Prerelease sequence $($Matches['pre']) is outside the valid Windows version-component range 1..65535."
        }

        if ($pre -lt 1 -or $pre -gt 65535)
        {
            throw "Prerelease sequence $($Matches['pre']) is outside the valid Windows version-component range 1..65535."
        }

        return "$($Matches['major']).$($Matches['minor']).$($Matches['patch']).$pre"
    }

    throw "Unsupported release tag '$tag'. Expected vX.Y.Z or vX.Y.Z-preNNN."
}
