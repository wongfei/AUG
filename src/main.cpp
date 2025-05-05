#include "AUG.h"
#include <curl/curl.h>

#if defined(_WIN32)

static void ResetSystemCursor()
{
	SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);
	while (ShowCursor(TRUE) < 0);
}

static LONG WINAPI CustomExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo)
{
	OutputDebugStringA("FAILURE MEANS DEATH, ADMIRAL!");
	ResetSystemCursor();
	fmtlog::poll(true);
	fmtlog::closeLogFile();
	return EXCEPTION_EXECUTE_HANDLER;
}

//int main(int argc, char* argv[])
int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	int rc = 0;

	if (!IsDebuggerPresent())
	{
		SetUnhandledExceptionFilter(CustomExceptionFilter);
	}

	{
		#if 1
		if (!freopen("AUG_stdout.log", "w+", stdout)) { printf("IMPOSSIBRU\n"); }
		if (!freopen("AUG_stderr.log", "w+", stderr)) { printf("IMPOSSIBRU\n"); }
		struct StdLogTerminator { ~StdLogTerminator() { fclose(stdout); fclose(stderr); } } StdLogTerm;
		#endif

		fmtlog::setLogFile("AUG.log", true);
		struct FmtLogTerminator { ~FmtLogTerminator() { fmtlog::poll(true); fmtlog::closeLogFile(); } } FmtLogTerm;

		curl_global_init(CURL_GLOBAL_DEFAULT);
		struct CurlTerminator { ~CurlTerminator() { curl_global_cleanup(); } } CurlTerm;

		struct CursorTerminator { ~CursorTerminator() { ResetSystemCursor(); } } CursorTerm;

		auto app(std::make_unique<AUG>());
		rc = app->Run();
		app.reset();
	}

	return rc;
}

#else

#error "STOP RIGHT THERE, CRIMINAL SCUM!"

#endif // _WIN32
