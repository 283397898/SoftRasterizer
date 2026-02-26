## ADDED Requirements

### Requirement: ResourcePool template provides type-safe resource management
The system SHALL provide a `ResourcePool<T>` template class that manages resources of type T with generational handle-based access.

#### Scenario: Allocate resource
- **WHEN** Allocate(args...) is called with construction arguments
- **THEN** the system SHALL create a new resource of type T
- **AND** return a valid Handle containing both index and generation

#### Scenario: Get resource by handle with generation check
- **WHEN** Get(handle) is called with a handle whose generation matches the current generation for that slot
- **THEN** the system SHALL return a pointer to the resource

#### Scenario: Get returns nullptr for stale handle
- **WHEN** Get(handle) is called with a handle whose generation does NOT match (stale/reused slot)
- **THEN** the system SHALL return nullptr
- **AND** the system SHALL NOT access the resource at that slot

#### Scenario: Handle reuse increments generation
- **WHEN** a resource is released and the slot is reused for a new allocation
- **THEN** the new handle's generation SHALL be greater than the old handle's generation
- **AND** the old handle SHALL fail the generation check in IsValid()

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
The ResourcePool SHALL support LRU (Least Recently Used) eviction with O(1) time complexity for all LRU operations.

#### Scenario: Set memory budget
- **WHEN** SetMemoryBudget(bytes) is called
- **THEN** the system SHALL track total memory usage
- **AND** trigger eviction when budget is exceeded

#### Scenario: LRU eviction on allocation
- **WHEN** Allocate() would exceed the memory budget
- **THEN** the system SHALL evict the least recently used resources
- **AND** allocate the new resource if enough memory is freed

#### Scenario: Touch updates LRU order in O(1)
- **WHEN** Get(handle) is called on a valid resource
- **THEN** the resource SHALL be moved to most-recently-used position in O(1) time
- **AND** the implementation SHALL use a doubly-linked list with iterator map (not linear search)

#### Scenario: Release removes from LRU in O(1)
- **WHEN** Release(handle) is called
- **THEN** the resource SHALL be removed from the LRU list in O(1) time

### Requirement: Handle is a composite type with index and generation
The ResourcePool Handle SHALL encode both a slot index and a generation counter.

#### Scenario: Handle encodes index and generation
- **WHEN** a Handle is created
- **THEN** it SHALL contain a 16-bit index and a 16-bit generation counter
- **AND** the total size SHALL be 32 bits (uint32_t compatible)

#### Scenario: InvalidHandle is well-defined
- **WHEN** `ResourcePool::InvalidHandle` is checked
- **THEN** it SHALL have index == 0xFFFF and generation == 0xFFFF
- **AND** IsValid(InvalidHandle) SHALL return false

#### Scenario: Handle comparison
- **WHEN** two Handles are compared
- **THEN** equality SHALL require both index and generation to match

### Requirement: ResourcePool uses vector<uint8_t> instead of vector<bool>
The alive flags SHALL use `std::vector<uint8_t>` instead of `std::vector<bool>` to avoid the bit-packing specialization.

#### Scenario: Alive flag access is not bit-packed
- **WHEN** IsValid() checks the alive status of a handle
- **THEN** it SHALL read from a `std::vector<uint8_t>` (not `std::vector<bool>`)
- **AND** each alive flag SHALL occupy a full byte for cache-friendly access

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
