#pragma once
#include <string>
#include <array>
#include "higanbana/graphics/common/resources/graphics_api.hpp"

namespace higanbana
{
  class HeapDescriptor
  {
  public:
    struct descriptor
    {
      std::string     name = "UnnamedHeap";
      CPUPageProperty cpuPage = CPUPageProperty::Unknown;
      HeapType        heapType = HeapType::Default;
      int64_t         customType = 0;
      MemoryPoolHint  memPool = MemoryPoolHint::Device;
      uint64_t        sizeInBytes = 0;
      uint64_t        alignment = 64 * 1024;
      bool            onlyBuffers = false;
      bool            onlyNonRtDsTextures = false;
      bool            onlyRtDsTextures = false;
      bool            allowShared = false;
    } desc;

    HeapDescriptor()
    {
    }

    HeapDescriptor& setName(std::string name)
    {
      desc.name = name;
      return *this;
    }
    HeapDescriptor& setCPUPageProperty(CPUPageProperty prop)
    {
      desc.cpuPage = prop;
      return *this;
    }
    HeapDescriptor& setHeapType(HeapType type)
    {
      desc.heapType = type;
      return *this;
    }
    HeapDescriptor& setHeapTypeSpecific(int64_t type)
    {
      desc.customType = type;
      return *this;
    }
    HeapDescriptor& setMemoryPoolHint(MemoryPoolHint pool)
    {
      desc.memPool = pool;
      return *this;
    }
    HeapDescriptor& setSizeInBytes(uint64_t size)
    {
      desc.sizeInBytes = size;
      return *this;
    }
    HeapDescriptor& setHeapAlignment(int64_t alignment)
    {
      desc.alignment = alignment;
      return *this;
    }
    HeapDescriptor& restrictOnlyBuffers()
    {
      desc.onlyBuffers = true;
      return *this;
    }
    HeapDescriptor& restrictOnlyNonRtDsTextures()
    {
      desc.onlyNonRtDsTextures = true;
      return *this;
    }
    HeapDescriptor& restrictOnlyRtDsTextures()
    {
      desc.onlyRtDsTextures = true;
      return *this;
    }
  };
}