## Context

The SoftRasterizer codebase has undergone significant architectural changes with the pipeline-architecture-optimization refactoring. The transition introduced:
- **MaterialTable SOA**: New cache-friendly material storage replacing per-triangle material data
- **Pass Pipeline System**: Configurable render passes with dependency resolution
- **ResourcePool<T>**: Generic resource management with LRU eviction

These changes have left behind legacy code, empty implementations, and transitional patterns that should be cleaned up to maintain code quality and reduce confusion.

## Goals / Non-Goals

**Goals:**
- Remove empty/unused source files that add no value
- Clean up placeholder code and TODO comments that are no longer relevant
- Consolidate material management to use MaterialTable exclusively
- Remove redundant data structures and code paths
- Improve code documentation to reflect current architecture

**Non-Goals:**
- No feature additions or API changes
- No performance optimizations (cleanup only)
- No refactoring of working code that doesn't need changes
- No changes to external interfaces or public APIs

## Decisions

### 1. Remove PBRMaterial.cpp
**Decision**: Delete the file entirely
**Rationale**: File contains only an include statement and empty namespace - no implementation exists
**Alternative considered**: Move implementation inline - rejected because PBRMaterial is a struct with only data members

### 2. MaterialPool vs MaterialTable
**Decision**: Keep MaterialPool for now, document its relationship to MaterialTable
**Rationale**:
- MaterialPool manages Material objects (AOS pattern) for scene-level material storage
- MaterialTable provides SOA storage for GPU-side material properties during rendering
- They serve different purposes and both are currently in use
**Alternative considered**: Remove MaterialPool - rejected because it's still used by GPUScene for material management

### 3. Placeholder Pass Implementations
**Decision**: Keep but document as "pass-through" implementations
**Rationale**:
- OpaquePass, SkyboxPass delegate to existing RenderPipeline methods
- This is intentional - Pass system provides extensibility while maintaining backward compatibility
- Full implementation would require significant refactoring of RenderPipeline
**Alternative considered**: Remove placeholder passes - rejected because they're needed for the Pass system to function

### 4. RendererConfig.cpp
**Decision**: Keep the file
**Rationale**:
- Contains Default() factory method
- Separating implementation from header allows future expansion without recompiling dependents
**Alternative considered**: Inline in header - rejected for maintainability

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Removing file breaks build | Verify file has no significant content before removal |
| MaterialPool removal breaks GPUScene | Keep MaterialPool, just clarify its role |
| Cleaning up comments removes useful context | Preserve architectural comments, remove only dead code |
| Pass-through implementations confuse developers | Add clear documentation explaining the design pattern |

## Migration Plan

1. **Phase 1: Safe Removals** - Remove clearly unused files and code
2. **Phase 2: Documentation** - Add clarifying comments to transitional code
3. **Phase 3: Verification** - Full build and runtime testing

No rollback needed - changes are additive (documentation) or remove dead code only.

## Open Questions

None - the cleanup scope is well-defined based on codebase exploration.
