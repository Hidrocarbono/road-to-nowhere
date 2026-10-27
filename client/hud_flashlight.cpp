/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// flashlight.cpp
//
// implementation of CHudFlashlight class
//

#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "gl_nvg.h"	// RTN: RTN_NVG_IsActive()/RTN_NVG_GetBattery() - ver Draw()

DECLARE_MESSAGE( m_Flash, FlashBat )
DECLARE_MESSAGE( m_Flash, Flashlight )

int CHudFlashlight::Init( void) 
{
	m_fFade = 0;
	m_fOn = 0;

	HOOK_MESSAGE( Flashlight );
	HOOK_MESSAGE( FlashBat );

	m_iFlags |= HUD_ACTIVE;

	gHUD.AddHudElem( this );

	return 1;
}

void CHudFlashlight::Reset( void )
{
	m_fFade = 0;
	m_fOn = 0;
}

int CHudFlashlight::VidInit( void )
{
	int HUD_flash_empty = gHUD.GetSpriteIndex( "flash_empty" );
	int HUD_flash_full = gHUD.GetSpriteIndex( "flash_full" );
	int HUD_flash_beam = gHUD.GetSpriteIndex( "flash_beam" );

	m_hSprite1 = gHUD.GetSprite( HUD_flash_empty );
	m_hSprite2 = gHUD.GetSprite( HUD_flash_full );
	m_hBeam = gHUD.GetSprite( HUD_flash_beam );
	m_prc1 = &gHUD.GetSpriteRect( HUD_flash_empty );
	m_prc2 = &gHUD.GetSpriteRect( HUD_flash_full );
	m_prcBeam = &gHUD.GetSpriteRect( HUD_flash_beam );
	m_iWidth = m_prc2->right - m_prc2->left;

	// RTN: icone do NVG, desenhado no mesmo canto quando o NVG esta ativo
	// (ver Draw()) - LoadSprite() ja resolve 320/640 sozinho, igual aos
	// icones de hud_status.cpp.
	m_hNVGIcon = LoadSprite( "sprites/rtn_hud_nvg_icon.spr" );

	return 1;
}

int CHudFlashlight:: MsgFunc_FlashBat( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	m_iBat = READ_BYTE();
	m_flBat = ((float)m_iBat ) / 100.0f;

	END_READ();

	return 1;
}

int CHudFlashlight:: MsgFunc_Flashlight( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	m_fOn = READ_BYTE();
	m_iBat = READ_BYTE();
	m_flBat = ((float)m_iBat ) / 100.0f;

	END_READ();

	return 1;
}

int CHudFlashlight::Draw( float flTime )
{
	if( gHUD.m_iHideHUDDisplay & ( HIDEHUD_FLASHLIGHT|HIDEHUD_ALL ))
		return 1;

	if (!gHUD.HasWeapon( WEAPON_SUIT ))
		return 1;

	int r, g, b, x, y, a;
	wrect_t rc;

	// RTN: lanterna e NVG disputam o mesmo canto superior direito. Com o NVG
	// ligado, o icone dele substitui o da lanterna por completo (nao importa
	// se a lanterna tambem esta ligada por baixo) - pedido do usuario. Sem
	// NVG, a lanterna volta a mandar: some da tela enquanto ela estiver em
	// uso (nao faz mais sentido mostrar a barra de bateria - a lanterna e
	// infinita, ver player.cpp), e continua aparecendo apagada quando ela
	// nao esta em uso, como sempre foi.
	if( RTN_NVG_IsActive( ))
	{
		if( !m_hNVGIcon )
			return 1;

		int nvgBat = RTN_NVG_GetBattery();

		if( nvgBat < 20 )
			UnpackRGB( r, g, b, RGB_REDISH );
		else
		{
			r = gHUD.m_color.r;
			g = gHUD.m_color.g;
			b = gHUD.m_color.b;
		}

		a = 225;
		ScaleColors( r, g, b, a );

		int iconW = SPR_Width( m_hNVGIcon, 0 );
		int iconH = SPR_Height( m_hNVGIcon, 0 );
		x = ScreenWidth - iconW - iconW / 2;
		y = ( m_prc1->bottom - m_prc2->top ) / 2 - ( iconH - ( m_prc1->bottom - m_prc1->top )) / 2;

		SPR_Set( m_hNVGIcon, r, g, b );
		SPR_DrawAdditive( 0, x, y, NULL );

		return 1;
	}

	if( m_fOn )
		return 1;	// lanterna em uso: oculta o icone dela (pedido do usuario)

	// a partir daqui m_fOn e sempre falso (o caso ligado ja retornou acima) -
	// so sobra o estado "apagada, disponivel", igual ao comportamento antigo.
	a = MIN_ALPHA;

	if (m_flBat < 0.2f) {
		UnpackRGB(r, g, b, RGB_REDISH);
	}
	else
	{
		r = gHUD.m_color.r;
		g = gHUD.m_color.g;
		b = gHUD.m_color.b;
	}

	ScaleColors( r, g, b, a );

	y = ( m_prc1->bottom - m_prc2->top ) / 2;
	x = ScreenWidth - m_iWidth - m_iWidth / 2;

	// Draw the flashlight casing
	SPR_Set( m_hSprite1, r, g, b );
	SPR_DrawAdditive( 0,  x, y, m_prc1 );

	// draw the flashlight energy level
	x = ScreenWidth - m_iWidth - m_iWidth / 2;
	int iOffset = m_iWidth * ( 1.0f - m_flBat );

	if( iOffset < m_iWidth )
	{
		rc = *m_prc2;
		rc.left += iOffset;

		SPR_Set( m_hSprite2, r, g, b );
		SPR_DrawAdditive( 0, x + iOffset, y, &rc );
	}
	return 1;
}
