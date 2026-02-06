#pragma once

#include <vector>

#include "Asset/GLTFAsset.h"

namespace SR {

class BufferAccessor {
public:
    template <typename T>
    std::vector<T> Read(const GLTFAsset& asset, const GLTFAccessor& accessor) const;
};

} // namespace SR

#include "Asset/BufferAccessor.inl"
