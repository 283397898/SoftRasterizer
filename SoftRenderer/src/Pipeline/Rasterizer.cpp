#include "Pipeline/Rasterizer.h"

#include "Pipeline/FragmentShader.h"
#include "Pipeline/Clipper.h"
#include "Pipeline/MaterialTable.h"
#include "Utils/DebugLog.h"
#include "Utils/TextureSampler.h"

#include "Asset/GLTFTypes.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cmath>
#include <vector>
#include <cstdio>
#include <omp.h>

#include <immintrin.h>

namespace SR {

namespace {

// ============================================================================
// AVX2 SIMD 辅助函数（256 位宽，一次处理 4 个 double）
// ============================================================================

/// @brief 快速判断 4 组重心坐标是否全部落在三角形内（全 ≥0 或全 ≤0）
inline bool CheckInsideTriangle4(const __m256d& w0, const __m256d& w1, const __m256d& w2) {
    __m256d zero = _mm256_setzero_pd();
    
    // 检查所有通道是否 >= 0（三角形正面测试）
    __m256d cmp0_pos = _mm256_cmp_pd(w0, zero, _CMP_GE_OQ);
    __m256d cmp1_pos = _mm256_cmp_pd(w1, zero, _CMP_GE_OQ);
    __m256d cmp2_pos = _mm256_cmp_pd(w2, zero, _CMP_GE_OQ);
    __m256d all_pos = _mm256_and_pd(_mm256_and_pd(cmp0_pos, cmp1_pos), cmp2_pos);
    
    // 检查所有通道是否 <= 0（三角形背面测试）
    __m256d cmp0_neg = _mm256_cmp_pd(w0, zero, _CMP_LE_OQ);
    __m256d cmp1_neg = _mm256_cmp_pd(w1, zero, _CMP_LE_OQ);
    __m256d cmp2_neg = _mm256_cmp_pd(w2, zero, _CMP_LE_OQ);
    __m256d all_neg = _mm256_and_pd(_mm256_and_pd(cmp0_neg, cmp1_neg), cmp2_neg);
    
    __m256d inside = _mm256_or_pd(all_pos, all_neg);
    int mask = _mm256_movemask_pd(inside);
    return mask != 0;
}

/// @brief 返回 4 个像素中各自是否在三角形内的位掩码（bit i = 1 表示第 i 个像素在内）
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

/// @brief 使用重心坐标对 Vec3 属性进行透视正确插值（除以 w 恢复线性空间）
inline Vec3 InterpolateVec3(const Vec3& a0, const Vec3& a1, const Vec3& a2,
                            double bw0, double bw1, double bw2, double w) {
    return Vec3{
        (a0.x * bw0 + a1.x * bw1 + a2.x * bw2) * w,
        (a0.y * bw0 + a1.y * bw1 + a2.y * bw2) * w,
        (a0.z * bw0 + a1.z * bw1 + a2.z * bw2) * w
    };
}

/// @brief 使用重心坐标对 Vec2 属性进行透视正确插值
inline Vec2 InterpolateVec2(const Vec2& a0, const Vec2& a1, const Vec2& a2,
                            double bw0, double bw1, double bw2, double w) {
    return Vec2{
        (a0.x * bw0 + a1.x * bw1 + a2.x * bw2) * w,
        (a0.y * bw0 + a1.y * bw1 + a2.y * bw2) * w
    };
}

/// @brief 使用重心坐标对 Vec4 属性进行透视正确插值
inline Vec4 InterpolateVec4(const Vec4& a0, const Vec4& a1, const Vec4& a2,
                            double bw0, double bw1, double bw2, double w) {
    return Vec4{
        (a0.x * bw0 + a1.x * bw1 + a2.x * bw2) * w,
        (a0.y * bw0 + a1.y * bw1 + a2.y * bw2) * w,
        (a0.z * bw0 + a1.z * bw1 + a2.z * bw2) * w,
        (a0.w * bw0 + a1.w * bw1 + a2.w * bw2) * w
    };
}

/**
 * @brief 光栅化阶段的三角形中间表示
 *
 * 包含屏幕空间坐标、透视除法后的属性、材质数据和边函数系数。
 * 所有顶点属性均已除以 w（为透视正确插值做准备）。
 */
struct RasterTriangle {
    double sx0, sy0, sx1, sy1, sx2, sy2; ///< 屏幕空间顶点坐标
    double invW0, invW1, invW2;           ///< 各顶点的 1/w（用于透视插值）

    Vec2 t0_over_w,   t1_over_w,   t2_over_w;   ///< 主 UV / w
    Vec2 t0_1_over_w, t1_1_over_w, t2_1_over_w; ///< 次 UV / w
    Vec4 c0_over_w,   c1_over_w,   c2_over_w;   ///< 顶点颜色 / w
    Vec3 tg0_over_w,  tg1_over_w,  tg2_over_w;  ///< 切线 / w
    double tangentW;                              ///< 切线 W 分量（副切线方向符号）
    Vec3 n0_over_w, n1_over_w, n2_over_w;        ///< 法线 / w
    Vec3 w0_o_w,    w1_o_w,    w2_o_w;           ///< 世界坐标 / w
    double z0_over_w, z1_over_w, z2_over_w;      ///< NDC 深度值（已透视除法）
    double zMin;                                   ///< 三角形最小深度（用于深度排序）

