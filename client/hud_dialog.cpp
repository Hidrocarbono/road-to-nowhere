/*
hud_dialog.cpp - dialogo com escolhas do jogador (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

Painel 2D no mesmo molde do hud_radio.cpp (sem VGUI, fundo translucido,
sem caixa preta solida - ver a conversa que fechou esse formato). A fala
do NPC e cada opcao vem de titles.txt, resolvidas aqui via TextMessageGet
(engine) - o bloco titles.txt de npc_line usa a mesma convencao de
titulo/mensagem ja documentada la: linha 1 ($color) = nome do falante,
linha 2+ ($color2) = a fala.
*/

#include "hud.h"
#include "hud_dialog.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"
#include "texture_handle.h"
#include "gl_local.h"
#include "hud_titlefont.h"

extern void OrthoQuad( int x1, int y1, int x2, int y2 );		// tri.cpp
extern void RTN_Utf8ToCp1252( char *szText );				// mesmo fix do hud_radio

// RTN F10 fix: a creditsFont nativa do engine nao tem acento latino de
// verdade em NENHUMA variante cp1252 disponivel neste projeto (conferido
// renderizando o atlas de cada uma - so tinha digito/simbolo lixo nos bytes
// 0x80-0xFF). Diálogo usa a Roboto (fonte custom RTN, client/hud_titlefont.h)
// por padrao agora - se o asset nao carregar por algum motivo, cai pra
// DrawHudString nativo (mesmo texto, so sem garantia de acento certo).
#define DLG_FONT_NAME	"roboto"

// resolvidos 1x no topo de Draw() e lidos por DLG_MeasureString/
// DLG_DrawMultilineCentered - evita passar fonte+escala por parametro em
// toda chamada (as duas funcoes so sao chamadas de dentro de Draw() mesmo).
static CRTNTitleFont *s_pDlgFont = NULL;
static float s_flDlgFontScale = 1.0f;

DECLARE_MESSAGE( m_Dialog, DialogShow )

int CHudDialog::Init( void )
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( DialogShow );
	m_iFlags |= HUD_ACTIVE;
	ResetState();
	return 1;
}

int CHudDialog::VidInit( void )
{
	ResetState();
	return 1;
}

void CHudDialog::ResetState( void )
{
	m_bActive = false;
	m_iTurn = 0;
	m_szSpeaker[0] = m_szLine[0] = 0;
	m_iNumSlots = 0;
	for( int i = 0; i < DLG_HUD_MAX_SLOTS; i++ )
		m_szOptions[i][0] = 0;
}

// Separa o pMessage de titles.txt (linha 1 = nome, linha 2+ = fala) em
// szSpeaker/szLine. Chave vazia ou nao encontrada = szSpeaker fica vazio
// (Draw() so pula a linha do nome, ainda desenha o resto normalmente).
static void DLG_ResolveSpeaker( const char *pszKey, char *szSpeaker, size_t speakerSz, char *szLine, size_t lineSz )
{
	szSpeaker[0] = szLine[0] = 0;
	if( !pszKey || !pszKey[0] )
		return;

	client_textmessage_t *pMsg = TextMessageGet( pszKey );
	if( !pMsg || !pMsg->pMessage )
	{
		Q_strncpy( szLine, pszKey, lineSz );	// chave sem entrada em titles.txt - mostra o nome cru, nunca em branco
		return;
	}

	const char *nl = strchr( pMsg->pMessage, '\n' );
	if( nl )
	{
		size_t len = (size_t)( nl - pMsg->pMessage );
		if( len >= speakerSz ) len = speakerSz - 1;
		memcpy( szSpeaker, pMsg->pMessage, len );
		szSpeaker[len] = 0;
		Q_strncpy( szLine, nl + 1, lineSz );
	}
	else
	{
		Q_strncpy( szLine, pMsg->pMessage, lineSz );
	}
}

static void DLG_ResolveLabel( const char *pszKey, char *out, size_t outSz )
{
	if( !pszKey || !pszKey[0] )
	{
		out[0] = 0;
		return;
	}

	client_textmessage_t *pMsg = TextMessageGet( pszKey );
	if( pMsg && pMsg->pMessage )
		Q_strncpy( out, pMsg->pMessage, outSz );
	else
		Q_strncpy( out, pszKey, outSz );	// mesma logica do speaker - nunca em branco
}

