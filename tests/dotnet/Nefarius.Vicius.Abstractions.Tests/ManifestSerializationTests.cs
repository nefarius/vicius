using System.Text.Json;
using System.Text.Json.Nodes;

using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Abstractions.Tests;

public sealed class ManifestSerializationTests
{
    [Fact]
    public void Optional_null_fields_are_omitted_and_empty_releases_are_kept()
    {
        string json = JsonSerializer.Serialize(new UpdateResponse(), ManifestJson.Options);
        JsonObject root = JsonNode.Parse(json)!.AsObject();

        Assert.True(root.ContainsKey("releases"));
        Assert.Empty(root["releases"]!.AsArray());
        Assert.False(root.ContainsKey("instance"));
        Assert.False(root.ContainsKey("shared"));
    }

    [Fact]
    public void Representative_manifest_uses_camel_case_string_enums_and_string_versions()
    {
        DateTimeOffset publishedAt = new(2024, 6, 1, 12, 0, 0, TimeSpan.Zero);
        UpdateResponse response = new()
        {
            Instance = new UpdateConfig
            {
                LatestVersion = Version.Parse("1.15.0"),
                HelpUrl = "https://docs.example.test/help"
            },
            Shared = new SharedConfig
            {
                ProductName = "Contoso",
                WindowTitle = "Contoso Updater",
                Detection = new RegistryValueConfig
                {
                    Hive = RegistryHive.HKLM,
                    Key = @"SOFTWARE\Contoso",
                    Value = "Version"
                },
                SignatureVerificationMode = SignatureVerificationMode.Required,
                SignaturePolicy = SignatureComparisonPolicy.Strict
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "Contoso 2.0.0",
                    Version = Version.Parse("2.0.0"),
                    Summary = "Notes",
                    PublishedAt = publishedAt,
                    DownloadUrl = "https://example.test/setup.exe",
                    DownloadSize = 42,
                    Checksum = new ChecksumParameters
                    {
                        Checksum = "abc",
                        ChecksumAlg = ChecksumAlgorithm.SHA256
                    },
                    ZipExtractDefaultFileDisposition = ZipExtractFileDisposition.CreateOrReplace
                }
            }
        };

        JsonObject root = JsonNode.Parse(JsonSerializer.Serialize(response, ManifestJson.Options))!.AsObject();

