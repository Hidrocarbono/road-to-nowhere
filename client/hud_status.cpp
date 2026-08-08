#include "hud.h"          // primeiro: define CHudBase e inclui hud_status.h (pos CHudBase)
#include "hud_status.h"
#include "utils.h"
#include "triangleapi.h"
#include "enginecallback.h"

// RTN F10: HUD de status (canto inferior esquerdo) estilo Paranoia 2.
//
// TECNICA DE MASCARA (fill): 2 sprites + SCISSOR (GPU, nativo do engine):
//   1. Empty (fundo) desenhado COMPLETO.
//   2. pfnSPR_EnableScissor(rect da area cheia) + Full desenhado COMPLETO
//      por cima -> o GL recorta o Full ao rect. Recorte REAL (sem esticar).
//   - Horizontal (barras): rect = (x, y, w*fill, h)   -> consumo da DIREITA.
//   - Vertical (silhueta): rect = (x, y + h*(1-fill), w, h*fill) -> consumo
//     de CIMA p/ BAIXO (o vermelho "desce"; o contorno escuro permanece).
//
// FLASH DE DANO: quando m_iHealth cai, desenha o sprite *_flash com cor
// decrescente (aditivo) ao longo de 0.3s + dispara o micro-shake (0.15s).
//
// cvar rtn_hud_style: 0 = HUD classico do HL, 1 = HUD novo (desliga os
// classicos health/battery/ammo e desenha este).

cvar_t *rtn_hud_style = NULL;  // global: health.cpp/battery.cpp/ammo.cpp leem

// momento do ultimo dano - lido pelo V_CalcView (r_view.cpp) p/ o micro-shake
float g_flRTNShakeTime = -999.0f;

// tempo do flash de dano (s) e do micro-shake (s)
#define RTN_DAMAGE_FLASH_TIME	0.3f
#define RTN_DAMAGE_SHAKE_TIME	0.15f

// ============================================================
// desenha um sprite com mascara (fill) - tecnica do scissor
// ============================================================
static void RTN_DrawFill( int x, int y, int w, int h, SpriteHandle hEmpty, SpriteHandle hFull, float fill, bool bVertical )
{
	if( !hEmpty || !hFull )
		return;

	if( fill < 0.0f ) fill = 0.0f;
	if( fill > 1.0f ) fill = 1.0f;

	// 1) fundo (vazio) completo
	SPR_Set( hEmpty, 255, 255, 255 );
	SPR_Draw( 0, x, y, NULL );

	// 2) frente (cheio) COMPLETO, recortado pelo scissor
	if( fill > 0.0f )
	{
		int fw = w, fh = h, fx = x, fy = y;
		if( bVertical )
		{
			// consumo de CIMA p/ BAIXO: area cheia e a parte INFERIOR
			fh = (int)((float)h * fill + 0.5f);
			fy = y + (h - fh);
		}
		else
		{
			// consumo da DIREITA p/ ESQUERDA: area cheia e a parte ESQUERDA
			fw = (int)((float)w * fill + 0.5f);
		}

		if( fw > 0 && fh > 0 )
		{
			SPR_EnableScissor( fx, fy, fw, fh );
			SPR_Set( hFull, 255, 255, 255 );
			SPR_Draw( 0, x, y, NULL );
			SPR_DisableScissor();
		}
	}
}

// ============================================================
// CHudStatus
// ============================================================
int CHudStatus::Init( void )
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;

	if( !rtn_hud_style )
		rtn_hud_style = gEngfuncs.pfnRegisterVariable( "rtn_hud_style", "1", FCVAR_ARCHIVE );

	return 1;
}

int CHudStatus::VidInit( void )
{
	m_iPrevHealth = 100;
	m_flDamageTime = -100.0f;

	m_hHealthEmpty	= LoadSprite( "sprites/rtn_hud_health_empty.spr" );
	m_hHealthFull	= LoadSprite( "sprites/rtn_hud_health_full.spr" );
	m_hHealthFlash	= LoadSprite( "sprites/rtn_hud_health_flash.spr" );
	m_hArmorEmpty	= LoadSprite( "sprites/rtn_hud_armor_empty.spr" );
	m_hArmorFull	= LoadSprite( "sprites/rtn_hud_armor_full.spr" );
	m_hArmorFlash	= LoadSprite( "sprites/rtn_hud_armor_flash.spr" );
	m_hArmorIcon	= LoadSprite( "sprites/rtn_hud_armor_icon.spr" );
	m_hStaminaEmpty	= LoadSprite( "sprites/rtn_hud_stamina_empty.spr" );
	m_hStaminaFull	= LoadSprite( "sprites/rtn_hud_stamina_full.spr" );
	m_hStaminaIcon	= LoadSprite( "sprites/rtn_hud_stamina_icon.spr" );

	return 1;
}

