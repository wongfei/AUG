#define OEMRESOURCE // HI M$ CLOWNS

#define AUG_HOTKEY_ID_OFFSET 666
#define AUG_ENABLE_CURTAIN 1
#define AUG_ENABLE_FAKE_CURSOR 1
#define AUG_ENABLE_COLOR_EDITOR 0

#include "AUG.h"
#include "imgui_utils.h"
#include "TextEditor.h" // https://github.com/goossens/ImGuiColorTextEdit

#include <iostream>
#include <fstream>

//=================================================================================================

static std::mutex s_LogMux;
static std::string s_LogBuffer;
static std::atomic<int> s_LogProduceId = 0;
static std::atomic<int> s_LogConsumeId = 0;

static void LogCallback(int64_t ns, fmtlog::LogLevel level, fmt::string_view location, size_t basePos,
	fmt::string_view threadName, fmt::string_view msg, size_t bodyPos, size_t logFilePos)
{
	const auto msg_view(std::string_view(msg.begin(), msg.end()));

	std::lock_guard<std::mutex> Lock(s_LogMux);
	s_LogBuffer.append(msg_view);
	s_LogBuffer.append("\n");
	++s_LogProduceId;
}

//=================================================================================================

AUG::AUG()
{
	logi("+AUG");

	memset(CurKeyState, 0, sizeof(CurKeyState));
	memset(OldKeyState, 0, sizeof(OldKeyState));

	AiSystemPrompt = 
		"You are senior software engineer taking part in technical interview. "
		"Your task is to answer interviewer questions and solve code problems using C++. "
		"Include a high level overview and complexity of the solution. "
	;

	// allocate before config
	ImageProc.reset(IImageToText::CreateInstance()); 
	SpeechProc.reset(ISpeechToText::CreateInstance());
	AiProc.reset(IAssistant::CreateInstance());
}

AUG::~AUG()
{
	logi("~AUG");
}

int AUG::Run()
{
	s_LogBuffer.reserve(4096);
	fmtlog::setLogCB(&LogCallback, fmtlog::LogLevel::DBG);
	struct FmtCBTerminator { ~FmtCBTerminator() { fmtlog::setLogCB(nullptr, fmtlog::LogLevel::DBG); } } FmtCBTerm;

	logi("ENTER Run");

	LoadConfig();
	InitGui();
	InitProcessors();

	MarkDirty();
	ExitFlag = false;

	while (!ExitFlag)
	{
		const auto Clock0 = std::chrono::high_resolution_clock::now();

		Tick();
		ExecuteDeferredTasks();

		if (RenderEveryTick || GotEvent || PendingFrames > 0)
		{
			RenderFrame();
		}

		const auto Clock1 = std::chrono::high_resolution_clock::now();
		const auto ClockDur = std::chrono::duration_cast<std::chrono::microseconds>(Clock1 - Clock0);
		const double Elapsed = ClockDur.count() * 1e-6;
		LastTickMs = (float)(ClockDur.count() * 0.001);

		if (FixedTickRate > 0.0f)
		{
			if (Elapsed < FixedTickRate)
			{
				SDL_Delay((Uint32)((FixedTickRate - Elapsed) * 1000.0));
			}
		}
		else if (!GotEvent)
		{
			SDL_Delay(0);
		}
	}

	SetOverlayActive(false);
	SaveConfig();
	SaveStyle();

	logi("LEAVE Run");

	return 0;
}

//=================================================================================================

#define AUG_CONFIG_FILE "AUG.ini"
#define AUG_STYLE_FILE "imgui.style"
#define AUG_FONTS_FILE "fonts.ini"

void AUG::LoadConfig()
{
	IniFile Conf(AUG_CONFIG_FILE);
	Serialize(Conf, false);
}

void AUG::SaveConfig()
{
	ImFont* CurFont = ImGui::GetIO().FontDefault;
	auto FontIter = std::find_if(FontMap.begin(), FontMap.end(), [CurFont](const auto& kv) { return kv.second == CurFont; });
	GuiFontName = (FontIter != FontMap.end() ? FontIter->first : "ImGui");

	IniFile Conf;
	Serialize(Conf, true);
	Conf.save(AUG_CONFIG_FILE);
}

void AUG::Serialize(IniFile& Config, bool Save) // WE DONT NEED REFLECTION IN C++
{
	INI_SERIALIZE_PROP("Window", ProtectContent);
	INI_SERIALIZE_PROP("Window", UseColorKey);
	INI_SERIALIZE_PROP("Window", LayerAlpha);
	INI_SERIALIZE_PROP("Window", LayerColorKey);
	INI_SERIALIZE_PROP("Window", BackBufferColor);

	INI_SERIALIZE_PROP("Tick", FixedTickRate);
	INI_SERIALIZE_PROP("Tick", RenderEveryTick);
	INI_SERIALIZE_PROP("Tick", PresentEveryTick);
	
	INI_SERIALIZE_PROP("User", GuiFontName);
	INI_SERIALIZE_PROP("User", GuiWindowBg);
	INI_SERIALIZE_PROP("User", ShowTitleBar);
	INI_SERIALIZE_PROP("User", ShowLogWindow);
	INI_SERIALIZE_PROP("User", AutoscrollSpeech);
	INI_SERIALIZE_PROP("User", AutoscrollAi);

	INI_SERIALIZE_PROP("Assistant", AiBackendUrl);
	INI_SERIALIZE_PROP("Assistant", AiBackendToken);
	INI_SERIALIZE_PROP("Assistant", AiSystemPrompt);

	if (SpeechProc)
		SpeechProc->Serialize(Config, Save);
}

