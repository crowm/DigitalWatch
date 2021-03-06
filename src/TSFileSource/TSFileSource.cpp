/**
 *	TSFileSource.cpp
 *	Copyright (C) 2004 Nate
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
#include "TSFileSource.h"
#include "Globals.h"
#include "GlobalFunctions.h"
#include "LogMessage.h"
#include <initguid.h>
#include "TSFileSourceGuids.h"
#include "MediaFormats.h"
#include <ks.h>
#include <ksmedia.h>
#include <bdamedia.h>
#include "ITSFileSource.h"

#include <process.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>

//////////////////////////////////////////////////////////////////////
// TSFileSource
//////////////////////////////////////////////////////////////////////

TSFileSource::TSFileSource(LogMessageCallback *callback) : m_strSourceType(L"TSFileSource")
{
	m_bInitialised = FALSE;
	m_pDWGraph = NULL;
	m_pFileName = NULL;
	g_pOSD->Data()->AddList(&streamList);
	g_pOSD->Data()->AddList(&filterList);
	m_bTimeShiftService = FALSE;
	m_lLocalStartTime = 0;
}

TSFileSource::~TSFileSource()
{
	Destroy();

	if (m_pFileName)
		delete[] m_pFileName;
}

void TSFileSource::SetLogCallback(LogMessageCallback *callback)
{
	DWSource::SetLogCallback(callback);
	streamList.SetLogCallback(callback);
	filterList.SetLogCallback(callback);

	graphTools.SetLogCallback(callback);
}

LPWSTR TSFileSource::GetSourceType()
{
	return m_strSourceType;
}

DWGraph *TSFileSource::GetFilterGraph(void)
{
	return m_pDWGraph;
}

IGraphBuilder *TSFileSource::GetGraphBuilder(void)
{
	return m_piGraphBuilder;
}

BOOL TSFileSource::IsInitialised()
{
	return m_bInitialised;
}

HRESULT TSFileSource::Initialise(DWGraph* pFilterGraph)
{
	m_bInitialised = TRUE;

	(log << "Initialising TSFileSource Source\n").Write();
	LogMessageIndent indent(&log);

	HRESULT hr;

	m_pDWGraph = pFilterGraph;

	if (!m_piGraphBuilder)
		if FAILED(hr = m_pDWGraph->QueryGraphBuilder(&m_piGraphBuilder))
			return (log << "Failed to get graph: " << hr <<"\n").Write(hr);

	streamList.Initialise(m_piGraphBuilder);
	for (int i = 0; i < g_pOSD->Data()->GetListCount(streamList.GetListName()); i++)
	{
		if (g_pOSD->Data()->GetListFromListName(streamList.GetListName()) != &streamList)
		{
			(log << "Streams List is not the same, Rotating List\n").Write();
			g_pOSD->Data()->RotateList(g_pOSD->Data()->GetListFromListName(streamList.GetListName()));
			continue;
		}
		(log << "Streams List found to be the same\n").Write();
		break;
	};

	filterList.Initialise(m_piGraphBuilder);
	for (int i = 0; i < g_pOSD->Data()->GetListCount(filterList.GetListName()); i++)
	{
		if (g_pOSD->Data()->GetListFromListName(filterList.GetListName()) != &filterList)
		{
			(log << "Filter List is not the same, Rotating List\n").Write();
			g_pOSD->Data()->RotateList(g_pOSD->Data()->GetListFromListName(filterList.GetListName()));
			continue;
		}
		(log << "Filter List found to be the same\n").Write();
		break;
	};


	wchar_t file[MAX_PATH];

	StringCchPrintfW((LPWSTR)&file, MAX_PATH, L"%sTSFileSource\\Keys.xml", g_pData->application.appPath);
	if FAILED(hr = m_sourceKeyMap.LoadFromFile((LPWSTR)&file))
		return hr;

	// Start the background thread for updating statistics
	if FAILED(hr = StartThread())
		return (log << "Failed to start background thread: " << hr << "\n").Write(hr);

	indent.Release();
	(log << "Finished Initialising TSFileSource Source\n").Write();

	return S_OK;
}

HRESULT TSFileSource::Destroy()
{
	(log << "Destroying TSFileSource Source\n").Write();
	LogMessageIndent indent(&log);

	g_pTv->HideOSDItem(L"Position");

	HRESULT hr;

	if (m_pDWGraph)
	{
		if (m_pDWGraph->IsPlaying())
			SaveResumePosition();

		if FAILED(hr = UnloadFilters())
			(log << "Failed to unload filters\n").Write();

		(log << "Saving Resume Times\n").Write();
		m_pDWGraph->SaveSettings();

	}

	m_pDWGraph = NULL;

	m_piGraphBuilder.Release();
	streamList.Destroy();
	filterList.Destroy();

	indent.Release();
	(log << "Finished Destroying TSFileSource Source\n").Write();

	return S_OK;
}

void TSFileSource::DestroyFilter(CComPtr <IBaseFilter> &pFilter)
{
	if (pFilter)
	{
		m_piGraphBuilder->RemoveFilter(pFilter);
		pFilter.Release();
	}
}

HRESULT TSFileSource::ExecuteCommand(ParseLine* command)
{
	(log << "TSFileSource::ExecuteCommand - " << command->LHS.Function << "\n").Write();
	LogMessageIndent indent(&log);

	int n1, n2, n3, n4;
	LPWSTR pCurr = command->LHS.FunctionName;

	if (_wcsicmp(pCurr, L"LoadFile") == 0)
	{
		if (command->LHS.ParameterCount > 1)
			return (log << "Expecting 0 or 1 parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		if (command->LHS.ParameterCount == 1)
			return LoadFile(command->LHS.Parameter[0]);
		else
			return LoadFile(NULL);
	}
	else if (_wcsicmp(pCurr, L"PlayPause") == 0)
	{
		if (command->LHS.ParameterCount != 0)
			return (log << "Expecting 0 parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		return PlayPause();
	}
	else if (_wcsicmp(pCurr, L"Seek") == 0)
	{
		if (command->LHS.ParameterCount != 1)
			return (log << "Expecting 1 parameter: " << command->LHS.Function << "\n").Show(E_FAIL);

		n1 = StringToLong(command->LHS.Parameter[0]);

		return Skip(n1);
	}
	else if (_wcsicmp(pCurr, L"SeekTo") == 0)
	{
		if (command->LHS.ParameterCount != 1)
			return (log << "Expecting no parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		n1 = StringToLong(command->LHS.Parameter[0]);

		return SeekTo(n1);
	}
	if (_wcsicmp(pCurr, L"SetStream") == 0)
	{
		if (command->LHS.ParameterCount != 1)
			return (log << "Expecting 1 parameter: " << command->LHS.Function << "\n").Show(E_FAIL);

		n1 = StringToLong(command->LHS.Parameter[0]);

		return SetStream(n1);
	}
	else if (_wcsicmp(pCurr, L"GetStreamList") == 0)
	{
		if (command->LHS.ParameterCount != 0)
			return (log << "Expecting 0 parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		return GetStreamList();
	}
	if (_wcsicmp(pCurr, L"ShowFilter") == 0)
	{
		if (command->LHS.ParameterCount != 1)
			return (log << "Expecting 1 parameter: " << command->LHS.Function << "\n").Show(E_FAIL);

		return ShowFilter(command->LHS.Parameter[0]);
	}
	else if (_wcsicmp(pCurr, L"GetFilterList") == 0)
	{
		if (command->LHS.ParameterCount != 0)
			return (log << "Expecting 0 parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		return GetFilterList();
	}
	else if (_wcsicmp(pCurr, L"MinimiseDisplay") == 0)
	{
		if (command->LHS.ParameterCount != 0)
			return (log << "Expecting 0 parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		g_pTv->MinimiseScreen();
		return CloseDisplay();
	}
	else if (_wcsicmp(pCurr, L"CloseDisplay") == 0)
	{
		if (command->LHS.ParameterCount != 0)
			return (log << "Expecting 0 parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		return CloseDisplay();
	}
	else if (_wcsicmp(pCurr, L"OpenDisplay") == 0)
	{
		if (command->LHS.ParameterCount != 0)
			return (log << "Expecting 0 parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		return OpenDisplay();
	}
	else if (_wcsicmp(pCurr, L"SetMediaTypeDecoder") == 0)
	{
		if (command->LHS.ParameterCount <= 0)
			return (log << "Expecting 2 or 4 parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		if (command->LHS.ParameterCount > 4)
			return (log << "Too many parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		if (!command->LHS.Parameter[1] && !command->LHS.Parameter[2])
			return (log << "Expecting 3 or 4 valid parameters: " << command->LHS.Function << "\n").Show(E_FAIL);

		//Get the media type index value
		n1 = StringToLong(command->LHS.Parameter[0]);

		if (n1)
			n1--;

		//Save the current decoder selection
		LPWSTR decoder = NULL;
		LPWSTR pTemp = m_pDWGraph->GetMediaTypeDecoder(n1);
		if(pTemp)
			strCopy(decoder, pTemp);

		//check if new decoder has correct media type specified
		if (wcsstr(command->LHS.Parameter[2], command->LHS.Parameter[3]) == NULL)
		{
			(log << "Error, now restoring the previous Decoder: " << decoder << " for Media type: "<< command->LHS.Parameter[3]<< "\n").Show();
			g_pOSD->Data()->SetItem(L"MediaTypeDecoder", decoder);
			return (log << "Unable to match MediaTypes between: " << command->LHS.Parameter[2] << " with "<< command->LHS.Parameter[3]<< "\n").Show(E_FAIL);
		}

		if (g_pData->settings.application.decoderTest)
		{

			g_pOSD->Data()->SetItem(L"warnings", L"Decoder testing in progress, Please wait a few seconds.");
			g_pTv->ShowOSDItem(L"Warnings", 2);
			//Set the new decoder in the list
			m_pDWGraph->SetMediaTypeDecoder(n1, command->LHS.Parameter[1], FALSE);

			//if requested we can test the decoder connection
			if (command->LHS.Parameter[3])
				if FAILED(TestDecoderSelection(command->LHS.Parameter[3]))
				{
					g_pOSD->Data()->SetItem(L"warnings", L"Decoder Test Failed, Please check the log for details.");
					g_pTv->ShowOSDItem(L"Warnings", 5);
					(log << "Error, now restoring the previous Decoder: " << decoder << " for Media type: "<< command->LHS.Parameter[3]<< "\n").Show();
					g_pOSD->Data()->SetItem(L"MediaTypeDecoder", decoder);
					m_pDWGraph->SetMediaTypeDecoder(n1, decoder, FALSE);
					OpenDisplay(TRUE);
					delete[] decoder;
					return (log << "Unable to connect the Selected Decoder: " << command->LHS.Parameter[1] << " using Media type: "<< command->LHS.Parameter[3]<< "\n").Show(E_FAIL);
				}

			g_pOSD->Data()->SetItem(L"warnings", L"Decoder testing completed Ok.");
			g_pTv->ShowOSDItem(L"Warnings", 2);
		}

		(log << "Setting the Selected Decoder: " << command->LHS.Parameter[1] << " for Media type: "<< command->LHS.Parameter[3]<< "\n").Show();
		g_pOSD->Data()->SetItem(L"MediaTypeDecoder", command->LHS.Parameter[1]);
		m_pDWGraph->SetMediaTypeDecoder(n1, command->LHS.Parameter[1]);
		delete[] decoder;
		return OpenDisplay(TRUE);
	}

	//Just referencing these variables to stop warnings.
	n1 = 0;
	n2 = 0;
	n3 = 0;
	n4 = 0;
	return S_FALSE;
}

BOOL TSFileSource::CanLoad(LPWSTR pCmdLine)
{
	long length = wcslen(pCmdLine);
	if ((length >= 9) && (_wcsicmp(pCmdLine+length-9, L".tsbuffer") == 0))
	{
		return TRUE;
	}
	if ((length >= 4) && (_wcsicmp(pCmdLine+length-4, L".mpg") == 0))
	{
		return TRUE;
	}
	if ((length >= 4) && (_wcsicmp(pCmdLine+length-4, L".vob") == 0))
	{
		return TRUE;
	}
	if ((length >= 3) && (_wcsicmp(pCmdLine+length-3, L".ts") == 0))
	{
		return TRUE;
	}
	return FALSE;
}

HRESULT TSFileSource::Load(LPWSTR pCmdLine)
{
	if (!m_pDWGraph)
		return (log << "Filter graph not set in TSFileSource::Play\n").Write(E_FAIL);

	HRESULT hr;

	if (!m_piGraphBuilder)
		if FAILED(hr = m_pDWGraph->QueryGraphBuilder(&m_piGraphBuilder))
			return (log << "Failed to get graph: " << hr <<"\n").Write(hr);

	return LoadFile(pCmdLine);
}

HRESULT TSFileSource::FastLoad(LPWSTR pCmdLine, DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt)
{
	if (!m_pDWGraph)
		return (log << "Filter graph not set in TSFileSource::Play\n").Write(E_FAIL);

	HRESULT hr;

	if (!m_piGraphBuilder)
		if FAILED(hr = m_pDWGraph->QueryGraphBuilder(&m_piGraphBuilder))
			return (log << "Failed to get graph: " << hr <<"\n").Write(hr);

	return LoadFile(pCmdLine, pService, pmt);
}

HRESULT TSFileSource::ReLoad(LPWSTR pCmdLine)
{
	return ReLoadFile(pCmdLine);
}

void TSFileSource::ThreadProc()
{
	NormalThread Normal;

	while (!ThreadIsStopping())
	{
		UpdateData();
		Sleep(100);
	}
}

HRESULT TSFileSource::QueryTransportStreamPin(IPin** piPin)
{
	if (!piPin || !m_pTSFileSource)
		return E_INVALIDARG;

	HRESULT hr;

	if FAILED(hr = graphTools.FindAnyPin(m_pTSFileSource, NULL, piPin, REQUESTED_PINDIR_OUTPUT))
		return (log << "Failed to get output pin on the TSFileSource Filter: " << hr << "\n").Write(hr);

	return S_OK;
}

HRESULT TSFileSource::LoadFile(LPWSTR pFilename, DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt)
{
	BOOL bOwnFilename = FALSE;
	if (!pFilename)
	{
		bOwnFilename = TRUE;
		TCHAR tmpFile[MAX_PATH];
		LPTSTR ptFilename = (LPTSTR)&tmpFile;
		ptFilename[0] = '\0';

		// Setup the OPENFILENAME structure
		OPENFILENAME ofn = { sizeof(OPENFILENAME), g_pData->hWnd, NULL,
							 TEXT("Transport Stream Files (*.mpg, *.ts, *.tsbuffer, *.vob)\0*.mpg;*.ts;*.tsbuffer;*.vob\0All Files\0*.*\0\0"), NULL,
							 0, 1,
							 ptFilename, MAX_PATH,
							 NULL, 0,
							 NULL,
							 TEXT("Load File"),
							 OFN_FILEMUSTEXIST|OFN_HIDEREADONLY, 0, 0,
							 NULL, 0, NULL, NULL };

		// Display the SaveFileName dialog.
		if( GetOpenFileName( &ofn ) == FALSE )
			return S_FALSE;

		USES_CONVERSION;
		strCopy(pFilename, T2W(ptFilename));
	}
	// if multicasting add path if specified
	else if (g_pData->settings.timeshift.folder &&
				wcsstr(g_pData->settings.timeshift.folder, L":") != NULL &&
				wcsstr(_wcslwr(pFilename), L"udp://") == pFilename)
	{
		LPWSTR temp = NULL;
		//check for "\" at end of folder name
		if (wcsstr(g_pData->settings.timeshift.folder, L"\\") != g_pData->settings.timeshift.folder + wcslen(g_pData->settings.timeshift.folder))
		{
			temp = new WCHAR[wcslen(g_pData->settings.timeshift.folder) + wcslen(L"\\") + wcslen(pFilename) + 1];
			StringCchPrintfW(temp, MAX_PATH, L"%s\\%s", g_pData->settings.timeshift.folder, pFilename);
		}
		else
		{
			temp = new WCHAR[wcslen(g_pData->settings.timeshift.folder) + wcslen(pFilename) + 1];
			StringCchPrintfW(temp, MAX_PATH, L"%s%s", g_pData->settings.timeshift.folder, pFilename);
		}
		pFilename = temp;
	}

	(log << "Building Graph (" << pFilename << ")\n").Write();
	LogMessageIndent indent(&log);

	HRESULT hr = E_FAIL;

	m_pDWGraph->Mute(1);

	if (TRUE && m_pDWGraph->IsPlaying())
	{
		SaveResumePosition();
//SeekTo(100);
		if (!pmt || pmt->subtype == MEDIASUBTYPE_MPEG2_TRANSPORT)
		{

			// Set Filename
			CComQIPtr<IFileSourceFilter> piFileSourceFilter(m_pTSFileSource);
			if (!piFileSourceFilter)
				return (log << "Cannot QI TSFileSource filter for IFileSourceFilter: " << hr << "\n").Write(hr);

			if FAILED(hr = piFileSourceFilter->Load(pFilename, pmt))
			{
				m_pDWGraph->Mute(g_pData->values.audio.bMute);
				return (log << "Failed to load filename: " << hr << "\n").Write(hr);
			}

			if (bOwnFilename)
				delete[] pFilename;

			// Set Demux pids if loaded from a TimeShift Source
			if (pService)
			{
				m_DWDemux.set_Auto(TRUE);
				m_DWDemux.set_AC3Mode(g_pData->settings.application.ac3Audio);
				m_DWDemux.set_MPEG2Audio2Mode(g_pData->settings.application.audioSwap);
				m_DWDemux.set_FixedAspectRatio(g_pData->settings.application.fixedAspectRatio);
				m_DWDemux.set_MPEG2AudioMediaType(g_pData->settings.application.mpg2Audio);
				m_DWDemux.set_ClockMode(g_pData->settings.application.refClock);
				if FAILED(hr = m_DWDemux.AOnConnect(m_pTSFileSource, pService))
				{
					(log << "Failed to change the Requested Service using channel zapping.\n").Write();
				}
			}

			LoadResumePosition();

			if FAILED(hr = m_pDWGraph->Start())
				return (log << "Failed to Start Graph.\n").Write(hr);

			m_pDWGraph->Mute(g_pData->values.audio.bMute);

			indent.Release();
			(log << "Finished Loading File\n").Write();

			UpdateData();
			g_pTv->ShowOSDItem(L"Position", g_pData->settings.application.positionOSDTime);

			return S_OK;
		}
	}

	if FAILED(hr = UnloadFilters())
	{
		m_pDWGraph->Mute(g_pData->values.audio.bMute);
		return (log << "Failed to unload previous filters\n").Write(hr);
	}

	//Set flag for timeshift
	m_bTimeShiftService = FALSE;

	if (pService)
		m_bTimeShiftService = TRUE;

	// TSFileSource
	if FAILED(hr = graphTools.AddFilter(m_piGraphBuilder, g_pData->settings.filterguids.filesourceclsid, &m_pTSFileSource, L"TSFileSource"))
	{
		m_pDWGraph->Mute(g_pData->values.audio.bMute);
		return (log << "Failed to add TSFileSource to the graph: " << hr << "\n").Write(hr);
	}

	// Set Filename
	CComQIPtr<IFileSourceFilter> piFileSourceFilter(m_pTSFileSource);
	if (!piFileSourceFilter)
	{
		m_pDWGraph->Mute(g_pData->values.audio.bMute);
		return (log << "Cannot QI TSFileSource filter for IFileSourceFilter: " << hr << "\n").Write(hr);
	}
	if (m_bTimeShiftService)
	{
		if FAILED(hr = SetSourceControl(m_pTSFileSource, TRUE))
		{
			(log << "Failed to disable the ITSFileSource auto control. " << hr << "\n").Write(hr);
		}
	}

	if FAILED(hr = piFileSourceFilter->Load(pFilename, pmt))
	{
		(log << "Failed to load filename: " << hr << "\n").Write();
		if FAILED(hr = UnloadFilters())
		{
			m_pDWGraph->Mute(g_pData->values.audio.bMute);
			return (log << "Failed to unload previous filters\n").Write(hr);
		}
		m_pDWGraph->Mute(g_pData->values.audio.bMute);
		return hr;
	}

	//creates a dummy service and gets the media types from the file source
	if FAILED(hr = SetSourceInterface(m_pTSFileSource, &pService))
	{
		(log << "Failed to Set ITSFileSource Interface: " << hr << "\n").Write(hr);
	}

	//Get the file source media type if pmt is NULL
	if (!m_bTimeShiftService && pmt == NULL)
	{
		CComPtr<IPin>piPin;
		if (SUCCEEDED(graphTools.FindAnyPin(m_pTSFileSource, NULL, &piPin, REQUESTED_PINDIR_OUTPUT) && piPin))
		{
			CComPtr <IEnumMediaTypes> piMediaTypes;
			if SUCCEEDED(piPin->EnumMediaTypes(&piMediaTypes))
			{
				while (piMediaTypes->Next(1, &pmt, 0) == NOERROR)
				{
					break;
				};
			}
		}
	}
	
	// MPEG-2 Demultiplexer (DW's)
	if FAILED(hr = graphTools.AddFilter(m_piGraphBuilder, g_pData->settings.filterguids.demuxclsid, &m_piBDAMpeg2Demux, L"DW MPEG-2 Demultiplexer"))
	{
		m_pDWGraph->Mute(g_pData->values.audio.bMute);
		return (log << "Failed to add DW MPEG-2 Demultiplexer to the graph: " << hr << "\n").Write(hr);
	}

	// Set Demux pids if loaded from a TimeShift Source of we have pids from the file source
	if (pService)
	{
		// Set Demux pins to force the MS demux into the correct mode
		if FAILED(hr = AddDemuxPins(pService, m_piBDAMpeg2Demux, pmt, FALSE)) //No Render Pins
		{
			(log << "Failed to Add Demux Pins\n").Write();
		}

		if FAILED(hr = graphTools.ConnectFilters(m_piGraphBuilder, m_pTSFileSource, m_piBDAMpeg2Demux))
		{
			m_pDWGraph->Mute(g_pData->values.audio.bMute);
			return (log << "Failed to connect TSFileSource to MPEG2 Demultiplexer: " << hr << "\n").Write(hr);
		}
		// Set Demux pids again if loaded from a TimeShift Source as the source filter will clear the demux when its connected
		if FAILED(hr = AddDemuxPins(pService, m_piBDAMpeg2Demux, pmt)) //Render the pins
		{
			(log << "Failed to Add Demux Pins\n").Write();
		}
	}
	else
	{
		if FAILED(hr = graphTools.ConnectFilters(m_piGraphBuilder, m_pTSFileSource, m_piBDAMpeg2Demux))
		{
			m_pDWGraph->Mute(g_pData->values.audio.bMute);
			return (log << "Failed to connect TSFileSource to MPEG2 Demultiplexer: " << hr << "\n").Write(hr);
		}

		// Render output pins
		CComPtr <IEnumPins> piEnumPins;
		if SUCCEEDED(hr = m_piBDAMpeg2Demux->EnumPins( &piEnumPins ))
		{
			CComPtr <IPin> piPin;
			while (piPin.Release(), piEnumPins->Next(1, &piPin, 0) == NOERROR )
			{
				PIN_INFO pinInfo;
				piPin->QueryPinInfo(&pinInfo);
				if (pinInfo.pFilter)
					pinInfo.pFilter->Release();	//QueryPinInfo adds a reference to the filter.

				if (pinInfo.dir == PINDIR_OUTPUT)
				{
					CComPtr<IGraphBuilder> piGraphBuilder;
					if (SUCCEEDED(graphTools.GetGraphBuilder(piPin, piGraphBuilder)))
					{
						if FAILED(hr = m_pDWGraph->RenderPin(piGraphBuilder, piPin))
						{
							(log << "Failed to render " << pinInfo.achName << " stream : " << hr << "\n").Write();
							continue;
						}
					}
				}
				piPin.Release();
			}
		}
	}

	//clear all objects if they exist outside of timeshifting
	if (!m_bTimeShiftService)
	{
		if (pService)
			delete pService;
	}

	m_DWDemux.set_Auto(TRUE);
	m_DWDemux.set_AC3Mode(g_pData->settings.application.ac3Audio);
	m_DWDemux.set_MPEG2Audio2Mode(g_pData->settings.application.audioSwap);
	m_DWDemux.set_FixedAspectRatio(g_pData->settings.application.fixedAspectRatio);
	m_DWDemux.set_MPEG2AudioMediaType(g_pData->settings.application.mpg2Audio);
	m_DWDemux.AOnConnect(m_pTSFileSource); //only sets the source filter reference
	m_DWDemux.set_ClockMode(g_pData->settings.application.refClock);
	m_DWDemux.SetRefClock();

	//Save the current fileName
	if (pFilename)
	{
		if (pFilename != m_pFileName)
		{
			if (m_pFileName)
			{
				delete[] m_pFileName;
				m_pFileName = NULL;
			}
		}

		if (!m_pFileName)
			strCopy(m_pFileName, pFilename);

		if (bOwnFilename)
			delete[] pFilename;
	}

	LoadResumePosition();

//g_pOSD->Data()->SetItem(L"warnings", L"Starting to play the TimeShift File");
//g_pTv->ShowOSDItem(L"Warnings", 2);

	if FAILED(hr = m_pDWGraph->Start())
	{
		m_pDWGraph->Mute(g_pData->values.audio.bMute);
		return (log << "Failed to Start Graph.\n").Write(hr);
	}

	indent.Release();
	(log << "Finished Loading File\n").Write();

	UpdateData();
	g_pTv->ShowOSDItem(L"Position", g_pData->settings.application.positionOSDTime);

	// Start the background thread for updating statistics
	if FAILED(hr = StartThread())
	{
		m_pDWGraph->Mute(g_pData->values.audio.bMute);
		return (log << "Failed to start background thread: " << hr << "\n").Write(hr);
	}

	m_pDWGraph->Mute(g_pData->values.audio.bMute);

	return S_OK;
}

HRESULT TSFileSource::ReLoadFile(LPWSTR pFilename)
{
	(log << "Reloading the FileSource Filter with fileName : (" << pFilename << ")\n").Write();
	LogMessageIndent indent(&log);

	HRESULT hr = E_FAIL;

	// Stop background thread
	if FAILED(hr = StopThread())
		return (log << "Failed to stop background thread: " << hr << "\n").Write(hr);
	if (hr == S_FALSE)
		(log << "Killed thread\n").Write();

	if (m_pTSFileSource)
	{
		SaveResumePosition();
//Seek(100);
		// Set Filename
		CComQIPtr<IFileSourceFilter> piFileSourceFilter(m_pTSFileSource);
		if (!piFileSourceFilter)
			return (log << "Cannot QI TSFileSource filter for IFileSourceFilter: " << hr << "\n").Write(hr);

		m_pDWGraph->Mute(1);

		if FAILED(hr = piFileSourceFilter->Load(pFilename, NULL))
		{
			m_pDWGraph->Mute(g_pData->values.audio.bMute);
			return (log << "Failed to load filename: " << hr << "\n").Write(hr);
		}

		LoadResumePosition();

		// Start the background thread for updating statistics
		if FAILED(hr = StartThread())
		{
			m_pDWGraph->Mute(g_pData->values.audio.bMute);
			return (log << "Failed to start background thread: " << hr << "\n").Write(hr);
		}

		indent.Release();
		(log << "Finished Reloading File\n").Write();

		m_pDWGraph->Mute(g_pData->values.audio.bMute);

		return hr;
	}
	return hr;
}

HRESULT TSFileSource::LoadResumePosition()
{
	HRESULT hr = NOERROR;
	long lPosition = 0;

	//Set the Resume File Time
	if (SUCCEEDED(GetPosition(&lPosition)))
	{
		// Get Filename
		CComQIPtr<IFileSourceFilter> piFileSourceFilter(m_pTSFileSource);
		if (!piFileSourceFilter)
			(log << "Cannot QI TSFileSource filter for IFileSourceFilter: " << hr << "\n").Write();

		LPOLESTR pFilename = NULL;
		if FAILED(hr = piFileSourceFilter->GetCurFile(&pFilename, NULL))
			(log << "Failed to get filename: " << hr << "\n").Write(hr);
		else if (pFilename)
		{
			// If it's a .tsbuffer file then seek to the end
			long length = wcslen(pFilename);
			if ((length >= 9) && (_wcsicmp(pFilename+length-9, L".tsbuffer") == 0))
			{
				if (g_pData->settings.timeshift.resume)
				{
					long lPosition = m_pDWGraph->GetResumePosition(pFilename);
					if (g_pData->settings.timeshift.localTime)
					{
						CComQIPtr<IMediaSeeking> piMediaSeeking(m_piGraphBuilder);
						if (piMediaSeeking)
						{
							REFERENCE_TIME rtCurrent, rtEarliest, rtLatest = 0;

							if FAILED(hr = piMediaSeeking->GetAvailable(&rtEarliest, &rtLatest))
								return (log << "Failed to get available times: " << hr << "\n").Write(hr);
(log << "rtLatest : " << rtLatest << "\n").Write();

							if FAILED(hr = piMediaSeeking->GetCurrentPosition(&rtCurrent))
								return (log << "Failed to get current time: " << hr << "\n").Write(hr);

							_tzset();
							time_t lCurrentTime;
							time(&lCurrentTime);
							lPosition = max(0, lPosition - (long)((long)lCurrentTime - (long)(rtLatest/10000000)));
(log << "Setting Position to : " << lPosition << "\n").Write();
						}
					}

					if (lPosition != 0)
						Seek(lPosition);
				}
				else
					SeekTo(100);
			}
			else
			{
				if (g_pData->settings.application.resumeLastTime)
				{
					//Do this to set the Filtergraph seek time since changing the reference clock upsets the demux
					long lPosition = m_pDWGraph->GetResumePosition(pFilename);
					Seek(lPosition);
				}
				else
					SeekTo(0);
			}
		}
	}
	m_lLocalStartTime = 0;
	return S_OK;
}

HRESULT TSFileSource::SaveResumePosition()
{
	if (!m_pDWGraph->IsPlaying())
		return S_FALSE;

	HRESULT hr = NOERROR;
	long lPosition = 0;

	//Set the Resume File Time
	if (SUCCEEDED(GetPosition(&lPosition)))
	{
		if (m_lLocalStartTime && m_bTimeShiftService && g_pData->settings.timeshift.localTime)
		{
(log << "Saving Current Position to : " << lPosition << "\n").Write();
			time_t lTime = m_lLocalStartTime + lPosition;
			lPosition = (long)lTime;
(log << "Saving Current Time Position to : " << lPosition << "\n").Write();
		}

		// Get Filename
		CComQIPtr<IFileSourceFilter> piFileSourceFilter(m_pTSFileSource);
		if (!piFileSourceFilter)
			(log << "Cannot QI TSFileSource filter for IFileSourceFilter: " << hr << "\n").Write();

		LPOLESTR pFilename = NULL;
		if FAILED(hr = piFileSourceFilter->GetCurFile(&pFilename, NULL))
			(log << "Failed to get filename: " << hr << "\n").Write(hr);
		else if (pFilename)
			m_pDWGraph->SetResumePosition(pFilename, lPosition);
	}

	return S_OK;
}

HRESULT TSFileSource::CloseDisplay()
{
	HRESULT hr = S_OK;

	//Check if service already running
	if (m_pDWGraph->IsPlaying())
	{
		SaveResumePosition();

		if FAILED(hr = UnloadFilters())
			return (log << "Failed to Unload the File Source Filters: " << hr << "\n").Write(hr);
	}
	return hr;
}

HRESULT TSFileSource::OpenDisplay(BOOL bTest)
{
	 //Check if service already running
	if (!m_pDWGraph->IsPlaying())
	{
		if (m_pFileName)
			return 	LoadFile(m_pFileName);
		else if (!bTest)
			return 	LoadFile(NULL);
	}

	return S_OK;
}

HRESULT TSFileSource::UnloadFilters()
{
	HRESULT hr;

	// Stop background thread
	if FAILED(hr = StopThread())
		return (log << "Failed to stop background thread: " << hr << "\n").Write(hr);
	if (hr == S_FALSE)
		(log << "Killed thread\n").Write();

	if (m_pDWGraph)
	{
		if FAILED(hr = m_pDWGraph->Stop())
			return (log << "Failed to stop DWGraph\n").Write(hr);
	}

(log << "Killing m_piMpeg2Demux\n").Write();
	if (m_piMpeg2Demux)
		m_piMpeg2Demux.Release();

(log << "Disconnect All Pins\n").Write();
	if FAILED(hr = graphTools.DisconnectAllPins(m_piGraphBuilder))
		(log << "Failed to DisconnectAllPins : " << hr << "\n").Write();

(log << "DestroyFilter m_pTSFileSource\n").Write();
	DestroyFilter(m_pTSFileSource);
(log << "DestroyFilter m_piBDAMpeg2Demux\n").Write();
	DestroyFilter(m_piBDAMpeg2Demux);

	if FAILED(hr = m_pDWGraph->Cleanup())
		return (log << "Failed to cleanup DWGraph\n").Write(hr);

	return S_OK;
}

HRESULT TSFileSource::AddDemuxPins(DVBTChannels_Service* pService, CComPtr<IBaseFilter>& pFilter, AM_MEDIA_TYPE *pmt, BOOL bRender, BOOL bForceConnect)
{
	if (pService == NULL)
	{
		(log << "Skipping Demux Pins. No service passed.\n").Write();
		return E_INVALIDARG;
	}

	if (pFilter == NULL)
	{
		(log << "Skipping Demux Pins. No Demultiplexer passed.\n").Write();
		return E_INVALIDARG;
	}

	(log << "Adding Demux Pins\n").Write();
	LogMessageIndent indent(&log);

	HRESULT hr;

	if (m_piMpeg2Demux)
		m_piMpeg2Demux.Release();

	if FAILED(hr = pFilter->QueryInterface(&m_piMpeg2Demux))
	{
		(log << "Failed to get the IMeg2Demultiplexer Interface on the Sink Demux.\n").Write();
		return E_FAIL;
	}

	long videoStreamsRendered = 0;
	long audioStreamsRendered = 0;
	long teletextStreamsRendered = 0;
	long subtitleStreamsRendered = 0;
	long tsStreamsRendered = 0;

	// render video
	hr = AddDemuxPinsVideo(pService, pmt, &videoStreamsRendered, bRender);
	if(FAILED(hr) && bForceConnect)
		return hr;

	// render h264 video if no mpeg2 video was rendered
	if (videoStreamsRendered == 0)
	{
		hr = AddDemuxPinsH264(pService, pmt, &videoStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render mpeg4 video if no mpeg2 or h264 video was rendered
	if (videoStreamsRendered == 0)
	{
		hr = AddDemuxPinsMpeg4(pService, pmt, &videoStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render SD video if no other video was rendered for audio only streams
	if (videoStreamsRendered == 0 && !bForceConnect)
	{
		DVBTChannels_Service* pService = new DVBTChannels_Service();
		pService->SetLogCallback(m_pLogCallback);
		DVBTChannels_Stream *pStream = new DVBTChannels_Stream();
		pStream->SetLogCallback(m_pLogCallback);
		pStream->PID = 0;
		pStream->Type = video;
		pService->AddStream(pStream);
		hr = AddDemuxPinsVideo(pService, pmt, &videoStreamsRendered, bRender);
		delete pService;
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render teletext if video was rendered
	if (videoStreamsRendered > 0)
	{
		hr = AddDemuxPinsTeletext(pService, pmt, &teletextStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render subtitle if video was rendered
	if (videoStreamsRendered > 0)
	{
		hr = AddDemuxPinsSubtitle(pService, pmt, &subtitleStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render ac3 audio if preferred
	if (g_pData->settings.application.ac3Audio && !bForceConnect)
	{
		hr = AddDemuxPinsAC3(pService, pmt, &audioStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}
	// render mp2 audio if prefered
	else if (g_pData->settings.application.mpg2Audio && !bForceConnect)
	{
		hr = AddDemuxPinsMp2(pService, pmt, &audioStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render mp1 audio
	hr = AddDemuxPinsMp1(pService, pmt, &audioStreamsRendered, bRender);
	if(FAILED(hr) && bForceConnect)
		return hr;

	// render mp2 audio if no mp1 was rendered
	if (audioStreamsRendered == 0)
	{
		hr = AddDemuxPinsMp2(pService, pmt, &audioStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render ac3 audio if no mp1/2 was rendered
	if (audioStreamsRendered == 0)
	{
		hr = AddDemuxPinsAC3(pService, pmt, &audioStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render aac audio if no ac3 or mp1/2 was rendered
	if (audioStreamsRendered == 0)
	{
		hr = AddDemuxPinsAAC(pService, pmt, &audioStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render aac audio if no ac3 or mp1/2 or acc was rendered
	if (audioStreamsRendered == 0)
	{
		hr = AddDemuxPinsDTS(pService, pmt, &audioStreamsRendered, bRender);
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	// render mp2 audio if no other audio was rendered for Video only streams
	if (audioStreamsRendered == 0 && !bForceConnect)
	{
		DVBTChannels_Service* pService = new DVBTChannels_Service();
		pService->SetLogCallback(m_pLogCallback);
		DVBTChannels_Stream *pStream = new DVBTChannels_Stream();
		pStream->SetLogCallback(m_pLogCallback);
		pStream->Type = mp2;
		pService->AddStream(pStream);
		hr = AddDemuxPinsMp2(pService, pmt, &audioStreamsRendered, bRender);
		delete pService;
		if(FAILED(hr) && bForceConnect)
			return hr;
	}

	if (m_piMpeg2Demux)
		m_piMpeg2Demux.Release();

	indent.Release();
	(log << "Finished Adding Demux Pins\n").Write();

	return S_OK;
}

HRESULT TSFileSource::AddDemuxPins(DVBTChannels_Service* pService, DVBTChannels_Service_PID_Types streamType, LPWSTR pPinName, AM_MEDIA_TYPE *pMediaType, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	if (pService == NULL)
		return E_INVALIDARG;

	HRESULT hr = S_OK;

	long renderedStreams = 0;
	long count = pService->GetStreamCount(streamType);
	BOOL bMultipleStreams = (pService->GetStreamCount(streamType) > 1) ? 1 : 0;

	for ( long currentStream=0 ; currentStream<count ; currentStream++ )
	{
		ULONG Pid = pService->GetStreamPID(streamType, currentStream);

		wchar_t text[32];
		StringCchPrintfW((wchar_t*)&text, 32, pPinName);
		if (bMultipleStreams && currentStream > 0)
			StringCchPrintfW((wchar_t*)&text, 32, L"%s %i", pPinName, currentStream+1);

		CComPtr <IPin> piPin;

		// Get the Pin
		CComPtr<IBaseFilter>pFilter;
		if SUCCEEDED(hr = m_piMpeg2Demux->QueryInterface(&pFilter))
		{
			if FAILED(hr = pFilter->FindPin(pPinName, &piPin))
			{
				// Create the Pin
				(log << "Creating pin: PID=" << (long)Pid << "   Name=\"" << (LPWSTR)&text << "\"\n").Write();
				LogMessageIndent indent(&log);

				if (S_OK != (hr = m_piMpeg2Demux->CreateOutputPin(pMediaType, (wchar_t*)&text, &piPin)))
				{
					(log << "Failed to create demux " << pPinName << " pin : " << hr << "\n").Write();
					return hr;
				}
				indent.Release();
			}
		}
		else
		{
			(log << "Creating pin: PID=" << (long)Pid << "   Name=\"" << (LPWSTR)&text << "\"\n").Write();
			LogMessageIndent indent(&log);

			// Create the Pin
			if (S_OK != (hr = m_piMpeg2Demux->CreateOutputPin(pMediaType, (wchar_t*)&text, &piPin)))
			{
				(log << "Failed to create demux " << pPinName << " pin : " << hr << "\n").Write();
				return hr;
			}
			indent.Release();
		}

		// Map the PID.
		CComPtr <IMPEG2PIDMap> piPidMap;
		CComPtr <IMPEG2StreamIdMap> piStreamIdMap;
		if(!pmt || pmt->subtype != MEDIASUBTYPE_MPEG2_PROGRAM)
		{
			if (SUCCEEDED(hr = piPin.QueryInterface(&piPidMap)))
			{
				if FAILED(hr = graphTools.VetDemuxPin(piPin, Pid))
				{
					(log << "Failed to unmap demux " << pPinName << " pin : " << hr << "\n").Write();
					continue;	//it's safe to not piPidMap.Release() because it'll go out of scope
				}

				if(Pid)
				{
					if(pMediaType->majortype == KSDATAFORMAT_TYPE_MPEG2_SECTIONS)
					{
						if FAILED(hr = piPidMap->MapPID(1, &Pid, MEDIA_TRANSPORT_PACKET))
						{
							(log << "Failed to map demux " << pPinName << " pin : " << hr << "\n").Write();
							continue;	//it's safe to not piPidMap.Release() because it'll go out of scope
						}
					}
					else if FAILED(hr = piPidMap->MapPID(1, &Pid, MEDIA_ELEMENTARY_STREAM))
					{
						(log << "Failed to map demux " << pPinName << " pin : " << hr << "\n").Write();
						continue;	//it's safe to not piPidMap.Release() because it'll go out of scope
					}
				}
			}
		}
		else if SUCCEEDED(hr = piPin.QueryInterface(&piStreamIdMap))
		{
			USHORT pidId = 0xC0;
			if (pMediaType->subtype == MEDIASUBTYPE_MPEG2_VIDEO)
			{
				if FAILED(hr = graphTools.VetDemuxPin(piPin, Pid))
				{
					(log << "Failed to unmap demux " << pPinName << " pin : " << hr << "\n").Write();
					continue;	//it's safe to not piPidMap.Release() because it'll go out of scope
				}

				if FAILED(hr = piStreamIdMap->MapStreamId(0xE0, MPEG2_PROGRAM_ELEMENTARY_STREAM, 0, 0))
				{
					(log << "Failed to map demux Stream ID's " << pPinName << " pin : " << hr << "\n").Write();
					continue;	//it's safe to not piStreamIdMap.Release() because it'll go out of scope
				}
			}
			else if (pMediaType->subtype == MEDIASUBTYPE_MPEG1Payload)
			{
				if FAILED(hr = graphTools.VetDemuxPin(piPin, Pid))
				{
					(log << "Failed to unmap demux " << pPinName << " pin : " << hr << "\n").Write();
					continue;	//it's safe to not piPidMap.Release() because it'll go out of scope
				}

				if FAILED(hr = piStreamIdMap->MapStreamId(0xC0, MPEG2_PROGRAM_ELEMENTARY_STREAM, 0, 0))
				{
					(log << "Failed to map demux Stream ID's " << pPinName << " pin : " << hr << "\n").Write();
					continue;	//it's safe to not piStreamIdMap.Release() because it'll go out of scope
				}
			}
			else if (pMediaType->subtype == MEDIASUBTYPE_MPEG2_AUDIO)
			{
				if FAILED(hr = graphTools.VetDemuxPin(piPin, Pid))
				{
					(log << "Failed to unmap demux " << pPinName << " pin : " << hr << "\n").Write();
					continue;	//it's safe to not piPidMap.Release() because it'll go out of scope
				}

				if FAILED(hr = piStreamIdMap->MapStreamId(0xC0, MPEG2_PROGRAM_ELEMENTARY_STREAM, 0, 0))
				{
					(log << "Failed to map demux Stream ID's " << pPinName << " pin : " << hr << "\n").Write();
					continue;	//it's safe to not piStreamIdMap.Release() because it'll go out of scope
				}
			}
			else if (pMediaType->subtype == MEDIASUBTYPE_DOLBY_AC3)
			{
				if FAILED(hr = graphTools.VetDemuxPin(piPin, Pid))
				{
					(log << "Failed to unmap demux " << pPinName << " pin : " << hr << "\n").Write();
					continue;	//it's safe to not piPidMap.Release() because it'll go out of scope
				}

				if FAILED(hr = piStreamIdMap->MapStreamId(0xBD, MPEG2_PROGRAM_ELEMENTARY_STREAM, 0x80, 0x04))
				{
					(log << "Failed to map demux Stream ID's " << pPinName << " pin : " << hr << "\n").Write();
					continue;	//it's safe to not piStreamIdMap.Release() because it'll go out of scope
				}
			}
		}
		else
			(log << "Failed to map demux " << pPinName << " pin : " << hr << "\n").Write();

		if (renderedStreams != 0)
			continue;

		if (bRender)
		{
			CComPtr<IPin>pOPin;
			if (piPin && piPin->ConnectedTo(&pOPin) && !pOPin)
			{
				CComPtr<IGraphBuilder> piGraphBuilder;
				if (SUCCEEDED(graphTools.GetGraphBuilder(piPin, piGraphBuilder)))
				{
					if FAILED(hr = m_pDWGraph->RenderPin(piGraphBuilder, piPin))
					{
						(log << "Failed to render " << pPinName << " stream : " << hr << "\n").Write();
						continue;
					}
				}
			}
		}

		renderedStreams++;
	}

	if (streamsRendered)
		*streamsRendered = renderedStreams;

	return hr;
}

HRESULT TSFileSource::AddDemuxPinsVideo(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	ZeroMemory(&mediaType, sizeof(AM_MEDIA_TYPE));
	graphTools.GetVideoMedia(&mediaType);
	return AddDemuxPins(pService, video, L"Video", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::AddDemuxPinsH264(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	graphTools.GetH264Media(&mediaType);
	return AddDemuxPins(pService, h264, L"Video", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::AddDemuxPinsMpeg4(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	graphTools.GetMpeg4Media(&mediaType);
	return AddDemuxPins(pService, mpeg4, L"Video", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::AddDemuxPinsMp1(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	graphTools.GetMP1Media(&mediaType);
	return AddDemuxPins(pService, mp1, L"Audio", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::AddDemuxPinsMp2(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	graphTools.GetMP2Media(&mediaType);
	return AddDemuxPins(pService, mp2, L"Audio", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::AddDemuxPinsAC3(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	graphTools.GetAC3Media(&mediaType);
	return AddDemuxPins(pService, ac3, L"Audio", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::AddDemuxPinsAAC(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	graphTools.GetAACMedia(&mediaType);
	return AddDemuxPins(pService, aac, L"Audio", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::AddDemuxPinsDTS(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	graphTools.GetDTSMedia(&mediaType);
	return AddDemuxPins(pService, dts, L"Audio", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::AddDemuxPinsTeletext(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	ZeroMemory(&mediaType, sizeof(AM_MEDIA_TYPE));
	graphTools.GetTelexMedia(&mediaType);
	return AddDemuxPins(pService, teletext, L"Teletext", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::AddDemuxPinsSubtitle(DVBTChannels_Service* pService, AM_MEDIA_TYPE *pmt, long *streamsRendered, BOOL bRender)
{
	AM_MEDIA_TYPE mediaType;
	ZeroMemory(&mediaType, sizeof(AM_MEDIA_TYPE));
	graphTools.GetSubtitleMedia(&mediaType);
	return AddDemuxPins(pService, subtitle, L"Subtitle", &mediaType, pmt, streamsRendered, bRender);
}

HRESULT TSFileSource::PlayPause()
{
	HRESULT hr;

	if (!m_pDWGraph)
		return S_FALSE;

	if (!m_pDWGraph->IsPlaying())
	{
		if FAILED(hr = m_pDWGraph->Start())
			return (log << "Failed to Unpause Graph.\n").Write(hr);

		g_pOSD->Data()->SetItem(L"warnings", L"Playing");
		g_pTv->ShowOSDItem(L"Warnings", g_pData->settings.application.warningOSDTime);
	}
	else
	{
		if (m_pDWGraph->IsPaused())
		{
			if FAILED(hr = m_pDWGraph->Pause(FALSE))
				return (log << "Failed to Unpause Graph.\n").Write(hr);

			g_pOSD->Data()->SetItem(L"warnings", L"Playing");
			g_pTv->ShowOSDItem(L"Warnings", g_pData->settings.application.warningOSDTime);
		}
		else
		{
			if FAILED(hr = m_pDWGraph->Pause(TRUE))
				return (log << "Failed to Pause Graph.\n").Write(hr);

			g_pOSD->Data()->SetItem(L"warnings", L"Paused");
			g_pTv->ShowOSDItem(L"Warnings", 100000);
		}
	}
	return S_OK;
}

HRESULT TSFileSource::Skip(long seconds)
{
	HRESULT hr;

	CComQIPtr<IMediaSeeking> piMediaSeeking(m_piGraphBuilder);

	REFERENCE_TIME rtNow, rtStop, rtEarliest, rtLatest;

	if FAILED(hr = piMediaSeeking->GetCurrentPosition(&rtNow))
		return (log << "Failed to get current time: " << hr << "\n").Write(hr);

	if FAILED(hr = piMediaSeeking->GetAvailable(&rtEarliest, &rtLatest))
		return (log << "Failed to get available times: " << hr << "\n").Write(hr);

	rtLatest = (__int64)max(rtEarliest, (__int64)(rtLatest-(__int64)20000000));

	rtStop = 0;
	rtNow += ((__int64)seconds * (__int64)10000000);
	if (rtNow < rtEarliest)
		rtNow = rtEarliest;
	if (rtNow > rtLatest)
		rtNow = rtLatest;

	if FAILED(piMediaSeeking->SetPositions(&rtNow, AM_SEEKING_AbsolutePositioning, &rtStop, AM_SEEKING_NoPositioning))
		return (log << "Failed to set positions: " << hr << "\n").Write(hr);

	UpdateData();

	return S_OK;
}

HRESULT TSFileSource::SeekTo(long percentage)
{
	HRESULT hr;

	CComQIPtr<IMediaSeeking> piMediaSeeking(m_piGraphBuilder);

	REFERENCE_TIME rtNow, rtStop, rtEarliest, rtLatest;

	if FAILED(hr = piMediaSeeking->GetAvailable(&rtEarliest, &rtLatest))
		return (log << "Failed to get available times: " << hr << "\n").Write(hr);

	rtLatest = (__int64)max(rtEarliest, (__int64)(rtLatest/*-(__int64)20000000*/));

	if (percentage > 100)
		percentage = 100;
	if (percentage < 0)
		percentage = 0;

	rtStop = 0;
	rtNow = rtLatest - rtEarliest;
	rtNow *= (__int64)percentage;
	rtNow /= (__int64)100;
	rtNow += rtEarliest;

	if FAILED(piMediaSeeking->SetPositions(&rtNow, AM_SEEKING_AbsolutePositioning, &rtStop, AM_SEEKING_NoPositioning))
		return (log << "Failed to set positions: " << hr << "\n").Write(hr);

	UpdateData();

	return S_OK;
}

