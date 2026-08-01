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
	int AddDuplicate(CBasePlayerItem *pItem) override;
	int GetItemInfo(ItemInfo *p) const override;
	int iItemSlot() override { return 1; }
	int iItemPosition() override { return 6; }
	int iMaxClip() override { return -1; }
	int iWeight() override { return 5; }
	int iFlags() override { return ITEM_FLAG_SELECTONEMPTY; }
};