#include "Runtime/GPUScene.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <chrono>
#include <cstdio>

#include "Asset/BufferAccessor.h"
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"
#include "Utils/DebugLog.h"

namespace SR {

/** @brief 预留容量 */
void GPUScene::Reserve(size_t count) {
	m_items.reserve(count);
}

/** @brief 添加渲染项 */
void GPUScene::AddDrawable(const GPUSceneDrawItem& item) {
	m_items.push_back(item);
}

/** @brief 批量设置渲染项 */
void GPUScene::SetItems(std::vector<GPUSceneDrawItem>&& items) {
	m_items = std::move(items);
}

/** @brief 清空场景 */
void GPUScene::Clear() {
	m_items.clear();
	m_ownedMeshes.clear();
	m_ownedMaterials.clear();
	m_ownedImages.clear();
	m_ownedSamplers.clear();
	// 同步清空 ResourcePool（网格池和材质池）
	m_meshPool.Clear();
	m_materialPool.Clear();
}

/** @brief 获取渲染项 */
const std::vector<GPUSceneDrawItem>& GPUScene::GetItems() const {
	return m_items;
}

/** @brief 获取所有图像资源 */
const std::vector<GLTFImage>& GPUScene::GetImages() const {
	return m_ownedImages;
}

/** @brief 获取所有采样器资源 */
const std::vector<GLTFSampler>& GPUScene::GetSamplers() const {
	return m_ownedSamplers;
}

namespace {

/**
 * @brief 将四元数转换为 4x4 旋转矩阵
 */
Mat4 QuaternionToMat4(double x, double y, double z, double w) {
	Mat4 result = Mat4::Identity();
	double xx = x * x;
	double yy = y * y;
	double zz = z * z;
	double xy = x * y;
	double xz = x * z;
	double yz = y * z;
	double wx = w * x;
	double wy = w * y;
	double wz = w * z;

	result.m[0][0] = 1.0 - 2.0 * (yy + zz);
	result.m[0][1] = 2.0 * (xy + wz);
	result.m[0][2] = 2.0 * (xz - wy);

	result.m[1][0] = 2.0 * (xy - wz);
	result.m[1][1] = 1.0 - 2.0 * (xx + zz);
	result.m[1][2] = 2.0 * (yz + wx);

	result.m[2][0] = 2.0 * (xz + wy);
	result.m[2][1] = 2.0 * (yz - wx);
	result.m[2][2] = 1.0 - 2.0 * (xx + yy);

	return result;
}

Mat4 BuildNodeLocalMatrix(const GLTFNode& node) {
	if (node.hasMatrix) {
		Mat4 result{};
		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				// glTF 矩阵为列主序；转为行向量数学约定需要转置
				result.m[row][col] = node.matrix[row * 4 + col];
			}
		}
		return result;
	}

