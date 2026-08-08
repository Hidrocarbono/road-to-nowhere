#include "hud.h"          // primeiro: define CHudBase e inclui hud_rtn_items.h (pos CHudBase)
#include "hud_rtn_items.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "enginecallback.h"

// RTN F9/F10: contadores laterais de estimulante (V) e painkiller (H).
// v4 (build #99 fix): SEM quadrado de fundo (o user pediu p/ eliminar os
// quadrados verde/azul). So o ICONE .spr (sólido, SPR_Draw) + quantidade
// em fonte ROBOTO (DrawHudString -> pfnDrawCharacter -> cls.creditsFont,
// que e a creditsfont_cp125X.fnt = Roboto) ao lado DIREITO do icone.
//
// v3 (build #94): usava .spr nativo + SPR_DrawAdditive (ficava translucido).
// v4: SPR_Draw (normal, nao additive) p/ o icone ficar solido.

#define RTN_ITEMS_X		10
#define RTN_STIM_Y		300	// estimulante (verde)
#define RTN_PAIN_Y		340	// painkiller (branco/azul)
#define RTN_ICON_SIZE	24
#define RTN_TEXT_X		(RTN_ITEMS_X + RTN_ICON_SIZE + 14)  // texto +14px do icone

DECLARE_MESSAGE( m_RTNItems, RTNItems );  // gera __MsgFunc_RTNItems -> gHUD.m_RTNItems.MsgFunc_RTNItems

static SpriteHandle g_hStimIcon = 0;
static SpriteHandle g_hPainIcon = 0;

// desenha o icone (solido) + quantidade em Roboto ao lado direito
static void RTN_DrawItemSlot( int y, int r, int g, int b, SpriteHandle hSpr, int iDoses )
{
	if( hSpr )
	{
		// SPR_Draw (normal) p/ icone SOLIDO. Cor 255,255,255 = mostra a
		// COR ORIGINAL do sprite (verde da seringa, cruz vermelha/branca
		// do medkit), que veio do TGA (tools/tga2spr.py --white removido).
		SPR_Set( hSpr, 255, 255, 255 );
		SPR_Draw( 0, RTN_ITEMS_X, y, NULL );
	}
	else
	{
		// fallback: se o sprite nao carregou, nada (sem quadrado)
		return;
	}

	// quantidade em fonte ROBOTO (DrawHudString usa pfnDrawCharacter ->
	// cls.creditsFont = Roboto). Cor #cc812b (204,129,43) fixa, separada
	// da cor do icone. O fundo do contorno e preto (0,0,0).
	char szDoses[8];
	Q_snprintf( szDoses, sizeof( szDoses ), "%d", iDoses );
	int tx = RTN_TEXT_X;  // +14px do icone (nao colado)
	int ty = y + RTN_ICON_SIZE / 2 - 4;  // centraliza verticalmente aprox.
	// contorno escuro p/ legibilidade sobre o cenario
	gHUD.DrawHudString( tx + 1, ty + 1, tx + 60, szDoses, 0, 0, 0 );
	// numero em #cc812b, area de 60px p/ caber 2 digitos sem cortar
	gHUD.DrawHudString( tx, ty, tx + 60, szDoses, 204, 129, 43 );
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
	// Se o arquivo nao existir, hSpr fica 0 -> nao desenha nada.
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
		RTN_DrawItemSlot( RTN_STIM_Y, 30, 160, 60, g_hStimIcon, m_iStimDoses );
	}

	// --- Painkiller (azul) ---
	if( m_iPainDoses > 0 )
	{
		RTN_DrawItemSlot( RTN_PAIN_Y, 60, 120, 200, g_hPainIcon, m_iPainDoses );
	}

	return 1;
}
