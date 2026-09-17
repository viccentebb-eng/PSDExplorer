#pragma once
#include "PsdCore.h"
#include <objidl.h>

class ComStreamAdapter final : public psdx::ByteStream {
public:
    explicit ComStreamAdapter(IStream* stream);
    bool valid() const { return stream_ != nullptr && validSize_; }
    bool seek(uint64_t absolute) override;
    uint64_t tell() const override { return pos_; }
    uint64_t size() const override { return size_; }
    bool read(void* dst, size_t bytes) override;
private:
    IStream* stream_{}; // borrowed: owner keeps it alive
    uint64_t size_{};
    uint64_t pos_{};
    bool validSize_{};
};
