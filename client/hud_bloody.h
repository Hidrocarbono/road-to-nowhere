// hud_bloody.h - RTN F10: HUD de sangue em tela cheia (estilo COD)
// O bloodyhud.spr cobre a tela com intensidade proporcional ao dano recente
// + vida baixa (quanto menos vida, mais sangue persistente nas bordas).
#ifndef HUD_BLOODY_H
#define HUD_BLOODY_H

#include "hud.h"

class CHudBloody : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );

private:
	int m_iHealth;        // vida do frame anterior (detecta queda)
	float m_flDamageTime; // quando o ultimo dano aconteceu
	float m_flDamageAmt;  // magnitude do ultimo dano
	int m_iBloody;        // sprite handle
};

#endif//HUD_BLOODY_H
