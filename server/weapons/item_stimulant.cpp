#include "item_stimulant.h"

LINK_ENTITY_TO_CLASS( item_stimulant, CItemStimulant );

CItemStimulant::CItemStimulant()
{
}

void CItemStimulant::Spawn()
{
	Precache();
	m_iId = WEAPON_NONE; // not in weapon list
	m_iClip = -1;
	FallInit();
}

void CItemStimulant::Precache()
{
	PRECACHE_MODEL("models/v_antidote.mdl");
	PRECACHE_SOUND("items/smallmedkit1.wav");
}

int CItemStimulant::AddToPlayer(CBasePlayer *pPlayer)
{
	if( CBasePlayerWeapon::AddToPlayer(pPlayer) )
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
		WRITE_BYTE(m_pWeaponContext ? m_pWeaponContext->m_iId : 0);
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CItemStimulant::Deploy()
{
	return DefaultDeploy( "models/v_antidote.mdl", "models/v_antidote.mdl", 0, "medkit" );
}

void CItemStimulant::PrimaryAttack()
{
	CBasePlayer *pPlayer = m_pPlayer;
	if( !pPlayer )
		return;

	// Heal
	pPlayer->TakeHealth( 50, DMG_GENERIC ); // heal 50 HP

	// Restore stamina
	pPlayer->pev->fuser2 = 100.0f; // stamina (fuser2 = stamina in Xash/PrimeXT)
	// Also reset sprint cooldown
	pPlayer->m_flStopEnergyTime = gpGlobals->time + 1.0f;

	// Play sound
	EMIT_SOUND( ENT(pPlayer), CHAN_ITEM, "items/smallmedkit1.wav", 1.0, ATTN_NORM );

	// Remove from inventory
	pPlayer->RemoveWeapon( this );

	// remove from world
	UTIL_Remove( this );
}

void CItemStimulant::WeaponIdle()
{
	m_flTimeWeaponIdle = m_pLayer ? m_pLayer->GetWeaponTimeBase(true) + 5.0f : gpGlobals->time + 5.0f;
}

int CItemStimulant::GetItemInfo(ItemInfo *p) const
{
	p->pszName = "item_stimulant";
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = -1;
	p->iSlot = 1;
	p->iPosition = 6;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY;
	p->iId = WEAPON_NONE;
	p->iWeight = 5;
	return 1;
}