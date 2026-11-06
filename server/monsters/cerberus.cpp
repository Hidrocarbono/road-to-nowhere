//=========================================================
// Cerberus - cão vadio contaminado
//=========================================================

#include	"cerberus.h"
#include	"nodes.h"
#include	"soundent.h"
#include	"weapons.h"
#include	"game.h"
#include	"animation.h"

LINK_ENTITY_TO_CLASS( monster_cerberus, CCerberus );

BEGIN_DATADESC( CCerberus )
	DEFINE_FIELD( m_flNextFlinch, FIELD_TIME ),
	DEFINE_FIELD( m_flNextWanderTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextLeap, FIELD_TIME ),
	DEFINE_FIELD( m_flLeapStart, FIELD_TIME ),
	DEFINE_FIELD( m_fLeaped, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iJumpSequence, FIELD_INTEGER ),
	DEFINE_FUNCTION( LeapTouch ),
END_DATADESC()

const char *CCerberus::pPainSounds[] =
{
	"cerberus/cer_pain.wav",
	"cerberus/cer_pain2.wav",
};

// tudo que o código OU o QC do modelo (eventos 1004) toca
const char *CCerberus::pAllSounds[] =
{
	"cerberus/cer_growl.wav",
	"cerberus/cer_run.wav",
	"cerberus/cer_alert.wav",
	"cerberus/cer_die.wav",
	"cerberus/cer_headburst.wav",
	"cerberus/cer_bite.wav",
	"cerberus/cer_bark.wav",
	"cerberus/cer_howl.wav",
	"cerberus/cer_jump.wav",
	"cerberus/cer_pain.wav",
	"cerberus/cer_pain2.wav",
};

//=========================================================
// Schedule: salto (ACT_RANGE_ATTACK1 -> jump_window, ver SetActivity)
//=========================================================
Task_t tlCerberusLeap[] =
{
	{ TASK_STOP_MOVING,		(float)0		},
	{ TASK_FACE_IDEAL,		(float)0		},
	{ TASK_RANGE_ATTACK1,	(float)0		},
	{ TASK_SET_ACTIVITY,	(float)ACT_IDLE	},
	{ TASK_WAIT_RANDOM,		(float)0.4		},
};

Schedule_t slCerberusLeap[] =
{
	{
		tlCerberusLeap,
		ARRAYSIZE( tlCerberusLeap ),
		bits_COND_ENEMY_OCCLUDED,
		0,
		"CerberusLeap"
	},
};

DEFINE_CUSTOM_SCHEDULES( CCerberus )
{
	slCerberusLeap,
};

IMPLEMENT_CUSTOM_SCHEDULES( CCerberus, CBaseMonster );

int CCerberus :: Classify( void )
{
	return m_iClass ? m_iClass : CLASS_ALIEN_MONSTER;
}

void CCerberus :: SetYawSpeed( void )
{
	pev->yaw_speed = 150;
}

//=========================================================
// SetActivity - o modelo só tem sequência ACT_RUN (a "walk" do QC é
// ACT_RUN) e o jump_window não tem activity. Então:
//  - ACT_WALK reusa a sequência de corrida (vagar = trote);
//  - ACT_RANGE_ATTACK1 toca "jump_window" por nome.
// Sem isso o monstro cai na sequência 0 (idle) e desliza.
//=========================================================
void CCerberus :: SetActivity( Activity NewActivity )
{
	if ( NewActivity == ACT_WALK )
	{
		CBaseMonster::SetActivity( ACT_RUN );
		m_Activity = ACT_WALK;
		m_IdealActivity = ACT_WALK;
		return;
	}

	if ( NewActivity == ACT_RANGE_ATTACK1 && m_iJumpSequence > ACTIVITY_NOT_AVAILABLE )
	{
		pev->sequence = m_iJumpSequence;
		pev->frame = 0;
		ResetSequenceInfo();
		SetYawSpeed();
		m_Activity = ACT_RANGE_ATTACK1;
		m_IdealActivity = ACT_RANGE_ATTACK1;
		return;
	}

	CBaseMonster::SetActivity( NewActivity );
}

//=========================================================
// Salto - mesma balística do headcrab, com teto de velocidade
//=========================================================
void CCerberus :: DoLeap( void )
{
	if ( m_fLeaped )
		return;
	m_fLeaped = TRUE;

	ClearBits( pev->flags, FL_ONGROUND );
	UTIL_SetOrigin( this, GetAbsOrigin() + Vector( 0, 0, 1 ) );
	UTIL_MakeVectors( GetAbsAngles() );

	Vector vecJumpDir;
	if ( m_hEnemy != NULL )
	{
		float gravity = g_psv_gravity->value;
		if ( gravity <= 1 )
			gravity = 1;

		float height = ( m_hEnemy->EyePosition().z - GetAbsOrigin().z );
		if ( height < 24 )
			height = 24;
		float speed = sqrt( 2 * gravity * height );
		float time = speed / gravity;

		vecJumpDir = ( m_hEnemy->EyePosition() - GetAbsOrigin() ) * ( 1.0 / time );
		vecJumpDir.z = speed;

		float distance = vecJumpDir.Length();
		if ( distance > 700 )
			vecJumpDir = vecJumpDir * ( 700.0 / distance );
	}
	else
	{
		vecJumpDir = Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_up.z ) * 350;
	}

	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "cerberus/cer_jump.wav", 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );

	SetAbsVelocity( vecJumpDir );
	m_flNextLeap = gpGlobals->time + RANDOM_FLOAT( 3, 5 );
}

