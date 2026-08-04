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
		// RTN F10 v3: NVG fosforo verde com GANHO DE LUZ no escuro.
		// Ordem do shader: sat->brightness->levels->contrast->grain->vignette.
		// O brightness soma luz AINDA EM CINZA (depois da saturation), e o
		// colorlevels tinge de verde PURO por ultimo - sem reintroduzir R/B.
		// fade-in time
		WRITE_FLOAT( 0.25f );
		// brightness (IR gain: 0.35 ilumina escuros em cinza - ve no escuro)
		WRITE_FLOAT( 0.35f );
		// saturation 0 = full desaturation (monochrome)
		WRITE_FLOAT( 0.0f );
		// contrast 0.75 comprime claros (o gain alto estouraria com 1.65)
		WRITE_FLOAT( 0.75f );
		// red level ZERO (verde puro, sem vermelho)
		WRITE_FLOAT( 0.0f );
		// green level 0.65 (fosforo verde intenso e escuro - #006400 estilo)
		WRITE_FLOAT( 0.65f );
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
