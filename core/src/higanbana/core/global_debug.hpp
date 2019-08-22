#pragma once

#include "higanbana/core/system/fazmesg.hpp"
#include "higanbana/core/platform/definitions.hpp"

#if defined(HIGANBANA_PLATFORM_WINDOWS)
#include <Windows.h>
#include <comdef.h>
#endif

#include <string.h>
#include <cstdio>
#include <cstdarg>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <string>
#include <sstream>
#include <cassert>

#include <locale>
#include <codecvt>

#define HIGAN_DEBUG_LOG(msg, ...) log_adv(__FILE__, __LINE__, msg, ##__VA_ARGS__)
#define HIGAN_LOG(msg, ...) log_sys("Output", msg, ##__VA_ARGS__)
#define HIGAN_LOGi(msg, ...) log_im(msg, ##__VA_ARGS__)
#define HIGAN_ILOG(prefix, msg, ...) log_imSys(prefix, msg, ##__VA_ARGS__)
#define HIGAN_SLOG(prefix, msg, ...) log_sys(prefix, msg, ##__VA_ARGS__)
#define HIGAN_LOG_UNFORMATTED(msg, ...) log_def(msg, ##__VA_ARGS__);

#if 0 //defined(DEBUG)
#if defined(HIGANBANA_PLATFORM_WINDOWS)
#define HIGAN_ERROR(msg, ...) \
  log_immideateAssert(__FILE__, __LINE__, msg, ##__VA_ARGS__); \
  if (IsDebuggerPresent()) \
    __debugbreak(); \
  abort();

#define HIGAN_ASSERT(cond, msg, ...)\
        do \
        { \
          if (!(cond)) \
          { \
            log_immideateAssert(__FILE__, __LINE__, msg, ##__VA_ARGS__); \
            if (IsDebuggerPresent()) \
              __debugbreak(); \
            abort();\
          } \
      } while (0)

#define HIGANBANA_CHECK_HR(hr) \
        do \
        { \
        if (FAILED(hr)) \
          { \
            _com_error err(hr); \
            LPCTSTR errMsg = err.ErrorMessage(); \
            std::string _msg = ws2s(errMsg); \
            log_immideateAssert(__FILE__, __LINE__, "[SYSTEM/fail]: HRESULT: \"%s\"", _msg.c_str()); \
            if (IsDebuggerPresent()) \
              __debugbreak(); \
            abort();\
          } \
      } while (0)
#else
#define HIGAN_ERROR(msg, ...) \
  log_immideate(__FILE__, __LINE__, msg, ##__VA_ARGS__); \
  abort();

#define HIGAN_ASSERT(cond, msg, ...)\
        do \
        { \
        if (!(cond)) \
          { \
            log_immideate(__FILE__, __LINE__, msg, ##__VA_ARGS__); \
            abort();\
          } \
      } while (0)
#endif
#else
#ifdef _MSC_VER
#define HIGAN_ERROR(msg, ...) __assume(false);
#define HIGAN_ASSERT(cond, msg, ...) __assume(cond);
#define HIGANBANA_CHECK_HR(hr) do { hr; } while (0)
#else
#define HIGAN_ERROR(msg, ...) __builtin_unreachable();
#define HIGAN_ASSERT(cond, msg, ...) do { if (!(cond)) { __builtin_unreachable(); } } while(0)
#define HIGANBANA_CHECK_HR(hr) do { hr; } while (0)
#endif
#endif

#ifdef _MSC_VER
#define _snprintf c99_snprintf
#define _vsnprintf c99_vsnprintf
inline int c99_vsnprintf(char* str, size_t size, const char* format, va_list ap);
inline int c99_snprintf(char* str, size_t size, const char* format, ...);

#endif // _MSC_VER

void log_adv(const char *fn, int ln, const char* format, ...);
void log_def(const char* format, ...);
void log_im(const char* format, ...);
void log_imSys(const char* prefix, const char* format, ...);
void log_sys(const char* prefix, const char* format, ...);

void log_immideate(const char *fn, int ln, const char* format, ...);
void log_immideateAssert(const char *fn, int ln, const char* format, ...);

std::string _log_getvalue(std::string type, float& value);
std::string _log_getvalue(std::string type, int64_t& value);
std::string _log_str(const char* s);

std::wstring s2ws(const std::string& str);
std::string ws2s(const std::wstring& wstr);