void AUG::LoadStyle() // WE DONT NEED REFLECTION IN C++
{
	ImGuiStyle& CurStyle = ImGui::GetStyle();
	const size_t DataSize = sizeof(ImGuiStyle);

	FILE* fd = fopen(AUG_STYLE_FILE, "rb");
	if (fd)
	{
		size_t TmpSize = 0;
		fread(&TmpSize, sizeof(size_t), 1, fd);

		if (TmpSize != DataSize)
		{
			loge("Size mismatch: {} != {}", TmpSize, DataSize);
		}
		else
		{
			ImGuiStyle TmpStyle {};
			if (fread(&TmpStyle, DataSize, 1, fd) != 1)
			{
				loge("fread style");
			}
			else
			{
				memcpy(&CurStyle, &TmpStyle, sizeof(ImGuiStyle));
			}
		}
		fclose(fd);
	}
}

void AUG::SaveStyle() // WE DONT NEED REFLECTION IN C++
{
	ImGuiStyle& CurStyle = ImGui::GetStyle();
	const size_t DataSize = sizeof(ImGuiStyle);

	FILE* fd = fopen(AUG_STYLE_FILE, "wb");
	if (fd)
	{
		fwrite(&DataSize, sizeof(size_t), 1, fd);
		fwrite(&CurStyle, DataSize, 1, fd);
		fclose(fd);
	}
}

void AUG::LoadFonts()
{
	ImGuiIO& io = ImGui::GetIO();

	FontMap.clear();
	io.Fonts->Clear();

	GuiFontDefault = io.Fonts->AddFontDefault();
	io.FontDefault = GuiFontDefault;

	IniFile Ini(AUG_FONTS_FILE);
	static const std::string Section("Fonts");
	if (Ini.hasSection(Section))
	{
		const auto& Sec = Ini.getSection(Section);
		for (auto FontIter : Sec.keys) // BigFont=tahoma.ttf,24.0
		{
			const auto FontArgs = split(FontIter.second, ",");
			if (FontArgs.size() == 2)
			{
				const float FontSize = std::stof(FontArgs[1]);

				if (!FontIter.first.empty() && !FontArgs[0].empty() && FontSize >= 6.0f && FontSize <= 72.0f)
				{
					ImFont* TmpFont = io.Fonts->AddFontFromFileTTF(FontArgs[0].c_str(), FontSize);
					if (TmpFont)
					{
						FontMap[FontIter.first] = TmpFont;
						//logi("Add font: {} -> {}", FontIter.first, FontIter.second);
					}
					else
					{
						loge("Invalid font: {} -> {}", FontIter.first, FontIter.second);
					}
				}
				else
				{
					loge("Invalid font: {} -> {}", FontIter.first, FontIter.second);
				}
			}
		}
	}
}

ImFont* AUG::GetFont(const std::string& Name) const
{
	auto Iter = FontMap.find(Name);
	return (Iter != FontMap.end() ? Iter->second : nullptr);
}

void AUG::SetFont(const std::string& Name)
{
	ImFont* Font = GetFont(Name);
	ImGui::GetIO().FontDefault = (Font ? Font : GuiFontDefault);
}

//=================================================================================================

