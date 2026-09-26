/*
hud_systemtip.cpp - aviso de sistema com icone (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

Ver hud_systemtip.h para o desenho geral. Servidor manda so (icone, chave
titles.txt) via "SystemTip" - o texto, cor e tempos (fadein/fadeout/
holdtime) vem do proprio titles.txt (TextMessageGet), igual o hud_dialog.
*/

#include "hud.h"
#include "hud_systemtip.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "texture_handle.h"
#include "gl_local.h"
#include "hud_titlefont.h"

extern void OrthoQuad( int x1, int y1, int x2, int y2 );		// tri.cpp
extern void RTN_Utf8ToCp1252( char *szText );				// mesmo fix do hud_radio/hud_dialog

// RTN F10 fix: mesma fonte custom do hud_dialog.cpp (Roboto) - a creditsFont
// nativa do engine nao tem acento latino de verdade em nenhuma variante
// cp1252 disponivel neste projeto. Cai pra DrawHudString nativo se o asset
// nao carregar.
#define SYSTIP_FONT_NAME	"roboto"

static CRTNTitleFont *s_pSysTipFont = NULL;
static float s_flSysTipFontScale = 1.0f;

// nomes fixos dos assets - so existe UMA variante por icone (o "_1024"/"_640"
// original do Paranoia 2 era por faixa de resolucao de tela; aqui o HUD ja
// escala pela resolucao real, entao so precisamos de uma).
static const char *SYSTIP_ICON_PATH[SYSTIP_NUM_ICONS] =
{
	"gfx/vgui/icon0_1024.tga",	// 0 = save
	"gfx/vgui/icon1_1024.tga",	// 1 = atencao/dica
};

DECLARE_MESSAGE( m_SystemTip, SystemTip )

int CHudSystemTip::Init( void )
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( SystemTip );
	m_iFlags |= HUD_ACTIVE;
	ResetState();
	return 1;
}

int CHudSystemTip::VidInit( void )
{
	ResetState();
	for( int i = 0; i < SYSTIP_NUM_ICONS; i++ )
		m_hIcons[i] = LOAD_TEXTURE( SYSTIP_ICON_PATH[i], NULL, 0, TF_CLAMP | TF_IMAGE | TF_HAS_ALPHA );
	return 1;
}

void CHudSystemTip::ResetState( void )
{
	m_bActive = false;
	m_bHasNext = false;
	m_iIcon = 0;
	m_szText[0] = 0;
	m_iR = m_iG = m_iB = 255;
	m_fShowTime = m_fHideTime = 0.0f;
	m_fFadeIn = 0.3f;
	m_fFadeOut = 0.4f;
}

int CHudSystemTip::MsgFunc_SystemTip( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int iIcon = READ_BYTE();

	char szKey[SYSTIP_MAX_TEXT];
	Q_strncpy( szKey, READ_STRING(), sizeof( szKey ));
	END_READ();

	if( iIcon < 0 || iIcon >= SYSTIP_NUM_ICONS )
		iIcon = 0;

	char szText[SYSTIP_MAX_TEXT];
	int r = 255, g = 255, b = 255;
	float fadein = 0.3f, fadeout = 0.4f, holdtime = 3.0f;

	client_textmessage_t *pMsg = TextMessageGet( szKey );
	if( pMsg && pMsg->pMessage )
	{
		Q_strncpy( szText, pMsg->pMessage, sizeof( szText ));
		r = pMsg->r1; g = pMsg->g1; b = pMsg->b1;
		if( pMsg->fadein > 0 ) fadein = pMsg->fadein;
		if( pMsg->fadeout > 0 ) fadeout = pMsg->fadeout;
		if( pMsg->holdtime > 0 ) holdtime = pMsg->holdtime;
	}
	else
	{
		Q_strncpy( szText, szKey, sizeof( szText ));	// chave sem entrada - mostra o nome cru, nunca em branco
	}
	RTN_Utf8ToCp1252( szText );

	float curtime = gEngfuncs.GetClientTime();

	if( !m_bActive || curtime > m_fHideTime )
	{
		// mostra agora
		m_iIcon = iIcon;
		Q_strncpy( m_szText, szText, sizeof( m_szText ));
		m_iR = r; m_iG = g; m_iB = b;
		m_fFadeIn = fadein;
		m_fFadeOut = fadeout;
		m_fShowTime = curtime;
		m_fHideTime = curtime + holdtime;
		m_bActive = true;
		m_bHasNext = false;
	}
	else
	{
		// ja tem um na tela - enfileira (mesma logica do hud_radio)
		m_iNextIcon = iIcon;
		Q_strncpy( m_szNextText, szText, sizeof( m_szNextText ));
		m_iNextR = r; m_iNextG = g; m_iNextB = b;
		m_fNextFadeIn = fadein;
		m_fNextFadeOut = fadeout;
		m_fNextHold = holdtime;
		m_bHasNext = true;
	}

	return 1;
}

