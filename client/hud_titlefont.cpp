/*
hud_titlefont.cpp - RTN: fontes customizadas para titles.txt ($font/$fontsize)

Ver o comentario grande em hud_titlefont.h e em utils/gen_titlefont.py.
Dois pedacos independentes vivem aqui:

  1) Cache de fontes (.rtnfont + .spr) - RTN_GetTitleFont().
  2) Parser leve das diretivas "$font"/"$fontsize" do titles.txt, separado do
     parser do proprio engine (que nao conhece essas diretivas e as ignora
     silenciosamente) - RTN_GetTitleFontOverride().

O motivo de precisarmos do nosso PROPRIO parser de titles.txt (em vez de só
usar client_textmessage_t, que o engine ja preenche) e que client_textmessage_t
(engine/cdll_int.h) e uma struct FIXA do engine, sem campo de fonte nem de
tamanho - nao da pra "pendurar" dado extra nela. Entao lemos o arquivo de novo,
so pra extrair essas duas diretivas por bloco de mensagem.
*/
#include "hud.h"
#include "utils.h"
#include "const.h"			// kRenderTransTexture
#include "triangleapi.h"	// SpriteHandle usa render mode do TriAPI
#include "hud_titlefont.h"

// DrawSpriteAsPoly (client/render/tri.cpp) nao tem header proprio - so este
// prototipo, igual a como o resto do client ja usa OrthoDraw/etc irmaos dela.
extern void DrawSpriteAsPoly( SpriteHandle hspr, wrect_t *rect, wrect_t *screenpos, int mode, float r, float g, float b, float a );

#define MAX_RTN_TITLEFONTS			8

static CRTNTitleFont g_TitleFonts[MAX_RTN_TITLEFONTS];
static int g_iTitleFontCount = 0;

// ----------------------------------------------------------------------
// Carregamento de fontes (.rtnfont + .spr)
// ----------------------------------------------------------------------

static bool RTN_LoadTitleFontFile( CRTNTitleFont *pFont, const char *pszName )
{
	char szPath[128];
	Q_snprintf( szPath, sizeof( szPath ), "fonts/%s.rtnfont", pszName );

	int iSize = 0;
	byte *pFile = (byte *)gEngfuncs.COM_LoadFile( szPath, 5, &iSize );
	if( !pFile )
	{
		gEngfuncs.Con_Printf( "RTN titlefont: nao achei %s (rode utils/gen_titlefont.py?)\n", szPath );
		return false;
	}

	char szSpritePath[128];
	szSpritePath[0] = 0;
	pFont->iBakeSize = 0;
	pFont->iLineHeight = 0;
	memset( pFont->glyphs, 0, sizeof( pFont->glyphs ));

	// parser linha a linha, bem tolerante - o arquivo e gerado por script
	// (utils/gen_titlefont.py), nao editado a mao, entao nao precisa ser
	// robusto contra formato hostil - so contra linha em branco/comentario.
	char *pText = (char *)pFile;
	while( *pText )
	{
		// pula espaco no inicio da linha
		while( *pText == ' ' || *pText == '\t' )
			pText++;

		char *pLineStart = pText;
		while( *pText && *pText != '\n' && *pText != '\r' )
			pText++;
		char *pLineEnd = pText;
		while( *pText == '\n' || *pText == '\r' )
			pText++;

		if( pLineEnd == pLineStart || *pLineStart == '#' )
			continue; // linha vazia ou comentario

		char szLine[256];
		size_t len = (size_t)( pLineEnd - pLineStart );
		if( len >= sizeof( szLine )) len = sizeof( szLine ) - 1;
		memcpy( szLine, pLineStart, len );
		szLine[len] = 0;

		if( !Q_strnicmp( szLine, "sprite ", 7 ))
		{
			Q_strncpy( szSpritePath, szLine + 7, sizeof( szSpritePath ));
		}
		else if( !Q_strnicmp( szLine, "size ", 5 ))
		{
			pFont->iBakeSize = Q_atoi( szLine + 5 );
		}
		else if( !Q_strnicmp( szLine, "lineheight ", 11 ))
		{
			pFont->iLineHeight = Q_atoi( szLine + 11 );
		}
		else if( !Q_strnicmp( szLine, "glyph ", 6 ))
		{
			int code, x, y, w, h;
			float advance;
			if( sscanf( szLine + 6, "%d %d %d %d %d %f", &code, &x, &y, &w, &h, &advance ) == 6 )
			{
				if( code >= 0 && code < RTN_TITLEFONT_MAX_GLYPHS )
				{
					rtn_titlefont_glyph_t *g = &pFont->glyphs[code];
					g->valid = ( w > 0 && h > 0 );
					g->x = (short)x;
					g->y = (short)y;
					g->w = (short)w;
					g->h = (short)h;
					g->advance = advance;
				}
			}
		}
	}

	gEngfuncs.COM_FreeFile( pFile );

	if( !szSpritePath[0] || pFont->iBakeSize <= 0 )
	{
		gEngfuncs.Con_Printf( "RTN titlefont: %s incompleto (sem 'sprite' ou 'size' validos)\n", szPath );
		return false;
	}

	pFont->hSprite = LoadSprite( szSpritePath );
	if( !pFont->hSprite )
	{
		gEngfuncs.Con_Printf( "RTN titlefont: nao consegui carregar o atlas %s\n", szSpritePath );
		return false;
	}

	if( pFont->iLineHeight <= 0 )
		pFont->iLineHeight = pFont->iBakeSize; // fallback razoavel

	Q_strncpy( pFont->szName, pszName, sizeof( pFont->szName ));
	return true;
}