void AUG::InitGui() // TODO: switch to DX11 backend
{
	if (!SDL_Init(SDL_INIT_VIDEO)) // SDL_INIT_AUDIO
	{
		loge("SDL_Init Error={}", SDL_GetError());
		throw std::exception("SDL_Init");
	}

	POINT mp {}; GetCursorPos(&mp); MouseX = mp.x; MouseY = mp.y;

	// invisible window to capture WM_HOTKEY because SDL cant
	HotkeyWindow.reset(IHotkeyWindow::CreateInstance());
	GUARD_THROW(HotkeyWindow->Init(), "HotkeyWindow");

	#if (AUG_ENABLE_CURTAIN)
	// invisible window to intercept non gui clicks when overlay is active
	CurtainWindow.reset(ICurtainWindow::CreateInstance());
	GUARD_THROW(CurtainWindow->Init(), "CurtainWindow");
	#endif

	#if (AUG_ENABLE_FAKE_CURSOR)
	DesktopCursor.reset(IFakeCursor::CreateInstance()); 
	GUARD_THROW(DesktopCursor->Init("DesktopCursor", false), "DesktopCursor");

	OverlayCursor.reset(IFakeCursor::CreateInstance());
	GUARD_THROW(OverlayCursor->Init("OverlayCursor", true), "OverlayCursor");
	#endif

	const char* GlslVersion = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	//SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	//SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	const int DesktopWidth = GetSystemMetrics(SM_CXSCREEN);
	const int DesktopHeight = GetSystemMetrics(SM_CYSCREEN);

	Uint32 WindowFlags =
		SDL_WINDOW_OPENGL
		| SDL_WINDOW_HIDDEN
		| SDL_WINDOW_BORDERLESS
		| SDL_WINDOW_ALWAYS_ON_TOP
		;

	GuiWindow.reset(SDL_CreateWindow("AUG", DesktopWidth - 2, DesktopHeight - 2, WindowFlags)); // full size SDL window is bugged
	if (!GuiWindow)
	{
		loge("SDL_CreateWindow Error={}", SDL_GetError());
		throw std::exception("SDL_CreateWindow");
	}

	SDL_SetWindowPosition(GuiWindow.get(), 0, 0);

	GlContext.reset(SDL_GL_CreateContext(GuiWindow.get()));
	if (!GlContext)
	{
		loge("SDL_GL_CreateContext Error={}", SDL_GetError());
		throw std::exception("SDL_GL_CreateContext");
	}

	#if defined(_WIN32)
	GuiHWND = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(GuiWindow.get()), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	if (GuiHWND)
	{
		LONG CurExStyle = GetWindowLong(GuiHWND, GWL_EXSTYLE);
		SetWindowLong(GuiHWND, GWL_EXSTYLE, CurExStyle | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE);
		//SetWindowLong(GuiHWND, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED); // WS_EX_TRANSPARENT
		//SetWindowLong(GuiHWND, GWL_STYLE, WS_POPUP);

		UpdateLayeredAttribs();

		if (ProtectContent)
		{
			if (!SetWindowDisplayAffinity(GuiHWND, WDA_EXCLUDEFROMCAPTURE))
			{
				if (!SetWindowDisplayAffinity(GuiHWND, WDA_MONITOR))
				{
					loge("SetWindowDisplayAffinity Error={}", GetLastError());
					throw std::exception("SetWindowDisplayAffinity");
				}
			}
		}
	}
	#endif

	SDL_GL_MakeCurrent(GuiWindow.get(), GlContext.get());
	SDL_GL_SetSwapInterval(0); // vsync
	SDL_ShowWindow(GuiWindow.get());

	// IMGUI

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (io);
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	EnableGuiInput(false);

	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();
	LoadStyle();

	ImGui_ImplSDL3_InitForOpenGL(GuiWindow.get(), GlContext.get());
	ImGui_ImplOpenGL3_Init(GlslVersion);
	FrameHashState.reset(XXH64_createState());

	LoadFonts();

	if (GuiFontName != "ImGui")
	{
		if (GuiFontName.empty())
		{
			if (DesktopHeight <= 1080)
				GuiFontName = "Default_HD";
			else
				GuiFontName = "Default_UHD";
		}
		if (!GetFont(GuiFontName))
		{
			loge("Font not found: {}", GuiFontName);
			GuiFontName.clear();
		}
		SetFont(GuiFontName);
	}

	#if (AUG_ENABLE_COLOR_EDITOR)
	CodeWindow.reset(new TextEditor());
	CodeWindow->SetReadOnlyEnabled(true);
	CodeWindow->SetShowWhitespacesEnabled(false);
	CodeWindow->SetPalette(TextEditor::GetDarkPalette());
	CodeWindow->SetLanguage(TextEditor::Language::Cpp());
	#if 0
	{
		std::ifstream CodeFile("D:/AUG/src/AUG.cpp");
		std::stringstream CodeBuffer;
		CodeBuffer << CodeFile.rdbuf();
		CodeWindow->SetText(CodeBuffer.str());
	}
	#endif
	#endif

	// HOTKEYS

	HotkeyId Uid = AUG_HOTKEY_ID_OFFSET;

	HotkeyWindow->AddHotkey(Uid++, MOD_SHIFT, VK_OEM_3, [this](HotkeyId Uid) // ~
	{
		//logi("Hotkey {}", Uid);
		SetOverlayActive(!OverlayActive);
	});

	#if 0
	HotkeyWindow->AddHotkey(Uid++, MOD_CONTROL, 'R', [this](HotkeyId Uid)
	{
		ToggleMarkTextMode();
		// auto prompt after selection done
		if (!MarkTextMode && !ImageText.empty())
		{
			AddPrompt(ImageText);
			ProcessPrompt();
		}
	});
	#endif
}

void AUG::UpdateLayeredAttribs()
{
	SetLayeredWindowAttributes(GuiHWND, 
		RGB(LayerColorKey.x * 255, LayerColorKey.y * 255, LayerColorKey.z * 255), 
		(BYTE)(LayerAlpha * 255), 
		UseColorKey ? LWA_COLORKEY | LWA_ALPHA : LWA_ALPHA);
}

