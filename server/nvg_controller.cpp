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
		// fade-in time
		WRITE_FLOAT( 0.25f );
		// brightness (IR gain)
		WRITE_FLOAT( 0.12f );
		// saturation 0 = full desaturation (monochrome green)
		WRITE_FLOAT( 0.0f );
		// contrast (crush blacks, boost highlights)
		WRITE_FLOAT( 1.45f );
		// red level (kill red -> green palette)
		WRITE_FLOAT( 0.15f );
		// green level (dominant)
		WRITE_FLOAT( 1.15f );
		// blue level (kill blue)
		WRITE_FLOAT( 0.2f );
		// vignette (lens edge darkening)
		WRITE_FLOAT( 1.0f );
		// film grain (analog sensor noise, animated in shader)
		WRITE_FLOAT( 0.8f );
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