HRESULT TSFileSource::Seek(long position)
{
	HRESULT hr;

	CComQIPtr<IMediaSeeking> piMediaSeeking(m_piGraphBuilder);

	REFERENCE_TIME rtNow, rtStop, rtEarliest, rtLatest;

	if FAILED(hr = piMediaSeeking->GetAvailable(&rtEarliest, &rtLatest))
		return (log << "Failed to get available times: " << hr << "\n").Write(hr);

	rtLatest = (__int64)max(rtEarliest, (__int64)(rtLatest-(__int64)20000000));

	rtNow = (__int64)max((__int64)rtEarliest, (__int64)position*10000000);
	rtNow = (__int64)min((__int64)rtLatest, (__int64)rtNow);

	rtStop = 0;

	if FAILED(piMediaSeeking->SetPositions(&rtNow, AM_SEEKING_AbsolutePositioning, &rtStop, AM_SEEKING_NoPositioning))
		return (log << "Failed to set positions: " << hr << "\n").Write(hr);

	UpdateData();

	return S_OK;
}

HRESULT TSFileSource::GetPosition(long *position)
{
	HRESULT hr;

	CComQIPtr<IMediaSeeking> piMediaSeeking(m_piGraphBuilder);

	REFERENCE_TIME rtNow; //, rtEarliest, rtLatest;

//	if FAILED(hr = piMediaSeeking->GetAvailable(&rtEarliest, &rtLatest))
//		return (log << "Failed to get available times: " << hr << "\n").Write(hr);

	if FAILED(hr = piMediaSeeking->GetCurrentPosition(&rtNow))
		return (log << "Failed to get available times: " << hr << "\n").Write(hr);

	*position = (long)(rtNow/10000000);

	return S_OK;
}

