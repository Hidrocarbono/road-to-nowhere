#include "hud.h"
#include "hud_bloody.h"
#include "utils.h"
#include "enginecallback.h"

// RTN F10: HUD de sangue em tela cheia (estilo COD).
// O bloodyhud.spr (Paranoia 2) cobre a tela toda. A intensidade (alpha)
// combina:
//   - pico imediato ao tomar dano (fade ~1.2s) - usa o m_flDamageTime interno
//     setado pelo proprio Draw (comparando a vida que caiu)
//   - nivel residual proporcional a vida baixa (quanto menos vida, mais
//     sangue constante nas bordas - imersao/tensao)
// O sprite e desenhado com SPR_DrawAdditive (como o 640_pain do HL).
//
// NAO hooka Health/Damage: o CHudHealth ja hookou essas mensagens e o engine
// ignora hook duplicado (pfnHookUserMsg). Le o m_iHealth do gHUD.m_Health.

#define BLOODY_SPRITE	"sprites/bloodyhud.spr"

// residuo por vida: 0% vida = alpha ~180, 100% = 0
static float RTN_BloodyResidue( int iHealth )
{
	float hp = (float)iHealth;
	if( hp <= 0.0f ) hp = 0.0f;
	if( hp >= 100.0f ) return 0.0f;
	return ( 100.0f - hp ) * 1.8f;  // 180 no maximo
}

int CHudBloody::Init( void )
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	m_iHealth = 100;
	m_flDamageTime = 0.0f;
	m_flDamageAmt = 0.0f;
	return 1;
}

int CHudBloody::VidInit( void )
{
	m_iHealth = 100;
	m_flDamageTime = 0.0f;
	m_flDamageAmt = 0.0f;
	m_iBloody = LoadSprite( BLOODY_SPRITE );
	return 1;
}

int CHudBloody::Draw( float flTime )
{
	if( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL )
		return 1;

	if( !m_iBloody )
		return 1;  // sprite nao carregou

	// detecta dano: a vida caiu desde o ultimo frame
	int iHealth = gHUD.m_Health.m_iHealth;
	if( iHealth < m_iHealth )
	{
		int iDmg = m_iHealth - iHealth;
		m_flDamageTime = flTime;
		m_flDamageAmt = Q_min( 200.0f, (float)iDmg * 5.0f );
	}
	m_iHealth = iHealth;

	// tempo desde o ultimo dano
	float fSince = flTime - m_flDamageTime;
	float fPeak = 0.0f;
	if( fSince >= 0.0f && fSince < 1.2f )
	{
		// decai linearmente em 1.2s
		fPeak = m_flDamageAmt * ( 1.0f - fSince / 1.2f );
	}

	// residuo por vida baixa
	float fResidue = RTN_BloodyResidue( iHealth );

	float fAlpha = fPeak + fResidue;
	if( fAlpha <= 2.0f )
		return 1;

	int a = (int)fAlpha;
	if( a > 255 ) a = 255;

	// tela cheia (SPR_DrawAdditive usa o tamanho nativo do frame - o
	// bloodyhud.spr do P2 e fullscreen 640x480)
	SPR_Set( m_iBloody, 255, 255, 255 );
	SPR_DrawAdditive( 0, 0, 0, NULL );

	// bordas vermelhas escuras quando a vida esta muito baixa (imersao)
	if( iHealth <= 20 )
	{
		int r = 120, g = 0, b = 0, edge = ( 20 - iHealth ) * 3;
		edge = Q_min( 40, edge );
		FillRGBA( 0, 0, ScreenWidth, edge, r, g, b, 60 );
		FillRGBA( 0, ScreenHeight - edge, ScreenWidth, edge, r, g, b, 60 );
	}

	return 1;
}
