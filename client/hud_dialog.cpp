/*
hud_dialog.cpp - dialogo com escolhas do jogador (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

Painel 2D no mesmo molde do hud_radio.cpp (sem VGUI, fundo translucido,
sem caixa preta solida - ver a conversa que fechou esse formato).

RTN F13: o texto (fala do NPC, labels de opcao, "ja conversamos") e o
estilo (font/fontsize/cor) chegam prontos do servidor em "DialogShow" -
nao ha mais resolucao via titles.txt/TextMessageGet aqui. A fala usa a
mesma convencao de antes (linha 1 = nome do falante, linha 2+ = a fala),
so que agora e o PROPRIO texto que chega assim, nao um bloco titles.txt.
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
//
// RTN F11 fix (texto ilegivel/sumindo): "roboto" (usado ate o commit
// anterior) e um atlas de TITULO, bakeado a 63px (o pedido original era
// 128px, mas MAX_ATLAS_DIM=1024 forcou o gen_titlefont.py a encolher pra
// caber o charset ASCII+Latin-1 inteiro - ver fit_bake_size() em
// utils/gen_titlefont.py). DrawSpriteAsPoly (client/render/tri.cpp) e um
// quad texturizado simples, SEM mipmap/SDF - desenhar esse atlas no tamanho
// de dialogo (a formula antiga colapsava pro piso de 8px) e minificar
// ~8x (8/63), e sampling simples nesse fator vira ruido ilegivel em vez de
// letra pequena e limpa. "roboto_small" e um atlas SEPARADO, bakeado a 24px
// (utils/gen_titlefont.py ... roboto_small 24) so pra este uso - o mesmo
// tamanho final de tela agora precisa de uma minificacao muito mais branda.
#define DLG_FONT_NAME	"roboto_small"

// RTN F13: duracao do painel "ja conversamos" (msgType SEALED) na tela -
// esse modo nao tem opcao nenhuma pra o jogador fechar com um clique, soo
// turn/anti-replay (nao e uma sessao), entao precisa de um timeout proprio
// (antes disso era o fade automatico do canal HudText, UTIL_ShowMessage).
#define DLG_SEALED_SECONDS	3.0f

enum dlgmsgtype_e
{
	DLG_MSG_CLOSE  = 0,
	DLG_MSG_NODE   = 1,
	DLG_MSG_SEALED = 2,
};

// resolvidos 1x no topo de Draw() e lidos por DLG_MeasureString/
// DLG_DrawMultilineCentered - evita passar fonte+escala por parametro em
// toda chamada (as duas funcoes so sao chamadas de dentro de Draw() mesmo).
static CRTNTitleFont *s_pDlgFont = NULL;
static float s_flDlgFontScale = 1.0f;

DECLARE_MESSAGE( m_Dialog, DialogShow )

// RTN: navegacao por seta - comandos de console discretos (disparam 1x por
// toque, o engine nao repete "bind" normal enquanto a tecla fica segurada -
// mesma premissa que ja vale pra slot1..slot9), bindados em rtn.cfg
// (uparrow/downarrow/ENTER). Fora do dialogo (!IsActive()) sao no-op, custo
// zero - mesmo padrao dos outros HOOK_COMMAND deste arquivo/client.
DECLARE_COMMAND( m_Dialog, DlgPrev );
DECLARE_COMMAND( m_Dialog, DlgNext );
DECLARE_COMMAND( m_Dialog, DlgConfirm );

int CHudDialog::Init( void )
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( DialogShow );
	HOOK_COMMAND( "dlg_prev", DlgPrev );
	HOOK_COMMAND( "dlg_next", DlgNext );
	HOOK_COMMAND( "dlg_confirm", DlgConfirm );
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
	m_bSealedOnly = false;
	m_flSealedHideTime = 0.0f;
	m_szSpeaker[0] = m_szLine[0] = 0;
	m_iNumSlots = 0;
	m_iHighlight = 1;
	for( int i = 0; i < DLG_HUD_MAX_SLOTS; i++ )
		m_szOptions[i][0] = 0;
	m_szFont[0] = 0;
	m_iFontSize = 0;
	m_bHasColorSpeaker = m_bHasColorText = false;
	m_iColorSpeaker[0] = m_iColorSpeaker[1] = m_iColorSpeaker[2] = 255;
	m_iColorText[0] = m_iColorText[1] = m_iColorText[2] = 255;
}

// Separa o texto recebido (linha 1 = nome, linha 2+ = fala, mesma
// convencao de antes) em szSpeaker/szLine. Sem "\n" nenhum, trata tudo
// como fala sem falante (szSpeaker fica vazio, Draw() so pula essa linha).
static void DLG_SplitSpeaker( const char *pszText, char *szSpeaker, size_t speakerSz, char *szLine, size_t lineSz )
{
	szSpeaker[0] = szLine[0] = 0;
	if( !pszText || !pszText[0] )
		return;

	const char *nl = strchr( pszText, '\n' );
	if( nl )
	{
		size_t len = (size_t)( nl - pszText );
		if( len >= speakerSz ) len = speakerSz - 1;
		memcpy( szSpeaker, pszText, len );
		szSpeaker[len] = 0;
		Q_strncpy( szLine, nl + 1, lineSz );
	}
	else
	{
		Q_strncpy( szLine, pszText, lineSz );
	}
}

// Le font/fontsize/flags-de-cor+RGB na MESMA ordem que DLG_SendStyle
// (server/dialogsession.cpp) escreveu - usado tanto por msgType NODE
// quanto SEALED, os dois mandam o mesmo bloco de estilo por ultimo.
void CHudDialog::ReadStyle( void )
{
	Q_strncpy( m_szFont, READ_STRING(), sizeof( m_szFont ));
	m_iFontSize = READ_BYTE();

	int flags = READ_BYTE();
	m_bHasColorSpeaker = ( flags & 1 ) != 0;
	m_bHasColorText = ( flags & 2 ) != 0;

	if( m_bHasColorSpeaker )
	{
		m_iColorSpeaker[0] = READ_BYTE();
		m_iColorSpeaker[1] = READ_BYTE();
		m_iColorSpeaker[2] = READ_BYTE();
	}
	if( m_bHasColorText )
	{
		m_iColorText[0] = READ_BYTE();
		m_iColorText[1] = READ_BYTE();
		m_iColorText[2] = READ_BYTE();
	}
}

int CHudDialog::MsgFunc_DialogShow( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int msgType = READ_BYTE();

	if( msgType == DLG_MSG_CLOSE )
	{
		END_READ();
		m_bActive = false;
		m_bSealedOnly = false;
		return 1;
	}

	if( msgType == DLG_MSG_SEALED )
	{
		char szText[DLG_HUD_MAX_TEXT];
		Q_strncpy( szText, READ_STRING(), sizeof( szText ));
		ReadStyle();
		END_READ();

		DLG_SplitSpeaker( szText, m_szSpeaker, sizeof( m_szSpeaker ), m_szLine, sizeof( m_szLine ));
		RTN_Utf8ToCp1252( m_szSpeaker );
		RTN_Utf8ToCp1252( m_szLine );

		m_iNumSlots = 0;
		m_bSealedOnly = true;
		m_flSealedHideTime = gEngfuncs.GetClientTime() + DLG_SEALED_SECONDS;
		m_bActive = true;
		return 1;
	}

	// DLG_MSG_NODE
	int turn = READ_BYTE();

	// RTN F10 fix: READ_STRING() devolve ponteiro pra um buffer interno do
	// engine que e REESCRITO a cada chamada dentro do mesmo BEGIN_READ -
	// guardar so o ponteiro (sem copiar na hora) fazia npcText virar o
	// ultimo texto lido (o do ultimo slot) assim que o loop abaixo rodava,
	// entao a fala mostrada na tela era sempre a da ULTIMA opcao, nunca a
	// do NPC. Copia pra um buffer proprio imediatamente.
	char npcText[DLG_HUD_MAX_TEXT];
	Q_strncpy( npcText, READ_STRING(), sizeof( npcText ));

	int numSlots = READ_BYTE();
	if( numSlots > DLG_HUD_MAX_SLOTS )
		numSlots = DLG_HUD_MAX_SLOTS;	// defensivo - servidor ja respeita DLG_MAX_OPTIONS+1

	char slotLabels[DLG_HUD_MAX_SLOTS][DLG_HUD_MAX_LABEL];
	for( int i = 0; i < numSlots; i++ )
		Q_strncpy( slotLabels[i], READ_STRING(), sizeof( slotLabels[i] ));

	ReadStyle();
	END_READ();

	m_iTurn = turn;
	m_iNumSlots = numSlots;
	m_bSealedOnly = false;

	DLG_SplitSpeaker( npcText, m_szSpeaker, sizeof( m_szSpeaker ), m_szLine, sizeof( m_szLine ));
	RTN_Utf8ToCp1252( m_szSpeaker );
	RTN_Utf8ToCp1252( m_szLine );

	for( int i = 0; i < numSlots; i++ )
	{
		Q_strncpy( m_szOptions[i], slotLabels[i], sizeof( m_szOptions[i] ));
		RTN_Utf8ToCp1252( m_szOptions[i] );
	}

	// RTN: reseta o destaque a CADA no novo - sem isso, um indice de um no
	// anterior com mais opcoes sobreviveria pro proximo no (com menos
	// opcoes), destacando um slot que nem existe mais.
	m_iHighlight = 1;

	m_bActive = true;
	return 1;
}

// RTN: anda o destaque por iDelta, dando a volta nas pontas (desce do 1 vai
// pro ultimo, sobe do ultimo volta pro 1) - convencao comum de menu, evita
// o jogador "preso" numa ponta tendo que segurar a tecla contraria.
void CHudDialog::MoveHighlight( int iDelta )
{
	if( !m_bActive || m_iNumSlots <= 0 )
		return;

	m_iHighlight += iDelta;
	if( m_iHighlight < 1 )
		m_iHighlight = m_iNumSlots;
	else if( m_iHighlight > m_iNumSlots )
		m_iHighlight = 1;
}

void CHudDialog::UserCmd_DlgPrev( void )
{
	MoveHighlight( -1 );
}

void CHudDialog::UserCmd_DlgNext( void )
{
	MoveHighlight( 1 );
}

void CHudDialog::UserCmd_DlgConfirm( void )
{
	if( !m_bActive )
		return;
	SelectOption( m_iHighlight );
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
	char buf[DLG_HUD_MAX_TEXT];
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

	// msgType SEALED nao tem opcao pra o jogador fechar - sai sozinho apos
	// DLG_SEALED_SECONDS (ver MsgFunc_DialogShow).
	if( m_bSealedOnly && gEngfuncs.GetClientTime() >= m_flSealedHideTime )
	{
		m_bActive = false;
		return 0;
	}

	// RTN F13: font/fontsize chegam prontos na mensagem (m_szFont/m_iFontSize)
	// em vez de consultados em titles.txt ($font/$fontsize,
	// RTN_GetTitleFontOverride) - vazio/0 = usa o padrao abaixo.
	char szFontName[DLG_HUD_MAX_FONT_NAME];
	Q_strncpy( szFontName, m_szFont[0] ? m_szFont : DLG_FONT_NAME, sizeof( szFontName ));
	int iFontSizeOverride = m_iFontSize;

	// RTN: resolve a fonte custom 1x por frame - se o asset nao carregar
	// (arquivo faltando/corrompido), s_pDlgFont fica NULL e todo o resto
	// desta funcao cai pro caminho nativo automaticamente (ver
	// DLG_MeasureString/DLG_DrawMultilineCentered acima).
	s_pDlgFont = RTN_GetTitleFont( szFontName );

	// RTN F11 fix (tamanho): a formula antiga (Q_max(8, Q_max(12,
	// gHUD.m_iFontHeight) - 7)) tinha DOIS problemas empilhados. Primeiro,
	// gHUD.m_iFontHeight vem do retangulo do sprite "number_0" (HUD de
	// munição, client/hud.cpp) - um arquivo referenciado por hud.txt que nao
	// existe neste repo (herdado do jogo base via "basedir valve"), ou seja,
	// era uma base OPACA, impossivel de verificar/prever neste projeto.
	// Segundo, subtrair 7 dali colapsava pro piso de 8px contra um atlas
	// "roboto" bakeado a 63px (~8x de minificacao) - ver comentario grande
	// no #define DLG_FONT_NAME acima pra por que isso rende ilegivel.
	// Agora que "roboto_small" e um bake dedicado a 24px, o alvo de tamanho
	// e um valor FIXO e direto (independente de resolucao, mesma escolha de
	// design que descartou XRES() aqui - ver historico) - 14px contra bake
	// de 24px = escala 0.58, testado legivel em simulacao (13-18px checados,
	// ver conversa) e do lado pequeno da faixa por causa do padrao repetido
	// de feedback "ainda esta grande" nas rodadas anteriores.
	// RTN F12: "$fontsize" no bloco de npc_line (ver acima) sobrescreve esse
	// 14 padrao, se presente.
	int nFontHeight = ( iFontSizeOverride > 0 ) ? iFontSizeOverride : 14;
	int lineGap = nFontHeight + 4;

	if( s_pDlgFont )
		s_flDlgFontScale = (float)nFontHeight / (float)s_pDlgFont->iBakeSize;
	else
		s_flDlgFontScale = 1.0f;

	// RTN F13: cor do falante/texto vem pronta na mensagem (m_iColorSpeaker/
	// m_iColorText) quando o estilo declara "color_speaker"/"color_text" -
	// sem override, mantem o mesmo default visual de sempre (amarelo de
	// acento / branco).
	int rSpk = m_bHasColorSpeaker ? m_iColorSpeaker[0] : 255;
	int gSpk = m_bHasColorSpeaker ? m_iColorSpeaker[1] : 210;
	int bSpk = m_bHasColorSpeaker ? m_iColorSpeaker[2] : 64;
	int rTxt = m_bHasColorText ? m_iColorText[0] : 255;
	int gTxt = m_bHasColorText ? m_iColorText[1] : 255;
	int bTxt = m_bHasColorText ? m_iColorText[2] : 255;

	int falaLines = ( m_szSpeaker[0] ? 1 : 0 ) + DLG_CountLines( m_szLine );
	int falaTall = falaLines * lineGap;

	// msgType SEALED nao tem opcoes - so a fala, sem a caixa de menu abaixo.
	int optionsTall = m_bSealedOnly ? 0 : ( m_iNumSlots * lineGap + 16 );	// +16 = folga da caixa (8 em cima, 8 embaixo)

	int gapBetween = m_bSealedOnly ? 0 : 8;
	int totalTall = falaTall + gapBetween + optionsTall;
	int y = ScreenHeight - totalTall - 48;	// acima da area de "say" / HUD inferior

	// --- fala do NPC: sem caixa, centralizada ---
	int falaY = y;
	if( m_szSpeaker[0] )
		falaY += DLG_DrawMultilineCentered( falaY, lineGap, m_szSpeaker, rSpk, gSpk, bSpk ) * lineGap;
	falaY += DLG_DrawMultilineCentered( falaY, lineGap, m_szLine, rTxt, gTxt, bTxt ) * lineGap;

	if( m_bSealedOnly )
		return 1;

	// --- opcoes: caixa translucida, centralizada, do tamanho do maior texto ---
	int panelWide = 0;
	for( int i = 0; i < m_iNumSlots; i++ )
	{
		// RTN: mede com o prefixo "> " sempre incluso (mesmo pras linhas que
		// NAO estao destacadas agora) - senao a caixa fica larga certa so
		// pra quem estava destacado no frame em que foi medida, e qualquer
		// outra opcao virar a destacada (seta) estoura a borda por ~2 chars.
		char szOpt[DLG_HUD_MAX_LABEL + 10];
		Q_snprintf( szOpt, sizeof( szOpt ), "> %d. %s", i + 1, m_szOptions[i] );
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
		// RTN: opcao destacada (navegacao por seta) ganha cor de acento +
		// marcador ">" - dois sinais independentes (cor E simbolo), nao so
		// cor, pra nao depender so de percepcao de cor.
		bool bHighlighted = ( ( i + 1 ) == m_iHighlight );
		char szOpt[DLG_HUD_MAX_LABEL + 10];
		Q_snprintf( szOpt, sizeof( szOpt ), "%s%d. %s", bHighlighted ? "> " : "", i + 1, m_szOptions[i] );

		if( bHighlighted )
			optTextY += DLG_DrawMultilineCentered( optTextY, lineGap, szOpt, rSpk, gSpk, bSpk ) * lineGap;	// mesma cor de acento do falante
		else
			optTextY += DLG_DrawMultilineCentered( optTextY, lineGap, szOpt, 200, 200, 200 ) * lineGap;
	}

	return 1;
}
