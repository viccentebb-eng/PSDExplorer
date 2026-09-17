#pragma once
#include <windows.h>

extern HMODULE g_module;
extern volatile long g_dllRef;

inline void DllAddRef() { InterlockedIncrement(&g_dllRef); }
inline void DllRelease() { InterlockedDecrement(&g_dllRef); }
