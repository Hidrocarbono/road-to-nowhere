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
// RTN weapon-script test target (see game_dir/scripts/weapons/weapon_parafal.txt).
// Any future "weapon_<name>" script just needs a matching LINK_ENTITY_TO_CLASS
// line here - the lookup below is generic, driven by the entity's own classname.
LINK_ENTITY_TO_CLASS( weapon_parafal, CWeaponScripted );

CWeaponScripted::CWeaponScripted()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>( this );
	auto contextImpl = std::make_unique<CMP5WeaponContext>( std::move( layerImpl ) );
	m_pWeaponContext = std::move( contextImpl );
	m_pInfo = NULL;
}

void CWeaponScripted::Spawn( void )
{
	// generic by classname: whatever entity name spawned us ("weapon_parafal",
	// "weapon_scripted", or any future LINK_ENTITY_TO_CLASS added above) is looked
	// up directly against scripts/weapons/<classname>.txt. Previously this was
	// hardcoded to always search "weapon_mp5", which meant nothing but an actual
	// weapon_mp5.txt (never committed) could ever be found here.
	const char *entClassname = STRING( pev->classname );
	m_pInfo = WeaponScript_FindWeaponByName( entClassname );
	// static_cast, NOT As<CMP5WeaponContext>(): As<>() asserts m_iId still equals
	// WEAPON_MP5 (CMP5WeaponContext's AssignedWeaponID), which stops being true
	// the moment SetScriptInfoWithDynamicId() below gives this context its own
	// id - the ctor-time m_iId (WEAPON_MP5) is only ever a starting default here.
	// We already know the concrete type (built as CMP5WeaponContext in our own
	// ctor above), so the cast is always safe.
	static_cast<CMP5WeaponContext *>( m_pWeaponContext.get() )->SetScriptInfoWithDynamicId( m_pInfo );
	Precache();
	if( m_pInfo && m_pInfo->worldmodel[0] )
		SET_MODEL( ENT( pev ), m_pInfo->worldmodel );
	else
		SET_MODEL( ENT( pev ), "models/w_9mmAR.mdl" );
	FallInit();
}

void CWeaponScripted::Precache( void )
{
	// engine may call Precache() before Spawn() - re-resolve defensively, same
	// pattern as CMP5::Precache() (weapon_mp5.cpp).
	const char *entClassname = STRING( pev->classname );
	if( entClassname && entClassname[0] )
	{
		m_pInfo = WeaponScript_FindWeaponByName( entClassname );
		static_cast<CMP5WeaponContext *>( m_pWeaponContext.get() )->SetScriptInfoWithDynamicId( m_pInfo );
	}
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
	// "none" is the RTN test scripts' convention for "no secondary ammo type"
	// (see weapon_parafal.txt); passing it through as a literal ammo name would
	// make the ammo system look up a type that doesn't exist in ammodesc.txt.
	p->pszAmmo2 = ( m_pInfo->secondary_ammo[0] && stricmp( m_pInfo->secondary_ammo, "none" ) )
		? m_pInfo->secondary_ammo : NULL;
	p->iMaxClip = m_pInfo->clip_size;
	p->iSlot = m_pInfo->bucket;
	p->iPosition = m_pInfo->bucket_position;
	p->iWeight = m_pInfo->weight;
	p->iFlags = m_pInfo->item_flags;
	// TODO: should be the primary_ammo type's MaxCarry from gAmmoInfo
	// (WeaponScript_FindAmmo), not MAX_WEAPON_NAME (a string-buffer size constant
	// that has nothing to do with ammo count) - pre-existing, untouched here.
	p->iMaxAmmo1 = MAX_WEAPON_NAME;
	// dynamic id (WeaponScript_GetWeaponID) - was hardcoded WEAPON_MP5 before,
	// which collided with the real MP5 in ItemInfoArray/HUD/client weapon
	// selection. m_pInfo is non-null here (checked at the top of this function),
	// so this always returns an already-assigned id (Spawn()/Precache() assign
	// it via SetScriptInfoWithDynamicId() before GetItemInfo() can run).
	p->iId = WeaponScript_GetWeaponID( m_pInfo );
	return 1;
}
