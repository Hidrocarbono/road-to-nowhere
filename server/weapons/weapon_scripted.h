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
	virtual int iMaxAmmo1( void ) override;
	virtual int iWeight( void ) override { return m_pInfo ? m_pInfo->weight : 0; }
	virtual int iItemSlot( void ) override { return m_pInfo ? m_pInfo->bucket : 3; }
	virtual int iItemPosition( void ) override { return m_pInfo ? m_pInfo->bucket_position : 0; }
	// NOT m_pInfo->item_flags - ver o comentario grande em GetItemInfo() (.cpp):
	// aquele campo e WIF_* (predicao), este e ITEM_FLAG_* (so-servidor) - fonte e
	// m_pInfo->inventory_flags, computado por ComputeIFlags(). Kept in sync with
	// what GetItemInfo() reports (mesma funcao, chamada dos dois lugares).
	virtual int iFlags( void ) override { return ComputeIFlags(); }
	// RTN: ITEM_FLAG_NODUPLICATE do script - ver item_info.h. Sem a flag, cai no
	// comportamento padrao de CBasePlayerWeapon (top de municao/clip).
	virtual int AddDuplicate( CBasePlayerItem *pOriginal ) override;
private:
	// Precacha um modelo do script so se o arquivo existir - ver o .cpp.
	bool PrecacheScriptModel( const char *path );
	int ComputeIFlags( void ) const;

	weaponinfo_t *m_pInfo;
};
