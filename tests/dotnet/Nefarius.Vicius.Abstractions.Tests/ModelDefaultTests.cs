using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Abstractions.Tests;

public sealed class ModelDefaultTests
{
    [Fact]
    public void UpdateResponse_starts_with_empty_releases_and_no_optional_sections()
    {
        UpdateResponse response = new();

        Assert.NotNull(response.Releases);
        Assert.Empty(response.Releases);
        Assert.Null(response.Instance);
        Assert.Null(response.Shared);
    }

    [Fact]
    public void ExitCodeCheck_defaults_to_enforced_check_with_empty_success_codes()
    {
        ExitCodeCheck check = new();

        Assert.False(check.SkipCheck);
        Assert.NotNull(check.SuccessCodes);
        Assert.Empty(check.SuccessCodes);
        Assert.Null(check.Messages);
    }

    [Fact]
    public void NetworkConfig_defaults_to_system_proxy_and_any_ip_family()
    {
        NetworkConfig network = new();

        Assert.Equal(ProxyMode.System, network.ProxyMode);
        Assert.Equal(IpFamily.Any, network.IpFamily);
        Assert.Null(network.ProxyUrl);
        Assert.Null(network.DohUrl);
        Assert.Null(network.PinnedHosts);
    }

    [Fact]
    public void PinnedHost_defaults_port_to_443()
    {
        PinnedHost pin = new() { Host = "vicius.example.test", Address = "127.0.0.1" };

        Assert.Equal(443, pin.Port);
    }

    [Fact]
    public void InstanceConfig_defaults_authority_to_remote()
    {
        InstanceConfig config = new()
        {
            ServerUrlTemplate = "https://example.test/{{ product }}/updates.json",
            FilenameRegex = @".+\.msi$"
        };

        Assert.Equal(Authority.Remote, config.Authority);
        Assert.Null(config.Network);
        Assert.Null(config.FallbackServerUrlTemplates);
    }

    [Fact]
    public void RegistryValueConfig_defaults_view_to_default()
    {
        RegistryValueConfig detection = new()
        {
            Hive = RegistryHive.HKLM,
            Key = @"SOFTWARE\Contoso",
            Value = "Version"
        };

        Assert.Equal(RegistryView.Default, detection.View);
    }

    [Fact]
    public void FileVersionConfig_defaults_statement_to_product_version()
    {
        FileVersionConfig detection = new() { Input = @"C:\App\app.exe" };

        Assert.Equal(VersionResource.PRODUCTVERSION, detection.Statement);
        Assert.Null(detection.Data);
    }
}
