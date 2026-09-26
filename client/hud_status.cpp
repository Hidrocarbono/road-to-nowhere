#include "hud.h"          // primeiro: define CHudBase e inclui hud_status.h (pos CHudBase)
#include "hud_status.h"
#include "utils.h"
#include "parsemsg.h"     // DECLARE_MESSAGE/HOOK_MESSAGE/BEGIN_READ/READ_SHORT
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

// RTN F10 fix: STAMINA compartilhada (0-100). Setada pelo MsgFunc_Stamina
// (mensagem do server - o curstate.fuser2 nao chega ao jogador local).
// Lida pelo CHudStatus (barra) e pelo input.cpp (bloqueio do pulo).
float g_flRTNStamina = 100.0f;

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

// RTN F10 fix: STAMINA via mensagem (estilo P2 - o server envia com dirty-check)
DECLARE_MESSAGE( m_Status, Stamina );

// ============================================================
// CHudStatus
// ============================================================
int CHudStatus::Init( void )
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	HOOK_MESSAGE( Stamina );  // RTN F10 fix

	if( !rtn_hud_style )
		rtn_hud_style = gEngfuncs.pfnRegisterVariable( "rtn_hud_style", "1", FCVAR_ARCHIVE );

	return 1;
}

// RTN F10 fix: recebe a stamina do server (READ_SHORT) - o curstate.fuser2
// nao chega ao jogador local (clientdata -> pmove apenas, nao -> curstate).
int CHudStatus::MsgFunc_Stamina( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	m_flStamina = (float)READ_SHORT();
	g_flRTNStamina = m_flStamina;  // compartilhada com o input.cpp (pulo)
	END_READ();
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
	m_flStamina = 100.0f;
	g_flRTNStamina = 100.0f;
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
	// RTN F10 fix: stamina vem da MENSAGEM "Stamina" (o curstate.fuser2 nao
	// chega ao jogador local) - m_flStamina e atualizado pelo MsgFunc_Stamina
	float flStamina = m_flStamina;

	if( iHealth < m_iPrevHealth )
		m_flDamageTime = flTime;  // tomou dano: flash + micro-shake

	m_iPrevHealth = iHealth;

	if( gHUD.m_fPlayerDead )
		return 0;

	// ---- layout (escalado pela resolucao) ----
	// RTN F10 fix (alinhamento): uma UNICA margem esquerda pra silhueta E
	// icones - antes a silhueta ficava em XRES(12) e os icones em XRES(6),
	// dois X diferentes sem relacao nenhuma (silhueta "flutuava" fora do
	// alinhamento da coluna de icones+barras). E uma UNICA margem inferior
	// pra silhueta E a barra de stamina - antes a base da barra de stamina
	// ficava BH pixels mais baixa que a base da silhueta (staminaBy era usado
	// como TOPO da barra, nao base), quebrando o nivelamento dos dois.
	int marginX = XRES( 12 );
	int bottomMargin = YRES( 12 );

	// Silhueta do soldado (vida): sprite 142x168, no canto inferior esquerdo
	int sw = SPR_Width( m_hHealthEmpty, 0 );
	int sh = SPR_Height( m_hHealthEmpty, 0 );
	int sx = marginX;
	int sy = ScreenHeight - bottomMargin - sh;

	// barras: 240x10 originais, escaladas; armor em cima, stamina embaixo
	int bw = XRES( 96 );
	int bh = YRES( 8 );
	int barGap = YRES( 3 );	// espaco vertical entre as duas barras

	// RTN F10 fix: a barra comecava em XRES(12) mas o icone era desenhado em
	// XRES(6) com ~14 unidades de largura - o icone TERMINAVA depois da barra
	// COMECAR (sobreposicao). Agora o X da barra nasce da largura REAL do
	// maior dos dois icones (+ um respiro fixo), entao nunca sobrepoe nenhum
	// dos dois - e as duas barras compartilham o MESMO bx, alinhadas entre si.
	int iconWArmor = SPR_Width( m_hArmorIcon, 0 );
	int iconWStam = SPR_Width( m_hStaminaIcon, 0 );
	int iconSlotW = Q_max( iconWArmor, iconWStam );
	int iconBarGap = XRES( 4 );	// espaco entre o icone e a barra
	int bx = marginX + iconSlotW + iconBarGap;

	// stamina embaixo (mesma margem inferior da silhueta), armor em cima dela
	int staminaBy = ScreenHeight - bottomMargin - bh;
	int armorBy = staminaBy - bh - barGap;

	// RTN F10 fix: icones agora centralizados na ALTURA REAL de cada barra
	// (SPR_Height do proprio icone), nao numa estimativa fixa de 14 unidades -
	// e ambos ficam encostados na mesma marginX da silhueta (alinhamento
	// pedido pelo usuario), nunca "flutuando" mais pra dentro que ela.
	int iconHArmor = SPR_Height( m_hArmorIcon, 0 );
	int iconHStam = SPR_Height( m_hStaminaIcon, 0 );
	int iconXarmor = marginX;
	int iconXstam = marginX;
	int iconYarmor = armorBy + ( bh - iconHArmor ) / 2;
	int iconYstam = staminaBy + ( bh - iconHStam ) / 2;

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
		SPR_Draw( 0, iconXarmor, iconYarmor, NULL );
	}
	float fArmorFill = (float)iBat / 100.0f;
	RTN_DrawFill( bx, armorBy, bw, bh, m_hArmorEmpty, m_hArmorFull, fArmorFill, false );

	// ---- stamina: barra VERDE + icone raio (mascara HORIZONTAL) ----
	if( m_hStaminaIcon )
	{
		SPR_Set( m_hStaminaIcon, 255, 255, 255 );
		SPR_Draw( 0, iconXstam, iconYstam, NULL );
	}
	float fStaminaFill = flStamina / 100.0f;
	RTN_DrawFill( bx, staminaBy, bw, bh, m_hStaminaEmpty, m_hStaminaFull, fStaminaFill, false );

	return 1;
}
