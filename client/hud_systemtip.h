/*
hud_systemtip.h - aviso de sistema com icone (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

Icone (icon0 = save, icon1 = atencao/dica) + texto, desenhado no TOPO da
tela, centralizado, SEM caixa de fundo - a "linguagem visual de sistema"
combinada em contraponto a fala de NPC (que e narrativa, no rodape, com
caixa sutil so nas opcoes). Ver game_dir/titles.txt (bloco SYSTEM TIPS) e
server/entities/{env_message,trigger_autosave}.cpp (quem dispara).

Fila de 1 slot (mesmo molde do hud_radio.cpp): se chegar um aviso novo
enquanto o atual ainda esta na tela, o atual termina o hold normalmente e
so DEPOIS o novo aparece - nunca se sobrescrevem.
*/

#ifndef HUD_SYSTEMTIP_H
#define HUD_SYSTEMTIP_H

#include "hud.h"
#include "texture_handle.h"

#define SYSTIP_NUM_ICONS	2	// icon0 (save), icon1 (atencao/dica)
#define SYSTIP_MAX_TEXT		256

class CHudSystemTip : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_SystemTip( const char *pszName, int iSize, void *pbuf );

private:
	void ResetState( void );

	bool	m_bActive;
	int	m_iIcon;
	char	m_szText[SYSTIP_MAX_TEXT];
	int	m_iR, m_iG, m_iB;
	float	m_fShowTime;
	float	m_fHideTime;
	float	m_fFadeIn;
	float	m_fFadeOut;

	// fila (proximo aviso, se um segundo chegar antes do atual terminar)
	bool	m_bHasNext;
	int	m_iNextIcon;
	char	m_szNextText[SYSTIP_MAX_TEXT];
	int	m_iNextR, m_iNextG, m_iNextB;
	float	m_fNextHold, m_fNextFadeIn, m_fNextFadeOut;

	TextureHandle	m_hIcons[SYSTIP_NUM_ICONS];
};

#endif // HUD_SYSTEMTIP_H
