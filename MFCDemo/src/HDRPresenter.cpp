#include "HDRPresenter.h"

#include <cstdint>
#include <vector>

namespace SR {

using Microsoft::WRL::ComPtr;

/**
 * @brief 初始化 D3D12 设备、交换链及相关资源
 */
bool HDRPresenter::Initialize(HWND hwnd, int width, int height) {
    m_width = width;
    m_height = height;

    if (!CreateDeviceAndSwapchain(hwnd)) {
        return false;
    }

    CreateRenderTargets();
    CreateUploadBuffer();

    m_initialized = true;
    return true;
}

/**
 * @brief 窗口尺寸改变时，重建后台缓冲和上传缓冲
 */
void HDRPresenter::Resize(int width, int height) {
    if (!m_initialized || (width == m_width && height == m_height)) {
        return;
    }

    m_width = width;
    m_height = height;

    // 等待 GPU 处理完当前帧，确保可以安全销毁旧资源
    WaitForGPU();

    for (auto& rt : m_renderTargets) {
        rt.Reset();
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    m_swapChain->GetDesc(&desc);
    m_swapChain->ResizeBuffers(2, width, height, m_format, desc.Flags);

    CreateRenderTargets();
    CreateUploadBuffer();
}

/**
 * @brief 将线性浮点像素数据提交给 D3D12 显示
 * 逻辑：浮点(float) -> 半精度浮点(half) -> 上传缓冲 -> GPU 纹理拷贝 -> 呈现
 */
void HDRPresenter::Present(const Vec3* linearPixels) {
    if (!m_initialized || !linearPixels) {
        return;
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // 等待该帧对应的缓冲区可用
    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
        m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // 映射上传缓冲进行数据填充
    uint8_t* mapped = nullptr;
    D3D12_RANGE readRange{0, 0};
    if (FAILED(m_uploadBuffers[m_frameIndex]->Map(0, &readRange, reinterpret_cast<void**>(&mapped)))) {
        return;
    }

    /**
     * @brief 简单的 float32 到 float16 (half) 的手动转换函数
     * HDR 显示器通常要求 R16G16B16A16 浮点格式
     */
    auto floatToHalf = [](float v) -> uint16_t {
        // IEEE 754 float to half (round to nearest even)
        uint32_t bits = *reinterpret_cast<uint32_t*>(&v);
        uint32_t sign = (bits >> 16) & 0x8000u;
        uint32_t mantissa = bits & 0x007FFFFFu;
        int32_t exp = static_cast<int32_t>((bits >> 23) & 0xFFu) - 127 + 15;

        if (exp <= 0) {
            if (exp < -10) {
                return static_cast<uint16_t>(sign);
            }
            mantissa = (mantissa | 0x00800000u) >> static_cast<uint32_t>(1 - exp);
            return static_cast<uint16_t>(sign | (mantissa + 0x00001000u) >> 13);
        }
        if (exp >= 31) {
            return static_cast<uint16_t>(sign | 0x7C00u);
        }
        return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp) << 10) | ((mantissa + 0x00001000u) >> 13));
    };

    const UINT rowPitch = m_footprint.Footprint.RowPitch;
    const UINT64 pixelStride = sizeof(uint16_t) * 4;

    // 将 CPU 端的 Vec3 HDR 像素转换为 GPU 的半精度 R16G16B16A16 格式
    for (int y = 0; y < m_height; ++y) {
        uint8_t* row = mapped + static_cast<size_t>(y) * rowPitch;
        for (int x = 0; x < m_width; ++x) {
            const Vec3& c = linearPixels[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
            uint16_t* dst = reinterpret_cast<uint16_t*>(row + static_cast<size_t>(x) * pixelStride);
            dst[0] = floatToHalf(static_cast<float>(c.x));
            dst[1] = floatToHalf(static_cast<float>(c.y));
            dst[2] = floatToHalf(static_cast<float>(c.z));
            dst[3] = floatToHalf(1.0f);
        }
    }

    m_uploadBuffers[m_frameIndex]->Unmap(0, nullptr);

    // 录制 GPU 拷贝指令
    m_commandAllocators[m_frameIndex]->Reset();
    m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);

