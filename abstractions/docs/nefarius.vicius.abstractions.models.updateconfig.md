# UpdateConfig

Namespace: Nefarius.Vicius.Abstractions.Models

Update instance configuration. Parameters applying to the entire product/tenant.

```csharp
public sealed class UpdateConfig
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [UpdateConfig](./nefarius.vicius.abstractions.models.updateconfig.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md)

## Properties

### <a id="properties-emergencyurl"/>**EmergencyUrl**

The emergency URL. See https://docs.nefarius.at/projects/Vicius/Emergency-Feature/

```csharp
public string EmergencyUrl { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-errorfallbackurl"/>**ErrorFallbackUrl**

URL pointing to a help article opening when the user was presented with an error dialog.

```csharp
public string ErrorFallbackUrl { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-exitcode"/>**ExitCode**

Setup exit code parameters.

```csharp
public ExitCodeCheck ExitCode { get; set; }
```

#### Property Value

[ExitCodeCheck](./nefarius.vicius.abstractions.models.exitcodecheck.md)<br>

### <a id="properties-helpurl"/>**HelpUrl**

URL pointing to a help article opening when the user clicks the help button.

```csharp
public string HelpUrl { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-latestchecksum"/>**LatestChecksum**

Optional checksum of the self-updater binary at [UpdateConfig.LatestUrl](./nefarius.vicius.abstractions.models.updateconfig.md#latesturl).
 Passed to the self-updater DLL so it can verify the download before swapping.

```csharp
public ChecksumParameters LatestChecksum { get; set; }
```

#### Property Value

[ChecksumParameters](./nefarius.vicius.abstractions.models.checksumparameters.md)<br>

### <a id="properties-latesturl"/>**LatestUrl**

The URL to the latest updater binary. Redirects are supported.

```csharp
public string LatestUrl { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-latestversion"/>**LatestVersion**

The latest version of the updater binary.

```csharp
public Version LatestVersion { get; set; }
```

#### Property Value

[Version](https://learn.microsoft.com/dotnet/api/system.version)<br>

### <a id="properties-runasadmin"/>**RunAsAdmin**

When true, the downloaded setup is launched elevated (As Administrator) via the "runas" verb.

```csharp
public Nullable<Boolean> RunAsAdmin { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[Boolean](https://learn.microsoft.com/dotnet/api/system.boolean)><br>

**Remarks:**

Only honoured when strong authenticity conditions are satisfied (signature verification mode is
 [SignatureVerificationMode.Required](./nefarius.vicius.abstractions.models.signatureverificationmode.md#required) or --strict-verification is active). Ignored otherwise
 to prevent a compromised server from triggering an unauthenticated UAC prompt.

### <a id="properties-updatesdisabled"/>**UpdatesDisabled**

Updates are currently disabled, do not do anything, even if new versions are found.

```csharp
public Nullable<Boolean> UpdatesDisabled { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[Boolean](https://learn.microsoft.com/dotnet/api/system.boolean)><br>

## Constructors

### <a id="constructors-.ctor"/>**UpdateConfig()**

```csharp
public UpdateConfig()
```
