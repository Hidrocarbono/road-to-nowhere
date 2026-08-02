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
		// RTN F8 v2: NVG de fosforo verde - calibracao Tarkov/Paranoia:
		// brightness alto = amplifica a luz (ve no escuro, detalhes estouram)
		// fade-in time
		WRITE_FLOAT( 0.25f );
		// brightness (IR gain - AMPLIFICA luz; 0.3 soma luz visivel no escuro)
		WRITE_FLOAT( 0.30f );
		// saturation 0 = full desaturation (monochrome)
		WRITE_FLOAT( 0.0f );
		// contrast (crush blacks + estoura highlights)
		WRITE_FLOAT( 1.65f );
		// red level (quase zero -> nada de vermelho)
		WRITE_FLOAT( 0.05f );
		// green level (dominante - fosforo verde)
		WRITE_FLOAT( 1.35f );
		// blue level (quase zero -> nada de azul)
		WRITE_FLOAT( 0.05f );
		// vignette (escurece TODAS as laterais - efeito de lente)
		WRITE_FLOAT( 2.2f );
		// film grain (chuvisco SUBTIL - 0.05 = 80% mais transparente que 0.25;
		// o contraste 1.65 do NVG amplifica o noise, entao tem que ser bem baixo)
		WRITE_FLOAT( 0.05f );
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
