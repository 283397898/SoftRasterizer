## ADDED Requirements

### Requirement: GLTFWrapMode enum replaces wrap mode magic numbers
The system SHALL provide a `GLTFWrapMode` enum class in `Asset/GLTFTypes.h` with values matching the glTF specification.

#### Scenario: Enum values match glTF constants
- **WHEN** GLTFWrapMode is defined
- **THEN** it SHALL contain `ClampToEdge = 33071`, `MirroredRepeat = 33648`, and `Repeat = 10497`

#### Scenario: WrapCoord accepts GLTFWrapMode
- **WHEN** `WrapCoord(coord, wrapMode)` is called
- **THEN** the wrapMode parameter SHALL be of type `GLTFWrapMode`
- **AND** the function SHALL NOT accept raw integer values

#### Scenario: No raw wrap mode integers in Pipeline code
- **WHEN** Rasterizer.cpp or FragmentShader.cpp performs texture coordinate wrapping
- **THEN** the code SHALL NOT contain literal values 33071, 33648, or 10497

### Requirement: GLTFAlphaMode enum replaces alpha mode magic numbers
The system SHALL provide a `GLTFAlphaMode` enum class in `Asset/GLTFTypes.h`.

#### Scenario: Enum values match convention
- **WHEN** GLTFAlphaMode is defined
- **THEN** it SHALL contain `Opaque = 0`, `Mask = 1`, and `Blend = 2`

#### Scenario: Alpha mode comparisons use enum
- **WHEN** rendering code checks alpha mode for sorting or branching
- **THEN** it SHALL compare against `GLTFAlphaMode::Blend`, `GLTFAlphaMode::Opaque`, etc.
- **AND** the code SHALL NOT contain literal values 0, 1, or 2 for alpha mode comparison

#### Scenario: PBRMaterial stores GLTFAlphaMode
- **WHEN** PBRMaterial.alphaMode is accessed
- **THEN** its type SHALL be `GLTFAlphaMode` (or `int` with documented enum semantics at minimum)

### Requirement: GLTFFilterMode enum replaces filter mode magic numbers
The system SHALL provide a `GLTFFilterMode` enum class in `Asset/GLTFTypes.h`.

#### Scenario: Enum values match glTF constants
- **WHEN** GLTFFilterMode is defined
- **THEN** it SHALL contain at minimum `Nearest = 9728` and `Linear = 9729`

#### Scenario: Sampler configuration uses GLTFFilterMode
- **WHEN** a texture sampler is configured with min/mag filter
- **THEN** the filter mode SHALL be specified using `GLTFFilterMode` instead of raw integers

### Requirement: GLTFComponentType enum replaces component type magic numbers
The system SHALL provide a `GLTFComponentType` enum class in `Asset/GLTFTypes.h`.

#### Scenario: Enum values match glTF constants
- **WHEN** GLTFComponentType is defined
- **THEN** it SHALL contain `Byte = 5120`, `UnsignedByte = 5121`, `Short = 5122`, `UnsignedShort = 5123`, `UnsignedInt = 5125`, and `Float = 5126`

#### Scenario: Buffer accessor uses GLTFComponentType
- **WHEN** glTF buffer data is read according to component type
- **THEN** the code SHALL use `GLTFComponentType` for type dispatch
- **AND** the code SHALL NOT contain raw values 5120-5126
