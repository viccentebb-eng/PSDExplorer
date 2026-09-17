#include "ComStreamAdapter.h"
#include <algorithm>

ComStreamAdapter::ComStreamAdapter(IStream* stream) : stream_(stream) {
    if (!stream_) return;
    LARGE_INTEGER zero{};
    ULARGE_INTEGER now{};
    if (SUCCEEDED(stream_->Seek(zero, STREAM_SEEK_CUR, &now))) pos_ = now.QuadPart;

    STATSTG st{};
    if (SUCCEEDED(stream_->Stat(&st, STATFLAG_NONAME))) {
        size_ = static_cast<uint64_t>(st.cbSize.QuadPart);
        validSize_ = true;
    } else {
        LARGE_INTEGER endMove{};
        ULARGE_INTEGER end{};
        if (SUCCEEDED(stream_->Seek(endMove, STREAM_SEEK_END, &end))) {
            size_ = end.QuadPart;
            validSize_ = true;
            LARGE_INTEGER restore{}; restore.QuadPart = static_cast<LONGLONG>(pos_);
            stream_->Seek(restore, STREAM_SEEK_SET, nullptr);
        }
    }
}

bool ComStreamAdapter::seek(uint64_t absolute) {
    if (!stream_ || !validSize_ || absolute > size_) return false;
    LARGE_INTEGER li{}; li.QuadPart = static_cast<LONGLONG>(absolute);
    ULARGE_INTEGER out{};
    if (FAILED(stream_->Seek(li, STREAM_SEEK_SET, &out))) return false;
    pos_ = out.QuadPart;
    return pos_ == absolute;
}

bool ComStreamAdapter::read(void* dst, size_t bytes) {
    if (!stream_ || bytes > size_ - pos_) return false;
    auto* p = static_cast<unsigned char*>(dst);
    size_t left = bytes;
    while (left) {
        ULONG chunk = static_cast<ULONG>(std::min<size_t>(left, 1u << 20));
        ULONG got = 0;
        HRESULT hr = stream_->Read(p, chunk, &got);
        if (FAILED(hr) || got != chunk) return false;
        p += got; left -= got; pos_ += got;
    }
    return true;
}
