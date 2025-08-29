#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <type_traits>
#include <utility>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <source_location>
#include <concepts>
#include <thread>
#include <string_view>
#include <queue>
#include <deque>
#include <ranges>
#include <span>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <vulkan/vulkan.h>
#ifdef ONYX_ENABLE_IMGUI
#    include <imgui.h>
#    include <backends/imgui_impl_glfw.h>
#    include <backends/imgui_impl_vulkan.h>
#endif
#ifdef ONYX_ENABLE_IMPLOT
#    include <implot.h>
#endif
#include "vkit/vulkan/allocator.hpp"
#include "onyx/core/glm.hpp"
