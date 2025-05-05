#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <xxh3.h>

#include "IniFile.h"
#include "HotkeyWindow.h"
#include "CurtainWindow.h"
#include "FakeCursor.h"
#include "ImageToText.h"
#include "SpeechToText.h"
#include "Assistant.h"

#include <mutex>
#include <future>
#include <chrono>
#include <queue>

class TextEditor;

class AUG
{
public:
	AUG();
	~AUG();
	int Run();

protected:
	void LoadConfig();
	void SaveConfig();
	void Serialize(IniFile& Conf, bool Save);
	void LoadStyle();
	void SaveStyle();
	void LoadFonts();
	inline ImFont* GetFont(const std::string& Name) const;
	void SetFont(const std::string& Name);

	void InitGui();
	void UpdateLayeredAttribs();
	void InitProcessors();

	void Tick();
	void RenderFrame();
	void PresentFrame();
	inline void MarkDirty() { PendingFrames = 2; }

	void AddPrompt(std::string Prompt, bool Refresh = true);
	void AddPrompt(std::string Prompt, std::string Role, bool Refresh = true);
	void UpdateAiMessages();
	void ProcessPrompt();

	void ReadKeyState();
	bool IsKeyDown(int vkey) const;
	bool WasKeyPressed(int vkey) const;

	void EnableGuiInput(bool Enable);
	void SetOverlayActive(bool Active);
	void UpdateFakeCursor();

	void SetSystemCursorToBlank();
	void SetSystemCursorToDefault();
	void ShowCursorHack(bool Show);

	void ToggleMarkTextMode();

protected:
	// window
	bool ProtectContent = true;
	bool UseColorKey = true;
	float LayerAlpha = 0.85f;
	float4 LayerColorKey {0, 0, 0, 1};
	float4 BackBufferColor {0, 0, 0, 1};

	// tick/draw
	float FixedTickRate = 0.0f;
	bool RenderEveryTick = false;
	bool PresentEveryTick = false;

	// customization
	std::string GuiFontName;
	float4 GuiWindowBg {0.06f, 0.06f, 0.06f, 1.0f};
	bool ShowTitleBar = false;
	bool ShowLogWindow = false;
	bool AutoscrollSpeech = true;
	bool AutoscrollAi = true;

	// assistant
	std::string AiBackendUrl = "http://127.0.0.1:8080/v1/chat/completions";
	std::string AiBackendToken;
	std::string AiSystemPrompt;

	std::mutex DeferredTasksMux;
	std::queue<std::packaged_task<void()>> DeferredTasks;
	std::queue<std::packaged_task<void()>> CurrentTasks;
	template<typename T>
	void DeferTask(T&& Task) // execute task in main (GUI) thread
	{
		std::lock_guard<std::mutex> Lock(DeferredTasksMux);
		DeferredTasks.emplace(std::move(Task));
	}
	void ExecuteDeferredTasks()
	{
		{
			std::lock_guard<std::mutex> Lock(DeferredTasksMux);
			std::swap(DeferredTasks, CurrentTasks);
		}
		while (!CurrentTasks.empty())
		{
			CurrentTasks.front()();
			CurrentTasks.pop();
		}
	}

	struct SDL_Terminator { inline ~SDL_Terminator() { SDL_Quit(); } };
	SDL_Terminator SdlTerminator {};

	std::unique_ptr<SDL_Window, decltype([] (SDL_Window* Handle) { SDL_DestroyWindow(Handle); })> GuiWindow {};
	std::unique_ptr<SDL_GLContextState, decltype([] (SDL_GLContextState* Handle) { SDL_GL_DestroyContext(Handle); })> GlContext {};
	
	std::unique_ptr<XXH64_state_t, decltype([](XXH64_state_t* Handle) { XXH64_freeState(Handle); })> FrameHashState {};
	XXH64_hash_t LastFrameHash {};

	HWND GuiHWND {};
	HWND FocusHWND {};
	HWND CaretHWND {};

	ImFont* GuiFontDefault {};
	std::unordered_map<std::string, ImFont*> FontMap;

	bool ExitFlag {};
	bool GotEvent {};
	bool OverlayActive {};
	bool ShowDemoWindow {};
	bool ShowStyleEditor {};
	
	int PendingFrames {};
	float LastTickMs {};

	bool MarkTextMode {};
	ImVec2 TextBoundsA {};
	ImVec2 TextBoundsB {};

	static constexpr size_t MaxKeys = 255;
	uint16_t OldKeyState[MaxKeys];
	uint16_t CurKeyState[MaxKeys];

	int MouseX {};
	int MouseY {};
	int LockedCursorX {};
	int LockedCursorY {};
	int OverlayCursorOffset = 0;
	ImVec4 CaretRc {};
	bool MouseMoved {};

	std::unique_ptr<IHotkeyWindow> HotkeyWindow;
	std::unique_ptr<ICurtainWindow> CurtainWindow;
	std::unique_ptr<IFakeCursor> DesktopCursor;
	std::unique_ptr<IFakeCursor> OverlayCursor;
	std::unique_ptr<TextEditor> CodeWindow;

	std::vector<IImageToText::Detection> ImageDetections;
	std::string ImageText;

	std::vector<std::string> SpeechSegments;
	ImGuiSelectionBasicStorage SpeechSelection;
	
	std::vector<IAssistant::Message> AiMessages;
	std::string AiPartial;
	std::string AiCombinedMessages;

	std::unique_ptr<IImageToText> ImageProc;
	std::unique_ptr<ISpeechToText> SpeechProc;
	std::unique_ptr<IAssistant> AiProc;
};
