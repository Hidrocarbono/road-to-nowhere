/*
radio_sentence.cpp - radio message entity (Paranoia 2 sound.cpp:2037)
Copyright (C) 2026 Hermes e Hidrocarboneto

Porta do P2 com 2 ajustes RTN F10:
  - EMIT_SOUND no CHAN_VOICE (canal de voz do radio) em vez do
    EMIT_SOUND_SUIT do P2 (que usa o canal do traje - competiria com os
    efeitos do CHAN_ITEM).
  - .ogg suportado nativamente pelo Xash (snd_ogg_vorbis.c) - sem
    mudanca de codigo.
*/

#include "radio_sentence.h"
#include "user_messages.h"

LINK_ENTITY_TO_CLASS( radio_sentence, CRadio );

void CRadio::Spawn()
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
}

void CRadio::Precache()
{
	char *szSoundFile = (char *)STRING( pev->message );

	if ( !FStringNull( pev->message ) && Q_strlen( szSoundFile ) > 1 )
	{
		if ( szSoundFile[0] != '!' )
			PRECACHE_SOUND( szSoundFile );
	}
}

void CRadio::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pPlayer = NULL;
	if ( pActivator && pActivator->IsPlayer() )
		pPlayer = pActivator;
	else
		pPlayer = CBaseEntity::Instance( g_engfuncs.pfnPEntityOfEntIndex( 1 ));

	if ( !pPlayer )
		return;

	// som do radio (wav ou ogg - o Xash detecta o formato)
	char *szSoundFile = (char *)STRING( pev->message );
	if ( !FStringNull( pev->message ) && Q_strlen( szSoundFile ) > 1 )
	{
		// RTN F10: CHAN_VOICE (voz do radio) - nao compete com os efeitos
		EMIT_SOUND( pPlayer->edict(), CHAN_VOICE, szSoundFile, VOL_NORM, ATTN_NORM );
	}

	// icone do radio no HUD (nome do talker + hold time + cor + alpha)
	char *szTitle = (char *)STRING( pev->netname );
	if ( !FStringNull( pev->netname ) && Q_strlen( szTitle ) > 1 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgRadioIcon, NULL, pPlayer->pev );
			WRITE_STRING( szTitle );
			WRITE_COORD( pev->health );
			WRITE_BYTE( pev->rendercolor.x );
			WRITE_BYTE( pev->rendercolor.y );
			WRITE_BYTE( pev->rendercolor.z );
			WRITE_BYTE( pev->renderamt );
		MESSAGE_END();
	}
}
