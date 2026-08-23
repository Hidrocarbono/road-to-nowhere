/*
hud_titlefont.h - RTN: fontes customizadas para titles.txt ($font/$fontsize)

Ver o comentario grande no topo de utils/gen_titlefont.py para o porque deste
sistema existir (o engine desenha titles.txt com uma unica fonte embutida, sem
parametro de tamanho nem de fonte - TextMessageDrawChar nao aceita nenhum dos
dois). Aqui carregamos um atlas gerado por aquele script (.spr v32) e as
metricas de cada glifo (.rtnfont) e desenhamos cada letra como um quad via
DrawSpriteAsPoly (client/render/tri.cpp), independente do desenho padrao do
engine - so entra em uso quando uma mensagem do titles.txt tem "$font <nome>".
*/
#pragma once

#define RTN_TITLEFONT_MAX_GLYPHS	256		// cobre 0x20-0x7E + 0xA0-0xFF direto pelo indice = codigo

struct rtn_titlefont_glyph_t
{
	bool	valid;			// existe entrada pra este codigo no .rtnfont?
	short	x, y;			// posicao no atlas, em pixels de bake
	short	w, h;			// tamanho no atlas, em pixels de bake
	float	advance;		// quanto avancar o cursor, em pixels de bake
};

class CRTNTitleFont
{
public:
	char				szName[32];		// nome curto usado em "$font <nome>"
	SpriteHandle		hSprite;		// atlas (game_dir/sprites/fonts/<nome>.spr)
	int					iBakeSize;		// tamanho (px) em que o atlas foi rasterizado
	int					iLineHeight;	// altura de linha, mesma unidade do bake
	rtn_titlefont_glyph_t	glyphs[RTN_TITLEFONT_MAX_GLYPHS];

	bool				IsValid( void ) const { return hSprite != 0; }
};

// Retorna a fonte custom carregada (cache interno - so le disco na primeira
// vez que "nome" e pedido). NULL se sprites/fonts/<nome>.spr ou
// fonts/<nome>.rtnfont nao existem ou sao invalidos (mensagem no console
// nesse caso - nao trava o jogo, so cai de volta pra fonte do engine).
CRTNTitleFont *RTN_GetTitleFont( const char *pszName );

// Consulta as diretivas $font/$fontsize do bloco de titles.txt cujo nome eh
// pszMessageName (ver formato no comentario de RTN_ParseTitleFontDirectives,
// em hud_titlefont.cpp). Retorna true e preenche szFontOut/piFontSize se a
// mensagem tiver "$font" no bloco dela; false caso contrario (fonte padrao
// do engine). *piFontSize vem 0 se so "$font" foi usado sem "$fontsize" (o
// chamador deve usar o tamanho de bake da fonte como tamanho "natural").
bool RTN_GetTitleFontOverride( const char *pszMessageName, char *szFontOut, size_t fontOutSize, int *piFontSize );

// Largura (em px de TELA) de um glifo escalado, 0 se o codigo nao existe na
// fonte (ainda avanca por um espaco generico pra nao empilhar letras).
int RTN_TitleFont_CharWidth( CRTNTitleFont *pFont, unsigned char c, float flScale );

// Altura de linha (em px de TELA), ja escalada.
int RTN_TitleFont_LineHeight( CRTNTitleFont *pFont, float flScale );

// Desenha um glifo em (x,y) (canto superior-esquerdo, mesma convencao de
// TextMessageDrawChar), tingido por (r,g,b,a). Nao faz nada se o codigo nao
// existe no atlas (ex.: espaco - so ocupa avanco, sem desenho).
void RTN_TitleFont_DrawChar( CRTNTitleFont *pFont, int x, int y, unsigned char c, float flScale, int r, int g, int b, int a );
