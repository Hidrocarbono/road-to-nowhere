/*
hud_radio.h - radio message HUD element (Paranoia 2 vgui_radio.cpp, RTN F10)
Copyright (C) 2026 Hermes e Hidrocarboneto

Shows the speaker icon (gfx/vgui/speaker4.tga) + talker name on the right
side of the screen (mid height) with a 0.25s fade in/out, hold time and a
queue for the next radio sentence. HUD 2D (no VGUI - RTN style).
*/

#ifndef HUD_RADIO_H
#define HUD_RADIO_H

#include "hud.h"
#include "texture_handle.h"

#define RADIO_FADE_TIME 0.25f

class CHudRadio : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_RadioIcon( const char *pszName, int iSize, void *pbuf );

private:
	void ResetState( void );

	bool m_bActive;
	char m_szTitle[128];	// talker name (ou "!entrada" do titles.txt)
	float m_fShowTime;
	float m_fHideTime;
	int m_iRed, m_iGreen, m_iBlue;
	int m_iAlpha;
	// fila do proximo radio (se outro tocar durante o atual)
	bool m_bHasNext;
	char m_szNextTitle[128];
	float m_fNextTime;
	int m_iNextRed, m_iNextGreen, m_iNextBlue, m_iNextAlpha;
	TextureHandle m_hSpeaker;	// icone do speaker4.tga (gfx/vgui/)
};

#endif // HUD_RADIO_H