void CCerberus :: LeapTouch( CBaseEntity *pOther )
{
	if ( !pOther->pev->takedamage )
		return;

	if ( pOther->Classify() == Classify() )
		return;

	// só machuca no ar
	if ( !FBitSet( pev->flags, FL_ONGROUND ) )
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "cerberus/cer_bite.wav", 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
		pOther->TakeDamage( pev, pev, gSkillData.cerberusDmgLeap, DMG_SLASH );
	}

	SetTouch( NULL );
}

BOOL CCerberus :: CheckRangeAttack1( float flDot, float flDist )
{
	return FBitSet( pev->flags, FL_ONGROUND ) &&
		   gpGlobals->time >= m_flNextLeap &&
		   flDist > 96 && flDist <= 320 && flDot >= 0.65;
}

void CCerberus :: StartTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		m_iTaskStatus = TASKSTATUS_RUNNING;
		m_fLeaped = FALSE;
		m_flLeapStart = gpGlobals->time;
		m_IdealActivity = ACT_RANGE_ATTACK1;
		SetTouch( &CCerberus::LeapTouch );
		break;
	default:
		CBaseMonster::StartTask( pTask );
		break;
	}
}

void CCerberus :: RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		// sem evento no jump_window: decola por tempo (event 2 antecipa, se existir)
		if ( !m_fLeaped && gpGlobals->time - m_flLeapStart >= CERBERUS_JUMP_DELAY )
			DoLeap();

		// termina quando pousa (ou a animação acaba sem nunca ter saltado)
		if ( ( m_fLeaped && FBitSet( pev->flags, FL_ONGROUND ) && gpGlobals->time - m_flLeapStart > CERBERUS_JUMP_DELAY + 0.2f )
			 || ( m_fSequenceFinished && !m_fLeaped ) )
		{
			TaskComplete();
			SetTouch( NULL );
			m_IdealActivity = ACT_IDLE;
		}
		break;
	default:
		CBaseMonster::RunTask( pTask );
		break;
	}
}

void CCerberus :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case CERBERUS_AE_BITE:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 64, gSkillData.cerberusDmgBite, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ) )
				{
					pHurt->pev->punchangle.x = 5;
					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() + gpGlobals->v_forward * -60 );
				}
				EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "cerberus/cer_bite.wav", 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			}
		}
		break;

	case CERBERUS_AE_JUMP:
		DoLeap();
		break;

	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

int CCerberus :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( IsAlive() )
		PainSound();
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

// tiro na cabeça que mata: troca pro bodygroup "headless" do modelo
void CCerberus :: Killed( entvars_t *pevAttacker, int iGib )
{
	if ( m_LastHitGroup == HITGROUP_HEAD )
		pev->body = 1;
	SetTouch( NULL );
	CBaseMonster::Killed( pevAttacker, iGib );
}

void CCerberus :: PainSound( void )
{
	if ( RANDOM_LONG( 0, 5 ) < 2 )
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, pPainSounds[ RANDOM_LONG( 0, ARRAYSIZE( pPainSounds ) - 1 ) ], 1.0, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 9 ) );
}

// late e avisa outros cães/monstros por perto (matilha)
void CCerberus :: AlertSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "cerberus/cer_bark.wav", 1.0, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 9 ) );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, GetAbsOrigin(), 512, 0.3 );
}

void CCerberus :: IdleSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "cerberus/cer_howl.wav", 1.0, ATTN_IDLE, 0, 100 + RANDOM_LONG( -5, 5 ) );
}

void CCerberus :: Spawn( void )
{
	Precache();

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	else
		SET_MODEL( ENT( pev ), "models/cerberus.mdl" );
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 40 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	if ( !pev->health ) pev->health = gSkillData.cerberusHealth;
	pev->view_ofs		= Vector( 0, 0, 28 );
	m_flFieldOfView		= 0.3;	// cão: campo de visão largo
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;
	m_flDistLook		= 800.0f;
	m_flNextLeap		= 0;
	m_iJumpSequence		= LookupSequence( "jump_window" );

	MonsterInit();
}

void CCerberus :: Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	else
		PRECACHE_MODEL( "models/cerberus.mdl" );

	for ( int i = 0; i < ARRAYSIZE( pAllSounds ); i++ )
		PRECACHE_SOUND( (char *)pAllSounds[i] );
}

//=========================================================
// Vagar pela Zona - mesma receita do CZombie/CHGrunt
//=========================================================
BOOL CCerberus :: WanderRandomly( void )
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
			m_flNextWanderTime = gpGlobals->time + RANDOM_FLOAT( 6, 16 );
			return TRUE;
		}
	}

	m_flNextWanderTime = gpGlobals->time + 3;
	return FALSE;
}

Schedule_t *CCerberus :: GetSchedule( void )
{
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_ALERT:
	case MONSTERSTATE_IDLE:
		if ( !FBitSet( pev->spawnflags, SF_CERBERUS_NO_WANDER ) &&
			 FStringNull( pev->target ) &&
			 FRouteClear() &&
			 !HasConditions( bits_COND_HEAR_SOUND | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) &&
			 WanderRandomly() )
		{
			return GetScheduleOfType( SCHED_IDLE_WALK );
		}
		break;
	}

	return CBaseMonster::GetSchedule();
}

Schedule_t *CCerberus :: GetScheduleOfType( int Type )
{
	if ( Type == SCHED_RANGE_ATTACK1 )
		return &slCerberusLeap[0];

	return CBaseMonster::GetScheduleOfType( Type );
}

int CCerberus :: IgnoreConditions( void )
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if ( m_Activity == ACT_MELEE_ATTACK1 && m_flNextFlinch >= gpGlobals->time )
		iIgnore |= ( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE );

	if ( m_Activity == ACT_SMALL_FLINCH && m_flNextFlinch < gpGlobals->time )
		m_flNextFlinch = gpGlobals->time + 2;

	return iIgnore;
}
