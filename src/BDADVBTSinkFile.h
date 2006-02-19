/**
 *	BDADVBTSinkFile.h
 *	Copyright (C) 2004 Nate
 *	Copyright (C) 2006 Bear
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

#ifndef BDADVBTSINKFILE_H
#define BDADVBTSINKFILE_H

#include "BDADVBTSink.h"
#include "StdAfx.h"
#include <bdatif.h>
#include "BDACardCollection.h"
#include "DWGraph.h"
#include "LogMessage.h"
#include "FilterGraphTools.h"
#include "DVBMpeg2DataParser.h"
#include "DWDump.h"


//{36A5F770-FE4C-11CE-A8ED-00AA002FEAB5}
//EXTERN_GUID(CLSID_FileWriterDump,
//0x36A5F770, 0xFE4C, 0x11CE, 0xA8, 0xED, 0x00, 0xAA, 0x00, 0x2F, 0xEA, 0xB5);

//{C16F5100-FD54-448E-BD99-E10CA5C088BC} hdtvpump
//EXTERN_GUID(CLSID_FileWriterDump,
//0xC16F5100, 0xFD54, 0x448E, 0xBD, 0x99, 0xE1, 0x0C, 0xA5, 0xC0, 0x88, 0xBC);

class BDADVBTSink;
class BDADVBTSinkFile : public LogMessageCaller
{
public:
	BDADVBTSinkFile(BDADVBTSink *pBDADVBTSink, BDADVBTSourceTuner *pCurrentTuner);
	virtual ~BDADVBTSinkFile();

	virtual void SetLogCallback(LogMessageCallback *callback);

	HRESULT Initialise(DWGraph *pDWGraph, int intSinkType);
	HRESULT DestroyAll();

	HRESULT AddSinkFilters(DVBTChannels_Service* pService);
	void DestroyFTSFilters();
	void DestroyTSFilters();
	void DestroyMPGFilters();
	void DestroyAVFilters();
	void DestroyFilter(CComPtr <IBaseFilter> &pFilter);
	HRESULT RemoveSinkFilters();
	void DeleteFilter(DWDump **pfDWDump);

	HRESULT SetTransportStreamPin(IPin* piPin);

	BOOL IsActive();
	HRESULT AddDWDumpFilter(LPWSTR name, DWDump **pfDWDump, CComPtr <IBaseFilter> &pFilter);
	HRESULT StartRecording(DVBTChannels_Service* pService);
	HRESULT StopRecording(void);
	HRESULT PauseRecording(void);
	HRESULT UnPauseRecording(DVBTChannels_Service* pService);
	BOOL IsRecording(void);
	BOOL IsPaused(void);

////	BOOL SetRecordingFullTS();
//	BOOL SetRecordingTSMux(long PIDs[]);
//	BOOL StartRecording(LPWSTR filename);
//	BOOL PauseRecording(BOOL bPause);
//	BOOL StopRecording();
//	BOOL IsRecording();

	BOOL SupportsRecording() { return FALSE; }

private:
	BDADVBTSink *m_pBDADVBTSink;
	BDADVBTSourceTuner *m_pCurrentTuner;
	DWGraph *m_pDWGraph;

	BOOL m_bInitialised;
	BOOL m_bActive;


	BOOL m_bRecording;
	BOOL m_bPaused;

	CComPtr <IGraphBuilder> m_piGraphBuilder;
	CComPtr <IMediaControl> m_piMediaControl;

	CComPtr <IBaseFilter> m_piInfinitePinTee;

	int m_intSinkType;

	CComPtr <IBaseFilter> m_piTelexSink;
	DWDump *m_piTelexDWDump;
	CComPtr <IBaseFilter> m_piVideoSink;
	DWDump *m_piVideoDWDump;
	CComPtr <IBaseFilter> m_piAudioSink;
	DWDump *m_piAudioDWDump;
	CComPtr <IBaseFilter> m_piAVMpeg2Demux;
	CComPtr <IBaseFilter> m_piMPGSink;
	DWDump *m_piMPGDWDump;
	CComPtr <IBaseFilter> m_piMPGMpeg2Mux;
	CComPtr <IBaseFilter> m_piMPGMpeg2Demux;
	CComPtr <IBaseFilter> m_piTSSink;
	DWDump *m_piTSDWDump;
	CComPtr <IBaseFilter> m_piTSMpeg2Demux;
	CComPtr <IBaseFilter> m_piFTSSink;
	DWDump *m_piFTSDWDump;

	/*
	CComPtr <IGuideData> m_piGuideData;
	DWGuideDataEvent* m_poGuideDataEvent;
	*/
	DVBMpeg2DataParser *m_pMpeg2DataParser;

	DWORD m_rotEntry;

	FilterGraphTools graphTools;
};

#endif