## ADDED Requirements

### Requirement: MaterialTable provides SOA material data access
The system SHALL provide a `MaterialTable` class that stores material indices in Structure of Arrays layout, accessible via a compact material handle.

#### Scenario: Create material table from existing material data
- **WHEN** a MaterialTable is constructed with material data containing baseColor, metallicRoughness, normal, occlusion, and emissive texture indices
- **THEN** the MaterialTable SHALL store each index type in a separate contiguous array
- **AND** the MaterialTable SHALL return a valid material handle (uint32_t)

#### Scenario: Access material data via handle
- **WHEN** GetBaseColorTextureIndex(handle) is called with a valid material handle
- **THEN** the system SHALL return the correct baseColor texture index stored in the SOA array

### Requirement: Triangle contains material handle instead of direct indices
The `Triangle` structure SHALL contain a single `materialId` field of type `uint32_t` instead of individual texture/material index fields.

#### Scenario: Triangle size reduction
- **WHEN** sizeof(Triangle) is measured after refactoring
- **THEN** the size SHALL be at least 40% smaller than the original Triangle structure

#### Scenario: Triangle references material via handle
- **WHEN** a Triangle is created with vertices and a materialId
- **THEN** the Triangle SHALL NOT contain any direct texture index fields
- **AND** all material data SHALL be accessible only via MaterialTable lookup

### Requirement: MaterialTable supports default material
The MaterialTable SHALL provide a default material (handle 0) with sensible defaults for all optional textures.

#### Scenario: Default material provides null texture indices
- **WHEN** GetBaseColorTextureIndex(0) is called on an empty MaterialTable
- **THEN** the system SHALL return -1 (invalid texture index)
- **AND** all other texture getters SHALL also return -1

### Requirement: MaterialTable supports material creation and deletion
The MaterialTable SHALL support adding new materials and removing existing materials with handle reuse.

#### Scenario: Add new material
- **WHEN** AddMaterial(params) is called with material parameters
- **THEN** the system SHALL return a new unique handle
- **AND** all material data SHALL be accessible via the returned handle

#### Scenario: Remove material and handle reuse
- **WHEN** RemoveMaterial(handle) is called
- **THEN** the handle SHALL be marked as invalid
- **AND** subsequent GetXXX(handle) calls SHALL return default values
- **AND** the handle MAY be reused for future AddMaterial calls