    MaterialHandle materialId; ///< 材质句柄（引用 MaterialTable）

    // 从 MaterialTable 复制的材质属性（热路径快速访问，避免间接寻址）
    Vec3  albedo;
    double metallic;
    double roughness;
    bool   doubleSided;
    double alpha;
    double transmissionFactor;
    GLTFAlphaMode alphaMode;
    double alphaCutoff;
    Vec3   emissiveFactor;
    double ior;
    double specularFactor;
    Vec3   specularColorFactor;

    TextureBindingArray textures; ///< 纹理绑定数组（从 MaterialTable 复制）

    int minX, maxX, minY, maxY; ///< 屏幕空间包围盒（像素坐标）
    double area;     ///< 有向面积（用于确定方向）
    double invArea;  ///< 面积倒数（用于重心坐标归一化）

    // 边函数系数（增量光栅化：w_i = A*x + B*y + C）
    double A12, B12, C12; ///< 边 v1→v2 的系数
    double A20, B20, C20; ///< 边 v2→v0 的系数
    double A01, B01, C01; ///< 边 v0→v1 的系数
};

/**
 * @brief 光栅化阶段复用的临时缓冲区（线程局部存储，避免重复堆分配）
 */
struct RasterScratchBuffers {
    std::vector<RasterTriangle> rasterTris;  ///< 裁剪后的光栅化三角形列表

    // Tile 坐标缓存（仅在分辨率变化时重建）
    std::vector<int> tileMinXs;
    std::vector<int> tileMinYs;
    std::vector<int> tileMaxXs;
    std::vector<int> tileMaxYs;

    // Tile Binning 数据结构（两遍统计 + 前缀和 + 填充）
    std::vector<size_t> binCounts;      ///< 每个 Tile 的三角形引用计数
    std::vector<size_t> binOffsets;     ///< 前缀和：Tile 在 binTriIndices 中的起始位置
    std::vector<size_t> binWriteCursor; ///< 填充阶段的写游标（原子操作）
    std::vector<size_t> binTriIndices;  ///< 各 Tile 引用的三角形索引数组（紧密存储）

    // 每个三角形覆盖的 Tile 范围
    std::vector<int> triMinTileX;
    std::vector<int> triMaxTileX;
    std::vector<int> triMinTileY;
    std::vector<int> triMaxTileY;