//=================================================================================================

void AUG::InitProcessors()
{
	GUARD_THROW(ImageProc->Init([this](IImageToText::Result& ProcessorRes)
	{
		auto Res(std::move(ProcessorRes)); // COMPILER IS SMARTER THEN CODE MONKEY, RIGHT!?
		DeferTask([this, Res = std::move(Res)]() mutable // HELLO BRAINFUCK
		{
			ImageDetections = std::move(Res.Detections);
			ImageText = std::move(Res.Text);
			MarkDirty();
		});
	}), "ImageToText");
	
	GUARD_THROW(SpeechProc->Init([this](ISpeechToText::Result& ProcessorRes)
	{
		auto Res(std::move(ProcessorRes));
		DeferTask([this, Res = std::move(Res)]() mutable
		{
			std::move(std::begin(Res.Segments), std::end(Res.Segments), std::back_inserter(SpeechSegments));
			MarkDirty();
		});
	}), "SpeechToText");
	
	GUARD_THROW(AiProc->Init(AiBackendUrl, AiBackendToken, [this](IAssistant::Result& ProcessorRes)
	{
		auto Res(std::move(ProcessorRes));
		DeferTask([this, Res = std::move(Res)]() mutable
		{
			if (Res.Partial) // delta
			{
				AiPartial.append(Res.Content);
			}
			else // full message
			{
				if (!Res.Content.empty())
				{
					AiMessages.emplace_back(IAssistant::Message { "assistant", std::move(Res.Content) });
				}
				AiPartial.clear();
			}
			UpdateAiMessages();
			MarkDirty();
		});
	}), "Assistant");
}

//=================================================================================================

void AUG::Tick()
{
	#if defined(_WIN32)
	{
		GUITHREADINFO ti {};
		ti.cbSize = sizeof(GUITHREADINFO);
		if (GetGUIThreadInfo(0, &ti))
		{
			FocusHWND = ti.hwndFocus;
			CaretHWND = ti.hwndCaret;

			if (ti.hwndCaret)
			{
				POINT CaretMin {ti.rcCaret.left, ti.rcCaret.top};
				POINT CaretMax {ti.rcCaret.right, ti.rcCaret.bottom};
				ClientToScreen(ti.hwndCaret, &CaretMin);
				ClientToScreen(ti.hwndCaret, &CaretMax);
				CaretRc = {(float)CaretMin.x, (float)CaretMin.y, (float)CaretMax.x, (float)CaretMax.y};
			}
		}
	}
	#endif // _WIN32

	ReadKeyState();

	SDL_Event event {};
	GotEvent = false;

	while (SDL_PollEvent(&event))
	{
		GotEvent = true;

		ImGui_ImplSDL3_ProcessEvent(&event);

		//logi("Event {}", event.type);

		if (event.type == SDL_EVENT_QUIT)
		{
			ExitFlag = true;
		}

		if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(GuiWindow.get()))
		{
			ExitFlag = true;
		}
	}

	const int OldMouseX = MouseX;
	const int OldMouseY = MouseY;

	POINT mp {}; GetCursorPos(&mp); MouseX = mp.x; MouseY = mp.y;
	MouseMoved = (MouseX != OldMouseX || MouseY != OldMouseY);

	ImGuiIO& io = ImGui::GetIO();
	io.MousePos = ImVec2((float)MouseX, (float)MouseY);

	UpdateFakeCursor();

	// window drag fix
	if (OverlayActive && MouseMoved && IsKeyDown(VK_LBUTTON))
	{
		GotEvent = true;
	}

	// exit
	if (WasKeyPressed(VK_PAUSE))
	{
		ExitFlag = true;
		MarkTextMode = false;
	}

	if (OverlayActive && WasKeyPressed(VK_MBUTTON))
	{
		ToggleMarkTextMode();
	}

	if (MarkTextMode)
	{
		TextBoundsB = { (float)MouseX, (float)MouseY };
		GotEvent = true;
	}

	if (GotEvent)
	{
		MarkDirty();
	}

	if (s_LogConsumeId != s_LogProduceId)
	{
		s_LogConsumeId = s_LogProduceId.load();
		MarkDirty();
	}
}

void AUG::ToggleMarkTextMode()
{
	MarkTextMode = !MarkTextMode;
	if (MarkTextMode)
	{
		TextBoundsA = { (float)MouseX, (float)MouseY };
		TextBoundsB = TextBoundsA;
		ImageDetections.clear();
	}
	else
	{
		TextBoundsB = { (float)MouseX, (float)MouseY };
		if (ImageProc)
		{
			ImageProc->Process(GetRect(TextBoundsA, TextBoundsB));
		}
	}
}

//=================================================================================================

