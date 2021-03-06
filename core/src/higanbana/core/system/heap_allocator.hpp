#pragma once
#include <optional>
#include <algorithm>

#include "higanbana/core/datastructures/vector.hpp"
#include "higanbana/core/global_debug.hpp"

namespace higanbana
{
struct RangeBlock {
  uint64_t offset;
  uint64_t size;
  operator bool() const { return size != 0; }
};
class HeapAllocator {
  struct TLSFSizeClass {
    size_t sizeClass;
    uint64_t slBitmap;
    vector<vector<RangeBlock>> freeBlocks;
  };

  struct TLSFControl {
    uint64_t flBitmap;
    vector<TLSFSizeClass> sizeclasses;
  };

  RangeBlock m_baseBlock;
  uint64_t fli;        // first level index
  uint64_t sli;        // second level index, typically 5
  unsigned sli_count;  // second level index, typically 5
  uint64_t mbs;        // minimum block size
  uint64_t min_fli;
  TLSFControl control;
  size_t m_usedSize;

  inline int fls(uint64_t size) const noexcept {
    if (size == 0)
      return -1;
#ifdef HIGANBANA_PLATFORM_WINDOWS
    unsigned long index;
    return _BitScanReverse64(&index, size) ? index : -1;
#else
    return  63 - __builtin_clzll(size);
#endif
  }

  inline int ffs(uint64_t size) const noexcept {
    if (size == 0)
      return -1;
#ifdef HIGANBANA_PLATFORM_WINDOWS
    unsigned long index;
    return _BitScanForward64(&index, size) ? index : -1;
#else
    return __builtin_ctzll(size);
#endif
  }

  inline void mapping(size_t size, int& fl, int& sl) noexcept {
    fl = fls(size);
    sl = ((size ^ (1ull << fl)) >> (fl - sli));
    fl = first_level_index(fl);
    // printf("%zu -> fl %u sl %u\n", size, fl, sl);
  }

  inline int first_level_index(int fli) noexcept {
    if (fli < min_fli )
      return 0;
    return fli - min_fli;
  }

  inline void initialize() noexcept {
    fli = fls(m_baseBlock.size);
    mbs = std::min(m_baseBlock.size, mbs);
    min_fli = fls(mbs);
    control.flBitmap = 0;
    for (int i = min_fli; i <= fli; ++i) {
      size_t sizeClass = 1 << i;
      vector<vector<RangeBlock>> vectors;
      for (int k = 0; k < sli_count; ++k) {
        vectors.push_back(vector<RangeBlock>());
      }
      control.sizeclasses.push_back(TLSFSizeClass{sizeClass, 0, vectors});
    }
  }

  inline void remove_bit(uint64_t& value, int index) noexcept { value = value ^ (1 << index); }

  inline void set_bit(uint64_t& value, int index) noexcept { value |= (1 << index); }

  inline void insert(RangeBlock block, int fl, int sl) noexcept {
    HIGAN_ASSERT(fl < control.sizeclasses.size() && fl >= 0, "fl should be valid, was fl:%d, sizeclasses %zu", fl, control.sizeclasses.size());
    auto& sizeClass = control.sizeclasses[fl];
    HIGAN_ASSERT(sl < sizeClass.freeBlocks.size() && sl >= 0, "sl should be valid, was fl:%d sl:%d freeBlocks %zu", fl, sl, sizeClass.freeBlocks.size());
    auto& secondLv = sizeClass.freeBlocks[sl];
    secondLv.push_back(block);
    set_bit(sizeClass.slBitmap, sl);
    set_bit(control.flBitmap, fl);
  }

  inline int findLargeEnoughBlockWithinFree(const vector<RangeBlock>& freeblocks, size_t size) {
    for (int i = 0; i < freeblocks.size(); ++i)
      if (freeblocks[i].size > size)
        return i;
    return 0;
  }

