#include "CurtainWindow.h"

#if defined(_WIN32)

#include <windows.h>

namespace Win32 {

class CurtainWindow : public ICurtainWindow
{
public:

	virtual ~CurtainWindow() override { Release(); }
	virtual bool Init() override;
	virtual void Release() override;
	virtual void Show(bool Show) override;
	virtual void* GetHandle() override { return Window; }

	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:

	HWND Window {};
	bool Visible = false;
};

static LRESULT CALLBACK CurtainWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CurtainWindow* Impl = (CurtainWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (Impl)
		return Impl->WndProc(hWnd, message, wParam, lParam);
	else
		return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK CurtainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_PAINT:
		{
			//return 0;
			break;
		}

		case WM_ERASEBKGND:
		{
			//return 0;
			break;
		}

		case WM_NCHITTEST:
		{
			return Visible ? HTCLIENT : HTTRANSPARENT;
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

bool CurtainWindow::Init()
{
	do
	{
		const char* WindowClass = "CurtainWindow";
		const int DesktopWidth = GetSystemMetrics(SM_CXSCREEN);
		const int DesktopHeight = GetSystemMetrics(SM_CYSCREEN);

		WNDCLASS wc {};
		wc.lpfnWndProc = CurtainWindowProc;
		wc.hInstance = GetModuleHandle(NULL);
		wc.lpszClassName = WindowClass;

		ATOM wcid = RegisterClass(&wc); (wcid);

		Window = CreateWindowEx(
			WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
			WindowClass, WindowClass, 
			WS_POPUP, 0, 0, DesktopWidth, DesktopHeight,
			NULL, NULL, wc.hInstance, NULL);
		GUARD_BREAK(Window, "CreateWindowEx");

		SetWindowLongPtr(Window, GWLP_USERDATA, (LONG_PTR)this);

		SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 128, LWA_ALPHA); // LWA_COLORKEY

		//SetWindowPos(Window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);

		if (!SetWindowDisplayAffinity(Window, WDA_EXCLUDEFROMCAPTURE))
		{
			if (!SetWindowDisplayAffinity(Window, WDA_MONITOR))
			{
				loge("SetWindowDisplayAffinity");
				break;
			}
		}

		return true;
	}
	while (0);

	Release();
	return false;
}

void CurtainWindow::Release()
{
	if (Window) { CloseWindow(Window); Window = nullptr; }
}

void CurtainWindow::Show(bool Show)
{
	if (Window)
	{
		if (ShowWindow(Window, Show ? SW_SHOWNOACTIVATE : SW_HIDE))
		{
			Visible = Show;
		}
	}
}

} // namespace

ICurtainWindow* ICurtainWindow::CreateInstance()
{
	return new Win32::CurtainWindow();
}

#endif // _WIN32
