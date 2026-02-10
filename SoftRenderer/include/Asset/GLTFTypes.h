#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace SR {

struct GLTFBuffer {
    std::vector<uint8_t> data;
};

struct GLTFBufferView {
    int bufferIndex = -1;
    size_t byteOffset = 0;
    size_t byteLength = 0;
    size_t byteStride = 0;
    int target = 0;
};

struct GLTFAccessor {
    int bufferViewIndex = -1;
    size_t byteOffset = 0;
    size_t count = 0;
    int componentType = 0;
    int type = 0;
    bool normalized = false;
    std::vector<double> minValues;
    std::vector<double> maxValues;
};

struct GLTFImage {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int channels = 0;
    bool isSRGB = false;
    std::string mimeType;
};

struct GLTFSampler {
    int wrapS = 0;
    int wrapT = 0;
    int minFilter = 0;
    int magFilter = 0;
};

struct GLTFTexture {
    int imageIndex = -1;
    int samplerIndex = -1;
};

struct GLTFTextureInfo {
    int textureIndex = -1;
    int texCoord = 0;
    double scale = 1.0;
    double strength = 1.0;
};

struct GLTFPBRMetallicRoughness {
    double baseColorFactor[4] = {1.0, 1.0, 1.0, 1.0};
    GLTFTextureInfo baseColorTexture{};
    double metallicFactor = 1.0;
    double roughnessFactor = 1.0;
    GLTFTextureInfo metallicRoughnessTexture{};
};

struct GLTFMaterial {
    std::string name;
    GLTFPBRMetallicRoughness pbr{};
    GLTFTextureInfo normalTexture{};
    GLTFTextureInfo occlusionTexture{};
    GLTFTextureInfo emissiveTexture{};
    struct {
        double transmissionFactor = 0.0;
        GLTFTextureInfo transmissionTexture{};
        bool hasTransmission = false;
    } transmission{};
    struct {
        double ior = 1.5;
        bool hasIOR = false;
    } iorExt{};
    struct {
        double specularFactor = 1.0;
        double specularColorFactor[3] = {1.0, 1.0, 1.0};
        GLTFTextureInfo specularTexture{};
        GLTFTextureInfo specularColorTexture{};
        bool hasSpecular = false;
    } specular{};
    double emissiveFactor[3] = {0.0, 0.0, 0.0};
    int alphaMode = 0;
    double alphaCutoff = 0.5;
    bool doubleSided = false;
};

struct GLTFPrimitive {
    int materialIndex = -1;
    int indices = -1;
    int mode = 4;
    std::unordered_map<std::string, int> attributes;
};

struct GLTFMesh {
    std::string name;
    std::vector<GLTFPrimitive> primitives;
};

struct GLTFNode {
    int meshIndex = -1;
    std::vector<int> children;
    double translation[3] = {0.0, 0.0, 0.0};
    double rotation[4] = {0.0, 0.0, 0.0, 1.0};
    double scale[3] = {1.0, 1.0, 1.0};
    bool hasMatrix = false;
    double matrix[16] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
};

struct GLTFScene {
    std::vector<int> rootNodes;
};

} // namespace SR
