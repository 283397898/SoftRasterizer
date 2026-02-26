## ADDED Requirements

### Requirement: TextureSlot enum defines all supported texture types
The system SHALL provide a `TextureSlot` enum class that enumerates all supported texture binding slots.

#### Scenario: Enum contains all current texture types
- **WHEN** TextureSlot is defined
- **THEN** it SHALL contain entries for BaseColor, MetallicRoughness, Normal, Occlusion, Emissive, and Transmission
- **AND** it SHALL contain a `Count` sentinel value equal to the number of texture types

#### Scenario: Adding a new texture type
- **WHEN** a developer needs to support a new texture type (e.g., ClearCoat)
- **THEN** they SHALL only need to add one entry to TextureSlot before `Count`
- **AND** all structures using `TextureSlot::Count` for array sizing SHALL automatically accommodate the new type

### Requirement: TextureBinding struct encapsulates per-texture binding data
The system SHALL provide a `TextureBinding` struct that groups all data needed to bind a single texture.

#### Scenario: TextureBinding default state
- **WHEN** a TextureBinding is default-constructed
- **THEN** textureIndex SHALL be -1
- **AND** imageIndex SHALL be -1
- **AND** samplerIndex SHALL be -1
- **AND** texCoordSet SHALL be 0

#### Scenario: TextureBinding replaces scattered fields
- **WHEN** DrawItem or GPUSceneDrawItem references texture data
- **THEN** it SHALL use an array `TextureBinding textures[static_cast<size_t>(TextureSlot::Count)]`
- **AND** individual texture fields (baseColorTextureIndex, baseColorImageIndex, etc.) SHALL NOT exist

### Requirement: DrawItem uses TextureBinding array
The `DrawItem` struct SHALL replace its 24 individual texture index fields with a single TextureBinding array.

#### Scenario: Access base color texture binding
- **WHEN** rendering code needs the base color texture index from a DrawItem
- **THEN** it SHALL access `item.textures[TextureSlot::BaseColor].textureIndex`

#### Scenario: DrawItem field count reduction
- **WHEN** DrawItem is measured after refactoring
- **THEN** it SHALL have fewer than 30 named fields (down from 50+)

### Requirement: GPUSceneDrawItem uses TextureBinding array
The `GPUSceneDrawItem` struct SHALL replace its scattered texture index fields with a TextureBinding array, consistent with DrawItem.

#### Scenario: GPUSceneDrawItem mirrors DrawItem texture layout
- **WHEN** GPUSceneRenderQueueBuilder copies texture data from GPUSceneDrawItem to DrawItem
- **THEN** it SHALL copy the TextureBinding array directly
- **AND** no per-field mapping SHALL be required

#### Scenario: GPUScene construction initializes TextureBinding
- **WHEN** GPUScene builds a GPUSceneDrawItem from glTF data
- **THEN** it SHALL populate `textures[slot]` using the TextureSlot enum
- **AND** the initialization code SHALL use a loop or helper function instead of 24 individual assignments
