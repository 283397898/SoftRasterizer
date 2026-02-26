## ADDED Requirements

### Requirement: Compression module provides unified Zlib/Deflate decompression
The system SHALL provide a `Compression` module in `Utils/Compression.h/cpp` that implements Zlib and Deflate decompression, replacing duplicate implementations in ImageDecoder and EXRDecoder.

#### Scenario: Inflate Zlib compressed data
- **WHEN** `InflateZlib(compressedData, compressedSize, output)` is called with valid Zlib-wrapped compressed data
- **THEN** the system SHALL decompress the data into the output buffer
- **AND** the result SHALL be byte-identical to the previous ImageDecoder and EXRDecoder implementations

#### Scenario: Inflate raw Deflate data
- **WHEN** `InflateDeflate(compressedData, compressedSize, output)` is called with raw Deflate data (no Zlib header)
- **THEN** the system SHALL decompress the data without expecting Zlib wrapper bytes

#### Scenario: BitReader utility is shared
- **WHEN** any module needs bit-level reading from a byte stream
- **THEN** it SHALL use the `BitReader` class from `Utils/Compression.h`
- **AND** `BitReader` SHALL NOT be defined in any other source file

#### Scenario: Huffman table building is shared
- **WHEN** any module needs to build Huffman decoding tables
- **THEN** it SHALL use `BuildHuffmanTable()` from `Utils/Compression.h`
- **AND** `BuildHuffmanTable()` SHALL NOT be defined in any other source file

### Requirement: TextureSampler module provides unified texture sampling functions
The system SHALL provide a `TextureSampler` module in `Utils/TextureSampler.h` that contains all texture coordinate wrapping and sampling functions as inline functions.

#### Scenario: WrapCoord replaces duplicate implementations
- **WHEN** a rendering component needs to wrap texture coordinates according to a wrap mode
- **THEN** it SHALL call `WrapCoord(coord, wrapMode)` from `Utils/TextureSampler.h`
- **AND** the function SHALL accept `GLTFWrapMode` enum instead of raw integer values

#### Scenario: Nearest sampling
- **WHEN** `SampleNearest(image, sampler, u, v)` is called
- **THEN** the system SHALL return the texel at the nearest integer coordinate
- **AND** the result SHALL be identical to the previous FragmentShader implementation

#### Scenario: Bilinear sampling
- **WHEN** `SampleBilinear(image, sampler, u, v)` is called
- **THEN** the system SHALL return the bilinearly interpolated texel
- **AND** the result SHALL be identical to the previous FragmentShader implementation

#### Scenario: SampleBaseColorAlpha replaces Rasterizer duplicate
- **WHEN** the Rasterizer needs to sample base color alpha for alpha testing
- **THEN** it SHALL use sampling functions from `Utils/TextureSampler.h`
- **AND** Rasterizer.cpp SHALL NOT contain its own texture sampling implementations

### Requirement: MathUtils module provides shared interpolation and clamping functions
The system SHALL provide a `MathUtils` module in `Utils/MathUtils.h` containing inline interpolation and clamping utilities.

#### Scenario: Lerp for Vec2, Vec3, Vec4
- **WHEN** any module needs linear interpolation between two vectors
- **THEN** it SHALL use `Lerp(a, b, t)` from `Utils/MathUtils.h`
- **AND** overloads SHALL exist for `Vec2`, `Vec3`, `Vec4`, and `double`

#### Scenario: Lerp replaces Clipper duplicate
- **WHEN** the Clipper performs vertex attribute interpolation during clipping
- **THEN** it SHALL use `Lerp()` from `Utils/MathUtils.h`
- **AND** Clipper.cpp SHALL NOT define its own Lerp functions

#### Scenario: Clamp and Saturate utilities
- **WHEN** any module needs to clamp a value to a range
- **THEN** it SHALL use `Clamp(value, min, max)` or `Saturate(value)` from `Utils/MathUtils.h`

### Requirement: PBRUtils module provides shared PBR math functions
The system SHALL provide a `PBRUtils` module in `Utils/PBRUtils.h` containing inline PBR lighting calculation utilities.

#### Scenario: GeometrySmith replaces duplicate implementations
- **WHEN** any module needs the Smith geometry function for PBR shading
- **THEN** it SHALL use `GeometrySmith(NdotV, NdotL, roughness)` from `Utils/PBRUtils.h`
- **AND** FragmentShader.cpp and EnvironmentMap.cpp SHALL NOT define their own GeometrySmith

#### Scenario: FresnelSchlick is shared
- **WHEN** any module needs the Fresnel-Schlick approximation
- **THEN** it SHALL use `FresnelSchlick(cosTheta, F0)` from `Utils/PBRUtils.h`

#### Scenario: DistributionGGX is shared
- **WHEN** any module needs the GGX normal distribution function
- **THEN** it SHALL use `DistributionGGX(NdotH, roughness)` from `Utils/PBRUtils.h`
