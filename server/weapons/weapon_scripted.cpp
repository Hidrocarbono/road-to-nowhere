/*
 * weapon_scripted.cpp - Fase 4: arma data-driven pelo weaponscript (Brother Hermes)
 * Road to Nowhere project
 */
#include "weapon_scripted.h"
#include "server_weapon_layer_impl.h"
#include "user_messages.h"
#include "weapon_layer.h"
#include "weapons/mp5.h"
#include "filesystem_utils.h"	// fs::FileExists - precache condicional dos modelos

LINK_ENTITY_TO_CLASS( weapon_scripted, CWeaponScripted );

//
// Registro DINAMICO das armas de script como entidades.
//
// Antes era preciso um LINK_ENTITY_TO_CLASS por arma aqui dentro - havia um para
// weapon_parafal e mais nenhum. Qualquer outro script da pasta batia em
// "Attempted to create unknown entity type weapon_m4!" no CreateEntityByName,
// mesmo com o .txt e os modelos no lugar certo. Um sistema de armas por SCRIPT
// que exige recompilar a dll para cada arma nova nao e um sistema por script.
//
// O PrimeXT resolve classname por um dicionario de fabricas
// (CEntityFactoryDictionary, server/util.cpp) cujo InstallFactory e publico na
// interface. LINK_ENTITY_TO_CLASS nada mais e que uma instancia estatica de
// CEntityFactory<T> que se registra sozinha no construtor. Nada impede registrar
// mais nomes depois, em runtime - e o que WeaponScript_RegisterEntities() faz,
// logo apos o parse dos scripts.
//
// Fabrica propria (em vez de CEntityFactory<CWeaponScripted>) porque aquela se
// instala sozinha no construtor, com UM nome fixo. Esta e uma so instancia
// compartilhada por todos os nomes: o Create() recebe o classname e o
// CWeaponScripted resolve o script por pev->classname, entao uma instancia
// atende qualquer quantidade de armas.
//
class CScriptedWeaponFactory : public IEntityFactory
{
public:
	CBaseEntity *Create( const char *pClassName, entvars_t *pev = NULL ) override
	{
		return GetClassPtr( (CWeaponScripted *)pev, pClassName );
	}
	void Destroy( CBaseEntity *pEntity ) override { UTIL_Remove( pEntity ); }
	size_t GetEntitySize() override { return sizeof( CWeaponScripted ); }
};

static CScriptedWeaponFactory g_ScriptedWeaponFactory;

void WeaponScript_RegisterEntities( void )
{
	int registered = 0;

	for( int i = 0; i < gNumWeaponInfo; i++ )
	{
		const char *name = gWeaponInfo[i].scriptname;
		if( !name || !name[0] )
			continue;

		// Nunca sobrescrever uma classe que ja existe: pode ser uma arma
		// hardcoded de verdade (weapon_mp5) que por acaso tambem tem .txt, e
		// nesse caso quem manda e a classe C++. InstallFactory tem assert de
		// nome duplicado, entao registrar por cima seria erro em debug.
		if( EntityFactoryDictionary()->FindFactory( name ))
			continue;

		EntityFactoryDictionary()->InstallFactory( &g_ScriptedWeaponFactory, name );
		registered++;
	}

	ALERT( at_console, "WeaponScript: %d arma(s) de script registradas como entidade\n", registered );
}

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
	// Mesma checagem de existencia do Precache: SET_MODEL com um caminho que nao
	// foi precacheado (porque o arquivo nao existe) derruba o servidor. Com o
	// registro dinamico de entidades, scripts sem modelo passaram a ser
	// spawnaveis, entao este caminho agora e alcancavel de verdade.
	if( m_pInfo && m_pInfo->worldmodel[0] && fs::FileExists( m_pInfo->worldmodel ))
		SET_MODEL( ENT( pev ), m_pInfo->worldmodel );
	else
		SET_MODEL( ENT( pev ), "models/w_9mmAR.mdl" );
	FallInit();
}

// Precacha um modelo do script apenas se ele existir de fato no disco. Devolve
// true quando o modelo foi registrado. Sem a checagem, um script sem os .mdl
// correspondentes derruba o carregamento do mapa inteiro.
bool CWeaponScripted::PrecacheScriptModel( const char *path )
{
	if( !path || !path[0] )
		return false;

	if( !fs::FileExists( path ))
	{
		ALERT( at_console, "WeaponScript [%s]: modelo [%s] nao existe - ignorado\n",
			STRING( pev->classname ), path );
		return false;
	}

	PRECACHE_MODEL( path );
	return true;
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
		// Precache CONDICIONAL a existencia do arquivo. Agora que TODA arma da
		// pasta de scripts vira uma entidade (WeaponScript_RegisterEntities),
		// varias delas vem do Paranoia 2 sem os modelos correspondentes - e
		// precachear modelo ausente aborta o carregamento do mapa. Preferimos
		// uma arma que nao aparece e uma linha no log a um mapa que nao abre.
		PrecacheScriptModel( m_pInfo->viewmodel );
		PrecacheScriptModel( m_pInfo->worldmodel );
		PrecacheScriptModel( m_pInfo->playermodel );

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
