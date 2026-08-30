/*
gl_nvg.cpp - Road to Nowhere: visao noturna (client-side)

Ver gl_nvg.h para a visao geral das duas partes (ganho de exposicao +
iluminador IR) e do porque essa abordagem, e nao um postfx de tint.

Historico curto, para nao repetir o erro: a primeira tentativa de NVG deste
mod fazia tudo em postfx/postprocessing, que roda DEPOIS do tonemap
(gl_backend.cpp: RenderTonemap -> ... -> RenderPostprocessing). Naquele ponto
a cena ja e LDR e a exposicao ja foi limitada em 1.0, ou seja, a informacao do
escuro ja foi jogada fora. Somado a isso, u_Brightness la e ADITIVO: num pixel
preto ele so levanta o piso, uniforme, e o tint verde por cima virava uma
mancha chapada sem contraste. Nao existe valor de brightness/levels que
conserte isso - por isso o ganho aqui e feito no auto-exposure, em HDR, antes
da compressao.
*/

#include "hud.h"
#include <stringlib.h>
#include "utils.h"
#include "gl_local.h"
#include "gl_cvars.h"
#include "gl_nvg.h"
#include "postfx_parameters.h"

// valores nativos do jogo (iguais aos consts que existiam no
// postfx/generate_exposure_fp.glsl antes de virarem uniform)
#define NVG_EXPOSURE_MAX_DEFAULT	1.0f
#define NVG_EXPOSURE_SCALE_DEFAULT	1.0f
#define NVG_ADAPT_DARK_DEFAULT		0.6f
#define NVG_ADAPT_BRIGHT_DEFAULT	1.6f

// tempos da rampa do tubo (liga/desliga)
#define NVG_WARMUP_TIME			0.35f
#define NVG_COOLDOWN_TIME		0.25f

// abaixo disso a bateria comeca a fazer o tubo tremular
#define NVG_LOW_BATTERY			15

static cvar_t *rtn_nvg_gain = NULL;			// teto de exposicao com NVG ligado
static cvar_t *rtn_nvg_ir = NULL;			// 0/1 - iluminador IR
static cvar_t *rtn_nvg_ir_radius = NULL;		// alcance do iluminador
static cvar_t *rtn_nvg_ir_intensity = NULL;		// intensidade do iluminador
static cvar_t *rtn_nvg_tint = NULL;			// 0/1 - tint verde + grain + vinheta
static cvar_t *rtn_nvg_debug = NULL;

static bool	s_bActive = false;
static int	s_iBattery = 100;
static float	s_flStateTime = 0.0f;	// quando o estado mudou pela ultima vez
static float	s_flLastIntensity = 0.0f;

void RTN_NVG_RegisterCvars( void )
{
	// teto de exposicao: e o "ganho do tubo". 1.0 = exposicao normal do jogo
	// (nenhuma amplificacao), 12 = ate 12x mais luz nas cenas escuras. O
	// auto-exposure continua limitando sozinho quando a cena esta clara, entao
	// isso nao estoura ambiente iluminado - so libera teto para o escuro.
	rtn_nvg_gain = CVAR_REGISTER( "rtn_nvg_gain", "12.0", FCVAR_ARCHIVE );
	rtn_nvg_ir = CVAR_REGISTER( "rtn_nvg_ir", "1", FCVAR_ARCHIVE );
	// raio curto de proposito: cada dlight custa um passe aditivo sobre a
	// geometria dentro do volume dela (R_RenderDynLightList). 300 unidades
	// cobrem o corredor a frente sem transformar isso numa segunda cena.
	rtn_nvg_ir_radius = CVAR_REGISTER( "rtn_nvg_ir_radius", "300.0", FCVAR_ARCHIVE );
	rtn_nvg_ir_intensity = CVAR_REGISTER( "rtn_nvg_ir_intensity", "0.35", FCVAR_ARCHIVE );
	rtn_nvg_tint = CVAR_REGISTER( "rtn_nvg_tint", "1", FCVAR_ARCHIVE );
	rtn_nvg_debug = CVAR_REGISTER( "rtn_nvg_debug", "0", 0 );
}

