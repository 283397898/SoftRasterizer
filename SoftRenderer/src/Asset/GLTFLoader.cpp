#include "Asset/GLTFLoader.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdio>
#if defined(_WIN32)
#include <windows.h>
#endif

#include "Asset/JSONParser.h"
#include "Asset/ImageDecoder.h"

namespace SR {

namespace {

struct DataUriResult {
    std::vector<uint8_t> data;
    std::string mimeType;
};

/**
 * @brief 读取二进制文件到字节数组
 */
bool ReadFileBinary(const std::filesystem::path& path, std::vector<uint8_t>& outData, std::string& outError) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        outError = "Failed to open file: " + path.string();
        return false;
    }
    auto size = file.tellg();
    if (size <= 0) {
        outError = "File is empty: " + path.string();
        return false;
    }
    outData.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(outData.data()), size)) {
        outError = "Failed to read file: " + path.string();
        return false;
    }
    return true;
}

/**
 * @brief 读取文本文件内容
 */
bool ReadFileText(const std::filesystem::path& path, std::string& outText, std::string& outError) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outError = "Failed to open file: " + path.string();
        return false;
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    outText = oss.str();
    if (outText.empty()) {
        outError = "File is empty: " + path.string();
        return false;
    }
    return true;
}

uint32_t ReadU32LE(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8) |
        (static_cast<uint32_t>(data[2]) << 16) |
        (static_cast<uint32_t>(data[3]) << 24);
}

bool DecodeBase64(const std::string& input, std::vector<uint8_t>& outData) {
    static constexpr uint8_t kInvalid = 0xFF;
    static const uint8_t kTable[256] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,62  ,0xFF,0xFF,0xFF,63  ,
        52  ,53  ,54  ,55  ,56  ,57  ,58  ,59  ,60  ,61  ,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0   ,1   ,2   ,3   ,4   ,5   ,6   ,7   ,8   ,9   ,10  ,11  ,12  ,13  ,14  ,
        15  ,16  ,17  ,18  ,19  ,20  ,21  ,22  ,23  ,24  ,25  ,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,26  ,27  ,28  ,29  ,30  ,31  ,32  ,33  ,34  ,35  ,36  ,37  ,38  ,39  ,40  ,
        41  ,42  ,43  ,44  ,45  ,46  ,47  ,48  ,49  ,50  ,51  ,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    };

    outData.clear();
    uint32_t buffer = 0;
    int bits = 0;
    for (char ch : input) {
        if (ch == '=' || ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t') {
            continue;
        }
        uint8_t value = kTable[static_cast<uint8_t>(ch)];
        if (value == kInvalid) {
            return false;
        }
        buffer = (buffer << 6) | value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            outData.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return true;
}

bool DecodeDataUri(const std::string& uri, DataUriResult& outResult, std::string& outError) {
    if (uri.rfind("data:", 0) != 0) {
        outError = "Not a data URI";
        return false;
    }
    auto commaPos = uri.find(',');
    if (commaPos == std::string::npos) {
        outError = "Invalid data URI";
        return false;
    }
    std::string header = uri.substr(5, commaPos - 5);
    std::string dataPart = uri.substr(commaPos + 1);
    bool isBase64 = false;
    auto base64Pos = header.find(";base64");
    if (base64Pos != std::string::npos) {
        isBase64 = true;
        header = header.substr(0, base64Pos);
    }
    outResult.mimeType = header.empty() ? "application/octet-stream" : header;
    if (!isBase64) {
        outError = "Only base64 data URI is supported";
        return false;
    }
    if (!DecodeBase64(dataPart, outResult.data)) {
        outError = "Failed to decode base64 data";
        return false;
    }
    return true;
}

int ReadInt(const JSONValue& value, int defaultValue) {
    if (!value.IsNumber()) {
        return defaultValue;
    }
    return static_cast<int>(value.numberValue);
}

double ReadDouble(const JSONValue& value, double defaultValue) {
    if (!value.IsNumber()) {
        return defaultValue;
    }
    return value.numberValue;
}

bool ReadBool(const JSONValue& value, bool defaultValue) {
    if (!value.IsBool()) {
        return defaultValue;
    }
    return value.boolValue;
}

std::string ReadString(const JSONValue& value, const std::string& defaultValue = {}) {
    if (!value.IsString()) {
        return defaultValue;
    }
    return value.stringValue;
}

int AccessorTypeFromString(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT3") return 9;
    if (type == "MAT4") return 16;
    return 0;
}

