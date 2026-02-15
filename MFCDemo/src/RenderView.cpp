#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "RenderView.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <vector>

#include "Math/Vec4.h"

namespace {

/**
 * @brief 包含场景中心和范围的辅助结构
 */
struct BoundsResult {
    SR::Vec3 center;
    SR::Vec3 extent;
};

inline uint8_t ToneMapToSRGB8(double v) {
    if (v < 0.0) {
        v = 0.0;
    }
    // 与软光栅器内部一致，便于肉眼比对
    constexpr double a = 2.51;
    constexpr double b = 0.03;
    constexpr double c = 2.43;
    constexpr double d = 0.59;
    constexpr double e = 0.14;
    const double mapped = (v * (a * v + b)) / (v * (c * v + d) + e);
    const double clamped = std::clamp(mapped, 0.0, 1.0);
    const double srgb = std::pow(clamped, 1.0 / 2.2);
    return static_cast<uint8_t>(srgb * 255.0 + 0.5);
}

bool SaveLinearFramebufferToBMP(const SR::Vec3* linearPixels,
                                int width,
                                int height,
                                double exposure,
                                const std::filesystem::path& outputPath) {
    if (!linearPixels || width <= 0 || height <= 0) {
        return false;
    }

    std::vector<uint8_t> pixelBytes(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t srcIndex = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            const SR::Vec3& c = linearPixels[srcIndex];
            const uint8_t r = ToneMapToSRGB8(c.x * exposure);
            const uint8_t g = ToneMapToSRGB8(c.y * exposure);
            const uint8_t b = ToneMapToSRGB8(c.z * exposure);

            // BMP 使用 bottom-up 存储
            const int dstY = height - 1 - y;
            const size_t dstIndex = (static_cast<size_t>(dstY) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
            pixelBytes[dstIndex + 0] = b;
            pixelBytes[dstIndex + 1] = g;
            pixelBytes[dstIndex + 2] = r;
            pixelBytes[dstIndex + 3] = 255u;
        }
    }

    #pragma pack(push, 1)
    struct BMPFileHeader {
        uint16_t bfType = 0x4D42; // "BM"
        uint32_t bfSize = 0;
        uint16_t bfReserved1 = 0;
        uint16_t bfReserved2 = 0;
        uint32_t bfOffBits = 14u + 40u;
    };

    struct BMPInfoHeader {
        uint32_t biSize = 40u;
        int32_t biWidth = 0;
        int32_t biHeight = 0;
        uint16_t biPlanes = 1u;
        uint16_t biBitCount = 32u;
        uint32_t biCompression = 0u; // BI_RGB
        uint32_t biSizeImage = 0u;
        int32_t biXPelsPerMeter = 0;
        int32_t biYPelsPerMeter = 0;
        uint32_t biClrUsed = 0u;
        uint32_t biClrImportant = 0u;
    };
    #pragma pack(pop)

    static_assert(sizeof(BMPFileHeader) == 14, "BMPFileHeader size must be 14 bytes");
    static_assert(sizeof(BMPInfoHeader) == 40, "BMPInfoHeader size must be 40 bytes");

    BMPFileHeader fileHeader{};
    BMPInfoHeader infoHeader{};
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biSizeImage = static_cast<uint32_t>(pixelBytes.size());
    fileHeader.bfSize = fileHeader.bfOffBits + infoHeader.biSizeImage;

    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    file.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));
    file.write(reinterpret_cast<const char*>(pixelBytes.data()), static_cast<std::streamsize>(pixelBytes.size()));
    return static_cast<bool>(file);
}

/**
 * @brief 从 GPUScene 中的网格计算世界空间包围球/包围盒
 * 用于自动调整相机对焦位置
 */
