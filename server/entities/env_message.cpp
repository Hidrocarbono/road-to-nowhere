/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "env_message.h"
#include "stringlib.h"  // Q_strncat/Q_strstr (RTN F10)

LINK_ENTITY_TO_CLASS( env_message, CMessage );

void CMessage::Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	switch( pev->impulse )
	{
	case 1: // Medium radius
		pev->speed = ATTN_STATIC;
		break;
	
	case 2:	// Large radius
		pev->speed = ATTN_NORM;
		break;

	case 3:	//EVERYWHERE
		pev->speed = ATTN_NONE;
		break;
	
	default:
	case 0: // Small radius
		pev->speed = ATTN_IDLE;
		break;
	}
	pev->impulse = 0;

	// No volume, use normal
	if ( pev->scale <= 0 )
		pev->scale = 1.0;
}

void CMessage::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
}

void CMessage::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "messagesound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messagevolume"))
	{
		pev->scale = atof(pkvd->szValue) * 0.1;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messageattenuation"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

// RTN F10: substitui %player_name% pelo nome do jogador (se presente)
// Usa Q_snprintf (3 args - Q_strncat nao tem limite de copia!)
static void RTN_SubstitutePlayerName( const char *src, char *dst, int dstSize, CBaseEntity *pPlayer )
{
	const char *pName = "%player_name%";
	const char *pFound = NULL;
	const char *pStart = src;
	int written = 0;

	const char *playerName = "Jogador";
	if( pPlayer && pPlayer->IsNetClient() )
	{
		playerName = STRING( pPlayer->pev->netname );
		if( !playerName || !playerName[0] )
			playerName = "Jogador";
	}

	dst[0] = 0;

	while( ( pFound = Q_strstr( pStart, pName ) ) != NULL )
	{
		// copia o trecho antes do %player_name%
		int preLen = pFound - pStart;
		if( preLen > 0 && written < dstSize - 1 )
			written += Q_snprintf( dst + written, dstSize - written, "%.*s", preLen, pStart );
		// insere o nome do jogador
		if( written < dstSize - 1 )
			written += Q_snprintf( dst + written, dstSize - written, "%s", playerName );
		pStart = pFound + Q_strlen( pName );
	}
	// copia o restante
	if( written < dstSize - 1 )
		Q_snprintf( dst + written, dstSize - written, "%s", pStart );
}

void CMessage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pPlayer = NULL;

	if ( pev->spawnflags & SF_MESSAGE_ALL )
	{
		// envia pra todos os jogadores (com nome de cada um)
		for( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pEnt = CBaseEntity::Instance( INDEXENT( i ) );
			if( pEnt && pEnt->IsPlayer() && pEnt->IsNetClient() )
			{
				char szBuf[512];
				szBuf[0] = 0;
				RTN_SubstitutePlayerName( STRING(pev->message), szBuf, sizeof( szBuf ), pEnt );
				UTIL_ShowMessage( szBuf, pEnt );
			}
		}
	}
	else
	{
		if ( pActivator && pActivator->IsPlayer() )
			pPlayer = pActivator;
		else
		{
			pPlayer = CBaseEntity::Instance( INDEXENT( 1 ) );
		}
		if ( pPlayer )
		{
			char szBuf[512];
			szBuf[0] = 0;
			RTN_SubstitutePlayerName( STRING(pev->message), szBuf, sizeof( szBuf ), pPlayer );
			UTIL_ShowMessage( szBuf, pPlayer );
		}
	}
	if ( pev->noise )
	{
		EMIT_SOUND( edict(), CHAN_BODY, STRING(pev->noise), pev->scale, pev->speed );
	}
	if ( pev->spawnflags & SF_MESSAGE_ONCE )
		UTIL_Remove( this );

	SUB_UseTargets( this, USE_TOGGLE, 0 );
}
