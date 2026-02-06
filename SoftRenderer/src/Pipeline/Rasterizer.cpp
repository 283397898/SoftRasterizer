#include "Pipeline/Rasterizer.h"

#include "Pipeline/FragmentShader.h"
#include "Pipeline/Clipper.h"

#include "Asset/GLTFTypes.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdio>
#include <omp.h>
#define NOMINMAX
#include <windows.h>

// SIMD intrinsics
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#include <immintrin.h>

namespace SR {

namespace {

// ============================================================================
// SIMD Helper Functions for AVX2 (256-bit, 4 doubles)
// ============================================================================

// Check if all 4 values are >= 0 or all <= 0 (for inside triangle test)
inline bool CheckInsideTriangle4(const __m256d& w0, const __m256d& w1, const __m256d& w2) {
    __m256d zero = _mm256_setzero_pd();
    
    // Check all >= 0
    __m256d cmp0_pos = _mm256_cmp_pd(w0, zero, _CMP_GE_OQ);
    __m256d cmp1_pos = _mm256_cmp_pd(w1, zero, _CMP_GE_OQ);
    __m256d cmp2_pos = _mm256_cmp_pd(w2, zero, _CMP_GE_OQ);
    __m256d all_pos = _mm256_and_pd(_mm256_and_pd(cmp0_pos, cmp1_pos), cmp2_pos);
    
    // Check all <= 0
    __m256d cmp0_neg = _mm256_cmp_pd(w0, zero, _CMP_LE_OQ);
    __m256d cmp1_neg = _mm256_cmp_pd(w1, zero, _CMP_LE_OQ);
    __m256d cmp2_neg = _mm256_cmp_pd(w2, zero, _CMP_LE_OQ);
    __m256d all_neg = _mm256_and_pd(_mm256_and_pd(cmp0_neg, cmp1_neg), cmp2_neg);
    
    __m256d inside = _mm256_or_pd(all_pos, all_neg);
    int mask = _mm256_movemask_pd(inside);
    return mask != 0;
}

// Get mask of which pixels are inside
inline int GetInsideMask4(const __m256d& w0, const __m256d& w1, const __m256d& w2) {
    __m256d zero = _mm256_setzero_pd();
    
    __m256d cmp0_pos = _mm256_cmp_pd(w0, zero, _CMP_GE_OQ);
    __m256d cmp1_pos = _mm256_cmp_pd(w1, zero, _CMP_GE_OQ);
    __m256d cmp2_pos = _mm256_cmp_pd(w2, zero, _CMP_GE_OQ);
    __m256d all_pos = _mm256_and_pd(_mm256_and_pd(cmp0_pos, cmp1_pos), cmp2_pos);
    
    __m256d cmp0_neg = _mm256_cmp_pd(w0, zero, _CMP_LE_OQ);
    __m256d cmp1_neg = _mm256_cmp_pd(w1, zero, _CMP_LE_OQ);
    __m256d cmp2_neg = _mm256_cmp_pd(w2, zero, _CMP_LE_OQ);
    __m256d all_neg = _mm256_and_pd(_mm256_and_pd(cmp0_neg, cmp1_neg), cmp2_neg);
    
    __m256d inside = _mm256_or_pd(all_pos, all_neg);
    return _mm256_movemask_pd(inside);
}

// Interpolate Vec3 attribute using barycentric coordinates (SIMD helper)
inline Vec3 InterpolateVec3(const Vec3& a0, const Vec3& a1, const Vec3& a2,
                            double bw0, double bw1, double bw2, double w) {
    return Vec3{
        (a0.x * bw0 + a1.x * bw1 + a2.x * bw2) * w,
        (a0.y * bw0 + a1.y * bw1 + a2.y * bw2) * w,
        (a0.z * bw0 + a1.z * bw1 + a2.z * bw2) * w
    };
}

// Interpolate Vec2 attribute using barycentric coordinates
inline Vec2 InterpolateVec2(const Vec2& a0, const Vec2& a1, const Vec2& a2,
                            double bw0, double bw1, double bw2, double w) {
    return Vec2{
        (a0.x * bw0 + a1.x * bw1 + a2.x * bw2) * w,
        (a0.y * bw0 + a1.y * bw1 + a2.y * bw2) * w
    };
}

struct RasterTriangle {
    double sx0, sy0, sx1, sy1, sx2, sy2;
    double invW0, invW1, invW2;
    Vec2 t0_over_w, t1_over_w, t2_over_w;
    Vec3 tg0_over_w, tg1_over_w, tg2_over_w;
    Vec3 n0_over_w, n1_over_w, n2_over_w;
    Vec3 w0_o_w, w1_o_w, w2_o_w;
    double z0_over_w, z1_over_w, z2_over_w;
    double zMin;
    PBRMaterial material;
    int meshIndex;
    int materialIndex;
    int primitiveIndex;
    int nodeIndex;
    int baseColorTextureIndex;
    int metallicRoughnessTextureIndex;
    int normalTextureIndex;
    int occlusionTextureIndex;
    int emissiveTextureIndex;
    int baseColorImageIndex;
    int metallicRoughnessImageIndex;
    int normalImageIndex;
    int occlusionImageIndex;
    int emissiveImageIndex;
    int baseColorSamplerIndex;
    int metallicRoughnessSamplerIndex;
    int normalSamplerIndex;
    int occlusionSamplerIndex;
    int emissiveSamplerIndex;
    int minX, maxX, minY, maxY;
    double area;
    double invArea;
    double A12, B12, C12;
    double A20, B20, C20;
    double A01, B01, C01;
};

/**
 * @brief 处理 UV 坐标的包裹模式 (重复、镜像、拉伸)
 */
double WrapCoord(double v, int mode) {
    if (mode == 33071) { // CLAMP_TO_EDGE
        return std::max(0.0, std::min(1.0, v));
    }
    if (mode == 33648) { // MIRRORED_REPEAT
        double w = std::fmod(v, 2.0);
        if (w < 0.0) w += 2.0;
        return (w > 1.0) ? (2.0 - w) : w;
    }
    double w = std::fmod(v, 1.0);
    if (w < 0.0) w += 1.0;
    return w;
}

/**
 * @brief 采样基础色贴图的 Alpha 通道进行 Alpha 测试
 */
double SampleBaseColorAlpha(const FrameContext& context, int imageIndex, int samplerIndex, const Vec2& uv) {
    if (!context.images || imageIndex < 0 || imageIndex >= static_cast<int>(context.images->size())) {
        return 1.0;
    }
    const GLTFImage& image = (*context.images)[imageIndex];
    const GLTFSampler* sampler = nullptr;
    if (context.samplers && samplerIndex >= 0 && samplerIndex < static_cast<int>(context.samplers->size())) {
        sampler = &(*context.samplers)[samplerIndex];
    }

    int wrapS = sampler ? sampler->wrapS : 10497;
    int wrapT = sampler ? sampler->wrapT : 10497;
    double u = WrapCoord(uv.x, wrapS);
    double v = WrapCoord(uv.y, wrapT);
    int x = static_cast<int>(std::floor(u * image.width));
    int y = static_cast<int>(std::floor((1.0 - v) * image.height));
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= image.width) x = image.width - 1;
    if (y >= image.height) y = image.height - 1;
    size_t index = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4;
    if (index + 3 >= image.pixels.size()) {
        return 1.0;
    }
    return image.pixels[index + 3] / 255.0;
}

} // namespace

