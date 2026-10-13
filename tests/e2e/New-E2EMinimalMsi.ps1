# Generates a per-user Windows Installer package that writes one HKCU value and exits 0.
# Used by the E2E harness so no MSI binary is committed. Dot-source and call New-E2EMinimalMsi.

function New-E2EMinimalMsi {
    param(
        [Parameter(Mandatory)]
        [string] $Path
    )

    $productCode = '{7E2A9C4B-1D55-4E8F-9A31-6C0B5F2E8D17}'
    $packageCode = '{C4D8E1A2-6B70-4F93-8E15-2A9D7C3B5F60}'
    $componentId = '{3B91E6D4-8A20-4C57-B1E4-90F6A2D85C33}'

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Force
    }

    $installer = New-Object -ComObject WindowsInstaller.Installer
    # msiOpenDatabaseModeCreate = 3
    $db = $installer.OpenDatabase($Path, 3)

    function Invoke-MsiSql {
        param([string] $Sql)
        $view = $db.OpenView($Sql)
        $view.Execute() | Out-Null
        $view.Close() | Out-Null
    }

    Invoke-MsiSql 'CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)'
    Invoke-MsiSql 'CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)'
    Invoke-MsiSql 'CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)'
    Invoke-MsiSql 'CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)'
    Invoke-MsiSql 'CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)'
    Invoke-MsiSql 'CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL, `Name` CHAR(255), `Value` CHAR(255), `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)'
    Invoke-MsiSql 'CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` LONG, `DiskPrompt` CHAR(64), `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)'
    Invoke-MsiSql 'CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)'

    Invoke-MsiSql "INSERT INTO ``Property`` (``Property``, ``Value``) VALUES ('ProductCode', '$productCode')"
    Invoke-MsiSql "INSERT INTO ``Property`` (``Property``, ``Value``) VALUES ('ProductName', 'Vicius E2E MSI')"
    Invoke-MsiSql "INSERT INTO ``Property`` (``Property``, ``Value``) VALUES ('ProductVersion', '1.0.0')"
    Invoke-MsiSql "INSERT INTO ``Property`` (``Property``, ``Value``) VALUES ('Manufacturer', 'Vicius')"
    Invoke-MsiSql "INSERT INTO ``Property`` (``Property``, ``Value``) VALUES ('ProductLanguage', '1033')"
    Invoke-MsiSql "INSERT INTO ``Property`` (``Property``, ``Value``) VALUES ('MSIINSTALLPERUSER', '1')"

    Invoke-MsiSql 'INSERT INTO `Directory` (`Directory`, `DefaultDir`) VALUES (''TARGETDIR'', ''SourceDir'')'
    Invoke-MsiSql 'INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES (''LocalAppDataFolder'', ''TARGETDIR'', ''.'')'
    Invoke-MsiSql 'INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES (''INSTALLDIR'', ''LocalAppDataFolder'', ''ViciusE2E'')'

    # Attributes 4 = registry key path. Root 1 = HKCU.
    Invoke-MsiSql "INSERT INTO ``Registry`` (``Registry``, ``Root``, ``Key``, ``Name``, ``Value``, ``Component_``) VALUES ('RegMarker', 1, 'Software\\ViciusE2E', 'Installed', '1', 'CompReg')"
    Invoke-MsiSql "INSERT INTO ``Component`` (``Component``, ``ComponentId``, ``Directory_``, ``Attributes``, ``KeyPath``) VALUES ('CompReg', '$componentId', 'INSTALLDIR', 4, 'RegMarker')"
    Invoke-MsiSql 'INSERT INTO `Feature` (`Feature`, `Title`, `Display`, `Level`, `Directory_`, `Attributes`) VALUES (''Main'', ''Main'', 1, 1, ''INSTALLDIR'', 0)'
    Invoke-MsiSql 'INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES (''Main'', ''CompReg'')'
    Invoke-MsiSql 'INSERT INTO `Media` (`DiskId`, `LastSequence`) VALUES (1, 0)'

    foreach ($action in @(
            @{ Name = 'CostInitialize'; Sequence = 800 },
            @{ Name = 'FileCost'; Sequence = 900 },
            @{ Name = 'CostFinalize'; Sequence = 1000 },
            @{ Name = 'InstallValidate'; Sequence = 1400 },
            @{ Name = 'InstallInitialize'; Sequence = 1500 },
            @{ Name = 'ProcessComponents'; Sequence = 1600 },
            @{ Name = 'UnpublishFeatures'; Sequence = 1800 },
            @{ Name = 'RemoveRegistryValues'; Sequence = 2600 },
            @{ Name = 'WriteRegistryValues'; Sequence = 5000 },
            @{ Name = 'RegisterUser'; Sequence = 6000 },
            @{ Name = 'RegisterProduct'; Sequence = 6100 },
            @{ Name = 'PublishFeatures'; Sequence = 6300 },
            @{ Name = 'PublishProduct'; Sequence = 6400 },
            @{ Name = 'InstallFinalize'; Sequence = 6600 }
        )) {
        Invoke-MsiSql "INSERT INTO ``InstallExecuteSequence`` (``Action``, ``Sequence``) VALUES ('$($action.Name)', $($action.Sequence))"
    }

    $summary = $db.GetType().InvokeMember('SummaryInformation', 'GetProperty', $null, $db, @(20))
    # PID_CODEPAGE, TITLE, SUBJECT, AUTHOR, TEMPLATE, REVNUMBER, PAGECOUNT (schema 500), WORDCOUNT, APPNAME, SECURITY
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(1, 1252)) | Out-Null
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(2, 'Vicius E2E MSI')) | Out-Null
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(3, 'E2E payload')) | Out-Null
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(4, 'vicius')) | Out-Null
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(7, 'Intel;1033')) | Out-Null
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(9, $packageCode)) | Out-Null
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(14, 500)) | Out-Null
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(15, 0)) | Out-Null
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(18, 'Windows Installer')) | Out-Null
    $summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @(19, 2)) | Out-Null
    $summary.GetType().InvokeMember('Persist', 'InvokeMethod', $null, $summary, $null) | Out-Null

    $db.GetType().InvokeMember('Commit', 'InvokeMethod', $null, $db, $null) | Out-Null

    return [pscustomobject]@{
        Path         = $Path
        ProductCode  = $productCode
    }
}
