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
	// "defaultammo" from the script instead of CMP5WeaponContext's ctor default
	// (MP5_DEFAULT_GIVE). This is what ExtractAmmo() hands to AddPrimaryAmmo() on
	// pickup, so it decides how full the clip starts - and a weapon that arrives
	// with an empty clip and no reserve ammo fails CanDeploy(), which makes
	// SwitchWeapon() refuse it without a word: picked up, never equipped.
	if( m_pInfo && m_pInfo->defaultammo > 0 )
		m_pWeaponContext->m_iDefaultAmmo = m_pInfo->defaultammo;

	// Self-register into the shared ItemInfoArray when W_Precache() has not
	// already done it. That table - not the script - is what CanDeploy(),
	// pszAmmo1()/pszAmmo2() and iMaxClip() read on the CONTEXT
	// (CBaseWeaponContext reads ItemInfoArray[m_iId] directly), so an
	// unregistered row makes all of them return zero/NULL: the weapon then
	// fails CanDeploy() and SwitchWeapon() drops it without a word - picked
	// up, never equipped. W_Precache() can miss it whenever the scripts are
	// (re)loaded after the map precached, e.g. weaponscript_reload.
	ItemInfo selfInfo;
	memset( &selfInfo, 0, sizeof( selfInfo ) );
	if( GetItemInfo( &selfInfo ) && selfInfo.iId > 0 && selfInfo.iId < MAX_WEAPONS )
	{
		// Overwrite unconditionally rather than only filling an empty row: the
		// row can also hold STALE data (a previous map's load, or a row written
		// before the script data was correct), and stale is just as fatal as
		// missing - CanDeploy() reads it and refuses the weapon either way.
		// GetItemInfo() above is authoritative: it comes straight from the script.
		CBaseWeaponContext::ItemInfoArray[selfInfo.iId] = selfInfo;
	}

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

		// Sons do SoundData - ver CMP5WeaponContext::PrecacheScriptSounds() para
		// por que o precache e condicional a existencia do arquivo.
		static_cast<CMP5WeaponContext *>( m_pWeaponContext.get() )->PrecacheScriptSounds();
	}
	else
	{
		PRECACHE_MODEL( "models/v_9mmAR.mdl" );
		PRECACHE_MODEL( "models/w_9mmAR.mdl" );
		PRECACHE_MODEL( "models/p_9mmAR.mdl" );
	}
}

int CWeaponScripted::iMaxAmmo1( void )
{
	// same source as GetItemInfo()'s p->iMaxAmmo1 - ExtractAmmo()/GiveAmmo() call
	// this one directly (not through ItemInfo), so the two must agree or reserve
	// ammo gets clamped to a different ceiling than the HUD advertises.
	const ammoinfo_t *ammo1 = ( m_pInfo && m_pInfo->primary_ammo[0] ) ? WeaponScript_FindAmmo( m_pInfo->primary_ammo ) : NULL;
	return ( ammo1 && ammo1->MaxCarry > 0 ) ? Q_min( ammo1->MaxCarry, 254 ) : 1;
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
	// NOT m_pInfo->item_flags: that field holds WIF_IRONSIGHT|WIF_AUTOAIM|
	// WIF_AUTOFIRE (1|2|4, weaponscript.h), while this field is read as
	// ITEM_FLAG_SELECTONEMPTY|NOAUTORELOAD|NOAUTOSWITCHEMPTY (1|2|4,
	// game_shared/item_info.h) - same bits, completely unrelated meanings.
	// weapon_parafal.txt's "IronSight|AutoAim|AutoFire" was silently turning
	// into NOAUTORELOAD|NOAUTOSWITCHEMPTY, disabling auto-reload. The WIF_*
	// flags have no ITEM_FLAG_* equivalent (they describe firing behaviour, not
	// inventory behaviour), so they stay in m_pInfo for the weapon logic to read
	// and this reports the same inventory behaviour as the classic MP5.
	p->iFlags = ITEM_FLAG_SELECTONEMPTY;
	// max carry comes from the ammo type's MaxCarry in ammodesc.txt (e.g. "ak"
	// -> 120). Was MAX_WEAPON_NAME (64) - a string-buffer size constant that has
	// nothing to do with ammo counts, it just happened to be a plausible number.
	const ammoinfo_t *ammo1 = m_pInfo->primary_ammo[0] ? WeaponScript_FindAmmo( m_pInfo->primary_ammo ) : NULL;
	// clamped to a byte: UpdateClientData/WeaponList send this with WRITE_BYTE,
	// and 255 is the wire value the client turns back into -1 ("unlimited").
	p->iMaxAmmo1 = ( ammo1 && ammo1->MaxCarry > 0 ) ? Q_min( ammo1->MaxCarry, 254 ) : 1;
	const ammoinfo_t *ammo2 = p->pszAmmo2 ? WeaponScript_FindAmmo( p->pszAmmo2 ) : NULL;
	p->iMaxAmmo2 = ( ammo2 && ammo2->MaxCarry > 0 ) ? Q_min( ammo2->MaxCarry, 254 ) : -1;
	// dynamic id (WeaponScript_GetWeaponID) - was hardcoded WEAPON_MP5 before,
	// which collided with the real MP5 in ItemInfoArray/HUD/client weapon
	// selection. m_pInfo is non-null here (checked at the top of this function),
	// so this always returns an already-assigned id (Spawn()/Precache() assign
	// it via SetScriptInfoWithDynamicId() before GetItemInfo() can run).
	p->iId = WeaponScript_GetWeaponID( m_pInfo );
	return 1;
}
