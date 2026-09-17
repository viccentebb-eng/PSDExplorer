#pragma once
#include <guiddef.h>

// Unique CLSIDs generated for PSD Explorer.
// {2B3E1132-1AE9-4D32-9D73-E6026B26E2A1}
inline constexpr GUID CLSID_PSDExplorerThumbnail =
{0x2b3e1132,0x1ae9,0x4d32,{0x9d,0x73,0xe6,0x02,0x6b,0x26,0xe2,0xa1}};

// {A749EA92-17B2-49E5-9DB4-DF6B12888C77}
inline constexpr GUID CLSID_PSDExplorerPreview =
{0xa749ea92,0x17b2,0x49e5,{0x9d,0xb4,0xdf,0x6b,0x12,0x88,0x8c,0x77}};

inline constexpr wchar_t CLSID_PSDExplorerThumbnail_Str[] = L"{2B3E1132-1AE9-4D32-9D73-E6026B26E2A1}";
inline constexpr wchar_t CLSID_PSDExplorerPreview_Str[]   = L"{A749EA92-17B2-49E5-9DB4-DF6B12888C77}";
inline constexpr wchar_t PSDExplorer_PreviewHostAppId[]   = L"{6D2B5079-2F0B-48DD-AB7F-97CEC514D30B}";
inline constexpr wchar_t Shell_ThumbnailProviderGuid[]    = L"{E357FCCD-A995-4576-B01F-234630154E96}";
inline constexpr wchar_t Shell_PreviewHandlerGuid[]       = L"{8895B1C6-B41F-4C1C-A562-0D564250836F}";
