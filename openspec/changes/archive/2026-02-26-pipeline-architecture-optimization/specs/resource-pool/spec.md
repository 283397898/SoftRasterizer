## ADDED Requirements

### Requirement: ResourcePool template provides type-safe resource management
The system SHALL provide a `ResourcePool<T>` template class that manages resources of type T with handle-based access.

#### Scenario: Allocate resource
- **WHEN** Allocate(args...) is called with construction arguments
- **THEN** the system SHALL create a new resource of type T
- **AND** return a valid Handle (uint32_t) for accessing the resource

#### Scenario: Get resource by handle
- **WHEN** Get(handle) is called with a valid handle
- **THEN** the system SHALL return a pointer to the resource
- **AND** the pointer SHALL be valid until Release(handle) is called

#### Scenario: Get returns nullptr for invalid handle
- **WHEN** Get(invalidHandle) is called
- **THEN** the system SHALL return nullptr

### Requirement: ResourcePool supports resource release and handle reuse
The ResourcePool SHALL support releasing resources and reusing handles for new allocations.

#### Scenario: Release resource
- **WHEN** Release(handle) is called
- **THEN** the resource SHALL be destroyed
- **AND** the handle SHALL be marked as invalid

#### Scenario: Handle reuse
- **WHEN** a handle is released and a new resource is allocated
- **THEN** the system MAY reuse the released handle for the new resource

### Requirement: ResourcePool provides LRU eviction
The ResourcePool SHALL support LRU (Least Recently Used) eviction when memory budget is exceeded.

#### Scenario: Set memory budget
- **WHEN** SetMemoryBudget(bytes) is called
- **THEN** the system SHALL track total memory usage
- **AND** trigger eviction when budget is exceeded

#### Scenario: LRU eviction on allocation
- **WHEN** Allocate() would exceed the memory budget
- **THEN** the system SHALL evict the least recently used resources
- **AND** allocate the new resource if enough memory is freed

#### Scenario: Touch updates LRU order
- **WHEN** Get(handle) is called
- **THEN** the resource SHALL be marked as recently used
- **AND** it SHALL NOT be evicted before older resources

### Requirement: Specialized resource pools provided
The system SHALL provide specialized resource pool types for common resource types.

#### Scenario: TexturePool manages Texture resources
- **WHEN** TexturePool::Allocate(width, height, format) is called
- **THEN** the system SHALL create a Texture with the specified parameters
- **AND** return a TextureHandle for access

#### Scenario: MeshPool manages Mesh resources
- **WHEN** MeshPool::Allocate(vertexCount, indexCount) is called
- **THEN** the system SHALL create a Mesh with the specified capacity
- **AND** return a MeshHandle for access

#### Scenario: MaterialPool manages Material resources
- **WHEN** MaterialPool::Allocate(materialParams) is called
- **THEN** the system SHALL create a Material with the specified parameters
- **AND** return a MaterialHandle for access

### Requirement: ResourcePool provides memory statistics
The ResourcePool SHALL provide methods to query memory usage and resource count.

#### Scenario: Query memory usage
- **WHEN** GetMemoryUsage() is called
- **THEN** the system SHALL return the total memory used by all resources in bytes

#### Scenario: Query resource count
- **WHEN** GetResourceCount() is called
- **THEN** the system SHALL return the number of active (non-released) resources