HRESULT TSFileSource::SetRate(double dRate)
{
	HRESULT hr;

	CComQIPtr<IMediaSeeking> piMediaSeeking(m_piGraphBuilder);
	if FAILED(hr = piMediaSeeking->SetRate(dRate))
		return (log << "Failed to set Source Rate: " << hr << "\n").Write(hr);

	UpdateData();
	return S_OK;
}

HRESULT TSFileSource::SetSourceControl(IBaseFilter *pFilter, BOOL autoMode)
{
	if (!pFilter)
		return E_INVALIDARG;

	HRESULT hr = E_NOINTERFACE;
	CComQIPtr <ITSFileSource, &IID_ITSFileSource> pITSFileSource(pFilter);
	if(!pITSFileSource)
		return (log << "Failed to get ITSFileSource interface from IBaseFilter filter: " << hr << "\n").Write(hr);

	pITSFileSource->SetAutoMode((USHORT)autoMode);

	pITSFileSource.Release();
	return S_OK;
}

HRESULT TSFileSource::SetSourceInterface(IBaseFilter *pFilter, DVBTChannels_Service** pService)
{
	if (!pFilter)
		return E_INVALIDARG;

	HRESULT hr = E_NOINTERFACE;
	CComQIPtr <ITSFileSource, &IID_ITSFileSource> pITSFileSource(pFilter);
	if(!pITSFileSource)
		return (log << "Failed to get ITSFileSource interface from IBaseFilter filter: " << hr << "\n").Write(hr);

	pITSFileSource->SetNPControl(0);
	pITSFileSource->SetNPSlave(0);
	pITSFileSource->SetDelayMode(0);
	pITSFileSource->SetMP2Mode(g_pData->settings.application.mpg2Audio);
	pITSFileSource->SetAC3Mode(g_pData->settings.application.ac3Audio);
	pITSFileSource->SetAudio2Mode(g_pData->settings.application.audioSwap);
	pITSFileSource->SetFixedAspectRatio(g_pData->settings.application.fixedAspectRatio);
	pITSFileSource->SetROTMode(0);
	pITSFileSource->SetClockMode(g_pData->settings.application.refClock);
	pITSFileSource->SetCreateTSPinOnDemux(0);
	pITSFileSource->SetCreateTxtPinOnDemux(0);
	pITSFileSource->SetAutoMode(1);
	pITSFileSource->SetRateControlMode(0);


	//if no service already defined then try and get the media type from the file
	if (pService && *pService == NULL)
	{
		(log << "Getting Stream info from the ITSFileSource interface\n").Write();
		*pService = new DVBTChannels_Service();
		DVBTChannels_Stream *pStream = NULL;

		USHORT pid = 0, pid2 = 0;
		BYTE *ptMedia = new BYTE[128];
		LPWSTR mediaType = NULL;

		pITSFileSource->GetVideoPidType(ptMedia);
		if (stricmp((const char *)ptMedia, "H.264") == 0)
			strCopy(mediaType, L"H264 Video");
		else if (stricmp((const char *)ptMedia, "MPEG 4") == 0)
			strCopy(mediaType, L"MPEG4 Video");
		else
			strCopy(mediaType, L"MPEG2 Video");

		pITSFileSource->GetVideoPid(&pid);
		if (pid)
			if SUCCEEDED(hr = LoadMediaStreamType(pid, mediaType, &pStream))
				(*pService)->AddStream(pStream);

		pITSFileSource->GetAACPid(&pid);
		if (pid)
		{
			strCopy(mediaType, L"AAC Audio");
			if SUCCEEDED(hr = LoadMediaStreamType(pid, mediaType, &pStream))
				(*pService)->AddStream(pStream);
		}
		
		pITSFileSource->GetAAC2Pid(&pid2);
		if (pid2)
		{
			strCopy(mediaType, L"AAC Audio");
			if SUCCEEDED(hr = LoadMediaStreamType(pid2, mediaType, &pStream))
				(*pService)->AddStream(pStream);
		}

		pITSFileSource->GetDTSPid(&pid);
		if (pid)
		{
			strCopy(mediaType, L"DTS Audio");
			if SUCCEEDED(hr = LoadMediaStreamType(pid, mediaType, &pStream))
				(*pService)->AddStream(pStream);
		}
		
		pITSFileSource->GetDTS2Pid(&pid2);
		if (pid2)
		{
			strCopy(mediaType, L"DTS Audio");
			if SUCCEEDED(hr = LoadMediaStreamType(pid2, mediaType, &pStream))
				(*pService)->AddStream(pStream);
		}

		strCopy(mediaType, L"MPEG Audio");
		pITSFileSource->GetMP2Mode(&pid);
		if (pid)
			strCopy(mediaType, L"MPEG2 Audio");

		pITSFileSource->GetAudioPid(&pid);
		if (pid)
		{
			if SUCCEEDED(hr = LoadMediaStreamType(pid, mediaType, &pStream))
				(*pService)->AddStream(pStream);
		}

		pITSFileSource->GetAudio2Pid(&pid2);
		if (pid2)
		{
			if SUCCEEDED(hr = LoadMediaStreamType(pid2, mediaType, &pStream))
				(*pService)->AddStream(pStream);
		}

		pITSFileSource->GetAC3Pid(&pid);
		if (pid || pid2)
		{
			if SUCCEEDED(hr = LoadMediaStreamType(pid, L"AC3 Audio", &pStream))
				(*pService)->AddStream(pStream);
		}

		pITSFileSource->GetAC3_2Pid(&pid2);
		if (pid2)
		{
			if SUCCEEDED(hr = LoadMediaStreamType(pid2, L"AC3 Audio", &pStream))
				(*pService)->AddStream(pStream);
		}

		pITSFileSource->GetTelexPid(&pid);
		if (pid)
		{
			if SUCCEEDED(hr = LoadMediaStreamType(pid, L"Teletext", &pStream))
				(*pService)->AddStream(pStream);
		}

		pITSFileSource->GetSubtitlePid(&pid);
		if (pid)
		{
			if SUCCEEDED(hr = LoadMediaStreamType(pid, L"Subtitle", &pStream))
				(*pService)->AddStream(pStream);
		}

		if(mediaType)
			delete[] mediaType;

		if(ptMedia)
			delete[] ptMedia;
		

		//if we failed to find a media type then null service
		if (!(*pService)->GetStreamCount())
		{
			delete *pService;
			*pService = NULL;
		}
	}
	pITSFileSource.Release();
	return S_OK;
}

