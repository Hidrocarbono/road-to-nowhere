/*
hud_dialog.h - dialogo com escolhas do jogador (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

HUD 2D (sem VGUI, mesmo estilo do hud_radio.cpp): painel translucido com a
fala do NPC (nome + linha) e, abaixo, ate DLG_MAX_OPTIONS+1 opcoes
numeradas (a ultima e o exit_option, se o no atual tiver um). Selecao
pelas teclas 1-9, que ja sao desviadas pra ca por
WeaponsResource::SelectSlot() (client/ammo.cpp), igual o menu nativo do
HL1 (client/menu.cpp) ja faz.

RTN F13: o servidor manda TEXTO PRONTO (nao mais chave de titles.txt) e o
ESTILO (font/fontsize/cor) direto na mensagem - dialogscript.cpp (server)
agora guarda os dois junto com a estrutura do dialogo, um arquivo por NPC.
O cliente so desenha o que chega, nunca consulta titles.txt nem decide
qual e o proximo no. Ver o comentario grande no topo de
server/dialogsession.cpp pro formato exato da mensagem "DialogShow".
*/

#ifndef HUD_DIALOG_H
#define HUD_DIALOG_H

#include "hud.h"

#define DLG_HUD_MAX_SLOTS	9	// DLG_MAX_OPTIONS (8, ver server/dialogscript.h) + exit_option
#define DLG_HUD_MAX_TEXT	512	// RTN F13: igual DLG_MAX_TEXT (server/dialogscript.h) - texto de verdade, nao mais chave curta
#define DLG_HUD_MAX_LABEL	128	// igual DLG_MAX_LABEL (server/dialogscript.h)
#define DLG_HUD_MAX_FONT_NAME	32

class CHudDialog : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_DialogShow( const char *pszName, int iSize, void *pbuf );

	bool IsActive( void ) { return m_bActive; }
	void SelectOption( int iSlot );	// chamado por WeaponsResource::SelectSlot (client/ammo.cpp)

private:
	void ResetState( void );
	void ReadStyle( void );	// le font/fontsize/flags de cor (mesma ordem de DLG_SendStyle, server/dialogsession.cpp)

	bool	m_bActive;
	int	m_iTurn;
	bool	m_bSealedOnly;		// msgType SEALED (ver hud_dialog.cpp) - sem opcao, fecha por timeout, nao por "dlgselect"
	float	m_flSealedHideTime;	// gEngfuncs.GetClientTime() em que m_bSealedOnly deve sair da tela

	char	m_szSpeaker[64];		// linha 1 do texto recebido (nome do falante, por convencao)
	char	m_szLine[DLG_HUD_MAX_TEXT];	// linha 2+ (a fala em si)

	int	m_iNumSlots;
	char	m_szOptions[DLG_HUD_MAX_SLOTS][DLG_HUD_MAX_LABEL];

	// RTN F13: estilo vindo direto na mensagem (nao mais via titles.txt/
	// RTN_GetTitleFontOverride) - szFont vazio = fonte padrao do cliente,
	// iFontSize 0 = tamanho padrao, bHasColorSpeaker/bHasColorText false =
	// cor padrao do cliente.
	char	m_szFont[DLG_HUD_MAX_FONT_NAME];
	int	m_iFontSize;
	bool	m_bHasColorSpeaker;
	int	m_iColorSpeaker[3];
	bool	m_bHasColorText;
	int	m_iColorText[3];
};

#endif // HUD_DIALOG_H
