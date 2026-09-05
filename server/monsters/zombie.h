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

// UNDONE: Don't flinch every time you get hit

#pragma once

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	ZOMBIE_AE_ATTACK_RIGHT		0x01
#define	ZOMBIE_AE_ATTACK_LEFT		0x02
#define	ZOMBIE_AE_ATTACK_BOTH		0x03

#define ZOMBIE_FLINCH_DELAY			2		// at most one flinch every n secs

// não vaguear à toa por node quando ocioso (opt-out, ver WanderRandomly) - mesmo
// padrão de nome/uso do SF_GRUNT_NO_WANDER, mas bit próprio da classe zombie
#define SF_ZOMBIE_NO_WANDER			4096

class CZombie : public CBaseMonster
{
	DECLARE_CLASS( CZombie, CBaseMonster );
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );

	float m_flNextFlinch;
	float m_flNextWanderTime; // cooldown do WanderRandomly() - espera pós-chegada e retry em caso de falha

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );

	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	// posição de acerto varia um pouco de altura a cada chamada, pra não ser
	// sempre o mesmo ponto exato (porte do Paranoia2_original)
	virtual Vector BodyTarget( const Vector &posSrc ) { return Center() + Vector( 0.0f, 0.0f, RANDOM_FLOAT( 1.0f, 20.0f )); }

	// multiplicador de dano por hitgroup exclusivo do zumbi + decal de
	// headshot + checagem de SF_MONSTER_INVINCIBLE (porte do Paranoia2_original)
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );

	// sem patrulha configurada no mapa e sem rota em andamento: vaguear por
	// node, igual ao CHGrunt (ver server/monsters/hgrunt.cpp)
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	BOOL WanderRandomly( void );

	CUSTOM_SCHEDULES;
};
