#pragma once

// RTN F9: HUD lateral esquerdo com doses de estimulante (V) e painkiller (H).
// NAO incluir hud.h aqui (include circular!). Este header eh incluido PELO hud.h
// apos a definicao de CHudBase, igual ao health.h/ammo.h. O .cpp inclui hud.h primeiro.
// Estilo Paranoia 2: icone + contador fixos na lateral esquerda da tela.
// Recebe gmsgRTNItems (2 shorts: doses estimulante, doses painkiller).
class CHudRTNItems : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	int MsgFunc_RTNItems( const char *pszName, int iSize, void *pbuf );

	int m_iStimDoses = 0;
	int m_iPainDoses = 0;

	// RTN F10 fix (Erro 3): RefreshAllUI - pede o estado do inventario ao
	// server no primeiro frame (o give inicial pode perder a mensagem se o
	// HUD do client ainda nao estava pronto; o query forca o reenvio).
	bool m_bSentInitialQuery = false;
};
