#include "FakeCursor.h"

#if defined(_WIN32)

#include <windows.h>

namespace Win32 {

struct CursorIconInfo
{
	DWORD xHotspot, yHotspot;
	LONG bmWidth, bmHeight;
};

static CursorIconInfo GetCursorIconInfo(HCURSOR ico)
{
	CursorIconInfo res {};
	if (ico)
	{
		ICONINFO info {};
		if (GetIconInfo(ico, &info))
		{
			res.xHotspot = info.xHotspot;
			res.yHotspot = info.yHotspot;

			BITMAP bmpinfo {};
			if (GetObject(info.hbmMask, sizeof(bmpinfo), &bmpinfo))
			{
				const bool bBWCursor = (info.hbmColor == NULL);
				res.bmWidth = bmpinfo.bmWidth;
				res.bmHeight = abs(bmpinfo.bmHeight) / (bBWCursor ? 2 : 1);
			}
			else
			{
				loge("GetObject Error={}", GetLastError());
			}

			if (info.hbmColor) DeleteObject(info.hbmColor);
			if (info.hbmMask) DeleteObject(info.hbmMask);
		}
		else
		{
			loge("GetIconInfo Error={}", GetLastError());
		}
	}
	return res;
}

class FakeCursor : public IFakeCursor
{
public:

	virtual ~FakeCursor() override { Release(); }
	virtual bool Init(const char* DbgName, bool Protect) override;
	virtual void Release() override;
	virtual bool SetToArrow(bool ForceCopy = true) override;
	virtual bool SetToSystemCursor(bool ForceCopy = true) override;
	virtual void Move(int x, int y, bool ForceMove = false) override;
	virtual void Show(bool Show, bool ForceUpdate = false) override;

	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	bool SetCursorHandle(HCURSOR NewCursor, bool ForceCopy);

	void ResetCursorHandle()
	{
		Cursor = nullptr;
		if (CursorOwned)
		{
			DestroyCursor(CursorOwned);
			CursorOwned = nullptr;
		}
	}

private:

