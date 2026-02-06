#pragma once

#include <windows.h>

#include "SoftRenderer.h"
#include "Scene/Scene.h"
#include "Scene/ObjectGroup.h"
#include "Scene/Mesh.h"
#include "Scene/Model.h"
#include "Scene/Transform.h"
#include "Scene/LightGroup.h"
#include "Runtime/GPUScene.h"
#include "Asset/GLTFLoader.h"
#include "Camera/OrbitCamera.h"
#include "HDRPresenter.h"

namespace SR {

/**
 * @brief 渲染视图类，封装了渲染器、场景和用户交互逻辑
 */
class CRenderView {
public:
    /** @brief 初始化 HDR 呈现环境 */
    void InitializeHDR(HWND hwnd, int width, int height);
    /** @brief 相应窗口尺寸变化 */
    void Resize(int width, int height);
    /** @brief 执行每帧绘制指令并输出 */
    void DrawHDR();

    /** @brief 获取当前帧率 */
    float GetFPS() const { return m_fps; }

    // 鼠标点击事件分发
    void OnMouseDown(int x, int y, bool leftButton);
    /** @brief 鼠标松开事件 */
    void OnMouseUp(bool leftButton);
    /** @brief 鼠标移动事件，用于控制相机旋转或平移 */
    void OnMouseMove(int x, int y);
    /** @brief 鼠标滚轮事件，用于控制相机缩放 */
    void OnMouseWheel(int delta);

private:
    /** @brief 内部初始化逻辑 */
    void Initialize(int width, int height);
    /** @brief 执行核心渲染流程 */
    void Render();

    int m_width = 0;              ///< 视口宽度
    int m_height = 0;             ///< 视口高度
    Renderer m_renderer;          ///< 渲染器核心实例

    Scene m_scene;                ///< 逻辑场景
    ObjectGroup m_objects;        ///< 场景物体列表
    LightGroup m_lights;          ///< 场景灯光列表
    Mesh m_mesh;                  ///< 默认测试网格
    Model m_model;                ///< 默认测试模型
    OrbitCamera m_camera;         ///< 用户交互相机
    GPUScene m_gpuScene;          ///< 运行时加速场景
    bool m_hasGLB = false;        ///< 是否加载了 GLB 资产

    bool m_useHDR = false;        ///< 是否启用 HDR 模式
    HDRPresenter m_hdrPresenter;   ///< D3D12 HDR 渲染辅助类

    // FPS calculation
    LARGE_INTEGER m_timerFreq{};
    LARGE_INTEGER m_lastTime{};
    float m_fps = 0.0f;
    int m_frameCount = 0;
    float m_fpsUpdateTimer = 0.0f;

    // Mouse state
    bool m_leftButtonDown = false;
    bool m_rightButtonDown = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
};

} // namespace SR

