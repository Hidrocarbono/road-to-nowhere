#include "item_stimulant.h"
#include "server_weapon_layer_impl.h"
#include "weapon_layer.h"
#include "weapons/stimulant.h"
#include "client.h"
// RTN F9: atualiza HUD lateral de doses (declarada em server/client.cpp)
void SendRTNItemsHUD( CBasePlayer *pPlayer );

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
	// RTN F10 fix: 3 doses por aquisicao (o user testou com 1 dose e achou
	// que "nao reusa" - com 3 doses o reuso fica visivel no HUD).
	if( CBasePlayerWeapon::AddToPlayer(pPlayer) )
	{
		m_pWeaponContext->m_iClip = 3;
		SendRTNItemsHUD( pPlayer );  // RTN F9: atualiza contador lateral
		return TRUE;
	}
	return FALSE;
}

// RTN cumulative: GoldSrc calls AddDuplicate when the player already has the item.
// pItem = the EXISTING stimulant in inventory -> add one more dose, discard the new pickup.
int CItemStimulant::AddDuplicate(CBasePlayerItem *pItem)
{
	CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>(pItem);
	if( pWeapon && pWeapon->m_pWeaponContext )
	{
		pWeapon->m_pWeaponContext->m_iClip += 1;  // +1 dose (m_iClip is public)
		CBasePlayer *pPlayer = dynamic_cast<CBasePlayer *>( CBaseEntity::Instance( pWeapon->pev->owner ) );
		if( pPlayer )
			SendRTNItemsHUD( pPlayer );  // RTN F9: atualiza contador lateral
		return TRUE;  // new pickup is consumed, existing item keeps the doses
	}
	return FALSE;
}

int CItemStimulant::GetItemInfo(ItemInfo *p) const
{
	return m_pWeaponContext->GetItemInfo(p);
}