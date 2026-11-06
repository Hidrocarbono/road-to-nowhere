//=========================================================
// scripted_dialog - ver scripted_dialog.h
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "dialogscript.h"
#include "scripted_dialog.h"

LINK_ENTITY_TO_CLASS( scripted_dialog, CScriptedDialog );

BEGIN_DATADESC( CScriptedDialog )
	DEFINE_FIELD( m_iszDialogTarget, FIELD_STRING ),
	DEFINE_FIELD( m_iszSpeaker, FIELD_STRING ),
	DEFINE_FIELD( m_iszTargetExit, FIELD_STRING ),
	DEFINE_FIELD( m_bFired, FIELD_BOOLEAN ),
END_DATADESC()

void CScriptedDialog :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "dialog_target" ))
	{
		m_iszDialogTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "speaker" ))
	{
		m_iszSpeaker = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "target_exit" ))
	{
		m_iszTargetExit = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CScriptedDialog :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( FBitSet( pev->spawnflags, SF_SCRIPTDIALOG_ONCE ) && m_bFired )
		return;

	if( FStringNull( m_iszDialogTarget ))
	{
		ALERT( at_console, "scripted_dialog '%s': sem dialog_target\n", STRING( pev->targetname ));
		return;
	}

	// singleplayer only (como o resto do sistema de dialogo): jogador do slot 1.
	CBaseEntity *pEnt = UTIL_PlayerByIndex( 1 );
	if( !pEnt || !pEnt->IsPlayer() )
		return;
	CBasePlayer *pPlayer = (CBasePlayer *)pEnt;

	CBaseEntity *pSpeaker = NULL;
	if( !FStringNull( m_iszSpeaker ))
	{
		pSpeaker = UTIL_FindEntityByTargetname( NULL, STRING( m_iszSpeaker ));
		if( !pSpeaker )
			ALERT( at_console, "scripted_dialog '%s': speaker '%s' nao encontrado, seguindo sem falante\n",
				STRING( pev->targetname ), STRING( m_iszSpeaker ));
	}

	bool bKeepControl = !FBitSet( pev->spawnflags, SF_SCRIPTDIALOG_RELEASE_CONTROL );
	int iResult = pPlayer->Dialog_Start( pSpeaker, STRING( m_iszDialogTarget ), this, bKeepControl );

	if( iResult == DLG_START_OK )
	{
		m_bFired = TRUE;
	}
	else if( iResult == DLG_START_SEALED )
	{
		// arvore ja concluida antes: trata como "pulado" pra cutscene nao travar
		m_bFired = TRUE;
		if( !FStringNull( m_iszTargetExit ))
			UTIL_FireTargets( m_iszTargetExit, pPlayer, this, USE_TOGGLE, 0 );
	}
}

// chamado por Dialog_NotifyEnded (dialogsession.cpp) com a sessao ja limpa
void CScriptedDialog :: Ended( CBasePlayer *pPlayer, int iReason )
{
	if( iReason == DLG_END_NATURAL )
		SUB_UseTargets( pPlayer, USE_TOGGLE, 0 );
	else if( iReason == DLG_END_EXIT && !FStringNull( m_iszTargetExit ))
		UTIL_FireTargets( m_iszTargetExit, pPlayer, this, USE_TOGGLE, 0 );
	// DLG_END_CANCEL (falante morreu/sumiu): nao dispara nada
}

void Dialog_NotifyEnded( CBaseEntity *pOwner, CBasePlayer *pPlayer, int iReason )
{
	if( pOwner && FClassnameIs( pOwner->pev, "scripted_dialog" ))
		((CScriptedDialog *)pOwner)->Ended( pPlayer, iReason );
}
