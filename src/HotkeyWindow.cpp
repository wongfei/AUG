#include "HotkeyWindow.h"

#if defined(_WIN32)

#include <windows.h>
#include <unordered_map>

namespace Win32 {

class HotkeyWindow : public IHotkeyWindow
{
public:

	virtual ~HotkeyWindow() override { Release(); }
	virtual bool Init() override;
	virtual bool AddHotkey(HotkeyId Uid, HotkeyCode Mod, HotkeyCode Key, HotkeyCallback Callback) override;
	virtual bool AddHotkey(HotkeyId Uid, std::string Combo, HotkeyCallback Callback) override;
	virtual bool RemoveHotkey(HotkeyId Uid) override;
	virtual void Release() override;

	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:

	HWND Window {};
	bool Visible = false;
	std::unordered_map<HotkeyId, HotkeyCallback> Hotkeys;
};

static LRESULT CALLBACK HotkeyWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HotkeyWindow* Impl = (HotkeyWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (Impl)
		return Impl->WndProc(hWnd, message, wParam, lParam);
	else
		return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK HotkeyWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_NCHITTEST:
		{
			return HTTRANSPARENT;
		}

		case WM_MOUSEACTIVATE:
		{
			return MA_NOACTIVATE;
		}

		#if 0
		case WM_SETCURSOR:
		{
			__debugbreak();
			SetCursor(NULL);
			return TRUE;
		}
		#endif

		case WM_HOTKEY:
		{
			HotkeyId Uid = (HotkeyId)wParam;
			//logi("WM_HOTKEY {}", Uid);
			auto Iter = Hotkeys.find(Uid);
			if (Iter != Hotkeys.end())
			{
				if (Iter->second)
					Iter->second(Uid);
			}
			break;
		}

		case WM_CREATE:
		{
			return 0;
		}

		case WM_CLOSE:
		{
			DestroyWindow(hWnd); // trigger WM_DESTROY
			return 0;
		}

		case WM_DESTROY:
		{
			PostQuitMessage(0); // trigger WM_QUIT
			return 0;
		}
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

bool HotkeyWindow::Init()
{
	do
	{
		const char* WindowClass = "HotkeyWindow";

		WNDCLASS wc {};
		wc.lpfnWndProc = HotkeyWindowProc;
		wc.hInstance = GetModuleHandle(NULL);
		wc.lpszClassName = WindowClass;

		ATOM wcid = RegisterClass(&wc); (wcid);

		Window = CreateWindowEx(
			WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, // WS_EX_LAYERED WS_EX_TOPMOST
			WindowClass, WindowClass, 
			WS_POPUP, 0, 0, 4, 4,
			NULL, NULL, wc.hInstance, NULL);
		GUARD_BREAK(Window, "CreateWindowEx");

		SetWindowLongPtr(Window, GWLP_USERDATA, (LONG_PTR)this);

		//SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 128, LWA_ALPHA); // LWA_COLORKEY

		//SetWindowPos(Window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);

		if (!SetWindowDisplayAffinity(Window, WDA_EXCLUDEFROMCAPTURE))
		{
			if (!SetWindowDisplayAffinity(Window, WDA_MONITOR))
			{
				loge("SetWindowDisplayAffinity");
				break;
			}
		}

		ShowWindow(Window, SW_SHOWNOACTIVATE);

		//RegisterHotKey(Window, 1, MOD_CONTROL, '1');

		return true;
	}
	while (0);

	Release();
	return false;
}

bool HotkeyWindow::AddHotkey(HotkeyId Uid, HotkeyCode Mod, HotkeyCode Key, HotkeyCallback Callback)
{
	if (Uid < 1 || Uid >= 0xBFFF)
	{
		loge("Invalid hotkey: Uid={}", Uid);
		return false;
	}

	if (!Callback)
	{
		loge("Invalid hotkey callback: Uid={} ", Uid);
		return false;
	}

	if (Hotkeys.find(Uid) != Hotkeys.end())
	{
		loge("Hotkey already exists: Uid={}", Uid);
		return false;
	}

	if (!RegisterHotKey(Window, (int)Uid, (UINT)Mod, (UINT)Key))
	{
		loge("RegisterHotKey Uid={} Mod={} Key={} Error={}", Uid, Mod, Key, GetLastError());
		return false;
	}

	Hotkeys[Uid] = std::move(Callback);

	return true;
}

bool HotkeyWindow::AddHotkey(HotkeyId Uid, std::string Combo, HotkeyCallback Callback)
{
	HotkeyCode Mod = 0;
	HotkeyCode Key = 0;

	const auto Args = split(Combo, "^");
	for (const auto& Arg : Args)
	{
		if      (Arg == "Alt")     { Mod |= MOD_ALT;     }
		else if (Arg == "Ctrl")    { Mod |= MOD_CONTROL; }
		else if (Arg == "Shift")   { Mod |= MOD_SHIFT;   }
		else
		{
			int IntArg = std::stoi(Arg);
			if (IntArg > 0 && IntArg < 0xFF) // https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
			{
				Key = (HotkeyCode)IntArg;
			}
		}
	}

	if (!Mod || !Key)
	{
		loge("Invalid hotkey combo: {}", Combo);
		return false;
	}

	return AddHotkey(Uid, Mod, Key, std::move(Callback));
}

bool HotkeyWindow::RemoveHotkey(HotkeyId Uid)
{
	auto Iter = Hotkeys.find(Uid);
	if (Iter != Hotkeys.end())
	{
		Hotkeys.erase(Iter);
		return true;
	}

	return false;
}

void HotkeyWindow::Release()
{
	Hotkeys.clear();
	if (Window) { CloseWindow(Window); Window = nullptr; }
}

} // namespace

IHotkeyWindow* IHotkeyWindow::CreateInstance()
{
	return new Win32::HotkeyWindow();
}

#endif // _WIN32