HRESULT TSFileSource::TestDecoderSelection(LPWSTR pwszMediaType)
{
	(log << "Building the Decoder Test Graph\n").Write();
	LogMessageIndent indent(&log);

	HRESULT hr = E_FAIL;

	//Close the Graph
	if FAILED(hr = CloseDisplay())
	{
		(log << "Failed to add Test MPEG-2 Demultiplexer to the graph: " << hr << "\n").Write();
		return hr;
	}
	
	g_pData->application.forceConnect = TRUE;

	DVBTChannels_Service* pService = new DVBTChannels_Service();
	pService->SetLogCallback(m_pLogCallback);
	DVBTChannels_Stream *pStream = new DVBTChannels_Stream();
	pStream->SetLogCallback(m_pLogCallback);

	CComPtr <IBaseFilter> piBDAMpeg2Demux;
	DWORD rotEntry = 0;

	while (TRUE)
	{
		BOOL bFound = FALSE;
		for (int i=0 ; i<DVBTChannels_Service_PID_Types_Count ; i++ )
		{
			if (_wcsicmp(pwszMediaType, DVBTChannels_Service_PID_Types_String[i]) == 0)
			{
				pStream->Type = (DVBTChannels_Service_PID_Types)i;
				bFound = TRUE;
			}
		}

		if (bFound)
			pService->AddStream(pStream);
		else
		{
			delete pStream;
			(log << "Failed to find a matching Media Type\n").Write();
			break; 
		}

		//MPEG-2 Demultiplexer (DW's)
		if FAILED(hr = graphTools.AddFilter(m_piGraphBuilder, g_pData->settings.filterguids.demuxclsid, &piBDAMpeg2Demux, L"DW MPEG-2 Demultiplexer"))
		{
			(log << "Failed to add Test MPEG-2 Demultiplexer to the graph: " << hr << "\n").Write();
			break;
		}

		if FAILED(hr = AddDemuxPins(pService, piBDAMpeg2Demux, NULL, TRUE, TRUE))
		{
			(log << "Failed to Add Demux Pins and render the graph\n").Write();
			break;
		}


		break;
	};
	 
	if FAILED(hr)
	{
		(log << "Failed Building the Decoder Test Graph\n").Write();
	}
	else
		(log << "Finished Building the Decoder Test Graph Ok\n").Write();

	delete pService;

	(log << "Cleaning up the Decoder Test Graph\n").Write();

	HRESULT hr2 = m_pDWGraph->Stop();
	if FAILED(hr2)
		(log << "Failed to stop DWGraph\n").Write();

	if (piBDAMpeg2Demux)
		piBDAMpeg2Demux.Release();

	hr2 = graphTools.DisconnectAllPins(m_piGraphBuilder);
	if FAILED(hr2)
		(log << "Failed to disconnect pins: " << hr << "\n").Write(hr);

	hr2 = graphTools.RemoveAllFilters(m_piGraphBuilder);
	if FAILED(hr2)
		(log << "Failed to remove filters: " << hr << "\n").Write(hr);

	g_pData->application.forceConnect = FALSE;

	indent.Release();
	(log << "Finished Cleaning up the Decoder Test Graph\n").Write();

	return hr;
}

