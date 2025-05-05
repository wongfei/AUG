#pragma once

#include "AUGCore.h"

using HotkeyId = int;
using HotkeyCode = unsigned int;
using HotkeyCallback = std::function<void(HotkeyId)>;

class IHotkeyWindow
{
public:

	static IHotkeyWindow* CreateInstance();

	virtual ~IHotkeyWindow() {}
	virtual bool Init() = 0;
	virtual bool AddHotkey(HotkeyId Uid, HotkeyCode Mod, HotkeyCode Key, HotkeyCallback Callback) = 0;
	virtual bool AddHotkey(HotkeyId Uid, std::string Combo, HotkeyCallback Callback) = 0;
	virtual bool RemoveHotkey(HotkeyId Uid) = 0;
	virtual void Release() = 0;
};