std::optional<BoundsResult> ComputeSceneBounds(const SR::GPUScene& scene) {
    using namespace SR;
    const double kInf = std::numeric_limits<double>::infinity();
    Vec3 minBounds{kInf, kInf, kInf};
    Vec3 maxBounds{-kInf, -kInf, -kInf};
    bool hasPoint = false;

    for (const GPUSceneDrawItem& item : scene.GetItems()) {
        if (!item.mesh) {
            continue;
        }

        const std::vector<Vertex>& vertices = item.mesh->GetVertices();
        for (const Vertex& v : vertices) {
            Vec4 p{v.position.x, v.position.y, v.position.z, 1.0};
            Vec4 world = item.modelMatrix.Multiply(p);
            double invW = (world.w != 0.0) ? (1.0 / world.w) : 1.0;
            Vec3 pos{world.x * invW, world.y * invW, world.z * invW};

            minBounds.x = std::min(minBounds.x, pos.x);
            minBounds.y = std::min(minBounds.y, pos.y);
            minBounds.z = std::min(minBounds.z, pos.z);
            maxBounds.x = std::max(maxBounds.x, pos.x);
            maxBounds.y = std::max(maxBounds.y, pos.y);
            maxBounds.z = std::max(maxBounds.z, pos.z);
            hasPoint = true;
        }
    }

    if (!hasPoint) {
        return std::nullopt;
    }

    Vec3 extent = maxBounds - minBounds;
    Vec3 center = minBounds + extent * 0.5;
    return BoundsResult{center, extent};
}

} // namespace

namespace SR {

/**
 * @brief 初始化视图，设置渲染器并加载测试 glTF 模型
 * @param width 初始宽度
 * @param height 初始高度
 */
void CRenderView::Initialize(int width, int height) {
    QueryPerformanceFrequency(&m_timerFreq);
    QueryPerformanceCounter(&m_lastTime);

    m_width = width;
    m_height = height;
    m_renderer.Initialize(width, height);
    m_renderer.SetHDR(false);

    m_camera.SetTarget(Vec3{0.0, 0.0, 0.0});
    m_camera.SetDistance(2.5);

    m_objects.Clear();
    m_hasGLB = false;
    GLTFLoader loader;
    // 自动加载桌面上的测试模型 (earth)
    GLTFAsset asset = loader.LoadGLB("example/2019_mazda_mx-5.glb");
    if (!asset.meshes.empty()) {
        m_gpuScene.Build(asset, -1);
        m_hasGLB = !m_gpuScene.GetItems().empty();
        if (m_hasGLB) {
            // 如果成功加载了模型，则计算其包围盒并自动调整相机视角
            if (auto bounds = ComputeSceneBounds(m_gpuScene)) {
                const RendererConfig& config = m_renderer.GetConfig();
                double fovY = config.frameContext.fovYRadians;
                double aspect = (m_height > 0) ? (static_cast<double>(m_width) / static_cast<double>(m_height)) : 1.0;
                double fovX = 2.0 * std::atan(std::tan(fovY * 0.5) * aspect);

                Vec3 half = bounds->extent * 0.5;
                double maxExtent = std::max({bounds->extent.x, bounds->extent.y, bounds->extent.z});
                // If model is extremely small, inflate bounds for a more usable default framing.
                double minExtent = 0.1; // scene units
                if (maxExtent > 0.0 && maxExtent < minExtent) {
                    double scale = minExtent / maxExtent;
                    half = half * scale;
                }
                double halfWidth = std::max(half.x, 1e-3);
                double halfHeight = std::max(half.y, 1e-3);
                double halfDepth = std::max(half.z, 1e-3);

                double distanceY = halfHeight / std::tan(fovY * 0.5);
                double distanceX = halfWidth / std::tan(fovX * 0.5);
                double distance = std::max(distanceX, distanceY);
                double margin = 1.02; // tighter fit

                m_camera.SetTarget(bounds->center);
                m_camera.SetDistance(distance * margin);

                // Adjust near/far to avoid clipping when depth range is large.
                RendererConfig updated = m_renderer.GetConfig();
                double depthPadding = std::max(halfDepth * 2.0, 1.0);
                updated.frameContext.zNear = std::max(0.01, distance * margin - depthPadding);
                updated.frameContext.zFar = std::max(updated.frameContext.zNear + 1.0, distance * margin + depthPadding);
                m_renderer.SetConfig(updated);
            }
        }
        if (!m_hasGLB) {
            OutputDebugStringA("GLB loaded but produced no draw items.\n");
        }
    } else {
        std::string error = loader.GetLastError();
        if (!error.empty()) {
            OutputDebugStringA(("GLB load failed: " + error + "\n").c_str());
        }
        m_mesh = Mesh::CreateSphere(0.6, 64, 32);
        m_model.SetMesh(&m_mesh);
        m_model.GetTransform().SetPosition(Vec3{0.0, 0.0, 0.0});
        m_model.GetMaterial().albedo = Vec3{0.9, 0.85, 0.75};
        m_model.GetMaterial().metallic = 0.1;
        m_model.GetMaterial().roughness = 0.4;

        m_objects.AddModel(m_model);
        m_scene.SetObjectGroup(&m_objects);
    }

    m_lights.Clear();
    DirectionalLight sun;
    sun.direction = Vec3{-0.4, -1.0, -0.2};
    sun.color = Vec3{1.0, 0.98, 0.95};
    sun.intensity = 1.1;
    m_lights.AddDirectionalLight(sun);
    m_scene.SetLightGroup(&m_lights);

    m_scene.SetCamera(&m_camera);

    // 加载 IBL 环境贴图（EXR）
    if (m_envMap.LoadFromEXR("example/german_town_street_4k.exr")) {
        RendererConfig config = m_renderer.GetConfig();
        config.environmentMap = &m_envMap;
        m_renderer.SetConfig(config);
        OutputDebugStringA("RenderView: environment map loaded\n");
    } else {
        OutputDebugStringA(("RenderView: failed to load env map: " + m_envMap.GetLastError() + "\n").c_str());
    }

    if (m_hasGLB) {
        ExportMaterialDebugFrames(asset);
    }
}

/**
 * @brief 初始化 HDR 环境
 */
void CRenderView::InitializeHDR(HWND hwnd, int width, int height) {
    Initialize(width, height);
    m_useHDR = m_hdrPresenter.Initialize(hwnd, width, height);
    m_renderer.SetHDR(m_useHDR);
}

/**
 * @brief 窗口尺寸改变时，重建渲染纹理和交换链
 */
void CRenderView::Resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    if (width == m_width && height == m_height) {
        return;
    }