int CHudDialog::MsgFunc_DialogShow( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int turn = READ_BYTE();

	// RTN F10 fix: READ_STRING() devolve ponteiro pra um buffer interno do
	// engine que e REESCRITO a cada chamada dentro do mesmo BEGIN_READ -
	// guardar so o ponteiro (sem copiar na hora) fazia npcLineKey virar a
	// ultima string lida (a do ultimo slot) assim que o loop abaixo rodava,
	// entao a fala mostrada na tela era sempre a da ULTIMA opcao, nunca a
	// do NPC. Copia pra um buffer proprio imediatamente.
	char npcLineKey[DLG_HUD_MAX_TEXT];
	Q_strncpy( npcLineKey, READ_STRING(), sizeof( npcLineKey ));

	int numSlots = READ_BYTE();

	char slotKeys[DLG_HUD_MAX_SLOTS][DLG_HUD_MAX_TEXT];
	if( numSlots > DLG_HUD_MAX_SLOTS )
		numSlots = DLG_HUD_MAX_SLOTS;	// defensivo - servidor ja respeita DLG_MAX_OPTIONS+1

	for( int i = 0; i < numSlots; i++ )
		Q_strncpy( slotKeys[i], READ_STRING(), sizeof( slotKeys[i] ));
	END_READ();

	m_iTurn = turn;
	m_iNumSlots = numSlots;

	if( numSlots == 0 )
	{
		// fecha a sessao (ver Dialog_SendClose em server/dialogsession.cpp)
		m_bActive = false;
		return 1;
	}

	DLG_ResolveSpeaker( npcLineKey, m_szSpeaker, sizeof( m_szSpeaker ), m_szLine, sizeof( m_szLine ));
	RTN_Utf8ToCp1252( m_szSpeaker );
	RTN_Utf8ToCp1252( m_szLine );

	for( int i = 0; i < numSlots; i++ )
	{
		DLG_ResolveLabel( slotKeys[i], m_szOptions[i], sizeof( m_szOptions[i] ));
		RTN_Utf8ToCp1252( m_szOptions[i] );
	}

	m_bActive = true;
	return 1;
}

void CHudDialog::SelectOption( int iSlot )
{
	if( !m_bActive || iSlot < 1 || iSlot > m_iNumSlots )
		return;

	char szbuf[32];
	sprintf( szbuf, "dlgselect %d %d\n", m_iTurn, iSlot );
	ClientCmd( szbuf );

	// esconde na hora (feedback imediato) - o proximo DialogShow (ou o
	// close com numSlots 0) do servidor reabre ou confirma o fim, mesma
	// logica do CHudMenu::SelectMenuItem nativo (client/menu.cpp).
	m_bActive = false;
}

// RTN F10 fix: a largura de texto era so uma estimativa (~11px/char), e
// numa linha longa o erro acumulado (fonte real e mais estreita) jogava o
// texto visivelmente pra esquerda do centro de verdade. Usa a largura REAL
// de cada glifo - da fonte custom (s_pDlgFont) quando carregada, senao a
// tabela nativa gHUD.m_scrinfo.charWidths[] (a mesma que CHud::DrawHudString
// usa, client/hud_redraw.cpp) - os dois casos dao a largura exata.
static int DLG_MeasureString( const char *sz )
{
	int width = 0;
	if( s_pDlgFont )
	{
		for( const byte *p = (const byte *)sz; *p; p++ )
			width += RTN_TitleFont_CharWidth( s_pDlgFont, *p, s_flDlgFontScale );
	}
	else
	{
		for( const byte *p = (const byte *)sz; *p; p++ )
			width += gHUD.m_scrinfo.charWidths[*p];
	}
	return width;
}