	Mat4 translation = Mat4::Translation(node.translation[0], node.translation[1], node.translation[2]);
	Mat4 rotation = QuaternionToMat4(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
	Mat4 scale = Mat4::Scale(node.scale[0], node.scale[1], node.scale[2]);
	return scale * rotation * translation;
}

void ApplyZFlip(Mat4& matrix) {
	Mat4 flip = Mat4::Identity();
	flip.m[2][2] = -1.0;
	matrix = flip * matrix * flip;
}

Mat4 ComputeNormalMatrix(const Mat4& modelMatrix) {
	double m00 = modelMatrix.m[0][0];
	double m01 = modelMatrix.m[0][1];
	double m02 = modelMatrix.m[0][2];
	double m10 = modelMatrix.m[1][0];
	double m11 = modelMatrix.m[1][1];
	double m12 = modelMatrix.m[1][2];
	double m20 = modelMatrix.m[2][0];
	double m21 = modelMatrix.m[2][1];
	double m22 = modelMatrix.m[2][2];

	double c00 = m11 * m22 - m12 * m21;
	double c01 = m02 * m21 - m01 * m22;
	double c02 = m01 * m12 - m02 * m11;

	double c10 = m12 * m20 - m10 * m22;
	double c11 = m00 * m22 - m02 * m20;
	double c12 = m02 * m10 - m00 * m12;

	double c20 = m10 * m21 - m11 * m20;
	double c21 = m01 * m20 - m00 * m21;
	double c22 = m00 * m11 - m01 * m10;

	double det = m00 * c00 + m01 * c10 + m02 * c20;
	if (std::abs(det) < 1e-12) {
		Mat4 fallback = modelMatrix;
		fallback.m[3][0] = 0.0;
		fallback.m[3][1] = 0.0;
		fallback.m[3][2] = 0.0;
		fallback.m[0][3] = 0.0;
		fallback.m[1][3] = 0.0;
		fallback.m[2][3] = 0.0;
		fallback.m[3][3] = 1.0;
		return fallback;
	}
	double invDet = 1.0 / det;

	Mat4 normal = Mat4::Identity();
	// 法线矩阵 = 模型矩阵左上 3x3 的逆转置（伴随矩阵法）
	normal.m[0][0] = c00 * invDet;
	normal.m[1][0] = c01 * invDet;
	normal.m[2][0] = c02 * invDet;

	normal.m[0][1] = c10 * invDet;
	normal.m[1][1] = c11 * invDet;
	normal.m[2][1] = c12 * invDet;

	normal.m[0][2] = c20 * invDet;
	normal.m[1][2] = c21 * invDet;
	normal.m[2][2] = c22 * invDet;

	return normal;
}

struct PrimitiveMeshes {
	std::vector<size_t> meshIndices;
};

} // namespace

/**
 * @brief 从原始 glTF 资产对象构建运行时加速场景 (GPUScene)
 * @param asset 加载完成的资产
 * @param sceneIndex 要构建的场景索引
 */
