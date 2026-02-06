#pragma once

#include <string>
#include <vector>

#include "Asset/GLTFTypes.h"

namespace SR {

class ImageDecoder {
public:
    bool Decode(const std::vector<uint8_t>& data, const std::string& mimeType, GLTFImage& outImage);
    const std::string& GetLastError() const;

private:
    std::string m_lastError;
};

} // namespace SR
