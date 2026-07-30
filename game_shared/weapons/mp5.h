#pragma once
#include "weapon_context.h"
#include "weapon_layer.h"
#include <utility>

#define WEAPON_MP5			4
#define MP5_WEIGHT			15
#define MP5_MAX_CLIP		50
#define MP5_DEFAULT_AMMO	25
#define MP5_DEFAULT_GIVE	25
#define MP5_CLASSNAME		weapon_mp5

enum mp5_anim_e
{
	MP5_ANIM_IDLE = 0,			// ACT_82
	MP5_ANIM_SHOOT1 = 1,		// ACT_84 1
	MP5_ANIM_SHOOT2 = 2,		// ACT_84 2
	MP5_ANIM_SHOOT3 = 3,		// ACT_84 3
	MP5_ANIM_RELOAD = 4,		// ACT_94 1
	MP5_ANIM_DEPLOY = 5,		// ACT_78 1
	MP5_ANIM_IDLE_AIM = 6,		// ACT_83 1
	MP5_ANIM_SHOOT1_AIM = 7,	// ACT_85 1
	MP5_ANIM_SHOOT2_AIM = 8,	// ACT_85 2
	MP5_ANIM_SHOOT3_AIM = 9,	// ACT_85 3
	MP5_ANIM_RELOAD_AIM = 10,	// ACT_95 1
	MP5_ANIM_AIM_IN = 11,		// ACT_109 1
	MP5_ANIM_AIM_OUT = 12,		// ACT_110 1
	// backward compat aliases for client event code
	MP5_LONGIDLE = 0,
	MP5_IDLE1 = 0,
	MP5_LAUNCH = 2,
	MP5_RELOAD = 4,
	MP5_DEPLOY = 5,
	MP5_FIRE1 = 1,
	MP5_FIRE2 = 2,
	MP5_FIRE3 = 3,
};

class CMP5WeaponContext : public CBaseWeaponContext
{
public:
	CMP5WeaponContext() = delete;
	~CMP5WeaponContext() = default;
	CMP5WeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 3; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	int SecondaryAmmoIndex() override;
	bool Deploy() override;
	void Reload() override;
	void WeaponIdle() override;

	bool m_bInIronSight = false;
	uint16_t m_usEvent1;
	uint16_t m_usEvent2;
};

template<>
struct CBaseWeaponContext::AssignedWeaponID<CMP5WeaponContext> {
	static constexpr int32_t value = WEAPON_MP5;
};