    // 资源屏障：将后台缓冲切换为拷贝目标状态
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = m_renderTargets[m_frameIndex].Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = m_uploadBuffers[m_frameIndex].Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = m_footprint;

    // 执行拷贝：从上传缓冲拷贝到后台缓冲纹理
    m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    // 资源屏障：恢复后台缓冲为呈现状态
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->Close();

    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    // 提交显示
    m_swapChain->Present(0, 0);

    // 设置下一个栅栏值，用于同步
    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    m_fenceValues[m_frameIndex] = m_fenceValue;
}

/**
 * @brief 释放所有 D3D12 相关资源
 */
void HDRPresenter::Shutdown() {
    if (!m_initialized) {
        return;
    }

    WaitForGPU();

    for (auto& rt : m_renderTargets) {
        rt.Reset();
    }
    for (auto& ub : m_uploadBuffers) {
        ub.Reset();
    }
    m_commandList.Reset();
    for (auto& ca : m_commandAllocators) {
        ca.Reset();
    }
    m_rtvHeap.Reset();
    m_swapChain.Reset();
    m_commandQueue.Reset();
    m_device.Reset();
    m_fence.Reset();

    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    m_initialized = false;
}

/**
 * @brief 创建 D3D12 设备和交换链，并启用 HDR 颜色空间
 */
bool HDRPresenter::CreateDeviceAndSwapchain(HWND hwnd) {
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) {
        return false;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)))) {
            break;
        }
    }

    if (!m_device) {
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)))) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.BufferCount = 2;
    scDesc.Width = m_width;
    scDesc.Height = m_height;
    scDesc.Format = m_format;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &scDesc, nullptr, nullptr, &swapChain1))) {
        return false;
    }

    swapChain1.As(&m_swapChain);

    ComPtr<IDXGISwapChain4> swapChain4;
    if (SUCCEEDED(m_swapChain.As(&swapChain4))) {
        // 设置颜色空间为 HDR10 (BT.709, BT.2020) 相关的元数据，确保系统识别为 HDR 内容
        swapChain4->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
    }

    for (auto& ca : m_commandAllocators) {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ca)))) {
            return false;
        }
    }

    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList)))) {
        return false;
    }

    m_commandList->Close();

    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) {
        return false;
    }

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        return false;
    }

    return true;
}

/**
 * @brief 创建渲染目标视图 (RTV) 堆并关联到交换链后台缓冲
 */
void HDRPresenter::CreateRenderTargets() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.NumDescriptors = 2; // 双重缓冲
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    m_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < 2; ++i) {
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, handle);
        handle.ptr += m_rtvDescriptorSize;
    }
}

/**
 * @brief 创建上传缓冲 (Upload Buffer)，用于从 CPU 同步数据到 GPU 纹理
 */
void HDRPresenter::CreateUploadBuffer() {
    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = static_cast<UINT64>(m_width);
    texDesc.Height = static_cast<UINT>(m_height);
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = m_format; // R16G16B16A16_FLOAT
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // 获取上传该纹理所需的内存布局 (Footprint)
    UINT64 totalBytes = 0;
    m_device->GetCopyableFootprints(&texDesc, 0, 1, 0, &m_footprint, nullptr, nullptr, &totalBytes);

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = totalBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    // 为每个缓冲帧创建一个上传资源
    for (auto& ub : m_uploadBuffers) {
        m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&ub));
    }

    m_uploadBufferSize = totalBytes;
}

/**
 * @brief 等待 GPU 执行完成，用于同步和安全资源释放
 */
void HDRPresenter::WaitForGPU() {
    if (!m_commandQueue || !m_fence) {
        return;
    }

    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    if (m_fence->GetCompletedValue() < m_fenceValue) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

} // namespace SR
