/*
 * weapon_scripted.cpp - Fase 4: arma data-driven pelo weaponscript (Brother Hermes)
 * Road to Nowhere project
 */
#include "weapon_scripted.h"
#include "server_weapon_layer_impl.h"
#include "user_messages.h"
#include "weapon_layer.h"
#include "weapons/mp5.h"

LINK_ENTITY_TO_CLASS( weapon_scripted, CWeaponScripted );

CWeaponScripted::CWeaponScripted()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>( this );
	auto contextImpl = std::make_unique<CMP5WeaponContext>( std::move( layerImpl ) );
	m_pWeaponContext = std::move( contextImpl );
	m_pInfo = NULL;
}

void CWeaponScripted::Spawn( void )
{
	m_pInfo = WeaponScript_FindWeaponByName( "weapon_mp5" );
	if( m_pInfo && m_pInfo->worldmodel[0] )
		pev->classname = MAKE_STRING( "weapon_scripted" );
	Precache();
	if( m_pInfo && m_pInfo->worldmodel[0] )
		SET_MODEL( ENT( pev ), m_pInfo->worldmodel );
	else
		SET_MODEL( ENT( pev ), "models/w_9mmAR.mdl" );
	FallInit();
}

void CWeaponScripted::Precache( void )
{
	if( m_pInfo )
	{
		if( m_pInfo->viewmodel[0] ) PRECACHE_MODEL( m_pInfo->viewmodel );
		if( m_pInfo->worldmodel[0] ) PRECACHE_MODEL( m_pInfo->worldmodel );
		if( m_pInfo->playermodel[0] ) PRECACHE_MODEL( m_pInfo->playermodel );
	}
	else
	{
		PRECACHE_MODEL( "models/v_9mmAR.mdl" );
		PRECACHE_MODEL( "models/w_9mmAR.mdl" );
		PRECACHE_MODEL( "models/p_9mmAR.mdl" );
	}
}

int CWeaponScripted::GetItemInfo( ItemInfo *p ) const
{
	if( !m_pInfo ) return 0;
	p->pszName = m_pInfo->scriptname;
	p->pszAmmo1 = m_pInfo->primary_ammo;
	p->pszAmmo2 = m_pInfo->secondary_ammo;
	p->iMaxClip = m_pInfo->clip_size;
	p->iSlot = m_pInfo->bucket;
	p->iPosition = m_pInfo->bucket_position;
	p->iWeight = m_pInfo->weight;
	p->iFlags = m_pInfo->item_flags;
	p->iMaxAmmo1 = MAX_WEAPON_NAME;
	p->iId = WEAPON_MP5;
	return 1;
}
