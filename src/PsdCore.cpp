#include "PsdCore.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <cmath>

namespace psdx {

MemoryStream::MemoryStream(const uint8_t* data, size_t size) : data_(data), size_(size), pos_(0) {}

bool MemoryStream::seek(uint64_t absolute) {
    if (absolute > size_) return false;
    pos_ = absolute;
    return true;
}

bool MemoryStream::read(void* dst, size_t bytes) {
    if (!dst && bytes) return false;
    if (bytes > size_ - pos_) return false;
    if (bytes) std::memcpy(dst, data_ + pos_, bytes);
    pos_ += bytes;
    return true;
}

namespace {

bool readU8(ByteStream& s, uint8_t& v) { return s.read(&v, 1); }

bool readU16(ByteStream& s, uint16_t& v) {
    uint8_t b[2];
    if (!s.read(b, 2)) return false;
    v = static_cast<uint16_t>((uint16_t(b[0]) << 8) | b[1]);
    return true;
}

bool readU32(ByteStream& s, uint32_t& v) {
    uint8_t b[4];
    if (!s.read(b, 4)) return false;
    v = (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | uint32_t(b[3]);
    return true;
}

bool readU64(ByteStream& s, uint64_t& v) {
    uint8_t b[8];
    if (!s.read(b, 8)) return false;
    v = (uint64_t(b[0]) << 56) | (uint64_t(b[1]) << 48) | (uint64_t(b[2]) << 40) | (uint64_t(b[3]) << 32) |
        (uint64_t(b[4]) << 24) | (uint64_t(b[5]) << 16) | (uint64_t(b[6]) << 8) | uint64_t(b[7]);
    return true;
}

bool addSafe(uint64_t a, uint64_t b, uint64_t& out) {
    if (b > std::numeric_limits<uint64_t>::max() - a) return false;
    out = a + b;
    return true;
}

DecodeStatus parseHeader(ByteStream& s, Header& h, std::string* detail) {
    if (!s.seek(0)) return DecodeStatus::IoError;
    char sig[4];
    if (!s.read(sig, 4)) return DecodeStatus::IoError;
    if (std::memcmp(sig, "8BPS", 4) != 0) {
        if (detail) *detail = "Firma 8BPS ausente";
        return DecodeStatus::Invalid;
    }
    if (!readU16(s, h.version)) return DecodeStatus::IoError;
    if (h.version != 1 && h.version != 2) {
        if (detail) *detail = "Versión PSD/PSB no soportada";
        return DecodeStatus::Unsupported;
    }
    uint8_t reserved[6];
    if (!s.read(reserved, sizeof(reserved))) return DecodeStatus::IoError;
    if (!readU16(s, h.channels) || !readU32(s, h.height) || !readU32(s, h.width) ||
        !readU16(s, h.depth) || !readU16(s, h.colorMode)) return DecodeStatus::IoError;

    if (h.channels == 0 || h.channels > 56 || h.width == 0 || h.height == 0) {
        if (detail) *detail = "Dimensiones/canales inválidos";
        return DecodeStatus::Invalid;
    }
    const uint32_t maxDim = h.version == 1 ? 30000u : 300000u;
    if (h.width > maxDim || h.height > maxDim) {
        if (detail) *detail = "Dimensiones fuera del límite PSD/PSB";
        return DecodeStatus::Invalid;
    }
    if (h.depth != 8 && h.depth != 16) {
        if (detail) *detail = "Solo profundidad 8/16 bits en fallback composite";
        // Header is still valid. Caller may still extract JPEG thumbnail.
    }
    return DecodeStatus::Ok;
}

struct Sections {
    Header h{};
    uint64_t resourcesStart{};
    uint64_t resourcesLength{};
    uint64_t layerMaskLengthField{};
    uint64_t imageDataStart{};
};

DecodeStatus locateSections(ByteStream& s, Sections& sec, std::string* detail) {
    DecodeStatus st = parseHeader(s, sec.h, detail);
    if (st != DecodeStatus::Ok) return st;

    uint32_t colorLen = 0;
    if (!readU32(s, colorLen)) return DecodeStatus::IoError;
    uint64_t pos = s.tell(), next = 0;
    if (!addSafe(pos, colorLen, next) || next > s.size() || !s.seek(next)) return DecodeStatus::Invalid;

    uint32_t resLen = 0;
    if (!readU32(s, resLen)) return DecodeStatus::IoError;
    sec.resourcesStart = s.tell();
    sec.resourcesLength = resLen;
    if (!addSafe(sec.resourcesStart, sec.resourcesLength, next) || next > s.size() || !s.seek(next)) return DecodeStatus::Invalid;

    sec.layerMaskLengthField = s.tell();
    uint64_t layerLen = 0;
    if (sec.h.version == 1) {
        uint32_t v = 0;
        if (!readU32(s, v)) return DecodeStatus::IoError;
        layerLen = v;
    } else {
        if (!readU64(s, layerLen)) return DecodeStatus::IoError;
    }
    pos = s.tell();
    if (!addSafe(pos, layerLen, next) || next > s.size() || !s.seek(next)) return DecodeStatus::Invalid;
    sec.imageDataStart = s.tell();
    return DecodeStatus::Ok;
}

DecodeStatus findEmbeddedJpeg(ByteStream& s, const Sections& sec, std::vector<uint8_t>* jpeg,
                              uint32_t* outW, uint32_t* outH, std::string* detail) {
    uint64_t end = 0;
    if (!addSafe(sec.resourcesStart, sec.resourcesLength, end) || end > s.size()) return DecodeStatus::Invalid;
    if (!s.seek(sec.resourcesStart)) return DecodeStatus::IoError;

    while (s.tell() + 12 <= end) {
        char sig[4];
        if (!s.read(sig, 4)) return DecodeStatus::IoError;
        if (std::memcmp(sig, "8BIM", 4) != 0 && std::memcmp(sig, "MeSa", 4) != 0) {
            if (detail) *detail = "Tabla de recursos PSD inválida";
            return DecodeStatus::Invalid;
        }
        uint16_t id = 0;
        if (!readU16(s, id)) return DecodeStatus::IoError;

        uint8_t nameLen = 0;
        if (!readU8(s, nameLen)) return DecodeStatus::IoError;
        uint64_t skipName = nameLen;
        if (s.tell() + skipName > end || !s.seek(s.tell() + skipName)) return DecodeStatus::Invalid;
        // Pascal string incl. length byte is padded to even byte count.
        if (((1u + nameLen) & 1u) != 0u) {
            if (s.tell() + 1 > end || !s.seek(s.tell() + 1)) return DecodeStatus::Invalid;
        }

        uint32_t dataSize = 0;
        if (!readU32(s, dataSize)) return DecodeStatus::IoError;
        uint64_t dataStart = s.tell();
        uint64_t dataEnd = 0;
        if (!addSafe(dataStart, dataSize, dataEnd) || dataEnd > end) return DecodeStatus::Invalid;

        if ((id == 1033 || id == 1036) && dataSize >= 28) {
            uint32_t format=0,w=0,h=0,widthBytes=0,totalSize=0,compressedSize=0;
            uint16_t bpp=0,planes=0;
            if (!readU32(s, format) || !readU32(s, w) || !readU32(s, h) || !readU32(s, widthBytes) ||
                !readU32(s, totalSize) || !readU32(s, compressedSize) || !readU16(s, bpp) || !readU16(s, planes)) {
                return DecodeStatus::IoError;
            }
            (void)widthBytes; (void)totalSize; (void)bpp; (void)planes;
            uint64_t payload = s.tell();
            uint64_t avail = dataEnd - payload;
            if (format == 1 && w > 0 && h > 0 && compressedSize > 0 && compressedSize <= avail && compressedSize <= 32u * 1024u * 1024u) {
                if (outW) *outW = w;
                if (outH) *outH = h;
                if (jpeg) {
                    jpeg->resize(compressedSize);
                    if (!s.read(jpeg->data(), compressedSize)) return DecodeStatus::IoError;
                }
                if (detail) *detail = id == 1036 ? "Thumbnail JPEG Photoshop 5+ (1036)" : "Thumbnail JPEG Photoshop 4 (1033)";
                return DecodeStatus::Ok;
            }
        }

        uint64_t paddedEnd = dataEnd + (dataSize & 1u);
        if (paddedEnd > end || !s.seek(paddedEnd)) return DecodeStatus::Invalid;
    }
    if (detail) *detail = "Sin thumbnail JPEG incrustado";
    return DecodeStatus::Unsupported;
}

bool decodePackBitsRow(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst, size_t expected) {
    dst.clear();
    dst.reserve(expected);
    size_t i = 0;
    while (i < src.size() && dst.size() < expected) {
        int8_t n = static_cast<int8_t>(src[i++]);
        if (n >= 0) {
            size_t count = static_cast<size_t>(n) + 1;
            if (count > src.size() - i || count > expected - dst.size()) return false;
            dst.insert(dst.end(), src.begin() + static_cast<std::ptrdiff_t>(i), src.begin() + static_cast<std::ptrdiff_t>(i + count));
            i += count;
        } else if (n >= -127) {
            size_t count = static_cast<size_t>(1 - n);
            if (i >= src.size() || count > expected - dst.size()) return false;
            uint8_t v = src[i++];
            dst.insert(dst.end(), count, v);
        } else {
            // -128 is a no-op
        }
    }
    return dst.size() == expected;
}

struct RowLayout {
    uint16_t compression{};
    uint64_t pixelDataStart{};
    std::vector<uint32_t> rleCounts;
    std::vector<uint64_t> rleOffsets;
};

DecodeStatus readRowLayout(ByteStream& s, const Sections& sec, RowLayout& layout, std::string* detail) {
    if (!s.seek(sec.imageDataStart)) return DecodeStatus::IoError;
    if (!readU16(s, layout.compression)) return DecodeStatus::IoError;
    if (layout.compression > 1) {
        if (detail) *detail = layout.compression == 2 ? "Composite ZIP no implementado en 1.1" :
                              layout.compression == 3 ? "Composite ZIP Prediction no implementado en 1.1" :
                              "Compresión composite desconocida";
        return DecodeStatus::Unsupported;
    }
    layout.pixelDataStart = s.tell();
    if (layout.compression == 0) return DecodeStatus::Ok;

    uint64_t rows64 = uint64_t(sec.h.channels) * sec.h.height;
    if (rows64 > 4'000'000ull) return DecodeStatus::TooLarge;
    size_t rows = static_cast<size_t>(rows64);
    layout.rleCounts.resize(rows);
    for (size_t i = 0; i < rows; ++i) {
        if (sec.h.version == 1) {
            uint16_t n = 0; if (!readU16(s, n)) return DecodeStatus::IoError; layout.rleCounts[i] = n;
        } else {
            uint32_t n = 0; if (!readU32(s, n)) return DecodeStatus::IoError; layout.rleCounts[i] = n;
        }
    }
    layout.pixelDataStart = s.tell();
    layout.rleOffsets.resize(rows);
    uint64_t off = layout.pixelDataStart;
    for (size_t i = 0; i < rows; ++i) {
        layout.rleOffsets[i] = off;
        uint64_t next = 0;
        if (!addSafe(off, layout.rleCounts[i], next) || next > s.size()) return DecodeStatus::Invalid;
        off = next;
    }
    return DecodeStatus::Ok;
}

uint8_t sampleTo8(const uint8_t* p, uint16_t depth) {
    return depth == 16 ? p[0] : p[0]; // PSD 16-bit is big-endian: high byte is sufficient for thumbnail.
}

uint8_t mul255(uint8_t a, uint8_t b) {
    return static_cast<uint8_t>((uint32_t(a) * uint32_t(b) + 127u) / 255u);
}

} // namespace

InspectResult Inspect(ByteStream& stream) {
    InspectResult r;
    Sections sec;
    std::string detail;
    r.status = locateSections(stream, sec, &detail);
    r.header = sec.h;
    r.detail = detail;
    if (r.status != DecodeStatus::Ok) return r;
    uint32_t w=0,h=0;
    std::string jpegDetail;
    auto j = findEmbeddedJpeg(stream, sec, nullptr, &w, &h, &jpegDetail);
    r.hasEmbeddedJpeg = (j == DecodeStatus::Ok);
    r.embeddedJpegWidth = w;
    r.embeddedJpegHeight = h;
    if (stream.seek(sec.imageDataStart)) {
        uint16_t compression = 0xFFFF;
        if (readU16(stream, compression)) r.compositeCompression = compression;
    }
    r.detail = r.hasEmbeddedJpeg ? jpegDetail : "PSD/PSB válido; " + jpegDetail;
    return r;
}

DecodeStatus ExtractEmbeddedJpeg(ByteStream& stream, std::vector<uint8_t>& jpeg,
                                 uint32_t& width, uint32_t& height, std::string* detail) {
    jpeg.clear(); width = height = 0;
    Sections sec;
    DecodeStatus st = locateSections(stream, sec, detail);
    if (st != DecodeStatus::Ok) return st;
    return findEmbeddedJpeg(stream, sec, &jpeg, &width, &height, detail);
}

DecodeStatus DecodeCompositeThumbnail(ByteStream& stream, uint32_t maxEdge,
                                      ImageBGRA& out, std::string* detail) {
    out = {};
    if (maxEdge == 0) maxEdge = 256;
    maxEdge = std::min<uint32_t>(maxEdge, 4096);

    Sections sec;
    DecodeStatus st = locateSections(stream, sec, detail);
    if (st != DecodeStatus::Ok) return st;
    const Header& h = sec.h;
    if (h.depth != 8 && h.depth != 16) {
        if (detail) *detail = "Fallback composite: solo 8/16 bits";
        return DecodeStatus::Unsupported;
    }
    if (!(h.colorMode == 1 || h.colorMode == 3 || h.colorMode == 4)) {
        if (detail) *detail = "Fallback composite: modo de color no soportado (solo Gray/RGB/CMYK)";
        return DecodeStatus::Unsupported;
    }
    uint16_t baseChannels = h.colorMode == 1 ? 1 : (h.colorMode == 3 ? 3 : 4);
    if (h.channels < baseChannels) {
        if (detail) *detail = "Número de canales insuficiente";
        return DecodeStatus::Invalid;
    }

    RowLayout layout;
    st = readRowLayout(stream, sec, layout, detail);
    if (st != DecodeStatus::Ok) return st;

    const uint32_t srcW = h.width, srcH = h.height;
    double scale = std::min(1.0, double(maxEdge) / double(std::max(srcW, srcH)));
    uint32_t dstW = std::max<uint32_t>(1, static_cast<uint32_t>(std::floor(srcW * scale + 0.5)));
    uint32_t dstH = std::max<uint32_t>(1, static_cast<uint32_t>(std::floor(srcH * scale + 0.5)));
    uint64_t pixCount = uint64_t(dstW) * dstH;
    if (pixCount > 16'777'216ull) return DecodeStatus::TooLarge; // 4096^2

    out.width = dstW;
    out.height = dstH;
    out.hasAlpha = h.channels > baseChannels;
    out.pixels.assign(static_cast<size_t>(pixCount) * 4, 255);

    const uint32_t bytesPerSample = h.depth / 8;
    uint64_t rowBytes64 = uint64_t(srcW) * bytesPerSample;
    if (rowBytes64 > 1ull << 31) return DecodeStatus::TooLarge;
    size_t rowBytes = static_cast<size_t>(rowBytes64);

    std::vector<std::vector<uint8_t>> channelRows(baseChannels + (out.hasAlpha ? 1 : 0));
    for (auto& v : channelRows) v.resize(rowBytes);
    std::vector<uint8_t> compressed, unpacked;

    auto readChannelRow = [&](uint16_t channel, uint32_t y, std::vector<uint8_t>& row) -> bool {
        if (layout.compression == 0) {
            uint64_t planeBytes = uint64_t(rowBytes) * srcH;
            uint64_t off = layout.pixelDataStart + uint64_t(channel) * planeBytes + uint64_t(y) * rowBytes;
            if (off > stream.size() || rowBytes > stream.size() - off || !stream.seek(off)) return false;
            return stream.read(row.data(), rowBytes);
        }
        size_t idx = static_cast<size_t>(uint64_t(channel) * srcH + y);
        if (idx >= layout.rleCounts.size()) return false;
        uint32_t count = layout.rleCounts[idx];
        compressed.resize(count);
        if (!stream.seek(layout.rleOffsets[idx]) || (count && !stream.read(compressed.data(), count))) return false;
        if (!decodePackBitsRow(compressed, unpacked, rowBytes)) return false;
        std::copy(unpacked.begin(), unpacked.end(), row.begin());
        return true;
    };

    for (uint32_t dy = 0; dy < dstH; ++dy) {
        uint32_t sy = std::min(srcH - 1, static_cast<uint32_t>((uint64_t(dy) * srcH) / dstH));
        uint16_t needed = static_cast<uint16_t>(baseChannels + (out.hasAlpha ? 1 : 0));
        for (uint16_t c = 0; c < needed; ++c) {
            if (!readChannelRow(c, sy, channelRows[c])) {
                if (detail) *detail = "No se pudo leer/descomprimir una fila composite";
                return DecodeStatus::IoError;
            }
        }
        for (uint32_t dx = 0; dx < dstW; ++dx) {
            uint32_t sx = std::min(srcW - 1, static_cast<uint32_t>((uint64_t(dx) * srcW) / dstW));
            size_t sp = static_cast<size_t>(sx) * bytesPerSample;
            uint8_t r=0,g=0,b=0,a=255;
            if (h.colorMode == 1) {
                r = g = b = sampleTo8(&channelRows[0][sp], h.depth);
                if (out.hasAlpha) a = sampleTo8(&channelRows[1][sp], h.depth);
            } else if (h.colorMode == 3) {
                r = sampleTo8(&channelRows[0][sp], h.depth);
                g = sampleTo8(&channelRows[1][sp], h.depth);
                b = sampleTo8(&channelRows[2][sp], h.depth);
                if (out.hasAlpha) a = sampleTo8(&channelRows[3][sp], h.depth);
            } else { // CMYK: practical thumbnail conversion, not color-managed proofing.
                uint8_t c = sampleTo8(&channelRows[0][sp], h.depth);
                uint8_t m = sampleTo8(&channelRows[1][sp], h.depth);
                uint8_t yv = sampleTo8(&channelRows[2][sp], h.depth);
                uint8_t k = sampleTo8(&channelRows[3][sp], h.depth);
                // PSD composite CMYK samples are commonly stored inverted relative to intuitive ink values.
                // Treat them as coverage values after inversion for a stable visual thumbnail.
                uint8_t C = static_cast<uint8_t>(255 - c), M = static_cast<uint8_t>(255 - m),
                        Y = static_cast<uint8_t>(255 - yv), K = static_cast<uint8_t>(255 - k);
                r = mul255(static_cast<uint8_t>(255 - C), static_cast<uint8_t>(255 - K));
                g = mul255(static_cast<uint8_t>(255 - M), static_cast<uint8_t>(255 - K));
                b = mul255(static_cast<uint8_t>(255 - Y), static_cast<uint8_t>(255 - K));
                if (out.hasAlpha) a = sampleTo8(&channelRows[4][sp], h.depth);
            }
            size_t dp = (static_cast<size_t>(dy) * dstW + dx) * 4;
            out.pixels[dp + 0] = b;
            out.pixels[dp + 1] = g;
            out.pixels[dp + 2] = r;
            out.pixels[dp + 3] = a;
        }
    }

    if (detail) *detail = layout.compression == 0 ? "Composite RAW decodificado" : "Composite RLE decodificado";
    return DecodeStatus::Ok;
}

const char* StatusText(DecodeStatus s) {
    switch (s) {
        case DecodeStatus::Ok: return "Ok";
        case DecodeStatus::Unsupported: return "Unsupported";
        case DecodeStatus::Invalid: return "Invalid";
        case DecodeStatus::IoError: return "IoError";
        case DecodeStatus::TooLarge: return "TooLarge";
    }
    return "Unknown";
}

} // namespace psdx
