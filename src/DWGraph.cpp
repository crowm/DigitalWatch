/**
 *	DWGraph.cpp
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

#include "DWGraph.h"
#include "Globals.h"

#include <mpconfig.h>
#include <ks.h> // Must be included before ksmedia.h
#include <ksmedia.h> // Must be included before bdamedia.h
#include "bdamedia.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DWGraph::DWGraph()
{
	m_piGraphBuilder = NULL;
	m_piMediaControl = NULL;
	m_bInitialised = FALSE;
	m_rotEntry = 0;
}

DWGraph::~DWGraph()
{
	Destroy();
}

void DWGraph::SetLogCallback(LogMessageCallback *callback)
{
	LogMessageCaller::SetLogCallback(callback);

	m_mediaTypes.SetLogCallback(callback);
	m_decoders.SetLogCallback(callback);
	graphTools.SetLogCallback(callback);
}

BOOL DWGraph::Initialise()
{
	(log << "Initialising DWGraph\n").Write();
	LogMessageIndent indent(&log);

	HRESULT hr;
	if (m_bInitialised)
		return (log << "DigitalWatch graph tried to initialise a second time\n").Write();

	wchar_t file[MAX_PATH];

	swprintf((LPWSTR)&file, L"%sDecoders.xml", g_pData->application.appPath);
	if FAILED(hr = m_decoders.Load((LPWSTR)&file))
		return (log << "Failed to load decoders: " << hr << "\n").Write(hr);

	m_mediaTypes.SetDecoders(&m_decoders);
	swprintf((LPWSTR)&file, L"%sMediaTypes.xml", g_pData->application.appPath);
	if FAILED(hr = m_mediaTypes.Load((LPWSTR)&file))
		return (log << "Failed to load mediatypes: " << hr << "\n").Write(hr);


	//--- COM should already be initialized ---

	//--- Create Graph ---
	if FAILED(hr = m_piGraphBuilder.CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER))
		return (log << "Failed Creating Graph Builder: " << hr << "\n").Write();

	//--- Add To Running Object Table --- (for graphmgr.exe)
	if (g_pData->settings.application.addToROT)
	{
		if FAILED(hr = graphTools.AddToRot(m_piGraphBuilder, &m_rotEntry))
		{
			//TODO: release graphbuilder
			return (log << "Failed adding graph to ROT: " << hr << "\n").Write();
		}
	}

	//--- Get InterfacesInFilters ---
	if FAILED(hr = m_piGraphBuilder->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&m_piMediaControl)))
		return (log << "Failed to get Media Control interface: " << hr << "\n").Write();

//	m_pOverlayCallback = new OverlayCallback(g_pData->hWnd, &hr);
	m_bInitialised = TRUE;

	indent.Release();
	(log << "Finished Initialising DWGraph\n").Write();

	return TRUE;
}

BOOL DWGraph::Destroy()
{
	if (m_piMediaControl)
		m_piMediaControl.Release();

	if (m_rotEntry)
	{
		graphTools.RemoveFromRot(m_rotEntry);
		m_rotEntry = 0;
	}

	if (m_piGraphBuilder)
		m_piGraphBuilder.Release();

	m_bInitialised = FALSE;

	return TRUE;
}

HRESULT DWGraph::QueryGraphBuilder(IGraphBuilder** piGraphBuilder)
{
	if (!m_piGraphBuilder)
		return E_POINTER;
	*piGraphBuilder = m_piGraphBuilder;
	(*piGraphBuilder)->AddRef();
	return S_OK;
}
	
HRESULT DWGraph::QueryMediaControl(IMediaControl** piMediaControl)
{
	if (!m_piMediaControl)
		return E_POINTER;
	*piMediaControl = m_piMediaControl;
	(*piMediaControl)->AddRef();
	return S_OK;
}
	
HRESULT DWGraph::Start()
{
	(log << "Stopping DW Graph\n").Write();
	LogMessageIndent indent(&log);

	if (m_piMediaControl == NULL)
		return (log << "Media Control interface is NULL\n").Write(E_POINTER);

	HRESULT hr;

	//Set the video renderer to use our window.
	CComQIPtr <IVideoWindow> piVideoWindow(m_piGraphBuilder);
	if (piVideoWindow)
	{
		if FAILED(hr = piVideoWindow->put_Owner((OAHWND)g_pData->hWnd))
			return (log << "could not set IVideoWindow Window Handle: " << hr << "\n").Write(hr);

		if FAILED(hr = piVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS))
			return (log << "could not set IVideoWindow Window Style: " << hr << "\n").Write(hr);

		if FAILED(hr = piVideoWindow->put_MessageDrain((OAHWND)g_pData->hWnd))
			return (log << "could not set IVideoWindow Message Drain: " << hr << "\n").Write(hr);

		//if FAILED(hr = piVideoWindow->put_AutoShow(OAFALSE))
		//	return (log << "could not set IVideoWindow AutoShow: " << hr << "\n").Write(hr);
	}

	hr = RefreshVideoPosition();
	if FAILED(hr)
		return (log << "Failed to refresh video posistion: " << hr << "\n").Write(hr);

/*	IBaseFilter *pfOverlayMixer = NULL;
	hr = GetOverlayMixer(m_piGraphBuilder, &pfOverlayMixer);
	if (hr == S_OK)
	{
		IDDrawExclModeVideo* piDDrawExclModeVideo = NULL;
		hr = pfOverlayMixer->QueryInterface(IID_IDDrawExclModeVideo, reinterpret_cast<void**>(piDDrawExclModeVideo));
		if (hr == S_OK)
		{
			piDDrawExclModeVideo->SetCallbackInterface(m_pOverlayCallback, 0);
			HelperRelease(piDDrawExclModeVideo);
		}
		HelperRelease(pfOverlayMixer);
	}
*/
	hr = m_piMediaControl->Run();
	if FAILED(hr)
		return (log << "Failed to start graph: " << hr << "\n").Write(hr);

	indent.Release();
	(log << "Stopping DW Graph\n").Write();

	return hr;
}

