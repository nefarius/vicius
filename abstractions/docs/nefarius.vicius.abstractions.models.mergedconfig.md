# MergedConfig

Namespace: Nefarius.Vicius.Abstractions.Models

The shared configuration that has been merged with local and remote parameters.

```csharp
public sealed class MergedConfig
```

Inheritance [Object](https://learn.microsoft.com/dotnet/api/system.object) → [MergedConfig](./nefarius.vicius.abstractions.models.mergedconfig.md)<br>
Attributes [NullableContextAttribute](./system.runtime.compilerservices.nullablecontextattribute.md), [NullableAttribute](./system.runtime.compilerservices.nullableattribute.md), [RequiredMemberAttribute](https://learn.microsoft.com/dotnet/api/system.runtime.compilerservices.requiredmemberattribute)

**Remarks:**

Keep in sync with [SharedConfig](./nefarius.vicius.abstractions.models.sharedconfig.md).

## Properties

### <a id="properties-detection"/>**Detection**

The details of the selected [MergedConfig.DetectionMethod](./nefarius.vicius.abstractions.models.mergedconfig.md#detectionmethod).
 Setting this property automatically keeps [MergedConfig.DetectionMethod](./nefarius.vicius.abstractions.models.mergedconfig.md#detectionmethod) in sync.

```csharp
public ProductVersionDetectionImplementation Detection { get; set; }
```

#### Property Value

[ProductVersionDetectionImplementation](./nefarius.vicius.abstractions.models.productversiondetectionimplementation.md)<br>

### <a id="properties-detectionmethod"/>**DetectionMethod**

The implementation to use to detect the locally installed product version to compare release versions against.
 Derived automatically from [MergedConfig.Detection](./nefarius.vicius.abstractions.models.mergedconfig.md#detection); do not set independently.

```csharp
public ProductVersionDetectionMethod DetectionMethod { get; private set; }
```

#### Property Value

[ProductVersionDetectionMethod](./nefarius.vicius.abstractions.models.productversiondetectionmethod.md)<br>

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

When true, the "Remind me tomorrow" button is hidden in the update UI. Default: false.

```csharp
public bool HideRemindButton { get; set; }
```

#### Property Value

[Boolean](https://learn.microsoft.com/dotnet/api/system.boolean)<br>

### <a id="properties-iconbase64"/>**IconBase64**

Base64-encoded Windows .ico data used as the window and taskbar icon at runtime.

```csharp
public string IconBase64 { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

**Remarks:**

When set, the client decodes the .ico in memory and applies it via WM_SETICON. Absent or invalid values are silently ignored; the compiled-in icon resource is used as fallback.

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
public bool RunAsTemporaryCopy { get; set; }
```

#### Property Value

[Boolean](https://learn.microsoft.com/dotnet/api/system.boolean)<br>

### <a id="properties-signatureconfig"/>**SignatureConfig**

Explicit certificate pin (used with [SignatureVerificationStrategy.FromConfiguration](./nefarius.vicius.abstractions.models.signatureverificationstrategy.md#fromconfiguration)).

```csharp
public SignatureConfig SignatureConfig { get; set; }
```

#### Property Value

[SignatureConfig](./nefarius.vicius.abstractions.models.signatureconfig.md)<br>

### <a id="properties-signaturepolicy"/>**SignaturePolicy**

Authenticode comparison policy. Default: [SignatureComparisonPolicy.Relaxed](./nefarius.vicius.abstractions.models.signaturecomparisonpolicy.md#relaxed).

```csharp
public SignatureComparisonPolicy SignaturePolicy { get; set; }
```

#### Property Value

[SignatureComparisonPolicy](./nefarius.vicius.abstractions.models.signaturecomparisonpolicy.md)<br>

### <a id="properties-signaturestrategy"/>**SignatureStrategy**

Authenticode pin strategy. Default: [SignatureVerificationStrategy.FromUpdaterBinary](./nefarius.vicius.abstractions.models.signatureverificationstrategy.md#fromupdaterbinary).

```csharp
public SignatureVerificationStrategy SignatureStrategy { get; set; }
```

#### Property Value

[SignatureVerificationStrategy](./nefarius.vicius.abstractions.models.signatureverificationstrategy.md)<br>

### <a id="properties-signatureverificationmode"/>**SignatureVerificationMode**

Authenticode verification mode for downloaded setups. Default: [SignatureVerificationMode.WhenPresent](./nefarius.vicius.abstractions.models.signatureverificationmode.md#whenpresent).

```csharp
public SignatureVerificationMode SignatureVerificationMode { get; set; }
```

#### Property Value

[SignatureVerificationMode](./nefarius.vicius.abstractions.models.signatureverificationmode.md)<br>

### <a id="properties-windowtitle"/>**WindowTitle**

The process window title visible in the taskbar.

```csharp
public string WindowTitle { get; set; }
```

#### Property Value

[String](https://learn.microsoft.com/dotnet/api/system.string)<br>

## Constructors

### <a id="constructors-.ctor"/>**MergedConfig()**

#### Caution

Constructors of types with required members are not supported in this version of your compiler.

---

```csharp
public MergedConfig()
```