    if (!m_useHDR) {
        return;
    }

    m_width = width;
    m_height = height;
    m_renderer.Initialize(width, height);
    m_renderer.SetHDR(true);
    m_hdrPresenter.Resize(width, height);
}

/**
 * @brief 绘制一帧 HDR 画面
 */
void CRenderView::DrawHDR() {
    if (!m_useHDR || m_width <= 0 || m_height <= 0) {
        return;
    }

    // Update FPS
    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    double deltaTime = static_cast<double>(currentTime.QuadPart - m_lastTime.QuadPart) / m_timerFreq.QuadPart;
    m_lastTime = currentTime;

    m_fpsUpdateTimer += static_cast<float>(deltaTime);
    m_frameCount++;
    if (m_fpsUpdateTimer >= 0.5f) {
        m_fps = static_cast<float>(m_frameCount) / m_fpsUpdateTimer;
        m_frameCount = 0;
        m_fpsUpdateTimer = 0.0f;
    }

    Render();
    // 将 HDR 线性数据提交给 D3D12 进行显示
    m_hdrPresenter.Present(m_renderer.GetFramebufferLinear());
}

/**
 * @brief 鼠标按下事件
 */
void CRenderView::OnMouseDown(int x, int y, bool leftButton) {
    if (leftButton) {
        m_leftButtonDown = true;
    } else {
        m_rightButtonDown = true;
    }
    m_lastMouseX = x;
    m_lastMouseY = y;
}

/**
 * @brief 鼠标抬起事件
 */
void CRenderView::OnMouseUp(bool leftButton) {
    if (leftButton) {
        m_leftButtonDown = false;
    } else {
        m_rightButtonDown = false;
    }
}

/**
 * @brief 鼠标移动事件，处理旋转和平移
 */