HRESULT TSFileSource::LoadMediaStreamType(USHORT pid, LPWSTR pwszMediaType, DVBTChannels_Stream** pStream )
{
	//Return if stream is not null
	if (!pStream)
		return E_INVALIDARG;

	HRESULT hr = E_FAIL;
	
	//Search for a matching media type and save into the service
	for (int i=0 ; i<DVBTChannels_Service_PID_Types_Count ; i++ )
	{
		if (_wcsicmp(pwszMediaType, DVBTChannels_Service_PID_Types_String[i]) == 0)
		{
			*pStream = new DVBTChannels_Stream();
			(*pStream)->SetLogCallback(m_pLogCallback);
			(*pStream)->Type = (DVBTChannels_Service_PID_Types)i;
			(*pStream)->PID = pid;
			return S_OK;
		}
	}
	return hr;
}


void PrintTime(__int64 value, __int64 divider)
{
	TCHAR sz[100];
	long ms = (long)(value / divider);
	long secs = ms / 1000;
	long mins = secs / 60;
	long hours = mins / 60;
	ms = ms % 1000;
	secs = secs % 60;
	mins = mins % 60;
	StringCchPrintf(sz, 100, TEXT("%02i:%02i:%02i.%03i"), hours, mins, secs, ms);
	::OutputDebugString(sz);
}


