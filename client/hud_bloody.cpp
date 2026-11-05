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

// RTN: debug/preferencia. 0 desliga SO este HUD de sangue (nao persiste entre
// sessoes de proposito - chave de diagnostico, ver CHANGELOG_AGENT.md secao 4).
// Serve para separar o que e do CHudBloody do que vem de outra fonte de
// efeito de tela (PostFx do mapa, screenfade do servidor etc).
static cvar_t *rtn_hud_bloody = NULL;

// RTN: expoente da curva de intensidade. alpha = 255 * ((100 - hp) / 100) ^ p.
//   p = 1.0  linear
//   p < 1.0  mais forte em vida media (a "metade da vida" fica ostensiva, como
//            o usuario pediu), mas a curva continua SUBINDO ate 0 HP
// Antes: alpha = (100 - hp) * 4.5, limitado a 255 - saturava em 45 HP, entao
// qualquer cura entre ~45 e 0 HP (ex.: um medkit) nao mudava NADA na tela, e a
// pulsacao sumia abaixo de ~35 HP. Agora a faixa 100..0 inteira e util.
//   hp:  75    50    25    10    0
//   p=0.7: 97   157   209   237   255   (antes: 112  225  255  255  255)
static cvar_t *rtn_hud_bloody_curve = NULL;

static float RTN_BloodyBase( int iHealth )
{
	float hp = (float)iHealth;
	if( hp <= 0.0f ) hp = 0.0f;
	if( hp >= 100.0f ) return 0.0f;

	float p = rtn_hud_bloody_curve ? rtn_hud_bloody_curve->value : 0.7f;
	p = Q_min( 3.0f, Q_max( 0.2f, p ));	// bound: evita expoente absurdo

	float t = ( 100.0f - hp ) / 100.0f;	// 0 = vida cheia, 1 = sem vida
	return 255.0f * powf( t, p );
}

int CHudBloody::Init( void )
{
	// nao ARCHIVE: ver o comentario de rtn_hud_bloody acima
	rtn_hud_bloody = gEngfuncs.pfnRegisterVariable( "rtn_hud_bloody", "1", 0 );
	rtn_hud_bloody_curve = gEngfuncs.pfnRegisterVariable( "rtn_hud_bloody_curve", "0.7", FCVAR_ARCHIVE );
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

	// RTN: rtn_hud_bloody 0 desliga o efeito inteiro (sprite E bordas).
	// Mesmo desligado, atualiza m_iHealth para nao disparar um falso "pico de
	// dano" ao religar.
	if( rtn_hud_bloody && rtn_hud_bloody->value < 1.0f )
	{
		m_iHealth = gHUD.m_Health.m_iHealth;
		return 1;
	}

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

	// ANALISE (nao era o canal alpha - conferi contra o codigo-fonte do
	// engine, ref/gl/gl_backend.c e engine/client/dll_int/cl_game.c, do fork
	// do usuario):
	//   - GL_SetRenderMode(kRenderTransAdd) usa glBlendFunc(GL_SRC_ALPHA,
	//     GL_ONE) com GL_ALPHA_TEST desligado - o alpha por pixel do v32 E
	//     lido e usado normalmente, sem corte binario nem banda.
	//   - o .spr convertido (tools/spr2_to_v32.py) fechou certo: 256x256,
	//     header v32 valido, bordas com alpha ~0-144 e RGB vermelho, centro
	//     (0,0,0,0) totalmente transparente - renderizei os pixels crus e
	//     bate com uma vinheta de sangue normal, sem quadrado nem artefato.
	//
	// O bug real e mais simples: o sprite e QUADRADO (256x256) e estava
	// sendo esticado direto para ScreenWidth x ScreenHeight SEM preservar
	// proporcao. Numa tela 16:9 isso estica ~1.8x mais no eixo horizontal
	// que no vertical - as manchas de sangue (originalmente redondas) saem
	// achatadas/ovaladas e a vinheta perde a simetria, o que facilmente lia
	// como "a textura ficou errada" (o proprio canal alpha, sendo o que
	// desenha a forma das manchas, e o que mais evidencia essa distorcao).
	//
	// Fix: escala UNIFORME que cobre a tela toda (o maior dos dois fatores),
	// centralizada - o excesso e cortado pelo viewport sozinho, sem precisar
	// de recorte manual de UV.
	int texW = SPR_Width( m_iBloody, 0 );
	int texH = SPR_Height( m_iBloody, 0 );
	if( texW <= 0 ) texW = 256;
	if( texH <= 0 ) texH = 256;

	float flScale = Q_max( (float)ScreenWidth / (float)texW, (float)ScreenHeight / (float)texH );
	int iDrawW = (int)( texW * flScale + 0.5f );
	int iDrawH = (int)( texH * flScale + 0.5f );
	int iDrawX = ( ScreenWidth - iDrawW ) / 2;
	int iDrawY = ( ScreenHeight - iDrawH ) / 2;

	gEngfuncs.pfnSPR_DrawGeneric( 0, iDrawX, iDrawY, NULL, 0, 0, iDrawW, iDrawH );
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );

	// bordas vermelhas escuras quando a vida esta muito baixa (imersao).
	// RTN: antes eram DUAS BARRAS SOLIDAS de ate 40 px (alpha fixo 60), com borda
	// reta - liam como "tarja"/letterbox. Agora e um DEGRADE: alpha maximo na
	// beirada da tela caindo a zero para dentro, e o tamanho/forca crescem
	// com a vida baixa (k: 0 em 25 HP -> 1 em 0 HP), nao de repente aos 20.
	if( iHealth < 25 )
	{
		float k = ( 25.0f - (float)Q_max( iHealth, 0 )) / 25.0f;	// 0..1
		const int nSteps = 16;
		int bandH = (int)( (float)ScreenHeight * 0.07f * k + 0.5f );	// ~75 px a 1080p, no maximo
		int stepH = Q_max( 1, bandH / nSteps );
		int maxAlpha = (int)( 90.0f * k );

		for( int i = 0; i < nSteps; i++ )
		{
			float f = 1.0f - (float)i / (float)nSteps;	// 1 na beirada -> ~0 para dentro
			int a = (int)( (float)maxAlpha * f * f );		// queda suave (quadratica)
			if( a <= 0 )
				break;
			FillRGBA( 0, i * stepH, ScreenWidth, stepH, 120, 0, 0, a );
			FillRGBA( 0, ScreenHeight - ( i + 1 ) * stepH, ScreenWidth, stepH, 120, 0, 0, a );
		}
	}

	return 1;
}
