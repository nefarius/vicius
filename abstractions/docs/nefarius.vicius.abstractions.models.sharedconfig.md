# SharedConfig

Namespace: Nefarius.Vicius.Abstractions.Models

Parameters that might be provided by both the server and the local configuration.

```csharp
public sealed class SharedConfig
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [SharedConfig](./nefarius.vicius.abstractions.models.sharedconfig.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md)

**Remarks:**

Keep in sync with [MergedConfig](./nefarius.vicius.abstractions.models.mergedconfig.md).

## Properties

### <a id="properties-detection"/>**Detection**

The details of the selected [SharedConfig.DetectionMethod](./nefarius.vicius.abstractions.models.sharedconfig.md#detectionmethod).

```csharp
public ProductVersionDetectionImplementation Detection { get; set; }
```

#### Property Value

[ProductVersionDetectionImplementation](./nefarius.vicius.abstractions.models.productversiondetectionimplementation.md)<br>

### <a id="properties-detectionmethod"/>**DetectionMethod**

The implementation to use to detect the locally installed product version to compare release versions against.

```csharp
public Nullable<ProductVersionDetectionMethod> DetectionMethod { get; private set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[ProductVersionDetectionMethod](./nefarius.vicius.abstractions.models.productversiondetectionmethod.md)><br>

### <a id="properties-downloadlocation"/>**DownloadLocation**

The preferred setup download directory.

```csharp
public DownloadLocationConfig DownloadLocation { get; set; }
```

#### Property Value

[DownloadLocationConfig](./nefarius.vicius.abstractions.models.downloadlocationconfig.md)<br>

**Remarks:**

By default, a temporary directory of the current user is used.

### <a id="properties-hideremindbutton"/>**HideRemindButton**

When true, the "Remind me tomorrow" button is hidden in the update UI.

```csharp
public Nullable<Boolean> HideRemindButton { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[Boolean](https://learn.microsoft.com/dotnet/api/system.boolean)><br>

### <a id="properties-installationerrorurl"/>**InstallationErrorUrl**

URL pointing to a help article opening when an update error occurred.

```csharp
public string InstallationErrorUrl { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-productname"/>**ProductName**

The product name displayed in the UI and dialogs.

```csharp
public string ProductName { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

### <a id="properties-runastemporarycopy"/>**RunAsTemporaryCopy**

Gets or sets whether the updater should run as a temporary copy instead from the origin directory.

```csharp
public Nullable<Boolean> RunAsTemporaryCopy { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[Boolean](https://learn.microsoft.com/dotnet/api/system.boolean)><br>

### <a id="properties-signatureconfig"/>**SignatureConfig**

Explicit certificate pin (used with [SignatureVerificationStrategy.FromConfiguration](./nefarius.vicius.abstractions.models.signatureverificationstrategy.md#fromconfiguration)).

```csharp
public SignatureConfig SignatureConfig { get; set; }
```

#### Property Value

[SignatureConfig](./nefarius.vicius.abstractions.models.signatureconfig.md)<br>

### <a id="properties-signaturepolicy"/>**SignaturePolicy**

Global Authenticode comparison policy.

```csharp
public Nullable<SignatureComparisonPolicy> SignaturePolicy { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[SignatureComparisonPolicy](./nefarius.vicius.abstractions.models.signaturecomparisonpolicy.md)><br>

### <a id="properties-signaturestrategy"/>**SignatureStrategy**

Global Authenticode pin strategy.

```csharp
public Nullable<SignatureVerificationStrategy> SignatureStrategy { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[SignatureVerificationStrategy](./nefarius.vicius.abstractions.models.signatureverificationstrategy.md)><br>

### <a id="properties-signatureverificationmode"/>**SignatureVerificationMode**

Global Authenticode verification mode for downloaded setups.

```csharp
public Nullable<SignatureVerificationMode> SignatureVerificationMode { get; set; }
```

#### Property Value

[Nullable](https://learn.microsoft.com/dotnet/api/system.nullable-1)<[SignatureVerificationMode](./nefarius.vicius.abstractions.models.signatureverificationmode.md)><br>

### <a id="properties-windowtitle"/>**WindowTitle**

The process window title visible in the taskbar.

```csharp
public string WindowTitle { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

## Constructors

### <a id="constructors-.ctor"/>**SharedConfig()**

```csharp
public SharedConfig()
```