// O ganho de exposicao so existe se o pipeline HDR estiver rodando:
// gl_backend.cpp so chama RenderAverageLuminance/RenderTonemap quando gl_hdr
// esta ligado, e o RenderTonemap ainda checa r_tonemap. Com qualquer um dos
// dois desligado nao ha passe de exposicao para levantar, e o NVG cai no
// caminho degradado (iluminador IR + ganho modesto em LDR).
static bool RTN_NVG_ExposurePathActive( void )
{
	return ( CVAR_GET_FLOAT( "gl_hdr" ) != 0.0f && CVAR_GET_FLOAT( "r_tonemap" ) != 0.0f );
}

void RTN_NVG_SetState( bool bActive, int iBattery )
{
	s_iBattery = bound( 0, iBattery, 100 );

	if( bActive == s_bActive )
		return;

	// a rampa parte de onde a anterior estava, para nao dar salto se o
	// jogador bater no botao duas vezes seguidas
	s_flLastIntensity = RTN_NVG_GetIntensity();
	s_bActive = bActive;
	s_flStateTime = tr.time;

	if( CVAR_TO_BOOL( rtn_nvg_debug ))
		ALERT( at_console, "NVG: %s (bateria %i%%)\n", bActive ? "ligado" : "desligado", s_iBattery );

	// aviso unico: sem HDR/tonemap o NVG perde justamente a parte que faz
	// enxergar. Melhor dizer isso uma vez do que o jogador achar que quebrou.
	static bool warned = false;
	if( bActive && !warned && !RTN_NVG_ExposurePathActive( ))
	{
		warned = true;
		ALERT( at_warning, "NVG: gl_hdr/r_tonemap desligados - sem ganho de exposicao, o efeito fica fraco\n" );
	}
}

void RTN_NVG_Reset( void )
{
	s_bActive = false;
	s_iBattery = 100;
	s_flStateTime = 0.0f;
	s_flLastIntensity = 0.0f;
}

bool RTN_NVG_IsActive( void )
{
	return s_bActive;
}

float RTN_NVG_GetIntensity( void )
{
	float target = s_bActive ? 1.0f : 0.0f;
	float rampTime = s_bActive ? NVG_WARMUP_TIME : NVG_COOLDOWN_TIME;
	float elapsed = tr.time - s_flStateTime;
	float t = ( rampTime > 0.0f ) ? bound( 0.0f, elapsed / rampTime, 1.0f ) : 1.0f;
	float intensity = s_flLastIntensity + ( target - s_flLastIntensity ) * t;

	// bateria no fim: o tubo pisca. E so uma modulacao do valor que ja seria
	// calculado - nao custa nada e comunica o estado sem HUD nenhum.
	if( s_bActive && s_iBattery <= NVG_LOW_BATTERY )
	{
		float health = (float)s_iBattery / (float)NVG_LOW_BATTERY;
		float flicker = sin( tr.time * 21.0f ) * sin( tr.time * 7.3f );
		intensity *= 1.0f - ( 1.0f - health ) * 0.45f * ( 0.5f + 0.5f * flicker );
	}

	return bound( 0.0f, intensity, 1.0f );
}

void RTN_NVG_GetExposureParams( float *pOut4 )
{
	float i = RTN_NVG_GetIntensity();

	pOut4[0] = NVG_EXPOSURE_MAX_DEFAULT;
	pOut4[1] = NVG_EXPOSURE_SCALE_DEFAULT;
	pOut4[2] = NVG_ADAPT_DARK_DEFAULT;
	pOut4[3] = NVG_ADAPT_BRIGHT_DEFAULT;

	if( i <= 0.0f )
		return;

	float gain = Q_max( 1.0f, rtn_nvg_gain ? rtn_nvg_gain->value : 12.0f );

	// interpola entre o comportamento nativo e o do NVG pela rampa do tubo.
	// A adaptacao ao claro fica mais rapida que a nativa de proposito: e o
	// "gating" do NVG real, que fecha o ganho quando uma luz entra em campo.
	pOut4[0] = NVG_EXPOSURE_MAX_DEFAULT + ( gain - NVG_EXPOSURE_MAX_DEFAULT ) * i;
	pOut4[2] = NVG_ADAPT_DARK_DEFAULT + ( 0.9f - NVG_ADAPT_DARK_DEFAULT ) * i;
	pOut4[3] = NVG_ADAPT_BRIGHT_DEFAULT + ( 4.0f - NVG_ADAPT_BRIGHT_DEFAULT ) * i;
}