void GPUScene::Build(const GLTFAsset& asset, int sceneIndex) {
	using Clock = std::chrono::high_resolution_clock;
	auto t0 = Clock::now();
	Clear();
	m_ownedMeshes.clear();
	m_ownedMaterials.clear();

	// 性能统计：各阶段耗时
	double totalAccessorReadMs = 0.0;
	double totalNormalsMs = 0.0;
	double totalTangentsMs = 0.0;
	size_t normalGenCount = 0;
	size_t tangentGenCount = 0;

	m_ownedImages = asset.images;
	m_ownedSamplers = asset.samplers;

	bool hasSceneGraph = !asset.scenes.empty();
	int chosenScene = sceneIndex;
	if (hasSceneGraph) {
		if (chosenScene < 0 || chosenScene >= static_cast<int>(asset.scenes.size())) {
			chosenScene = asset.defaultSceneIndex >= 0 ? asset.defaultSceneIndex : 0;
		}
	}

	BufferAccessor accessor;

	m_ownedMaterials.reserve(asset.materials.size() + 1);
	for (const auto& srcMat : asset.materials) {
		PBRMaterial material{};
		material.albedo = Vec3{srcMat.pbr.baseColorFactor[0], srcMat.pbr.baseColorFactor[1], srcMat.pbr.baseColorFactor[2]};
		material.metallic = srcMat.pbr.metallicFactor;
		material.roughness = srcMat.pbr.roughnessFactor;
		material.doubleSided = srcMat.doubleSided;
		material.alpha = srcMat.pbr.baseColorFactor[3];
		material.transmissionFactor = srcMat.transmission.hasTransmission
			? std::clamp(srcMat.transmission.transmissionFactor, 0.0, 1.0)
			: 0.0;
		material.alphaMode = srcMat.alphaMode;
		material.alphaCutoff = srcMat.alphaCutoff;
		material.emissiveFactor = Vec3{srcMat.emissiveFactor[0], srcMat.emissiveFactor[1], srcMat.emissiveFactor[2]};
		// KHR_materials_ior 扩展：折射率
		if (srcMat.iorExt.hasIOR) {
			material.ior = std::max(1.0, srcMat.iorExt.ior);
		}
		// KHR_materials_specular 扩展：高光强度与颜色
		if (srcMat.specular.hasSpecular) {
			material.specularFactor = std::clamp(srcMat.specular.specularFactor, 0.0, 1.0);
			material.specularColorFactor = Vec3{
				std::clamp(srcMat.specular.specularColorFactor[0], 0.0, 1.0),
				std::clamp(srcMat.specular.specularColorFactor[1], 0.0, 1.0),
				std::clamp(srcMat.specular.specularColorFactor[2], 0.0, 1.0)
			};
		}
		bool hasTransmissionTexture = srcMat.transmission.hasTransmission &&
			srcMat.transmission.transmissionTexture.textureIndex >= 0;
		if (srcMat.transmission.hasTransmission &&
			(material.transmissionFactor > 0.0 || hasTransmissionTexture)) {
			material.alphaMode = GLTFAlphaMode::Blend;
		}
		m_ownedMaterials.push_back(material);
	}
	PBRMaterial defaultMaterial{};
	m_ownedMaterials.push_back(defaultMaterial);
	const size_t defaultMaterialIndex = m_ownedMaterials.size() - 1;

	std::vector<PrimitiveMeshes> meshPrimitiveTable(asset.meshes.size());
	for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); ++meshIndex) {
		const GLTFMesh& mesh = asset.meshes[meshIndex];
		PrimitiveMeshes primMeshes;
		primMeshes.meshIndices.reserve(mesh.primitives.size());

		for (const GLTFPrimitive& prim : mesh.primitives) {
			if (prim.mode != 4) {
				primMeshes.meshIndices.push_back(static_cast<size_t>(-1));
				continue;
			}

			auto posIt = prim.attributes.find("POSITION");
			if (posIt == prim.attributes.end()) {
				primMeshes.meshIndices.push_back(static_cast<size_t>(-1));
				continue;
			}

			int posAccessorIndex = posIt->second;
			if (posAccessorIndex < 0 || posAccessorIndex >= static_cast<int>(asset.accessors.size())) {
				primMeshes.meshIndices.push_back(static_cast<size_t>(-1));
				continue;
			}

			const GLTFAccessor& posAccessor = asset.accessors[posAccessorIndex];
			auto tReadStart = Clock::now();
			std::vector<Vec3> positions = accessor.Read<Vec3>(asset, posAccessor);
			if (positions.empty()) {
				primMeshes.meshIndices.push_back(static_cast<size_t>(-1));
				continue;
			}

			std::vector<Vec3> normals;
			auto normalIt = prim.attributes.find("NORMAL");
			if (normalIt != prim.attributes.end()) {
				int normalAccessorIndex = normalIt->second;
				if (normalAccessorIndex >= 0 && normalAccessorIndex < static_cast<int>(asset.accessors.size())) {
					normals = accessor.Read<Vec3>(asset, asset.accessors[normalAccessorIndex]);
				}
			}

			std::vector<Vec2> texcoords;
			auto texIt = prim.attributes.find("TEXCOORD_0");
			if (texIt != prim.attributes.end()) {
				int texAccessorIndex = texIt->second;
				if (texAccessorIndex >= 0 && texAccessorIndex < static_cast<int>(asset.accessors.size())) {
					texcoords = accessor.Read<Vec2>(asset, asset.accessors[texAccessorIndex]);
				}
			}
			std::vector<Vec2> texcoords1;
			auto tex1It = prim.attributes.find("TEXCOORD_1");
			if (tex1It != prim.attributes.end()) {
				int texAccessorIndex = tex1It->second;
				if (texAccessorIndex >= 0 && texAccessorIndex < static_cast<int>(asset.accessors.size())) {
					texcoords1 = accessor.Read<Vec2>(asset, asset.accessors[texAccessorIndex]);
				}
			}
			std::vector<Vec4> colors;
			auto colorIt = prim.attributes.find("COLOR_0");
			if (colorIt != prim.attributes.end()) {
				int colorAccessorIndex = colorIt->second;
				if (colorAccessorIndex >= 0 && colorAccessorIndex < static_cast<int>(asset.accessors.size())) {
					const GLTFAccessor& colorAccessor = asset.accessors[colorAccessorIndex];
					if (colorAccessor.type == 3) {
						std::vector<Vec3> rgb = accessor.Read<Vec3>(asset, colorAccessor);
						colors.reserve(rgb.size());
						for (const Vec3& c : rgb) {
							colors.emplace_back(c.x, c.y, c.z, 1.0);
						}
					} else {
						colors = accessor.Read<Vec4>(asset, colorAccessor);
					}
				}
			}

			std::vector<Vec4> tangents4;
			auto tanIt = prim.attributes.find("TANGENT");
			if (tanIt != prim.attributes.end()) {
				int tanAccessorIndex = tanIt->second;
				if (tanAccessorIndex >= 0 && tanAccessorIndex < static_cast<int>(asset.accessors.size())) {
					tangents4 = accessor.Read<Vec4>(asset, asset.accessors[tanAccessorIndex]);
				}
			}

			std::vector<uint32_t> indices;
			if (prim.indices >= 0 && prim.indices < static_cast<int>(asset.accessors.size())) {
				indices = accessor.Read<uint32_t>(asset, asset.accessors[prim.indices]);
			}
			if (indices.empty()) {
				indices.resize(positions.size());
				for (size_t i = 0; i < indices.size(); ++i) {
					indices[i] = static_cast<uint32_t>(i);
				}
			}

			std::vector<Vertex> vertices;
			vertices.resize(positions.size());
			for (size_t i = 0; i < positions.size(); ++i) {
				Vec3 pos = positions[i];
				pos.z = -pos.z;
				Vec3 normal{};
				if (i < normals.size()) {
					normal = normals[i];
					normal.z = -normal.z;
				}
				Vec2 uv{};
				if (i < texcoords.size()) {
					uv = texcoords[i];
				}
				Vec2 uv1 = uv;
				if (i < texcoords1.size()) {
					uv1 = texcoords1[i];
				}
				Vec4 color{1.0, 1.0, 1.0, 1.0};
				if (i < colors.size()) {
					color = colors[i];
				}
				Vec3 tangent{};
				double tangentW = 1.0;
				if (i < tangents4.size()) {
					tangent = Vec3{tangents4[i].x, tangents4[i].y, tangents4[i].z};
					tangent.z = -tangent.z;
					// glTF 顶点在导入时做了 Z 翻转 (右手->左手)，该变换会改变切线空间手性，
					// 需要同步翻转 tangentW，否则法线贴图的副切线方向会反，导致光照方向错误。
					tangentW = -tangents4[i].w;
				}

				vertices[i] = Vertex{pos, normal, uv, uv1, color, tangent, tangentW};
			}

			for (size_t i = 0; i + 2 < indices.size(); i += 3) {
				std::swap(indices[i + 1], indices[i + 2]);
			}
			auto tReadEnd = Clock::now();
			totalAccessorReadMs += std::chrono::duration<double, std::milli>(tReadEnd - tReadStart).count();

			Mesh outMesh;
			outMesh.SetData(std::move(vertices), std::move(indices));
			if (normals.empty()) {
				auto tNormStart = Clock::now();
				outMesh.GenerateNormals();
				auto tNormEnd = Clock::now();
				totalNormalsMs += std::chrono::duration<double, std::milli>(tNormEnd - tNormStart).count();
				normalGenCount++;
			}
			if (tangents4.empty() && !texcoords.empty()) {
				auto tTanStart = Clock::now();
				outMesh.GenerateTangents();
				auto tTanEnd = Clock::now();
				totalTangentsMs += std::chrono::duration<double, std::milli>(tTanEnd - tTanStart).count();
				tangentGenCount++;
			}

			m_ownedMeshes.push_back(std::move(outMesh));
			primMeshes.meshIndices.push_back(m_ownedMeshes.size() - 1);
		}

		meshPrimitiveTable[meshIndex] = std::move(primMeshes);
	}

	auto addNode = [&](auto&& self, int nodeIndex, const Mat4& parentMatrix) -> void {
		if (nodeIndex < 0 || nodeIndex >= static_cast<int>(asset.nodes.size())) {
			return;
		}
		const GLTFNode& node = asset.nodes[nodeIndex];
		Mat4 local = BuildNodeLocalMatrix(node);
		ApplyZFlip(local);
		Mat4 world = local * parentMatrix;

		if (node.meshIndex >= 0 && node.meshIndex < static_cast<int>(asset.meshes.size())) {
			const PrimitiveMeshes& primMeshes = meshPrimitiveTable[node.meshIndex];
			const GLTFMesh& mesh = asset.meshes[node.meshIndex];
			for (size_t primIndex = 0; primIndex < mesh.primitives.size(); ++primIndex) {
				size_t meshSlot = primMeshes.meshIndices[primIndex];
				if (meshSlot == static_cast<size_t>(-1) || meshSlot >= m_ownedMeshes.size()) {
					continue;
				}
				const GLTFPrimitive& prim = mesh.primitives[primIndex];
				size_t matIndex = defaultMaterialIndex;
				if (prim.materialIndex >= 0 && prim.materialIndex < static_cast<int>(m_ownedMaterials.size())) {
					matIndex = static_cast<size_t>(prim.materialIndex);
				}
				TextureBindingArray textures{};
				if (prim.materialIndex >= 0 && prim.materialIndex < static_cast<int>(asset.materials.size())) {
					const GLTFMaterial& gltfMat = asset.materials[prim.materialIndex];
					textures[static_cast<size_t>(TextureSlot::BaseColor)].textureIndex = gltfMat.pbr.baseColorTexture.textureIndex;
					textures[static_cast<size_t>(TextureSlot::MetallicRoughness)].textureIndex = gltfMat.pbr.metallicRoughnessTexture.textureIndex;
					textures[static_cast<size_t>(TextureSlot::Normal)].textureIndex = gltfMat.normalTexture.textureIndex;
					textures[static_cast<size_t>(TextureSlot::Occlusion)].textureIndex = gltfMat.occlusionTexture.textureIndex;
					textures[static_cast<size_t>(TextureSlot::Emissive)].textureIndex = gltfMat.emissiveTexture.textureIndex;
					textures[static_cast<size_t>(TextureSlot::BaseColor)].texCoordSet = gltfMat.pbr.baseColorTexture.texCoord;
					textures[static_cast<size_t>(TextureSlot::MetallicRoughness)].texCoordSet = gltfMat.pbr.metallicRoughnessTexture.texCoord;
					textures[static_cast<size_t>(TextureSlot::Normal)].texCoordSet = gltfMat.normalTexture.texCoord;
					textures[static_cast<size_t>(TextureSlot::Occlusion)].texCoordSet = gltfMat.occlusionTexture.texCoord;
					textures[static_cast<size_t>(TextureSlot::Emissive)].texCoordSet = gltfMat.emissiveTexture.texCoord;
					if (gltfMat.transmission.hasTransmission) {
						textures[static_cast<size_t>(TextureSlot::Transmission)].textureIndex = gltfMat.transmission.transmissionTexture.textureIndex;
						textures[static_cast<size_t>(TextureSlot::Transmission)].texCoordSet = gltfMat.transmission.transmissionTexture.texCoord;
					}
				}
				auto resolveTexture = [&](TextureBinding& binding) {
					int textureIndex = binding.textureIndex;
					if (textureIndex < 0 || textureIndex >= static_cast<int>(asset.textures.size())) {
						return;
					}
					const GLTFTexture& tex = asset.textures[textureIndex];
					binding.imageIndex = tex.imageIndex;
					binding.samplerIndex = tex.samplerIndex;
				};
				for (size_t i = 0; i < textures.size(); ++i) {
					resolveTexture(textures[i]);
				}

				GPUSceneDrawItem item{};
				item.mesh = &m_ownedMeshes[meshSlot];
				item.material = &m_ownedMaterials[matIndex];
				item.modelMatrix = world;
				item.normalMatrix = ComputeNormalMatrix(world);
				item.meshIndex = node.meshIndex;
				item.materialIndex = prim.materialIndex;
				item.primitiveIndex = static_cast<int>(primIndex);
				item.nodeIndex = nodeIndex;
				item.textures = textures;
				AddDrawable(item);
			}
		}

		for (int childIndex : node.children) {
			self(self, childIndex, world);
		}
	};

	Mat4 identity = Mat4::Identity();
	auto tSceneGraphStart = Clock::now();
	if (hasSceneGraph) {
		const GLTFScene& scene = asset.scenes[chosenScene];
		for (int nodeIndex : scene.rootNodes) {
			addNode(addNode, nodeIndex, identity);
		}
	} else {
		for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); ++meshIndex) {
			const PrimitiveMeshes& primMeshes = meshPrimitiveTable[meshIndex];
			const GLTFMesh& mesh = asset.meshes[meshIndex];
			for (size_t primIndex = 0; primIndex < mesh.primitives.size(); ++primIndex) {
				size_t meshSlot = primMeshes.meshIndices[primIndex];
				if (meshSlot == static_cast<size_t>(-1) || meshSlot >= m_ownedMeshes.size()) {
					continue;
				}
				const GLTFPrimitive& prim = mesh.primitives[primIndex];
				size_t matIndex = defaultMaterialIndex;
				if (prim.materialIndex >= 0 && prim.materialIndex < static_cast<int>(m_ownedMaterials.size())) {
					matIndex = static_cast<size_t>(prim.materialIndex);
				}

				GPUSceneDrawItem item{};
				item.mesh = &m_ownedMeshes[meshSlot];
				item.material = &m_ownedMaterials[matIndex];
				item.modelMatrix = identity;
				item.normalMatrix = ComputeNormalMatrix(identity);
				item.meshIndex = static_cast<int>(meshIndex);
				item.materialIndex = prim.materialIndex;
				item.primitiveIndex = static_cast<int>(primIndex);
				item.nodeIndex = -1;
				AddDrawable(item);
			}
		}
	}
	auto tSceneGraphEnd = Clock::now();
	double sceneGraphMs = std::chrono::duration<double, std::milli>(tSceneGraphEnd - tSceneGraphStart).count();

	size_t totalPrims = 0;
	for (const auto& mesh : asset.meshes) {
		totalPrims += mesh.primitives.size();
	}
	auto t1 = Clock::now();
	double totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

	char buffer[512];
	std::snprintf(buffer, sizeof(buffer),
		"GPUScene Build(ms): total=%.3f accessor=%.3f normals=%.3f(x%zu) tangents=%.3f(x%zu) sceneGraph=%.3f\n"
		"  meshes=%zu primitives=%zu items=%zu images=%zu\n",
		totalMs, totalAccessorReadMs, totalNormalsMs, normalGenCount, totalTangentsMs, tangentGenCount, sceneGraphMs,
		asset.meshes.size(), totalPrims, m_items.size(), asset.images.size());
	SR_DEBUG_LOG(buffer);
}

// ========== ResourcePool 集成 API ==========

void GPUScene::SetMemoryBudget(size_t meshBytes, size_t materialBytes) {
	m_meshPool.SetMemoryBudget(meshBytes);
	m_materialPool.SetMemoryBudget(materialBytes);
}

void GPUScene::EvictResources() {
	m_meshPool.EvictLRU();
	m_materialPool.EvictLRU();
}

size_t GPUScene::GetTotalMemoryUsage() const {
	size_t total = 0;
	// 统计传统存储（拥有权网格和图像）的内存
	for (const auto& mesh : m_ownedMeshes) {
		total += mesh.GetVertices().size() * sizeof(Vertex);
		total += mesh.GetIndices().size() * sizeof(uint32_t);
	}
	for (const auto& image : m_ownedImages) {
		total += image.pixels.size();
	}
	// 累加资源池中的内存
	total += m_meshPool.GetTotalTriangleCount() * 3 * sizeof(uint32_t);
	return total;
}

} // namespace SR
