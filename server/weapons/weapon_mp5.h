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

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "weaponscript.h"

class CMP5 : public CBasePlayerWeapon
{
	DECLARE_CLASS( CMP5, CBasePlayerWeapon );

public:
	CMP5();

	void Spawn() override;
	void Precache() override;
	int AddToPlayer(CBasePlayer *pPlayer);
	void Deploy() override;
	int GetItemInfo(ItemInfo *p) const override;
	int iMaxClip() override;
	const char *pszAmmo1() override;
	int iWeight() override;
	int iItemSlot() override;
	int iItemPosition() override;
	int iFlags() override;
private:
	weaponinfo_t *m_pScriptInfo;
};
