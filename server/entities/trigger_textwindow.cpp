/*
trigger_textwindow.cpp - entity that shows a document window (Paranoia 2 style)
Copyright (C) 2026 Hermes e Hidrocarboneto (RTN)

RTN F10: porta do Paranoia 2 (triggers.cpp: CTriggerTextWindow).
O Use envia a mensagem "TextWindow" (MSG_ONE) com o NOME do arquivo
(pev->message, ex: "p_army1b"). O client abre texts/<nome>.txt e exibe
a janela do documento (imagem TGA + botao fechar).
*/

#include "trigger_textwindow.h"
#include "user_messages.h"

LINK_ENTITY_TO_CLASS( trigger_textwindow, CTriggerTextWindow );

void CTriggerTextWindow :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !pActivator || !pActivator->IsPlayer( ))
		return;

	if( !pev->message )
		return;

	// RTN F10: envia o NOME do arquivo (texts/<nome>.txt) ao client
	MESSAGE_BEGIN( MSG_ONE, gmsgTextWindow, NULL, pActivator->pev );
		WRITE_STRING( STRING( pev->message ));
	MESSAGE_END();
}
