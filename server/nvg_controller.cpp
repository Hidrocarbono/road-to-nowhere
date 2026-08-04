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
		// RTN F10 v4: NVG fosforo verde com GANHO MULTIPLICATIVO real.
		// Ordem do shader: sat->brightness->levels->contrast->grain->vignette.
		// BUG da v3: green 0.65 MULTIPLICAVA o brilho por 0.65 (escurecia!) e o
		// brightness 0.35 somava luz em tudo (lavava o contraste - tudo verde
		// uniforme, "dificulta mais a visao").
		// v4 (estilo F.E.A.R./MW): green > 1.0 = amplifica a luz EXISTENTE
		// (pixel escuro 0.1 -> 0.18 verde, iluminado 0.5 -> 0.9), brightness baixo
		// so levanta o preto absoluto, contrast 1.0 preserva a faixa dinamica.
		// fade-in time
		WRITE_FLOAT( 0.25f );
		// brightness (floor: so o preto absoluto ganha 0.12 - nao lava o resto)
		WRITE_FLOAT( 0.12f );
		// saturation 0 = full desaturation (monochrome)
		WRITE_FLOAT( 0.0f );
		// contrast 1.0 = preserva contraste (o ganho vem do green, nao da compressao)
		WRITE_FLOAT( 1.0f );
		// red level ZERO (verde puro, sem vermelho)
		WRITE_FLOAT( 0.0f );
		// green level 1.8 = GANHO real: multiplica a luminancia em 1.8x (amplifica escuros)
		WRITE_FLOAT( 1.8f );
		// blue level ZERO (verde puro, sem azul)
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
