#include "stimulant.h"

#ifdef CLIENT_DLL
#else
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#endif

CStimulantWeaponContext::CStimulantWeaponContext(std::unique_ptr<IWeaponLayer> &&layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = -1; // not a standard weapon
	m_iClip = -1;
}

int CStimulantWeaponContext::GetItemInfo(ItemInfo *p) const
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
	p->iId = -1;
	p->iWeight = 5;
	return 1;
}

bool CStimulantWeaponContext::Deploy()
{
	return DefaultDeploy( "models/v_antidote.mdl", "models/w_antidote.mdl", 0, "medkit" );
}

void CStimulantWeaponContext::PrimaryAttack()
{
#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	if( !player ) return;

	// Heal 50 HP
	player->TakeHealth( 50, DMG_GENERIC );

	// Restore stamina - fuser2 is stamina in Xash/PrimeXT
	player->pev->fuser2 = 100.0f;

	// Play sound (only if not client)
	EMIT_SOUND( ENT(player), CHAN_ITEM, "items/smallmedkit1.wav", 1.0, ATTN_NORM );

	// Remove from player inventory
	player->RemovePlayerItem( m_pLayer->GetWeaponEntity() );

	// Remove from world
	m_pLayer->GetWeaponEntity()->SetThink( NULL );
	UTIL_Remove( m_pLayer->GetWeaponEntity() );
#endif
}

void CStimulantWeaponContext::WeaponIdle()
{
	ResetEmptySound();
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 5.0f;
}