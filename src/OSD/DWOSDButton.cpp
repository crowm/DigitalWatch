/**
 *	DWOSDButton.cpp
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

#include "DWOSDButton.h"
#include "Globals.h"
#include "GlobalFunctions.h"

//////////////////////////////////////////////////////////////////////
// DWOSDButton
//////////////////////////////////////////////////////////////////////

DWOSDButton::DWOSDButton(DWSurface* pSurface) : DWOSDControl(pSurface)
{
	m_nPosX = 0;
	m_nPosY = 0;
	m_nWidth = 0;
	m_nHeight = 0;
	m_wszText = NULL;
	m_wszFont = NULL;
	m_dwTextColor = 0;
	m_nTextHeight = 40;
	m_nTextWeight = 400;

	m_pBackgroundImage = NULL;
	m_pHighlightImage = NULL;
}

DWOSDButton::~DWOSDButton()
{
	if (m_wszText)
		delete[] m_wszText;
}

HRESULT DWOSDButton::LoadFromXML(XMLElement *pElement)
{
	DWOSDControl::LoadFromXML(pElement);

	XMLAttribute *attr;
	XMLElement *element = NULL;
	XMLElement *subelement = NULL;

	int elementCount = pElement->Elements.Count();
	for ( int item=0 ; item<elementCount ; item++ )
	{
		element = pElement->Elements.Item(item);
		if (_wcsicmp(element->name, L"pos") == 0)
		{
			attr = element->Attributes.Item(L"x");
			if (attr)
				m_nPosX = _wtoi(attr->value);

			attr = element->Attributes.Item(L"y");
			if (attr)
				m_nPosY = _wtoi(attr->value);
		}
		else if (_wcsicmp(element->name, L"size") == 0)
		{
			attr = element->Attributes.Item(L"width");
			if (attr)
				m_nWidth = _wtoi(attr->value);

			attr = element->Attributes.Item(L"height");
			if (attr)
				m_nHeight = _wtoi(attr->value);
		}
		else if (_wcsicmp(element->name, L"text") == 0)
		{
			if (element->value)
				strCopy(m_wszText, element->value);
		}
		else if (_wcsicmp(element->name, L"font") == 0)
		{
			attr = element->Attributes.Item(L"name");
			if (attr)
				strCopy(m_wszFont, attr->value);

			attr = element->Attributes.Item(L"height");
			if (attr)
				m_nTextHeight = _wtoi(attr->value);

			attr = element->Attributes.Item(L"weight");
			if (attr)
				m_nTextWeight = _wtoi(attr->value);

			attr = element->Attributes.Item(L"color");
			if (attr)
				m_dwTextColor = wcsToColor(attr->value);
		}
		else if (_wcsicmp(element->name, L"background") == 0)
		{
			int subElementCount = element->Elements.Count();
			for ( int subitem=0 ; subitem<subElementCount ; subitem++ )
			{
				subelement = element->Elements.Item(subitem);
				if (_wcsicmp(subelement->name, L"image") == 0)
				{
					if (subelement->value)
						m_pBackgroundImage = g_pOSD->GetImage(subelement->value);
				}
				else if (_wcsicmp(subelement->name, L"highlightImage") == 0)
				{
					if (subelement->value)
						m_pHighlightImage = g_pOSD->GetImage(subelement->value);
				}
			}
		}
		else if (_wcsicmp(element->name, L"highlight") == 0)
		{
			m_bCanHighlight = TRUE;
			
			attr = element->Attributes.Item(L"default");
			if (attr)
				m_bHighlighted = TRUE;

			int subElementCount = element->Elements.Count();
			for ( int subitem=0 ; subitem<subElementCount ; subitem++ )
			{
				subelement = element->Elements.Item(subitem);
				if (_wcsicmp(subelement->name, L"onSelect") == 0)
				{
					if (subelement->value)
						strCopy(m_pwcsCommand, subelement->value);
				}
				else if (_wcsicmp(subelement->name, L"onUp") == 0)
				{
					if (subelement->value)
						strCopy(m_pwcsControlUp, subelement->value);
				}
				else if (_wcsicmp(subelement->name, L"onDown") == 0)
				{
					if (subelement->value)
						strCopy(m_pwcsControlDown, subelement->value);
				}
				else if (_wcsicmp(subelement->name, L"onLeft") == 0)
				{
					if (subelement->value)
						strCopy(m_pwcsControlLeft, subelement->value);
				}
				else if (_wcsicmp(subelement->name, L"onRight") == 0)
				{
					if (subelement->value)
						strCopy(m_pwcsControlRight, subelement->value);
				}
			}
		}
	}

	return S_OK;
}

HRESULT DWOSDButton::Draw(long tickCount)
{
	USES_CONVERSION;

	HRESULT hr;

	LPWSTR pStr = NULL;
	//Replace Tokens
	g_pOSD->data.ReplaceTokens(m_wszText, pStr);

	if (pStr[0] == '\0')
		return S_OK;

	DWSurfaceText text;
	strCopy(text.text, pStr);

	//Set Font
	ZeroMemory(&text.font, sizeof(LOGFONT));
	text.font.lfHeight = m_nTextHeight;
	text.font.lfWeight = m_nTextWeight;
	text.font.lfOutPrecision = OUT_OUTLINE_PRECIS; //OUT_DEVICE_PRECIS;
	text.font.lfQuality = ANTIALIASED_QUALITY;
	lstrcpy(text.font.lfFaceName, (m_wszFont) ? W2A(m_wszFont) : TEXT("Arial"));
	
	text.crTextColor = m_dwTextColor;
	
	if (m_bHighlighted)
	{
		if (m_pHighlightImage)
			m_pHighlightImage->Draw(m_pSurface, m_nPosX, m_nPosY, m_nWidth, m_nHeight);
		else
		{
			//TODO: draw something since no image was supplied
		}
	}
	else
	{
		if (m_pBackgroundImage)
			m_pBackgroundImage->Draw(m_pSurface, m_nPosX, m_nPosY, m_nWidth, m_nHeight);
		else
		{
			//TODO: draw something since no image was supplied
		}
	}

	SIZE extent;
	hr = text.GetTextExtent(&extent);

	long nPosX = m_nPosX + (m_nWidth / 2)  - (extent.cx / 2);
	long nPosY = m_nPosY + (m_nHeight / 2) - (extent.cy / 2);

	m_pSurface->DrawText(&text, nPosX, nPosY);

	return S_OK;
}

