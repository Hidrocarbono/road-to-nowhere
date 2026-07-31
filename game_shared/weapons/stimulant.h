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
	int GetItemInfo(ItemInfo *p) const override;
	bool Deploy() override;
	void PrimaryAttack() override;
	void WeaponIdle() override;
};