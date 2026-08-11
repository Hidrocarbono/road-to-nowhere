#include "nvg_controller.h"
#include "player.h"
#include "user_messages.h"

// RTN Fase 8: controlador NVG global (tecla N). Usa o sistema de post-fx nativo.

// Meyers singleton: instancia estatica local (construtor privado)
CNVGController &CNVGController::GetInstance()
{
	static CNVGController instance;
	return instance;
}

void NVG_Toggle_f( void )
{
	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
	if( pPlayer )
		CNVGController::GetInstance().Toggle( pPlayer );
}

void NVG_Init()
{
	g_engfuncs.pfnAddServerCommand( "nvg_toggle", NVG_Toggle_f );
}

void CNVGController::Toggle( CBaseEntity *pPlayer )
{
	if( m_bActive )
	{
		ApplyState( pPlayer, false );
	}
	else if( m_flBattery > 0.0f )
	{
		ApplyState( pPlayer, true );
	}
}

void CNVGController::Update()
{
	if( !m_bActive )
		return;

	m_flBattery -= gpGlobals->frametime;
	if( m_flBattery <= 0.0f )
	{
		m_flBattery = 0.0f;
		CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
		if( pPlayer )
			ApplyState( pPlayer, false );  // bateria acabou - desliga
	}
}

void CNVGController::ApplyState( CBaseEntity *pPlayer, bool active )
{
	m_bActive = active;

	MESSAGE_BEGIN( MSG_ONE, gmsgPostFxSettings, NULL, pPlayer->edict() );

	if( active )
	{
		// RTN F10 v5 (AGGRESSIVO, aprovado): NVG fosforo verde com ganho REAL no escuro.
		// v4 tinha brightness 0.12 + green 1.8 -> no escuro absoluto (0.0) o resultado
		// era 0.12*1.8 = 0.216, e o SRGB final escurecia -> parecia que "nao fazia nada".
		// v5: brightness 0.35 (levanta o preto absoluto p/ 0.35) + green 2.5 (fosforo
		// intenso) -> pixel escuro 0.1 vira (0.1+0.35)*2.5 = 1.12 (estoura p/ verde
		// claro), pixel 0.5 vira 2.12 (verde quase branco). Contraste 1.0 preserva.
		// Ordem do shader: sat->brightness->levels->contrast->grain->vignette.
		// fade-in time
		WRITE_FLOAT( 0.25f );
		// brightness (IR gain: 0.35 levanta TUDO - inclusive o preto absoluto)
		WRITE_FLOAT( 0.35f );
		// saturation 0 = full desaturation (monochrome)
		WRITE_FLOAT( 0.0f );
		// contrast 1.0 = preserva faixa dinamica
		WRITE_FLOAT( 1.0f );
		// red level ZERO (verde puro)
		WRITE_FLOAT( 0.0f );
		// green level 2.5 = GANHO multiplicativo alto (fosforo P2 estilo vivido)
		WRITE_FLOAT( 2.5f );
		// blue level ZERO (verde puro)
		WRITE_FLOAT( 0.0f );
		// vignette (escurece TODAS as laterais - efeito de lente)
		WRITE_FLOAT( 2.2f );
		// film grain 0.25 (chuvisco analogico pedido pelo user)
		WRITE_FLOAT( 0.25f );
		// color accent scale (0 = no accent)
		WRITE_FLOAT( 0.0f );
		// accent color (unused, white)
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
	}
	else
	{
		// restore defaults (no effect)
		WRITE_FLOAT( 0.25f );
		WRITE_FLOAT( 0.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 0.0f );
		WRITE_FLOAT( 0.0f );
		WRITE_FLOAT( 0.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
	}

	MESSAGE_END();
}
