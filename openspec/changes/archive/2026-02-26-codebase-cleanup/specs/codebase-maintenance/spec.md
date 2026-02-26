## ADDED Requirements

### Requirement: Empty source files shall be removed
The codebase SHALL NOT contain source files with empty or trivial implementations (only includes and empty namespaces).

#### Scenario: Remove PBRMaterial.cpp
- **WHEN** a .cpp file contains only include statements and an empty namespace
- **THEN** the file SHALL be removed from the project

### Requirement: Material management roles shall be documented
The relationship between MaterialPool and MaterialTable SHALL be clearly documented to prevent confusion about their respective roles.

#### Scenario: Document MaterialPool purpose
- **WHEN** a developer reads MaterialPool.h
- **THEN** they SHALL see documentation explaining it manages Material objects for scene-level storage

#### Scenario: Document MaterialTable purpose
- **WHEN** a developer reads MaterialTable.h
- **THEN** they SHALL see documentation explaining it provides SOA storage for GPU-side material properties during rendering

### Requirement: Pass-through implementations shall be documented
Render passes that delegate to existing RenderPipeline methods SHALL have clear documentation explaining this design pattern.

#### Scenario: Document OpaquePass delegation
- **WHEN** a developer reads OpaquePass implementation
- **THEN** they SHALL see comments explaining the pass-through pattern and its rationale

### Requirement: Build configuration shall be updated for removed files
When source files are removed, the CMakeLists.txt SHALL be updated to remove references to those files.

#### Scenario: Update CMakeLists for PBRMaterial removal
- **WHEN** PBRMaterial.cpp is removed
- **THEN** CMakeLists.txt SHALL NOT contain a reference to src/Material/PBRMaterial.cpp
