#include "hud.h"          // primeiro: define CHudBase e inclui hud_rtn_items.h (pos CHudBase)
#include "hud_rtn_items.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "enginecallback.h"

// RTN F9: contadores laterais de estimulante (V) e painkiller (H).
// Desenho estilo Paranoia 2: caixinha com ICONE (gfx/vgui/*.tga) + numero de doses,
// na lateral ESQUERDA da tela (x=10), acima da vida (que fica embaixo).
// Fallback: se a textura nao carregar, desenha a letra inicial (E/P).

#define RTN_ITEMS_X		10
#define RTN_STIM_Y		300	// estimulante (verde)
#define RTN_PAIN_Y		340	// painkiller (branco/azul)
#define RTN_ICON_SIZE	24

DECLARE_MESSAGE( m_RTNItems, RTNItems );  // gera __MsgFunc_RTNItems -> gHUD.m_RTNItems.MsgFunc_RTNItems

static TextureHandle g_hStimIcon = TextureHandle::Null();
static TextureHandle g_hPainIcon = TextureHandle::Null();

// desenha um quad com a textura ativa (coords de tela)
static void RTN_DrawQuad( float xmin, float ymin, float xmax, float ymax )
{
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
	gEngfuncs.pTriAPI->Vertex3f( xmin, ymin, 0 );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
	gEngfuncs.pTriAPI->Vertex3f( xmin, ymax, 0 );
	gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
	gEngfuncs.pTriAPI->Vertex3f( xmax, ymax, 0 );
	gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
	gEngfuncs.pTriAPI->Vertex3f( xmax, ymin, 0 );
	gEngfuncs.pTriAPI->End();
}

// desenha a caixinha (fundo + borda) e o conteudo (icone OU letra de fallback)
static void RTN_DrawItemSlot( int y, int r, int g, int b, TextureHandle hTex, const char *pszLetter, int iDoses )
{
	FillRGBA( RTN_ITEMS_X, y, RTN_ICON_SIZE, RTN_ICON_SIZE, r, g, b, 200 );
	// borda
	FillRGBA( RTN_ITEMS_X, y, RTN_ICON_SIZE, 2, 255, 255, 255, 120 );
	FillRGBA( RTN_ITEMS_X, y + RTN_ICON_SIZE - 2, RTN_ICON_SIZE, 2, 255, 255, 255, 120 );
	FillRGBA( RTN_ITEMS_X, y, 2, RTN_ICON_SIZE, 255, 255, 255, 120 );
	FillRGBA( RTN_ITEMS_X + RTN_ICON_SIZE - 2, y, 2, RTN_ICON_SIZE, 255, 255, 255, 120 );

	if( hTex.Initialized() )
	{
		// icone TGA com alpha (GL_Bind + quad, sem converter p/ .spr)
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
		GL_Bind( 0, hTex );
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		RTN_DrawQuad( RTN_ITEMS_X + 1, y + 1, RTN_ITEMS_X + RTN_ICON_SIZE - 1, y + RTN_ICON_SIZE - 1 );
		gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
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

	// icones do mod (gfx/vgui/). Se o arquivo nao existir, hTex fica 0 -> fallback letra.
	g_hStimIcon = LOAD_TEXTURE( "gfx/vgui/painkiller.tga", NULL, 0, 0 );
	g_hPainIcon = LOAD_TEXTURE( "gfx/vgui/medikit.tga", NULL, 0, 0 );

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