    // 缓存的分辨率信息（用于判断是否需要重建 Tile 网格）
    int cachedWidth   = -1;
    int cachedHeight  = -1;
    int cachedTilesX  = -1;
    int cachedTilesY  = -1;
};

/// 每线程独立的光栅化临时缓冲区（避免线程间竞争）
thread_local RasterScratchBuffers g_rasterScratch;

double SampleTextureChannel(const FrameContext& context, int imageIndex, int samplerIndex, const Vec2& uv, int channel) {
    if (!context.images || imageIndex < 0 || imageIndex >= static_cast<int>(context.images->size())) {
        return 1.0;
    }
    const GLTFImage& image = (*context.images)[imageIndex];
    const GLTFSampler* sampler = nullptr;
    if (context.samplers && samplerIndex >= 0 && samplerIndex < static_cast<int>(context.samplers->size())) {
        sampler = &(*context.samplers)[samplerIndex];
    }
    SampledColor sampled = SampleImageNearest(image, sampler, uv, false);
    switch (channel) {
    case 0: return sampled.rgb.x;
    case 1: return sampled.rgb.y;
    case 2: return sampled.rgb.z;
    case 3: return sampled.a;
    default: return 1.0;
    }
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

    SR_DEBUG_LOG("Rasterizer: begin\n");

    int width = m_framebuffer->GetWidth();
    int height = m_framebuffer->GetHeight();

    // ── 阶段一：裁剪并准备光栅化三角形 ──────────────────────────────────
    stats.trianglesInput = static_cast<uint64_t>(triangles.size());

    RasterScratchBuffers& scratch = g_rasterScratch;
    std::vector<RasterTriangle>& rasterTris = scratch.rasterTris;
    rasterTris.clear();

    auto toScreenX = [width](double x) {
        return (x * 0.5 + 0.5) * static_cast<double>(width - 1);
    };
    auto toScreenY = [height](double y) {
        return (1.0 - (y * 0.5 + 0.5)) * static_cast<double>(height - 1);
    };

    // 并行裁剪：每线程独立 Clipper + 本地三角形列表，避免锁竞争
    const int numInputTris = static_cast<int>(triangles.size());
    const int maxClipThreads = omp_get_max_threads();
    std::vector<std::vector<RasterTriangle>> perThreadClipTris(maxClipThreads);
    std::vector<uint64_t> perThreadClipCount(maxClipThreads, 0);

    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        auto& localTris = perThreadClipTris[tid];
        localTris.clear();
        localTris.reserve(static_cast<size_t>(numInputTris / omp_get_num_threads()) * 2 + 16);
        uint64_t localClipped = 0;
        Clipper clipper;

        #pragma omp for schedule(static)
        for (int triIdx = 0; triIdx < numInputTris; ++triIdx) {
            const auto& tri = triangles[static_cast<size_t>(triIdx)];
        // 背面剔除在透视除法后的屏幕空间中执行（见后续有向面积符号判断）

        // Sutherland-Hodgman 视锥体裁剪
        ClipVertex a{tri.v0, tri.n0, tri.w0, tri.t0, tri.t0_1, tri.c0, tri.tg0};
        ClipVertex b{tri.v1, tri.n1, tri.w1, tri.t1, tri.t1_1, tri.c1, tri.tg1};
        ClipVertex c{tri.v2, tri.n2, tri.w2, tri.t2, tri.t2_1, tri.c2, tri.tg2};

        std::vector<ClipVertex> clipped = clipper.ClipTriangle(a, b, c);
        if (clipped.size() < 3) {
            continue;
        }
        localClipped += static_cast<uint64_t>(clipped.size() - 2);

        // 扇形三角化：将裁剪后的凸多边形拆分为三角形
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

            // 从 MaterialTable 获取材质属性（通过句柄，O(1) 访问）
            const MaterialTable* matTable = m_frameContext.materialTable;
            MaterialHandle matId = tri.materialId;

            // 背面剔除：有向面积 <= 0 表示背面，双面材质不剔除
            bool doubleSided = matTable ? matTable->GetDoubleSided(matId) : false;
            if (rt.area <= 0.0 && !doubleSided) {
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

            // 修正极点处 UV 环绕问题（三角形跨越 0/1 边界时）
            Vec2 t0 = tri.t0;
            Vec2 t1 = tri.t1;
            Vec2 t2 = tri.t2;
            Vec2 t0_1 = tri.t0_1;
            Vec2 t1_1 = tri.t1_1;
            Vec2 t2_1 = tri.t2_1;
            
            // 检测 U 方向接缝
            bool t0NearZeroU = t0.x < 0.25;
            bool t1NearZeroU = t1.x < 0.25;
            bool t2NearZeroU = t2.x < 0.25;
            bool t0NearOneU  = t0.x > 0.75;
            bool t1NearOneU  = t1.x > 0.75;
            bool t2NearOneU  = t2.x > 0.75;
            
            bool hasNearZeroU = t0NearZeroU || t1NearZeroU || t2NearZeroU;
            bool hasNearOneU  = t0NearOneU  || t1NearOneU  || t2NearOneU;
            
            // 仅当顶点同时存在于接缝两侧时才进行修正
            if (hasNearZeroU && hasNearOneU) {
                int countNearZero = (t0NearZeroU ? 1 : 0) + (t1NearZeroU ? 1 : 0) + (t2NearZeroU ? 1 : 0);
                int countNearOne  = (t0NearOneU  ? 1 : 0) + (t1NearOneU  ? 1 : 0) + (t2NearOneU  ? 1 : 0);
                
                if (countNearZero <= countNearOne) {
                    // 将靠近 0 的顶点 U 坐标加 1，使其靠近 1
                    if (t0NearZeroU) t0.x += 1.0;
                    if (t1NearZeroU) t1.x += 1.0;
                    if (t2NearZeroU) t2.x += 1.0;
                } else {
                    // 将靠近 1 的顶点 U 坐标减 1，使其靠近 0
                    if (t0NearOneU) t0.x -= 1.0;
                    if (t1NearOneU) t1.x -= 1.0;
                    if (t2NearOneU) t2.x -= 1.0;
                }
            }

            // 检测 V 方向接缝（极冠区域）
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
            rt.t0_1_over_w = v0.texCoord1 * rt.invW0;
            rt.t1_1_over_w = v1.texCoord1 * rt.invW1;
            rt.t2_1_over_w = v2.texCoord1 * rt.invW2;
            rt.c0_over_w = v0.color * rt.invW0;
            rt.c1_over_w = v1.color * rt.invW1;
            rt.c2_over_w = v2.color * rt.invW2;

            rt.tg0_over_w = v0.tangent * rt.invW0;
            rt.tg1_over_w = v1.tangent * rt.invW1;
            rt.tg2_over_w = v2.tangent * rt.invW2;
            rt.tangentW = tri.tangentW;

            rt.w0_o_w = v0.world * rt.invW0;
            rt.w1_o_w = v1.world * rt.invW1;
            rt.w2_o_w = v2.world * rt.invW2;

            rt.z0_over_w = p0.z;
            rt.z1_over_w = p1.z;
            rt.z2_over_w = p2.z;
            rt.zMin = std::min({rt.z0_over_w, rt.z1_over_w, rt.z2_over_w});

            // 从 MaterialTable 复制材质数据到光栅化三角形
            rt.materialId = matId;
            if (matTable) {
                rt.albedo = matTable->GetAlbedo(matId);
                rt.metallic = matTable->GetMetallic(matId);
                rt.roughness = matTable->GetRoughness(matId);
                rt.doubleSided = matTable->GetDoubleSided(matId);
                rt.alpha = matTable->GetAlpha(matId);
                rt.transmissionFactor = matTable->GetTransmissionFactor(matId);
                rt.alphaMode = matTable->GetAlphaMode(matId);
                rt.alphaCutoff = matTable->GetAlphaCutoff(matId);
                rt.emissiveFactor = matTable->GetEmissiveFactor(matId);
                rt.ior = matTable->GetIOR(matId);
                rt.specularFactor = matTable->GetSpecularFactor(matId);
                rt.specularColorFactor = matTable->GetSpecularColorFactor(matId);

                rt.textures[static_cast<size_t>(TextureSlot::BaseColor)] = {
                    matTable->GetBaseColorTextureIndex(matId),
                    matTable->GetBaseColorImageIndex(matId),
                    matTable->GetBaseColorSamplerIndex(matId),
                    matTable->GetBaseColorTexCoordSet(matId)
                };
                rt.textures[static_cast<size_t>(TextureSlot::MetallicRoughness)] = {
                    matTable->GetMetallicRoughnessTextureIndex(matId),
                    matTable->GetMetallicRoughnessImageIndex(matId),
                    matTable->GetMetallicRoughnessSamplerIndex(matId),
                    matTable->GetMetallicRoughnessTexCoordSet(matId)
                };
                rt.textures[static_cast<size_t>(TextureSlot::Normal)] = {
                    matTable->GetNormalTextureIndex(matId),
                    matTable->GetNormalImageIndex(matId),
                    matTable->GetNormalSamplerIndex(matId),
                    matTable->GetNormalTexCoordSet(matId)
                };
                rt.textures[static_cast<size_t>(TextureSlot::Occlusion)] = {
                    matTable->GetOcclusionTextureIndex(matId),
                    matTable->GetOcclusionImageIndex(matId),
                    matTable->GetOcclusionSamplerIndex(matId),
                    matTable->GetOcclusionTexCoordSet(matId)
                };
                rt.textures[static_cast<size_t>(TextureSlot::Emissive)] = {
                    matTable->GetEmissiveTextureIndex(matId),
                    matTable->GetEmissiveImageIndex(matId),
                    matTable->GetEmissiveSamplerIndex(matId),
                    matTable->GetEmissiveTexCoordSet(matId)
                };
                rt.textures[static_cast<size_t>(TextureSlot::Transmission)] = {
                    matTable->GetTransmissionTextureIndex(matId),
                    matTable->GetTransmissionImageIndex(matId),
                    matTable->GetTransmissionSamplerIndex(matId),
                    matTable->GetTransmissionTexCoordSet(matId)
                };
            } else {
                // MaterialTable 不可用时使用默认材质（白色不透明电介质）
                rt.albedo = Vec3{1.0, 1.0, 1.0};
                rt.metallic = 0.0;
                rt.roughness = 0.5;
                rt.doubleSided = false;
                rt.alpha = 1.0;
                rt.transmissionFactor = 0.0;
                rt.alphaMode = GLTFAlphaMode::Opaque;
                rt.alphaCutoff = 0.5;
                rt.emissiveFactor = Vec3{0.0, 0.0, 0.0};
                rt.ior = 1.5;
                rt.specularFactor = 1.0;
                rt.specularColorFactor = Vec3{1.0, 1.0, 1.0};

                rt.textures = {};
            }

            localTris.push_back(rt);
        }
    }

        perThreadClipCount[tid] = localClipped;
    } // end omp parallel (clipping)

