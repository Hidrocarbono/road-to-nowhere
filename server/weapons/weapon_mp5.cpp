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
	// PLAN B: hardcoded viewmodel from weapon_mp5.txt (Fase 4 Final)
	BOOL result = CBasePlayerWeapon::Deploy();
	m_pPlayer->pev->viewmodel = MAKE_STRING( "models/v_mp5.mdl" );
	m_pPlayer->pev->weaponmodel = MAKE_STRING( "models/p_mp5.mdl" );
	return result;
}

int CMP5::GetItemInfo(ItemInfo *p) const
{
	int base = CBasePlayerWeapon::GetItemInfo( p );
	// PLAN B (Fase 4 Final): hardcoded from weapon_mp5.txt
	p->iMaxClip = 30;
	p->pszAmmo1 = "9mm";
	p->pszAmmo2 = "ARgrenades";
	p->iSlot = 3;
	p->iPosition = 6;
	p->iWeight = 15;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY;
	return base;
}

int CMP5::iMaxClip() { return 30; }
const char *CMP5::pszAmmo1() { return "9mm"; }
int CMP5::iWeight() { return 15; }
int CMP5::iItemSlot() { return 3; }
int CMP5::iItemPosition() { return 6; }
int CMP5::iFlags() { return ITEM_FLAG_SELECTONEMPTY; }