float RTN_NVG_GetFallbackGain( void )
{
	// so entra em cena com gl_hdr/r_tonemap 0, quando o passe de exposicao nao roda.
	// Aqui a cena ja e LDR, entao o ganho tem que ser modesto: multiplicar
	// muito nesse ponto so estoura os claros (foi exatamente o erro da
	// primeira versao do NVG deste mod).
	float i = RTN_NVG_GetIntensity();
	if( i <= 0.0f )
		return 1.0f;

	float gain = Q_max( 1.0f, rtn_nvg_gain ? rtn_nvg_gain->value : 12.0f );
	float fallback = 1.0f + ( gain - 1.0f ) * 0.12f;
	fallback = bound( 1.0f, fallback, 3.0f );
	return 1.0f + ( fallback - 1.0f ) * i;
}

void RTN_NVG_ApplyPostFx( CPostFxParameters &fx )
{
	if( !CVAR_TO_BOOL( rtn_nvg_tint ))
		return;

	float i = RTN_NVG_GetIntensity();
	if( i <= 0.0f )
		return;

	// tint: dessatura primeiro e tinge depois (a ordem no shader ja e essa:
	// saturation -> brightness -> levels -> contrast). Verde de fosforo, mas
	// discreto - o que faz enxergar e o ganho e o contraste, nao a cor.
	// com o tonemap ligado (padrao) o ganho ja veio da exposicao, e aqui os
	// levels servem so de tint. Com r_tonemap 0 nao existe passe de exposicao,
	// entao o unico ganho possivel e este, em LDR e por isso modesto.
	float gainComp = RTN_NVG_ExposurePathActive() ? 1.0f : RTN_NVG_GetFallbackGain();

	fx.SetSaturation( fx.GetSaturation() + ( 0.10f - fx.GetSaturation() ) * i );
	fx.SetContrast( fx.GetContrast() + ( 1.20f - fx.GetContrast() ) * i );
	fx.SetRedLevel(( fx.GetRedLevel() + ( 0.45f - fx.GetRedLevel() ) * i ) * gainComp );
	fx.SetGreenLevel(( fx.GetGreenLevel() + ( 1.25f - fx.GetGreenLevel() ) * i ) * gainComp );
	fx.SetBlueLevel(( fx.GetBlueLevel() + ( 0.55f - fx.GetBlueLevel() ) * i ) * gainComp );
	fx.SetVignetteScale( Q_max( fx.GetVignetteScale(), 0.85f * i ));
	fx.SetFilmGrainScale( Q_max( fx.GetFilmGrainScale(), 0.10f * i ));
}

/*
===============
RTN_NVG_SetupPlayerLight

Iluminador IR: dlight omni presa ao jogador local. Sem sombra (senao um
LIGHT_OMNI aloca as 6 faces do depthCubemap por frame) e sem bump - e luz de
preenchimento, nao precisa de detalhe. Molde: R_SetupPlayerFlashlight.
===============
*/
void RTN_NVG_SetupPlayerLight( cl_entity_t *ent )
{
	if( !CVAR_TO_BOOL( rtn_nvg_ir ))
		return;

	float i = RTN_NVG_GetIntensity();
	if( i <= 0.0f )
		return;

	if( !UTIL_IsLocal( ent->index ))
		return;	// so o jogador local ve o proprio NVG

	Vector origin, angles, forward, right, up;
	vec3_t viewOffset;
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();

	if( !localPlayer )
		return;

	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( viewOffset );
	origin = localPlayer->origin + Vector( viewOffset );

	gEngfuncs.GetViewAngles( angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	origin += forward * 4.0f;	// um palmo a frente dos olhos, como o emissor

	float radius = Q_max( 32.0f, rtn_nvg_ir_radius->value );
	float intensity = Q_max( 0.0f, rtn_nvg_ir_intensity->value ) * i;

	CDynLight *pl = CL_AllocDlight( NVG_LIGHT_KEY - ent->index );
	R_SetupLightParams( pl, origin, angles, radius, 90.0f, LIGHT_OMNI, DLF_NOSHADOWS|DLF_NOBUMP );

	// levemente esverdeada: o tint do postfx ja faz o resto
	pl->color = Vector( 0.75f, 1.0f, 0.80f ) * intensity;
	pl->parentEntity = ent;
	pl->die = tr.time + 0.05f;
}
