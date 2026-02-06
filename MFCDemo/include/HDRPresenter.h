#pragma once

#include <windows.h>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "Math/Vec3.h"

namespace SR {

/**
 * @brief D3D12 HDR 视频呈现器，用于将浮点线性颜色输出到屏幕
 */
class HDRPresenter {
public:
    /** @brief 初始化 Direct3D 12 设备与交换链 */
    bool Initialize(HWND hwnd, int width, int height);
    /** @brief 响应窗口大小调整 */
    void Resize(int width, int height);
    /** @brief 将线性浮点像素数据提交并显示 */
    void Present(const Vec3* linearPixels);
    /** @brief 释放所有 D3D12 资源 */
    void Shutdown();

    /** @brief 检查是否已初始化 */
    bool IsInitialized() const { return m_initialized; }

private:
    bool CreateDeviceAndSwapchain(HWND hwnd);
    void CreateRenderTargets();
    void CreateUploadBuffer();
    void WaitForGPU();

    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[2];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[2];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffers[2];

    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceValue = 0;
    UINT64 m_fenceValues[2] = {0, 0};
    UINT m_rtvDescriptorSize = 0;

    DXGI_FORMAT m_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;

    UINT m_frameIndex = 0;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_footprint{};
    UINT64 m_uploadBufferSize = 0;
};

} // namespace SR