HRESULT DWGraph::Stop()
{
	(log << "Stopping DW Graph\n").Write();
	LogMessageIndent indent(&log);

	HRESULT hr = m_piMediaControl->Stop();
	if FAILED(hr)
		return (log << "Failed to stop graph: " << hr << "\n").Write(hr);

	indent.Release();
	(log << "Finished Stopping DW Graph\n").Write();

	return hr;
}

HRESULT DWGraph::Cleanup()
{
	(log << "Cleaning up DW Graph\n").Write();
	LogMessageIndent indent(&log);

	HRESULT hr;
/*	CComQIPtr<IFilterChain> piFilterChain(m_piGraphBuilder);
	if (piFilterChain == NULL)
		return (log << "Failed to query IFilterChain\n").Write(E_NOINTERFACE);

	piFilterChain->RemoveChain(filter, NULL);*/

	hr = graphTools.DisconnectAllPins(m_piGraphBuilder);
	if FAILED(hr)
		(log << "Failed to disconnect pins: " << hr << "\n").Write(hr);

	hr = graphTools.RemoveAllFilters(m_piGraphBuilder);
	if FAILED(hr)
		(log << "Failed to remove filters: " << hr << "\n").Write(hr);

	indent.Release();
	(log << "Finished Cleaning up DW Graph\n").Write();

	return S_OK;
}

HRESULT DWGraph::RenderPin(IPin *piPin)
{
	if (!piPin)
		return E_INVALIDARG;

	HRESULT hr;

	CComPtr <IEnumMediaTypes> piMediaTypes;
	if FAILED(hr = piPin->EnumMediaTypes(&piMediaTypes))
	{
		return (log << "Failed to enum media types: " << hr << "\n").Write(hr);
	}

	hr = S_FALSE;
	AM_MEDIA_TYPE *mediaType;
	while (piMediaTypes->Next(1, &mediaType, 0) == NOERROR)
	{
		DWMediaType *dwMediaType = m_mediaTypes.FindMediaType(mediaType);
		if (dwMediaType == NULL)
			continue;

		DWDecoder *dwDecoder = dwMediaType->get_Decoder();
		if (dwDecoder == NULL)
			continue;

		if SUCCEEDED(hr = dwDecoder->AddFilters(m_piGraphBuilder, piPin))
			break;

		//hr = m_piGraphBuilder->Render(piPin);
	}
	piMediaTypes.Release();

	return hr;
}