        Assert.Equal("1.15.0", root["instance"]?["latestVersion"]?.GetValue<string>());
        Assert.Equal("Contoso", root["shared"]?["productName"]?.GetValue<string>());
        Assert.Equal("RegistryValue", root["shared"]?["detectionMethod"]?.GetValue<string>());
        Assert.Equal("HKLM", root["shared"]?["detection"]?["hive"]?.GetValue<string>());
        Assert.Equal("Required", root["shared"]?["signatureVerificationMode"]?.GetValue<string>());
        Assert.Equal("Strict", root["shared"]?["signaturePolicy"]?.GetValue<string>());
        Assert.Equal("2.0.0", root["releases"]?[0]?["version"]?.GetValue<string>());
        Assert.Equal("2024-06-01T12:00:00Z", root["releases"]?[0]?["publishedAt"]?.GetValue<string>());
        Assert.Equal("SHA256", root["releases"]?[0]?["checksum"]?["checksumAlg"]?.GetValue<string>());
        Assert.Equal("CreateOrReplace", root["releases"]?[0]?["zipExtractDefaultFileDisposition"]?.GetValue<string>());
        Assert.Null(root["releases"]?[0]?["mirrorUrls"]);
        Assert.Null(root["releases"]?[0]?["disabled"]);
    }

    [Fact]
    public void Representative_manifest_round_trips_through_the_server_serializer()
    {
        UpdateResponse original = new()
        {
            Shared = new SharedConfig
            {
                ProductName = "Contoso",
                Detection = new FixedVersionConfig { Version = "0.0.1" }
            },
            Releases =
            {
                new UpdateRelease
                {
                    Name = "Contoso 2.0.0",
                    Version = Version.Parse("2.0.0"),
                    Summary = "Notes",
                    PublishedAt = new DateTimeOffset(2024, 6, 1, 12, 0, 0, TimeSpan.Zero),
                    DownloadUrl = "https://example.test/setup.exe",
                    ExitCode = new ExitCodeCheck
                    {
                        SuccessCodes = { 0, 3010 },
                        Messages = new Dictionary<string, ExitCodeMessage>
                        {
                            ["3010"] = new()
                            {
                                Message = "Reboot required",
                                IsSuccess = true
                            }
                        }
                    }
                }
            }
        };

        string json = JsonSerializer.Serialize(original, ManifestJson.Options);
        UpdateResponse? roundTripped = JsonSerializer.Deserialize<UpdateResponse>(json, ManifestJson.Options);

        Assert.NotNull(roundTripped);
        Assert.Equal("Contoso", roundTripped.Shared?.ProductName);
        Assert.Equal(ProductVersionDetectionMethod.FixedVersion, roundTripped.Shared?.DetectionMethod);
        FixedVersionConfig detection = Assert.IsType<FixedVersionConfig>(roundTripped.Shared?.Detection);
        Assert.Equal("0.0.1", detection.Version);
        UpdateRelease release = Assert.Single(roundTripped.Releases);
        Assert.Equal(new Version(2, 0, 0), release.Version);
        Assert.Equal(new DateTimeOffset(2024, 6, 1, 12, 0, 0, TimeSpan.Zero), release.PublishedAt);
        Assert.Equal([0, 3010], release.ExitCode?.SuccessCodes);
        Assert.True(release.ExitCode?.Messages?["3010"].IsSuccess);
    }

    [Theory]
    [InlineData(ChecksumAlgorithm.MD5, "MD5")]
    [InlineData(ChecksumAlgorithm.SHA1, "SHA1")]
    [InlineData(ChecksumAlgorithm.SHA256, "SHA256")]
    [InlineData(RegistryHive.HKCU, "HKCU")]
    [InlineData(RegistryHive.HKLM, "HKLM")]
    [InlineData(RegistryHive.HKCR, "HKCR")]
    [InlineData(RegistryView.Default, "Default")]
    [InlineData(RegistryView.WOW64_64KEY, "WOW64_64KEY")]
    [InlineData(RegistryView.WOW64_32KEY, "WOW64_32KEY")]
    [InlineData(VersionResource.FILEVERSION, "FILEVERSION")]
    [InlineData(VersionResource.PRODUCTVERSION, "PRODUCTVERSION")]
    [InlineData(ZipExtractFileDisposition.CreateIfAbsent, "CreateIfAbsent")]
    [InlineData(ZipExtractFileDisposition.CreateOrReplace, "CreateOrReplace")]
    [InlineData(ZipExtractFileDisposition.DeleteIfPresent, "DeleteIfPresent")]
    [InlineData(ProxyMode.System, "System")]
    [InlineData(ProxyMode.None, "None")]
    [InlineData(ProxyMode.Manual, "Manual")]
    [InlineData(IpFamily.Any, "Any")]
    [InlineData(IpFamily.V4, "V4")]
    [InlineData(IpFamily.V6, "V6")]
    [InlineData(Authority.Local, "Local")]
    [InlineData(Authority.Remote, "Remote")]
    [InlineData(SignatureComparisonPolicy.Relaxed, "Relaxed")]
    [InlineData(SignatureComparisonPolicy.Strict, "Strict")]
    [InlineData(SignatureVerificationStrategy.FromUpdaterBinary, "FromUpdaterBinary")]
    [InlineData(SignatureVerificationStrategy.FromConfiguration, "FromConfiguration")]
    [InlineData(SignatureVerificationMode.Disabled, "Disabled")]
    [InlineData(SignatureVerificationMode.WhenPresent, "WhenPresent")]
    [InlineData(SignatureVerificationMode.Required, "Required")]
    [InlineData(ProductVersionDetectionMethod.RegistryValue, "RegistryValue")]
    [InlineData(ProductVersionDetectionMethod.FileVersion, "FileVersion")]
    [InlineData(ProductVersionDetectionMethod.FileSize, "FileSize")]
    [InlineData(ProductVersionDetectionMethod.FileChecksum, "FileChecksum")]
    [InlineData(ProductVersionDetectionMethod.CustomExpression, "CustomExpression")]
    [InlineData(ProductVersionDetectionMethod.FixedVersion, "FixedVersion")]
    public void Enum_wire_values_are_the_member_names(Enum value, string expected)
    {
        string json = JsonSerializer.Serialize(value, value.GetType(), ManifestJson.Options);

        Assert.Equal($"\"{expected}\"", json);
        Assert.Equal(value, JsonSerializer.Deserialize(json, value.GetType(), ManifestJson.Options));
    }
}
