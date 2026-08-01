#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

class CItemNVG : public CBasePlayerWeapon
{
	DECLARE_CLASS( CItemNVG, CBasePlayerWeapon );

public:
	CItemNVG();
	void Spawn() override;
	void Precache() override;
	int AddToPlayer(CBasePlayer *pPlayer);
	int AddDuplicate(CBasePlayerItem *pItem) override;
	int GetItemInfo(ItemInfo *p) const override;
	int iItemSlot() override { return 1; }
	int iItemPosition() override { return 4; }
	int iMaxClip() override { return 1; }
	int iWeight() override { return 5; }
	int iFlags() override { return ITEM_FLAG_SELECTONEMPTY; }
};
