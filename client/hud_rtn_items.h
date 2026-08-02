#pragma once
#include "hud.h"

// RTN F9: HUD lateral esquerdo com doses de estimulante (V) e painkiller (H).
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
};
