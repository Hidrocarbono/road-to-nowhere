/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// Zombie
//=========================================================

#include	"zombie.h"
#include	"nodes.h"
#include	"soundent.h"
#include	"weapons.h" // SpawnBlood, AddMultiDamage

LINK_ENTITY_TO_CLASS( monster_zombie, CZombie );

const char *CZombie::pAttackHitSounds[] =
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CZombie::pAttackMissSounds[] =
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CZombie::pAttackSounds[] =
{
	"zombie/zo_attack1.wav",
	"zombie/zo_attack2.wav",
};

const char *CZombie::pIdleSounds[] =
{
	"zombie/zo_idle1.wav",
	"zombie/zo_idle2.wav",
	"zombie/zo_idle3.wav",
	"zombie/zo_idle4.wav",
};

const char *CZombie::pAlertSounds[] =
{
	"zombie/zo_alert10.wav",
	"zombie/zo_alert20.wav",
	"zombie/zo_alert30.wav",
};

const char *CZombie::pPainSounds[] =
{
	"zombie/zo_pain1.wav",
	"zombie/zo_pain2.wav",
};

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CZombie :: Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CZombie :: SetYawSpeed ( void )
{
	int ys;

	ys = 120;

#if 0
	switch ( m_Activity )
	{
	}
#endif

	pev->yaw_speed = ys;
}

int CZombie :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// zumbi não respira - imune a gás nervoso (porte do Paranoia2_original)
	if ( bitsDamageType & DMG_NERVEGAS )
		return 0;

	// Take 30% damage from bullets
	if ( bitsDamageType == DMG_BULLET )
	{
		Vector vecDir = GetAbsOrigin() - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();
		float flForce = DamageForce( flDamage );
		SetAbsVelocity( GetAbsVelocity() + vecDir * flForce );
		flDamage *= 0.3;
	}

	// HACK HACK -- until we fix this.
	if ( IsAlive() )
		PainSound();
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// TraceAttack - overridden pro zumbi ter multiplicador de dano por
// hitgroup PRÓPRIO (não o genérico mon* compartilhado com todo
// monstro), decal de "miolos" na cabeça e respeito ao
// SF_MONSTER_INVINCIBLE (porte do Paranoia2_original).
//=========================================================
void CZombie :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( !pev->takedamage )
		return;

	if ( FBitSet( pev->spawnflags, SF_MONSTER_INVINCIBLE ) )
	{
		CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );
		if ( pAttacker && pAttacker->IsPlayer() )
			return;

		if ( pevAttacker->owner )
		{
			CBaseEntity *pOwner = CBaseEntity::Instance( pevAttacker->owner );
			if ( pOwner && pOwner->IsPlayer() )
				return;
		}
	}

	m_LastHitGroup = ptr->iHitgroup;
	TraceBleed( flDamage, vecDir, ptr, bitsDamageType );

	// RTN DEBUG (temporário): se o dano continuar igual em qualquer parte do
	// corpo, é pra ver aqui se o hitgroup reportado pelo modelo é sempre 0
	// (HITGROUP_GENERIC) - nesse caso o problema é o .mdl (hitboxes do
	// zombie.mdl sem grupos distintos), não este código. Remover depois de
	// confirmado.
	ALERT( at_aiconsole, "zombie TraceAttack: hitgroup=%d dmg_antes=%.1f\n", ptr->iHitgroup, flDamage );

	switch ( ptr->iHitgroup )
	{
	case HITGROUP_GENERIC:
		break;
	case HITGROUP_HEAD:
		{
			// respinga na parede atrás da cabeça, não no próprio zumbi
			// (decal só cola em geometria do mapa, não em modelo de monstro)
			TraceResult btr;
			UTIL_TraceLine( ptr->vecEndPos, ptr->vecEndPos + vecDir * 172, ignore_monsters, ENT(pev), &btr );
			UTIL_TraceCustomDecal( &btr, "brains", RANDOM_FLOAT( 0.0f, 360.0f ) );
			SpawnBlood( ptr->vecEndPos, BloodColor(), flDamage * 4 );
			flDamage *= gSkillData.zomHead;
		}
		break;
	case HITGROUP_CHEST:
		flDamage *= gSkillData.zomChest;
		break;
	case HITGROUP_STOMACH:
		flDamage *= gSkillData.zomStomach;
		break;
	case HITGROUP_LEFTARM:
	case HITGROUP_RIGHTARM:
		flDamage *= gSkillData.zomArm;
		break;
	case HITGROUP_LEFTLEG:
	case HITGROUP_RIGHTLEG:
		flDamage *= gSkillData.zomLeg;
		break;
	default:
		break;
	}

	SpawnBlood( ptr->vecEndPos, BloodColor(), flDamage * 2 );
	// RTN F10: sangue no corpo do monstro, não só na parede
	UTIL_BloodStudioDecalTrace( ptr, BloodColor() );
	AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
}

