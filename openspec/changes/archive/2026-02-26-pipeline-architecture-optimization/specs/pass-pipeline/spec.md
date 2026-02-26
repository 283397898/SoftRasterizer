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
The system SHALL provide standard RenderPass implementations for common rendering stages.

#### Scenario: OpaquePass renders opaque geometry
- **WHEN** OpaquePass::Execute(context) is called
- **THEN** the pass SHALL render all opaque draw items from the render queue

#### Scenario: TransparentPass renders transparent geometry
- **WHEN** TransparentPass::Execute(context) is called
- **THEN** the pass SHALL render all transparent draw items with back-to-front sorting

#### Scenario: SkyboxPass renders environment map
- **WHEN** SkyboxPass::Execute(context) is called
- **THEN** the pass SHALL render the skybox/environment cubemap

#### Scenario: PostProcessPass applies post-processing
- **WHEN** PostProcessPass::Execute(context) is called
- **THEN** the pass SHALL apply FXAA, tone mapping, and sRGB conversion

### Requirement: RenderPipeline executes passes
The RenderPipeline SHALL execute all configured passes in dependency-resolved order.

#### Scenario: Execute all passes
- **WHEN** RenderPipeline::Render(context) is called
- **THEN** the pipeline SHALL execute all passes that have ShouldExecute() returning true
- **AND** the execution order SHALL respect declared dependencies
