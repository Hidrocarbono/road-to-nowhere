#pragma once
#include "weapon_context.h"
#include "weapon_layer.h"
#include <utility>

#define WEAPON_STIMULANT	30

class CStimulantWeaponContext : public CBaseWeaponContext
{
public:
	CStimulantWeaponContext() = delete;
	~CStimulantWeaponContext() = default;
	CStimulantWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 1; }
	int iItemPosition() override { return 5; }
	int iMaxClip() override { return 99; }         // doses maximas (m_iClip = doses)
	int iMaxAmmo1() override { return -1; }
	const char *pszAmmo1() override { return NULL; }
	int iFlags() override { return ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_NOAUTORELOAD; }
	bool IsUseable() override { return true; }
	// no prediction: server always sends anims (SVC_WEAPONANIM) and GetWeaponTimeBase returns real time
	bool UsePredicting() override { return false; }
	bool ShouldWeaponIdle() override { return true; }  // run WeaponIdle every frame (use timer)
	bool m_bUseInProgress = false;
	bool m_bPendingUse = false;       // RTN F10: V equipou -> WeaponIdle dispara PrimaryAttack apos deploy
	float m_flUseFinishTime = 0.0f;
	int GetItemInfo(ItemInfo *p) const override;
	bool Deploy() override;
	void PrimaryAttack() override;
	void WeaponIdle() override;
};