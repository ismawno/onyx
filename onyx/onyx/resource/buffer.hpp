#pragma once

#ifndef ONYX_INDEX_TYPE
#    define ONYX_INDEX_TYPE TKit::Alias::u32
#endif

#ifndef ONYX_BUFFER_INITIAL_CAPACITY
#    define ONYX_BUFFER_INITIAL_CAPACITY 4
#endif

#include "vkit/resource/device_buffer.hpp"

namespace Onyx
{
enum DeviceBufferFlags : VKit::DeviceBufferFlags
{
    Buffer_DeviceVertex = VKit::DeviceBufferFlag_Vertex | VKit::DeviceBufferFlag_DeviceLocal,
    Buffer_DeviceIndex = VKit::DeviceBufferFlag_Index | VKit::DeviceBufferFlag_DeviceLocal,
    Buffer_DeviceStorage =
        VKit::DeviceBufferFlag_Storage | VKit::DeviceBufferFlag_DeviceLocal | VKit::DeviceBufferFlag_Source,
    Buffer_Staging =
        VKit::DeviceBufferFlag_Staging | VKit::DeviceBufferFlag_HostMapped | VKit::DeviceBufferFlag_Destination,
    Buffer_HostVertex = VKit::DeviceBufferFlag_Vertex | VKit::DeviceBufferFlag_HostMapped,
    Buffer_HostIndex = VKit::DeviceBufferFlag_Index | VKit::DeviceBufferFlag_HostMapped,
};

using Index = ONYX_INDEX_TYPE;

using DeviceLocalBuffer = VKit::DeviceBuffer;
using HostVisibleBuffer = VKit::DeviceBuffer;
} // namespace Onyx
