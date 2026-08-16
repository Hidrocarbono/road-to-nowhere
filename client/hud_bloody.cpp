#include "hud.h"
#include "hud_bloody.h"
#include "utils.h"
#include "enginecallback.h"
#include "triangleapi.h"  // pTriAPI->RenderMode (kRenderTransAlpha p/ o blend)

// RTN F10: HUD de sangue em tela cheia (estilo COD).
// O bloodyhud.spr cobre a tela TODA (esticado via pfnSPR_DrawGeneric).
// Intensidade (alpha) INVERSA a vida: 100% HP = quase transparente,
// 0% HP = sangue forte constante. Ao tomar dano, um pico de sangue
// aparece e decai em ~1.2s.
//
// NAO hooka Health/Damage: o CHudHealth ja hookou e o engine ignora hook
// duplicado. Le o m_iHealth do gHUD.m_Health (publico) a cada frame.

#define BLOODY_SPRITE	"sprites/bloodyhud.spr"

// alpha base inverso a vida: 100 HP -> 0, 50 HP -> 160 (visivel na metade!),
// 0 HP -> 320 (clamp 255 - sangue forte). O user pediu: comeca a aparecer
// de forma ostensiva na METADE da vida (antes so no limite).
static float RTN_BloodyBase( int iHealth )
{
	float hp = (float)iHealth;
	if( hp <= 0.0f ) hp = 0.0f;
	if( hp >= 100.0f ) return 0.0f;
	return ( 100.0f - hp ) * 4.5f;   // RTN F10: mais visivel ainda (50 HP -> 225)
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

	// pico de dano (decai em 1.2s)
	float fSince = flTime - m_flDamageTime;
	float fPeak = 0.0f;
	if( fSince >= 0.0f && fSince < 1.2f )
		fPeak = m_flDamageAmt * ( 1.0f - fSince / 1.2f );

	// base inversa a vida (100% HP = 0, 0% HP = 320 -> clamp 255)
	float fBase = RTN_BloodyBase( iHealth );

	// RTN F10: efeito pulsante (aparecendo e dissolvendo) - o sin oscila
	// o alpha ~ +/-30% num ciclo de ~2.5s. Quanto mais dano, mais forte o
	// pulso (a oscilacao eh proporcional ao fBase, que cresce com o dano).
	float fPulse = sin( flTime * 2.5f ) * 0.5f + 0.5f;   // 0..1
	float fAlpha = fPeak + fBase * ( 0.7f + 0.3f * fPulse );
	if( fAlpha <= 2.0f )
		return 1;

	int a = (int)fAlpha;
	if( a > 255 ) a = 255;

	// sprite esticado em TELA CHEIA (pfnSPR_DrawGeneric com width/height).
	// kRenderTransAdd (additive, como o CS) + ScaleColors: o brilho da cor
	// controla a intensidade (vida alta = cor escura = quase invisivel; vida
	// baixa = cor clara = sangue forte) e a cena aparece ATRAVES (additive
	// nao cobre). O fundo preto do sprite foi convertido p/ alpha 0 no v32
	// (tools/spr2_to_v32.py) - sem quadrado preto.
	int r = 255, g = 255, b = 255;
	ScaleColors( r, g, b, a );
	SPR_Set( m_iBloody, r, g, b );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pfnSPR_DrawGeneric( 0, 0, 0, NULL, 0, 0, ScreenWidth, ScreenHeight );
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );

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