HRESULT TSFileSource::UpdateData()
{
	HRESULT hr;

	if (!m_pDWGraph->IsPlaying())
		return S_OK;

	CComQIPtr<IMediaSeeking> piMediaSeeking(m_piGraphBuilder);

	if (piMediaSeeking)
	{
		REFERENCE_TIME rtCurrent, rtEarliest, rtLatest;

		if FAILED(hr = piMediaSeeking->GetAvailable(&rtEarliest, &rtLatest))
			return (log << "Failed to get available times: " << hr << "\n").Write(hr);

		if FAILED(hr = piMediaSeeking->GetCurrentPosition(&rtCurrent))
			return (log << "Failed to get current time: " << hr << "\n").Write(hr);

/*
		REFERENCE_TIME rtCurrent2;
		CComQIPtr<IMediaSeeking> piTSMediaSeeking(m_pTSFileSource);
		if FAILED(hr = piTSMediaSeeking->GetCurrentPosition(&rtCurrent2))
			return (log << "Failed to get current time: " << hr << "\n").Write(hr);

		::OutputDebugString(TEXT("Current Positions -\t"));
		PrintTime(rtCurrent2, 10000);
		::OutputDebugString(TEXT("\t\t"));
		PrintTime(rtCurrent, 10000);
		::OutputDebugString(TEXT("\t\t"));
		PrintTime(rtCurrent2 - rtCurrent, 10000);
		::OutputDebugString(TEXT("\n"));
*/
		WCHAR sz[MAX_PATH];

		if (m_bTimeShiftService && g_pData->settings.timeshift.localTime)
		{
			_tzset();
			struct tm *tmTime;
			time_t lCurrentTime;
			time(&lCurrentTime);

			if (!m_lLocalStartTime || m_lLocalStartTime >  (lCurrentTime - rtLatest/10000000))
			{
				time(&m_lLocalStartTime);
				m_lLocalStartTime -= rtLatest/10000000;
			}

			lCurrentTime = m_lLocalStartTime + rtEarliest/10000000;;
			tmTime = localtime(&lCurrentTime);
			wcsftime(sz, 32, L"%H:%M:%S", tmTime);
			g_pOSD->Data()->SetItem(L"PositionEarliest", (LPWSTR)&sz);

			lCurrentTime = m_lLocalStartTime + rtCurrent/10000000;
			tmTime = localtime(&lCurrentTime);
			wcsftime(sz, 32, L"%H:%M:%S", tmTime);
			g_pOSD->Data()->SetItem(L"PositionCurrent", (LPWSTR)&sz);

			lCurrentTime = m_lLocalStartTime + rtLatest/10000000;
			tmTime = localtime(&lCurrentTime);
			wcsftime(sz, 32, L"%H:%M:%S", tmTime);
			g_pOSD->Data()->SetItem(L"PositionLatest", (LPWSTR)&sz);
		}
		else
		{
			long milli, secs, mins, hours;

			milli = (long)(rtEarliest / 10000);
			secs = milli / 1000;
			mins = secs / 60;
			hours = mins / 60;
			milli %= 1000;
			secs %= 60;
			mins %= 60;
			StringCchPrintfW((LPWSTR)&sz, MAX_PATH, L"%02i:%02i:%02i", hours, mins, secs, milli);
			g_pOSD->Data()->SetItem(L"PositionEarliest", (LPWSTR)&sz);

			milli = (long)(rtCurrent / 10000);
			secs = milli / 1000;
			mins = secs / 60;
			hours = mins / 60;
			milli %= 1000;
			secs %= 60;
			mins %= 60;
			StringCchPrintfW((LPWSTR)&sz, MAX_PATH, L"%02i:%02i:%02i", hours, mins, secs, milli);
			g_pOSD->Data()->SetItem(L"PositionCurrent", (LPWSTR)&sz);

			milli = (long)(rtLatest / 10000);
			secs = milli / 1000;
			mins = secs / 60;
			hours = mins / 60;
			milli %= 1000;
			secs %= 60;
			mins %= 60;
			StringCchPrintfW((LPWSTR)&sz, MAX_PATH, L"%02i:%02i:%02i", hours, mins, secs, milli);
			g_pOSD->Data()->SetItem(L"PositionLatest", (LPWSTR)&sz);
		}
	}
	
	if (!streamList.GetListSize())
		if FAILED(hr = streamList.LoadStreamList(FALSE))
			return (log << "Failed to get a Stream List: " << hr << "\n").Write(hr);
		else
		{
			if (streamList.GetServiceName())
			{
				g_pOSD->Data()->SetItem(L"CurrentService", streamList.GetServiceName() + 2);
				g_pTv->ShowOSDItem(L"Channel", g_pData->settings.application.channelOSDTime);
			}
		}

	return S_OK;
}

