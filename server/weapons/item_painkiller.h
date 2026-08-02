#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

// RTN F9: painkiller - item de mundo estilo Paranoia 2.
// Dá munição "painkillers" (max 10 doses). Uso via comando painkiller_use (tecla H).
#define PAINKILLER_MAX_CARRY	10

class CItemPainkiller : public CBasePlayerAmmo
{
	DECLARE_CLASS( CItemPainkiller, CBasePlayerAmmo );
public:
	void Spawn( void ) override;
	void Precache( void ) override;
	BOOL AddAmmo( CBaseEntity *pOther ) override;
};
