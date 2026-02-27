## ADDED Requirements

### Requirement: RenderPass abstract base class
The system SHALL provide a `RenderPass` abstract base class that defines the interface for render pipeline stages.

#### Scenario: Execute pass
- **WHEN** Execute(context) is called on a RenderPass
- **THEN** the pass SHALL perform its designated rendering operations using the provided RenderContext

#### Scenario: Conditional pass execution
- **WHEN** ShouldExecute(context) returns false for a RenderPass
- **THEN** the pipeline SHALL skip calling Execute() for that pass

### Requirement: PassBuilder constructs render pipelines
The system SHALL provide a `PassBuilder` class that allows fluent configuration of render pipelines.

#### Scenario: Add passes in order
- **WHEN** PassBuilder::AddPass(pass1).AddPass(pass2).AddPass(pass3) is called
- **THEN** the resulting pipeline SHALL execute passes in the order they were added (unless dependencies override)

#### Scenario: Build pipeline
- **WHEN** PassBuilder::Build() is called after adding passes
- **THEN** the system SHALL return a configured RenderPipeline instance
- **AND** the pipeline SHALL be ready to execute

### Requirement: Pass dependency management
The PassBuilder SHALL support declaring dependencies between passes to control execution order.

#### Scenario: Add dependency between passes
- **WHEN** AddDependency("shadow", "opaque") is called
- **THEN** the "opaque" pass SHALL NOT execute until "shadow" pass completes successfully

#### Scenario: Circular dependency detection
- **WHEN** AddDependency creates a circular dependency
- **THEN** the system SHALL report an error during Build()
- **AND** Build() SHALL return nullptr or throw an exception

### Requirement: Standard passes provided
The system SHALL provide standard RenderPass implementations for common rendering stages, each containing complete rendering logic.

#### Scenario: OpaquePass renders opaque geometry
- **WHEN** OpaquePass::Execute(context) is called
- **THEN** the pass SHALL perform material pre-registration in a single-threaded loop before geometry processing
- **AND** the pass SHALL perform geometry processing via GeometryProcessor for all opaque/mask draw items using pre-computed MaterialHandles
- **AND** the pass SHALL rasterize the resulting opaque triangles via Rasterizer
- **AND** the pass SHALL store deferred blend triangles in `context.deferredBlendTriangles`
- **AND** the pass SHALL NOT use any mutex or blocking lock during the parallel geometry processing phase

#### Scenario: TransparentPass renders transparent geometry
- **WHEN** TransparentPass::Execute(context) is called
- **THEN** the pass SHALL read deferred blend triangles from `context.deferredBlendTriangles`
- **AND** the pass SHALL rasterize them with correct back-to-front ordering
- **AND** the pass SHALL NOT use `SetDeferredTriangles()` pointer injection

#### Scenario: SkyboxPass renders environment map
- **WHEN** SkyboxPass::Execute(context) is called
- **THEN** the pass SHALL render the skybox by sampling the environment map for pixels with depth == 1.0
- **AND** the rendering logic SHALL be self-contained within the pass (not delegated to RenderPipeline)

#### Scenario: PostProcessPass applies post-processing
- **WHEN** PostProcessPass::Execute(context) is called
- **THEN** the pass SHALL apply FXAA, tone mapping, and sRGB conversion

### Requirement: RenderPipeline executes passes
The RenderPipeline SHALL execute all configured passes in dependency-resolved order, acting purely as a dispatcher.

#### Scenario: Execute all passes
- **WHEN** RenderPipeline::Render(context) is called
- **THEN** the pipeline SHALL clear buffers, then call ExecutePasses() to run all enabled passes
- **AND** the pipeline SHALL NOT contain rendering logic (geometry processing, rasterization, skybox rendering)

#### Scenario: RenderPipeline does not duplicate pass logic
- **WHEN** RenderPipeline source code is inspected
- **THEN** it SHALL NOT contain calls to GeometryProcessor, Rasterizer, or EnvironmentMap directly
- **AND** all such logic SHALL reside within individual Pass implementations

### Requirement: RenderContext carries inter-pass shared data
The `RenderContext` SHALL provide typed fields for data shared between passes, replacing `void* passData`.

#### Scenario: Deferred blend triangles passed via RenderContext
- **WHEN** OpaquePass produces deferred transparent triangles
- **THEN** it SHALL store them in `context.deferredBlendTriangles` (type: `std::vector<Triangle>*`)
- **AND** TransparentPass SHALL read from the same field

#### Scenario: MaterialTable shared via RenderContext
- **WHEN** multiple passes need access to the MaterialTable
- **THEN** the MaterialTable SHALL be accessible via `context.materialTable`
- **AND** it SHALL be created at the pipeline level and live for the entire frame

### Requirement: Renderer provides unified render path
The `Renderer` class SHALL extract common rendering logic into private helper methods shared by both `Render(const Scene&)` and `Render(const GPUScene&)`.

#### Scenario: Buffer clearing is unified
- **WHEN** either Render() overload is called
- **THEN** it SHALL call a single `ClearBuffers()` private method

#### Scenario: PassContext building is unified
- **WHEN** either Render() overload needs to build a PassContext
- **THEN** it SHALL call `BuildPassContext(frameContext)` private method
- **AND** only the FrameContext construction SHALL differ between the two overloads

#### Scenario: Stats logging is unified
- **WHEN** either Render() overload finishes
- **THEN** it SHALL call a single `LogFrameStats()` private method
