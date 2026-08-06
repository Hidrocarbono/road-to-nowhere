#include "hud.h"          // primeiro: define CHudBase e inclui hud_rtn_items.h (pos CHudBase)
#include "hud_rtn_items.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "enginecallback.h"

// RTN F9: contadores laterais de estimulante (V) e painkiller (H).
// Desenho estilo Paranoia 2: caixinha com ICONE (sprite .spr) + numero de doses,
// na lateral ESQUERDA da tela (x=10), acima da vida (que fica embaixo).
// Fallback: se o sprite nao carregar, desenha a letra inicial (E/P).
//
// v3 (build #94): usa .spr nativo do engine (SPR_Load + SPR_Set + SPR_DrawAdditive)
// em vez de TGA + pTriAPI. O pTriAPI->Color4f nao aplica cor/textura no HUD 2D
// deste engine (o quad desenhava mas ficava invisivel). O .spr e o caminho nativo
// que o health.cpp usa e FUNCIONA. Conversor: tools/tga2spr.py.

#define RTN_ITEMS_X		10
#define RTN_STIM_Y		300	// estimulante (verde)
#define RTN_PAIN_Y		340	// painkiller (branco/azul)
#define RTN_ICON_SIZE	24

DECLARE_MESSAGE( m_RTNItems, RTNItems );  // gera __MsgFunc_RTNItems -> gHUD.m_RTNItems.MsgFunc_RTNItems

static SpriteHandle g_hStimIcon = 0;
static SpriteHandle g_hPainIcon = 0;

// desenha a caixinha (fundo + borda) e o conteudo (icone OU letra de fallback)
static void RTN_DrawItemSlot( int y, int r, int g, int b, SpriteHandle hSpr, const char *pszLetter, int iDoses )
{
	FillRGBA( RTN_ITEMS_X, y, RTN_ICON_SIZE, RTN_ICON_SIZE, r, g, b, 200 );
	// borda
	FillRGBA( RTN_ITEMS_X, y, RTN_ICON_SIZE, 2, 255, 255, 255, 120 );
	FillRGBA( RTN_ITEMS_X, y + RTN_ICON_SIZE - 2, RTN_ICON_SIZE, 2, 255, 255, 255, 120 );
	FillRGBA( RTN_ITEMS_X, y, 2, RTN_ICON_SIZE, 255, 255, 255, 120 );
	FillRGBA( RTN_ITEMS_X + RTN_ICON_SIZE - 2, y, 2, RTN_ICON_SIZE, 255, 255, 255, 120 );

	if( hSpr )
	{
		// icone .spr nativo do engine - funciona no HUD 2D (como o health.cpp)
		SPR_Set( hSpr, 255, 255, 255 );
		SPR_DrawAdditive( 0, RTN_ITEMS_X + 1, y + 1, NULL );
	}
	else
	{
		// fallback: letra inicial centralizada
		int tx = RTN_ITEMS_X + RTN_ICON_SIZE / 2 - 4;
		int ty = y + RTN_ICON_SIZE / 2 - 6;
		char szLetter[2] = { pszLetter[0], '\0' };
		gHUD.DrawHudString( tx, ty, tx + 20, szLetter, 255, 255, 255 );
	}

	// contador de doses ao lado
	gHUD.DrawHudNumber( RTN_ITEMS_X + RTN_ICON_SIZE + 8, y + 4, DHN_3DIGITS, iDoses, r, g, b );
}

int CHudRTNItems::Init( void )
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( RTNItems );
	m_iFlags |= HUD_ACTIVE;  // RTN F10 fix: sem isso o Redraw() nao chama Draw() -> numeros somem
	return 1;
}

int CHudRTNItems::VidInit( void )
{
	m_iStimDoses = 0;
	m_iPainDoses = 0;

	// icones .spr nativos (gerados dos TGA via tools/tga2spr.py).
	// Se o arquivo nao existir, hSpr fica 0 -> fallback letra.
	g_hStimIcon = LoadSprite( "sprites/rtn_stim.spr" );
	g_hPainIcon = LoadSprite( "sprites/rtn_pain.spr" );

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

	// --- Estimulante (verde) ---
	if( m_iStimDoses > 0 )
	{
		RTN_DrawItemSlot( RTN_STIM_Y, 30, 160, 60, g_hStimIcon, "E", m_iStimDoses );
	}

	// --- Painkiller (azul) ---
	if( m_iPainDoses > 0 )
	{
		RTN_DrawItemSlot( RTN_PAIN_Y, 60, 120, 200, g_hPainIcon, "P", m_iPainDoses );
	}

	return 1;
}