void CZombie :: PainSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

//=========================================================
// AlertSound - toca o grito de "te avistei" e avisa os zumbis
// próximos por som de combate, pra reagirem em cadeia (nenhum dos
// dois lados, RTN nem P2, tinha isso - ideia própria pra dar
// sensação de horda em vez de zumbi isolado).
//=========================================================
void CZombie :: AlertSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAlertSounds[ RANDOM_LONG(0,ARRAYSIZE(pAlertSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );

	// bits_SOUND_COMBAT já está no ISoundMask padrão (CBaseMonster), então
	// qualquer outro zumbi/monstro por perto já escuta isso sem mudança
	// nenhuma de mask - só precisa alguém emitindo o som.
	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, GetAbsOrigin(), 384, 0.3 );
}

void CZombie :: IdleSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	// Play a random idle sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pIdleSounds[ RANDOM_LONG(0,ARRAYSIZE(pIdleSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}

void CZombie :: AttackSound( void )
{
	// Play a random attack sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CZombie :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case ZOMBIE_AE_ATTACK_RIGHT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash right!\n" );
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.zombieDmgOneSlash, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() - gpGlobals->v_right * 100 );
				}
				// Play a random attack hit sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
			else // Play a random attack miss sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case ZOMBIE_AE_ATTACK_LEFT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash left!\n" );
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.zombieDmgOneSlash, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = 18;
					pHurt->pev->punchangle.x = 5;
					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + gpGlobals->v_right * 100 );
				}
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
			else
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case ZOMBIE_AE_ATTACK_BOTH:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.zombieDmgBothSlash, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.x = 5;
					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + gpGlobals->v_forward * -100 );
				}
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
			else
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CZombie :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/zombie.mdl");
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	if (!pev->health) pev->health	= gSkillData.zombieHealth;
	pev->view_ofs		= VEC_VIEW;// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CZombie :: Precache()
{
	int i;

	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/zombie.mdl");

	for ( i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackHitSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackMissSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pIdleSounds ); i++ )
		PRECACHE_SOUND((char *)pIdleSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PRECACHE_SOUND((char *)pAlertSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PRECACHE_SOUND((char *)pPainSounds[i]);
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

//=========================================================
// busca por som mais persistente que o padrão: em vez de esperar 10s
// parado antes de voltar, espera bem mais - zumbi não desiste fácil de
// um barulho que ouviu (ideia própria, não existe nem no RTN nem no P2)
//=========================================================
#define ZOMBIE_INVESTIGATE_WAIT	25

Task_t tlZombieInvestigateSound[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_STORE_LASTPOSITION,		(float)0					},
	{ TASK_GET_PATH_TO_BESTSOUND,	(float)0					},
	{ TASK_FACE_IDEAL,				(float)0					},
	{ TASK_WALK_PATH,				(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_IDLE				},
	{ TASK_WAIT,					(float)ZOMBIE_INVESTIGATE_WAIT	},
	{ TASK_GET_PATH_TO_LASTPOSITION,(float)0					},
	{ TASK_WALK_PATH,				(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_CLEAR_LASTPOSITION,		(float)0					},
};

Schedule_t slZombieInvestigateSound[] =
{
	{
		tlZombieInvestigateSound,
		ARRAYSIZE ( tlZombieInvestigateSound ),
		bits_COND_NEW_ENEMY			|
		bits_COND_SEE_FEAR			|
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_HEAR_SOUND,

		bits_SOUND_DANGER,
		"ZombieInvestigateSound"
	},
};

DEFINE_CUSTOM_SCHEDULES( CZombie )
{
	slZombieInvestigateSound,
};

IMPLEMENT_CUSTOM_SCHEDULES( CZombie, CBaseMonster );

Schedule_t *CZombie :: GetScheduleOfType ( int Type )
{
	if ( Type == SCHED_INVESTIGATE_SOUND )
		return &slZombieInvestigateSound[ 0 ];

	return CBaseMonster :: GetScheduleOfType( Type );
}

//=========================================================
// WanderRandomly - mesma receita do CHGrunt (server/monsters/hgrunt.cpp):
// sem patrulha configurada no mapa, escolhe um node alcançável qualquer
// por perto e manda o zumbi andar até lá, em vez de ficar parado o
// tempo todo em que não tem inimigo.
//=========================================================
BOOL CZombie :: WanderRandomly ( void )
{
	if ( gpGlobals->time < m_flNextWanderTime )
		return FALSE;

	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet )
	{
		m_flNextWanderTime = gpGlobals->time + 5;
		return FALSE;
	}

	int iMyNode = WorldGraph.FindNearestNode( GetAbsOrigin(), this );
	if ( iMyNode == NO_NODE || WorldGraph.m_cNodes <= 1 )
		return FALSE;

	int iMyHullIndex = WorldGraph.HullIndex( this );

	for ( int tries = 0; tries < 5; tries++ )
	{
		int iCandidate = RANDOM_LONG( 0, WorldGraph.m_cNodes - 1 );
		if ( iCandidate == iMyNode )
			continue;

		float flPathLength = WorldGraph.PathLength( iMyNode, iCandidate, iMyHullIndex, m_afCapability );
		if ( flPathLength <= 0 )
			continue;

		if ( MoveToLocation( ACT_WALK, 0, WorldGraph.Node( iCandidate ).m_vecOrigin ) )
		{
			m_flNextWanderTime = gpGlobals->time + RANDOM_FLOAT( 8, 20 );
			return TRUE;
		}
	}

	m_flNextWanderTime = gpGlobals->time + 3;
	return FALSE;
}

Schedule_t *CZombie :: GetSchedule( void )
{
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_ALERT:
	case MONSTERSTATE_IDLE:
		{
			// mesmos gates do CHGrunt: sem patrulha, sem rota em andamento,
			// sem som/dano recente pra reagir primeiro.
			if ( !FBitSet( pev->spawnflags, SF_ZOMBIE_NO_WANDER ) &&
				 FStringNull( pev->target ) &&
				 FRouteClear() &&
				 !HasConditions( bits_COND_HEAR_SOUND | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) &&
				 WanderRandomly() )
			{
				return GetScheduleOfType( SCHED_IDLE_WALK );
			}
		}
		break;
	}

	return CBaseMonster :: GetSchedule();
}

int CZombie::IgnoreConditions ( void )
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if ((m_Activity == ACT_MELEE_ATTACK1) || (m_Activity == ACT_MELEE_ATTACK1))
	{
#if 0
		if (pev->health < 20)
			iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
		else
#endif
		if (m_flNextFlinch >= gpGlobals->time)
			iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
	}

	if ((m_Activity == ACT_SMALL_FLINCH) || (m_Activity == ACT_BIG_FLINCH))
	{
		if (m_flNextFlinch < gpGlobals->time)
			m_flNextFlinch = gpGlobals->time + ZOMBIE_FLINCH_DELAY;
	}

	return iIgnore;

}
