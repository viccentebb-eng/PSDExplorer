#pragma once
#include "PsdCore.h"
#include <windows.h>
#include <cstdint>
#include <vector>

HRESULT DecodeJpegToHBitmap(const std::vector<uint8_t>& jpeg, UINT maxEdge, HBITMAP* bitmap,
                            UINT* outWidth = nullptr, UINT* outHeight = nullptr);
HRESULT ImageToHBitmap(const psdx::ImageBGRA& image, HBITMAP* bitmap);