// RTN F10 fix: largura REAL do texto - da fonte custom (s_pSysTipFont)
// quando carregada, senao gHUD.m_scrinfo.charWidths[] (a mesma tabela que
// CHud::DrawHudString usa), nao mais uma estimativa de ~11px/char.
static int SysTip_MeasureString( const char *sz )
{
	int width = 0;
	if( s_pSysTipFont )
	{
		for( const byte *p = (const byte *)sz; *p; p++ )
			width += RTN_TitleFont_CharWidth( s_pSysTipFont, *p, s_flSysTipFontScale );
	}
	else
	{
		for( const byte *p = (const byte *)sz; *p; p++ )
			width += gHUD.m_scrinfo.charWidths[*p];
	}
	return width;
}

// Maior largura entre as linhas (separadas por '\n') de szText.
static int SysTip_MaxLineWidth( const char *szText )
{
	char buf[SYSTIP_MAX_TEXT];
	Q_strncpy( buf, szText, sizeof( buf ));

	int maxWide = 0;
	char *sptr = buf;
	while( true )
	{
		char *nl = strchr( sptr, '\n' );
		if( nl ) *nl = '\0';

		int w = SysTip_MeasureString( sptr );
		if( w > maxWide ) maxWide = w;

		if( !nl )
			break;
		sptr = nl + 1;
	}
	return maxWide;
}

// Desenha cada linha (separada por '\n') alinhada a esquerda a partir de x
// - o bloco "icone + texto" inteiro e que fica centralizado (ver Draw()),
// nao cada linha de texto individualmente (o icone fica fixo a esquerda,
// igual a referencia do Paranoia 2). Devolve quantas linhas desenhou.
//
// RTN F10 fix: com a fonte custom, o alpha agora acompanha o fade do icone
// de verdade (alpha passado em 'a') - DrawHudString (fallback nativo) nao
// aceita alpha, entao SO nesse caminho o texto ainda entra/sai "seco".
static int SysTip_DrawLeftAligned( int x, int y, int lineGap, const char *szText, int r, int g, int b, int a )
{
	char buf[SYSTIP_MAX_TEXT];
	Q_strncpy( buf, szText, sizeof( buf ));

	int lines = 0;
	char *sptr = buf;
	while( true )
	{
		char *nl = strchr( sptr, '\n' );
		if( nl ) *nl = '\0';

		int ly = y + lines * lineGap;

		if( s_pSysTipFont )
		{
			int xcur = x;
			for( const byte *p = (const byte *)sptr; *p; p++ )
			{
				RTN_TitleFont_DrawChar( s_pSysTipFont, xcur, ly, *p, s_flSysTipFontScale, r, g, b, a );
				xcur += RTN_TitleFont_CharWidth( s_pSysTipFont, *p, s_flSysTipFontScale );
			}
		}
		else
		{
			gHUD.DrawHudString( x, ly, ScreenWidth, sptr, r, g, b );
		}
		lines++;

		if( !nl )
			break;
		sptr = nl + 1;
	}
	return lines;
}