void CHudStatus::Reset( void )
{
	m_iPrevHealth = 100;
	m_flDamageTime = -100.0f;
}

int CHudStatus::Draw( float flTime )
{
	if( !rtn_hud_style || rtn_hud_style->value < 1.0f )
		return 0;  // HUD classico ativo

	cl_entity_t *pLocal = gEngfuncs.GetLocalPlayer();
	if( !pLocal )
		return 0;

	// ---- dados (dirty-check + deteccao de dano) ----
	int iHealth = gHUD.m_Health.m_iHealth;
	int iBat = gHUD.m_Battery.GetBat();
	float flStamina = pLocal->curstate.fuser2;

	if( iHealth < m_iPrevHealth )
		m_flDamageTime = flTime;  // tomou dano: flash + micro-shake

	m_iPrevHealth = iHealth;

	if( gHUD.m_fPlayerDead )
		return 0;

	// ---- layout (escalado pela resolucao) ----
	// Silhueta do soldado (vida): sprite 142x168, no canto inferior esquerdo
	int sw = SPR_Width( m_hHealthEmpty, 0 );
	int sh = SPR_Height( m_hHealthEmpty, 0 );
	int sx = XRES( 12 );
	int sy = ScreenHeight - YRES( 12 ) - sh;

	// barras: 240x10 originais, escaladas; armor em cima, stamina embaixo
	int bw = XRES( 96 );
	int bh = YRES( 8 );
	int bx = XRES( 12 );
	int armorBy = ScreenHeight - YRES( 12 ) - bh - YRES( 2 );
	int staminaBy = ScreenHeight - YRES( 12 );

	// icones 20x20 ao lado esquerdo de cada barra
	int iw = XRES( 14 );
	int iconYarmor = armorBy - ( iw - bh ) / 2;
	int iconYstam = staminaBy - ( iw - bh ) / 2;

	// ---- vida: silhueta (mascara VERTICAL, consumo de cima p/ baixo) ----
	float fHealthFill = (float)iHealth / 100.0f;
	RTN_DrawFill( sx, sy, sw, sh, m_hHealthEmpty, m_hHealthFull, fHealthFill, true );

	// flash de dano sobre a silhueta (alpha/cor decrescente em 0.3s)
	float fFlashFrac = ( flTime - m_flDamageTime ) / RTN_DAMAGE_FLASH_TIME;
	if( fFlashFrac >= 0.0f && fFlashFrac < 1.0f && m_hHealthFlash )
	{
		int fA = (int)( 200.0f * ( 1.0f - fFlashFrac ) );
		if( fA > 0 )
		{
			SPR_EnableScissor( sx, sy + (int)((float)sh * ( 1.0f - fHealthFill )), sw, (int)((float)sh * fHealthFill ));
			SPR_Set( m_hHealthFlash, fA, fA, fA );
			SPR_DrawAdditive( 0, sx, sy, NULL );
			SPR_DisableScissor();
		}
	}

	// ---- armor: barra AZUL + icone escudo (mascara HORIZONTAL) ----
	if( m_hArmorIcon )
	{
		SPR_Set( m_hArmorIcon, 255, 255, 255 );
		SPR_Draw( 0, XRES( 6 ), iconYarmor, NULL );
	}
	float fArmorFill = (float)iBat / 100.0f;
	RTN_DrawFill( bx, armorBy, bw, bh, m_hArmorEmpty, m_hArmorFull, fArmorFill, false );

	// ---- stamina: barra VERDE + icone raio (mascara HORIZONTAL) ----
	if( m_hStaminaIcon )
	{
		SPR_Set( m_hStaminaIcon, 255, 255, 255 );
		SPR_Draw( 0, XRES( 6 ), iconYstam, NULL );
	}
	float fStaminaFill = flStamina / 100.0f;
	RTN_DrawFill( bx, staminaBy, bw, bh, m_hStaminaEmpty, m_hStaminaFull, fStaminaFill, false );

	return 1;
}