CRTNTitleFont *RTN_GetTitleFont( const char *pszName )
{
	if( !pszName || !pszName[0] )
		return NULL;

	for( int i = 0; i < g_iTitleFontCount; i++ )
	{
		if( !Q_stricmp( g_TitleFonts[i].szName, pszName ))
			return g_TitleFonts[i].IsValid() ? &g_TitleFonts[i] : NULL;
	}

	if( g_iTitleFontCount >= MAX_RTN_TITLEFONTS )
	{
		gEngfuncs.Con_Printf( "RTN titlefont: limite de %d fontes custom atingido\n", MAX_RTN_TITLEFONTS );
		return NULL;
	}

	CRTNTitleFont *pFont = &g_TitleFonts[g_iTitleFontCount++];
	Q_strncpy( pFont->szName, pszName, sizeof( pFont->szName )); // marca a entrada mesmo se falhar, evita reler toda hora
	if( !RTN_LoadTitleFontFile( pFont, pszName ))
		return NULL;

	return pFont;
}

// ----------------------------------------------------------------------
// Diretivas $font/$fontsize do titles.txt
// ----------------------------------------------------------------------

struct rtn_titlefont_override_t
{
	char	szMessageName[64];
	char	szFont[32];
	int		iFontSize;
};

#define MAX_RTN_TITLEFONT_OVERRIDES		256

static rtn_titlefont_override_t g_TitleFontOverrides[MAX_RTN_TITLEFONT_OVERRIDES];
static int g_iTitleFontOverrideCount = 0;
static bool g_bTitleFontOverridesParsed = false;

/*
Formato relevante do titles.txt (ver o cabecalho do proprio arquivo pra
descricao completa - aqui so as duas diretivas novas):

    $font <nome_curto>     -> nome gerado por utils/gen_titlefont.py
    $fontsize <px>         -> tamanho na tela, em pixels (altura de linha)

Como $position/$color/etc, NAO SAO PERSISTENTES entre blocos: se a mensagem
seguinte nao repetir "$font", ela volta a usar a fonte padrao do engine.
"$fontsize" sozinho (sem "$font") nao tem efeito nenhum - nao ha como pedir ao
engine pra desenhar a fonte dele maior, entao MessageDrawScan ignora
$fontsize se $font nao foi setado no mesmo bloco (ver client/message.cpp).

O parser abaixo e deliberadamente simples (nao entende { } de verdade, so
procura pela linha que e o NOME sozinho seguida de uma linha "{") porque e
exatamente o que o formato do arquivo garante - mensagens sao sempre
"$diretivas... \n NOME \n { \n texto \n }".
*/
static void RTN_ParseTitleFontDirectives( void )
{
	g_bTitleFontOverridesParsed = true; // so tenta uma vez por sessao, mesmo se o arquivo faltar

	int iSize = 0;
	byte *pFile = (byte *)gEngfuncs.COM_LoadFile( "titles.txt", 5, &iSize );
	if( !pFile )
		return; // sem titles.txt, sem overrides - nada de novo quebra (a fonte legado ainda funciona)

	char szPendingFont[32];
	int iPendingSize = 0;
	szPendingFont[0] = 0;

	char *pText = (char *)pFile;
	while( *pText )
	{
		while( *pText == ' ' || *pText == '\t' )
			pText++;

		char *pLineStart = pText;
		while( *pText && *pText != '\n' && *pText != '\r' )
			pText++;
		char *pLineEnd = pText;
		while( *pText == '\n' || *pText == '\r' )
			pText++;

		size_t len = (size_t)( pLineEnd - pLineStart );
		if( len == 0 )
			continue;

		char szLine[256];
		if( len >= sizeof( szLine )) len = sizeof( szLine ) - 1;
		memcpy( szLine, pLineStart, len );
		szLine[len] = 0;

		// tira \r residual e espacos do fim
		while( len > 0 && ( szLine[len - 1] == '\r' || szLine[len - 1] == ' ' || szLine[len - 1] == '\t' ))
			szLine[--len] = 0;

		if( len == 0 )
			continue;

		if( szLine[0] == '/' && szLine[1] == '/' )
			continue; // comentario

		if( !Q_strnicmp( szLine, "$font ", 6 ))
		{
			Q_strncpy( szPendingFont, szLine + 6, sizeof( szPendingFont ));
			continue;
		}

		if( !Q_strnicmp( szLine, "$fontsize ", 10 ))
		{
			iPendingSize = Q_atoi( szLine + 10 );
			continue;
		}

		if( szLine[0] == '$' || szLine[0] == '{' || szLine[0] == '}' )
			continue; // outra diretiva, ou chave de abre/fecha bloco - nao nos interessa aqui

		// unica possibilidade restante numa linha assim (fora do corpo do
		// bloco) e o NOME da mensagem - so vale se a proxima linha nao-vazia
		// for "{" (senao e uma linha de texto dentro de um bloco anterior,
		// que este parser simples nao rastreia - mas como so nos importamos
		// com o nome que vem ANTES do "{", basta olhar a proxima linha).
		char *pLookahead = pText;
		while( *pLookahead == '\n' || *pLookahead == '\r' || *pLookahead == ' ' || *pLookahead == '\t' )
			pLookahead++;

		if( *pLookahead == '{' && ( szPendingFont[0] || iPendingSize > 0 ))
		{
			if( g_iTitleFontOverrideCount < MAX_RTN_TITLEFONT_OVERRIDES )
			{
				rtn_titlefont_override_t *o = &g_TitleFontOverrides[g_iTitleFontOverrideCount++];
				Q_strncpy( o->szMessageName, szLine, sizeof( o->szMessageName ));
				Q_strncpy( o->szFont, szPendingFont, sizeof( o->szFont ));
				o->iFontSize = iPendingSize;
			}
			else
			{
				gEngfuncs.Con_Printf( "RTN titlefont: limite de %d overrides de fonte no titles.txt atingido\n", MAX_RTN_TITLEFONT_OVERRIDES );
			}
		}

		// nao-sticky: some tenha sido usada ou nao, a proxima mensagem tem
		// que declarar "$font"/"$fontsize" de novo se quiser fonte custom.
		szPendingFont[0] = 0;
		iPendingSize = 0;
	}

	gEngfuncs.COM_FreeFile( pFile );
}

