#pragma once
#include "weapon_context.h"
#include "weapon_layer.h"
#include <utility>

#ifndef CLIENT_DLL
// weaponscript.h lives under server/ and is only on the server's include path
// (server/CMakeLists.txt); the parser itself (weaponscript.cpp) is server-only
// too (uses std::filesystem + GET_GAME_DIR), so script-driven data can't reach
// the client build yet. Client-side prediction keeps using the hardcoded
// fallback models/anims below until that's ported - see m_pScriptInfo.
#include "weaponscript.h"
#endif

#define WEAPON_MP5			4

// RTN weapon-script: id range dynamically handed out to script-only weapons
// (weapon_scripted/CWeaponScripted instances, e.g. weapon_parafal) by
// WeaponScript_GetWeaponID() (server/weaponscript.h/.cpp) - never a fixed
// classic WEAPON_* constant, to avoid colliding with the real weapon that
// owns it in CBaseWeaponContext::ItemInfoArray[m_iId]. Defined here (not just
// in weaponscript.h) because client/weapon_predicting_context.cpp needs the
// range too, to build a predicted context for these ids instead of returning
// null - see GetWeaponContext()'s default case. Keep both copies in sync.
// The ceiling is imposed by game_dir/delta.lst (m_iId encoded with 5 bits), not
// by the engine - see the long comment on the same pair in server/weaponscript.h
// before changing either copy.
#ifndef WEAPON_SCRIPT_ID_BASE
#define WEAPON_SCRIPT_ID_BASE	31
#define WEAPON_SCRIPT_ID_MAX	31
#endif

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
	bool m_bFOVLerpActive = false;
	float m_fFOVLerpStart = 0.0f;
	float m_fFOVFrom = 90.0f;
	float m_fFOVTo = 90.0f;
	bool ShouldWeaponIdle() override { return true; }  // so WeaponIdle runs every frame (FOV lerp)
	uint16_t m_usEvent1;
	uint16_t m_usEvent2;

#ifndef CLIENT_DLL
	// populated server-side (Spawn/Precache) from WeaponScript_FindWeaponByName();
	// stays null when no matching scripts/weapons/<classname>.txt was loaded,
	// in which case Deploy()/GetItemInfo() keep the classic hardcoded MP5 values.
	// Used by the REAL weapon_mp5 entity (CMP5) - never touches m_iId, so it
	// stays WEAPON_MP5 always, even if a future weapon_mp5.txt gets added.
	void SetScriptInfo( const weaponinfo_t *info ) { m_pScriptInfo = info; }

	// Used by CWeaponScripted (any weapon_<name> that isn't the real MP5): same
	// as SetScriptInfo(), but also gives the context its own dynamic m_iId via
	// WeaponScript_GetWeaponID() - see the big comment on WEAPON_SCRIPT_ID_BASE
	// above for why this must NOT stay WEAPON_MP5. Falls back to WEAPON_MP5 when
	// info is null (no script found), matching the entity's own model/stat
	// fallback to the classic MP5 in that case - consistent id for a consistent
	// fallback identity.
	void SetScriptInfoWithDynamicId( const weaponinfo_t *info )
	{
		m_pScriptInfo = info;
		m_iId = info ? WeaponScript_GetWeaponID( const_cast<weaponinfo_t *>( info ) ) : WEAPON_MP5;
	}

	const weaponinfo_t *m_pScriptInfo = nullptr;
#endif
};

template<>
struct CBaseWeaponContext::AssignedWeaponID<CMP5WeaponContext> {
	static constexpr int32_t value = WEAPON_MP5;
};