    // 合并各线程裁剪结果
    {
        size_t totalRasterTris = 0;
        for (int t = 0; t < maxClipThreads; ++t) {
            totalRasterTris += perThreadClipTris[t].size();
            stats.trianglesClipped += perThreadClipCount[t];
        }
        rasterTris.reserve(totalRasterTris);
        for (auto& v : perThreadClipTris) {
            rasterTris.insert(rasterTris.end(),
                std::make_move_iterator(v.begin()),
                std::make_move_iterator(v.end()));
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
        SR_DEBUG_LOG(buffer);
    }

    // ── 阶段二：无锁 Tile-Based 并行光栅化 ───────────────────────────────
    double* depthData = m_depthBuffer->Data();
    if (!depthData) {
        return stats;
    }
    
    // 直接获取线性像素缓冲写指针，避免每次通过接口函数间接访问
    Vec3* linearPixels = m_framebuffer->GetLinearPixelsWritable();

    constexpr int TILE_SIZE = 32;
    int tilesX = (width + TILE_SIZE - 1) / TILE_SIZE;
    int tilesY = (height + TILE_SIZE - 1) / TILE_SIZE;

    int totalTiles = tilesX * tilesY;
    std::vector<int>& tileMinXs = scratch.tileMinXs;
    std::vector<int>& tileMinYs = scratch.tileMinYs;
    std::vector<int>& tileMaxXs = scratch.tileMaxXs;
    std::vector<int>& tileMaxYs = scratch.tileMaxYs;
    const bool tileGridChanged =
        scratch.cachedWidth != width ||
        scratch.cachedHeight != height ||
        scratch.cachedTilesX != tilesX ||
        scratch.cachedTilesY != tilesY;

    if (tileGridChanged) {
        tileMinXs.resize(static_cast<size_t>(totalTiles));
        tileMinYs.resize(static_cast<size_t>(totalTiles));
        tileMaxXs.resize(static_cast<size_t>(totalTiles));
        tileMaxYs.resize(static_cast<size_t>(totalTiles));
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
        scratch.cachedWidth = width;
        scratch.cachedHeight = height;
        scratch.cachedTilesX = tilesX;
        scratch.cachedTilesY = tilesY;
    }

    // 将三角形分配到 Tile Bin（紧密存储，两遍算法）：
    // 第一遍：统计每个 Tile 的引用数；前缀和计算偏移；第二遍：填充索引数组。
    std::vector<size_t>& binCounts = scratch.binCounts;
    std::vector<size_t>& binOffsets = scratch.binOffsets;
    std::vector<size_t>& binWriteCursor = scratch.binWriteCursor;
    std::vector<size_t>& binTriIndices = scratch.binTriIndices;
    std::vector<int>& triMinTileX = scratch.triMinTileX;
    std::vector<int>& triMaxTileX = scratch.triMaxTileX;
    std::vector<int>& triMinTileY = scratch.triMinTileY;
    std::vector<int>& triMaxTileY = scratch.triMaxTileY;
    binCounts.assign(static_cast<size_t>(totalTiles), 0);
    triMinTileX.resize(rasterTris.size());
    triMaxTileX.resize(rasterTris.size());
    triMinTileY.resize(rasterTris.size());
    triMaxTileY.resize(rasterTris.size());

    const int numRasterTris = static_cast<int>(rasterTris.size());

    // Pass 1: 并行计算每个三角形的 tile 范围 + 每线程独立直方图统计
    #pragma omp parallel
    {
        std::vector<size_t> localBinCounts(static_cast<size_t>(totalTiles), 0);

        #pragma omp for schedule(static)
        for (int i = 0; i < numRasterTris; ++i) {
            const RasterTriangle& rt = rasterTris[static_cast<size_t>(i)];
            int minTileX = std::max(rt.minX / TILE_SIZE, 0);
            int maxTileX = std::min(rt.maxX / TILE_SIZE, tilesX - 1);
            int minTileY = std::max(rt.minY / TILE_SIZE, 0);
            int maxTileY = std::min(rt.maxY / TILE_SIZE, tilesY - 1);
            triMinTileX[static_cast<size_t>(i)] = minTileX;
            triMaxTileX[static_cast<size_t>(i)] = maxTileX;
            triMinTileY[static_cast<size_t>(i)] = minTileY;
            triMaxTileY[static_cast<size_t>(i)] = maxTileY;
            for (int ty = minTileY; ty <= maxTileY; ++ty) {
                const int rowBase = ty * tilesX;
                for (int tx = minTileX; tx <= maxTileX; ++tx) {
                    ++localBinCounts[static_cast<size_t>(rowBase + tx)];
                }
            }
        }

        // 归约：合并各线程直方图
        #pragma omp critical
        {
            for (int t = 0; t < totalTiles; ++t) {
                binCounts[static_cast<size_t>(t)] += localBinCounts[static_cast<size_t>(t)];
            }
        }
    }

    // Prefix sum（串行，O(totalTiles) 极快）
    binOffsets.resize(static_cast<size_t>(totalTiles) + 1);
    binOffsets[0] = 0;
    for (int t = 0; t < totalTiles; ++t) {
        binOffsets[static_cast<size_t>(t + 1)] = binOffsets[static_cast<size_t>(t)] + binCounts[static_cast<size_t>(t)];
    }
    const size_t totalBinRefs = binOffsets[static_cast<size_t>(totalTiles)];
    binTriIndices.resize(totalBinRefs);
    binWriteCursor.assign(binOffsets.begin(), binOffsets.begin() + static_cast<size_t>(totalTiles));

    // 第二遍：并行填充 Bin 索引（使用 C++20 atomic_ref 写游标，无锁）
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < numRasterTris; ++i) {
        const int minTileX = triMinTileX[static_cast<size_t>(i)];
        const int maxTileX = triMaxTileX[static_cast<size_t>(i)];
        const int minTileY = triMinTileY[static_cast<size_t>(i)];
        const int maxTileY = triMaxTileY[static_cast<size_t>(i)];
        for (int ty = minTileY; ty <= maxTileY; ++ty) {
            const int rowBase = ty * tilesX;
            for (int tx = minTileX; tx <= maxTileX; ++tx) {
                const size_t tileIndex = static_cast<size_t>(rowBase + tx);
                std::atomic_ref<size_t> cursor(binWriteCursor[tileIndex]);
                const size_t pos = cursor.fetch_add(1, std::memory_order_relaxed);
                binTriIndices[pos] = static_cast<size_t>(i);
            }
        }
    }

