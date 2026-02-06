#pragma once

#include <string>
#include <vector>

#include "Asset/GLTFTypes.h"

namespace SR {

struct GLTFAsset {
    std::vector<GLTFBuffer> buffers;
    std::vector<GLTFBufferView> bufferViews;
    std::vector<GLTFAccessor> accessors;
    std::vector<GLTFImage> images;
    std::vector<GLTFSampler> samplers;
    std::vector<GLTFTexture> textures;
    std::vector<GLTFMaterial> materials;
    std::vector<GLTFMesh> meshes;
    std::vector<GLTFNode> nodes;
    std::vector<GLTFScene> scenes;
    int defaultSceneIndex = -1;
    std::string generator;
};

} // namespace SR
