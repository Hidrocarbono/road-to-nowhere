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
#include "weapon_context.h"
#include "weapon_layer.h"
#include <memory>

#ifndef CLIENT_DLL
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#endif

#define WEAPON_CROWBAR		1
#define CROWBAR_WEIGHT		0
#define CROWBAR_CLASSNAME	weapon_crowbar

// RTN: o crowbar do HL foi substituido pelo SOCO (models/v_hand.mdl). Estes
// nomes continuam sendo usados pelo codigo (crowbar.cpp e o evento de swing no
// cliente), mas agora apontam para as SEQUENCIAS do v_hand.mdl, que tem so 7:
//   0 idle | 1 miss_small1 | 2 miss_small2 | 3 ready | 4 hit_small1 | 5 hit_small2 | 6 inspect
// Nao ha sequencia de holster nem terceira variante de golpe, entao repetem-se
// valores (IDLE no holster; miss1/hit1 reaproveitados no 3o golpe).
// Se um dia o modelo voltar a ser o do crowbar, restaurar a ordem original do HL:
// IDLE 0, DRAW 1, HOLSTER 2, ATTACK1HIT 3, ATTACK1MISS 4, ATTACK2MISS 5,
// ATTACK2HIT 6, ATTACK3MISS 7, ATTACK3HIT 8.
enum crowbar_e
{
	CROWBAR_IDLE = 0,
	CROWBAR_DRAW = 3,		// ready
	CROWBAR_HOLSTER = 0,		// sem holster no v_hand: fica em idle
	CROWBAR_ATTACK1HIT = 4,		// hit_small1
	CROWBAR_ATTACK1MISS = 1,	// miss_small1
	CROWBAR_ATTACK2MISS = 2,	// miss_small2
	CROWBAR_ATTACK2HIT = 5,		// hit_small2
	CROWBAR_ATTACK3MISS = 1,	// reaproveita miss_small1
	CROWBAR_ATTACK3HIT = 4		// reaproveita hit_small1
};

class CCrowbarWeaponContext : public CBaseWeaponContext
{
public:
	CCrowbarWeaponContext() = delete;
	~CCrowbarWeaponContext() = default;
	CCrowbarWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 1; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	bool Swing(bool fFirst);
	void PlaybackEvent();

	int m_iSwing;
	uint16_t m_usCrowbar;
#ifndef CLIENT_DLL
	TraceResult m_trHit;
#endif
};

template<>
struct CBaseWeaponContext::AssignedWeaponID<CCrowbarWeaponContext> {
	static constexpr int32_t value = WEAPON_CROWBAR;
};