	std::string Name {};
	HCURSOR Cursor {};
	HCURSOR CursorOwned {};
	CursorIconInfo IconInfo {};
	COLORREF BrushColor {};
	HBRUSH Brush {};
	HWND Window {};
	int PosX = 0;
	int PosY = 0;
	int WinW = 0;
	int WinH = 0;
};

static LRESULT CALLBACK FakeCursorProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	FakeCursor* Impl = (FakeCursor*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (Impl)
		return Impl->WndProc(hWnd, message, wParam, lParam);
	else
		return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK FakeCursor::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		#if 1
		case WM_PAINT:
		{
			//logi("{} WM_PAINT", Name);

			PAINTSTRUCT ps {};
			HDC hDC = BeginPaint(hWnd, &ps);
			if (hDC)
			{
				RECT rc {};
				GetClientRect(hWnd, &rc);
				const int width = rc.right - rc.left;
				const int height = rc.bottom - rc.top;

				#if 0

				FillRect(hDC, &rc, Brush);
				//DrawIcon(hDC, 0, 0, Cursor); // bugged on win11
				DrawIconEx(hDC, 0, 0, Cursor, 0, 0, 0, nullptr, DI_NORMAL);
				//DrawIconEx(hDC, 0, 0, Cursor, 0, 0, 0, nullptr, DI_MASK);
				//DrawIconEx(hDC, 0, 0, Cursor, 0, 0, 0, nullptr, DI_IMAGE);

				#else // use offscreen buffer

				HDC hdcMem = CreateCompatibleDC(hDC);
				HBITMAP hbmMem = CreateCompatibleBitmap(hDC, width, height);
				HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

				FillRect(hdcMem, &rc, Brush);
				DrawIconEx(hdcMem, 0, 0, Cursor, 0, 0, 0, nullptr, DI_NORMAL);
				//DrawIconEx(hdcMem, 0, 0, Cursor, 0, 0, 0, nullptr, DI_MASK);
				//DrawIconEx(hdcMem, 0, 0, Cursor, 0, 0, 0, nullptr, DI_IMAGE);

				BitBlt(hDC, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);
				SelectObject(hdcMem, hbmOld);
				DeleteObject(hbmMem);
				DeleteDC(hdcMem);

				#endif

				EndPaint(hWnd, &ps);
			}
			else
			{
				//loge("BeginPaint Error={}", GetLastError());
			}
			return 0;
		}
		#endif

		#if 0
		case WM_ERASEBKGND:
		{
			return 0;
		}
		#endif

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

bool FakeCursor::Init(const char* DbgName, bool Protect)
{
	do
	{
		Name = DbgName ? DbgName : "";

		GUARD_BREAK(SetToArrow(true), "SetToArrow");

		WinW = IconInfo.bmWidth;
		WinH = IconInfo.bmHeight;
		//logi("{} W={} H={}", Name, WinW, WinH);

		BrushColor = RGB(0, 0, 0);
		//BrushColor = RGB(255, 0, 0);

		Brush = CreateSolidBrush(BrushColor);
		GUARD_BREAK(Brush, "CreateSolidBrush");

		const char* WindowClass = "FakeCursor";
		WNDCLASS wc {};
		wc.lpfnWndProc = FakeCursorProc;
		wc.hInstance = GetModuleHandle(NULL);
		wc.lpszClassName = WindowClass;
		//wc.style = CS_HREDRAW | CS_VREDRAW;

		ATOM wcid = RegisterClass(&wc); (wcid);

		DWORD StyleEx = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE;
		//StyleEx |= WS_EX_TRANSPARENT;

		Window = CreateWindowEx(
			StyleEx,
			WindowClass, WindowClass, 
			WS_POPUP, 0, 0, WinW, WinH,
			NULL, NULL, wc.hInstance, NULL);
		GUARD_BREAK(Window, "CreateWindowEx");

		SetWindowLongPtr(Window, GWLP_USERDATA, (LONG_PTR)this);

		DWORD LayeredFlags = LWA_ALPHA;
		LayeredFlags |= LWA_COLORKEY;
		SetLayeredWindowAttributes(Window, BrushColor, 230, LayeredFlags); // 230

		//SetWindowPos(Window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

		if (Protect)
		{
			if (!SetWindowDisplayAffinity(Window, WDA_EXCLUDEFROMCAPTURE))
			{
				if (!SetWindowDisplayAffinity(Window, WDA_MONITOR))
				{
					loge("SetWindowDisplayAffinity");
				}
			}
		}

		return true;
	}
	while (0);

	Release();
	return false;
}

void FakeCursor::Release()
{
	if (Window) { CloseWindow(Window); Window = nullptr; }
	if (Brush) { DeleteObject(Brush); Brush = nullptr; }
	ResetCursorHandle();
}

bool FakeCursor::SetCursorHandle(HCURSOR NewCursor, bool ForceCopy)
{
	do
	{
		if (!ForceCopy && Cursor == NewCursor)
			return true;

		const auto NewInfo = GetCursorIconInfo(NewCursor);
		GUARD_BREAK((NewInfo.bmWidth && NewInfo.bmHeight), "GetCursorIconInfo");

		if (ForceCopy)
		{
			NewCursor = (HCURSOR)CopyImage(NewCursor, IMAGE_CURSOR, 0, 0, 0);
			GUARD_BREAK(NewCursor, "CopyImage");
		}

		//logi("{} {} -> {}", Name, (size_t)Cursor, (size_t)NewCursor);
		//logi("{} {} {}", Name, NewInfo.xHotspot, NewInfo.yHotspot);

		ResetCursorHandle();
		CursorOwned = ForceCopy ? NewCursor : nullptr;
		Cursor = NewCursor;
		IconInfo = NewInfo;

		InvalidateRect(Window, nullptr, FALSE);
		//UpdateWindow(Window);

		return true;
	}
	while (0);

	return false;
}

bool FakeCursor::SetToArrow(bool ForceCopy)
{
	HCURSOR NewCursor = LoadCursor(NULL, IDC_ARROW); // IDC_ARROW IDC_CROSS
	if (NewCursor)
	{
		return SetCursorHandle(NewCursor, ForceCopy);
	}

	return false;
}

bool FakeCursor::SetToSystemCursor(bool ForceCopy)
{
	CURSORINFO ci {};
	ci.cbSize = sizeof(ci);
	GetCursorInfo(&ci);

	if (ci.hCursor && (ci.flags & CURSOR_SHOWING))
	{
		return SetCursorHandle(ci.hCursor, ForceCopy);
	}

	return false;
}

void FakeCursor::Move(int x, int y, bool ForceMove)
{
	if (Window && (ForceMove || PosX != x || PosY != y))
	{
		PosX = x;
		PosY = y;

		DWORD swp_flags = SWP_NOACTIVATE;

		if (WinW == IconInfo.bmWidth && WinH == IconInfo.bmHeight)
		{
			swp_flags |= SWP_NOSIZE;
		}
		else
		{
			WinW = IconInfo.bmWidth;
			WinH = IconInfo.bmHeight;
		}

		if (!SetWindowPos(Window, HWND_TOPMOST, x - IconInfo.xHotspot, y - IconInfo.yHotspot, WinW, WinH, swp_flags))
		{
			loge("SetWindowPos");
		}

		//InvalidateRect(Window, nullptr, FALSE);
		//UpdateWindow(Window);
		//RedrawWindow(Window, NULL, NULL, RDW_INVALIDATE | RDW_INTERNALPAINT);
	}
}

void FakeCursor::Show(bool Show, bool ForceUpdate)
{
	if (Window)
	{
		if (ForceUpdate)
			UpdateWindow(Window);

		ShowWindow(Window, Show ? SW_SHOWNOACTIVATE : SW_HIDE);
	}
}

} // namespace

IFakeCursor* IFakeCursor::CreateInstance()
{
	return new Win32::FakeCursor();
}

#endif // _WIN32
