#include "item_nvg.h"
#include "server_weapon_layer_impl.h"
#include "weapon_layer.h"
#include "weapons/nvg.h"

LINK_ENTITY_TO_CLASS( item_nvg, CItemNVG );

CItemNVG::CItemNVG()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>( this );
	auto contextImpl = std::make_unique<CNVGWeaponContext>( std::move( layerImpl ) );
	m_pWeaponContext = std::move( contextImpl );
}

void CItemNVG::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING( "item_nvg" );
	FallInit();
}

void CItemNVG::Precache()
{
	PRECACHE_MODEL("models/v_antidote.mdl");
	PRECACHE_MODEL("models/w_antidote.mdl");
	PRECACHE_SOUND("items/smallmedkit1.wav");
}

int CItemNVG::AddToPlayer(CBasePlayer *pPlayer)
{
	// first time: add normally (starts with 1 charge = 60s battery)
	if( CBasePlayerWeapon::AddToPlayer(pPlayer) )
	{
		return TRUE;
	}
	return FALSE;
}

// RTN cumulative: picking another NVG adds battery to the existing one
int CItemNVG::AddDuplicate(CBasePlayerItem *pItem)
{
	CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>(pItem);
	if( pWeapon && pWeapon->m_pWeaponContext )
	{
		// add another 60s of battery (capped at 180s = 3 charges)
		CNVGWeaponContext *ctx = dynamic_cast<CNVGWeaponContext *>(pWeapon->m_pWeaponContext.get());
		if( ctx )
		{
			float battery = ctx->GetBatteryLeft() + NVG_BATTERY_MAX;
			if( battery > NVG_BATTERY_MAX * 3.0f )
				battery = NVG_BATTERY_MAX * 3.0f;
			ctx->AddBattery( NVG_BATTERY_MAX );
			return TRUE;
		}
	}
	return FALSE;
}

int CItemNVG::GetItemInfo(ItemInfo *p) const
{
	return m_pWeaponContext->GetItemInfo(p);
}
