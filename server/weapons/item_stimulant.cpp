#include "item_stimulant.h"
#include "server_weapon_layer_impl.h"
#include "weapon_layer.h"
#include "weapons/stimulant.h"

LINK_ENTITY_TO_CLASS( item_stimulant, CItemStimulant );

CItemStimulant::CItemStimulant()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>( this );
	auto contextImpl = std::make_unique<CStimulantWeaponContext>( std::move( layerImpl ) );
	m_pWeaponContext = std::move( contextImpl );
}

void CItemStimulant::Spawn()
{
	Precache();
	// Use this to handle pickup
	pev->classname = MAKE_STRING( "item_stimulant" );
	FallInit();
}

void CItemStimulant::Precache()
{
	PRECACHE_MODEL("models/v_antidote.mdl");
	PRECACHE_MODEL("models/w_antidote.mdl");
	PRECACHE_SOUND("items/smallmedkit1.wav");
}

int CItemStimulant::AddToPlayer(CBasePlayer *pPlayer)
{
	// RTN cumulative: if player already has a stimulant, just add a dose
	for( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[i];
		while( pItem )
		{
			if( pItem->iItemSlot() == 1 && pItem->m_pWeaponContext &&
				pItem->m_pWeaponContext->m_iId == WEAPON_STIMULANT )
			{
				pItem->m_pWeaponContext->m_iClip += 1;  // +1 dose (m_iClip is public)
				return TRUE;  // picked up, no duplicate entity added
			}
			pItem = pItem->m_pNext;
		}
	}

	// first time: add normally (starts with 1 dose)
	if( CBasePlayerWeapon::AddToPlayer(pPlayer) )
	{
		m_pWeaponContext->m_iClip = 1;
		return TRUE;
	}
	return FALSE;
}

int CItemStimulant::GetItemInfo(ItemInfo *p) const
{
	return m_pWeaponContext->GetItemInfo(p);
}