void AUG::RenderFrame()
{
	if (PendingFrames > 0) PendingFrames--;

	ImGuiIO& io = ImGui::GetIO();

	float mx = 0, my = 0;
	SDL_GetGlobalMouseState(&mx, &my);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	#if 1
	ImDrawList* DrawList = ImGui::GetBackgroundDrawList();
	if (DrawList)
	{
		//DrawList->AddRect(ImVec2(mx-4, my-4), ImVec2(mx+4, my+4), IM_COL32(0, 255, 0, 128));
		//DrawList->AddRect(ImVec2(fake_mx, fake_my), ImVec2(fake_mx+8, fake_my+8), IM_COL32(255, 0, 0, 128));
		//DrawList->AddRect(ImVec2(CaretRc.x, CaretRc.y), ImVec2(CaretRc.z, CaretRc.w), IM_COL32(0, 255, 0, 128));

		#if 0
		for (const auto& Det : ImageDetections)
		{
			const auto& r = Det.Rect;
			DrawList->AddRect(ImVec2((float)r.Left, (float)r.Top), ImVec2((float)r.Right, (float)r.Bottom), IM_COL32(255, 0, 0, 128));
		}
		#endif

		if (MarkTextMode)
		{
			auto r1 = TextBoundsA;
			auto r2 = TextBoundsB;
			MinMax(r1, r2);

			if (r2.x > r1.x && r2.y > r1.y)
			{
				DrawList->AddRect(r1, r2, IM_COL32(0, 255, 0, 128));
			}
		}
	}
	#endif

	ImGuiWindowFlags WndFlags = ImGuiWindowFlags_None 
		| ImGuiWindowFlags_NoCollapse 
		| ImGuiWindowFlags_NoFocusOnAppearing 
		| ImGuiWindowFlags_NoBringToFrontOnFocus
	;
	if (!ShowTitleBar)
	{
		WndFlags |= ImGuiWindowFlags_NoTitleBar;
	}

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImConv(GuiWindowBg));

	if (OverlayActive)
	{
		ImGui::SetNextWindowPos(ImVec2(17,623), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(332,327), ImGuiCond_FirstUseEver);
		ImGui::Begin("AUG", nullptr, WndFlags);

		if (ImGui::CollapsingHeader("App"))
		{
			ImGui::ShowFontSelector("Font");
			ImGui::Checkbox("Style Editor", &ShowStyleEditor);

			bool b1 = ImGui::SliderFloat("LayerAlpha", &LayerAlpha, 0.1f, 1.0f);
			bool b2 = ImGui::ColorEdit3("LayerColorKey", (float*)&LayerColorKey);
			bool b3 = ImGui::Checkbox("UseColorKey", &UseColorKey);

			if (b1 || b2 || b3)
			{
				UpdateLayeredAttribs();
			}

			ImGui::ColorEdit4("BackBufferColor", (float*)&BackBufferColor);
			ImGui::ColorEdit4("GuiWindowBg", (float*)&GuiWindowBg);

			ImGui::SliderFloat("FixedTickRate", &FixedTickRate, 0.0f, 0.2f);
			ImGui::Checkbox("RenderEveryTick", &RenderEveryTick);
			ImGui::Checkbox("PresentEveryTick", &PresentEveryTick);

			ImGui::Checkbox("ShowTitleBar", &ShowTitleBar);
			//ImGui::Checkbox("AutoscrollSpeech", &AutoscrollSpeech);
			//ImGui::Checkbox("AutoscrollAi", &AutoscrollAi);
		}

		if (SpeechProc && ImGui::CollapsingHeader("Speech"))
		{
			SpeechProc->RenderUI();
		}

		if (ImGui::CollapsingHeader("Debug"))
		{
			ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			//ImGui::Text("%.3f ms", LastTickMs);

			ImGui::Text("ImGui      %p", GuiHWND);
			ImGui::Text("Curtain    %p", CurtainWindow ? CurtainWindow->GetHandle() : nullptr);
			ImGui::Text("Foreground %p", GetForegroundWindow());
			ImGui::Text("Focus      %p", FocusHWND);
			ImGui::Text("Caret      %p", CaretHWND);

			ImGui::Checkbox("Log Window", &ShowLogWindow);
			ImGui::Checkbox("Demo Window", &ShowDemoWindow);

			#if 0
			if (ImGui::Button("tell joke")) { AddPrompt("tell joke"); }
			if (ImGui::Button("one more")) { AddPrompt("one more"); }
			#endif

			if (ImGui::Button("!CRASH!"))
			{
				*((size_t*)(nullptr)) = 0xDEADBEEF;
			}
		}

		if (ImGui::Button("ExitApp"))
		{
			ExitFlag = true;
		}

		ImGui::End();
	}

	if (ShowLogWindow)
	{
		ImGui::SetNextWindowPos(ImVec2(420,60), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(813,162), ImGuiCond_FirstUseEver);
		ImGui::Begin("Log##AUG", nullptr, WndFlags);
		{
			fmtlog::poll();
			std::lock_guard<std::mutex> Lock(s_LogMux);
			ImGui::TextUnformatted(s_LogBuffer.data(), s_LogBuffer.data() + s_LogBuffer.size());
		}
		ImGuiAutoScrollY();
		ImGui::End();
	}

	#if 1
	if (OverlayActive || !ImageText.empty())
	{
		ImGui::SetNextWindowPos(ImVec2(1254,96), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(640,212), ImGuiCond_FirstUseEver);
		ImGui::Begin("Image##AUG", nullptr, WndFlags);
		if (OverlayActive)
		{
			if (ImGui::Button("Assist##PromptImage"))
			{
				if (!ImageText.empty())
				{
					AddPrompt(ImageText);
					ProcessPrompt();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear##ClearImage"))
			{
				ImageDetections.clear();
				ImageText.clear();
			}
		}
		TextUnformattedWithWrap(ImageText.data(), ImageText.data() + ImageText.size());
		ImGuiAutoScrollY();
		ImGui::End();
	}
	#endif

	#if 1
	{
		ImGui::SetNextWindowPos(ImVec2(1263,327), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(617,381), ImGuiCond_FirstUseEver);
		ImGui::Begin("Speech##AUG", nullptr, WndFlags); // ImGuiWindowFlags_NoScrollbar
		if (OverlayActive)
		{
			if (ImGui::Button("Assist##PromptSpeech"))
			{
				std::string Text;
				Text.reserve(1024);
				for (size_t id = 0; id < SpeechSegments.size(); ++id)
				{
					if (SpeechSelection.Contains((ImGuiID)id))
					{
						Text.append(SpeechSegments[id]);
					}
				}
				if (!Text.empty())
				{
					AddPrompt(Text);
					ProcessPrompt();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear##ClearSpeech"))
			{
				SpeechSegments.clear();
			}
			ImGui::SameLine();
			ImGui::Checkbox("Autoscroll##AutoscrollSpeech", &AutoscrollSpeech);
		}
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		//if (ImGui::BeginChild("##SpeechChild", ImVec2(-FLT_MIN, ImGui::GetFontSize() * 20), ImGuiChildFlags_FrameStyle | ImGuiChildFlags_ResizeY))
		if (ImGui::BeginChild("##SpeechChild", ImVec2(0, 0), ImGuiChildFlags_FrameStyle))
		{
			const int ItemCount = (int)SpeechSegments.size();
			ImGuiMultiSelectFlags SelFlags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_BoxSelect1d;
			ImGuiMultiSelectIO* SelIO = ImGui::BeginMultiSelect(SelFlags, SpeechSelection.Size, ItemCount);
			SpeechSelection.ApplyRequests(SelIO);

			ImGuiListClipper Clipper;
			Clipper.Begin(ItemCount);
			if (SelIO->RangeSrcItem != -1)
				Clipper.IncludeItemByIndex((int)SelIO->RangeSrcItem);

			while (Clipper.Step())
			{
				for (int n = Clipper.DisplayStart; n < Clipper.DisplayEnd; n++)
				{
					const bool Selected = SpeechSelection.Contains((ImGuiID)n);
					ImGui::PushID(n);
					ImGui::SetNextItemSelectionUserData(n);
					ImGui::Selectable(SpeechSegments[n].c_str(), Selected);
					ImGui::PopID();
				}
			}

			SelIO = ImGui::EndMultiSelect();
			SpeechSelection.ApplyRequests(SelIO);
			if (AutoscrollSpeech)
			{
				ImGuiAutoScrollY();
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::End();
	}
	#endif

	#if 1
	{
		ImGui::SetNextWindowPos(ImVec2(385,524), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(817,454), ImGuiCond_FirstUseEver);
		ImGui::Begin("AI##AUG", nullptr, WndFlags);
		if (OverlayActive)
		{
			#if 0
			if (ImGui::Button(">>##ProcessPrompt"))
			{
				ProcessPrompt();
			}
			ImGui::SameLine();
			#endif
			if (ImGui::Button("Clear##ClearMessages"))
			{
				AiMessages.clear();
				UpdateAiMessages();
			}
			ImGui::SameLine();
			ImGui::Checkbox("Autoscroll##AutoscrollAi", &AutoscrollAi);
		}
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		if (ImGui::BeginChild("##AIChild", ImVec2(0, 0), ImGuiChildFlags_FrameStyle))
		{
			#if (AUG_ENABLE_COLOR_EDITOR)
			CodeWindow->Render("CodeEditor");
			#else
				TextUnformattedWithWrap(AiCombinedMessages.data(), AiCombinedMessages.data() + AiCombinedMessages.size());
				if (AutoscrollAi)
				{
					ImGuiAutoScrollY();
				}
			#endif
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::End();
	}
	#endif

	if (ShowDemoWindow)
	{
		ImGui::ShowDemoWindow(&ShowDemoWindow);
	}

	if (ShowStyleEditor)
    {
        ImGui::Begin("Style Editor", &ShowStyleEditor);
        ImGui::ShowStyleEditor();
        ImGui::End();
    }

	ImGui::PopStyleColor();
	ImGui::Render();

	PresentFrame();
}

//=================================================================================================

void AUG::PresentFrame()
{
	XXH64_hash_t NewFrameHash = 0;

	if (!PresentEveryTick)
	{
		// https://github.com/ocornut/imgui/issues/2268
		ImDrawData* DrawData = ImGui::GetDrawData(); // only available after Render()
		if (DrawData)
		{
			auto* Hash = FrameHashState.get();
			XXH64_reset(Hash, 0);
			for (int i = 0; i < DrawData->CmdListsCount; i++) // very fast XD
			{
				XXH64_update(Hash, &DrawData->CmdLists[i]->VtxBuffer[0], DrawData->CmdLists[i]->VtxBuffer.size() * sizeof(ImDrawVert));
				XXH64_update(Hash, &DrawData->CmdLists[i]->IdxBuffer[0], DrawData->CmdLists[i]->IdxBuffer.size() * sizeof(ImDrawIdx));
				XXH64_update(Hash, &DrawData->CmdLists[i]->CmdBuffer[0], DrawData->CmdLists[i]->CmdBuffer.size() * sizeof(ImDrawCmd));
			}
			NewFrameHash = XXH64_digest(Hash);
		}
		else
		{
			LastFrameHash = 0xFFFF;
		}
	}
	else
	{
		LastFrameHash = 0xFFFF;
	}

	if (LastFrameHash != NewFrameHash)
	{
		LastFrameHash = NewFrameHash;

		ImGuiIO& io = ImGui::GetIO();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);

		glClearColor(BackBufferColor.x * BackBufferColor.w, BackBufferColor.y * BackBufferColor.w, BackBufferColor.z * BackBufferColor.w, BackBufferColor.w);
		//glClearColor(BackBufferColor.x, BackBufferColor.y, BackBufferColor.z, BackBufferColor.w);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(GuiWindow.get());
	}
}

//=================================================================================================

void AUG::AddPrompt(std::string Prompt, bool Refresh)
{
	AiMessages.emplace_back(IAssistant::Message {"user", std::move(Prompt)});
	if (Refresh)
		UpdateAiMessages();
}

void AUG::AddPrompt(std::string Prompt, std::string Role, bool Refresh)
{
	AiMessages.emplace_back(IAssistant::Message {std::move(Role), std::move(Prompt)});
	if (Refresh)
		UpdateAiMessages();
}

void AUG::UpdateAiMessages()
{
	AiCombinedMessages.clear();
	for (const auto& Msg : AiMessages)
	{
		AiCombinedMessages.append("\n<");
		AiCombinedMessages.append(Msg.Role);
		AiCombinedMessages.append(">\n");
		AiCombinedMessages.append(Msg.Content);
		AiCombinedMessages.append("\n");
	}

	if (!AiPartial.empty())
	{
		AiCombinedMessages.append("\n<assistant>\n");
		AiCombinedMessages.append(AiPartial);
	}

	#if (AUG_ENABLE_COLOR_EDITOR)
	CodeWindow->SetText(AiCombinedMessages);
	CodeWindow->ScrollToLine(CodeWindow->GetLineCount(), TextEditor::Scroll::alignBottom);
	#endif
}

void AUG::ProcessPrompt()
{
	if (AiMessages.empty())
		return;

	if (AiMessages[0].Role != "system" && !AiSystemPrompt.empty())
	{
		AiMessages.insert(AiMessages.begin(), IAssistant::Message { "system", AiSystemPrompt });
		UpdateAiMessages();
	}

	AiProc->Process(AiMessages);
}

//=================================================================================================

#if defined(_WIN32)

// SHORT GetAsyncKeyState([in] int vKey)
// if the most significant bit is set, the key is down
// if the least significant bit is set, the key was pressed after the previous call to GetAsyncKeyState
// however, you should not rely on this last behavior -> GJ BILLY :D

inline bool GetKeyDownBit(uint16_t State)
{
	return State & 0x8000;
}

inline bool GetKeyPressedBit(uint16_t State)
{
	return State & 0x1;
}

void AUG::ReadKeyState()
{
	for (int i = 0; i < (int)MaxKeys; ++i)
	{
		OldKeyState[i] = CurKeyState[i];
		CurKeyState[i] = GetAsyncKeyState(i);

		#if 0
		if (OldKeyState[i] != CurKeyState[i])
		{
			logi("KEY {} -> {}", i, CurKeyState[i]);
		}
		#endif
	}
}

bool AUG::IsKeyDown(int vkey) const
{
	return GetKeyDownBit(CurKeyState[vkey]);
}

bool AUG::WasKeyPressed(int vkey) const
{
	const bool OldDown = GetKeyDownBit(OldKeyState[vkey]);
	const bool CurDown = GetKeyDownBit(CurKeyState[vkey]);
	return (CurDown && !OldDown);
}

void AUG::EnableGuiInput(bool Enable)
{
	ImGuiIO& io = ImGui::GetIO();
	LONG ExStyle = GetWindowLong(GuiHWND, GWL_EXSTYLE);

	if (Enable)
	{
		ExStyle &= ~WS_EX_TRANSPARENT;
		SetWindowLong(GuiHWND, GWL_EXSTYLE, ExStyle);

		io.ConfigFlags = ImGuiConfigFlags_NoKeyboard;
	}
	else
	{
		ExStyle |= WS_EX_TRANSPARENT;
		SetWindowLong(GuiHWND, GWL_EXSTYLE, ExStyle);

		io.ConfigFlags = (ImGuiConfigFlags_NoKeyboard | ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
	}
}

void AUG::SetOverlayActive(bool Active)
{
	if (OverlayActive == Active)
		return;

	OverlayActive = Active;
	//logi("SetOverlayActive {}", Active);

	// TODO: FKN CURSOR IS FLICKERING ON TOGGLE

	if (Active)
	{
		LockedCursorX = MouseX;
		LockedCursorY = MouseY;

		#if (AUG_ENABLE_CURTAIN)
		CurtainWindow->Show(true);
		#endif

		#if (AUG_ENABLE_FAKE_CURSOR)

		DesktopCursor->SetToSystemCursor(true);
		
		ShowCursorHack(false);
		SetSystemCursorToBlank();

		const int off = OverlayCursorOffset;
		DesktopCursor->Move(MouseX, MouseY, true);
		OverlayCursor->Move(MouseX + off, MouseY + off, true);

		//CurtainWindow->Show(true);
		OverlayCursor->Show(true, true);
		DesktopCursor->Show(true, true);

		if (off)
		{
			MouseX += off;
			MouseY += off;
			SetCursorPos(MouseX, MouseY);
		}

		#endif // AUG_ENABLE_FAKE_CURSOR

		EnableGuiInput(true);
	}
	else
	{
		EnableGuiInput(false);

		#if (AUG_ENABLE_CURTAIN)
		CurtainWindow->Show(false);
		#endif

		#if (AUG_ENABLE_FAKE_CURSOR)

		OverlayCursor->Show(false);
		DesktopCursor->Show(false);

		if (MouseX != LockedCursorX || MouseY != LockedCursorY)
		{
			MouseX = LockedCursorX;
			MouseY = LockedCursorY;
			SetCursorPos(MouseX, MouseY);
		}

		SetSystemCursorToDefault();
		ShowCursorHack(true);

		#endif // AUG_ENABLE_FAKE_CURSOR
	}

	ImageDetections.clear();
	MarkTextMode = false;

	MarkDirty();
}

void AUG::UpdateFakeCursor()
{
	#if (AUG_ENABLE_FAKE_CURSOR)

	if (OverlayActive && MouseMoved)
	{
		OverlayCursor->Move(MouseX, MouseY);
	}

	#endif // AUG_ENABLE_FAKE_CURSOR
}

// TY BILLY FOR THIS BRAIN FUCK FEST

static BOOL SetSystemCursorBlank(DWORD CursorId)
{
	ICONINFO icon {};
	icon.fIcon = FALSE;
	icon.xHotspot = 0;
	icon.yHotspot = 0;
	icon.hbmMask = CreateBitmap(1, 1, 1, 1, NULL);
	icon.hbmColor = NULL;
	HCURSOR Cursor = CreateIconIndirect(&icon);

	if (!Cursor)
	{
		loge("CreateIconIndirect Error={}", GetLastError());
		DeleteObject(icon.hbmMask);
		return FALSE;
	}

	if (!SetSystemCursor(Cursor, CursorId))
	{
		loge("SetSystemCursor Error={}", GetLastError());
		DestroyCursor(Cursor);
        DeleteObject(icon.hbmMask);
		return FALSE;
	}

	return TRUE;
}

void AUG::SetSystemCursorToBlank()
{
	const DWORD Ids[] = {OCR_NORMAL, OCR_IBEAM, OCR_WAIT, OCR_CROSS, OCR_UP, 
		OCR_SIZENWSE, OCR_SIZENESW, OCR_SIZEWE, OCR_SIZENS, OCR_SIZEALL, 
		OCR_NO, OCR_HAND, OCR_APPSTARTING};

	for (DWORD Id : Ids)
	{
		if (!SetSystemCursorBlank(Id))
		{
			loge("SetSystemCursorBlank {}", Id);
		}
	}
}

void AUG::SetSystemCursorToDefault()
{
	SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);
}

void AUG::ShowCursorHack(bool Show)
{
	if (Show)
	{
		while (ShowCursor(TRUE) < 0);
	}
	else
	{
		while (ShowCursor(FALSE) >= 0);
	}
}

#endif // _WIN32
