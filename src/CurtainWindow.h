#pragma once

#include "AUGCore.h"

class ICurtainWindow
{
public:

	static ICurtainWindow* CreateInstance();

	virtual ~ICurtainWindow() {}
	virtual bool Init() = 0;
	virtual void Release() = 0;
	virtual void Show(bool Show) = 0;
	virtual void* GetHandle() = 0;
};
