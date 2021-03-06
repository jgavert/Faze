#pragma once
#include "higanbana/core/system/memview.hpp"
#include "higanbana/core/datastructures/hashmap.hpp"
#include "higanbana/core/datastructures/vector.hpp"
#include <cstdio>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <optional>
#include <functional>

namespace higanbana
{
  class WatchFile
  {
    std::shared_ptr<std::atomic<bool>> m_updated = nullptr;

  public:

    WatchFile()
    {}

    WatchFile(std::shared_ptr<std::atomic<bool>> updated)
      : m_updated(updated)
    {}

    void react()
    {
      if (m_updated)
        *m_updated = false;
    }

    bool updated()
    {
      if (m_updated)
        return *m_updated;
      return false;
    }

    void update()
    {
      *m_updated = true;
    }

    bool empty()
    {
      return !m_updated;
    }
  };

  struct FileInfo
  {
    std::string nativePath;
    std::string withoutNative;
  };

  class MemoryBlob
  {
  private:
    std::vector<uint8_t> m_data;
  public:
    MemoryBlob();
    MemoryBlob(std::vector<uint8_t> data);
    uint8_t* data() noexcept;
    size_t size() const noexcept;
    const uint8_t* cdata() const noexcept;
  };

  class FileSystem
  {
  private:

    struct FileObj
    {
      size_t timeModified;
      vector<uint8_t> data;
    };
    //std::string m_resolvedFullPath;
    std::unordered_map<std::string, FileObj> m_files;
    std::unordered_set<std::string> m_dirs;

    // watch
    std::unordered_map<std::string, vector<std::string>> m_dependencies;
    std::unordered_map<std::string, WatchFile> m_watchedFiles;
    int rollingUpdate = 0;

    // 
    std::mutex m_lock;

    bool m_initialLoadComplete = false;

    std::string mountPointOSPath(std::string_view filepath);
    std::optional<std::unordered_map<std::string, std::string>> tryExtractMappings(std::string mappingjsonpath);
    std::unordered_map<std::string, std::string> m_mappings;
  public:
    enum class MappingMode {
      NoMapping,
      TryFirstMappingFile,
      UseMappingFile
    };

    FileSystem();
    FileSystem(std::string relativeOffset, MappingMode mode, const char* mappingFileName = "");
    void initialLoad();
    bool fileExists(std::string path);
    MemoryBlob readFile(std::string path);
    higanbana::MemView<const uint8_t> viewToFile(std::string path);
    void loadDirectoryContentsRecursive(std::string path);
    void getFilesWithinDir(std::string path, std::function<void(std::string&, MemView<const uint8_t>)> func);
    vector<std::string> getFilesWithinDir(std::string path);
    vector<std::string> recursiveList(std::string path, std::string filter);
    size_t timeModified(std::string path);
    bool tryLoadFile(std::string path);

    std::string directoryPath(std::string filePath);
    std::string mountPoint(std::string_view filepath);

    bool writeFile(std::string path, const uint8_t* ptr, size_t size);
    bool writeFile(std::string path, higanbana::MemView<const uint8_t> view);

    WatchFile watchFile(std::string path, std::optional<vector<std::string>> optionalTriggers = {});
    void addWatchDependency(std::string watchedPath, std::string notifyPath);
    void updateWatchedFiles();

    std::optional<std::string> resolveNativePath(std::string_view filepath);
  private:
    bool loadFileFromHDD(FileInfo& path, std::string mountpoint, size_t& size);
  };

}