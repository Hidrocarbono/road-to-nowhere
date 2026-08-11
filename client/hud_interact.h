#pragma once

// RTN F10: MAOZINHA DE INTERACAO (estilo Paranoia 2 - vgui_hud.cpp:401).
// NAO incluir hud.h aqui (include circular!) - incluido PELO hud.h.
//
// O server faz o trace a frente do jogador e envia a mensagem "CanUse"
// (1 byte: 1 = pode interagir, 0 = nao). Aqui desenhamos o icone de uso
// (sprites/rtn_hud_usage.spr, convertido do gfx/vgui/640_usage.tga) no
// CENTRO da tela, abaixo da crosshair - igual ao P2 (ScreenHeight/2 + 100).
class CHudInteract : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	int MsgFunc_CanUse( const char *pszName, int iSize, void *pbuf );

	bool m_bShow = false;   // ultimo estado recebido do server

private:
	SpriteHandle m_hUsageSpr = 0;
};