// DrawHudString (e RTN_TitleFont_DrawChar) NAO quebram "\n" sozinhos (quem
// faz isso e o menu nativo, client/menu.cpp, percorrendo a string na mao) -
// e npc_line pode ter varias linhas de fala (mesma convencao do titles.txt
// normal). Desenha cada pedaco separado por '\n' CENTRALIZADO (meio da tela,
// com a largura REAL medida acima) numa linha propria e devolve quantas
// linhas desenhou, pra Draw() somar na altura.
static int DLG_DrawMultilineCentered( int y, int lineGap, const char *szText, int r, int g, int b )
{
	char buf[256];
	Q_strncpy( buf, szText, sizeof( buf ));

	int lines = 0;
	char *sptr = buf;
	while( true )
	{
		char *nl = strchr( sptr, '\n' );
		if( nl ) *nl = '\0';

		int x = ( ScreenWidth - DLG_MeasureString( sptr )) / 2;
		int ly = y + lines * lineGap;

		if( s_pDlgFont )
		{
			int xcur = x;
			for( const byte *p = (const byte *)sptr; *p; p++ )
			{
				RTN_TitleFont_DrawChar( s_pDlgFont, xcur, ly, *p, s_flDlgFontScale, r, g, b, 255 );
				xcur += RTN_TitleFont_CharWidth( s_pDlgFont, *p, s_flDlgFontScale );
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

static int DLG_CountLines( const char *szText )
{
	int lines = 1;
	for( const char *p = szText; *p; p++ )
		if( *p == '\n' ) lines++;
	return lines;
}

// RTN: "Opcao B" do desenho (decidida em conversa) - a fala do NPC fica
// SEM caixa, centralizada, igual a legenda padrao do titles.txt
// (EXEMPLO 1 - $position -1 -0.1). So as OPCOES (menu de verdade,
// acionavel) ganham uma caixa bem sutil, tambem centralizada - o
// suficiente pra sinalizar "isso aqui e clicavel", sem competir
// visualmente com o resto do HUD "narrativo" do jogo.
int CHudDialog::Draw( float flTime )
{
	if( !m_bActive )
		return 0;

	// RTN: resolve a fonte custom 1x por frame - se o asset nao carregar
	// (arquivo faltando/corrompido), s_pDlgFont fica NULL e todo o resto
	// desta funcao cai pro caminho nativo automaticamente (ver
	// DLG_MeasureString/DLG_DrawMultilineCentered acima).
	s_pDlgFont = RTN_GetTitleFont( DLG_FONT_NAME );

	// RTN F10 fix (tamanho): XRES(16) rendia enorme (XRES escala com a
	// resolucao - a 1920px de largura isso ja da 48px de glifo, quase o
	// dobro do que o sistema antigo mostrava). O pedido foi manter o MESMO
	// tamanho de antes, so trocando quais glifos sao desenhados - entao o
	// alvo de tamanho agora e o proprio nFontHeight nativo (o mesmo valor
	// que ja era usado pra tudo antes desta fonte existir), nao um valor
	// novo escalado por resolucao.
	int nFontHeight = Q_max( 12, gHUD.m_iFontHeight );
	int lineGap = nFontHeight + 4;

	if( s_pDlgFont )
		s_flDlgFontScale = (float)nFontHeight / (float)s_pDlgFont->iBakeSize;
	else
		s_flDlgFontScale = 1.0f;

	int falaLines = ( m_szSpeaker[0] ? 1 : 0 ) + DLG_CountLines( m_szLine );
	int falaTall = falaLines * lineGap;

	int optionsTall = m_iNumSlots * lineGap + 16;	// +16 = folga da caixa (8 em cima, 8 embaixo)

	int gapBetween = 8;
	int totalTall = falaTall + gapBetween + optionsTall;
	int y = ScreenHeight - totalTall - 48;	// acima da area de "say" / HUD inferior

	// --- fala do NPC: sem caixa, centralizada ---
	int falaY = y;
	if( m_szSpeaker[0] )
		falaY += DLG_DrawMultilineCentered( falaY, lineGap, m_szSpeaker, 255, 210, 64 ) * lineGap;	// mesmo amarelo do \y do menu nativo
	falaY += DLG_DrawMultilineCentered( falaY, lineGap, m_szLine, 255, 255, 255 ) * lineGap;

	// --- opcoes: caixa translucida, centralizada, do tamanho do maior texto ---
	int panelWide = 0;
	for( int i = 0; i < m_iNumSlots; i++ )
	{
		char szOpt[DLG_HUD_MAX_TEXT + 8];
		Q_snprintf( szOpt, sizeof( szOpt ), "%d. %s", i + 1, m_szOptions[i] );
		int optWide = DLG_MeasureString( szOpt );
		if( optWide > panelWide ) panelWide = optWide;
	}
	panelWide += 24;

	int minWide = ScreenWidth / 6;
	int maxWide = ( ScreenWidth * 2 ) / 3;
	if( panelWide < minWide ) panelWide = minWide;
	if( panelWide > maxWide ) panelWide = maxWide;

	int optionsY = y + falaTall + gapBetween;
	int panelX = ( ScreenWidth - panelWide ) / 2;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pTriAPI->Color4f( 0.0f, 0.0f, 0.0f, 0.5f );	// RTN F10: 0.18 ficava quase opaco na pratica (feedback em jogo) - 0.5 pedido direto
	GL_Blend( GL_TRUE );
	GL_Bind( 0, FIND_TEXTURE( "*white" ));
	OrthoQuad( panelX, optionsY, panelX + panelWide, optionsY + optionsTall );
	GL_Blend( GL_FALSE );
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );

	int optTextY = optionsY + 8;
	for( int i = 0; i < m_iNumSlots; i++ )
	{
		char szOpt[DLG_HUD_MAX_TEXT + 8];
		Q_snprintf( szOpt, sizeof( szOpt ), "%d. %s", i + 1, m_szOptions[i] );
		optTextY += DLG_DrawMultilineCentered( optTextY, lineGap, szOpt, 200, 200, 200 ) * lineGap;
	}

	return 1;
}
