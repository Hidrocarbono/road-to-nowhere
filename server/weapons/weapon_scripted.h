/*
 * weapon_scripted.h - Fase 4: arma data-driven pelo weaponscript (Brother Hermes)
 * Road to Nowhere project
 */
#pragma once
#include "weapons.h"
#include "weapon_mp5.h"   // reusa o contexto de tiro da MP5
#include "weaponscript.h"

class CWeaponScripted : public CBasePlayerWeapon
{
public:
	DECLARE_CLASS( CWeaponScripted, CBasePlayerWeapon );
	CWeaponScripted();
	virtual void Spawn( void ) override;
	virtual void Precache( void ) override;
	virtual int GetItemInfo( ItemInfo *p ) const override;
	virtual const char *pszName( void ) override { return m_pInfo ? m_pInfo->scriptname : "weapon_scripted"; }
	virtual int iMaxClip( void ) override { return m_pInfo ? m_pInfo->clip_size : 0; }
	virtual const char *pszAmmo1( void ) override { return m_pInfo ? m_pInfo->primary_ammo : ""; }
	virtual int iMaxAmmo1( void ) override { return m_pInfo ? MAX_WEAPON_NAME : 0; }
	virtual int iWeight( void ) override { return m_pInfo ? m_pInfo->weight : 0; }
	virtual int iItemSlot( void ) override { return m_pInfo ? m_pInfo->bucket : 3; }
	virtual int iItemPosition( void ) override { return m_pInfo ? m_pInfo->bucket_position : 0; }
	// TODO: m_pInfo->item_flags holds WIF_IRONSIGHT/WIF_AUTOAIM/WIF_AUTOFIRE bits
	// (weaponscript.h), a different bit layout than ItemInfo::iFlags' ITEM_FLAG_*
	// bits (weapons.h) - returning it as-is here is pre-existing and untouched by
	// this pass, but needs a real WIF_*->ITEM_FLAG_* translation before it's correct.
	virtual int iFlags( void ) override { return m_pInfo ? m_pInfo->item_flags : 0; }
private:
	weaponinfo_t *m_pInfo;
};
