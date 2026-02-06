#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "RenderView.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "Math/Vec4.h"

namespace {

/**
 * @brief 包含场景中心和范围的辅助结构
 */
struct BoundsResult {
    SR::Vec3 center;
    SR::Vec3 extent;
};

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
    // 自动加载桌面上的测试模型 (mazda mx-5)
    GLTFAsset asset = loader.LoadGLB("C:\\Users\\isis\\Desktop\\2019_mazda_mx-5.glb");
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
