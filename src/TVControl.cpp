/**
 *	TVControl.cpp
 *	Copyright (C) 2003-2004 Nate
 *	Copyright (C) 2004 RAvenGEr
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

#include "StdAfx.h"
#include <time.h>
#include "TVControl.h"
#include "Globals.h"
//#include "DWSource.h"
#include "BDADVBTSource.h"
#include "ParseLine.h"

 /*#include "DVBInput.h"
#include "DVBSourceTSSplitter.h"
#include "DVBSourceMSDemux.h"
#include "DVBSourceMSDemuxTimeShift.h"
#include "DVBSourceBDA.h"
#include "StateInformation.h"
*/

TVControl::TVControl()
{
	m_pFilterGraph = NULL;
/*	m_pFilterGraph = new FilterGraphManager(m_pAppData);
	m_pControlBar = new ControlBar(m_pAppData, m_pFilterGraph);
	m_pOSD = new OSD(pAppData, m_pFilterGraph, m_pControlBar);
	m_pDVBInput = 0;*/
	log.AddCallback(&g_DWLogWriter);
}
TVControl::~TVControl(void)
{
	std::vector<DWSource *>::iterator it = m_sources.begin();
	for ( ; it != m_sources.end() ; it++ )
	{
		DWSource * source = *it;
		source->Destroy();
		delete source;
	}
	m_sources.clear();

	if (m_pFilterGraph)
	{
		m_pFilterGraph->Destroy();
		delete m_pFilterGraph;
	}

/*	if (m_pDVBInput)
	{
		m_pDVBInput->DestroyAll();
		delete m_pDVBInput;
	}
	m_pFilterGraph->DestroyAll();
	delete m_pFilterGraph;
	delete m_pOSD;
	delete m_pControlBar;*/
}

BOOL TVControl::Initialise()
{
	(log << "Initialising TVControl\n").Write();

	wchar_t file[MAX_PATH];
	swprintf((LPWSTR)&file, L"%s%s", g_pData->application.appPath, L"Keys.xml");
	if (globalKeyMap.LoadKeyMap((LPWSTR)&file) == FALSE)
		return FALSE;

	m_pFilterGraph = new DWGraph();
	m_pFilterGraph->Initialise();
	m_pActiveSource = NULL;

	m_sources.push_back(new BDADVBTSource());
//	m_sources.push_back(new TSFileSource());

	return TRUE;
}
/*
void TVControl::StartTimer()
{
	SetTimer(m_pAppData->hWnd, 996, 100, NULL);
	SetTimer(m_pAppData->hWnd, 997, 1000, NULL);
}
*/
HRESULT TVControl::Exit()
{
/*	if (m_pFilterGraph->IsRecording())
	{
		return_FALSE_SHOWMSG("Cannot exit while recording");
	}*/
//	m_pFilterGraph->Stop();
//	m_pFilterGraph->Cleanup();
//	KillTimer(m_pAppData->hWnd, 996);
//	m_pOSD->HideAll();
	return (DestroyWindow(g_pData->hWnd) ? S_OK : E_FAIL);
}

