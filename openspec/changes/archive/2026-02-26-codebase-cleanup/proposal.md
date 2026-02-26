## Why

After the pipeline-architecture-optimization refactoring, the codebase contains legacy code, redundant implementations, and placeholder code that is no longer needed. The transition from the old material system to MaterialTable SOA, and from hardcoded pipeline to the Pass system, has left behind deprecated patterns and unused files. Cleaning up this technical debt will improve code maintainability, reduce confusion for future development, and ensure the new architecture is fully utilized.

## What Changes

### Removals
- **Empty implementation file**: Remove `PBRMaterial.cpp` (contains only include and empty namespace)
- **Redundant material system**: Evaluate and potentially remove `MaterialPool` if `MaterialTable` fully replaces it
- **Placeholder code**: Clean up unimplemented Pass methods with TODO comments
- **Commented code blocks**: Remove large commented-out code sections
- **Unused includes**: Audit and remove unnecessary header includes
- **Minimal implementations**: Consider inlining trivial implementations like `RendererConfig::Default()`

### Consolidations
- **Material management**: Unify material handling to use MaterialTable exclusively
- **Pass system**: Complete the transition from hardcoded RenderPipeline to Pass-based execution
- **Documentation**: Update code comments to reflect current architecture

## Capabilities

### New Capabilities
None - this is a cleanup effort that removes code rather than adding features.

### Modified Capabilities
- `material-table`: Clarify that MaterialTable is the primary material storage mechanism (may update spec to note MaterialPool deprecation)
- `pass-pipeline`: Update to note that placeholder implementations should be completed or removed

## Impact

### Files to Remove
- `SoftRenderer/src/Material/PBRMaterial.cpp` - Empty implementation file

### Files to Audit/Modify
- `SoftRenderer/include/Runtime/MaterialPool.h` - Evaluate for deprecation
- `SoftRenderer/src/Pipeline/OpaquePass.cpp` - Remove placeholder comments
- `SoftRenderer/include/Pipeline/FragmentShader.h` - Clean up forward declarations
- `SoftRenderer/src/Render/RendererConfig.cpp` - Consider inlining
- `SoftRenderer/include/Scene/Model.h` - Audit material includes

### Dependencies
- No external dependencies affected
- Internal: MaterialTable must be fully functional before removing MaterialPool

### Risk Assessment
- **Low risk**: Removing empty files and commented code
- **Medium risk**: Deprecating MaterialPool (requires verification that it's not used)
- **Minimal impact**: Changes are internal cleanup, no API changes for external users
