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

extern void OrthoQuad( int x1, int y1, int x2, int y2 );		// tri.cpp
extern void RTN_Utf8ToCp1252( char *szText );				// mesmo fix do hud_radio

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

// DrawHudString NAO quebra "\n" sozinho (quem faz isso e o menu nativo,
// client/menu.cpp, percorrendo a string na mao) - e npc_line pode ter
// varias linhas de fala (mesma convencao do titles.txt normal). Desenha
// cada pedaco separado por '\n' numa linha propria e devolve quantas
// linhas desenhou, pra Draw() somar na altura do painel.
static int DLG_DrawMultiline( int x, int y, int maxX, int lineGap, const char *szText, int r, int g, int b )
{
	char buf[256];
	Q_strncpy( buf, szText, sizeof( buf ));

	int lines = 0;
	char *sptr = buf;
	while( true )
	{
		char *nl = strchr( sptr, '\n' );
		if( nl ) *nl = '\0';

		gHUD.DrawHudString( x, y + lines * lineGap, maxX, sptr, r, g, b );
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

// Maior linha (entre \n's) de szText, em caracteres - usado pra dimensionar
// a largura do painel pelo CONTEUDO em vez de uma fracao fixa da tela.
static int DLG_MaxLineLen( const char *szText )
{
	int maxLen = 0, curLen = 0;
	for( const char *p = szText; ; p++ )
	{
		if( *p == '\0' || *p == '\n' )
		{
			if( curLen > maxLen ) maxLen = curLen;
			if( *p == '\0' ) break;
			curLen = 0;
		}
		else curLen++;
	}
	return maxLen;
}

int CHudDialog::Draw( float flTime )
{
	if( !m_bActive )
		return 0;

	int nFontHeight = Q_max( 12, gHUD.m_iFontHeight );
	int lineGap = nFontHeight + 4;

	// altura total: nome (se tiver) + fala (pode ter varias linhas) + uma
	// linha por opcao + folgas
	int numLines = ( m_szSpeaker[0] ? 1 : 0 ) + DLG_CountLines( m_szLine ) + m_iNumSlots;
	int panelTall = numLines * lineGap + 16;

	// Largura PROPORCIONAL ao maior texto (nome, cada linha da fala, cada
	// opcao numerada) em vez de sempre 2/3 da tela - um dialogo com textos
	// curtos (ex: "1. Sim") nao precisa de um painel largo. ~11px/char e a
	// mesma aproximacao do hud_radio.cpp (12px/char, aqui um pouco mais
	// justo porque o texto de opcao costuma ser curto). Clampado entre um
	// minimo (pra nao ficar minusculo com "Sim"/"Nao") e o teto antigo de
	// 2/3 da tela (pra nao estourar com fala longa). Sempre centralizado.
	int maxChars = m_szSpeaker[0] ? (int)strlen( m_szSpeaker ) : 0;
	int lineMax = DLG_MaxLineLen( m_szLine );
	if( lineMax > maxChars ) maxChars = lineMax;
	for( int i = 0; i < m_iNumSlots; i++ )
	{
		int optChars = (int)strlen( m_szOptions[i] ) + 3;	// "N. " na frente
		if( optChars > maxChars ) maxChars = optChars;
	}

	int panelWide = maxChars * 11 + 24;
	int minWide = ScreenWidth / 4;
	int maxWide = ( ScreenWidth * 2 ) / 3;
	if( panelWide < minWide ) panelWide = minWide;
	if( panelWide > maxWide ) panelWide = maxWide;

	int x = ( ScreenWidth - panelWide ) / 2;
	int y = ScreenHeight - panelTall - 48;	// acima da area de "say" / HUD inferior

	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pTriAPI->Color4f( 0.0f, 0.0f, 0.0f, 0.3f );	// painel bem translucido - nunca preto solido opaco
	GL_Blend( GL_TRUE );
	GL_Bind( 0, FIND_TEXTURE( "*white" ));
	OrthoQuad( x, y, x + panelWide, y + panelTall );
	GL_Blend( GL_FALSE );
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );

	int textX = x + 12;
	int textY = y + 8;
	int textMaxX = x + panelWide - 12;

	if( m_szSpeaker[0] )
	{
		gHUD.DrawHudString( textX, textY, textMaxX, m_szSpeaker, 255, 210, 64 );	// mesmo amarelo do \y do menu nativo
		textY += lineGap;
	}

	textY += DLG_DrawMultiline( textX, textY, textMaxX, lineGap, m_szLine, 255, 255, 255 ) * lineGap;
	textY += 4;

	for( int i = 0; i < m_iNumSlots; i++ )
	{
		char szOpt[DLG_HUD_MAX_TEXT + 8];
		Q_snprintf( szOpt, sizeof( szOpt ), "%d. %s", i + 1, m_szOptions[i] );
		gHUD.DrawHudString( textX, textY, textMaxX, szOpt, 200, 200, 200 );
		textY += lineGap;
	}

	return 1;
}