int AlphaModeFromString(const std::string& mode) {
    if (mode == "MASK") return 1;
    if (mode == "BLEND") return 2;
    return 0;
}

void ReadArray(const JSONValue& value, double* outData, size_t count) {
    if (!value.IsArray() || value.arrayValue.size() < count) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (value.arrayValue[i].IsNumber()) {
            outData[i] = value.arrayValue[i].numberValue;
        }
    }
}

void ParseTextureInfo(const JSONValue& value, GLTFTextureInfo& outInfo, bool readScale, bool readStrength) {
    if (!value.IsObject()) {
        return;
    }
    outInfo.textureIndex = ReadInt(value["index"], -1);
    outInfo.texCoord = ReadInt(value["texCoord"], 0);
    if (readScale) {
        outInfo.scale = ReadDouble(value["scale"], outInfo.scale);
    }
    if (readStrength) {
        outInfo.strength = ReadDouble(value["strength"], outInfo.strength);
    }
}

void MarkTextureSRGB(const GLTFTextureInfo& info, GLTFAsset& asset) {
    if (info.textureIndex < 0 || info.textureIndex >= static_cast<int>(asset.textures.size())) {
        return;
    }
    const GLTFTexture& texture = asset.textures[info.textureIndex];
    if (texture.imageIndex < 0 || texture.imageIndex >= static_cast<int>(asset.images.size())) {
        return;
    }
    asset.images[texture.imageIndex].isSRGB = true;
}

bool ExtractBufferViewBytes(const GLTFAsset& asset, int viewIndex, std::vector<uint8_t>& outData, std::string& outError) {
    if (viewIndex < 0 || viewIndex >= static_cast<int>(asset.bufferViews.size())) {
        outError = "Invalid bufferView index";
        return false;
    }
    const GLTFBufferView& view = asset.bufferViews[viewIndex];
    if (view.bufferIndex < 0 || view.bufferIndex >= static_cast<int>(asset.buffers.size())) {
        outError = "Invalid buffer index in bufferView";
        return false;
    }
    const GLTFBuffer& buffer = asset.buffers[view.bufferIndex];
    if (view.byteOffset + view.byteLength > buffer.data.size()) {
        outError = "bufferView range out of bounds";
        return false;
    }
    const uint8_t* begin = buffer.data.data() + view.byteOffset;
    outData.assign(begin, begin + view.byteLength);
    return true;
}