static int SysTip_CountLines( const char *szText )
{
	int lines = 1;
	for( const char *p = szText; *p; p++ )
		if( *p == '\n' ) lines++;
	return lines;
}

int CHudSystemTip::Draw( float flTime )
{
	if( !m_bActive )
		return 0;

	float curtime = gEngfuncs.GetClientTime();

	if( curtime > m_fHideTime )
	{
		if( m_bHasNext )
		{
			m_iIcon = m_iNextIcon;
			Q_strncpy( m_szText, m_szNextText, sizeof( m_szText ));
			m_iR = m_iNextR; m_iG = m_iNextG; m_iB = m_iNextB;
			m_fFadeIn = m_fNextFadeIn;
			m_fFadeOut = m_fNextFadeOut;
			m_fShowTime = curtime;
			m_fHideTime = curtime + m_fNextHold;
			m_bHasNext = false;
		}
		else
		{
			m_bActive = false;
			return 0;
		}
	}

	float fFadeAlpha = 1.0f;
	if( curtime < m_fShowTime + m_fFadeIn )
		fFadeAlpha = ( curtime - m_fShowTime ) / m_fFadeIn;
	else if( curtime > m_fHideTime - m_fFadeOut )
		fFadeAlpha = ( m_fHideTime - curtime ) / m_fFadeOut;
	fFadeAlpha = Q_min( 1.0f, Q_max( 0.0f, fFadeAlpha ));

	// RTN: mesma fonte custom do hud_dialog.cpp (Roboto) - ver comentario
	// no topo do arquivo. NULL cai pro caminho nativo automaticamente.
	s_pSysTipFont = RTN_GetTitleFont( SYSTIP_FONT_NAME );

	int nFontHeight;
	if( s_pSysTipFont )
	{
		int iSize = XRES( 16 );
		s_flSysTipFontScale = (float)iSize / (float)s_pSysTipFont->iBakeSize;
		nFontHeight = RTN_TitleFont_LineHeight( s_pSysTipFont, s_flSysTipFontScale );
	}
	else
	{
		s_flSysTipFontScale = 1.0f;
		nFontHeight = Q_max( 12, gHUD.m_iFontHeight );
	}

	// RTN F10 fix: layout era icone GRANDE empilhado ACIMA do texto (ficava
	// por cima/colado nele). Referencia do Paranoia 2 (print mandado) e
	// icone PEQUENO ao lado esquerdo do texto, os dois lado a lado - o
	// bloco inteiro (icone+texto) e que fica centralizado na tela.
	int lineGap = nFontHeight + 2;
	int iconSize = nFontHeight;			// do tamanho da fonte, nao 2x - "pequeno" como pedido
	int iconTextGap = 8;

	int textLines = SysTip_CountLines( m_szText );
	int textTall = textLines * lineGap;
	int textWide = SysTip_MaxLineWidth( m_szText );

	int blockWide = iconSize + iconTextGap + textWide;
	int blockX = ( ScreenWidth - blockWide ) / 2;
	int blockTall = Q_max( iconSize, textTall );
	int y = 40;	// topo da tela - longe da fala de NPC (rodape) e da maozinha (centro)

	int iconX = blockX;
	int iconY = y + ( blockTall - iconSize ) / 2;	// icone centralizado verticalmente contra o texto

	if( m_hIcons[m_iIcon].Initialized() )
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, fFadeAlpha );
		GL_Bind( 0, m_hIcons[m_iIcon] );
		OrthoQuad( iconX, iconY, iconX + iconSize, iconY + iconSize );
		gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	}

	// texto a direita do icone, alinhado a esquerda dentro do bloco. Sem
	// caixa, sem centralizar linha a linha - e um bloco unico com o icone.
	int textX = blockX + iconSize + iconTextGap;
	int textY = y + ( blockTall - textTall ) / 2;
	SysTip_DrawLeftAligned( textX, textY, lineGap, m_szText, m_iR, m_iG, m_iB, (int)( 255.0f * fFadeAlpha ));

	return 1;
}
