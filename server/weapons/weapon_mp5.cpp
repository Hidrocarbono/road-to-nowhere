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
	m_pScriptInfo = NULL;  // init before Precache/Spawn can use it
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CMP5WeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CMP5::Spawn()
{
	m_pScriptInfo = WeaponScript_FindWeaponByName( "weapon_mp5" );
	m_pWeaponContext->As<CMP5WeaponContext>()->SetScriptInfo( m_pScriptInfo );
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
	// always re-lookup script in Precache (engine may call Precache before Spawn)
	m_pScriptInfo = WeaponScript_FindWeaponByName( "weapon_mp5" );
	m_pWeaponContext->As<CMP5WeaponContext>()->SetScriptInfo( m_pScriptInfo );
	if( m_pScriptInfo )
	{
		if( m_pScriptInfo->viewmodel[0] ) PRECACHE_MODEL( m_pScriptInfo->viewmodel );
		if( m_pScriptInfo->worldmodel[0] ) PRECACHE_MODEL( m_pScriptInfo->worldmodel );
		if( m_pScriptInfo->playermodel[0] ) PRECACHE_MODEL( m_pScriptInfo->playermodel );
	}
	PRECACHE_MODEL("models/v_mp5.mdl");
	PRECACHE_MODEL("models/p_mp5.mdl");
	PRECACHE_MODEL("models/w_mp5.mdl");
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
	// CMP5WeaponContext::Deploy() (game_shared/weapons/mp5.cpp) already sets
	// pev->viewmodel/weaponmodel - from m_pScriptInfo when a script was loaded,
	// falling back to the hardcoded MP5 models otherwise. No override needed here
	// anymore (removed the old "PLAN B" hardcode that was forcing v_mp5/p_mp5
	// unconditionally and silently defeating the script data).
	return CBasePlayerWeapon::Deploy();
}

int CMP5::GetItemInfo(ItemInfo *p) const
{
	// CMP5WeaponContext::GetItemInfo() (game_shared/weapons/mp5.cpp) already reads
	// from m_pScriptInfo when present, falling back to the classic MP5 values
	// otherwise - no need to overwrite them again here (removed the old "PLAN B"
	// hardcode block that was discarding script-driven clip/ammo/slot data).
	return CBasePlayerWeapon::GetItemInfo( p );
}

int CMP5::iMaxClip() { return 30; }
const char *CMP5::pszAmmo1() { return "9mm"; }
int CMP5::iWeight() { return 15; }
int CMP5::iItemSlot() { return 3; }
int CMP5::iItemPosition() { return 1; }
int CMP5::iFlags() { return ITEM_FLAG_SELECTONEMPTY; }

