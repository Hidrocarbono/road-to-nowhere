#include "item_stimulant.h"
#include "server_weapon_layer_impl.h"
#include "weapon_layer.h"
#include "stimulant.h"

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
	if( CBasePlayerWeapon::AddToPlayer(pPlayer) )
	{
		return TRUE;
	}
	return FALSE;
}

int CItemStimulant::GetItemInfo(ItemInfo *p) const
{
	return m_pWeaponContext->GetItemInfo(p);
}