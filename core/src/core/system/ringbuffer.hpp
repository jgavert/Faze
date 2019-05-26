#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include <functional>
#include <cmath>

namespace faze
{

  template <class T, size_t ArraySize>
  class RingBuffer
  {
  public:
    RingBuffer() : m_ptr(0) {}
    ~RingBuffer() {}

    void push_back(T value)
    {
      moveptr();
      m_buffer[m_ptr] = std::move(value);
    }

    T& get() { return m_buffer[m_ptr]; }
    T* data() const { return m_buffer; }
    size_t size() const { return m_buffer.size(); }
    T& operator[](int index) { return m_buffer[getIndex(index)]; }
    const T& operator[](int index) const { return m_buffer[getIndex(index)]; }
    int start_ind() { return m_ptr; }
    int end_ind() { return m_ptr + static_cast<int>(m_buffer.size()); }
    void forEach(std::function< void(T&) > apply)
    {
      for (int i = start_ind(); i < end_ind(); i++)
      {
        apply(m_buffer[getIndex(i)]);
      }
    }
  private:
    void moveptr() { m_ptr = getIndex(m_ptr + 1); }
    int getIndex(int ind) { return ind % m_buffer.size(); }

    std::array<T, ArraySize> m_buffer;
    int m_ptr;
  };
}