bool ParseGLTFJson(const JSONValue& root, const std::filesystem::path& basePath, const std::vector<uint8_t>* binChunk, GLTFAsset& outAsset, std::string& outError) {
    if (!root.IsObject()) {
        outError = "Root JSON is not an object";
        return false;
    }

    if (root.HasKey("asset")) {
        const JSONValue& assetObj = root["asset"];
        outAsset.generator = ReadString(assetObj["generator"], "");
    }

    const JSONValue& buffers = root["buffers"];
    if (buffers.IsArray()) {
        outAsset.buffers.reserve(buffers.arrayValue.size());
        for (size_t i = 0; i < buffers.arrayValue.size(); ++i) {
            const JSONValue& bufferObj = buffers.arrayValue[i];
            if (!bufferObj.IsObject()) {
                outError = "Invalid buffer entry";
                return false;
            }
            int byteLength = ReadInt(bufferObj["byteLength"], 0);
            std::string uri = ReadString(bufferObj["uri"], "");
            std::vector<uint8_t> data;
            if (uri.empty()) {
                if (binChunk && i == 0) {
                    data = *binChunk;
                } else {
                    outError = "Buffer uri missing and no BIN chunk available";
                    return false;
                }
            } else if (uri.rfind("data:", 0) == 0) {
                DataUriResult uriResult;
                if (!DecodeDataUri(uri, uriResult, outError)) {
                    return false;
                }
                data = std::move(uriResult.data);
            } else {
                std::filesystem::path filePath = basePath / uri;
                if (!ReadFileBinary(filePath, data, outError)) {
                    return false;
                }
            }
            if (byteLength > 0) {
                if (data.size() < static_cast<size_t>(byteLength)) {
                    outError = "Buffer data is smaller than byteLength";
                    return false;
                }
                if (data.size() > static_cast<size_t>(byteLength)) {
                    data.resize(static_cast<size_t>(byteLength));
                }
            }
            GLTFBuffer buffer;
            buffer.data = std::move(data);
            outAsset.buffers.push_back(std::move(buffer));
        }
    }

    const JSONValue& bufferViews = root["bufferViews"];
    if (bufferViews.IsArray()) {
        outAsset.bufferViews.reserve(bufferViews.arrayValue.size());
        for (const auto& viewObj : bufferViews.arrayValue) {
            if (!viewObj.IsObject()) {
                outError = "Invalid bufferView entry";
                return false;
            }
            GLTFBufferView view;
            view.bufferIndex = ReadInt(viewObj["buffer"], -1);
            view.byteOffset = static_cast<size_t>(ReadInt(viewObj["byteOffset"], 0));
            view.byteLength = static_cast<size_t>(ReadInt(viewObj["byteLength"], 0));
            view.byteStride = static_cast<size_t>(ReadInt(viewObj["byteStride"], 0));
            view.target = ReadInt(viewObj["target"], 0);
            outAsset.bufferViews.push_back(view);
        }
    }

    const JSONValue& accessors = root["accessors"];
    if (accessors.IsArray()) {
        outAsset.accessors.reserve(accessors.arrayValue.size());
        for (const auto& accessorObj : accessors.arrayValue) {
            if (!accessorObj.IsObject()) {
                outError = "Invalid accessor entry";
                return false;
            }
            GLTFAccessor accessor;
            accessor.bufferViewIndex = ReadInt(accessorObj["bufferView"], -1);
            accessor.byteOffset = static_cast<size_t>(ReadInt(accessorObj["byteOffset"], 0));
            accessor.count = static_cast<size_t>(ReadInt(accessorObj["count"], 0));
            accessor.componentType = ReadInt(accessorObj["componentType"], 0);
            accessor.normalized = ReadBool(accessorObj["normalized"], false);
            std::string typeString = ReadString(accessorObj["type"], "");
            accessor.type = AccessorTypeFromString(typeString);
            const JSONValue& minArray = accessorObj["min"];
            if (minArray.IsArray()) {
                accessor.minValues.reserve(minArray.arrayValue.size());
                for (const auto& value : minArray.arrayValue) {
                    if (value.IsNumber()) {
                        accessor.minValues.push_back(value.numberValue);
                    }
                }
            }
            const JSONValue& maxArray = accessorObj["max"];
            if (maxArray.IsArray()) {
                accessor.maxValues.reserve(maxArray.arrayValue.size());
                for (const auto& value : maxArray.arrayValue) {
                    if (value.IsNumber()) {
                        accessor.maxValues.push_back(value.numberValue);
                    }
                }
            }
            outAsset.accessors.push_back(accessor);
        }
    }

    ImageDecoder decoder;
    const JSONValue& images = root["images"];
    if (images.IsArray()) {
        outAsset.images.reserve(images.arrayValue.size());
        for (const auto& imageObj : images.arrayValue) {
            if (!imageObj.IsObject()) {
                outError = "Invalid image entry";
                return false;
            }
            GLTFImage image;
            std::string mimeType = ReadString(imageObj["mimeType"], "");
            std::vector<uint8_t> imageData;
            if (imageObj.HasKey("uri")) {
                std::string uri = ReadString(imageObj["uri"], "");
                if (uri.rfind("data:", 0) == 0) {
                    DataUriResult uriResult;
                    if (!DecodeDataUri(uri, uriResult, outError)) {
                        return false;
                    }
                    imageData = std::move(uriResult.data);
                    if (mimeType.empty()) {
                        mimeType = uriResult.mimeType;
                    }
                } else {
                    std::filesystem::path filePath = basePath / uri;
                    if (!ReadFileBinary(filePath, imageData, outError)) {
                        return false;
                    }
                    if (mimeType.empty()) {
                        auto ext = filePath.extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        if (ext == ".png") {
                            mimeType = "image/png";
                        } else if (ext == ".jpg" || ext == ".jpeg") {
                            mimeType = "image/jpeg";
                        }
                    }
                }
            } else if (imageObj.HasKey("bufferView")) {
                int viewIndex = ReadInt(imageObj["bufferView"], -1);
                if (!ExtractBufferViewBytes(outAsset, viewIndex, imageData, outError)) {
                    return false;
                }
            } else {
                outError = "Image entry missing uri or bufferView";
                return false;
            }

            if (mimeType.empty()) {
                outError = "Image mimeType is missing";
                return false;
            }

            if (!decoder.Decode(imageData, mimeType, image)) {
                outError = decoder.GetLastError();
                return false;
            }
            image.mimeType = mimeType;
            outAsset.images.push_back(std::move(image));
        }
    }

    const JSONValue& samplers = root["samplers"];
    if (samplers.IsArray()) {
        outAsset.samplers.reserve(samplers.arrayValue.size());
        for (const auto& samplerObj : samplers.arrayValue) {
            if (!samplerObj.IsObject()) {
                outError = "Invalid sampler entry";
                return false;
            }
            GLTFSampler sampler;
            sampler.wrapS = ReadInt(samplerObj["wrapS"], 10497);
            sampler.wrapT = ReadInt(samplerObj["wrapT"], 10497);
            sampler.minFilter = ReadInt(samplerObj["minFilter"], 0);
            sampler.magFilter = ReadInt(samplerObj["magFilter"], 0);
            outAsset.samplers.push_back(sampler);
        }
    }

    const JSONValue& textures = root["textures"];
    if (textures.IsArray()) {
        outAsset.textures.reserve(textures.arrayValue.size());
        for (const auto& textureObj : textures.arrayValue) {
            if (!textureObj.IsObject()) {
                outError = "Invalid texture entry";
                return false;
            }
            GLTFTexture texture;
            texture.imageIndex = ReadInt(textureObj["source"], -1);
            texture.samplerIndex = ReadInt(textureObj["sampler"], -1);
            outAsset.textures.push_back(texture);
        }
    }

    const JSONValue& materials = root["materials"];
    if (materials.IsArray()) {
        outAsset.materials.reserve(materials.arrayValue.size());
        for (const auto& materialObj : materials.arrayValue) {
            if (!materialObj.IsObject()) {
                outError = "Invalid material entry";
                return false;
            }
            GLTFMaterial material;
            material.name = ReadString(materialObj["name"], "");
            const JSONValue& pbr = materialObj["pbrMetallicRoughness"];
            if (pbr.IsObject()) {
                ReadArray(pbr["baseColorFactor"], material.pbr.baseColorFactor, 4);
                ParseTextureInfo(pbr["baseColorTexture"], material.pbr.baseColorTexture, false, false);
                material.pbr.metallicFactor = ReadDouble(pbr["metallicFactor"], material.pbr.metallicFactor);
                material.pbr.roughnessFactor = ReadDouble(pbr["roughnessFactor"], material.pbr.roughnessFactor);
                ParseTextureInfo(pbr["metallicRoughnessTexture"], material.pbr.metallicRoughnessTexture, false, false);
            }
            const JSONValue& extensions = materialObj["extensions"];
            if (extensions.IsObject()) {
                // KHR_materials_transmission
                if (extensions.HasKey("KHR_materials_transmission")) {
                    const JSONValue& transmission = extensions["KHR_materials_transmission"];
                    if (transmission.IsObject()) {
                        material.transmission.hasTransmission = true;
                        material.transmission.transmissionFactor =
                            ReadDouble(transmission["transmissionFactor"], material.transmission.transmissionFactor);
                        ParseTextureInfo(transmission["transmissionTexture"], material.transmission.transmissionTexture, false, false);
                    }
                }
                // KHR_materials_ior
                if (extensions.HasKey("KHR_materials_ior")) {
                    const JSONValue& iorObj = extensions["KHR_materials_ior"];
                    if (iorObj.IsObject()) {
                        material.iorExt.hasIOR = true;
                        material.iorExt.ior = ReadDouble(iorObj["ior"], material.iorExt.ior);
                    }
                }
                // KHR_materials_specular
                if (extensions.HasKey("KHR_materials_specular")) {
                    const JSONValue& specObj = extensions["KHR_materials_specular"];
                    if (specObj.IsObject()) {
                        material.specular.hasSpecular = true;
                        material.specular.specularFactor =
                            ReadDouble(specObj["specularFactor"], material.specular.specularFactor);
                        ReadArray(specObj["specularColorFactor"], material.specular.specularColorFactor, 3);
                        ParseTextureInfo(specObj["specularTexture"], material.specular.specularTexture, false, false);
                        ParseTextureInfo(specObj["specularColorTexture"], material.specular.specularColorTexture, false, false);
                    }
                }
            }
            ParseTextureInfo(materialObj["normalTexture"], material.normalTexture, true, false);
            ParseTextureInfo(materialObj["occlusionTexture"], material.occlusionTexture, false, true);
            ParseTextureInfo(materialObj["emissiveTexture"], material.emissiveTexture, false, false);
            ReadArray(materialObj["emissiveFactor"], material.emissiveFactor, 3);
            material.alphaMode = AlphaModeFromString(ReadString(materialObj["alphaMode"], "OPAQUE"));
            material.alphaCutoff = ReadDouble(materialObj["alphaCutoff"], material.alphaCutoff);
            material.doubleSided = ReadBool(materialObj["doubleSided"], false);
            outAsset.materials.push_back(std::move(material));
        }
    }

    for (const auto& material : outAsset.materials) {
        MarkTextureSRGB(material.pbr.baseColorTexture, outAsset);
        MarkTextureSRGB(material.emissiveTexture, outAsset);
    }

    const JSONValue& meshes = root["meshes"];
    if (meshes.IsArray()) {
        outAsset.meshes.reserve(meshes.arrayValue.size());
        for (const auto& meshObj : meshes.arrayValue) {
            if (!meshObj.IsObject()) {
                outError = "Invalid mesh entry";
                return false;
            }
            GLTFMesh mesh;
            mesh.name = ReadString(meshObj["name"], "");
            const JSONValue& primitives = meshObj["primitives"];
            if (primitives.IsArray()) {
                mesh.primitives.reserve(primitives.arrayValue.size());
                for (const auto& primObj : primitives.arrayValue) {
                    if (!primObj.IsObject()) {
                        continue;
                    }
                    GLTFPrimitive prim;
                    prim.materialIndex = ReadInt(primObj["material"], -1);
                    prim.indices = ReadInt(primObj["indices"], -1);
                    prim.mode = ReadInt(primObj["mode"], 4);
                    const JSONValue& attributes = primObj["attributes"];
                    if (attributes.IsObject()) {
                        for (const auto& entry : attributes.objectValue) {
                            if (entry.second.IsNumber()) {
                                prim.attributes.emplace(entry.first, static_cast<int>(entry.second.numberValue));
                            }
                        }
                    }
                    mesh.primitives.push_back(prim);
                }
            }
            outAsset.meshes.push_back(std::move(mesh));
        }
    }

    const JSONValue& nodes = root["nodes"];
    if (nodes.IsArray()) {
        outAsset.nodes.reserve(nodes.arrayValue.size());
        for (const auto& nodeObj : nodes.arrayValue) {
            if (!nodeObj.IsObject()) {
                outError = "Invalid node entry";
                return false;
            }
            GLTFNode node;
            node.meshIndex = ReadInt(nodeObj["mesh"], -1);
            const JSONValue& children = nodeObj["children"];
            if (children.IsArray()) {
                node.children.reserve(children.arrayValue.size());
                for (const auto& child : children.arrayValue) {
                    if (child.IsNumber()) {
                        node.children.push_back(static_cast<int>(child.numberValue));
                    }
                }
            }
            ReadArray(nodeObj["translation"], node.translation, 3);
            ReadArray(nodeObj["rotation"], node.rotation, 4);
            ReadArray(nodeObj["scale"], node.scale, 3);
            if (nodeObj.HasKey("matrix")) {
                ReadArray(nodeObj["matrix"], node.matrix, 16);
                node.hasMatrix = true;
            }
            outAsset.nodes.push_back(node);
        }
    }

    const JSONValue& scenes = root["scenes"];
    if (scenes.IsArray()) {
        outAsset.scenes.reserve(scenes.arrayValue.size());
        for (const auto& sceneObj : scenes.arrayValue) {
            if (!sceneObj.IsObject()) {
                outError = "Invalid scene entry";
                return false;
            }
            GLTFScene scene;
            const JSONValue& nodesArray = sceneObj["nodes"];
            if (nodesArray.IsArray()) {
                scene.rootNodes.reserve(nodesArray.arrayValue.size());
                for (const auto& nodeIndex : nodesArray.arrayValue) {
                    if (nodeIndex.IsNumber()) {
                        scene.rootNodes.push_back(static_cast<int>(nodeIndex.numberValue));
                    }
                }
            }
            outAsset.scenes.push_back(std::move(scene));
        }
    }

    outAsset.defaultSceneIndex = ReadInt(root["scene"], -1);

    return true;
}

} // namespace

