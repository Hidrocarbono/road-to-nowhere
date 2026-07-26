/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "weapon_mp5.h"
#include "user_messages.h"
#include "weapon_layer.h"
#include "weapons/mp5.h"
#include "server_weapon_layer_impl.h"
#include "weaponscript.h"

LINK_ENTITY_TO_CLASS( weapon_mp5, CMP5 );
LINK_ENTITY_TO_CLASS( weapon_9mmAR, CMP5 );

CMP5::CMP5()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CMP5WeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CMP5::Spawn()
{
	m_pScriptInfo = WeaponScript_FindWeaponByName( "weapon_mp5" );
	pev->classname = MAKE_STRING(CLASSNAME_STR(MP5_CLASSNAME));
	Precache();
	if( m_pScriptInfo && m_pScriptInfo->worldmodel[0] )
		SET_MODEL(ENT(pev), m_pScriptInfo->worldmodel );
	else
		SET_MODEL(ENT(pev), "models/w_9mmAR.mdl");
	FallInit(); // get ready to fall down.
}

void CMP5::Precache()
{
	if( m_pScriptInfo )
	{
		if( m_pScriptInfo->viewmodel[0] ) PRECACHE_MODEL( m_pScriptInfo->viewmodel );
		if( m_pScriptInfo->worldmodel[0] ) PRECACHE_MODEL( m_pScriptInfo->worldmodel );
		if( m_pScriptInfo->playermodel[0] ) PRECACHE_MODEL( m_pScriptInfo->playermodel );
	}
	PRECACHE_MODEL("models/v_9mmAR.mdl");
	PRECACHE_MODEL("models/w_9mmAR.mdl");
	PRECACHE_MODEL("models/p_9mmAR.mdl");

	PRECACHE_MODEL("models/shell.mdl");// brass shellTE_MODEL

	PRECACHE_MODEL("models/grenade.mdl");	// grenade

	PRECACHE_MODEL("models/w_9mmARclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("items/clipinsert1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	PRECACHE_SOUND("weapons/hks1.wav");// H to the K
	PRECACHE_SOUND("weapons/hks2.wav");// H to the K
	PRECACHE_SOUND("weapons/hks3.wav");// H to the K

	PRECACHE_SOUND("weapons/glauncher.wav");
	PRECACHE_SOUND("weapons/glauncher2.wav");

	PRECACHE_SOUND("weapons/357_cock1.wav");
}

int CMP5::AddToPlayer(CBasePlayer *pPlayer)
{
	if (CBasePlayerWeapon::AddToPlayer(pPlayer))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
			WRITE_BYTE(m_pWeaponContext->m_iId);
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CMP5::Deploy()
{
	BOOL result = CBasePlayerWeapon::Deploy();
	if( m_pScriptInfo )
	{
		if( m_pScriptInfo->viewmodel[0] )
			m_pPlayer->pev->viewmodel = MAKE_STRING( m_pScriptInfo->viewmodel );
		if( m_pScriptInfo->playermodel[0] )
			m_pPlayer->pev->weaponmodel = MAKE_STRING( m_pScriptInfo->playermodel );
	}
	return result;
}

int CMP5::GetItemInfo(ItemInfo *p) const
{
	int base = CBasePlayerWeapon::GetItemInfo( p );
	if( m_pScriptInfo )
	{
		p->iMaxClip = m_pScriptInfo->clip_size;
		p->pszAmmo1 = m_pScriptInfo->primary_ammo;
		p->pszAmmo2 = m_pScriptInfo->secondary_ammo;
		p->iSlot = m_pScriptInfo->bucket;
		p->iPosition = m_pScriptInfo->bucket_position;
		p->iWeight = m_pScriptInfo->weight;
		p->iFlags = m_pScriptInfo->item_flags;
	}
	return base;
}

int CMP5::iMaxClip() { return m_pScriptInfo ? m_pScriptInfo->clip_size : CBasePlayerWeapon::iMaxClip(); }
const char *CMP5::pszAmmo1() { return m_pScriptInfo ? m_pScriptInfo->primary_ammo : CBasePlayerWeapon::pszAmmo1(); }
int CMP5::iWeight() { return m_pScriptInfo ? m_pScriptInfo->weight : CBasePlayerWeapon::iWeight(); }
int CMP5::iItemSlot() { return m_pScriptInfo ? m_pScriptInfo->bucket : CBasePlayerWeapon::iItemSlot(); }
int CMP5::iItemPosition() { return m_pScriptInfo ? m_pScriptInfo->bucket_position : CBasePlayerWeapon::iItemPosition(); }
int CMP5::iFlags() { return m_pScriptInfo ? m_pScriptInfo->item_flags : CBasePlayerWeapon::iFlags(); }

