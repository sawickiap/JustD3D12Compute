#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Must include DirectX 12 Agility SDK before Windows.h!
#include <d3d12.h>
#include <d3dx12\d3dx12.h>
#include <dxgi1_6.h>

#include <Windows.h>
#include <atlbase.h>

#include "c:\Libraries\_Microsoft\dxc_2025_07_14\inc\dxcapi.h"
#include "c:\Libraries\_Microsoft\dxc_2025_07_14\inc\d3d12shader.h"

#include <array>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <atomic>
#include <iterator>
#include <type_traits>

#include <cassert>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstddef>

#define CHECK_HR(expr) do { HRESULT hr__ = (expr); if(hr__ != S_OK) __debugbreak(); } while(false)