HRESULT TSFileSource::SetStream(long index)
{
	CAutoLock listLock(&m_listLock);
	HRESULT hr;
	CComPtr<IAMStreamSelect>pIAMStreamSelect;
	hr = m_pTSFileSource->QueryInterface(IID_IAMStreamSelect, (void**)&pIAMStreamSelect);
	if (SUCCEEDED(hr))
	{
		// Stop background thread
		if FAILED(hr = StopThread())
		{
			return (log << "Failed to stop background thread: " << hr << "\n").Write(hr);
		}
		if (hr == S_FALSE)
			(log << "Killed thread\n").Write();

		HRESULT hr2 = pIAMStreamSelect->Enable((index & 0xff), AMSTREAMSELECTENABLE_ENABLE);

		// Start the background thread for updating statistics
		if FAILED(hr = StartThread())
			return (log << "Failed to start background thread: " << hr << "\n").Write(hr);

		if FAILED (hr2)
			return (log << "Failed to Enable Stream Select: " << hr2 << "\n").Write(hr2);
	}

	return hr;
}

HRESULT TSFileSource::GetStreamList(void)
{
	CAutoLock listLock(&m_listLock);
	HRESULT hr = S_OK;

	if FAILED(hr = streamList.LoadStreamList())
		return (log << "Failed to get a Stream List: " << hr << "\n").Write(hr);

	return hr;
}

