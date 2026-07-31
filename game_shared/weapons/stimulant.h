#pragma once
#include "weapon_context.h"
#include "weapon_layer.h"
#include <utility>

class CStimulantWeaponContext : public CBaseWeaponContext
{
public:
	CStimulantWeaponContext() = delete;
	~CStimulantWeaponContext() = default;
	CStimulantWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 1; }
	int iItemPosition() override { return 6; }
	int iMaxClip() override { return 1; }          // 1 use per item
	int iMaxAmmo1() override { return -1; }
	const char *pszAmmo1() override { return NULL; }
	int iFlags() override { return ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_NOAUTORELOAD; }
	bool IsUseable() override { return true; }
	int GetItemInfo(ItemInfo *p) const override;
	bool Deploy() override;
	void PrimaryAttack() override;
	void WeaponIdle() override;
};