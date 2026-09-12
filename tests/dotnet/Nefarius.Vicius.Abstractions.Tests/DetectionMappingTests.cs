using Nefarius.Vicius.Abstractions.Models;

namespace Nefarius.Vicius.Abstractions.Tests;

public sealed class DetectionMappingTests
{
    [Fact]
    public void SharedConfig_maps_each_detection_implementation_to_its_method()
    {
        SharedConfig shared = new();

        shared.Detection = new RegistryValueConfig
        {
            Hive = RegistryHive.HKLM,
            Key = @"SOFTWARE\Contoso",
            Value = "Version"
        };
        Assert.Equal(ProductVersionDetectionMethod.RegistryValue, shared.DetectionMethod);

        shared.Detection = new FileVersionConfig { Input = @"C:\App\app.exe" };
        Assert.Equal(ProductVersionDetectionMethod.FileVersion, shared.DetectionMethod);

        shared.Detection = new FileSizeConfig { Input = @"C:\App\app.exe" };
        Assert.Equal(ProductVersionDetectionMethod.FileSize, shared.DetectionMethod);

        shared.Detection = new FileChecksumConfig { Input = @"C:\App\app.exe" };
        Assert.Equal(ProductVersionDetectionMethod.FileChecksum, shared.DetectionMethod);

        shared.Detection = new CustomExpressionConfig { Input = "{{ version }}" };
        Assert.Equal(ProductVersionDetectionMethod.CustomExpression, shared.DetectionMethod);

        shared.Detection = new FixedVersionConfig { Version = "1.2.3" };
        Assert.Equal(ProductVersionDetectionMethod.FixedVersion, shared.DetectionMethod);

        shared.Detection = null;
        Assert.Null(shared.DetectionMethod);
    }
}
