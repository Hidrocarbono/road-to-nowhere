#include "hud.h"          // primeiro: define CHudBase e inclui hud_interact.h (pos CHudBase)
#include "hud_interact.h"
#include "utils.h"
#include "parsemsg.h"
#include "enginecallback.h"

// RTN F10: MAOZINHA DE INTERACAO (estilo Paranoia 2).
// Recebe "CanUse" (1 byte) do server e desenha o icone de uso centralizado.
// Sprite: sprites/rtn_hud_usage.spr (convertido do gfx/vgui/640_usage.tga).

DECLARE_MESSAGE( m_Interact, CanUse );  // gera __MsgFunc_CanUse -> gHUD.m_Interact.MsgFunc_CanUse

int CHudInteract::Init( void )
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( CanUse );
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudInteract::VidInit( void )
{
	m_hUsageSpr = LoadSprite( "sprites/rtn_hud_usage.spr" );
	m_bShow = false;
	return 1;
}

void CHudInteract::Reset( void )
{
	m_bShow = false;
}

int CHudInteract::MsgFunc_CanUse( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	m_bShow = READ_BYTE() != 0;
	END_READ();
	return 1;
}

int CHudInteract::Draw( float flTime )
{
	extern cvar_t *rtn_hud_style;
	if( !rtn_hud_style || rtn_hud_style->value < 1.0f )
		return 0;

	if( !m_bShow || !m_hUsageSpr )
		return 0;

	// centralizado, abaixo da crosshair (estilo P2: ScreenHeight/2 + 100)
	int w = SPR_Width( m_hUsageSpr, 0 );
	int h = SPR_Height( m_hUsageSpr, 0 );
	int x = ScreenWidth / 2 - w / 2;
	int y = ScreenHeight / 2 + YRES( 80 ) - h / 2;

	SPR_Set( m_hUsageSpr, 255, 255, 255 );
	SPR_Draw( 0, x, y, NULL );

	return 1;
}
