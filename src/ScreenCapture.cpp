#include "ScreenCapture.h"

#if defined(_WIN32)

#include <windows.h>

namespace Win32 {

class BltScreenCapture : public IScreenCapture
{
public:

	virtual ~BltScreenCapture() override { Release(); }
	virtual bool Init() override;
	virtual void Release() override;
	virtual bool Capture() override;
	virtual cv::Mat& GetImage() override { return BgrMat; }

private:

	HWND Window  {};
	HDC WindowDC  {};
	HDC CompatibleDC  {};
	HBITMAP Bitmap  {};
	BITMAPINFOHEADER BitmapInfo {};
	int Height = 0, Width = 0, SrcHeight = 0, SrcWidth = 0;
	cv::Mat BgrMat;
};

bool BltScreenCapture::Init()
{
	// https://github.com/sturkmen72/opencv_samples/blob/master/Screen-Capturing.cpp

	do
	{
		Window = GetDesktopWindow();
		GUARD_BREAK(Window, "GetDesktopWindow");

		WindowDC = GetDC(Window);
		GUARD_BREAK(WindowDC, "GetDC");

		CompatibleDC = CreateCompatibleDC(WindowDC);
		GUARD_BREAK(CompatibleDC, "CreateCompatibleDC");

		int OldMode = SetStretchBltMode(CompatibleDC, COLORONCOLOR);
		GUARD_BREAK(OldMode != 0, "SetStretchBltMode");

		RECT WindowRect {};
		GetClientRect(Window, &WindowRect);

		SrcHeight = WindowRect.bottom;
		SrcWidth = WindowRect.right;
		Height = WindowRect.bottom;
		Width = WindowRect.right;

		ZeroMemory(&BitmapInfo, sizeof(BitmapInfo));
		BitmapInfo.biSize = sizeof(BITMAPINFOHEADER);
		BitmapInfo.biWidth = Width;
		BitmapInfo.biHeight = -Height; // upside down or not
		BitmapInfo.biPlanes = 1;
		BitmapInfo.biBitCount = 32;
		BitmapInfo.biCompression = BI_RGB;
		BitmapInfo.biSizeImage = 0;
		BitmapInfo.biXPelsPerMeter = 0;
		BitmapInfo.biYPelsPerMeter = 0;
		BitmapInfo.biClrUsed = 0;
		BitmapInfo.biClrImportant = 0;

		Bitmap = CreateCompatibleBitmap(WindowDC, Width, Height);
		GUARD_BREAK(Bitmap, "CreateCompatibleBitmap");

		SelectObject(CompatibleDC, Bitmap);
		BgrMat.create(Height, Width, CV_8UC4);

		return true;
	}
	while (0);

	Release();
	return false;
}

bool BltScreenCapture::Capture()
{
	if (Bitmap)
	{
		if (StretchBlt(CompatibleDC, 0, 0, Width, Height, WindowDC, 0, 0, SrcWidth, SrcHeight, SRCCOPY))
		{
			if (GetDIBits(CompatibleDC, Bitmap, 0, Height, BgrMat.data, (BITMAPINFO*)&BitmapInfo, DIB_RGB_COLORS))
			{
				return true;
			}
		}
	}

	return false;
}

void BltScreenCapture::Release()
{
	if (Bitmap) { DeleteObject(Bitmap); Bitmap = nullptr; }
	if (CompatibleDC) { DeleteDC(CompatibleDC); CompatibleDC = nullptr; }
	if (WindowDC) { ReleaseDC(Window, WindowDC); WindowDC = nullptr; }
}

} // namespace

IScreenCapture* IScreenCapture::CreateInstance()
{
	return new Win32::BltScreenCapture();
}

#endif // _WIN32
