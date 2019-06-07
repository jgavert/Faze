#pragma once

#include "graphics/common/prototypes.hpp"
#include "graphics/common/pipeline_descriptor.hpp"

#include "core/datastructures/proxy.hpp"

#include "core/filesystem/filesystem.hpp"

namespace faze
{
  class ComputePipeline
  {
  public:
    std::shared_ptr<ResourceHandle> impl;
    std::shared_ptr<WatchFile> m_update;
    ComputePipelineDescriptor descriptor;

    ComputePipeline()
    {}

    ComputePipeline(std::shared_ptr<ResourceHandle> impl, ComputePipelineDescriptor desc)
      : impl(impl)
      , m_update(std::make_shared<WatchFile>())
      , descriptor(desc)
    {}
  };

  class GraphicsPipeline
  {
  public:
    std::shared_ptr<ResourceHandle> pipeline;
    GraphicsPipelineDescriptor descriptor;

    WatchFile vs;
    WatchFile ds;
    WatchFile hs;
    WatchFile gs;
    WatchFile ps;

    bool needsUpdating()
    {
      return vs.updated() || ps.updated() || ds.updated() || hs.updated() || gs.updated();
    }

    void updated()
    {
      vs.react();
      ds.react();
      hs.react();
      gs.react();
      ps.react();
    }

    GraphicsPipeline()
      : descriptor()
    {}

    GraphicsPipeline(std::shared_ptr<ResourceHandle> handle, GraphicsPipelineDescriptor desc)
      : pipeline(handle)
      , descriptor(desc)
    {}
  };
}