  inline RangeBlock search_suitable_block(size_t size, int fl, int sl) noexcept {
    // first step, assume we got something at fl / sl location
    if (control.sizeclasses.size() <= fl)
      return {};
    auto& secondLevel = control.sizeclasses[fl];
    auto& freeblocks = secondLevel.freeBlocks[sl];
    auto candidate = findLargeEnoughBlockWithinFree(freeblocks, size);
    if (!freeblocks.empty() && freeblocks[candidate].size >= size) {
      auto block = freeblocks[candidate];
      freeblocks.erase(freeblocks.begin() + candidate);
      // remove bitmap bit
      if (freeblocks.empty()) remove_bit(secondLevel.slBitmap, sl);
      if (secondLevel.slBitmap == 0) remove_bit(control.flBitmap, fl);
      return block;
    }
    else {
      sl = ffs(secondLevel.slBitmap);
      candidate = 0;
      if (sl >= 0)
        candidate = findLargeEnoughBlockWithinFree(secondLevel.freeBlocks[sl], size);
      if (sl >= 0 && secondLevel.freeBlocks[sl][candidate].size >= size) { // somethings still in this size class
        auto& freeblocks2 = secondLevel.freeBlocks[sl];
        auto block = freeblocks2[candidate];
        freeblocks2.erase(freeblocks2.begin() + candidate);
        // remove bitmap bit
        if (freeblocks2.empty())
          remove_bit(secondLevel.slBitmap, sl);
        if (secondLevel.slBitmap == 0) remove_bit(control.flBitmap, fl);
        return block;
      }
      else {
        // second step, scan bitmaps for empty slots
        // create mask to ignore first bits, could be wrong
        auto mask = ~((1 << (fl+1)) - 1);
        auto fl2 = ffs(control.flBitmap & mask);
        if (fl2 >= 0) {
          auto& secondLevel2 = control.sizeclasses[fl2];
          HIGAN_ASSERT(secondLevel2.sizeClass >= size && secondLevel2.slBitmap != 0, "bitmap expected to have something");
          auto sl2 = ffs(secondLevel2.slBitmap);
          HIGAN_ASSERT(!secondLevel2.freeBlocks[sl2].empty(), "freeblocks expected to contain something");
          candidate = (sl2 >= 0) ? findLargeEnoughBlockWithinFree(secondLevel2.freeBlocks[sl2], size) : 0;
          if (sl2 >= 0 && secondLevel2.freeBlocks[sl2][candidate].size >= size) {
            auto& freeblocks3 = secondLevel2.freeBlocks[sl2];
            auto block = freeblocks3[candidate];
            freeblocks3.erase(freeblocks3.begin() + candidate);
            // remove bitmap bit
            if (freeblocks3.empty())
              remove_bit(secondLevel2.slBitmap, sl2);
            if (secondLevel2.slBitmap == 0) remove_bit(control.flBitmap, fl2);
            return block;
          }
        }
      }
    }

    return {};
  }

  inline RangeBlock split(RangeBlock& block, size_t size) noexcept {
    auto new_size = block.size - size;
    RangeBlock new_block = {block.offset + size, new_size};
    block.size = size;
    return new_block;
  }

  inline RangeBlock merge(RangeBlock block) noexcept {
    auto otf = block.offset;
    auto otf2 = block.offset + block.size;
    // oh no, nail in the coffin. BRUTEFORCE, we got not boundary tagging
    // possible sped up by using bitmaps to avoid checking empty vectors
    auto fl = 0;

    // scan through only the memory where blocks reside using bitfields
    auto flBM = control.flBitmap;
    while (flBM != 0) {
      auto fl = ffs(flBM);
      remove_bit(flBM, fl);

      auto& secondLevel = control.sizeclasses[fl];
      // use the bitmap to only check relevant vectors
      auto slBM = secondLevel.slBitmap;
      while (slBM != 0) {
        auto sl = ffs(slBM);
        remove_bit(slBM, sl);

        auto& freeBlocks = secondLevel.freeBlocks[sl];

        auto iter = std::find_if(
            freeBlocks.begin(), freeBlocks.end(), [otf, otf2](RangeBlock b) {
              return (b.offset + b.size == otf) || (b.offset == otf2);
            });
        if (iter != freeBlocks.end()) {
          auto rb = *iter;
          freeBlocks.erase(iter);
          if (freeBlocks.empty()) remove_bit(secondLevel.slBitmap, sl);
          if (secondLevel.slBitmap == 0) remove_bit(control.flBitmap, fl);

          if (rb.offset + rb.size == otf) {
            rb.size += block.size;
            return rb;
          } else if (rb.offset == otf2) {
            block.size += rb.size;
            return block;
          }
        }
      }
    }
    return block;
  }

 public:
  HeapAllocator();
  HeapAllocator(RangeBlock initialBlock, size_t minimumBlockSize = 16, int sli = 3);
  HeapAllocator(size_t size, size_t minimumBlockSize = 16, int sli = 3);
  std::optional<RangeBlock> allocate(size_t size, size_t alignment = 1) noexcept;
  void free(RangeBlock block) noexcept;
  void resize(size_t size) noexcept;
  size_t findLargestAllocation() const noexcept;

  inline size_t size() const noexcept {
    return m_baseBlock.size - m_usedSize;
  }
  inline size_t max_size() const noexcept {
    return m_baseBlock.size;
  }
  inline size_t size_allocated() const noexcept {
    return m_usedSize;
  }
};
}