/*
hud_radio.cpp - radio message HUD element (Paranoia 2 vgui_radio.cpp, RTN F10)
Copyright (C) 2026 Hermes e Hidrocarboneto

HUD 2D (sem VGUI): painel com a cor (rendercolor) + alpha (renderamt),
icone do speaker4.tga a direita, nome do talker a esquerda (Roboto),
fade 0.25s (RADIO_FADE_TIME) + hold time (health) + fila do proximo.
*/

#include "hud.h"
#include "hud_radio.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "texture_handle.h"
#include "gl_local.h"

// tri.cpp - quad ortogonal 2D com a textura atual bindada
extern void OrthoQuad( int x1, int y1, int x2, int y2 );

DECLARE_MESSAGE( m_Radio, RadioIcon );

int CHudRadio::Init( void )
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( RadioIcon );
	m_iFlags |= HUD_ACTIVE;
	ResetState();
	return 1;
}

int CHudRadio::VidInit( void )
{
	ResetState();
	m_hSpeaker = LOAD_TEXTURE( "gfx/vgui/speaker4.tga", NULL, 0, TF_CLAMP | TF_IMAGE );
	return 1;
}

void CHudRadio::ResetState( void )
{
	m_bActive = false;
	m_bHasNext = false;
	m_fShowTime = 0.0f;
	m_fHideTime = 0.0f;
	m_iRed = m_iGreen = m_iBlue = 255;
	m_iAlpha = 255;
	m_szTitle[0] = m_szNextTitle[0] = 0;
}

int CHudRadio::MsgFunc_RadioIcon( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	const char *szTitle = READ_STRING();
	float fTime = READ_COORD();
	int r = READ_BYTE();
	int g = READ_BYTE();
	int b = READ_BYTE();
	int a = READ_BYTE();
	END_READ();

	float curtime = gEngfuncs.GetClientTime();

	if ( !m_bActive || curtime > m_fHideTime )
	{
		// mostra agora
		Q_strncpy( m_szTitle, szTitle, sizeof( m_szTitle ));
		m_fShowTime = curtime;
		m_fHideTime = curtime + fTime;
		m_iRed = r; m_iGreen = g; m_iBlue = b;
		m_iAlpha = a;
		m_bActive = true;
		m_bHasNext = false;
	}
	else if ( curtime > (m_fHideTime - RADIO_FADE_TIME) )
	{
		// atual no fade out - enfileira o proximo
		Q_strncpy( m_szNextTitle, szTitle, sizeof( m_szNextTitle ));
		m_fNextTime = fTime;
		m_iNextRed = r; m_iNextGreen = g; m_iNextBlue = b; m_iNextAlpha = a;
		m_bHasNext = true;
	}
	else
	{
		// esconde o atual (fade rapido) e enfileira o proximo
		m_fHideTime = curtime + RADIO_FADE_TIME;
		Q_strncpy( m_szNextTitle, szTitle, sizeof( m_szNextTitle ));
		m_fNextTime = fTime;
		m_iNextRed = r; m_iNextGreen = g; m_iNextBlue = b; m_iNextAlpha = a;
		m_bHasNext = true;
	}

	return 1;
}

int CHudRadio::Draw( float flTime )
{
	if ( !m_bActive )
		return 0;

	float curtime = gEngfuncs.GetClientTime();

	// radio terminou -> mostra o proximo (ou esconde)
	if ( curtime > m_fHideTime )
	{
		if ( m_bHasNext )
		{
			Q_strncpy( m_szTitle, m_szNextTitle, sizeof( m_szTitle ));
			m_fShowTime = curtime;
			m_fHideTime = curtime + m_fNextTime;
			m_iRed = m_iNextRed; m_iGreen = m_iNextGreen; m_iBlue = m_iNextBlue;
			m_iAlpha = m_iNextAlpha;
			m_bHasNext = false;
		}
		else
		{
			m_bActive = false;
			return 0;
		}
	}

	// alpha do fade (0.25s in / 0.25s out - estilo P2)
	float fFadeAlpha = 1.0f;
	if ( curtime < m_fShowTime + RADIO_FADE_TIME )
		fFadeAlpha = (curtime - m_fShowTime) / RADIO_FADE_TIME;
	else if ( curtime > m_fHideTime - RADIO_FADE_TIME )
		fFadeAlpha = (m_fHideTime - curtime) / RADIO_FADE_TIME;
	fFadeAlpha = Q_min( 1.0f, Q_max( 0.0f, fFadeAlpha ));

	// texto localizado (o "!entrada" do titles.txt - o mesmo do F7)
	char szText[128];
	Q_strncpy( szText, m_szTitle, sizeof( szText ));
	if ( m_szTitle[0] == '!' )
	{
		char *pLocalized = CHudTextMessage::BufferedLocaliseTextString( m_szTitle );
		if ( pLocalized && pLocalized[0] )
			Q_strncpy( szText, pLocalized, sizeof( szText ));
	}

	// medidas (o P2: x = largura - painel - 8, y = altura/2)
	// RTN F10 ajustes: icone do tamanho da FONTE do talker, nome SEM corte,
	// espacamento entre o texto e o icone (gap)
	int iconSize = gHUD.m_iFontHeight;  // o icone do tamanho da fonte do talker
	if( iconSize < 12 ) iconSize = 14;
	int textWide = (int)Q_strlen( szText ) * 12 + 8;  // ~12px/char + folga (sem corte)
	if( textWide > (ScreenWidth * 2) / 3 )
		textWide = (ScreenWidth * 2) / 3;
	int border = 2;
	int gap = 10;  // o espaco entre o nome e o icone
	int bgWide = textWide + gap + iconSize + border * 3;
	int bgTall = iconSize + border * 2;
	int x = ScreenWidth - bgWide - 8;
	int y = ScreenHeight / 2;
	int textY = y + (bgTall - gHUD.m_iFontHeight) / 2;

	// fundo do painel (a cor + o alpha do renderamt * fade)
	int a = (int)(m_iAlpha * fFadeAlpha);
	a = Q_min( 255, Q_max( 0, a ));
	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pTriAPI->Color4f( m_iRed / 255.0f, m_iGreen / 255.0f, m_iBlue / 255.0f, a / 255.0f );
	GL_Blend( GL_TRUE );  // RTN F10 fix: sem o blend o alpha nao e aplicado
	GL_Bind( 0, FIND_TEXTURE( "*white" ));
	OrthoQuad( x, y, x + bgWide, y + bgTall );

	// icone do speaker (a direita do painel - do tamanho da fonte - branco)
	if ( m_hSpeaker.Initialized( ))
	{
		int iconX = x + bgWide - iconSize - border;
		int iconY = y + (bgTall - iconSize) / 2;
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, fFadeAlpha );
		GL_Bind( 0, m_hSpeaker );
		OrthoQuad( iconX, iconY, iconX + iconSize, iconY + iconSize );
	}

	// nome do talker (Roboto - a esquerda - com folga p/ nao cortar)
	gHUD.DrawHudString( x + border, textY, x + border + textWide + 4, szText, 255, 255, 255 );

	// RTN F10 fix: restaura o estado GL (blend desligado - nao vazar)
	GL_Blend( GL_FALSE );
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );

	return 1;
}