BOOL TVControl::AlwaysOnTop(int nAlwaysOnTop)
{
	BOOL bResult = FALSE;

	g_pData->values.window.bAlwaysOnTop = ((nAlwaysOnTop == 1) || ((nAlwaysOnTop == 2) && !g_pData->values.window.bAlwaysOnTop));

	if (g_pData->values.window.bAlwaysOnTop)
		bResult = SetWindowPos(g_pData->hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
	else
		bResult = SetWindowPos(g_pData->hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );

	//if (bResult)
	//	m_pOSD->ShowItem("AlwaysOnTop");
	return bResult;
}

BOOL TVControl::Fullscreen(int nFullScreen)
{
	WINDOWPLACEMENT wPlace;
	GetWindowPlacement(g_pData->hWnd, &wPlace);

	g_pData->values.window.bFullScreen = ((nFullScreen == 1) || ((nFullScreen == 2) && !g_pData->values.window.bFullScreen));

	ShowCursor();

	if (g_pData->values.window.bFullScreen)
		wPlace.showCmd = SW_MAXIMIZE;
	else
		wPlace.showCmd = SW_RESTORE;
	//m_pOSD->ShowItem("Fullscreen");
	return SetWindowPlacement(g_pData->hWnd, &wPlace);
}

HRESULT TVControl::Key(int nKeycode, BOOL bShift, BOOL bCtrl, BOOL bAlt)
{
	wchar_t wcfunction[256];
	LPWSTR command = (LPWSTR)&wcfunction;
	if (globalKeyMap.GetFunction(nKeycode, bShift, bCtrl, bAlt, command))
	{
		HRESULT hr = ExecuteCommand(command);
		if (hr == S_FALSE)	//S_FALSE if the ExecuteFunction didn't handle the function
		{
			if (m_pActiveSource)
			{
				hr = m_pActiveSource->ExecuteCommand(command);
			}
		}
		if (hr == S_FALSE)
			(log << "Function '" << command << "' called but has no implementation.\n").Write();
		return hr;
	}
/*
	m_pAppData->KeyPress.Set(nKeycode, bShift, bCtrl, bAlt);
	m_pOSD->ShowItem("KeyPress");
*/
	return S_FALSE;
}

BOOL TVControl::ShowCursor(BOOL bAllowHide)
{
	KillTimer(g_pData->hWnd, TIMER_AUTO_HIDE_CURSOR);
	if (g_pData->values.window.bFullScreen && bAllowHide)
	{
		SetTimer(g_pData->hWnd, TIMER_AUTO_HIDE_CURSOR, 3000, NULL);
	}
	if (!g_pData->application.bCursorVisible)
	{
		::ShowCursor(TRUE);
		g_pData->application.bCursorVisible = TRUE;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL TVControl::HideCursor()
{
	KillTimer(g_pData->hWnd, TIMER_AUTO_HIDE_CURSOR);
	if (g_pData->application.bCursorVisible)
	{
		while (::ShowCursor(FALSE) >= 0);
		g_pData->application.bCursorVisible = FALSE;

		//m_pControlBar->Hide();
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

HRESULT TVControl::ExecuteCommand(LPCWSTR command)
{
	int n1, n2, n3, n4;//, n5, n6;
	//wchar_t buff[256];
	LPWSTR pBuff = (LPWSTR)command;
	LPWSTR pCurr;

	//pBuff = (LPWSTR)&buff;
	//wcscpy(pBuff, strFunction);

//	pCurr = pBuff;
//	skipWhitespaces(pCurr);

	ParseLine parseLine;
	if (parseLine.Parse(pBuff) == FALSE)
		return (log << "Parse error in function: " << command << "\n").Show(E_FAIL);

	(log << "TVControl::ExecuteCommand - " << command << "\n").Write();

	if (parseLine.HasRHS())
		return (log << "Cannot have RHS for function.\n").Show(E_FAIL);

	pCurr = parseLine.LHS.FunctionName;

	if (_wcsicmp(pCurr, L"Exit") == 0)
	{
		if (parseLine.LHS.ParameterCount > 0)
			return (log << "Too many parameters: " << parseLine.LHS.Function << "\n").Show(E_FAIL);
		return Exit();
	}
	else if (_wcsicmp(pCurr, L"Key") == 0)
	{
		if (parseLine.LHS.ParameterCount < 1)
			return (log << "Keycode parameter expected: " << parseLine.LHS.Function << "\n").Show(E_FAIL);
		if (parseLine.LHS.ParameterCount > 4)
			return (log << "Too many parameters: " << parseLine.LHS.Function << "\n").Show(E_FAIL);

		if ((parseLine.LHS.Parameter[0][0] == '\'') && (parseLine.LHS.Parameter[0][2] == '\''))
			n1 = parseLine.LHS.Parameter[0][1];
		else
			n1 = _wtoi(parseLine.LHS.Parameter[0]);

		n2 = 0;
		if (parseLine.LHS.ParameterCount >= 2)
			n2 = _wtoi(parseLine.LHS.Parameter[1]);

		n3 = 0;
		if (parseLine.LHS.ParameterCount >= 3)
			n3 = _wtoi(parseLine.LHS.Parameter[2]);

		n4 = 0;
		if (parseLine.LHS.ParameterCount >= 4)
			n4 = _wtoi(parseLine.LHS.Parameter[3]);

		return Key(n1, n2, n3, n4);
	}
	if (_wcsicmp(pCurr, L"AlwaysOnTop") == 0)
	{
		if (parseLine.LHS.ParameterCount != 1)
			return (log << "Expecting 1 parameter: " << parseLine.LHS.Function << "\n").Show(E_FAIL);

		n1 = _wtoi(parseLine.LHS.Parameter[0]);
		return AlwaysOnTop(n1);
	}
	else if (_wcsicmp(pCurr, L"Fullscreen") == 0)
	{
		if (parseLine.LHS.ParameterCount != 1)
			return (log << "Expecting 1 parameter: " << parseLine.LHS.Function << "\n").Show(E_FAIL);

		n1 = _wtoi(parseLine.LHS.Parameter[0]);
		return Fullscreen(n1);
	}
	else if (_wcsicmp(pCurr, L"SetSource") == 0)
	{
		if (parseLine.LHS.ParameterCount != 1)
			return (log << "Expecting 1 parameter: " << parseLine.LHS.Function << "\n").Show(E_FAIL);

		return SetSource(parseLine.LHS.Parameter[0]);
	}
	
	/*
	if (_wcsicmp(pCurr, "SetChannel") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			n2 = -1;
			if (parseFunction.GetNextParam(pCurr) == TRUE)
				n2 = atoi(pCurr);
			return SetChannel(n1, n2);
		}
	}
	if (_wcsicmp(pCurr, "ManualChannel") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n1 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n2 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			if ((pCurr[0] == 'A') || (pCurr[0] == 'a'))
			{
				pCurr++;
				n4 = 1;
			}
			else
				n4 = 0;
				
			n3 = atoi(pCurr);
			return ManualChannel(n1, n2, n3, n4);
		}
	}
	if (_wcsicmp(pCurr, "NetworkUp") == 0)
	{
		return NetworkUp();
	}
	if (_wcsicmp(pCurr, "NetworkDown") == 0)
	{
		return NetworkDown();
	}
	if (_wcsicmp(pCurr, "ProgramUp") == 0)
	{
		return ProgramUp();
	}
	if (_wcsicmp(pCurr, "ProgramDown") == 0)
	{
		return ProgramDown();
	}
	if (_wcsicmp(pCurr, "LastChannel") == 0)
	{
		return LastChannel();
	}

	if (_wcsicmp(pCurr, "TVPlaying") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return TVPlaying(n1);
		}
	}
	if (_wcsicmp(pCurr, "Recording") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
		}
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			char filename[256];
			strcpy((char*)&filename, pCurr);
			return Recording(n1, (char*)&filename);
		}
		else
		{
			return Recording(n1);
		}
	}
	if (_wcsicmp(pCurr, "RecordingTimer") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return RecordingTimer(n1);
		}
	}
	if (_wcsicmp(pCurr, "RecordingPause") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return RecordingPause(n1);
		}
	}

	if (_wcsicmp(pCurr, "VolumeUp") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return VolumeUp(n1);
		}
	}
	if (_wcsicmp(pCurr, "VolumeDown") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return VolumeDown(n1);
		}
	}
	if (_wcsicmp(pCurr, "SetVolume") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return SetVolume(n1);
		}
	}
	if (_wcsicmp(pCurr, "Mute") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return Mute(n1);
		}
	}

	if (_wcsicmp(pCurr, "VideoDecoderEntry") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return VideoDecoderEntry(n1);
		}
	}
	if (_wcsicmp(pCurr, "AudioDecoderEntry") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return AudioDecoderEntry(n1);
		}
	}
	if (_wcsicmp(pCurr, "ResolutionEntry") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return ResolutionEntry(n1);
		}
	}

	if (_wcsicmp(pCurr, "Resolution") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n1 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n2 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n3 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n4 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n5 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n6 = atoi(pCurr);
			return Resolution(n1, n2, n3, n4, (n5==0), (n6!=0));
		}
	}
	if (_wcsicmp(pCurr, "SetColorControls") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n1 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n2 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n3 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n4 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n5 = atoi(pCurr);
			return SetColorControls(n1, n2, n3, n4, n5);
		}
	}
	*/
	/*
	if (_wcsicmp(pCurr, "Zoom") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return Zoom(n1);
		}
	}
	if (_wcsicmp(pCurr, "ZoomIn") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return ZoomIn(n1);
		}
	}
	if (_wcsicmp(pCurr, "ZoomOut") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return ZoomOut(n1);
		}
	}
	if (_wcsicmp(pCurr, "ZoomMode") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return ZoomMode(n1);
		}
	}
	if (_wcsicmp(pCurr, "AspectRatio") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
			n1 = atoi(pCurr);
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n2 = atoi(pCurr);
			return AspectRatio(n1, n2);
		}
	}

	if (_wcsicmp(pCurr, "ShowFilterProperties") == 0)
	{
		return ShowFilterProperties();
	}

	if (_wcsicmp(pCurr, "TimeShift") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return TimeShift(n1);
		}
	}

	if (_wcsicmp(pCurr, "TimeShiftJump") == 0)
	{
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
			return TimeShiftJump(n1);
		}
	}
	*/
	/*
	else
	{
		n1 = 1;
		if (parseFunction.GetNextParam(pCurr) == TRUE)
		{
			n1 = atoi(pCurr);
		}

		switch (n1)
		{
		case 1:
			//m_pOSD->ShowEntry(GROUPID_NAN, OSD_ON);
			m_pOSD->ShowItem(pBuff);
			break;
		case 2:
			//m_pOSD->ShowEntry(GROUPID_NAN, OSD_TOGGLE);
			m_pOSD->ToggleItem(pBuff);
			break;
		default:
			//m_pOSD->ShowEntry(GROUPID_NAN, OSD_OFF);
			m_pOSD->HideItem(pBuff);
			break;
		}
	}
	*/
	return S_FALSE;
}
/*
int TVControl::GetFunctionType(char* strFunction)
{
	ParseFunctionLine parseFunction;

	int nResult = FT_UNKNOWN;

	//char buff[256];
	char* pBuff;
	//pBuff = (char *)&buff;
	pBuff = strFunction;

	//strcpy(pBuff, strFunction);
	while (pBuff[0] == ' ') pBuff++;

	if (parseFunction.Parse(pBuff) == TRUE)
	{
		if (_wcsicmp(pBuff, "SetChannel") == 0)
			nResult = FT_CHANNEL_CHANGE;
		else if (_wcsicmp(pBuff, "ManualChannel") == 0)
			nResult = FT_CHANNEL_CHANGE;
		else if (_wcsicmp(pBuff, "NetworkUp") == 0)
			nResult = FT_CHANNEL_CHANGE;
		else if (_wcsicmp(pBuff, "NetworkDown") == 0)
			nResult = FT_CHANNEL_CHANGE;
		else if (_wcsicmp(pBuff, "ProgramUp") == 0)
			nResult = FT_CHANNEL_CHANGE;
		else if (_wcsicmp(pBuff, "ProgramDown") == 0)
			nResult = FT_CHANNEL_CHANGE;
		else if (_wcsicmp(pBuff, "LastChannel") == 0)
			nResult = FT_CHANNEL_CHANGE;

		else if (_wcsicmp(pBuff, "TVPlaying") == 0)
			nResult = FT_CHANNEL_CHANGE;
		else if (_wcsicmp(pBuff, "Recording") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "RecordingTimer") == 0)
			nResult = FT_AFTER_START;

		else if (_wcsicmp(pBuff, "VolumeUp") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "VolumeDown") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "SetVolume") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "Mute") == 0)
			nResult = FT_AFTER_START;

		else if (_wcsicmp(pBuff, "VideoDecoderEntry") == 0)
			nResult = FT_BEFORE_START;
		else if (_wcsicmp(pBuff, "AudioDecoderEntry") == 0)
			nResult = FT_BEFORE_START;
		else if (_wcsicmp(pBuff, "ResolutionEntry") == 0)
			nResult = FT_AFTER_START;

		else if (_wcsicmp(pBuff, "Resolution") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "SetColorControls") == 0)
			nResult = FT_AFTER_START;

		else if (_wcsicmp(pBuff, "AlwaysOnTop") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "Fullscreen") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "Zoom") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "ZoomIn") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "ZoomOut") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "ZoomMode") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "AspectRatio") == 0)
			nResult = FT_AFTER_START;

		else if (_wcsicmp(pBuff, "ShowFilterProperties") == 0)
			nResult = FT_AFTER_START;

		else if (_wcsicmp(pBuff, "TimeShift") == 0)
			nResult = FT_AFTER_START;
		else if (_wcsicmp(pBuff, "TimeShiftJump") == 0)
			nResult = FT_AFTER_START;

		else if (_wcsicmp(pBuff, "Key") == 0)
			nResult = FT_ANY;

		else
			nResult = FT_ANY;

		parseFunction.UndoParse();
	}
	return nResult;
}
*/
HRESULT TVControl::SetSource(LPWSTR szSourceName)
{
	LPWSTR pType = NULL;

	std::vector<DWSource *>::iterator it = m_sources.begin();
	for ( ; it < m_sources.end() ; it++ )
	{
		DWSource *source = *it;
		source->GetSourceType(pType);

		if (_wcsicmp(szSourceName, pType) == 0)
		{
			delete pType;

			if (m_pActiveSource)
			{
				//TODO: create a m_pActiveSource->DisconnectFromGraph() method
				m_pActiveSource->Destroy();
				m_pActiveSource = NULL;
			}

			if (SUCCEEDED(source->Initialise(m_pFilterGraph)))
			{
				m_pActiveSource = source;
				return m_pActiveSource->Play();
			}
			return E_FAIL;
		}
	}
	delete pType;

	return S_FALSE;
}
/*
BOOL TVControl::SetChannel(int nNetwork, int nProgram)
{
	if (m_pFilterGraph->IsRecording())
	{
		return_FALSE_SHOWERROR("Cannot change channel while recording");
	}
	if ((nNetwork <= 0) || (nNetwork >= m_pAppData->tvChannels.MaxNetworkCount))
		return FALSE;
	if (m_pAppData->tvChannels.Networks[nNetwork].bValid == FALSE)
		return FALSE;

	long freq, bwid;
	long pnum, vpid, apid;
	BOOL ac3;

	m_pOSD->HideItem("Channel");

	if (nNetwork == m_pAppData->values.currTVNetwork)
	{
		if ((nProgram == m_pAppData->values.currTVProgram) || (nProgram == -1))
		{
			//Show OSD if selecting same channel
			m_pOSD->ShowItem("Channel");

			return TRUE;
		}
		if (nProgram == -2)
		{
			//Select next program
			nProgram = m_pAppData->values.currTVProgram;
			do
			{
				nProgram++;
				if (nProgram >= m_pAppData->tvChannels.Networks[nNetwork].MaxProgramCount)
					nProgram = 0;
			}
			while (m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].bValid == FALSE);
		}
	}
	if ((nProgram < 0) || (nProgram >= m_pAppData->tvChannels.Networks[nNetwork].MaxProgramCount))
		nProgram = m_pAppData->tvChannels.Networks[nNetwork].DefaultProgram;
	if ((nProgram < 0) || (nProgram >= m_pAppData->tvChannels.Networks[nNetwork].MaxProgramCount))
		nProgram = 1;

	if (m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].bValid == FALSE)
		return FALSE;

	freq = m_pAppData->tvChannels.Networks[nNetwork].Frequency;
	bwid = m_pAppData->tvChannels.Networks[nNetwork].Bandwidth;
	pnum = m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].ProgramNumber;
	vpid = m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].VideoPid;
	apid = m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].AudioPid;
	ac3  = m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].AudioPidAC3;

	while (1)
	{
		m_pAppData->bChannelLocked = FALSE;
		m_pAppData->bBuilt = FALSE;
		if ((m_pAppData->values.currTVNetwork != nNetwork) || (m_pAppData->values.currTVProgram != nProgram))
		{
			m_pAppData->values.lastTVNetwork = m_pAppData->values.currTVNetwork;
			m_pAppData->values.lastTVProgram = m_pAppData->values.currTVProgram;
			m_pAppData->values.currTVNetwork = nNetwork;
			m_pAppData->values.currTVProgram = nProgram;
		}

		m_pOSD->ShowItem("Channel");

		m_pAppData->RestoreMarkedChanges();
		m_pAppData->StoreGlobalValues();

		if (m_pAppData->values.selectedVideoDecoder != m_pAppData->VideoDecoders.GetCurrent())
			VideoDecoderEntry(m_pAppData->values.selectedVideoDecoder);
		if (m_pAppData->values.selectedAudioDecoder != m_pAppData->AudioDecoders.GetCurrent())
			AudioDecoderEntry(m_pAppData->values.selectedAudioDecoder);

		//Do BEFORE functions
		for (int prefunc=0 ; prefunc<m_pAppData->tvChannels.Networks[nNetwork].FunctionsCount ; prefunc++)
		{
			int type = GetFunctionType(m_pAppData->tvChannels.Networks[nNetwork].Functions[prefunc]);
			if (type == FT_BEFORE_START)
				ExecuteCommand(m_pAppData->tvChannels.Networks[nNetwork].Functions[prefunc]);
		}
		for ( prefunc=0 ; prefunc<m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].FunctionsCount ; prefunc++)
		{
			int type = GetFunctionType(m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].Functions[prefunc]);
			if (type == FT_BEFORE_START)
				ExecuteCommand(m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].Functions[prefunc]);
		}

		if (!m_pFilterGraph->Stop())
			return_FALSE_SHOWERROR("Error Stopping");
		if (!m_pFilterGraph->Cleanup())
			return_FALSE_SHOWERROR("Error Unbuilding");
		if (!m_pDVBInput->LockChannel(freq, bwid))
			return_FALSE_SHOWERROR("Error Locking Channel");
		if (!m_pDVBInput->SetPid(pnum,vpid,apid,ac3))
			return_FALSE_SHOWERROR("Error Setting PID's");
		m_pAppData->bChannelLocked = TRUE;
		static int buildCount = 0;
		buildCount = 0;
		if (!m_pFilterGraph->Build(buildCount))
			return_FALSE_SHOWERROR("Error Building");
		if (buildCount >= 1)
			break;
		m_pAppData->bBuilt = TRUE;
		if (!m_pFilterGraph->Start())
			return_FALSE_SHOWERROR("Error Playing");
		if (!m_pFilterGraph->RefreshVideoPosition())
			return_FALSE_SHOWERROR("Error Refreshing Video");

		buildCount++;

		m_pOSD->GetStateInformation()->NaNCollClear();

		//Do AFTER functions
		for (int postfunc=0 ; postfunc<m_pAppData->tvChannels.Networks[nNetwork].FunctionsCount ; postfunc++)
		{
			int type = GetFunctionType(m_pAppData->tvChannels.Networks[nNetwork].Functions[postfunc]);
			if ((type == FT_AFTER_START) || (type == FT_ANY))
				ExecuteCommand(m_pAppData->tvChannels.Networks[nNetwork].Functions[postfunc]);
		}
		for ( postfunc=0 ; postfunc<m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].FunctionsCount ; postfunc++)
		{
			int type = GetFunctionType(m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].Functions[postfunc]);
			if ((type == FT_AFTER_START) || (type == FT_ANY))
				ExecuteCommand(m_pAppData->tvChannels.Networks[nNetwork].Programs[nProgram].Functions[postfunc]);
		}

		m_pFilterGraph->Mute(m_pAppData->values.bMute);

		m_pAppData->MarkChanges();

		return TRUE;
	}
	m_pFilterGraph->Stop();

	return FALSE;
}

BOOL TVControl::ManualChannel(int nFrequency, int nVPid, int nAPid, BOOL bAC3)
{
	int nPNum = 0;
	int Network = m_pAppData->tvChannels.FindNetwork(nFrequency);
	int Program = m_pAppData->tvChannels.FindProgram(nFrequency, nVPid, nAPid);
	if ((Network >= 0) && (Program >= 0))
		return SetChannel(Network, Program);

	if (m_pFilterGraph->IsRecording())
	{
		return_FALSE_SHOWERROR("Cannot change channel while recording");
	}

	while (1)
	{
		m_pAppData->bChannelLocked = FALSE;

		m_pAppData->values.lastTVNetwork = m_pAppData->values.currTVNetwork;
		m_pAppData->values.lastTVProgram = m_pAppData->values.currTVProgram;
		m_pAppData->values.currTVNetwork = 0;
		m_pAppData->values.currTVProgram = 0;

		m_pOSD->ShowItem("Channel");

		if (m_pAppData->values.selectedVideoDecoder != m_pAppData->VideoDecoders.GetCurrent())
			VideoDecoderEntry(m_pAppData->values.selectedVideoDecoder);
		if (m_pAppData->values.selectedAudioDecoder != m_pAppData->AudioDecoders.GetCurrent())
			AudioDecoderEntry(m_pAppData->values.selectedAudioDecoder);

		if (!m_pFilterGraph->Stop()						) return_FALSE_SHOWERROR("Error Stopping");
		if (!m_pDVBInput->LockChannel(nFrequency, 7)	) return_FALSE_SHOWERROR("Error Locking Channel");
		if (!m_pDVBInput->SetPid(nPNum,nVPid,nAPid,bAC3)) return_FALSE_SHOWERROR("Error Setting PID's");
		m_pAppData->bChannelLocked = TRUE;
		if (!m_pFilterGraph->Build()					) return FALSE;//SHOWERROR("Error Playing");
		//if (!m_pFilterGraph->Start()					) return FALSE;
		//if (!m_pFilterGraph->RefreshVideoPosition()	) return FALSE;//SHOWERROR("Error Refreshing Video");

		return TRUE;
	}
	m_pFilterGraph->Stop();

	return FALSE;
}

BOOL TVControl::NetworkUp()
{
	int value = m_pAppData->values.currTVNetwork;
	do
	{
		value++;
		if (value >= m_pAppData->tvChannels.MaxNetworkCount)
			value = 0;
	} while (m_pAppData->tvChannels.Networks[value].bValid == FALSE);
	return SetChannel(value, -1);
}

BOOL TVControl::NetworkDown()
{
	int value = m_pAppData->values.currTVNetwork;
	do
	{
		value--;
		if (value < 0)
			value = m_pAppData->tvChannels.MaxNetworkCount-1;
	} while (m_pAppData->tvChannels.Networks[value].bValid == FALSE);
	return SetChannel(value, -1);
}

BOOL TVControl::ProgramUp()
{
	int network = m_pAppData->values.currTVNetwork;
	while (m_pAppData->tvChannels.Networks[network].bValid == FALSE)
	{
		network++;
		if (network >= m_pAppData->tvChannels.MaxNetworkCount)
			network = 0;
	}

	int value = m_pAppData->values.currTVProgram;
	do
	{
		value++;
		if (value >= m_pAppData->tvChannels.Networks[network].MaxProgramCount)
			value = 0;
	} while (m_pAppData->tvChannels.Networks[network].Programs[value].bValid == FALSE);
	return SetChannel(network, value);
}

BOOL TVControl::ProgramDown()
{
	int network = m_pAppData->values.currTVNetwork;
	while (m_pAppData->tvChannels.Networks[network].bValid == FALSE)
	{
		network++;
		if (network >= m_pAppData->tvChannels.MaxNetworkCount)
			network = 0;
	}

	int value = m_pAppData->values.currTVProgram;
	do
	{
		value--;
		if (value <= 0)
			value = m_pAppData->tvChannels.Networks[network].MaxProgramCount-1;
	} while (m_pAppData->tvChannels.Networks[network].Programs[value].bValid == FALSE);
	return SetChannel(network, value);
}

BOOL TVControl::LastChannel()
{
	return SetChannel(m_pAppData->values.lastTVNetwork, m_pAppData->values.lastTVProgram);
}


BOOL TVControl::TVPlaying(int nPlaying)
{
	if ((nPlaying == 1) || ((nPlaying == 2) && !m_pFilterGraph->IsPlaying()))
	{
		if (!m_pFilterGraph->Start()				) return FALSE;
		if (!m_pFilterGraph->RefreshVideoPosition()	) return FALSE;
		return TRUE;
	}
	else
	{
		if (m_pFilterGraph->IsRecording())
		{
			return_FALSE_SHOWERROR("Cannot stop playing while recording");
		}

		return m_pFilterGraph->Stop();
	}
}

BOOL TVControl::Recording(int nRecording, char* filename)
{
	if ((nRecording == 1) || ((nRecording == 2) && !m_pFilterGraph->IsRecording()))
	{
		if (m_pAppData->bChannelLocked == FALSE)
		{
			return_FALSE_SHOWERROR("Can't Record. No channel selected");
		}

		BOOL bResult;
		if (filename != NULL)
		{
			bResult = m_pFilterGraph->StartRecording(filename);
		}
		else
		{
			char realFilename[512];
			m_pAppData->ReplaceTokens(m_pAppData->captureFileName, (char*)&realFilename, 512);
			bResult = m_pFilterGraph->StartRecording(realFilename);
		}

		if (bResult)
		{
			m_pAppData->bRecording = TRUE;
			m_pAppData->bRecordingPaused = FALSE;
			m_pOSD->ShowItem("Recording");
			return TRUE;
		}
	}
	else
	{
		KillTimer(m_pAppData->hWnd, 990);
		m_pAppData->recordingTimeLeft = 0;
		m_pAppData->bRecording = FALSE;
		m_pAppData->bRecordingPaused = FALSE;
		if (m_pFilterGraph->StopRecording())
		{
			m_pOSD->ShowItem("RecordingStopped");
			return TRUE;
		}
	}
	return FALSE;
}

BOOL TVControl::RecordingTimer(int nAddTime)
{
	if (m_pFilterGraph->IsRecording())
	{
		m_pAppData->recordingTimeLeft += nAddTime*60;
		m_pOSD->ShowItem("Recording");
		SetTimer(m_pAppData->hWnd, 990, 1000, NULL);
		return TRUE;
	}
	else
	{
		m_pAppData->recordingTimeLeft = 0;
		BOOL bResult = Recording(1);
		SetTimer(m_pAppData->hWnd, 990, 1000, NULL);
		return bResult;
	}
}

BOOL TVControl::RecordingPause(int nPause)
{
	if ((nPause == 1) || ((nPause == 2) && !m_pFilterGraph->IsRecordingPaused()))
	{
		if (m_pFilterGraph->IsRecording() == FALSE)
		{
			return_FALSE_SHOWERROR("Can't Pause Recording when not recording");
		}

		if (m_pFilterGraph->PauseRecording(TRUE))
		{
			m_pAppData->bRecordingPaused = TRUE;
			m_pOSD->ShowItem("RecordingPaused");
			return TRUE;
		}
	}
	else
	{
		if (m_pFilterGraph->IsRecording())
		{
			if (m_pFilterGraph->PauseRecording(FALSE))
			{
				m_pAppData->bRecordingPaused = FALSE;
				m_pOSD->ShowItem("RecordingUnpaused");
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL TVControl::VolumeUp(int value)
{
	BOOL bResult;
	int volume = m_pFilterGraph->GetVolume();
	bResult = m_pFilterGraph->SetVolume(volume+value);
	if (bResult)
	{
		m_pOSD->ShowItem("Volume");
	}
	return bResult;
}

BOOL TVControl::VolumeDown(int value)
{
	BOOL bResult;
	int volume = m_pFilterGraph->GetVolume();
	bResult = m_pFilterGraph->SetVolume(volume-value);
	if (bResult)
	{
		m_pOSD->ShowItem("Volume");
	}
	return bResult;
}

BOOL TVControl::SetVolume(int value)
{
	BOOL bResult = m_pFilterGraph->SetVolume(value);
	m_pOSD->ShowItem("Volume");
	return bResult;
}

BOOL TVControl::Mute(int nMute)
{
	m_pAppData->values.bMute = ((nMute == 1) || ((nMute == 2) && !m_pAppData->values.bMute));
	if (m_pFilterGraph->Mute(m_pAppData->values.bMute))
	{
		m_pOSD->ShowItem("Mute");
		return TRUE;
	}
	return FALSE;
}

BOOL TVControl::VideoDecoderEntry(int nIndex)
{
	BOOL bResult;
	if (nIndex < 0)
		bResult = m_pAppData->VideoDecoders.Next();
	else
		bResult = m_pAppData->VideoDecoders.SetCurrent(nIndex);
	if (bResult)
		m_pOSD->ShowItem("VideoDecoder");
	m_pAppData->values.selectedVideoDecoder = m_pAppData->VideoDecoders.GetCurrent();
	return bResult;
}

BOOL TVControl::AudioDecoderEntry(int nIndex)
{
	BOOL bResult;
	if (nIndex < 0)
		bResult = m_pAppData->AudioDecoders.Next();
	else
		bResult = m_pAppData->AudioDecoders.SetCurrent(nIndex);
	if (bResult)
		m_pOSD->ShowItem("AudioDecoder");
	m_pAppData->values.selectedAudioDecoder = m_pAppData->AudioDecoders.GetCurrent();
	return bResult;
}

BOOL TVControl::ResolutionEntry(int nIndex)
{
	if (m_pAppData->values.bFullScreen)
		return FALSE;
	BOOL bResult;
	if (nIndex < 0)
		bResult = m_pAppData->resolutions.Next();
	else
		bResult = m_pAppData->resolutions.SetCurrent(nIndex);
	if (bResult)
		Resolution(m_pAppData->resolutions.Current()->left,
				   m_pAppData->resolutions.Current()->top,
				   m_pAppData->resolutions.Current()->width,
				   m_pAppData->resolutions.Current()->height,
				   m_pAppData->resolutions.Current()->move,
				   m_pAppData->resolutions.Current()->size);
	return TRUE;
}
BOOL TVControl::Resolution(int nLeft, int nTop, int nWidth, int nHeight, BOOL bMove, BOOL bResize)
{
	if (m_pAppData->values.bFullScreen)
		return FALSE;
	BOOL bResult = FALSE;
	RECT gwr, gcr;
	GetWindowRect(m_pAppData->hWnd, &gwr);
	GetClientRect(m_pAppData->hWnd, &gcr);
	int left = nLeft - (((gwr.right-gwr.left) - (gcr.right-gcr.left)) / 2);
	int top = nTop - (((gwr.bottom-gwr.top) - (gcr.bottom-gcr.top)) / 2);
	int width = nWidth + ((gwr.right-gwr.left) - (gcr.right-gcr.left));
	int height = nHeight + ((gwr.bottom-gwr.top) - (gcr.bottom-gcr.top));

	char text[256] = "";
	if (bMove && bResize)
	{
		sprintf((char *) &text, "Move to (%i, %i)  Resize to (%i, %i)", m_pAppData->resolutions.Current()->left, m_pAppData->resolutions.Current()->top, m_pAppData->resolutions.Current()->width, m_pAppData->resolutions.Current()->height);
		bResult = SetWindowPos(m_pAppData->hWnd, NULL, left, top, width, height, SWP_NOZORDER);
	}
	else if (bMove)
	{
		sprintf((char *) &text, "Move to (%i, %i)", m_pAppData->resolutions.Current()->left, m_pAppData->resolutions.Current()->top);
		bResult = SetWindowPos(m_pAppData->hWnd, NULL, left, top, width, height, SWP_NOZORDER | SWP_NOSIZE);
	}
	else if (bResize)
	{
		sprintf((char *) &text, "Resize to (%i, %i)", m_pAppData->resolutions.Current()->width, m_pAppData->resolutions.Current()->height);
		bResult = SetWindowPos(m_pAppData->hWnd, NULL, left, top, width, height, SWP_NOZORDER | SWP_NOMOVE);
	}
	if (bResult)
		m_pOSD->ShowItem("Resolution");
	return bResult;
}

BOOL TVControl::SetColorControls(int nBrightness, int nContrast, int nHue, int nSaturation, int nGamma)
{
	if (nBrightness <     0) nBrightness =     0;
	if (nBrightness > 10000) nBrightness = 10000;
	if (nContrast   <     0) nContrast   =     0;
	if (nContrast   > 20000) nContrast   = 20000;
	if (nHue        <  -180) nHue        =  -180;
	if (nHue        >   180) nHue        =   180;
	if (nSaturation <     0) nSaturation =     0;
	if (nSaturation > 20000) nSaturation = 20000;
	if (nGamma      <     1) nGamma      =     1;
	if (nGamma      >   500) nGamma      =   500;

	BOOL bResult = m_pFilterGraph->SetColorControls(nBrightness, nContrast, nHue, nSaturation, nGamma);
	if (bResult)
		m_pOSD->ShowItem("ColorControls");
	return bResult;
}

BOOL TVControl::Zoom(int percentage)
{
	m_pAppData->values.zoom = percentage;
	m_pFilterGraph->RefreshVideoPosition();
	m_pOSD->ShowItem("Zoom");
	return TRUE;
}

BOOL TVControl::ZoomIn(int percentage)
{
	m_pAppData->values.zoom += percentage;
	m_pFilterGraph->RefreshVideoPosition();
	m_pOSD->ShowItem("ZoomIn");
	return TRUE;
}

BOOL TVControl::ZoomOut(int percentage)
{
	if (m_pAppData->values.zoom-percentage <= 0)
		return FALSE;
	m_pAppData->values.zoom -= percentage;
	m_pFilterGraph->RefreshVideoPosition();
	m_pOSD->ShowItem("ZoomOut");
	return TRUE;
}

BOOL TVControl::ZoomMode(int mode)
{
	int maxModes = 1;
	if (mode > maxModes)
		return FALSE;
	if (mode < 0)
	{
		mode = m_pAppData->values.zoomMode + 1;
		if (mode > maxModes)
			mode = 0;
	}
	m_pAppData->values.zoomMode = mode;
	m_pFilterGraph->RefreshVideoPosition();
	m_pOSD->ShowItem("ZoomMode");
	return TRUE;
}

BOOL TVControl::AspectRatio(int width, int height)
{
	if ((width <= 0) || (height <= 0))
		return FALSE;
	m_pAppData->values.aspectRatioWidth = width;
	m_pAppData->values.aspectRatioHeight = height;
	m_pFilterGraph->RefreshVideoPosition();
	return TRUE;
}

BOOL TVControl::ShowFilterProperties()
{
	ShowCursor(FALSE);
	BOOL bResult = m_pFilterGraph->ShowFilterProperties();
	ShowCursor();
	return bResult;
}

BOOL TVControl::TimeShift(int nMode)
{
	BOOL bResult;
	if (((nMode == 1) || ((nMode == 2) && !m_pDVBInput->GetTimeShiftMode())))
		bResult = (m_pDVBInput->TimeShiftPlay() != 0);
	else
		bResult = (m_pDVBInput->TimeShiftPause() == 0);
	if (bResult)
		m_pOSD->ShowItem("TimeShift");
	return bResult;
}

BOOL TVControl::TimeShiftJump(int nSeconds)
{
	if (nSeconds == 0)
		return false;
	if (m_pDVBInput->TimeShiftJump(nSeconds))
	{
		m_pAppData->TimeShiftJump = nSeconds;
		m_pOSD->ShowItem("TimeShiftJump");
		return true;
	}
	else
	{
		return_FALSE_SHOWERROR("Seek Failed");
	}
}
*/

/*
BOOL TVControl::ControlBarMouseDown(int x, int y)
{
	if (m_pControlBar->MouseDown(x, y))
	{
		char function[256];
		if (m_pControlBar->GetFunction((char*)&function))
			return ExecuteCommand(function);
	}
	return FALSE;
}

BOOL TVControl::ControlBarMouseUp(int x, int y)
{
	m_pControlBar->MouseUp(x, y);
	return FALSE;
}

BOOL TVControl::ControlBarMouseMove(int x, int y)
{
	return m_pControlBar->MouseMove(x, y);
}

*/

void TVControl::Timer(int wParam)
{
	switch (wParam)
	{
	/*case 990:	//Every second while recording
		if (m_pAppData->recordingTimeLeft == 1)
		{
			m_pTv->Recording(0);
		}
		if (m_pAppData->recordingTimeLeft > 1)
		{
			m_pAppData->recordingTimeLeft--;
		}
		break;
	case 996:	//every 100ms the whole time the program is running.
		m_pTv->m_pOSD->RepaintOSD(TRUE);
		break;
	case 997:	//Every second the whole time the program is running.
		m_pTv->m_pOSD->Update();
		break;
	case 998:	//Every 30 seconds to keep power saving coming on.
		SetThreadExecutionState(ES_DISPLAY_REQUIRED);
		break;*/
	case TIMER_AUTO_HIDE_CURSOR:	//3 seconds after mouse movement
		KillTimer(g_pData->hWnd, TIMER_AUTO_HIDE_CURSOR);
		if (g_pData->values.window.bFullScreen)
			HideCursor();
		break;
	default:	//values from 1000 up can be used by the osd.
		//m_pOSD->Timer(wParam);
		break;
	}
}

