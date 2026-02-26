## ADDED Requirements

### Requirement: Tile-based parallel fragment shading
The Rasterizer SHALL support parallel execution of fragment shading across multiple tiles using OpenMP.

#### Scenario: Parallel tile shading with multiple threads
- **WHEN** ShadeTiles() is called with a list of tile tasks
- **THEN** the system SHALL distribute tiles across available OpenMP threads
- **AND** each thread SHALL independently shade its assigned tiles

#### Scenario: Dynamic load balancing
- **WHEN** tiles have varying complexity (different triangle counts)
- **THEN** the system SHALL use schedule(dynamic) to balance work across threads

### Requirement: TileShadingTask encapsulates shading work
The system SHALL provide a `TileShadingTask` structure that encapsulates all data needed to shade a tile.

#### Scenario: TileShadingTask contains tile coordinates and triangles
- **WHEN** a TileShadingTask is created for a tile
- **THEN** it SHALL contain the tile's screen coordinates (x, y, width, height)
- **AND** it SHALL contain references to triangles overlapping the tile
- **AND** it SHALL contain material table reference

### Requirement: Thread-safe framebuffer access
The Rasterizer SHALL ensure thread-safe writes to the framebuffer during parallel shading.

#### Scenario: No false sharing in framebuffer
- **WHEN** multiple threads write to adjacent pixels
- **THEN** the system SHALL ensure no cache line false sharing occurs
- **AND** the final framebuffer SHALL contain correct pixel values

#### Scenario: Thread-local tile buffer
- **WHEN** a thread shades a tile
- **THEN** the thread MAY use a thread-local buffer for intermediate results
- **AND** the final results SHALL be committed to the global framebuffer atomically if needed

### Requirement: Performance scaling with core count
The parallel shading system SHALL demonstrate near-linear performance scaling with CPU core count.

#### Scenario: Performance improvement measurement
- **WHEN** comparing parallel shading (N threads) vs single-threaded shading
- **THEN** the speedup SHALL be at least 0.7 * N (70% parallel efficiency)

#### Scenario: Overhead is acceptable
- **WHEN** rendering a simple scene with few triangles
- **THEN** the parallel overhead SHALL be less than 10% of total shading time

### Requirement: Early-Z preserved in parallel mode
The Early-Z optimization SHALL continue to function correctly in parallel shading mode.

#### Scenario: Early-Z culling in parallel
- **WHEN** a fragment fails the depth test
- **THEN** the fragment shader SHALL NOT be invoked
- **AND** this optimization SHALL work correctly with parallel execution
