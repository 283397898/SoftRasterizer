#pragma once

#include "Runtime/GPUScene.h"

namespace SR {

class ObjectGroup;

class GPUSceneBuilder {
public:
    void BuildFromObjectGroup(const ObjectGroup& objects, GPUScene& outScene) const;
};

} // namespace SR