void CRenderView::OnMouseMove(int x, int y) {
    int deltaX = x - m_lastMouseX;
    int deltaY = y - m_lastMouseY;
    m_lastMouseX = x;
    m_lastMouseY = y;

    if (m_leftButtonDown) {
        // 左键拖拽：控制相机轨道旋转
        double sensitivity = 0.005;
        double azimuthDelta = -static_cast<double>(deltaX) * sensitivity;
        double elevationDelta = static_cast<double>(deltaY) * sensitivity;
        m_camera.Rotate(azimuthDelta, elevationDelta);
    }

    if (m_rightButtonDown) {
        // 右键拖拽：平移相机观测目标
        double panSensitivity = 0.003 * m_camera.GetDistance();
        double panX = -static_cast<double>(deltaX) * panSensitivity;
        double panY = static_cast<double>(deltaY) * panSensitivity;

        // 在相机局部空间计算偏移方向
        Vec3 forward = m_camera.GetTarget() - m_camera.GetPosition();
        Vec3 worldUp{0.0, 1.0, 0.0};
        Vec3 right = Vec3::Cross(forward, worldUp).Normalized();
        Vec3 up = Vec3::Cross(right, forward).Normalized();

        Vec3 offset = right * panX + up * panY;
        m_camera.SetTarget(m_camera.GetTarget() + offset);
    }
}

/**
 * @brief 鼠标滚轮旋转，处理缩放
 */
void CRenderView::OnMouseWheel(int delta) {
    // 缩放：改变相机到目标的距离
    if (delta == 0) {
        return;
    }

    // Use exponential zoom for consistent feel across large distances
    double steps = static_cast<double>(delta) / 120.0;
    double zoomStep = 0.08; // smaller = slower zoom
    double factor = std::pow(1.0 - zoomStep, steps);
    double newDistance = m_camera.GetDistance() * factor;

    // 限制缩放范围 (更宽范围以适配大模型)
    newDistance = std::fmax(0.2, std::fmin(newDistance, 5000.0));
    m_camera.SetDistance(newDistance);
}

void CRenderView::ExportMaterialDebugFrames(const GLTFAsset& asset) {
    if (m_materialDebugExported) {
        return;
    }

    int targetMaterialIndex = -1;
    for (size_t i = 0; i < asset.materials.size(); ++i) {
        if (asset.materials[i].name == m_debugMaterialName) {
            targetMaterialIndex = static_cast<int>(i);
            break;
        }
    }

    if (targetMaterialIndex < 0) {
        OutputDebugStringA(("Material debug export skipped: material not found: " + m_debugMaterialName + "\n").c_str());
        return;
    }

    RendererConfig originalConfig = m_renderer.GetConfig();

    // 先导出全场景结果用于对照
    RendererConfig fullConfig = originalConfig;
    fullConfig.debugOnlyMaterialIndex = -1;
    m_renderer.SetConfig(fullConfig);
    Render();

    const std::filesystem::path exportDir = std::filesystem::current_path() / "debug_exports";
    const std::filesystem::path fullPath = exportDir / "full_scene_reference.bmp";
    bool fullOk = SaveLinearFramebufferToBMP(
        m_renderer.GetFramebufferLinear(),
        m_renderer.GetWidth(),
        m_renderer.GetHeight(),
        fullConfig.exposure,
        fullPath);

    // 再导出仅目标材质的结果
    RendererConfig isolateConfig = originalConfig;
    isolateConfig.debugOnlyMaterialIndex = targetMaterialIndex;
    m_renderer.SetConfig(isolateConfig);
    Render();

    const std::filesystem::path isolatedPath = exportDir / ("material_only_" + m_debugMaterialName + ".bmp");
    bool isolatedOk = SaveLinearFramebufferToBMP(
        m_renderer.GetFramebufferLinear(),
        m_renderer.GetWidth(),
        m_renderer.GetHeight(),
        isolateConfig.exposure,
        isolatedPath);

    m_renderer.SetConfig(originalConfig);
    m_materialDebugExported = fullOk && isolatedOk;

    std::string message = "Material debug export: ";
    message += fullOk ? ("full=" + fullPath.string()) : "full=FAILED";
    message += ", ";
    message += isolatedOk ? ("isolated=" + isolatedPath.string()) : "isolated=FAILED";
    message += "\n";
    OutputDebugStringA(message.c_str());
}

/**
 * @brief 调用渲染器执行场景绘制
 */
void CRenderView::Render() {
    if (m_hasGLB) {
        RendererConfig config = m_renderer.GetConfig();
        config.useViewOverride = true;
        config.viewOverride = m_camera.GetViewMatrix();
        config.useCameraPosOverride = true;
        config.cameraPosOverride = m_camera.GetPosition();
        m_renderer.SetConfig(config);
        m_renderer.Render(m_gpuScene);
    } else {
        m_renderer.Render(m_scene);
    }
}

} // namespace SR
