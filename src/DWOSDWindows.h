/**
 *	DWOSDWindows.h
 *	Copyright (C) 2005 Nate
 *
 *	This file is part of DigitalWatch, a free DTV watching and recording
 *	program for the VisionPlus DVB-T.
 *
 *	DigitalWatch is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	DigitalWatch is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with DigitalWatch; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef DWOSDWINDOWS_H
#define DWOSDWINDOWS_H

#include "StdAfx.h"
#include "LogMessage.h"
#include "XMLDocument.h"
#include "DWOSDImage.h"
#include "DWOSDControl.h"
#include <vector>

class DWOSDWindows;
class DWOSDWindow : public LogMessageCaller
{
	friend DWOSDWindows;
public:
	DWOSDWindow();
	virtual ~DWOSDWindow();

	LPWSTR Name();
	HRESULT Render(long tickCount);

	DWOSDControl *GetControl(LPWSTR pName);

private:
	HRESULT LoadFromXML(XMLElement *pElement);

	LPWSTR m_pName;
	std::vector<DWOSDControl *> m_controls;
};


class DWOSDWindows : public LogMessageCaller
{
public:
	DWOSDWindows();
	virtual ~DWOSDWindows();

	virtual void SetLogCallback(LogMessageCallback *callback);

	HRESULT Load(LPWSTR filename);
	
	void Show(LPWSTR pWindowName);

	DWOSDWindow *GetWindow(LPWSTR pName);
	DWOSDImage *GetImage(LPWSTR pName);

private:
	std::vector<DWOSDWindow *> m_windows;
	std::vector<DWOSDImage *> m_images;

	LPWSTR m_filename;
};

#endif
