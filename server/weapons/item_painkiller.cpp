#include "item_painkiller.h"
#include "client.h"
// RTN F9: atualiza HUD lateral de doses (declarada em server/client.cpp)
void SendRTNItemsHUD( CBasePlayer *pPlayer );

// RTN F9: painkiller estilo Paranoia 2 - item de mundo que dá 1 dose de "painkillers".
// O som de uso é tocado pelo comando painkiller_use (server client.cpp).

LINK_ENTITY_TO_CLASS( item_painkiller, CItemPainkiller );

void CItemPainkiller::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/w_antidote.mdl" );  // reuso do modelo antidote (Paranoia 2 usa w_painkiller.mdl)
	CBasePlayerAmmo::Spawn( );
}

void CItemPainkiller::Precache( void )
{
	PRECACHE_MODEL( "models/w_antidote.mdl" );
	PRECACHE_SOUND( "items/painkiller_pickup.wav" );
}

BOOL CItemPainkiller::AddAmmo( CBaseEntity *pOther )
{
	int bResult = ( pOther->GiveAmmo( 1, "painkillers", PAINKILLER_MAX_CARRY ) != -1 );
	if ( bResult )
	{
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/painkiller_pickup.wav", 1, ATTN_NORM );
		pOther->pev->weapons |= ( 1 << 31 );  // flag de posse (bit 31, igual WEAPON_PAINKILLER do Paranoia)
		SendRTNItemsHUD( (CBasePlayer *)pOther );  // RTN F9: atualiza contador lateral
	}
	return bResult;
}
