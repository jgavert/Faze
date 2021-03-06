#include "higanbana/core/platform/Window.hpp"

#include "higanbana/core/global_debug.hpp"
#include "higanbana/core/profiling/profiling.hpp"

#define DEBUG_LOG_RESIZE 0

namespace higanbana
{
#if defined(HIGANBANA_PLATFORM_WINDOWS)
  LRESULT CALLBACK Window::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    Window* me = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    switch (message)
    {
    case WM_DESTROY:
    {
      PostQuitMessage(0);
      return 0;
    }
    case WM_MENUCHAR:
    {
      /*
      if (LOWORD(wParam) & VK_RETURN)
          return MAKELRESULT(0, MNC_CLOSE);
      */
      return MAKELRESULT(0, MNC_CLOSE);
    }
    case WM_ENTERSIZEMOVE:
    {
      if (me)
      {
        //me->resizing = true;
      }
      break;
    }
    case WM_EXITSIZEMOVE:
    {
      if (me)
      {
        me->resizeEvent("Resized");
        me->resizing = false;
      }
      break;
    }
    case WM_KEYDOWN:
    {
      if (me)
      {
        me->keyDown(static_cast<int>(wParam));
      }
      break;
    }
    case WM_KEYUP:
    {
      if (me)
      {
        me->keyUp(static_cast<int>(wParam));
      }
      break;
    }

    case WM_MOUSEMOVE:
    {
      int xPos = GET_X_LPARAM(lParam);
      int yPos = GET_Y_LPARAM(lParam);
      if (me)
      {
        if (me->resetMousePosToMiddle)
        {
          auto xMiddle = me->m_width / 2;
          auto yMiddle = me->m_height / 2;

          POINT asd;
          asd.x = me->m_width / 2;
          asd.y = me->m_height / 2;
          ClientToScreen(me->m_window->getHWND(), &asd);
          
          SetCursorPos(asd.x, asd.y);

          me->m_mouse.m_pos = int2{ xMiddle - xPos, yMiddle - yPos };
        }
        else
        {
          me->m_mouse.m_pos = int2{ xPos, yPos };
        }
      }
      return 0;
      break;
    }
    case WM_SIZE:
    {
      if (me)
      {
        auto evnt = static_cast<int>(wParam);
        if (SIZE_MINIMIZED == evnt)
        {
          me->m_minimized = true;
          me->resizeEvent("Minimized");
        }

        if (!me->m_minimized)
        {
          me->m_resizeWidth = static_cast<int>(LOWORD(lParam));
          me->m_resizeHeight = static_cast<int>(HIWORD(lParam));
        }

        if (SIZE_MAXIMIZED == evnt)
        {
          me->m_minimized = false;
          me->resizeEvent("Maximized");
        }
        else if (SIZE_RESTORED == evnt && !me->resizing)
        {
          me->m_minimized = false;
          me->resizeEvent("Restored");
        }
        return 0;
      }
    }
    case WM_DPICHANGED:
    {
      if (me)
      {
        auto dpi = LOWORD(wParam);
        if (dpi == 0)
          break;
        me->setDpi(dpi);
        //RECT* lprcNewScale = reinterpret_cast<RECT*>(lParam);
        return 0;
      }
    }
    case WM_CHAR:
    {
      if (me)
      {
        wchar_t ch = static_cast<wchar_t>(wParam);
        if (ch == wParam)
        {
          me->m_inputs.charInputs().emplace_back(ch);
        }
        return 0;
      }
    }
    case WM_MOUSEWHEEL:
      if (me)
      {
        me->m_mouse.mouseWheel += GET_WHEEL_DELTA_WPARAM(wParam);
        return 0;
      }
      break;
    case WM_TOUCH:
    {
      if (me)
      {
        unsigned numInputs = static_cast<unsigned>(wParam);
        PTOUCHINPUT ti = new TOUCHINPUT[numInputs];

        if (ti)
        {
          me->prepareTouchInputs();

          if (GetTouchInputInfo((HTOUCHINPUT)lParam, numInputs, ti, sizeof(TOUCHINPUT)))
          {
            for (int i = 0; i < static_cast<INT>(numInputs); i++)
            {
              me->ptInput.x = TOUCH_COORD_TO_PIXEL(ti[i].x);
              me->ptInput.y = TOUCH_COORD_TO_PIXEL(ti[i].y);
              ScreenToClient(hWnd, &me->ptInput);
              me->updateTouchInput(ti[i].dwID, me->ptInput.x, me->ptInput.y);
            }
          }

          me->cleanTouchInputs();
          CloseTouchInputHandle((HTOUCHINPUT)lParam);

          return 0;
        }
      }
      break;
    }
    default:
      break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
  }

