## MODIFIED Requirements

### Requirement: Tile-based parallel fragment shading
The Rasterizer SHALL support parallel execution of fragment shading across multiple tiles using OpenMP, and SHALL avoid high-contention global serialization points in tile bin construction and scheduling.

#### Scenario: Parallel tile shading with multiple threads
- **WHEN** ShadeTiles() is called with a list of tile tasks
- **THEN** the system SHALL distribute tiles across available OpenMP threads
- **AND** each thread SHALL independently shade its assigned tiles

#### Scenario: Dynamic load balancing
- **WHEN** tiles have varying complexity (different triangle counts)
- **THEN** the system SHALL use a tunable scheduling policy (`dynamic` or `guided`) to balance work across threads
- **AND** tuning chunk-size SHALL be supported to control scheduling overhead

### Requirement: Performance scaling with core count
The parallel shading system SHALL demonstrate stable multi-core performance scaling, and binning-related synchronization overhead MUST remain bounded under increasing thread counts.

#### Scenario: Performance improvement measurement
- **WHEN** comparing parallel shading (N threads) vs single-threaded shading
- **THEN** the speedup SHALL be at least 0.7 * N (70% parallel efficiency)

#### Scenario: Binning synchronization overhead bound
- **WHEN** rendering a representative scene at 1080p using 8-16 threads
- **THEN** the tile bin synchronization overhead SHALL NOT dominate frame time
- **AND** profiling output SHALL identify the binning phase cost explicitly for regression tracking