bool RTN_GetTitleFontOverride( const char *pszMessageName, char *szFontOut, size_t fontOutSize, int *piFontSize )
{
	if( !pszMessageName || !pszMessageName[0] )
		return false;

	if( !g_bTitleFontOverridesParsed )
		RTN_ParseTitleFontDirectives();

	for( int i = 0; i < g_iTitleFontOverrideCount; i++ )
	{
		if( !Q_stricmp( g_TitleFontOverrides[i].szMessageName, pszMessageName ))
		{
			if( !g_TitleFontOverrides[i].szFont[0] )
				return false; // so tinha $fontsize sem $font - sem efeito, ver comentario acima

			Q_strncpy( szFontOut, g_TitleFontOverrides[i].szFont, fontOutSize );
			*piFontSize = g_TitleFontOverrides[i].iFontSize;
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------------
// Medida e desenho
// ----------------------------------------------------------------------

int RTN_TitleFont_CharWidth( CRTNTitleFont *pFont, unsigned char c, float flScale )
{
	if( !pFont )
		return 0;

	rtn_titlefont_glyph_t *g = &pFont->glyphs[c];
	if( g->advance <= 0.0f )
		return (int)( pFont->iBakeSize * 0.3f * flScale ); // codigo sem entrada no atlas - avanco generico, nao trava a linha

	return (int)( g->advance * flScale + 0.5f );
}

int RTN_TitleFont_LineHeight( CRTNTitleFont *pFont, float flScale )
{
	if( !pFont )
		return 0;

	return (int)( pFont->iLineHeight * flScale + 0.5f );
}

void RTN_TitleFont_DrawChar( CRTNTitleFont *pFont, int x, int y, unsigned char c, float flScale, int r, int g, int b, int a )
{
	if( !pFont || !pFont->hSprite )
		return;

	rtn_titlefont_glyph_t *pGlyph = &pFont->glyphs[c];
	if( !pGlyph->valid )
		return; // espaco etc. - so avanco, nada pra desenhar

	wrect_t rect;
	rect.left = pGlyph->x;
	rect.top = pGlyph->y;
	rect.right = pGlyph->x + pGlyph->w;
	rect.bottom = pGlyph->y + pGlyph->h;

	wrect_t screenpos;
	screenpos.left = x;
	screenpos.top = y;
	screenpos.right = x + (int)( pGlyph->w * flScale + 0.5f );
	screenpos.bottom = y + (int)( pGlyph->h * flScale + 0.5f );

	DrawSpriteAsPoly( pFont->hSprite, &rect, &screenpos, kRenderTransTexture, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f );
}
