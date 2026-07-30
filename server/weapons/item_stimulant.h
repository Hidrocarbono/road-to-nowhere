#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

class CItemStimulant : public CBasePlayerWeapon
{
	DECLARE_CLASS( CItemStimulant, CBasePlayerWeapon );

public:
	CItemStimulant();

	void Spawn() override;
	void Precache() override;
	int AddToPlayer(CBasePlayer *pPlayer);
	BOOL Deploy() override;
	void PrimaryAttack() override;
	void WeaponIdle() override;
	int GetItemInfo(ItemInfo *p) const override;

	int iMaxClip() override { return -1; }
	int iItemSlot() override { return 1; }
	int iItemPosition() override { return 6; }
	int iWeight() override { return 5; }
	int iFlags() override { return ITEM_FLAG_SELECTONEMPTY; }
};