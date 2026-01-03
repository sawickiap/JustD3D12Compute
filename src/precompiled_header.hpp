// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Must include DirectX 12 Agility SDK before Windows.h!
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dx12/d3dx12.h>
#include <dxgi1_6.h>

#include <Windows.h>
#include <atlbase.h>

#include <dxcapi.h>
#include <d3d12shader.h>

#include <array>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <atomic>
#include <mutex>
#include <iterator>
#include <type_traits>
#include <filesystem>

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cstdarg>
#include <cinttypes>