/**
 * @brief 设置光栅化渲染目标
 * @param framebuffer 帧缓冲区指针
 * @param depthBuffer 深度缓冲区指针
 */
void Rasterizer::SetTargets(Framebuffer* framebuffer, DepthBuffer* depthBuffer) {
    m_framebuffer = framebuffer;
    m_depthBuffer = depthBuffer;
}

/**
 * @brief 设置当前帧的全局渲染上下文
 */
void Rasterizer::SetFrameContext(const FrameContext& context) {
    m_frameContext = context;
}

/**
 * @brief 执行主光栅化循环
 * @param triangles 待渲染的三角形集合
 * @return 渲染统计信息
 */
RasterStats Rasterizer::RasterizeTriangles(const std::vector<Triangle>& triangles) {
    RasterStats stats{};
    if (!m_framebuffer || !m_depthBuffer) {
        return stats;
    }

    OutputDebugStringA("Rasterizer: begin\n");

    int width = m_framebuffer->GetWidth();
    int height = m_framebuffer->GetHeight();

    // Phase 1: Clip and prepare triangles
    stats.trianglesInput = static_cast<uint64_t>(triangles.size());

    std::vector<RasterTriangle> rasterTris;
    rasterTris.reserve(triangles.size() * 2);

    auto toScreenX = [width](double x) {
        return (x * 0.5 + 0.5) * static_cast<double>(width - 1);
    };
    auto toScreenY = [height](double y) {
        return (1.0 - (y * 0.5 + 0.5)) * static_cast<double>(height - 1);
    };

    Clipper clipper;

    for (const auto& tri : triangles) {
        // Backface culling moved to screen-space after projection.

        // Sutherland-Hodgman clipping
        ClipVertex a{tri.v0, tri.n0, tri.w0, tri.t0, tri.tg0};
        ClipVertex b{tri.v1, tri.n1, tri.w1, tri.t1, tri.tg1};
        ClipVertex c{tri.v2, tri.n2, tri.w2, tri.t2, tri.tg2};

        std::vector<ClipVertex> clipped = clipper.ClipTriangle(a, b, c);
        if (clipped.size() < 3) {
            continue;
        }
        stats.trianglesClipped += static_cast<uint64_t>(clipped.size() - 2);

        // Fan triangulation of clipped polygon
        for (size_t i = 1; i + 1 < clipped.size(); ++i) {
            ClipVertex v0 = clipped[0];
            ClipVertex v1 = clipped[i];
            ClipVertex v2 = clipped[i + 1];

            if (v0.clip.w <= 0.0 || v1.clip.w <= 0.0 || v2.clip.w <= 0.0) {
                continue;
            }

            RasterTriangle rt{};
            rt.invW0 = 1.0 / v0.clip.w;
            rt.invW1 = 1.0 / v1.clip.w;
            rt.invW2 = 1.0 / v2.clip.w;

            Vec4 p0{v0.clip.x * rt.invW0, v0.clip.y * rt.invW0, v0.clip.z * rt.invW0, 1.0};
            Vec4 p1{v1.clip.x * rt.invW1, v1.clip.y * rt.invW1, v1.clip.z * rt.invW1, 1.0};
            Vec4 p2{v2.clip.x * rt.invW2, v2.clip.y * rt.invW2, v2.clip.z * rt.invW2, 1.0};

            rt.sx0 = toScreenX(p0.x);
            rt.sy0 = toScreenY(p0.y);
            rt.sx1 = toScreenX(p1.x);
            rt.sy1 = toScreenY(p1.y);
            rt.sx2 = toScreenX(p2.x);
            rt.sy2 = toScreenY(p2.y);

            rt.minX = static_cast<int>(std::floor(std::min({rt.sx0, rt.sx1, rt.sx2})));
            rt.maxX = static_cast<int>(std::ceil(std::max({rt.sx0, rt.sx1, rt.sx2})));
            rt.minY = static_cast<int>(std::floor(std::min({rt.sy0, rt.sy1, rt.sy2})));
            rt.maxY = static_cast<int>(std::ceil(std::max({rt.sy0, rt.sy1, rt.sy2})));

            rt.minX = std::max(rt.minX, 0);
            rt.minY = std::max(rt.minY, 0);
            rt.maxX = std::min(rt.maxX, width - 1);
            rt.maxY = std::min(rt.maxY, height - 1);

            if (rt.minX > rt.maxX || rt.minY > rt.maxY) {
                continue;
            }

            auto edge = [](double ax, double ay, double bx, double by, double cx, double cy) {
                return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
            };

            rt.area = edge(rt.sx0, rt.sy0, rt.sx1, rt.sy1, rt.sx2, rt.sy2);
            if (rt.area == 0.0) {
                continue;
            }
            if (rt.area <= 0.0 && !tri.material.doubleSided) {
                continue;
            }

            rt.invArea = 1.0 / rt.area;
            rt.A12 = rt.sy2 - rt.sy1;
            rt.B12 = rt.sx1 - rt.sx2;
            rt.C12 = rt.sx2 * rt.sy1 - rt.sx1 * rt.sy2;

            rt.A20 = rt.sy0 - rt.sy2;
            rt.B20 = rt.sx2 - rt.sx0;
            rt.C20 = rt.sx0 * rt.sy2 - rt.sx2 * rt.sy0;

            rt.A01 = rt.sy1 - rt.sy0;
            rt.B01 = rt.sx0 - rt.sx1;
            rt.C01 = rt.sx1 * rt.sy0 - rt.sx0 * rt.sy1;

            rt.n0_over_w = v0.normal * rt.invW0;
            rt.n1_over_w = v1.normal * rt.invW1;
            rt.n2_over_w = v2.normal * rt.invW2;

            // Fix UV wrap-around at poles (when triangle spans 0/1 boundary)
            Vec2 t0 = tri.t0;
            Vec2 t1 = tri.t1;
            Vec2 t2 = tri.t2;
            
            // Check U seam
            bool t0NearZeroU = t0.x < 0.25;
            bool t1NearZeroU = t1.x < 0.25;
            bool t2NearZeroU = t2.x < 0.25;
            bool t0NearOneU  = t0.x > 0.75;
            bool t1NearOneU  = t1.x > 0.75;
            bool t2NearOneU  = t2.x > 0.75;
            
            bool hasNearZeroU = t0NearZeroU || t1NearZeroU || t2NearZeroU;
            bool hasNearOneU  = t0NearOneU  || t1NearOneU  || t2NearOneU;
            
            // Only apply fix when we have vertices on both sides of the seam
            if (hasNearZeroU && hasNearOneU) {
                int countNearZero = (t0NearZeroU ? 1 : 0) + (t1NearZeroU ? 1 : 0) + (t2NearZeroU ? 1 : 0);
                int countNearOne  = (t0NearOneU  ? 1 : 0) + (t1NearOneU  ? 1 : 0) + (t2NearOneU  ? 1 : 0);
                
                if (countNearZero <= countNearOne) {
                    // Wrap vertices near 0 to be near 1
                    if (t0NearZeroU) t0.x += 1.0;
                    if (t1NearZeroU) t1.x += 1.0;
                    if (t2NearZeroU) t2.x += 1.0;
                } else {
                    // Wrap vertices near 1 to be near 0
                    if (t0NearOneU) t0.x -= 1.0;
                    if (t1NearOneU) t1.x -= 1.0;
                    if (t2NearOneU) t2.x -= 1.0;
                }
            }

            // Check V seam (for polar caps)
            bool t0NearZeroV = t0.y < 0.25;
            bool t1NearZeroV = t1.y < 0.25;
            bool t2NearZeroV = t2.y < 0.25;
            bool t0NearOneV  = t0.y > 0.75;
            bool t1NearOneV  = t1.y > 0.75;
            bool t2NearOneV  = t2.y > 0.75;

            bool hasNearZeroV = t0NearZeroV || t1NearZeroV || t2NearZeroV;
            bool hasNearOneV  = t0NearOneV  || t1NearOneV  || t2NearOneV;

            if (hasNearZeroV && hasNearOneV) {
                int countNearZero = (t0NearZeroV ? 1 : 0) + (t1NearZeroV ? 1 : 0) + (t2NearZeroV ? 1 : 0);
                int countNearOne  = (t0NearOneV  ? 1 : 0) + (t1NearOneV  ? 1 : 0) + (t2NearOneV  ? 1 : 0);

                if (countNearZero <= countNearOne) {
                    if (t0NearZeroV) t0.y += 1.0;
                    if (t1NearZeroV) t1.y += 1.0;
                    if (t2NearZeroV) t2.y += 1.0;
                } else {
                    if (t0NearOneV) t0.y -= 1.0;
                    if (t1NearOneV) t1.y -= 1.0;
                    if (t2NearOneV) t2.y -= 1.0;
                }
            }

            rt.t0_over_w = t0 * rt.invW0;
            rt.t1_over_w = t1 * rt.invW1;
            rt.t2_over_w = t2 * rt.invW2;

            rt.tg0_over_w = v0.tangent * rt.invW0;
            rt.tg1_over_w = v1.tangent * rt.invW1;
            rt.tg2_over_w = v2.tangent * rt.invW2;

            rt.w0_o_w = v0.world * rt.invW0;
            rt.w1_o_w = v1.world * rt.invW1;
            rt.w2_o_w = v2.world * rt.invW2;

            rt.z0_over_w = p0.z;
            rt.z1_over_w = p1.z;
            rt.z2_over_w = p2.z;
            rt.zMin = std::min({rt.z0_over_w, rt.z1_over_w, rt.z2_over_w});

            rt.material = tri.material;
            rt.meshIndex = tri.meshIndex;
            rt.materialIndex = tri.materialIndex;
            rt.primitiveIndex = tri.primitiveIndex;
            rt.nodeIndex = tri.nodeIndex;
            rt.baseColorTextureIndex = tri.baseColorTextureIndex;
            rt.metallicRoughnessTextureIndex = tri.metallicRoughnessTextureIndex;
            rt.normalTextureIndex = tri.normalTextureIndex;
            rt.occlusionTextureIndex = tri.occlusionTextureIndex;
            rt.emissiveTextureIndex = tri.emissiveTextureIndex;
            rt.baseColorImageIndex = tri.baseColorImageIndex;
            rt.metallicRoughnessImageIndex = tri.metallicRoughnessImageIndex;
            rt.normalImageIndex = tri.normalImageIndex;
            rt.occlusionImageIndex = tri.occlusionImageIndex;
            rt.emissiveImageIndex = tri.emissiveImageIndex;
            rt.baseColorSamplerIndex = tri.baseColorSamplerIndex;
            rt.metallicRoughnessSamplerIndex = tri.metallicRoughnessSamplerIndex;
            rt.normalSamplerIndex = tri.normalSamplerIndex;
            rt.occlusionSamplerIndex = tri.occlusionSamplerIndex;
            rt.emissiveSamplerIndex = tri.emissiveSamplerIndex;

            rasterTris.push_back(rt);
        }
    }

    stats.trianglesRaster = static_cast<uint64_t>(rasterTris.size());

    {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer),
            "Rasterizer: prepared tris=%zu input=%llu clipped=%llu\n",
            rasterTris.size(),
            static_cast<unsigned long long>(stats.trianglesInput),
            static_cast<unsigned long long>(stats.trianglesClipped));
        OutputDebugStringA(buffer);
    }

    // Phase 2: Tile-based rasterization in parallel (lock-free)
    double* depthData = m_depthBuffer->Data();
    if (!depthData) {
        return stats;
    }
    
    // Get direct write access to framebuffer for faster pixel writes
    Vec3* linearPixels = m_framebuffer->GetLinearPixelsWritable();

    constexpr int TILE_SIZE = 32;
    int tilesX = (width + TILE_SIZE - 1) / TILE_SIZE;
    int tilesY = (height + TILE_SIZE - 1) / TILE_SIZE;

    int totalTiles = tilesX * tilesY;
    std::vector<double> tileMaxDepths(static_cast<size_t>(totalTiles), 1.0);
    std::vector<int> tileMinXs(static_cast<size_t>(totalTiles));
    std::vector<int> tileMinYs(static_cast<size_t>(totalTiles));
    std::vector<int> tileMaxXs(static_cast<size_t>(totalTiles));
    std::vector<int> tileMaxYs(static_cast<size_t>(totalTiles));
    for (int t = 0; t < totalTiles; ++t) {
        int ty = t / tilesX;
        int tx = t - ty * tilesX;
        int tileMinX = tx * TILE_SIZE;
        int tileMinY = ty * TILE_SIZE;
        int tileMaxX = std::min(tileMinX + TILE_SIZE - 1, width - 1);
        int tileMaxY = std::min(tileMinY + TILE_SIZE - 1, height - 1);
        tileMinXs[static_cast<size_t>(t)] = tileMinX;
        tileMinYs[static_cast<size_t>(t)] = tileMinY;
        tileMaxXs[static_cast<size_t>(t)] = tileMaxX;
        tileMaxYs[static_cast<size_t>(t)] = tileMaxY;
    }

    // Bin triangles into tiles to avoid O(tiles * tris)
    // Also compute conservative triangle zMax for Hi-Z culling
    std::vector<std::vector<size_t>> tileBins(static_cast<size_t>(totalTiles));
    std::vector<double> triZMax(rasterTris.size());  // Conservative max depth per triangle
    for (size_t i = 0; i < rasterTris.size(); ++i) {
        const RasterTriangle& rt = rasterTris[i];
        // Compute zMax for this triangle
        triZMax[i] = std::max({rt.z0_over_w, rt.z1_over_w, rt.z2_over_w});
        
        int minTileX = rt.minX / TILE_SIZE;
        int maxTileX = rt.maxX / TILE_SIZE;
        int minTileY = rt.minY / TILE_SIZE;
        int maxTileY = rt.maxY / TILE_SIZE;
        minTileX = std::max(minTileX, 0);
        minTileY = std::max(minTileY, 0);
        maxTileX = std::min(maxTileX, tilesX - 1);
        maxTileY = std::min(maxTileY, tilesY - 1);
        for (int ty = minTileY; ty <= maxTileY; ++ty) {
            int rowBase = ty * tilesX;
            for (int tx = minTileX; tx <= maxTileX; ++tx) {
                tileBins[static_cast<size_t>(rowBase + tx)].push_back(i);
            }
        }
    }

    // Sort each tile's triangles by zMin (front-to-back) for early-z benefit
    #pragma omp parallel for schedule(static)
    for (int t = 0; t < totalTiles; ++t) {
        auto& bin = tileBins[static_cast<size_t>(t)];
        std::sort(bin.begin(), bin.end(), [&rasterTris](size_t a, size_t b) {
            return rasterTris[a].zMin < rasterTris[b].zMin;
        });
    }

    size_t totalBinRefs = 0;
    size_t maxBinSize = 0;
    for (const auto& bin : tileBins) {
        totalBinRefs += bin.size();
        if (bin.size() > maxBinSize) {
            maxBinSize = bin.size();
        }
    }
    {
        char buffer[256];
        double avgBin = totalTiles > 0 ? static_cast<double>(totalBinRefs) / static_cast<double>(totalTiles) : 0.0;
        std::snprintf(buffer, sizeof(buffer),
            "Rasterizer: bin refs=%zu avgBin=%.1f maxBin=%zu\n",
            totalBinRefs, avgBin, maxBinSize);
        OutputDebugStringA(buffer);
    }

    {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "Rasterizer: tiles=%d (%d x %d)\n", totalTiles, tilesX, tilesY);
        OutputDebugStringA(buffer);
    }

    #pragma omp parallel for schedule(static)
    for (int t = 0; t < totalTiles; ++t) {
        int tileMinX = tileMinXs[static_cast<size_t>(t)];
        int tileMinY = tileMinYs[static_cast<size_t>(t)];
        int tileMaxX = tileMaxXs[static_cast<size_t>(t)];
        int tileMaxY = tileMaxYs[static_cast<size_t>(t)];

        double tileMaxDepth = 0.0;
        for (int y = tileMinY; y <= tileMaxY; ++y) {
            int rowBase = y * width;
            for (int x = tileMinX; x <= tileMaxX; ++x) {
                tileMaxDepth = std::max(tileMaxDepth, depthData[rowBase + x]);
            }
        }
        tileMaxDepths[static_cast<size_t>(t)] = tileMaxDepth;
    }

    OutputDebugStringA("Rasterizer: tile max depth pass done\n");

    OutputDebugStringA("Rasterizer: tile raster pass start\n");

    // Precompute light data ONCE for entire frame (avoid per-work-item computation)
    std::vector<PrecomputedLight> globalPrecomputedLights;
    if (!m_frameContext.lights.empty()) {
        globalPrecomputedLights.reserve(m_frameContext.lights.size());
        for (const DirectionalLight& light : m_frameContext.lights) {
            PrecomputedLight pl;
            pl.L = Vec3{-light.direction.x, -light.direction.y, -light.direction.z}.Normalized();
            pl.radiance = light.color * light.intensity;
            globalPrecomputedLights.push_back(pl);
        }
    }

    #pragma omp parallel
    {
        FragmentShader fragmentShader;

        // Parallelize by tile to avoid cross-thread writes to the same pixels
        #pragma omp for schedule(dynamic, 1)
        for (int t = 0; t < totalTiles; ++t) {
            const auto& bin = tileBins[static_cast<size_t>(t)];

            int tileMinX = tileMinXs[static_cast<size_t>(t)];
            int tileMinY = tileMinYs[static_cast<size_t>(t)];
            int tileMaxX = tileMaxXs[static_cast<size_t>(t)];
            int tileMaxY = tileMaxYs[static_cast<size_t>(t)];
            for (size_t triIndex : bin) {
                const RasterTriangle& rt = rasterTris[triIndex];
                // Skip if triangle's nearest point is behind tile's farthest written depth
                double tileMaxDepth = tileMaxDepths[static_cast<size_t>(t)];
                if (rt.zMin > tileMaxDepth) {
                    continue;
                }
                
                // Build per-triangle fragment context ONCE (not per-pixel!)
                FragmentContext fragCtx;
                fragCtx.cameraPos = m_frameContext.cameraPos;
                fragCtx.material = &rt.material;
                fragCtx.lights = &m_frameContext.lights;
                fragCtx.ambientColor = m_frameContext.ambientColor;
                fragCtx.images = m_frameContext.images;
                fragCtx.samplers = m_frameContext.samplers;
                fragCtx.baseColorImageIndex = rt.baseColorImageIndex;
                fragCtx.metallicRoughnessImageIndex = rt.metallicRoughnessImageIndex;
                fragCtx.normalImageIndex = rt.normalImageIndex;
                fragCtx.occlusionImageIndex = rt.occlusionImageIndex;
                fragCtx.emissiveImageIndex = rt.emissiveImageIndex;
                fragCtx.baseColorSamplerIndex = rt.baseColorSamplerIndex;
                fragCtx.metallicRoughnessSamplerIndex = rt.metallicRoughnessSamplerIndex;
                fragCtx.normalSamplerIndex = rt.normalSamplerIndex;
                fragCtx.occlusionSamplerIndex = rt.occlusionSamplerIndex;
                fragCtx.emissiveSamplerIndex = rt.emissiveSamplerIndex;
                
                // Use globally precomputed light data (pointer, no copy)
                fragCtx.precomputedLights = globalPrecomputedLights.empty() ? nullptr : globalPrecomputedLights.data();
                fragCtx.precomputedLightCount = globalPrecomputedLights.size();

                int minX = std::max(rt.minX, tileMinX);
                int maxX = std::min(rt.maxX, tileMaxX);
                int minY = std::max(rt.minY, tileMinY);
                int maxY = std::min(rt.maxY, tileMaxY);

                // Precompute row base for edge function
                double pyBase = static_cast<double>(minY) + 0.5;
                double pxStart = static_cast<double>(minX) + 0.5;
                
                // Initial edge values at (minX, minY)
                double w0_row = rt.A12 * pxStart + rt.B12 * pyBase + rt.C12;
                double w1_row = rt.A20 * pxStart + rt.B20 * pyBase + rt.C20;
                double w2_row = rt.A01 * pxStart + rt.B01 * pyBase + rt.C01;

                // Check if we need alpha testing or alpha blending for this triangle
                const bool needsAlphaTest = (rt.material.alphaMode == 1 && rt.baseColorImageIndex >= 0);
                const bool needsAlphaBlend = (rt.material.alphaMode == 2);

                // SIMD constants for edge function increments (4 pixels at a time)
                const __m256d A12_4 = _mm256_set1_pd(rt.A12);
                const __m256d A20_4 = _mm256_set1_pd(rt.A20);
                const __m256d A01_4 = _mm256_set1_pd(rt.A01);
                const __m256d invArea_4 = _mm256_set1_pd(rt.invArea);
                const __m256d z0_4 = _mm256_set1_pd(rt.z0_over_w);
                const __m256d z1_4 = _mm256_set1_pd(rt.z1_over_w);
                const __m256d z2_4 = _mm256_set1_pd(rt.z2_over_w);
                const __m256d invW0_4 = _mm256_set1_pd(rt.invW0);
                const __m256d invW1_4 = _mm256_set1_pd(rt.invW1);
                const __m256d invW2_4 = _mm256_set1_pd(rt.invW2);
                const __m256d zero_4 = _mm256_setzero_pd();
                const __m256d xOffset_4 = _mm256_set_pd(3.0, 2.0, 1.0, 0.0);

                for (int y = minY; y <= maxY; ++y) {
                    double w0 = w0_row;
                    double w1 = w1_row;
                    double w2 = w2_row;
                    int rowBase = y * width;

                    int x = minX;
                    
                    // SIMD path: process 4 pixels at a time
                    const int simdEnd = minX + ((maxX - minX + 1) / 4) * 4;
                    for (; x < simdEnd; x += 4) {
                        // Compute edge values for 4 pixels
                        __m256d offset = _mm256_mul_pd(xOffset_4, A12_4);
                        __m256d w0_4 = _mm256_add_pd(_mm256_set1_pd(w0), offset);
                        offset = _mm256_mul_pd(xOffset_4, A20_4);
                        __m256d w1_4 = _mm256_add_pd(_mm256_set1_pd(w1), offset);
                        offset = _mm256_mul_pd(xOffset_4, A01_4);
                        __m256d w2_4 = _mm256_add_pd(_mm256_set1_pd(w2), offset);

                        // Quick check: any pixels inside?
                        int insideMask = GetInsideMask4(w0_4, w1_4, w2_4);
                        if (insideMask == 0) {
                            // Skip all 4 pixels
                            w0 += rt.A12 * 4.0;
                            w1 += rt.A20 * 4.0;
                            w2 += rt.A01 * 4.0;
                            continue;
                        }

                        // Compute barycentric coordinates for 4 pixels
                        __m256d bw0_4 = _mm256_mul_pd(w0_4, invArea_4);
                        __m256d bw1_4 = _mm256_mul_pd(w1_4, invArea_4);
                        __m256d bw2_4 = _mm256_mul_pd(w2_4, invArea_4);

                        // Compute depths for 4 pixels
                        __m256d depth_4 = _mm256_add_pd(_mm256_add_pd(
                            _mm256_mul_pd(bw0_4, z0_4),
                            _mm256_mul_pd(bw1_4, z1_4)),
                            _mm256_mul_pd(bw2_4, z2_4));

                        // Compute invW for 4 pixels
                        __m256d invW_4 = _mm256_add_pd(_mm256_add_pd(
                            _mm256_mul_pd(bw0_4, invW0_4),
                            _mm256_mul_pd(bw1_4, invW1_4)),
                            _mm256_mul_pd(bw2_4, invW2_4));

                        // Extract and process each pixel
                        alignas(32) double depths[4];
                        alignas(32) double bw0s[4], bw1s[4], bw2s[4], invWs[4];
                        _mm256_store_pd(depths, depth_4);
                        _mm256_store_pd(bw0s, bw0_4);
                        _mm256_store_pd(bw1s, bw1_4);
                        _mm256_store_pd(bw2s, bw2_4);
                        _mm256_store_pd(invWs, invW_4);

                        for (int i = 0; i < 4; ++i) {
                            if (!(insideMask & (1 << i))) continue;
                            
                            int px = x + i;
                            int index = rowBase + px;
                            double depth = depths[i];
                            
                            if (depth < 0.0 || depth >= depthData[index]) continue;
                            if (invWs[i] <= 0.0) continue;

                            double bw0 = bw0s[i];
                            double bw1 = bw1s[i];
                            double bw2 = bw2s[i];
                            double wVal = 1.0 / invWs[i];

                            // Build per-pixel varying (tiny struct)
                            FragmentVarying varying;
                            varying.normal = InterpolateVec3(rt.n0_over_w, rt.n1_over_w, rt.n2_over_w, bw0, bw1, bw2, wVal);
                            varying.worldPos = InterpolateVec3(rt.w0_o_w, rt.w1_o_w, rt.w2_o_w, bw0, bw1, bw2, wVal);
                            varying.texCoord = InterpolateVec2(rt.t0_over_w, rt.t1_over_w, rt.t2_over_w, bw0, bw1, bw2, wVal);
                            varying.tangent = (rt.normalImageIndex >= 0) 
                                ? InterpolateVec3(rt.tg0_over_w, rt.tg1_over_w, rt.tg2_over_w, bw0, bw1, bw2, wVal)
                                : Vec3{0.0, 0.0, 0.0};

                            double alpha = rt.material.alpha;
                            if (rt.baseColorImageIndex >= 0) {
                                alpha *= SampleBaseColorAlpha(m_frameContext, rt.baseColorImageIndex, rt.baseColorSamplerIndex, varying.texCoord);
                            }
                            if (needsAlphaTest && alpha < rt.material.alphaCutoff) continue;

                            Vec3 shaded = fragmentShader.ShadeFast(fragCtx, varying);
                            // Alpha blend if needed (do not write depth for translucent fragments)
                            if (needsAlphaBlend && alpha < 0.999) {
                                Vec3 dst = linearPixels[y * width + px];
                                linearPixels[y * width + px] = shaded * alpha + dst * (1.0 - alpha);
                            } else {
                                depthData[index] = depth;
                                linearPixels[y * width + px] = shaded;
                            }
                        }

                        w0 += rt.A12 * 4.0;
                        w1 += rt.A20 * 4.0;
                        w2 += rt.A01 * 4.0;
                    }

                    // Scalar path: process remaining pixels
                    for (; x <= maxX; ++x) {
                        // Fast inside test
                        bool allPos = (w0 >= 0.0) & (w1 >= 0.0) & (w2 >= 0.0);
                        bool allNeg = (w0 <= 0.0) & (w1 <= 0.0) & (w2 <= 0.0);
                        if (!allPos && !allNeg) {
                            w0 += rt.A12;
                            w1 += rt.A20;
                            w2 += rt.A01;
                            continue;
                        }

                        double bw0 = w0 * rt.invArea;
                        double bw1 = w1 * rt.invArea;
                        double bw2 = w2 * rt.invArea;

                        // Early depth test before expensive interpolation
                        double depth = bw0 * rt.z0_over_w + bw1 * rt.z1_over_w + bw2 * rt.z2_over_w;
                        int index = rowBase + x;
                        if (depth < 0.0 || depth >= depthData[index]) {
                            w0 += rt.A12;
                            w1 += rt.A20;
                            w2 += rt.A01;
                            continue;
                        }

                        double invW = bw0 * rt.invW0 + bw1 * rt.invW1 + bw2 * rt.invW2;
                        if (invW <= 0.0) {
                            w0 += rt.A12;
                            w1 += rt.A20;
                            w2 += rt.A01;
                            continue;
                        }

                        double wVal = 1.0 / invW;

                        // Build per-pixel varying (tiny struct)
                        FragmentVarying varying;
                        varying.normal = InterpolateVec3(rt.n0_over_w, rt.n1_over_w, rt.n2_over_w, bw0, bw1, bw2, wVal);
                        varying.worldPos = InterpolateVec3(rt.w0_o_w, rt.w1_o_w, rt.w2_o_w, bw0, bw1, bw2, wVal);
                        varying.texCoord = InterpolateVec2(rt.t0_over_w, rt.t1_over_w, rt.t2_over_w, bw0, bw1, bw2, wVal);
                        varying.tangent = (rt.normalImageIndex >= 0)
                            ? InterpolateVec3(rt.tg0_over_w, rt.tg1_over_w, rt.tg2_over_w, bw0, bw1, bw2, wVal)
                            : Vec3{0.0, 0.0, 0.0};

                        double alpha = rt.material.alpha;
                        if (rt.baseColorImageIndex >= 0) {
                            alpha *= SampleBaseColorAlpha(m_frameContext, rt.baseColorImageIndex, rt.baseColorSamplerIndex, varying.texCoord);
                        }
                        if (needsAlphaTest && alpha < rt.material.alphaCutoff) {
                            w0 += rt.A12;
                            w1 += rt.A20;
                            w2 += rt.A01;
                            continue;
                        }

                        Vec3 shaded = fragmentShader.ShadeFast(fragCtx, varying);
                        // Alpha blend if needed (do not write depth for translucent fragments)
                        if (needsAlphaBlend && alpha < 0.999) {
                            Vec3 dst = linearPixels[rowBase + x];
                            linearPixels[rowBase + x] = shaded * alpha + dst * (1.0 - alpha);
                        } else {
                            depthData[index] = depth;
                            linearPixels[rowBase + x] = shaded;
                        }

                        w0 += rt.A12;
                        w1 += rt.A20;
                        w2 += rt.A01;
                    }
                    // Advance to next row
                    w0_row += rt.B12;
                    w1_row += rt.B20;
                    w2_row += rt.B01;
                }
            }
        }
    } // end omp parallel

    OutputDebugStringA("Rasterizer: tile raster pass done\n");

    OutputDebugStringA("Rasterizer: end\n");
    return stats;
}

} // namespace SR
