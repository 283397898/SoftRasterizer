## MODIFIED Requirements

### Requirement: OpaquePass renders opaque geometry
The system SHALL provide standard RenderPass implementations for common rendering stages, each containing complete rendering logic.

#### Scenario: OpaquePass renders opaque geometry
- **WHEN** OpaquePass::Execute(context) is called
- **THEN** the pass SHALL perform material pre-registration in a single-threaded loop before geometry processing
- **AND** the pass SHALL perform geometry processing via GeometryProcessor for all opaque/mask draw items using pre-computed MaterialHandles
- **AND** the pass SHALL rasterize the resulting opaque triangles via Rasterizer
- **AND** the pass SHALL store deferred blend triangles in `context.deferredBlendTriangles`
- **AND** the pass SHALL NOT use any mutex or blocking lock during the parallel geometry processing phase
