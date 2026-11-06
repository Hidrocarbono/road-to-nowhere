//=========================================================
// scripted_dialog - abre uma arvore de dialogo (game_dir/scripts/dialogs/
// dialog_*.txt) por TRIGGER, sem precisar de um monstro com "dialog_target".
// Pensado pra cutscenes em que o "personagem" e um env_model.
// Ver game_dir/devkit/GUIA_DIALOGOS.md.
//=========================================================

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define SF_SCRIPTDIALOG_RELEASE_CONTROL	1	// DESLIGADO (default) = nao mexe no controle do jogador
#define SF_SCRIPTDIALOG_ONCE			2	// dispara so uma vez

class CScriptedDialog : public CPointEntity
{
	DECLARE_CLASS( CScriptedDialog, CPointEntity );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Ended( CBasePlayer *pPlayer, int iReason );

	DECLARE_DATADESC();

	string_t	m_iszDialogTarget;	// no inicial da arvore
	string_t	m_iszSpeaker;		// targetname do falante (opcional: env_model ou monstro)
	string_t	m_iszTargetExit;	// disparado ao sair pelo exit_option (ou arvore ja selada)
	BOOL		m_bFired;
};
