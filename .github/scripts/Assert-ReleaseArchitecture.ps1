<#
.SYNOPSIS
    Verifies that a release payload contains exactly the architectures vicius ships.

.DESCRIPTION
    Release assets are architecture-specific archives, but the executable inside
    keeps the plain "manufacturer_product_Updater.exe" name that NV_FILENAME_REGEX
    parses to build the update server URL. Nothing about the file name therefore
    reveals its architecture, which is how v1.16.0 ended up shipping 32-bit
    binaries under an architecture-neutral asset name.

    This script closes that hole by reading the PE header machine type of every
    executable and comparing it against the architecture claimed by its parent
    directory. Any 32-bit build, any unexpected architecture and any missing
    architecture is a hard failure.
#>

Set-StrictMode -Version Latest

# Architecture folder name -> required IMAGE_FILE_HEADER.Machine value.
# Adding an entry here is the only supported way to release a new architecture.
$script:ReleaseArchitectures = [ordered]@{
    'x64'   = 0x8664  # IMAGE_FILE_MACHINE_AMD64
    'ARM64' = 0xAA64  # IMAGE_FILE_MACHINE_ARM64
}

# Reported instead of a bare number so a failure names the offending architecture.
$script:MachineNames = @{
    0x014C = 'i386 (32-bit)'
    0x01C4 = 'ARM Thumb-2 (32-bit)'
    0x0200 = 'Itanium'
    0x8664 = 'AMD64'
    0xAA64 = 'ARM64'
}

function Get-PeMachineType
{
    <#
    .SYNOPSIS
        Returns the IMAGE_FILE_HEADER.Machine value of a PE image.
    #>
    [OutputType([int])]
    param(
        [Parameter(Mandatory)]
        [string] $Path
    )

    $stream = [System.IO.File]::OpenRead($Path)
    try
    {
        $reader = [System.IO.BinaryReader]::new($stream)

        if ($reader.ReadUInt16() -ne 0x5A4D)
        {
            throw "$Path is not a PE image (missing MZ signature)."
        }

        # e_lfanew holds the offset of the PE signature.
        $stream.Position = 0x3C
        $stream.Position = $reader.ReadUInt32()

        if ($reader.ReadUInt32() -ne 0x00004550)
        {
            throw "$Path is not a PE image (missing PE\0\0 signature)."
        }

        # Cast so hashtable lookups against the int-keyed name table match.
        return [int] $reader.ReadUInt16()
    }
    finally
    {
        $stream.Dispose()
    }
}

function Assert-ReleaseArchitecture
{
    <#
    .SYNOPSIS
        Throws unless $Root holds one directory per releasable architecture, each
        containing only correctly built executables.

    .PARAMETER Root
        Directory whose immediate subdirectories are named after an architecture.

    .PARAMETER UpdaterName
        Base name every executable must carry, e.g. "example_Demo_Updater".

    .PARAMETER RequireSigned
        Additionally require a valid Authenticode signature on every executable.
    #>
    param(
        [Parameter(Mandatory)]
        [string] $Root,

        [Parameter(Mandatory)]
        [string] $UpdaterName,

        [switch] $RequireSigned
    )

    if (-not (Test-Path -LiteralPath $Root))
    {
        throw "Release payload directory $Root does not exist."
    }

    $found = @(Get-ChildItem -LiteralPath $Root -Directory | Select-Object -ExpandProperty Name)
    $expected = @($script:ReleaseArchitectures.Keys)

    $unexpected = @($found | Where-Object { $_ -notin $expected })
    if ($unexpected.Count -gt 0)
    {
        $message = 'Release payload contains architectures that must never be published: ' +
                   "$($unexpected -join ', '). Expected exactly: $($expected -join ', ')."
        throw $message
    }

    $missing = @($expected | Where-Object { $_ -notin $found })
    if ($missing.Count -gt 0)
    {
        throw "Release payload is missing architectures: $($missing -join ', ')."
    }

    foreach ($arch in $expected)
    {
        $archRoot = Join-Path $Root $arch
        $files = @(Get-ChildItem -LiteralPath $archRoot -File -Recurse)

        $strays = @($files | Where-Object { $_.Extension -ne '.exe' })
        if ($strays.Count -gt 0)
        {
            $names = ($strays | Select-Object -ExpandProperty Name) -join ', '
            throw "Unexpected non-executable files in ${arch}: $names."
        }

        # The unsuffixed executable is what consumers rename and ship, so its
        # absence means the archive is unusable even if everything else passes.
        if (-not ($files | Where-Object { $_.Name -eq "$UpdaterName.exe" }))
        {
            throw "Architecture $arch is missing the canonical $UpdaterName.exe."
        }

        foreach ($file in $files)
        {
            if (-not $file.BaseName.StartsWith($UpdaterName, [StringComparison]::Ordinal))
            {
                throw "Executable $($file.Name) in $arch is not named after the updater ($UpdaterName)."
            }

            $machine = Get-PeMachineType -Path $file.FullName
            if ($machine -ne $script:ReleaseArchitectures[$arch])
            {
                $actual = if ($script:MachineNames.ContainsKey($machine)) { $script:MachineNames[$machine] } else { '0x{0:X4}' -f $machine }
                throw "$($file.FullName) is a $actual image but sits in the $arch payload."
            }

            if ($RequireSigned)
            {
                $sig = Get-AuthenticodeSignature -LiteralPath $file.FullName
                if ($sig.Status -ne 'Valid' -or -not $sig.SignerCertificate)
                {
                    throw "$($file.FullName) is not validly signed (Status=$($sig.Status))."
                }
            }

            Write-Output "OK: $($file.FullName) ($arch)"
        }
    }
}