/**
 * @brief 解析并加载 GLB 二进制模型
 */
GLTFAsset GLTFLoader::LoadGLB(const std::string& path) {
    using Clock = std::chrono::high_resolution_clock;
    auto t0 = Clock::now();
    m_lastError.clear();

    std::vector<uint8_t> fileData;
    std::string error;
    if (!ReadFileBinary(path, fileData, error)) {
        m_lastError = error;
        return {};
    }
    auto tRead = Clock::now();

    if (fileData.size() < 12) {
        m_lastError = "Invalid GLB file size";
        return {};
    }

    const uint32_t magic = ReadU32LE(fileData.data());
    const uint32_t version = ReadU32LE(fileData.data() + 4);
    const uint32_t length = ReadU32LE(fileData.data() + 8);
    if (magic != 0x46546C67) {
        m_lastError = "Invalid GLB magic";
        return {};
    }
    if (version < 2) {
        m_lastError = "Unsupported GLB version";
        return {};
    }
    if (length > fileData.size()) {
        m_lastError = "GLB length mismatch";
        return {};
    }

    std::vector<uint8_t> jsonChunk;
    std::vector<uint8_t> binChunk;
    size_t offset = 12;
    while (offset + 8 <= fileData.size()) {
        uint32_t chunkLength = ReadU32LE(fileData.data() + offset);
        uint32_t chunkType = ReadU32LE(fileData.data() + offset + 4);
        offset += 8;
        if (offset + chunkLength > fileData.size()) {
            m_lastError = "GLB chunk out of bounds";
            return {};
        }
        const uint8_t* chunkData = fileData.data() + offset;
        if (chunkType == 0x4E4F534A) {
            jsonChunk.assign(chunkData, chunkData + chunkLength);
        } else if (chunkType == 0x004E4942) {
            binChunk.assign(chunkData, chunkData + chunkLength);
        }
        offset += chunkLength;
    }

    if (jsonChunk.empty()) {
        m_lastError = "Missing GLB JSON chunk";
        return {};
    }

    while (!jsonChunk.empty() && jsonChunk.back() == '\0') {
        jsonChunk.pop_back();
    }

    std::string jsonText(reinterpret_cast<const char*>(jsonChunk.data()), jsonChunk.size());
    JSONParser parser;
    auto jsonValue = parser.Parse(jsonText);
    if (!jsonValue) {
        m_lastError = parser.GetLastError();
        return {};
    }
    auto tJson = Clock::now();

    GLTFAsset asset;
    auto basePath = std::filesystem::path(path).parent_path();
    if (!ParseGLTFJson(*jsonValue, basePath, &binChunk, asset, m_lastError)) {
        return {};
    }
    auto tParse = Clock::now();

    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
        "GLB perf(ms): read=%.3f json=%.3f parse=%.3f total=%.3f\n",
        std::chrono::duration<double, std::milli>(tRead - t0).count(),
        std::chrono::duration<double, std::milli>(tJson - tRead).count(),
        std::chrono::duration<double, std::milli>(tParse - tJson).count(),
        std::chrono::duration<double, std::milli>(tParse - t0).count());
    OutputDebugStringA(buffer);
    return asset;
}

