/***
*
*	Road to Nowhere - visao noturna
*
****/
#include "item_nvgoggles.h"

LINK_ENTITY_TO_CLASS( item_nvgoggles, CItemNVGoggles );

// modelo default: reaproveita o w_battery ate existir arte propria do NVG.
// Como todo item do mod, aceita "model" e "noise" pelo mapa (padrao LRC).
#define NVG_DEFAULT_MODEL	"models/w_battery.mdl"
#define NVG_DEFAULT_SOUND	"items/gunpickup2.wav"

void CItemNVGoggles::Spawn( void )
{
	Precache();

	if( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ));
	else SET_MODEL( ENT( pev ), NVG_DEFAULT_MODEL );

	CItem::Spawn();
}

void CItemNVGoggles::Precache( void )
{
	if( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ));
	else PRECACHE_MODEL( NVG_DEFAULT_MODEL );

	if( pev->noise )
		PRECACHE_SOUND( (char *)STRING( pev->noise ));
	else PRECACHE_SOUND( NVG_DEFAULT_SOUND );

	// usados pelo liga/desliga e pela negativa de bateria vazia
	PRECACHE_SOUND( "items/9mmclip1.wav" );
	PRECACHE_SOUND( "items/suitchargeno1.wav" );
}

BOOL CItemNVGoggles::MyTouch( CBasePlayer *pPlayer )
{
	if( pPlayer->pev->deadflag != DEAD_NO )
		return FALSE;

	// ja tem o item e a bateria esta cheia: deixa o item no chao
	if( pPlayer->HasNVG() && pPlayer->m_iNVGBattery >= 100 )
		return FALSE;

	pPlayer->GiveNVG();

	if( pev->noise )
		EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, STRING( pev->noise ), 1, ATTN_NORM );
	else EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, NVG_DEFAULT_SOUND, 1, ATTN_NORM );

	MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev );
		WRITE_STRING( STRING( pev->classname ));
	MESSAGE_END();

	return TRUE;
}
