#include "Render/RendererConfig.h"

namespace SR {

namespace {
inline int ClampChunk(int chunk) {
    return chunk < 1 ? 1 : chunk;
}
} // namespace

void RendererConfig::Sanitize() {
    openmp.clipChunk = ClampChunk(openmp.clipChunk);
    openmp.binCountChunk = ClampChunk(openmp.binCountChunk);
    openmp.clearChunk = ClampChunk(openmp.clearChunk);
    openmp.postProcessChunk = ClampChunk(openmp.postProcessChunk);
    openmp.rasterTileChunk = ClampChunk(openmp.rasterTileChunk);
    openmp.drawItemBuildChunk = ClampChunk(openmp.drawItemBuildChunk);
}

/**
 * @brief 创建并返回一个默认配置对象
 */
RendererConfig RendererConfig::Default() {
    RendererConfig cfg{};
    cfg.Sanitize();
    return cfg;
}

} // namespace SR