HRESULT TSFileSource::SetStreamName(LPWSTR pService, BOOL bEnable)
{
	if (!pService)
		return S_FALSE;

	HRESULT hr;

	int index = -1;
	//check if the service name is in the stream list, fail if not
	if FAILED(hr = streamList.FindServiceName(pService, &index))
		return hr;

	//check if we need to force a service selection, and if we have a go ahead
	if(hr == S_OK && bEnable)
	{
		m_pDWGraph->Mute(1);
		SetStream(index);
		m_pDWGraph->Mute(g_pData->values.audio.bMute);
	}

	return hr;
}

HRESULT TSFileSource::GetFilterList(void)
{
	HRESULT hr = S_OK;

	if FAILED(hr = filterList.LoadFilterList(TRUE))
		return (log << "Failed to get a Filter Property List: " << hr << "\n").Write(hr);

	return hr;
}

HRESULT TSFileSource::ShowFilter(LPWSTR filterName)
{
	HRESULT hr;

	// Stop background thread
	if FAILED(hr = StopThread())
		return (log << "Failed to stop background thread: " << hr << "\n").Write(hr);
	if (hr == S_FALSE)
		(log << "Killed thread\n").Write();

	if FAILED(hr = filterList.ShowFilterProperties(g_pData->hWnd, filterName, 0))
		return (log << "Failed to Show the Filter Property Page: " << hr << "\n").Write(hr);

	// Start the background thread for updating statistics
	if FAILED(hr = StartThread())
		return (log << "Failed to start background thread: " << hr << "\n").Write(hr);

	return hr;
}

BOOL TSFileSource::IsRecording()
{
		return FALSE;
}

