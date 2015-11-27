#pragma once
#include "Platform/EntryPoint.hpp"
#include "Platform/Window.hpp"
#include "core/src/system/LBS.hpp"
#include "core/src/system/logger.hpp"
#include "core/src/entity/database.hpp"
#include "core/src/tests/TestWorks.hpp"

#include "Graphics/gfxApi.hpp"
#include <cstdio>
#include <iostream>
#include <d3d12shader.h>
#include <D3Dcompiler.h>

class AdvTests
{
private:
  static void runTestsForDevice(GpuDevice& dev, GpuCommandQueue& queue, Window& window, SwapChain& sc, ViewPort& port, GfxCommandList& gfx)
  {
    faze::TestWorks t("advtests");
    

    t.setAfterTest([&]()
    {
      auto fence = dev.createFence();
      queue.insertFence(fence);
      fence.wait(); // verify that queue has finished with current work
      if (!gfx.isClosed())
      {
        auto fence = dev.createFence();
        
        gfx.close(); // as expected, explicit close is kind of nice.
        // Much nicer to close and enforce errors if trying to add after that.
        // Although seems like you can still "put" stuff there.
      }
      dev.resetCmdList(gfx); // clean the resource use
    });

    t.addTest("Move data to upload heap and move to gpu memory", [&]()
    {
      auto srcdata = dev.createBufferSRV(
        Dimension(4096), Format<float>(), ResUsage::Upload);
      auto dstdata = dev.createBufferSRV(
        Dimension(4096), Format<float>(), ResUsage::Gpu);

      {
        auto tmp = srcdata.buffer().Map<float>();
        for (int i = 0;i < srcdata.buffer().size; ++i)
        {
          tmp.get()[i] = static_cast<float>(i);
        }
      }

      gfx.CopyResource(dstdata.buffer(), srcdata.buffer());
      gfx.close();
      queue.submit(gfx);
      GpuFence fence = dev.createFence();
      queue.insertFence(fence);
      fence.wait();

      return true;
    });
    t.addTest("Upload and readback the same data", [&]()
    {
      GpuFence fence = dev.createFence();

      auto srcdata = dev.createBufferSRV(Dimension(4096), Format<float>(), ResUsage::Upload);
      auto dstdata = dev.createBufferSRV(Dimension(4096), Format<float>(), ResUsage::Gpu);
      auto rbdata = dev.createBufferSRV(Dimension(4096), Format<float>(), ResUsage::Readback);

      {
        auto tmp = srcdata.buffer().Map<float>();
        for (int i = 0;i < srcdata.buffer().size; ++i)
        {
          tmp[i] = static_cast<float>(i);
        }
      }

      gfx.CopyResource(dstdata.buffer(), srcdata.buffer());
      gfx.CopyResource(rbdata.buffer(), dstdata.buffer());
      gfx.close();
      queue.submit(gfx);
      queue.insertFence(fence);
      fence.wait();
      {
        auto woot = rbdata.buffer().Map<float>();
        for (int i = 0;i < rbdata.buffer().size; ++i)
        {
          if (woot[i] != static_cast<float>(i))
          {
            return false;
          }
        }
        return true;
      }
    });


	t.addTest("Pipeline binding and modify data in compute (sub 50 lines!)", [&]()
	{
		GpuFence fence = dev.createFence();

    ComputePipeline pipeline = dev.createComputePipeline(ComputePipelineDescriptor().shader("compute_1.hlsl"));

    struct buf
    {
      int i;
      int k;
      int x;
      int y;
    };
		auto srcdata = dev.createBufferSRV(Dimension(1000 * 1000), Format<buf>(), ResUsage::Upload);
		auto dstdata = dev.createBufferSRV(Dimension(1000 * 1000), Format<buf>(), ResUsage::Gpu);
		auto completedata = dev.createBufferUAV(Dimension(1000 * 1000), Format<buf>(), ResUsage::Gpu);
		auto rbdata = dev.createBufferSRV(Dimension(1000 * 1000), Format<buf>(), ResUsage::Readback);

		{
			auto tmp = srcdata.buffer().Map<buf>();
			for (int i = 0;i < srcdata.buffer().size; ++i)
			{
        tmp[i].i = i;
        tmp[i].k = i;
			}
		}

		gfx.CopyResource(dstdata.buffer(), srcdata.buffer());
		auto bind = gfx.bind(pipeline);
		bind.SRV(0, dstdata);
		bind.UAV(0, completedata);
    size_t shaderGroup = 50;
    size_t inputSize = 1000 * 1000;
		gfx.Dispatch(bind, inputSize / shaderGroup, 1, 1);

		gfx.CopyResource(rbdata.buffer(), completedata.buffer());
    gfx.close();
    queue.submit(gfx);
		queue.insertFence(fence);
		fence.wait();
    auto mapd = rbdata.buffer().Map<buf>();
    for (int i = 0;i < rbdata.buffer().size;++i)
    {
      auto& obj = mapd[i];
      if (obj.i != i + i)
      {
        return false;
      }
      if (obj.k != i)
      {
        return false;
      }
    }
    return true;
	});


	t.addTest("render(?) for full 1 second in loop", [&]()
	{
    auto timepoint2 = std::chrono::high_resolution_clock::now();
    float i = 0;
    auto vec = faze::vec4({ i, 0.2f, 0.2f, 1.0f });
    auto limit = std::chrono::seconds(1).count();

    auto timeSince = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - timepoint2).count();
    while (timeSince < limit)
    {
      timeSince = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - timepoint2).count();
      window.simpleReadMessages();
      GpuFence fence = dev.createFence();
      gfx.setViewPort(port);
      UINT backBufferIndex = sc->GetCurrentBackBufferIndex();
      vec[0] += 0.02f;
      if (vec[0] > 1.0f)
        vec[0] = 0.f;
      gfx.ClearRenderTargetView(sc[backBufferIndex], vec);
      gfx.close();
      queue.submit(gfx);

      sc->Present(1, 0);
      queue.insertFence(fence);
      fence.wait();
      dev.resetCmdList(gfx);
    }
    dev.resetCmdList(gfx);
		return true;
	});
  
  t.addTest("render a triangle for full 1 second in loop", [&]()
  {
    struct buf
    {
      float pos[4];
    };
    auto srcdata = dev.createBufferSRV(Dimension(6), Format<buf>(), ResUsage::Upload);
    auto dstdata = dev.createBufferSRV(Dimension(6), Format<buf>(), ResUsage::Gpu);

    {
      auto tmp = srcdata.buffer().Map<buf>();
      float size = 0.5f;
      tmp[0] = { size, size, 0.f, 1.f };
      tmp[1] = { size, -size, 0.f, 1.f };
      tmp[2] = { -size, size, 0.f, 1.f };
      tmp[3] = { -size, size, 0.f, 1.f };
      tmp[4] = { size, -size, 0.f, 1.f };
      tmp[5] = { -size, -size, 0.f, 1.f };
    }

    gfx.CopyResource(dstdata.buffer(), srcdata.buffer());
    GpuFence fence = dev.createFence();
    gfx.close();
    queue.submit(gfx);
    queue.insertFence(fence);

    auto pipeline = dev.createGraphicsPipeline(GraphicsPipelineDescriptor()
      .PixelShader("pixel.hlsl")
      .VertexShader("vertex.hlsl")
      .setRenderTargetCount(1)
      .RTVFormat(0, FormatType::R8G8B8A8_UNORM)
      .DepthStencil(DepthStencilDescriptor().DepthEnable(false)));

    auto vec = faze::vec4({ 0.2f, 0.2f, 0.2f, 1.0f });

    auto limit = std::chrono::seconds(1).count();
    auto timepoint2 = std::chrono::high_resolution_clock::now();
    auto timeSince = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - timepoint2).count();
    while (timeSince < limit)
    {
      timeSince = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - timepoint2).count();
      if (window.simpleReadMessages())
        break;
      fence.wait();
      dev.resetCmdList(gfx);

      // Rendertarget
      gfx.setViewPort(port);
      vec[2] += 0.02f;
      if (vec[2] > 1.0f)
        vec[2] = 0.f;
      auto backBufferIndex = sc->GetCurrentBackBufferIndex();
      gfx.ClearRenderTargetView(sc[backBufferIndex], vec);
      gfx.setRenderTarget(sc[backBufferIndex]);
      // graphics begin
      auto bind = gfx.bind(pipeline);
      bind.SRV(0, dstdata);
      gfx.Draw(bind, 6);


      // submit all
      gfx.close();
      queue.submit(gfx);

      // present
      sc->Present(1, 0);
      queue.insertFence(fence);
    }
    fence.wait();

		return true;
	});

	t.addTest("Rotating triangle with shaders", [&]()
	{

		return false;
	});

    t.runTests();
  }

public:
  static void run(GpuDevice& dev, GpuCommandQueue& queue, Window& window, SwapChain& sc, ViewPort& port, GfxCommandList& gfx)
  {
    runTestsForDevice(dev, queue, window, sc, port, gfx);
  }
};