    // 对每个 Tile 内的三角形排序：
    //   - 不透明/Mask：从近到远（Early-Z 优化，减少片元着色调用）
    //   - 半透明（Blend）：从远到近（保证 Alpha 混合正确性）
    const bool isBatchTransparent = !rasterTris.empty() && rasterTris[0].alphaMode == GLTFAlphaMode::Blend;
    #pragma omp parallel for schedule(static)
    for (int t = 0; t < totalTiles; ++t) {
        const size_t begin = binOffsets[static_cast<size_t>(t)];
        const size_t end = binOffsets[static_cast<size_t>(t + 1)];
        if (end - begin < 2) {
            continue;
        }
        auto itBegin = binTriIndices.begin() + static_cast<std::ptrdiff_t>(begin);
        auto itEnd = binTriIndices.begin() + static_cast<std::ptrdiff_t>(end);
        if (isBatchTransparent) {
            // 半透明三角形从远到近排序（保证正确 Alpha 混合）
            std::sort(itBegin, itEnd, [&rasterTris](size_t a, size_t b) {
                return rasterTris[a].zMin > rasterTris[b].zMin;
            });
        } else {
            // 不透明三角形从近到远排序（Early-Z：近处片元先通过深度测试，远处可提前 discard）
            std::sort(itBegin, itEnd, [&rasterTris](size_t a, size_t b) {
                return rasterTris[a].zMin < rasterTris[b].zMin;
            });
        }
    }

