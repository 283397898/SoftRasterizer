## Requirements

### Requirement: OpenMP scheduling policy SHALL be configurable per hotspot stage
The renderer SHALL provide configurable OpenMP scheduling policy and chunk-size for performance-critical stages without changing rendering correctness.

#### Scenario: Configure scheduling for raster hotspot
- **WHEN** the runtime or build-time OpenMP tuning configuration is provided for raster hotspot loops
- **THEN** the system SHALL apply the configured policy (`static`/`dynamic`/`guided`) and chunk-size
- **AND** the final color and depth outputs SHALL remain bitwise-consistent within the same execution mode constraints

#### Scenario: Fallback to safe defaults
- **WHEN** no tuning configuration is provided or provided values are invalid
- **THEN** the system SHALL fallback to validated default scheduling parameters
- **AND** rendering SHALL continue without failure

### Requirement: OpenMP hotspot metrics SHALL be observable
The renderer SHALL expose stage-level timing and workload metrics for OpenMP hotspots to support tuning and regression detection.

#### Scenario: Stage timing output
- **WHEN** a frame is rendered with profiling enabled
- **THEN** the system SHALL report timing for at least geometry build, bin construction, and tile raster stages
- **AND** reported units SHALL be milliseconds

#### Scenario: Regression guard
- **WHEN** a benchmark run compares baseline and optimized builds
- **THEN** the system SHALL provide enough metrics to determine whether frame time regression exceeds a configured threshold

### Requirement: OpenMP tuning MUST preserve thread safety
All OpenMP tuning changes MUST preserve existing data-race safety guarantees in shading and binning paths.

#### Scenario: Tile-exclusive writes
- **WHEN** multiple OpenMP threads execute tile raster in parallel
- **THEN** each tile's color/depth write region SHALL be exclusively owned by one thread at a time
- **AND** no pixel-level mutex SHALL be required

#### Scenario: Global counters consistency
- **WHEN** per-thread statistics are merged into frame-level counters
- **THEN** the merge operation SHALL use thread-safe mechanisms (atomic or equivalent reduction)
- **AND** the final counters SHALL be deterministic for identical inputs and thread count
