#include "hud.h"          // primeiro: define CHudBase e inclui hud_rtn_items.h (pos CHudBase)
#include "hud_rtn_items.h"
#include "utils.h"
#include "parsemsg.h"

// RTN F9: contadores laterais de estimulante (V) e painkiller (H).
// Desenho estilo Paranoia 2: caixinha colorida com inicial + numero de doses,
// na lateral ESQUERDA da tela (x=10), acima da vida (que fica embaixo).

#define RTN_ITEMS_X		10
#define RTN_STIM_Y		300	// estimulante (verde)
#define RTN_PAIN_Y		340	// painkiller (branco/azul)
#define RTN_ICON_SIZE	24

DECLARE_MESSAGE( m_RTNItems, RTNItems );  // gera __MsgFunc_RTNItems -> gHUD.m_RTNItems.MsgFunc_RTNItems

int CHudRTNItems::Init( void )
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( RTNItems );
	return 1;
}

int CHudRTNItems::VidInit( void )
{
	m_iStimDoses = 0;
	m_iPainDoses = 0;
	return 1;
}

void CHudRTNItems::Reset( void )
{
	m_iStimDoses = 0;
	m_iPainDoses = 0;
}

int CHudRTNItems::MsgFunc_RTNItems( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	m_iStimDoses = READ_SHORT();
	m_iPainDoses = READ_SHORT();
	END_READ();
	return 1;
}

int CHudRTNItems::Draw( float flTime )
{
	if( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL )
		return 1;

	// --- Estimulante (verde, "E") ---
	if( m_iStimDoses > 0 )
	{
		FillRGBA( RTN_ITEMS_X, RTN_STIM_Y, RTN_ICON_SIZE, RTN_ICON_SIZE, 30, 160, 60, 200 );
		// borda
		FillRGBA( RTN_ITEMS_X, RTN_STIM_Y, RTN_ICON_SIZE, 2, 255, 255, 255, 120 );
		FillRGBA( RTN_ITEMS_X, RTN_STIM_Y + RTN_ICON_SIZE - 2, RTN_ICON_SIZE, 2, 255, 255, 255, 120 );
		FillRGBA( RTN_ITEMS_X, RTN_STIM_Y, 2, RTN_ICON_SIZE, 255, 255, 255, 120 );
		FillRGBA( RTN_ITEMS_X + RTN_ICON_SIZE - 2, RTN_STIM_Y, 2, RTN_ICON_SIZE, 255, 255, 255, 120 );

		// letra "E" centralizada
		int tx = RTN_ITEMS_X + RTN_ICON_SIZE / 2 - 4;
		int ty = RTN_STIM_Y + RTN_ICON_SIZE / 2 - 6;
		gHUD.DrawHudString( tx, ty, tx + 20, "E", 255, 255, 255 );

		// contador de doses ao lado
		gHUD.DrawHudNumber( RTN_ITEMS_X + RTN_ICON_SIZE + 8, RTN_STIM_Y + 4, DHN_3DIGITS, m_iStimDoses, 30, 160, 60 );
	}

	// --- Painkiller (branco/azul, "P") ---
	if( m_iPainDoses > 0 )
	{
		FillRGBA( RTN_ITEMS_X, RTN_PAIN_Y, RTN_ICON_SIZE, RTN_ICON_SIZE, 60, 120, 200, 200 );
		FillRGBA( RTN_ITEMS_X, RTN_PAIN_Y, RTN_ICON_SIZE, 2, 255, 255, 255, 120 );
		FillRGBA( RTN_ITEMS_X, RTN_PAIN_Y + RTN_ICON_SIZE - 2, RTN_ICON_SIZE, 2, 255, 255, 255, 120 );
		FillRGBA( RTN_ITEMS_X, RTN_PAIN_Y, 2, RTN_ICON_SIZE, 255, 255, 255, 120 );
		FillRGBA( RTN_ITEMS_X + RTN_ICON_SIZE - 2, RTN_PAIN_Y, 2, RTN_ICON_SIZE, 255, 255, 255, 120 );

		int tx = RTN_ITEMS_X + RTN_ICON_SIZE / 2 - 4;
		int ty = RTN_PAIN_Y + RTN_ICON_SIZE / 2 - 6;
		gHUD.DrawHudString( tx, ty, tx + 20, "P", 255, 255, 255 );

		gHUD.DrawHudNumber( RTN_ITEMS_X + RTN_ICON_SIZE + 8, RTN_PAIN_Y + 4, DHN_3DIGITS, m_iPainDoses, 60, 120, 200 );
	}

	return 1;
}
