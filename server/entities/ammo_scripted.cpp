/*
 * ammo_scripted.cpp - pickup de municao data-driven pelo weaponscript
 * Road to Nowhere project
 *
 * Ver o comentario grande em ammo_scripted.h para o porque disto existir.
 */
#include "ammo_scripted.h"

//
// Registro DINAMICO dos pickups de municao como entidades - mesmo mecanismo
// de CScriptedWeaponFactory (server/weapons/weapon_scripted.cpp): o PrimeXT
// resolve classname por um dicionario de fabricas (CEntityFactoryDictionary),
// e InstallFactory aceita registrar um nome novo em runtime, depois que o
// ammodesc.txt ja foi parseado.
//
// Fabrica propria (nao CEntityFactory<CAmmoScripted>) pelo mesmo motivo de
// la: aquela se registra sozinha com UM nome fixo no construtor; esta e uma
// unica instancia compartilhada por todos os classnames "ammo_*" do script -
// CAmmoScripted::Spawn() resolve qual bloco usar por pev->classname.
//
class CScriptedAmmoFactory : public IEntityFactory
{
public:
	CBaseEntity *Create( const char *pClassName, entvars_t *pev = NULL ) override
	{
		return GetClassPtr( (CAmmoScripted *)pev, pClassName );
	}
	void Destroy( CBaseEntity *pEntity ) override { UTIL_Remove( pEntity ); }
	size_t GetEntitySize() override { return sizeof( CAmmoScripted ); }
};

static CScriptedAmmoFactory g_ScriptedAmmoFactory;

void AmmoScript_RegisterEntities( void )
{
	int registered = 0;

	for( int i = 0; i < gNumAmmoPickups; i++ )
	{
		const char *name = gAmmoPickups[i].classname;
		if( !name || !name[0] )
			continue;

		// Nunca sobrescrever uma classe que ja existe: ammo_9mmclip,
		// ammo_buckshot e ammo_rpgclip tem entidade C++ fixa
		// (LINK_ENTITY_TO_CLASS em server/entities/ammo_*.cpp) - o bloco
		// delas no ammodesc.txt e decorativo, e quem manda e o C++.
		// InstallFactory tem assert de nome duplicado, entao registrar por
		// cima seria erro em debug.
		if( EntityFactoryDictionary()->FindFactory( name ) )
			continue;

		EntityFactoryDictionary()->InstallFactory( &g_ScriptedAmmoFactory, name );
		registered++;
	}

	ALERT( at_console, "WeaponScript: %d pickup(s) de municao registrados como entidade\n", registered );
}

void CAmmoScripted::Spawn( void )
{
	m_pPickup = WeaponScript_FindAmmoPickup( STRING( pev->classname ) );

	if( !m_pPickup )
	{
		// Nao deveria acontecer em uso normal: so chegamos aqui se o classname
		// que a fabrica registrou (a partir de gAmmoPickups[]) nao bate mais
		// com nada nele - ex: ammodesc.txt recarregado (weaponscript_reload)
		// com o bloco removido, mas a entidade ja spawnada no mapa continua
		// tentando achar o que nao existe mais.
		ALERT( at_console, "CAmmoScripted::Spawn: [%s] sem bloco correspondente no ammodesc.txt\n",
			STRING( pev->classname ) );
		UTIL_Remove( this );
		return;
	}

	Precache();
	SET_MODEL( ENT( pev ), m_pPickup->model );

	BaseClass::Spawn();
}

void CAmmoScripted::Precache( void )
{
	if( !m_pPickup )
		return;

	if( m_pPickup->model[0] )
		PRECACHE_MODEL( m_pPickup->model );

	if( m_pPickup->sound[0] )
		PRECACHE_SOUND( m_pPickup->sound );
}

BOOL CAmmoScripted::AddAmmo( CBaseEntity *pOther )
{
	if( !m_pPickup )
		return FALSE;

	const int count = m_pPickup->count > 0 ? m_pPickup->count : 1;

	// MaxCarry vem do ammoinfo_t (o bloco "ammoinfo" do tipo, ex: "7.62"),
	// nao do pickup em si - mesma fonte que WeaponScript_FindAmmo() ja
	// alimenta em outros lugares (CWeaponScripted::iMaxAmmo1(), por exemplo).
	// Tipo nao encontrado (ammodesc.txt com "type" apontando pra ammoinfo
	// inexistente) cai num teto generoso em vez de travar o pickup.
	const ammoinfo_t *info = m_pPickup->type[0] ? WeaponScript_FindAmmo( m_pPickup->type ) : NULL;
	const int maxCarry = ( info && info->MaxCarry > 0 ) ? info->MaxCarry : 999;

	if( pOther->GiveAmmo( count, const_cast<char *>( m_pPickup->type ), maxCarry ) != -1 )
	{
		if( m_pPickup->sound[0] )
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, m_pPickup->sound, 1, ATTN_NORM );
		return TRUE;
	}

	return FALSE;
}
