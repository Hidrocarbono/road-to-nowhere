//=========================================================
// Cerberus - cão vadio contaminado. Vaga pela Zona, morde de perto
// e salta de média distância (como o headcrab). Base: CZombie
// (wander/alerta em cadeia) + CHeadCrab (leap).
//=========================================================

#pragma once

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"

#define	CERBERUS_AE_BITE		0x01	// evento do QC: attack2 frame 15
#define	CERBERUS_AE_JUMP		0x02	// opcional: se o QC de jump_window ganhar "event 2 <frame>"

// sem evento no jump_window do modelo atual: o salto sai por tempo
#define CERBERUS_JUMP_DELAY		0.25f

#define SF_CERBERUS_NO_WANDER	4096

class CCerberus : public CBaseMonster
{
	DECLARE_CLASS( CCerberus, CBaseMonster );
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void SetActivity( Activity NewActivity );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	void Killed( entvars_t *pevAttacker, int iGib );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	int  IgnoreConditions( void );

	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack2( float flDot, float flDist ) { return FALSE; }

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );

	void LeapTouch( CBaseEntity *pOther );
	void DoLeap( void );

	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	BOOL WanderRandomly( void );

	DECLARE_DATADESC();
	CUSTOM_SCHEDULES;

	float	m_flNextFlinch;
	float	m_flNextWanderTime;
	float	m_flNextLeap;		// cooldown entre saltos
	float	m_flLeapStart;		// quando o salto atual começou (anim)
	BOOL	m_fLeaped;			// já saiu do chão neste salto
	int		m_iJumpSequence;	// índice de "jump_window"

	static const char *pPainSounds[];
	static const char *pAllSounds[];
};