HRESULT DWGraph::RefreshVideoPosition()
{
//	if (!m_bInitialised || (m_pAppData->bPlaying == FALSE))
//		return FALSE;
//	if (!m_bVideo)
//		return TRUE;
	HRESULT hr = S_OK;

	CComQIPtr <IVideoWindow> piVideoWindow(m_piGraphBuilder);
	if (!piVideoWindow)
		return (log << "Could not query graph builder for IVideoWindow\n").Write(S_FALSE);

	CComQIPtr <IBasicVideo> piBasicVideo(m_piGraphBuilder);
	if (!piBasicVideo)
		return (log << "could not query graph builder for IBasicVideo\n").Write(S_FALSE);

	RECT mainRect, zoomRect;
	GetVideoRect(&zoomRect);
	GetClientRect(g_pData->hWnd, &mainRect);
	
	//RECT srcRect;
	//srcRect.left = 0; srcRect.top = 0;
	//hr = piBasicVideo->GetVideoSize(&srcRect.right, &srcRect.bottom);
	//if (hr == S_OK) hr = piBasicVideo->SetSourcePosition(srcRect.left, srcRect.top, srcRect.right-srcRect.left, srcRect.bottom-srcRect.top);

	//SetWindowPos(g_pData->hWnd, NULL, mainRect.left, mainRect.top, mainRect.right-mainRect.left, mainRect.bottom-mainRect.top, /*SWP_NOMOVE | */SWP_NOZORDER);
	if FAILED(hr = piVideoWindow->SetWindowPosition(0, 0, mainRect.right-mainRect.left, mainRect.bottom-mainRect.top))
		return (log << "could not set IVideoWindow position: " << hr << "\n").Write(hr);

	if (hr == S_OK)
		hr = piBasicVideo->SetDestinationPosition(zoomRect.left, zoomRect.top, zoomRect.right-zoomRect.left, zoomRect.bottom-zoomRect.top);

	//Set overlay to streched AR mode
	CComPtr <IPin> piPin;
	hr = graphTools.GetOverlayMixerInputPin(m_piGraphBuilder, L"Input0", &piPin);
	if (hr == S_OK)
	{
		CComPtr <IMixerPinConfig> piMixerPinConfig;
		hr = piPin->QueryInterface(IID_IMixerPinConfig, reinterpret_cast<void**>(&piMixerPinConfig));
		if (hr == S_OK)
		{
			hr = piMixerPinConfig->SetAspectRatioMode(AM_ARMODE_STRETCHED);
		}
	}

	if FAILED(hr = piVideoWindow->put_Visible(OATRUE))
		return (log << "could not set IVideoWindow visible: " << hr << "\n").Write(hr);

	Sleep(10);

	return S_OK;
}

void DWGraph::GetVideoRect(RECT *rect)
{
	GetClientRect(g_pData->hWnd, rect);

	double ar   = 0;
	if (g_pData->values.display.aspectRatio.height > 0)
		ar = g_pData->values.display.aspectRatio.width / g_pData->values.display.aspectRatio.height;
	double zoom = g_pData->values.display.zoom / 100.0;
	
	if (ar >= 0)
	{
		long windowWidth  = (rect->right  - rect->left);
		long windowHeight = (rect->bottom - rect->top );
		long newWidth, newHeight;

		switch (g_pData->values.display.zoomMode)
		{
		case 1:
			{
				newWidth  = long(windowHeight * ar * zoom);
				newHeight = long(windowHeight * zoom);

				rect->left   = long((windowWidth  - newWidth ) / 2);
				rect->top    = long((windowHeight - newHeight) / 2);
				rect->right  = rect->left + newWidth;
				rect->bottom = rect->top  + newHeight;
			}
			break;
		default:
			{
				if ((windowWidth/(double)windowHeight) < ar)
				{
					newWidth  = long(windowWidth * zoom);
					newHeight = long(windowWidth * (1.0 / ar) * zoom);

					rect->left   = long((windowWidth  - newWidth ) / 2);
					rect->top    = long((windowHeight - newHeight) / 2);
					rect->right  = rect->left + newWidth;
					rect->bottom = rect->top  + newHeight;
				}
				else
				{
					newWidth  = long(windowHeight * ar * zoom);
					newHeight = long(windowHeight * zoom);

					rect->left   = long((windowWidth  - newWidth ) / 2);
					rect->top    = long((windowHeight - newHeight) / 2);
					rect->right  = rect->left + newWidth;
					rect->bottom = rect->top  + newHeight;
				}
			}
			break;
		}
	}
}

