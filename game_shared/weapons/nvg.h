#pragma once
#include "weapon_context.h"
#include "weapon_layer.h"
#include <utility>

#define WEAPON_NVG		31
#define NVG_BATTERY_MAX	60.0f  // 60 seconds of battery per pickup

class CNVGWeaponContext : public CBaseWeaponContext
{
public:
	CNVGWeaponContext() = delete;
	~CNVGWeaponContext() = default;
	CNVGWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 1; }
	int iItemPosition() override { return 4; }
	int iMaxClip() override { return 1; }
	int iMaxAmmo1() override { return -1; }
	const char *pszAmmo1() override { return NULL; }
	int iFlags() override { return ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_NOAUTORELOAD; }
	bool IsUseable() override { return true; }
	bool UsePredicting() override { return false; }
	bool ShouldWeaponIdle() override { return true; }
	int GetItemInfo(ItemInfo *p) const override;
	bool Deploy() override;
	void PrimaryAttack() override;
	void ItemPostFrame() override;
	void WeaponIdle() override;

	bool IsNVGActive() const { return m_bNVGActive; }
	float GetBatteryLeft() const { return m_flBattery; }

private:
	void ApplyNVGState(bool active);
	bool m_bNVGActive = false;
	float m_flBattery = NVG_BATTERY_MAX;
};
