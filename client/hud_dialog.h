/*
hud_dialog.h - dialogo com escolhas do jogador (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

HUD 2D (sem VGUI, mesmo estilo do hud_radio.cpp): painel translucido com a
fala do NPC (nome + linha, ambos vindos de titles.txt via TextMessageGet)
e, abaixo, ate DLG_MAX_OPTIONS+1 opcoes numeradas (a ultima e o
exit_option, se o no atual tiver um). Selecao pelas teclas 1-9, que ja sao
desviadas pra ca por WeaponsResource::SelectSlot() (client/ammo.cpp),
igual o menu nativo do HL1 (client/menu.cpp) ja faz.

Servidor manda tudo pronto (chaves de titles.txt) em "DialogShow" - o
cliente so resolve o texto localmente, nunca decide qual e o proximo no.
*/

#ifndef HUD_DIALOG_H
#define HUD_DIALOG_H

#include "hud.h"

#define DLG_HUD_MAX_SLOTS	9	// DLG_MAX_OPTIONS (8, ver server/dialogscript.h) + exit_option
#define DLG_HUD_MAX_TEXT	128

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

	bool	m_bActive;
	int	m_iTurn;

	char	m_szSpeaker[64];		// linha 1 do bloco titles.txt (nome do falante)
	char	m_szLine[256];			// linha 2+ (a fala em si)

	int	m_iNumSlots;
	char	m_szOptions[DLG_HUD_MAX_SLOTS][DLG_HUD_MAX_TEXT];
};

#endif // HUD_DIALOG_H