/**
 * @brief 解析并加载文本格式 glTF 模型
 */
GLTFAsset GLTFLoader::LoadGLTF(const std::string& path) {
    using Clock = std::chrono::high_resolution_clock;
    auto t0 = Clock::now();
    m_lastError.clear();

    std::string jsonText;
    std::string error;
    if (!ReadFileText(path, jsonText, error)) {
        m_lastError = error;
        return {};
    }
    auto tRead = Clock::now();

    JSONParser parser;
    auto jsonValue = parser.Parse(jsonText);
    if (!jsonValue) {
        m_lastError = parser.GetLastError();
        return {};
    }
    auto tJson = Clock::now();

    GLTFAsset asset;
    auto basePath = std::filesystem::path(path).parent_path();
    if (!ParseGLTFJson(*jsonValue, basePath, nullptr, asset, m_lastError)) {
        return {};
    }
    auto tParse = Clock::now();

    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
        "GLTF perf(ms): read=%.3f json=%.3f parse=%.3f total=%.3f\n",
        std::chrono::duration<double, std::milli>(tRead - t0).count(),
        std::chrono::duration<double, std::milli>(tJson - tRead).count(),
        std::chrono::duration<double, std::milli>(tParse - tJson).count(),
        std::chrono::duration<double, std::milli>(tParse - t0).count());
    OutputDebugStringA(buffer);
    return asset;
}

const std::string& GLTFLoader::GetLastError() const {
    return m_lastError;
}

} // namespace SR
