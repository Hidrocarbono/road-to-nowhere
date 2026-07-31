#include "mp5.h"

#ifdef CLIENT_DLL
#else
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#include "ggrenade.h"
#endif

CMP5WeaponContext::CMP5WeaponContext(std::unique_ptr<IWeaponLayer> &&layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_MP5;
	m_iDefaultAmmo = MP5_DEFAULT_GIVE;
	m_usEvent1 = m_pLayer->PrecacheEvent("events/mp5.sc");
	m_usEvent2 = m_pLayer->PrecacheEvent("events/mp52.sc");
	m_bInIronSight = false;
}

int CMP5WeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(MP5_CLASSNAME);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = "ARgrenades";
	p->iMaxAmmo2 = M203_GRENADE_MAX_CARRY;
	p->iMaxClip = MP5_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId;
	p->iWeight = MP5_WEIGHT;
	return 1;
}

int CMP5WeaponContext::SecondaryAmmoIndex()
{
	return m_iSecondaryAmmoType;
}

bool CMP5WeaponContext::Deploy()
{
	m_bInIronSight = false;
	return DefaultDeploy( "models/v_mp5.mdl", "models/p_mp5.mdl", MP5_ANIM_DEPLOY, "mp5" );
}

void CMP5WeaponContext::PrimaryAttack()
{
	// don't fire underwater
	if (m_pLayer->GetPlayerWaterlevel() == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.15f);
		return;
	}

	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.15f);
		return;
	}

	m_iClip--;

	// adjust gun position for v_mp5.mdl model offset (pull back 4 units when not aiming)
 = m_pLayer->GetCameraOrientation();
	cameraTransform.SetForward(m_pLayer->GetAutoaimVector(AUTOAIM_5DEGREES));

	// spread depends on ironsight: normal = 3deg, aiming = 1deg (more accurate)
	Vector spread;
	if( m_bInIronSight )
		spread = m_pLayer->IsMultiplayer() ? VECTOR_CONE_1DEGREES : VECTOR_CONE_1DEGREES;
	else
		spread = m_pLayer->IsMultiplayer() ? VECTOR_CONE_6DEGREES : VECTOR_CONE_3DEGREES;

	Vector vecDir = m_pLayer->FireBullets(1, vecSrc, cameraTransform, 8192, spread.x, BULLET_PLAYER_MP5, m_pLayer->GetRandomSeed());

	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usEvent1;
	params.delay = 0.0f;
	params.origin = vecSrc;
	params.angles = cameraTransform.GetAngles();
	params.fparam1 = vecDir.x;
	params.fparam2 = vecDir.y;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = 0;
	params.bparam2 = 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

	// send correct shoot animation based on ironsight state


#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	player->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	player->m_iWeaponFlash = NORMAL_GUN_FLASH;
	player->pev->effects = (int)(player->pev->effects) | EF_MUZZLEFLASH;
	player->SetAnimation(PLAYER_ATTACK1);

	if (!m_iClip && player->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		player->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#endif

	m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.1f);
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
}

void CMP5WeaponContext::SecondaryAttack()
{
	// Toggle ironsight on right-click
	m_bInIronSight = !m_bInIronSight;

	// Play the ironsight transition animation
	if( m_bInIronSight )
	{
		SendWeaponAnim( MP5_ANIM_AIM_IN );
		m_pLayer->SetPlayerFOV( 55 );  // slight zoom when aiming
	}
	else
	{
		SendWeaponAnim( MP5_ANIM_AIM_OUT );
		m_pLayer->SetPlayerFOV( 90 );  // back to normal
	}

	m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.3f;
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 2.0f;
}

void CMP5WeaponContext::Reload()
{
	if( m_bInIronSight )
		DefaultReload( MP5_MAX_CLIP, MP5_ANIM_RELOAD_AIM, 1.5 );
	else
		DefaultReload( MP5_MAX_CLIP, MP5_ANIM_RELOAD, 1.5 );
}

void CMP5WeaponContext::WeaponIdle()
{
	ResetEmptySound();
	m_pLayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	if( m_bInIronSight )
		SendWeaponAnim( MP5_ANIM_IDLE_AIM );
	else
		SendWeaponAnim( MP5_ANIM_IDLE );

	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
}