  LRESULT CALLBACK Window::LowLevelKeyboardHook(
    int nCode,
    WPARAM wParam,
    LPARAM lParam
  )
  {
    // process event
    KBDLLHOOKSTRUCT *pkbhs = (KBDLLHOOKSTRUCT *)lParam;
    BOOL bControlKeyDown = 0;
    switch (nCode)
    {
    case HC_ACTION:
    {
      // Check to see if the CTRL key is pressed
      bControlKeyDown = GetAsyncKeyState(VK_CONTROL) >> ((sizeof(SHORT) * 8) - 1);

      // Disable CTRL+ESC
      if (pkbhs->vkCode == VK_ESCAPE && bControlKeyDown)
        return 1;

      // Disable ALT+ESC
      if (pkbhs->vkCode == VK_ESCAPE && pkbhs->flags & LLKHF_ALTDOWN)
        return 1;

      // Disable ALT+ENTER
      if (pkbhs->vkCode == VK_RETURN && pkbhs->flags & LLKHF_ALTDOWN)
        return 1;

      break;
    }

    default:
      break;
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

#if DEBUG_LOG_RESIZE
  void Window::resizeEvent(const char* eventName)
#else
  void Window::resizeEvent(const char* )
#endif
  {
    if ((m_width != m_resizeWidth || m_height != m_resizeHeight) && !m_minimized)
    {
#if DEBUG_LOG_RESIZE
      HIGAN_SLOG("Window", "%s: %s %dx%d\n", m_name.c_str(), eventName, m_resizeWidth, m_resizeHeight);
#endif
      needToResize = true;

      m_width = m_resizeWidth;
      m_height = m_resizeHeight;
    }
  }

  void Window::setDpi(unsigned scale)
  {
    m_dpi = scale;
  }

  bool Window::hasResized()
  {
    return needToResize && !m_minimized;
  }

  void Window::resizeHandled()
  {
    needToResize = false;
  }

  void Window::captureMouse(bool val)
  {
    if (val)
    {
      while (ShowCursor(FALSE) >= 0);
      SetCapture(m_window->getHWND());
      resetMousePosToMiddle = true;

      POINT asd;
      asd.x = m_width / 2;
      asd.y = m_height / 2;
      ClientToScreen(m_window->getHWND(), &asd);

      SetCursorPos(asd.x, asd.y);
    }
    else
    {
      while (ShowCursor(TRUE) < 0);
      ReleaseCapture();
      resetMousePosToMiddle = false;
    }
  }

  void Window::keyDown(int)
  {
    //m_inputs.insert(key, 1, m_frame);
  }
  void Window::keyUp(int)
  {
    //m_inputs.insert(key, 0, m_frame);
  }

#endif

#if defined(HIGANBANA_PLATFORM_WINDOWS)
  // Initializes the window and shows it
  Window::Window(ProgramParams params, std::string windowname, int width, int height, int offsetX, int offsetY)
    : m_width(width)
    , m_height(height)
    , m_name(windowname)
  {
    HIGAN_CPU_BRACKET("create window");
    WNDCLASSEX wc{};
    HWND hWnd{};

    std::string classname = (windowname);

    std::wstring cnw = s2ws(classname);

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = params.m_hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    const wchar_t* cnwPtr = cnw.c_str();
    wc.lpszClassName = cnwPtr;
    RegisterClassEx(&wc);

    RECT wr = { offsetX, offsetY, offsetX + width, offsetY + height };
    AdjustWindowRect(&wr, m_baseWindowFlags, FALSE);

    auto result = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (result != S_OK)
    {
      HIGAN_SLOG("Window", "Tried to set dpi awareness.\n");
    }

    hWnd = CreateWindowEx(NULL, cnw.c_str(), cnw.c_str(), m_baseWindowFlags, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, params.m_hInstance, NULL);
    if (hWnd == NULL)
    {
      printf("wtf window null\n");
    }

    RegisterTouchWindow(hWnd, 0);

    SetWindowLongPtr(hWnd, GWLP_USERDATA, (size_t)this);

    std::string lol = (windowname + "class");
    m_window = std::make_shared<WindowInternal>(hWnd, params.m_hInstance, wc, params, lol);

    // clean messages away
    simpleReadMessages(-1);
  }
#else
  Window::Window(ProgramParams, std::string windowname, int width, int height, int, int)
    : m_width(width)
    , m_height(height)
    , m_name(windowname)
  {
  }
#endif

  bool Window::open()
  {
#if defined(HIGANBANA_PLATFORM_WINDOWS)
    ShowWindow(m_window->hWnd, m_window->m_params.m_nCmdShow);
    GetWindowRect(m_window->hWnd, &m_windowRect);
    m_windowVisible = true;
#endif
    return true;
  }

  void Window::title(std::string newString)
  {
#if defined(HIGANBANA_PLATFORM_WINDOWS)
    std::wstring cnw = s2ws(newString);
    SetWindowText(m_window->hWnd, cnw.c_str());
#endif
  }

  void Window::close()
  {
#if defined(HIGANBANA_PLATFORM_WINDOWS)
    CloseWindow(m_window->hWnd);
#endif
  }
#if defined(HIGANBANA_PLATFORM_WINDOWS)
  void Window::cursorHidden(bool enabled)
  {
    ShowCursor(enabled);
  }
#else
  void Window::cursorHidden(bool)
  {
  }
#endif

  void Window::toggleBorderlessFullscreen()
  {
#if defined(HIGANBANA_PLATFORM_WINDOWS)
    if (m_windowVisible)
    {
      RECT targetRect{};
      if (!m_borderlessFullscreen)
      {
        m_borderlessFullscreen = true;
        GetWindowRect(m_window->hWnd, &m_windowRect);
        m_baseWindowFlags = static_cast<int>(GetWindowLongPtr(m_window->hWnd, GWL_STYLE));
        SetWindowLongPtr(m_window->hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        HMONITOR monitor = MonitorFromWindow(m_window->hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info;
        info.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(monitor, &info);
        targetRect = info.rcMonitor;
        SetWindowPos(m_window->hWnd, nullptr, targetRect.left, targetRect.top, targetRect.right - targetRect.left, targetRect.bottom - targetRect.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
      }
      else
      {
        m_borderlessFullscreen = false;
        SetWindowLongPtr(m_window->hWnd, GWL_STYLE, m_baseWindowFlags);
        targetRect = m_windowRect;
        SetWindowPos(m_window->hWnd, nullptr, targetRect.left, targetRect.top, targetRect.right - targetRect.left, targetRect.bottom - targetRect.top, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
      }
    }
#endif
  }

  bool Window::simpleReadMessages(int64_t frame)
  {
    HIGAN_CPU_FUNCTION_SCOPE();
    m_frame = frame;
    m_inputs.setFrame(frame);
#if defined(HIGANBANA_PLATFORM_WINDOWS)
    MSG msg;
    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, Window::LowLevelKeyboardHook, m_window->hInstance, 0);
    m_characterInput.clear();

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT)
        return true;

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(m_keyboardHook);
    updateInputs();

    return false;
#else
    return true;
#endif
  }

  void Window::updateInputs()
  {
    HIGAN_CPU_FUNCTION_SCOPE();
#if defined(HIGANBANA_PLATFORM_WINDOWS)
    if (GetFocus() == m_window->hWnd)
    {
      BYTE cur[256]{};
      GetKeyboardState(cur);
      // handle new keyboards

      for (int i = 0; i < 256; ++i)
      {
        bool pressedCurrent = static_cast<bool>((cur[i] & 0xF0) > 0);
        bool pressedOld = static_cast<bool>((m_oldKeyboardState[i] & 0xF0) > 0);

        if (pressedCurrent && !(pressedOld))
        {
          m_inputs.insert(i, 1, m_frame);
        }
        else if (pressedCurrent)
        {
          m_inputs.insert(i, 2, m_frame);
        }
        else if (!(pressedCurrent) && pressedOld)
        {
          m_inputs.insert(i, 0, m_frame);
        }
      }

      memcpy(m_oldKeyboardState, cur, 256 * sizeof(BYTE));

      /*
      POINT point;
      if (GetCursorPos(&point))
      {
        HIGAN_LOG("%dx%d %dx%dx\n", m_mouse.m_pos.x(), m_mouse.m_pos.y(), point.x, point.y);
        //m_mouse.m_pos = ivec2{ point.x, point.y };
      }*/
    }
    else
    {
      BYTE cur[256]{};
      for (int i = 0; i < 256; ++i)
      {
        bool pressedOld = static_cast<bool>((m_oldKeyboardState[i] & 0xF0) > 0);
        if (pressedOld)
          m_inputs.insert(i, 0, m_frame);
      }
      memcpy(m_oldKeyboardState, cur, 256 * sizeof(BYTE));
    }
#endif
  }
}