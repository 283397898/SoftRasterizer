#include "Pipeline/FragmentShader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Asset/GLTFTypes.h"

namespace SR {

namespace {

// Fast inverse square root (Quake III style, adapted for double precision)
// Provides ~1% accuracy with much faster execution than 1/sqrt()
inline double FastInvSqrt(double x) {
    if (x < 1e-12) return 0.0;
    // Use standard sqrt for now - compiler will optimize with -ffast-math
    // For double precision, the bit trick is less effective
    return 1.0 / std::sqrt(x);
}

// Precomputed sRGB to linear lookup table (256 entries)
static double kSRGBToLinearLUT[256];
static bool kSRGBTableInitialized = false;

void InitSRGBTable() {
    if (kSRGBTableInitialized) return;
    for (int i = 0; i < 256; ++i) {
        double v = i / 255.0;
        if (v <= 0.04045) {
            kSRGBToLinearLUT[i] = v / 12.92;
        } else {
            kSRGBToLinearLUT[i] = std::pow((v + 0.055) / 1.055, 2.4);
        }
    }
    kSRGBTableInitialized = true;
}

/**
 * @brief sRGB 到线性空间的快速查表转换
 */
inline double SRGBToLinearFast(uint8_t v) {
    if (!kSRGBTableInitialized) InitSRGBTable();
    return kSRGBToLinearLUT[v];
}

double Clamp01(double v) {
    return std::max(0.0, std::min(1.0, v));
}

Vec3 Clamp01(const Vec3& v) {
    return Vec3{Clamp01(v.x), Clamp01(v.y), Clamp01(v.z)};
}

Vec3 Add(const Vec3& a, const Vec3& b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Mul(const Vec3& a, const Vec3& b) {
    return Vec3{a.x * b.x, a.y * b.y, a.z * b.z};
}

Vec3 Mul(const Vec3& a, double s) {
    return Vec3{a.x * s, a.y * s, a.z * s};
}

Vec3 Div(const Vec3& a, double s) {
    return Vec3{a.x / s, a.y / s, a.z / s};
}

Vec3 Lerp(const Vec3& a, const Vec3& b, double t) {
    return Vec3{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kInvPi = 1.0 / kPi;

constexpr bool kDebugTextureIndexTint = false;

inline double Saturate(double v) {
    return (v < 0.0) ? 0.0 : ((v > 1.0) ? 1.0 : v);
}

/**
 * @brief Fresnel-Schlick 近似公式
 */
inline Vec3 FresnelSchlick(double cosTheta, const Vec3& F0) {
    double t = 1.0 - Saturate(cosTheta);
    double t2 = t * t;
    double t5 = t2 * t2 * t;  // Fast pow5
    return Vec3{
        F0.x + (1.0 - F0.x) * t5,
        F0.y + (1.0 - F0.y) * t5,
        F0.z + (1.0 - F0.z) * t5
    };
}

double SRGBToLinear(double v) {
    return std::pow(v, 2.2);
}

struct SampledColor {
    Vec3 rgb{1.0, 1.0, 1.0};
    double a = 1.0;
};

double WrapCoord(double v, int mode) {
    if (mode == 33071) { // CLAMP_TO_EDGE
        return std::max(0.0, std::min(1.0, v));
    }
    if (mode == 33648) { // MIRRORED_REPEAT
        double w = std::fmod(v, 2.0);
        if (w < 0.0) w += 2.0;
        return (w > 1.0) ? (2.0 - w) : w;
    }
    // REPEAT
    double w = std::fmod(v, 1.0);
    if (w < 0.0) w += 1.0;
    return w;
}

SampledColor SampleImageNearest(const GLTFImage& image, const GLTFSampler* sampler, const Vec2& uv) {
    int wrapS = sampler ? sampler->wrapS : 10497;
    int wrapT = sampler ? sampler->wrapT : 10497;

    double u = WrapCoord(uv.x, wrapS);
    double v = WrapCoord(uv.y, wrapT);

    int x = static_cast<int>(u * image.width);
    int y = static_cast<int>((1.0 - v) * image.height);
    x = std::max(0, std::min(x, image.width - 1));
    y = std::max(0, std::min(y, image.height - 1));

    size_t index = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4;
    if (index + 3 >= image.pixels.size()) {
        return {};
    }

    const uint8_t* p = image.pixels.data() + index;
    double r, g, b;
    if (image.isSRGB) {
        r = SRGBToLinearFast(p[0]);
        g = SRGBToLinearFast(p[1]);
        b = SRGBToLinearFast(p[2]);
    } else {
        r = p[0] / 255.0;
        g = p[1] / 255.0;
        b = p[2] / 255.0;
    }
    double a = p[3] / 255.0;

    return {Vec3{r, g, b}, a};
}

SampledColor SampleImageBilinear(const GLTFImage& image, const GLTFSampler* sampler, const Vec2& uv) {
    int wrapS = sampler ? sampler->wrapS : 10497;
    int wrapT = sampler ? sampler->wrapT : 10497;

    double u = WrapCoord(uv.x, wrapS);
    double v = WrapCoord(uv.y, wrapT);

    double fx = u * (image.width - 1);
    double fy = (1.0 - v) * (image.height - 1);
    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int x1 = std::min(x0 + 1, image.width - 1);
    int y1 = std::min(y0 + 1, image.height - 1);
    double tx = fx - x0;
    double ty = fy - y0;

    auto sample = [&](int x, int y) -> SampledColor {
        size_t index = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4;
        if (index + 3 >= image.pixels.size()) {
            return {};
        }
        const uint8_t* p = image.pixels.data() + index;
        double r, g, b;
        if (image.isSRGB) {
            r = SRGBToLinearFast(p[0]);
            g = SRGBToLinearFast(p[1]);
            b = SRGBToLinearFast(p[2]);
        } else {
            r = p[0] / 255.0;
            g = p[1] / 255.0;
            b = p[2] / 255.0;
        }
        double a = p[3] / 255.0;
        return {Vec3{r, g, b}, a};
    };

    SampledColor c00 = sample(x0, y0);
    SampledColor c10 = sample(x1, y0);
    SampledColor c01 = sample(x0, y1);
    SampledColor c11 = sample(x1, y1);

    Vec3 c0 = c00.rgb * (1.0 - tx) + c10.rgb * tx;
    Vec3 c1 = c01.rgb * (1.0 - tx) + c11.rgb * tx;
    Vec3 rgb = c0 * (1.0 - ty) + c1 * ty;
    double a0 = c00.a * (1.0 - tx) + c10.a * tx;
    double a1 = c01.a * (1.0 - tx) + c11.a * tx;
    double a = a0 * (1.0 - ty) + a1 * ty;
    return {rgb, a};
}

bool UseLinearFilter(const GLTFSampler* sampler) {
    if (!sampler) {
        return false;
    }
    int minFilter = sampler->minFilter;
    int magFilter = sampler->magFilter;
    if (magFilter == 9729) {
        return true;
    }
    if (minFilter == 9729 || minFilter == 9984 || minFilter == 9985 || minFilter == 9986 || minFilter == 9987) {
        return true;
    }
    return false;
}

SampledColor SampleBaseColor(const FragmentInput& input) {
    SampledColor result{};
    if (!input.images || input.baseColorImageIndex < 0 || input.baseColorImageIndex >= static_cast<int>(input.images->size())) {
        return result;
    }

    const GLTFImage& image = (*input.images)[input.baseColorImageIndex];
    const GLTFSampler* sampler = nullptr;
    if (input.samplers && input.baseColorSamplerIndex >= 0 && input.baseColorSamplerIndex < static_cast<int>(input.samplers->size())) {
        sampler = &(*input.samplers)[input.baseColorSamplerIndex];
    }
    if (UseLinearFilter(sampler)) {
        return SampleImageBilinear(image, sampler, input.texCoord);
    }
    return SampleImageNearest(image, sampler, input.texCoord);
}

SampledColor SampleGeneric(const FragmentInput& input, int imageIndex, int samplerIndex, bool srgb) {
    if (!input.images || imageIndex < 0 || imageIndex >= static_cast<int>(input.images->size())) {
        return {};
    }
    const GLTFImage& image = (*input.images)[imageIndex];
    const GLTFSampler* sampler = nullptr;
    if (input.samplers && samplerIndex >= 0 && samplerIndex < static_cast<int>(input.samplers->size())) {
        sampler = &(*input.samplers)[samplerIndex];
    }
    
    // Create a temporary wrapper to handle sRGB override without copying the pixel data
    int wrapS = sampler ? sampler->wrapS : 10497;
    int wrapT = sampler ? sampler->wrapT : 10497;
    double u = WrapCoord(input.texCoord.x, wrapS);
    double v = WrapCoord(input.texCoord.y, wrapT);
    
    bool useLinear = UseLinearFilter(sampler);
    bool useSRGB = srgb || image.isSRGB;
    
    if (useLinear) {
        double fx = u * (image.width - 1);
        double fy = (1.0 - v) * (image.height - 1);
        int x0 = static_cast<int>(fx);
        int y0 = static_cast<int>(fy);
        int x1 = std::min(x0 + 1, image.width - 1);
        int y1 = std::min(y0 + 1, image.height - 1);
        double tx = fx - x0;
        double ty = fy - y0;

        auto sample = [&](int x, int y) -> SampledColor {
            size_t index = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4;
            if (index + 3 >= image.pixels.size()) return {};
            const uint8_t* p = image.pixels.data() + index;
            double r, g, b;
            if (useSRGB) {
                r = SRGBToLinearFast(p[0]);
                g = SRGBToLinearFast(p[1]);
                b = SRGBToLinearFast(p[2]);
            } else {
                r = p[0] / 255.0;
                g = p[1] / 255.0;
                b = p[2] / 255.0;
            }
            return {Vec3{r, g, b}, p[3] / 255.0};
        };
        SampledColor c00 = sample(x0, y0);
        SampledColor c10 = sample(x1, y0);
        SampledColor c01 = sample(x0, y1);
        SampledColor c11 = sample(x1, y1);
        Vec3 c0 = c00.rgb * (1.0 - tx) + c10.rgb * tx;
        Vec3 c1 = c01.rgb * (1.0 - tx) + c11.rgb * tx;
        Vec3 rgb = c0 * (1.0 - ty) + c1 * ty;
        double a0 = c00.a * (1.0 - tx) + c10.a * tx;
        double a1 = c01.a * (1.0 - tx) + c11.a * tx;
        return {rgb, a0 * (1.0 - ty) + a1 * ty};
    } else {
        int x = static_cast<int>(u * image.width);
        int y = static_cast<int>((1.0 - v) * image.height);
        x = std::max(0, std::min(x, image.width - 1));
        y = std::max(0, std::min(y, image.height - 1));
        size_t index = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4;
        if (index + 3 >= image.pixels.size()) return {};
        const uint8_t* p = image.pixels.data() + index;
        double r, g, b;
        if (useSRGB) {
            r = SRGBToLinearFast(p[0]);
            g = SRGBToLinearFast(p[1]);
            b = SRGBToLinearFast(p[2]);
        } else {
            r = p[0] / 255.0;
            g = p[1] / 255.0;
            b = p[2] / 255.0;
        }
        return {Vec3{r, g, b}, p[3] / 255.0};
    }
}

Vec3 TintFromIndex(int index) {
    if (index < 0) {
        return Vec3{1.0, 1.0, 1.0};
    }
    uint32_t v = static_cast<uint32_t>(index) * 2654435761u;
    double r = ((v >> 0) & 0xFF) / 255.0;
    double g = ((v >> 8) & 0xFF) / 255.0;
    double b = ((v >> 16) & 0xFF) / 255.0;
    return Vec3{0.5 + 0.5 * r, 0.5 + 0.5 * g, 0.5 + 0.5 * b};
}

inline double DistributionGGX(double ndoth, double roughness) {
    double a = roughness * roughness;
    double a2 = a * a;
    double denom = (ndoth * ndoth) * (a2 - 1.0) + 1.0;
    return a2 * kInvPi / (denom * denom + 1e-12);
}

inline double GeometrySchlickGGX(double ndotv, double roughness) {
    double r = roughness + 1.0;
    double k = (r * r) * 0.125;  // k = (r*r) / 8
    return ndotv / (ndotv * (1.0 - k) + k + 1e-12);
}

inline double GeometrySmith(double ndotv, double ndotl, double roughness) {
    double r = roughness + 1.0;
    double k = (r * r) * 0.125;
    double ggx1 = ndotv / (ndotv * (1.0 - k) + k + 1e-12);
    double ggx2 = ndotl / (ndotl * (1.0 - k) + k + 1e-12);
    return ggx1 * ggx2;
}

} // namespace

Vec3 FragmentShader::Shade(const FragmentInput& input) const {
    Vec3 N = input.normal.Normalized();
    Vec3 V = (input.cameraPos - input.worldPos).Normalized();

    double roughness = std::max(0.04, input.material.roughness);
    double metallic = Saturate(input.material.metallic);
    Vec3 albedo = Clamp01(input.material.albedo);
    double alpha = input.material.alpha;

    SampledColor baseColor = SampleBaseColor(input);
    albedo = Mul(albedo, baseColor.rgb);
    alpha *= baseColor.a;
    if (input.metallicRoughnessImageIndex >= 0) {
        SampledColor mr = SampleGeneric(input, input.metallicRoughnessImageIndex, input.metallicRoughnessSamplerIndex, false);
        metallic = Saturate(metallic * mr.rgb.z);
        roughness = std::max(0.04, mr.rgb.y * roughness);
    }

    if (input.normalImageIndex >= 0) {
        Vec3 T = input.tangent.Normalized();
        if (T.x != 0.0 || T.y != 0.0 || T.z != 0.0) {
            SampledColor nm = SampleGeneric(input, input.normalImageIndex, input.normalSamplerIndex, false);
            Vec3 tangentNormal{nm.rgb.x * 2.0 - 1.0, nm.rgb.y * 2.0 - 1.0, nm.rgb.z * 2.0 - 1.0};
            Vec3 B = Vec3::Cross(N, T).Normalized();
            Vec3 worldNormal{
                T.x * tangentNormal.x + B.x * tangentNormal.y + N.x * tangentNormal.z,
                T.y * tangentNormal.x + B.y * tangentNormal.y + N.y * tangentNormal.z,
                T.z * tangentNormal.x + B.z * tangentNormal.y + N.z * tangentNormal.z
            };
            N = worldNormal.Normalized();
        }
    }

    if (kDebugTextureIndexTint && input.baseColorImageIndex >= 0) {
        albedo = Mul(albedo, TintFromIndex(input.baseColorImageIndex));
    }

    Vec3 F0 = Lerp(Vec3{0.04, 0.04, 0.04}, albedo, metallic);

    double ndotv = std::max(0.0, Vec3::Dot(N, V));

    Vec3 Lo{0.0, 0.0, 0.0};

    // Accumulate lighting from all directional lights
    if (input.lights) {
        for (const DirectionalLight& light : *input.lights) {
            Vec3 L = Vec3{-light.direction.x, -light.direction.y, -light.direction.z}.Normalized();
            Vec3 H = (L + V).Normalized();

            double ndotl = std::max(0.0, Vec3::Dot(N, L));
            double ndoth = std::max(0.0, Vec3::Dot(N, H));
            double vdoth = std::max(0.0, Vec3::Dot(V, H));

            Vec3 F = FresnelSchlick(vdoth, F0);
            double D = DistributionGGX(ndoth, roughness);
            double G = GeometrySmith(ndotv, ndotl, roughness);

            Vec3 numerator = Mul(F, D * G);
            double denom = 4.0 * ndotv * ndotl + 1e-12;
            Vec3 specular = Div(numerator, denom);

            Vec3 kS = F;
            Vec3 kD = Mul(Sub(Vec3{1.0, 1.0, 1.0}, kS), 1.0 - metallic);
            Vec3 diffuse = Mul(kD, Div(albedo, kPi));

            Vec3 radiance = Mul(light.color, light.intensity);
            Vec3 contrib = Mul(Add(diffuse, specular), ndotl);
            contrib = Mul(contrib, radiance);

            Lo = Add(Lo, contrib);
        }
    }

    Vec3 ambient = Mul(input.ambientColor, albedo);
    if (input.occlusionImageIndex >= 0) {
        SampledColor occ = SampleGeneric(input, input.occlusionImageIndex, input.occlusionSamplerIndex, false);
        ambient = Mul(ambient, occ.rgb.x);
    }
    Vec3 color = Add(ambient, Lo);
    if (input.emissiveImageIndex >= 0) {
        SampledColor emissive = SampleGeneric(input, input.emissiveImageIndex, input.emissiveSamplerIndex, true);
        color = Add(color, emissive.rgb);
    }

    return color;
}

// ============================================================================
// Optimized ShadeFast - separates per-triangle context from per-pixel varying
// ============================================================================

namespace {

// Fast texture sampling without FragmentInput wrapper
// Optimized texture sampling with precomputed constants and reduced branching
SampledColor SampleImageFast(const std::vector<GLTFImage>* images, 
                             const std::vector<GLTFSampler>* samplers,
                             int imageIndex, int samplerIndex, 
                             const Vec2& texCoord, bool srgb) {
    if (!images || imageIndex < 0 || imageIndex >= static_cast<int>(images->size())) {
        return {};
    }
    const GLTFImage& image = (*images)[imageIndex];
    const GLTFSampler* sampler = nullptr;
    if (samplers && samplerIndex >= 0 && samplerIndex < static_cast<int>(samplers->size())) {
        sampler = &(*samplers)[samplerIndex];
    }
    
    // Precompute constants (these could be cached per-image in future)
    const int wrapS = sampler ? sampler->wrapS : 10497;
    const int wrapT = sampler ? sampler->wrapT : 10497;
    const double u = WrapCoord(texCoord.x, wrapS);
    const double v = WrapCoord(texCoord.y, wrapT);
    
    const bool useLinear = UseLinearFilter(sampler);
    const bool useSRGB = srgb || image.isSRGB;
    
    // Precompute frequently used values
    const int w = image.width;
    const int h = image.height;
    const int stride = w * 4;  // RGBA stride
    const uint8_t* pixelData = image.pixels.data();
    constexpr double inv255 = 1.0 / 255.0;
    
    if (useLinear) {
        // Bilinear filtering
        const double fx = u * (w - 1);
        const double fy = (1.0 - v) * (h - 1);
        const int x0 = static_cast<int>(fx);
        const int y0 = static_cast<int>(fy);
        const int x1 = (x0 + 1 < w) ? x0 + 1 : x0;  // Branchless clamp
        const int y1 = (y0 + 1 < h) ? y0 + 1 : y0;
        const double tx = fx - x0;
        const double ty = fy - y0;
        const double omtx = 1.0 - tx;
        const double omty = 1.0 - ty;

        // Direct pointer arithmetic (no lambda overhead)
        const uint8_t* p00 = pixelData + (y0 * stride + x0 * 4);
        const uint8_t* p10 = pixelData + (y0 * stride + x1 * 4);
        const uint8_t* p01 = pixelData + (y1 * stride + x0 * 4);
        const uint8_t* p11 = pixelData + (y1 * stride + x1 * 4);
        
        double r0, g0, b0, a0, r1, g1, b1, a1, r2, g2, b2, a2, r3, g3, b3, a3;
        if (useSRGB) {
            r0 = SRGBToLinearFast(p00[0]); g0 = SRGBToLinearFast(p00[1]); b0 = SRGBToLinearFast(p00[2]);
            r1 = SRGBToLinearFast(p10[0]); g1 = SRGBToLinearFast(p10[1]); b1 = SRGBToLinearFast(p10[2]);
            r2 = SRGBToLinearFast(p01[0]); g2 = SRGBToLinearFast(p01[1]); b2 = SRGBToLinearFast(p01[2]);
            r3 = SRGBToLinearFast(p11[0]); g3 = SRGBToLinearFast(p11[1]); b3 = SRGBToLinearFast(p11[2]);
        } else {
            r0 = p00[0] * inv255; g0 = p00[1] * inv255; b0 = p00[2] * inv255;
            r1 = p10[0] * inv255; g1 = p10[1] * inv255; b1 = p10[2] * inv255;
            r2 = p01[0] * inv255; g2 = p01[1] * inv255; b2 = p01[2] * inv255;
            r3 = p11[0] * inv255; g3 = p11[1] * inv255; b3 = p11[2] * inv255;
        }
        a0 = p00[3] * inv255; a1 = p10[3] * inv255;
        a2 = p01[3] * inv255; a3 = p11[3] * inv255;
        
        // Bilinear interpolation (fused multiply-add friendly)
        const double w00 = omtx * omty;
        const double w10 = tx * omty;
        const double w01 = omtx * ty;
        const double w11 = tx * ty;
        
        return {
            Vec3{
                r0 * w00 + r1 * w10 + r2 * w01 + r3 * w11,
                g0 * w00 + g1 * w10 + g2 * w01 + g3 * w11,
                b0 * w00 + b1 * w10 + b2 * w01 + b3 * w11
            },
            a0 * w00 + a1 * w10 + a2 * w01 + a3 * w11
        };
    } else {
        // Nearest filtering
        int x = static_cast<int>(u * w);
        int y = static_cast<int>((1.0 - v) * h);
        // Branchless clamp
        x = (x < 0) ? 0 : ((x >= w) ? w - 1 : x);
        y = (y < 0) ? 0 : ((y >= h) ? h - 1 : y);
        
        const uint8_t* p = pixelData + (y * stride + x * 4);
        double r, g, b;
        if (useSRGB) {
            r = SRGBToLinearFast(p[0]);
            g = SRGBToLinearFast(p[1]);
            b = SRGBToLinearFast(p[2]);
        } else {
            r = p[0] * inv255;
            g = p[1] * inv255;
            b = p[2] * inv255;
        }
        return {Vec3{r, g, b}, p[3] * inv255};
    }
}

} // namespace

/**
 * @brief 高性能片元着色实现
 */
Vec3 FragmentShader::ShadeFast(const FragmentContext& ctx, const FragmentVarying& varying) const {
    // Inline normalize for N (avoid function call overhead)
    Vec3 N = varying.normal;
    double nLenSq = N.x * N.x + N.y * N.y + N.z * N.z;
    if (nLenSq > 1e-12) {
        double invNLen = 1.0 / std::sqrt(nLenSq);
        N.x *= invNLen; N.y *= invNLen; N.z *= invNLen;
    }
    
    // Inline normalize for V (avoid function call overhead)
    Vec3 V{ctx.cameraPos.x - varying.worldPos.x, 
           ctx.cameraPos.y - varying.worldPos.y, 
           ctx.cameraPos.z - varying.worldPos.z};
    double vLenSq = V.x * V.x + V.y * V.y + V.z * V.z;
    if (vLenSq > 1e-12) {
        double invVLen = 1.0 / std::sqrt(vLenSq);
        V.x *= invVLen; V.y *= invVLen; V.z *= invVLen;
    }

    double roughness = std::max(0.04, ctx.material->roughness);
    double metallic = Saturate(ctx.material->metallic);
    Vec3 albedo = Clamp01(ctx.material->albedo);

    // Sample base color texture
    if (ctx.baseColorImageIndex >= 0) {
        SampledColor baseColor = SampleImageFast(ctx.images, ctx.samplers, 
            ctx.baseColorImageIndex, ctx.baseColorSamplerIndex, varying.texCoord, true);
        albedo = Mul(albedo, baseColor.rgb);
    }
    
    // Sample metallic-roughness texture
    if (ctx.metallicRoughnessImageIndex >= 0) {
        SampledColor mr = SampleImageFast(ctx.images, ctx.samplers,
            ctx.metallicRoughnessImageIndex, ctx.metallicRoughnessSamplerIndex, varying.texCoord, false);
        metallic = Saturate(metallic * mr.rgb.z);
        roughness = std::max(0.04, mr.rgb.y * roughness);
    }

    // Apply normal mapping
    if (ctx.normalImageIndex >= 0) {
        // Inline normalize for T
        Vec3 T = varying.tangent;
        double tLenSq = T.x * T.x + T.y * T.y + T.z * T.z;
        if (tLenSq > 1e-12) {
            double invTLen = 1.0 / std::sqrt(tLenSq);
            T.x *= invTLen; T.y *= invTLen; T.z *= invTLen;
            
            SampledColor nm = SampleImageFast(ctx.images, ctx.samplers,
                ctx.normalImageIndex, ctx.normalSamplerIndex, varying.texCoord, false);
            Vec3 tangentNormal{nm.rgb.x * 2.0 - 1.0, nm.rgb.y * 2.0 - 1.0, nm.rgb.z * 2.0 - 1.0};
            
            // Inline Cross and normalize for B
            Vec3 B{N.y * T.z - N.z * T.y, N.z * T.x - N.x * T.z, N.x * T.y - N.y * T.x};
            double bLenSq = B.x * B.x + B.y * B.y + B.z * B.z;
            if (bLenSq > 1e-12) {
                double invBLen = 1.0 / std::sqrt(bLenSq);
                B.x *= invBLen; B.y *= invBLen; B.z *= invBLen;
            }
            
            Vec3 worldNormal{
                T.x * tangentNormal.x + B.x * tangentNormal.y + N.x * tangentNormal.z,
                T.y * tangentNormal.x + B.y * tangentNormal.y + N.y * tangentNormal.z,
                T.z * tangentNormal.x + B.z * tangentNormal.y + N.z * tangentNormal.z
            };
            
            // Inline normalize for worldNormal
            double wnLenSq = worldNormal.x * worldNormal.x + worldNormal.y * worldNormal.y + worldNormal.z * worldNormal.z;
            if (wnLenSq > 1e-12) {
                double invWnLen = 1.0 / std::sqrt(wnLenSq);
                N.x = worldNormal.x * invWnLen;
                N.y = worldNormal.y * invWnLen;
                N.z = worldNormal.z * invWnLen;
            }
        }
    }

    Vec3 F0 = Lerp(Vec3{0.04, 0.04, 0.04}, albedo, metallic);
    double ndotv = std::max(0.0, Vec3::Dot(N, V));

    Vec3 Lo{0.0, 0.0, 0.0};

    // Use precomputed light data if available (pointer-based, no vector copy)
    if (ctx.precomputedLights && ctx.precomputedLightCount > 0) {
        for (size_t i = 0; i < ctx.precomputedLightCount; ++i) {
            const PrecomputedLight& pl = ctx.precomputedLights[i];
            const Vec3& L = pl.L;
            
            double ndotl = Vec3::Dot(N, L);
            
            // Early exit if light is on back side
            if (ndotl <= 0.0) continue;
            
            // Compute H = normalize(L + V) - inline to avoid function call
            Vec3 H{L.x + V.x, L.y + V.y, L.z + V.z};
            double hLenSq = H.x * H.x + H.y * H.y + H.z * H.z;
            if (hLenSq > 1e-12) {
                double invHLen = 1.0 / std::sqrt(hLenSq);
                H.x *= invHLen; H.y *= invHLen; H.z *= invHLen;
            }

            double ndoth = std::max(0.0, Vec3::Dot(N, H));
            double vdoth = std::max(0.0, Vec3::Dot(V, H));

            Vec3 F = FresnelSchlick(vdoth, F0);
            double D = DistributionGGX(ndoth, roughness);
            double G = GeometrySmith(ndotv, ndotl, roughness);

            Vec3 numerator = Mul(F, D * G);
            double denom = 4.0 * ndotv * ndotl + 1e-12;
            Vec3 specular = Div(numerator, denom);

            Vec3 kS = F;
            Vec3 kD = Mul(Sub(Vec3{1.0, 1.0, 1.0}, kS), 1.0 - metallic);
            Vec3 diffuse = Mul(kD, Div(albedo, kPi));

            Vec3 contrib = Mul(Add(diffuse, specular), ndotl);
            contrib = Mul(contrib, pl.radiance);  // Use precomputed radiance

            Lo = Add(Lo, contrib);
        }
    } else if (ctx.lights) {
        // Fallback to original path (for backwards compatibility)
        for (const DirectionalLight& light : *ctx.lights) {
            Vec3 L = Vec3{-light.direction.x, -light.direction.y, -light.direction.z}.Normalized();
            Vec3 H = (L + V).Normalized();

            double ndotl = std::max(0.0, Vec3::Dot(N, L));
            double ndoth = std::max(0.0, Vec3::Dot(N, H));
            double vdoth = std::max(0.0, Vec3::Dot(V, H));

            Vec3 F = FresnelSchlick(vdoth, F0);
            double D = DistributionGGX(ndoth, roughness);
            double G = GeometrySmith(ndotv, ndotl, roughness);

            Vec3 numerator = Mul(F, D * G);
            double denom = 4.0 * ndotv * ndotl + 1e-12;
            Vec3 specular = Div(numerator, denom);

            Vec3 kS = F;
            Vec3 kD = Mul(Sub(Vec3{1.0, 1.0, 1.0}, kS), 1.0 - metallic);
            Vec3 diffuse = Mul(kD, Div(albedo, kPi));

            Vec3 radiance = Mul(light.color, light.intensity);
            Vec3 contrib = Mul(Add(diffuse, specular), ndotl);
            contrib = Mul(contrib, radiance);

            Lo = Add(Lo, contrib);
        }
    }

    Vec3 ambient = Mul(ctx.ambientColor, albedo);
    if (ctx.occlusionImageIndex >= 0) {
        SampledColor occ = SampleImageFast(ctx.images, ctx.samplers,
            ctx.occlusionImageIndex, ctx.occlusionSamplerIndex, varying.texCoord, false);
        ambient = Mul(ambient, occ.rgb.x);
    }
    Vec3 color = Add(ambient, Lo);
    if (ctx.emissiveImageIndex >= 0) {
        SampledColor emissive = SampleImageFast(ctx.images, ctx.samplers,
            ctx.emissiveImageIndex, ctx.emissiveSamplerIndex, varying.texCoord, true);
        color = Add(color, emissive.rgb);
    }

    return color;
}

} // namespace SR
