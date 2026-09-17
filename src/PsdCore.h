#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace psdx {

class ByteStream {
public:
    virtual ~ByteStream() = default;
    virtual bool seek(uint64_t absolute) = 0;
    virtual uint64_t tell() const = 0;
    virtual uint64_t size() const = 0;
    virtual bool read(void* dst, size_t bytes) = 0;
};

class MemoryStream final : public ByteStream {
public:
    MemoryStream(const uint8_t* data, size_t size);
    bool seek(uint64_t absolute) override;
    uint64_t tell() const override { return pos_; }
    uint64_t size() const override { return size_; }
    bool read(void* dst, size_t bytes) override;
private:
    const uint8_t* data_{};
    uint64_t size_{};
    uint64_t pos_{};
};

struct Header {
    uint16_t version{}; // 1 = PSD, 2 = PSB
    uint16_t channels{};
    uint32_t width{};
    uint32_t height{};
    uint16_t depth{};
    uint16_t colorMode{};
};

struct ImageBGRA {
    uint32_t width{};
    uint32_t height{};
    bool hasAlpha{};
    std::vector<uint8_t> pixels; // BGRA, top-down
};

enum class DecodeStatus {
    Ok,
    Unsupported,
    Invalid,
    IoError,
    TooLarge
};

struct InspectResult {
    DecodeStatus status{DecodeStatus::Invalid};
    Header header{};
    bool hasEmbeddedJpeg{};
    uint32_t embeddedJpegWidth{};
    uint32_t embeddedJpegHeight{};
    uint16_t compositeCompression{0xFFFF}; // 0 RAW, 1 RLE, 2 ZIP, 3 ZIP Prediction
    std::string detail;
};

// Reads only the PSD header and resource table; does not decode the full image.
InspectResult Inspect(ByteStream& stream);

// Extracts Photoshop thumbnail resource 1033/1036 when it contains JPEG data.
DecodeStatus ExtractEmbeddedJpeg(ByteStream& stream, std::vector<uint8_t>& jpeg,
                                 uint32_t& width, uint32_t& height, std::string* detail = nullptr);

// Decodes the merged composite image for common PSD/PSB files using RAW or PackBits RLE.
// The result is downsampled while decoding so memory use is bounded by maxEdge.
DecodeStatus DecodeCompositeThumbnail(ByteStream& stream, uint32_t maxEdge,
                                      ImageBGRA& out, std::string* detail = nullptr);

const char* StatusText(DecodeStatus s);

} // namespace psdx
