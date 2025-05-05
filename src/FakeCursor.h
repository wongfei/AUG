#pragma once

#include "AUGCore.h"

class IFakeCursor
{
public:

	static IFakeCursor* CreateInstance();

	virtual ~IFakeCursor() {}
	virtual bool Init(const char* DbgName, bool Protect) = 0;
	virtual void Release() = 0;
	virtual bool SetToArrow(bool ForceCopy = true) = 0;
	virtual bool SetToSystemCursor(bool ForceCopy = true) = 0;
	virtual void Move(int x, int y, bool ForceMove = false) = 0;
	virtual void Show(bool Show, bool ForceUpdate = false) = 0;
};
