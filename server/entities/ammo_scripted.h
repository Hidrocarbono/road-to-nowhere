/*
 * ammo_scripted.h - pickup de municao data-driven pelo weaponscript
 * Road to Nowhere project
 *
 * RTN: faltava o equivalente de CWeaponScripted (weapons/weapon_scripted.h)
 * do lado da MUNICAO. O parser (server/weaponscript.cpp) sempre leu os
 * blocos "ammo_<nome> { model, sound, type, count }" do ammodesc.txt pra
 * dentro de gAmmoPickups[], mas nada nunca registrava esses classnames como
 * entidade de verdade - so as tres que ja tinham LINK_ENTITY_TO_CLASS
 * proprio (ammo_9mmclip, ammo_buckshot, ammo_rpgclip) eram spawnaveis.
 * Toda outra ("ammo_aks", "ammo_m16", "ammo_painkillers", etc.) batia em
 * "Attempted to create unknown entity type" no CreateEntityByName e nunca
 * aparecia no mapa - com modelo certo, com o .fgd certo, tanto faz.
 */
#pragma once
#include "weapons.h"
#include "weaponscript.h"

class CAmmoScripted : public CBasePlayerAmmo
{
	DECLARE_CLASS( CAmmoScripted, CBasePlayerAmmo );
public:
	void Spawn( void ) override;
	void Precache( void ) override;
	BOOL AddAmmo( CBaseEntity *pOther ) override;

private:
	// resolvido em Spawn() por pev->classname - mesmo truque de m_pInfo em
	// CWeaponScripted. NULL so deve acontecer se o classname que spawnou nao
	// bate com nenhum bloco do ammodesc.txt (arquivo desatualizado/editado
	// a mao errado), nunca em uso normal via .fgd.
	const ammopickup_t *m_pPickup;
};