    size_t maxBinSize = 0;
    for (int t = 0; t < totalTiles; ++t) {
        const size_t binSize = binCounts[static_cast<size_t>(t)];
        if (binSize > maxBinSize) {
            maxBinSize = binSize;
        }
    }
    {
        char buffer[256];
        double avgBin = totalTiles > 0 ? static_cast<double>(totalBinRefs) / static_cast<double>(totalTiles) : 0.0;
        std::snprintf(buffer, sizeof(buffer),
            "Rasterizer: bin refs=%zu avgBin=%.1f maxBin=%zu\n",
            totalBinRefs, avgBin, maxBinSize);
        SR_DEBUG_LOG(buffer);
    }

    {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "Rasterizer: tiles=%d (%d x %d)\n", totalTiles, tilesX, tilesY);
        SR_DEBUG_LOG(buffer);
    }

    SR_DEBUG_LOG("Rasterizer: tile max depth pass skipped\n");

    SR_DEBUG_LOG("Rasterizer: tile raster pass start\n");

    // 全帧预计算光照数据（仅一次，避免在每个三角形/像素重复计算 normalize 和 radiance）
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
        uint64_t localPixelsTested = 0;
        uint64_t localPixelsShaded = 0;

        // 按 Tile 并行，每个 Tile 只由一个线程写入，天然无锁（无相邻像素冲突）
        #pragma omp for schedule(dynamic, 1)
        for (int t = 0; t < totalTiles; ++t) {
            const size_t binBegin = binOffsets[static_cast<size_t>(t)];
            const size_t binEnd = binOffsets[static_cast<size_t>(t + 1)];
            if (binBegin == binEnd) {
                continue;
            }

            int tileMinX = tileMinXs[static_cast<size_t>(t)];
            int tileMinY = tileMinYs[static_cast<size_t>(t)];
            int tileMaxX = tileMaxXs[static_cast<size_t>(t)];
            int tileMaxY = tileMaxYs[static_cast<size_t>(t)];

            for (size_t binPos = binBegin; binPos < binEnd; ++binPos) {
                const size_t triIndex = binTriIndices[binPos];
                const RasterTriangle& rt = rasterTris[triIndex];
                
                // 构建三角形级 FragmentContext（仅一次，该三角形所有像素共享）
                FragmentContext fragCtx;
                fragCtx.cameraPos = m_frameContext.cameraPos;

                // 从 RasterTriangle 拷贝已缓存的材质属性（避免每像素间接访问 MaterialTable）
                fragCtx.albedo = rt.albedo;
                fragCtx.metallic = rt.metallic;
                fragCtx.roughness = rt.roughness;
                fragCtx.doubleSided = rt.doubleSided;
                fragCtx.alpha = rt.alpha;
                fragCtx.transmissionFactor = rt.transmissionFactor;
                fragCtx.alphaMode = rt.alphaMode;
                fragCtx.alphaCutoff = rt.alphaCutoff;
                fragCtx.emissiveFactor = rt.emissiveFactor;
                fragCtx.ior = rt.ior;
                fragCtx.specularFactor = rt.specularFactor;
                fragCtx.specularColorFactor = rt.specularColorFactor;

                fragCtx.textures = rt.textures;

                fragCtx.lights = &m_frameContext.lights;
                fragCtx.ambientColor = m_frameContext.ambientColor;
                fragCtx.environmentMap = m_frameContext.environmentMap;
                fragCtx.images = m_frameContext.images;
                fragCtx.samplers = m_frameContext.samplers;
                fragCtx.tangentW = rt.tangentW;
                
                // 传入全帧预计算光照（指针方式，零拷贝）
                fragCtx.precomputedLights = globalPrecomputedLights.empty() ? nullptr : globalPrecomputedLights.data();
                fragCtx.precomputedLightCount = globalPrecomputedLights.size();

                int minX = std::max(rt.minX, tileMinX);
                int maxX = std::min(rt.maxX, tileMaxX);
                int minY = std::max(rt.minY, tileMinY);
                int maxY = std::min(rt.maxY, tileMaxY);

                // 预计算边函数初始值（像素中心 +0.5 偏移，DirectX 光栅规则）
                double pyBase = static_cast<double>(minY) + 0.5;
                double pxStart = static_cast<double>(minX) + 0.5;
                
                // 在 (minX, minY) 处初始化三条边函数的值
                double w0_row = rt.A12 * pxStart + rt.B12 * pyBase + rt.C12;
                double w1_row = rt.A20 * pxStart + rt.B20 * pyBase + rt.C20;
                double w2_row = rt.A01 * pxStart + rt.B01 * pyBase + rt.C01;

                // 判断该三角形是否需要 Alpha 测试（Mask 模式）或 Alpha 混合（Blend 模式）
                const TextureBinding& baseColorBinding = rt.textures[static_cast<size_t>(TextureSlot::BaseColor)];
                const TextureBinding& transmissionBinding = rt.textures[static_cast<size_t>(TextureSlot::Transmission)];
                const bool needsAlphaTest = (rt.alphaMode == GLTFAlphaMode::Mask && baseColorBinding.imageIndex >= 0);
                const bool needsAlphaBlend = (rt.alphaMode == GLTFAlphaMode::Blend);

                // 预广播边函数增量为 AVX2 寄存器（每次处理 4 个连续像素）
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
                    
                    // SIMD 快速路径：每次处理 4 个连续像素
                    const int simdEnd = minX + ((maxX - minX + 1) / 4) * 4;
                    for (; x < simdEnd; x += 4) {
                        // 向量化计算 4 个像素的边函数值（使用预广播系数）
                        __m256d offset = _mm256_mul_pd(xOffset_4, A12_4);
                        __m256d w0_4 = _mm256_add_pd(_mm256_set1_pd(w0), offset);
                        offset = _mm256_mul_pd(xOffset_4, A20_4);
                        __m256d w1_4 = _mm256_add_pd(_mm256_set1_pd(w1), offset);
                        offset = _mm256_mul_pd(xOffset_4, A01_4);
                        __m256d w2_4 = _mm256_add_pd(_mm256_set1_pd(w2), offset);

                        // 快速判断：4 个像素中是否有任何一个落在三角形内（全部在外则跳过）
                        int insideMask = GetInsideMask4(w0_4, w1_4, w2_4);
                        if (insideMask == 0) {
                            // 4 个像素均在三角形外，直接跳过
                            w0 += rt.A12 * 4.0;
                            w1 += rt.A20 * 4.0;
                            w2 += rt.A01 * 4.0;
                            continue;
                        }

                        // 计算 4 个像素的重心坐标（归一化边函数值）
                        __m256d bw0_4 = _mm256_mul_pd(w0_4, invArea_4);
                        __m256d bw1_4 = _mm256_mul_pd(w1_4, invArea_4);
                        __m256d bw2_4 = _mm256_mul_pd(w2_4, invArea_4);

                        // 使用重心坐标插值深度（NDC Z 值，用于 Early-Z 深度测试）
                        __m256d depth_4 = _mm256_add_pd(_mm256_add_pd(
                            _mm256_mul_pd(bw0_4, z0_4),
                            _mm256_mul_pd(bw1_4, z1_4)),
                            _mm256_mul_pd(bw2_4, z2_4));

                        // 插值 1/w（用于透视正确的属性插值）
                        __m256d invW_4 = _mm256_add_pd(_mm256_add_pd(
                            _mm256_mul_pd(bw0_4, invW0_4),
                            _mm256_mul_pd(bw1_4, invW1_4)),
                            _mm256_mul_pd(bw2_4, invW2_4));

                        // 将 AVX2 寄存器结果写回对齐内存，逐像素进行深度测试和着色
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
                            localPixelsTested++;
                            
                            if (depth < 0.0 || depth >= depthData[index]) continue;
                            if (invWs[i] <= 0.0) continue;

                            double bw0 = bw0s[i];
                            double bw1 = bw1s[i];
                            double bw2 = bw2s[i];
                            double wVal = 1.0 / invWs[i];

                            // 构建像素级插值数据（轻量结构体）
                            FragmentVarying varying;
                            varying.normal = InterpolateVec3(rt.n0_over_w, rt.n1_over_w, rt.n2_over_w, bw0, bw1, bw2, wVal);
                            varying.worldPos = InterpolateVec3(rt.w0_o_w, rt.w1_o_w, rt.w2_o_w, bw0, bw1, bw2, wVal);
                            varying.texCoord = InterpolateVec2(rt.t0_over_w, rt.t1_over_w, rt.t2_over_w, bw0, bw1, bw2, wVal);
                            varying.texCoord1 = InterpolateVec2(rt.t0_1_over_w, rt.t1_1_over_w, rt.t2_1_over_w, bw0, bw1, bw2, wVal);
                            varying.color = InterpolateVec4(rt.c0_over_w, rt.c1_over_w, rt.c2_over_w, bw0, bw1, bw2, wVal);
                            varying.tangent = (rt.textures[static_cast<size_t>(TextureSlot::Normal)].imageIndex >= 0)
                                ? InterpolateVec3(rt.tg0_over_w, rt.tg1_over_w, rt.tg2_over_w, bw0, bw1, bw2, wVal)
                                : Vec3{0.0, 0.0, 0.0};

                            double alpha = rt.alpha;
                            if (baseColorBinding.imageIndex >= 0) {
                                Vec2 baseUv = (baseColorBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
                                alpha *= SampleTextureChannel(m_frameContext, baseColorBinding.imageIndex, baseColorBinding.samplerIndex, baseUv, 3);
                            }
                            alpha *= std::clamp(varying.color.w, 0.0, 1.0);
                            if (rt.transmissionFactor > 0.0 || transmissionBinding.imageIndex >= 0) {
                                double t = std::clamp(rt.transmissionFactor, 0.0, 1.0);
                                Vec2 tUv = (transmissionBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
                                if (transmissionBinding.imageIndex >= 0) {
                                    t *= SampleTextureChannel(m_frameContext, transmissionBinding.imageIndex, transmissionBinding.samplerIndex, tUv, 0);
                                }
                                alpha *= (1.0 - std::clamp(t, 0.0, 1.0));
                            }
                            if (needsAlphaTest && alpha < rt.alphaCutoff) continue;

                            double effectiveAlpha = alpha;
                            localPixelsShaded++;
                            Vec3 shaded = fragmentShader.ShadeFast(fragCtx, varying,
                                needsAlphaBlend ? &effectiveAlpha : nullptr);
                            // 预乘 Alpha 混合：shaded 中漫反射/环境光已按 alpha 预乘，
                            // 镜面反射保持全强度（Fresnel）。effectiveAlpha 含玻璃的 Fresnel 贡献。
                            // 公式：result = premul_shaded + bg * (1 - effectiveAlpha)
                            if (needsAlphaBlend && effectiveAlpha < 0.999) {
                                Vec3 dst = linearPixels[y * width + px];
                                linearPixels[y * width + px] = shaded + dst * (1.0 - effectiveAlpha);
                            } else {
                                depthData[index] = depth;
                                linearPixels[y * width + px] = shaded;
                            }
                        }

                        w0 += rt.A12 * 4.0;
                        w1 += rt.A20 * 4.0;
                        w2 += rt.A01 * 4.0;
                    }

                    // 标量路径：处理 SIMD 对齐尾部的剩余像素
                    for (; x <= maxX; ++x) {
                        // 快速三角形内部测试
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

                        // Early-Z：在昂贵的属性插值之前先做深度测试
                        double depth = bw0 * rt.z0_over_w + bw1 * rt.z1_over_w + bw2 * rt.z2_over_w;
                        int index = rowBase + x;
                        localPixelsTested++;
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

                        // 构建像素级插值数据（轻量结构体）
                        FragmentVarying varying;
                        varying.normal = InterpolateVec3(rt.n0_over_w, rt.n1_over_w, rt.n2_over_w, bw0, bw1, bw2, wVal);
                        varying.worldPos = InterpolateVec3(rt.w0_o_w, rt.w1_o_w, rt.w2_o_w, bw0, bw1, bw2, wVal);
                        varying.texCoord = InterpolateVec2(rt.t0_over_w, rt.t1_over_w, rt.t2_over_w, bw0, bw1, bw2, wVal);
                        varying.texCoord1 = InterpolateVec2(rt.t0_1_over_w, rt.t1_1_over_w, rt.t2_1_over_w, bw0, bw1, bw2, wVal);
                        varying.color = InterpolateVec4(rt.c0_over_w, rt.c1_over_w, rt.c2_over_w, bw0, bw1, bw2, wVal);
                        varying.tangent = (rt.textures[static_cast<size_t>(TextureSlot::Normal)].imageIndex >= 0)
                            ? InterpolateVec3(rt.tg0_over_w, rt.tg1_over_w, rt.tg2_over_w, bw0, bw1, bw2, wVal)
                            : Vec3{0.0, 0.0, 0.0};

                        double alpha = rt.alpha;
                        if (baseColorBinding.imageIndex >= 0) {
                            Vec2 baseUv = (baseColorBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
                            alpha *= SampleTextureChannel(m_frameContext, baseColorBinding.imageIndex, baseColorBinding.samplerIndex, baseUv, 3);
                        }
                        alpha *= std::clamp(varying.color.w, 0.0, 1.0);
                        if (rt.transmissionFactor > 0.0 || transmissionBinding.imageIndex >= 0) {
                            double t = std::clamp(rt.transmissionFactor, 0.0, 1.0);
                            Vec2 tUv = (transmissionBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
                            if (transmissionBinding.imageIndex >= 0) {
                                t *= SampleTextureChannel(m_frameContext, transmissionBinding.imageIndex, transmissionBinding.samplerIndex, tUv, 0);
                            }
                            alpha *= (1.0 - std::clamp(t, 0.0, 1.0));
                        }
                        if (needsAlphaTest && alpha < rt.alphaCutoff) {
                            w0 += rt.A12;
                            w1 += rt.A20;
                            w2 += rt.A01;
                            continue;
                        }

                        double effectiveAlpha = alpha;
                        localPixelsShaded++;
                        Vec3 shaded = fragmentShader.ShadeFast(fragCtx, varying,
                            needsAlphaBlend ? &effectiveAlpha : nullptr);
                        // 预乘 Alpha 混合（与 SIMD 路径逻辑相同）
                        if (needsAlphaBlend && effectiveAlpha < 0.999) {
                            Vec3 dst = linearPixels[rowBase + x];
                            linearPixels[rowBase + x] = shaded + dst * (1.0 - effectiveAlpha);
                        } else {
                            depthData[index] = depth;
                            linearPixels[rowBase + x] = shaded;
                        }

                        w0 += rt.A12;
                        w1 += rt.A20;
                        w2 += rt.A01;
                    }
                    // 推进到下一行（Y 方向边函数增量）
                    w0_row += rt.B12;
                    w1_row += rt.B20;
                    w2_row += rt.B01;
                }
            }
        }

        #pragma omp atomic
        stats.pixelsTested += localPixelsTested;
        #pragma omp atomic
        stats.pixelsShaded += localPixelsShaded;
    } // end omp parallel

    SR_DEBUG_LOG("Rasterizer: tile raster pass done\n");

    SR_DEBUG_LOG("Rasterizer: end\n");
    return stats;
}

} // namespace SR
