#include "higanbana/graphics/dx12/dx12subsystem.hpp"
#if defined(HIGANBANA_PLATFORM_WINDOWS)
#include "higanbana/graphics/common/gpudevice.hpp"
#include "higanbana/graphics/common/graphicssurface.hpp"
#include "higanbana/graphics/definitions.hpp"
#include <higanbana/core/Platform/Window.hpp>
#include <higanbana/core/global_debug.hpp>

namespace higanbana
{
  namespace backend
  {
    DX12Subsystem::DX12Subsystem(const char*, unsigned, const char*, unsigned, bool debug)
      : m_debug(debug)
    {
#ifdef HIGANBANA_GRAPHICS_VALIDATION_LAYER
      m_debug = true;
#endif
      CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(pFactory.GetAddressOf()));
    }

    std::string DX12Subsystem::gfxApi()
    {
      return "DX12";
    }

    vector<GpuInfo> DX12Subsystem::availableGpus()
    {
      vAdapters.clear();
      UINT i = 0;
      ComPtr<IDXGIAdapter1> pAdapter;
      while (pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND)
      {
        DXGI_ADAPTER_DESC desc;
        pAdapter->GetDesc(&desc);
        HRESULT hr = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
        if (SUCCEEDED(hr))
        {
          char ch[128];
          char DefChar = ' ';
          WideCharToMultiByte(CP_ACP, 0, desc.Description, -1, ch, 128, &DefChar, NULL);

          GpuInfo info{};
          info.name = std::string(ch);

          if (info.name.find("Microsoft Basic Render Driver") != std::string::npos)
          {
            i++;
            continue;
          }

          ComPtr<IDXGIAdapter3> wanted;
          hr = pAdapter->QueryInterface(IID_PPV_ARGS(&wanted));

          if (SUCCEEDED(hr))
          {
            vAdapters.push_back(wanted);
          }
          else
          {
            i++;
            continue;
          }
          info.vendor = VendorID::Unknown;
          info.type = DeviceType::Unknown;
          if (desc.VendorId == 4098)
          {
            info.vendor = VendorID::Amd;
            info.type = DeviceType::DiscreteGpu;
          }
          else if (desc.VendorId == 4318)
          {
            info.vendor = VendorID::Nvidia;
            info.type = DeviceType::DiscreteGpu;
          }
          else if (desc.VendorId == 32902)
          {
            info.vendor = VendorID::Intel;
            info.type = DeviceType::IntegratedGpu;
          }
          info.id = static_cast<int>(vAdapters.size()) - 1;
          info.memory = static_cast<int64_t>(desc.DedicatedVideoMemory);
          info.canPresent = true;

          infos.push_back(info);
        }
        ++i;
      }

      {
        GpuInfo info{};
        info.name = "Warp";
        info.id = static_cast<int>(vAdapters.size());
        info.canPresent = true;
        info.type = DeviceType::Cpu;
        info.vendor = VendorID::Unknown;
        infos.push_back(info);
      }

      return infos;
    }

    std::shared_ptr<prototypes::DeviceImpl> DX12Subsystem::createGpuDevice(FileSystem& fs, GpuInfo gpu)
    {
      if (m_debug)
      {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
          debugController->EnableDebugLayer();
#if defined(HIGANBANA_GRAPHICS_GPUBASED_VALIDATION)
          ComPtr<ID3D12Debug1> debugController1;
          debugController.As(&debugController1);
          debugController1->SetEnableGPUBasedValidation(true);
          debugController1->SetEnableSynchronizedCommandQueueValidation(true);
#endif
          // TODO: Figure out which Windows SDK this needs.
          /*
          ComPtr<ID3D12DeviceRemovedExtendedDataSettings> pDredSettings;
          HIGANBANA_CHECK_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&pDredSettings)));

          // Turn on auto-breadcrumbs and page fault reporting.
          pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
          pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
          */
        }
      }

      ID3D12Device* device;
      D3D_FEATURE_LEVEL createdLevel;
      constexpr D3D_FEATURE_LEVEL tryToEnable[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
      };

      // dxil needs this... still(?)
      UUID experimentalFeatures[] = { D3D12ExperimentalShaderModels };
      D3D12EnableExperimentalFeatures(1, experimentalFeatures, NULL, NULL);

      if (gpu.id == static_cast<int>(vAdapters.size()))
      {
        ComPtr<IDXGIAdapter> pAdapter;
        pFactory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter));
        HRESULT hr = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr))
        {
          device->SetName(L"Warp");
        }
        else
        {
          HIGAN_ASSERT(false, ":( no warp");
        }
      }
      else
      {
        int i = 0;
        HRESULT hr;
        while (i < 4)
        {
          hr = D3D12CreateDevice(vAdapters[gpu.id].Get(), tryToEnable[i], IID_PPV_ARGS(&device));
          if (SUCCEEDED(hr))
          {
            createdLevel = tryToEnable[i];
            device->SetName(L"HwDevice");
            break;
          }
          ++i;
        }
        HIGAN_ASSERT(SUCCEEDED(hr), "oh no, we didn't get a device");
      }

      if (m_debug)
      {
        {
          ComPtr<ID3D12InfoQueue> infoQueue;
          device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf()));

          infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
          infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
          //infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

          D3D12_MESSAGE_SEVERITY disabledSeverities[] = {
            D3D12_MESSAGE_SEVERITY_INFO,
          };
          D3D12_MESSAGE_ID disabledMessages[] = {
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED,
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES
          };

          D3D12_INFO_QUEUE_FILTER filter = {};
          filter.DenyList.NumSeverities = ArrayLength(disabledSeverities);
          filter.DenyList.NumIDs = ArrayLength(disabledMessages);
          filter.DenyList.pSeverityList = disabledSeverities;
          filter.DenyList.pIDList = disabledMessages;
          infoQueue->PushStorageFilter(&filter);
        }
      }

      std::shared_ptr<DX12Device> impl = std::shared_ptr<DX12Device>(new DX12Device(gpu, device, pFactory, fs, m_debug),
        [device](DX12Device* ptr)
      {
        device->Release();
        delete ptr;
      });

      return impl;
    }

    GraphicsSurface DX12Subsystem::createSurface(Window& window)
    {
      return GraphicsSurface(std::make_shared<DX12GraphicsSurface>(window.getInternalWindow().getHWND(), window.getInternalWindow().getHInstance()));
    }
  }
}
#endif