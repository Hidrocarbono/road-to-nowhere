/*
weaponscript.c - data-driven weapon/ammo script system (Xash Weapon System)
Copyright (C) 2026 Brother Hermes - sistema de armas por script, por Hermes e Hidrocarboneto

Parser for Uncle Mike's Paranoia 2 script format:
  ammodesc.txt -> ammoinfo { }  and  ammo_<name> { }
  weapon_*.txt -> WeaponData { } PrimaryAttack { } SecondaryAttack { }
                 SoundData { } hudsprite { }
Scripts are line-oriented key/value pairs inside { } blocks.
*/

#include "extdll.h"
#include "enginecallback.h"
#include "filesystem_utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <string>
#include <vector>
#include <filesystem>
#include "weaponscript.h"
#include "player.h"
#include "cbase.h"
#include "entities.h"

static void WS_Printf( const char *fmt, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, fmt );
#ifdef _MSC_VER
	_vsnprintf( buf, sizeof( buf ) - 1, fmt, ap );
#else
	vsnprintf( buf, sizeof( buf ), fmt, ap );
#endif
	va_end( ap );
	buf[sizeof( buf ) - 1] = 0;
	g_engfuncs.pfnServerPrint( buf );
}

static int WS_stricmp( const char *a, const char *b )
{
	while( *a && *b ) {
		int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
		if( ca != cb ) return ca - cb;
		a++; b++;
	}
	return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
static int WS_strnicmp( const char *a, const char *b, int n )
{
	for( int i = 0; i < n && a[i] && b[i]; i++ ) {
		int ca = tolower((unsigned char)a[i]), cb = tolower((unsigned char)b[i]);
		if( ca != cb ) return ca - cb;
	}
	return 0;
}
static void WS_strncpy( char *dst, const char *src, int n )
{
	strncpy( dst, src, n - 1 ); dst[n-1] = 0;
}


ammoinfo_t	gAmmoInfo[MAX_AMMO_TYPES];
int		gNumAmmoInfo = 0;
ammopickup_t	gAmmoPickups[MAX_AMMO_TYPES];
int		gNumAmmoPickups = 0;
weaponinfo_t	gWeaponInfo[MAX_AMMO_TYPES];
int		gNumWeaponInfo = 0;

ammoinfo_t *WeaponScript_FindAmmo( const char *name )
{
	int i;
	for( i = 0; i < gNumAmmoInfo; i++ )
	{
		if( !WS_stricmp( gAmmoInfo[i].name, name ) )
			return &gAmmoInfo[i];
	}
	return NULL;
}

weaponinfo_t *WeaponScript_FindWeaponByName( const char *scriptname )
{
	for( int i = 0; i < gNumWeaponInfo; i++ )
		if( !WS_stricmp( gWeaponInfo[i].scriptname, scriptname ) )
			return &gWeaponInfo[i];
	return NULL;
}

// Read a whole file (engine VFS) into a NUL-terminated buffer. Mem_Free() it.
static char *WS_LoadText( const char *filename )
{
	int size;
	char *buf = (char*)LOAD_FILE( filename, &size );
	char *text;

	if( !buf )
		return NULL;

	text = (char *)malloc( size + 1 );
	memcpy( text, buf, size );
	text[size] = '\0';
	FREE_FILE( buf );
	return text;
}

// Advance *pp past whitespace and comments; return next token (in place) or NULL.
static char *WS_NextToken( char **pp )
{
	char *p = *pp;

	while( *p )
	{
		while( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' )
			p++;

		if( *p == '/' && p[1] == '*' )
		{
			p += 2;
			while( *p && !(*p == '*' && p[1] == '/') )
				p++;
			if( *p ) p += 2;
			continue;
		}
		if( *p == '/' && p[1] == '/' )
		{
			while( *p && *p != '\n' )
				p++;
			continue;
		}

		if( *p == '"' )
		{
			char *start = p + 1;
			char *dst = p;
			p++;
			while( *p && *p != '"' )
				*dst++ = *p++;
			if( *p == '"' ) p++;
			*dst = '\0';
			*pp = p;
			return start;
		}

		if( *p != '{' && *p != '}' )
		{
			char *start = p;
			while( *p && *p != ' ' && *p != '\t' && *p != '\r'
				&& *p != '\n' && *p != '{' && *p != '}' )
				p++;
			*pp = p;
			return start;
		}

		{ char *start = p; p++; *pp = p; return start; }
	}
	*pp = p;
	return NULL;
}

// Parse "a..b" into outMin/outMax (midpoint when no range).
static void WS_ParseRange( const char *s, float *outMin, float *outMax )
{
	float a, b;
	const char *dot = strchr( s, '.' );
	if( dot && dot[1] == '.' )
	{
		char buf[64];
		WS_strncpy( buf, s, sizeof( buf ) );
		buf[dot - s] = '\0';
		a = atof( buf );
		b = atof( dot + 2 );
	}
	else
	{
		a = b = atof( s );
	}
	*outMin = a;
	*outMax = b;
}

// Read a { } block of key/value pairs, calling apply() for each pair.
static qboolean WS_ParseKVBlock( char **pp, void *out,
	void (*apply)( void *out, const char *key, const char *val ) )
{
	char *t = WS_NextToken( pp );
	if( !t || t[0] != '{' )
		return false;

	while( true )
	{
		char *k = WS_NextToken( pp );
		if( !k ) return false;
		if( k[0] == '}' )
			break;
		char *v = WS_NextToken( pp );
		if( !v ) return false;
		apply( out, k, v );
	}
	return true;
}

static void WS_ApplyAmmoInfo( void *out, const char *key, const char *val )
{
	ammoinfo_t *a = (ammoinfo_t *)out;
	if( !WS_stricmp( key, "name" ) ) WS_strncpy( a->name, val, sizeof( a->name ) );
	else if( !WS_stricmp( key, "MaxCarry" ) ) a->MaxCarry = atoi( val );
	else if( !WS_stricmp( key, "PlayerDamage" ) ) a->PlayerDamage = atoi( val );
	else if( !WS_stricmp( key, "MonsterDamage" ) ) a->MonsterDamage = atoi( val );
	else if( !WS_stricmp( key, "Damage" ) ) a->Damage = atoi( val );
	else if( !WS_stricmp( key, "Distance" ) ) a->Distance = atof( val );
	else if( !WS_stricmp( key, "NumShots" ) ) a->NumShots = atoi( val );
	else if( !WS_stricmp( key, "ShellModel" ) ) WS_strncpy( a->ShellModel, val, sizeof( a->ShellModel ) );
	else if( !WS_stricmp( key, "Missile" ) ) WS_strncpy( a->Missile, val, sizeof( a->Missile ) );
	else if( !WS_stricmp( key, "count" ) ) a->count = atoi( val );
}

static void WS_ApplyAmmoPickup( void *out, const char *key, const char *val )
{
	ammopickup_t *p = (ammopickup_t *)out;
	if( !WS_stricmp( key, "model" ) ) WS_strncpy( p->model, val, sizeof( p->model ) );
	else if( !WS_stricmp( key, "sound" ) ) WS_strncpy( p->sound, val, sizeof( p->sound ) );
	else if( !WS_stricmp( key, "type" ) ) WS_strncpy( p->type, val, sizeof( p->type ) );
	else if( !WS_stricmp( key, "count" ) ) p->count = atoi( val );
}

static int WS_FlagsFromString( const char *val )
{
	int f = 0;
	// format: "IronSight|AutoAim|AutoFire"
	char buf[128];
	WS_strncpy( buf, val, sizeof( buf ) );
	char *tok = strtok( buf, "|" );
	while( tok )
	{
		if( !WS_stricmp( tok, "IronSight" ) ) f |= WIF_IRONSIGHT;
		else if( !WS_stricmp( tok, "AutoAim" ) ) f |= WIF_AUTOAIM;
		else if( !WS_stricmp( tok, "AutoFire" ) ) f |= WIF_AUTOFIRE;
		tok = strtok( NULL, "|" );
	}
	return f;
}

static void WS_ApplyWeaponData( void *out, const char *key, const char *val )
{
	weaponinfo_t *w = (weaponinfo_t *)out;
	if( !WS_stricmp( key, "viewmodel" ) ) WS_strncpy( w->viewmodel, val, sizeof( w->viewmodel ) );
	else if( !WS_stricmp( key, "playermodel" ) ) WS_strncpy( w->playermodel, val, sizeof( w->playermodel ) );
	else if( !WS_stricmp( key, "worldmodel" ) ) WS_strncpy( w->worldmodel, val, sizeof( w->worldmodel ) );
	else if( !WS_stricmp( key, "anim_prefix" ) ) WS_strncpy( w->anim_prefix, val, sizeof( w->anim_prefix ) );
	else if( !WS_stricmp( key, "bucket" ) ) w->bucket = atoi( val );
	else if( !WS_stricmp( key, "bucket_position" ) ) w->bucket_position = atoi( val );
	else if( !WS_stricmp( key, "clip_size" ) ) w->clip_size = atoi( val );
	else if( !WS_stricmp( key, "defaultammo" ) ) w->defaultammo = atoi( val );
	else if( !WS_stricmp( key, "primary_ammo" ) ) WS_strncpy( w->primary_ammo, val, sizeof( w->primary_ammo ) );
	else if( !WS_stricmp( key, "secondary_ammo" ) ) WS_strncpy( w->secondary_ammo, val, sizeof( w->secondary_ammo ) );
	else if( !WS_stricmp( key, "weight" ) ) w->weight = atoi( val );
	else if( !WS_stricmp( key, "SpreadTime" ) ) w->SpreadTime = atof( val );
	else if( !WS_stricmp( key, "item_flags" ) ) w->item_flags = WS_FlagsFromString( val );
	else if( !WS_stricmp( key, "MaxSpeed" ) ) w->MaxSpeed = atof( val );
	else if( !WS_stricmp( key, "MaxSpeedIS" ) ) w->MaxSpeedIS = atof( val );
	else if( !WS_stricmp( key, "volume" ) ) WS_strncpy( w->volume, val, sizeof( w->volume ) );
	else if( !WS_stricmp( key, "flash" ) ) WS_strncpy( w->flash, val, sizeof( w->flash ) );
}


// Parse a "x..y" "z..w" "k" style triple (three quoted subs in one value) into 3 mids.
static void WS_ParseTriple( const char *s, float *out )
{
	char buf[256];
	WS_strncpy( buf, s, sizeof( buf ) );
	// split by '"' into up to 3 tokens
	char *tok = strtok( buf, "\"" );
	int n = 0;
	while( tok && n < 3 )
	{
		// skip leading spaces
		while( *tok == ' ' ) tok++;
		if( *tok )
		{
			float lo, hi;
			WS_ParseRange( tok, &lo, &hi );
			out[n++] = (lo + hi) * 0.5f;
		}
		tok = strtok( NULL, "\"" );
	}
	while( n < 3 ) out[n++] = 0;
}

static void WS_ApplyAttack( void *out, const char *key, const char *val )
{
	weaponattack_t *at = (weaponattack_t *)out;
	float lo, hi;
	if( !WS_stricmp( key, "action" ) ) WS_strncpy( at->action, val, sizeof( at->action ) );
	else if( !WS_stricmp( key, "nextattack" ) ) at->nextattack = atof( val );
	else if( !WS_stricmp( key, "PunchAngle" ) )
	{
		// "a..b" "c..d" "e"
		WS_ParseTriple( val, at->PunchAngle );
	}
	else if( !WS_stricmp( key, "PunchAngleIS" ) )
	{
		WS_ParseRange( val, &lo, &hi ); at->PunchAngleIS[0] = (lo + hi) * 0.5f;
	}
	else if( !WS_stricmp( key, "SpreadRange" ) )
	{
		WS_ParseRange( val, &at->SpreadRange[0], &at->SpreadRange[1] );
	}
	else if( !WS_stricmp( key, "SpreadExpand" ) ) at->SpreadExpand = atof( val );
	else if( !WS_stricmp( key, "SpreadRangeIS" ) )
	{
		WS_ParseRange( val, &at->SpreadRangeIS[0], &at->SpreadRangeIS[1] );
	}
	else if( !WS_stricmp( key, "SpreadExpandIS" ) ) at->SpreadExpandIS = atof( val );
}

static void WS_ApplySound( void *out, const char *key, const char *val )
{
	weaponsound_t *s = (weaponsound_t *)out;
	if( !WS_stricmp( key, "shootsound1" ) )
	{
		if( !s->shootsound1[0] ) WS_strncpy( s->shootsound1, val, sizeof( s->shootsound1 ) );
		else WS_strncpy( s->shootsound2, val, sizeof( s->shootsound2 ) );
	}
	else if( !WS_stricmp( key, "emptysound" ) ) WS_strncpy( s->emptysound, val, sizeof( s->emptysound ) );
}

static void WS_ApplySprite( void *out, const char *key, const char *val )
{
	weaponsprite_t *sp = (weaponsprite_t *)out;
	if( !WS_stricmp( key, "name" ) ) WS_strncpy( sp->name, val, sizeof( sp->name ) );
	else if( !WS_stricmp( key, "file" ) ) WS_strncpy( sp->file, val, sizeof( sp->file ) );
	else if( !WS_stricmp( key, "x" ) ) sp->x = atoi( val );
	else if( !WS_stricmp( key, "y" ) ) sp->y = atoi( val );
	else if( !WS_stricmp( key, "width" ) ) sp->width = atoi( val );
	else if( !WS_stricmp( key, "height" ) ) sp->height = atoi( val );
}

int WeaponScript_ParseAmmoDesc( const char *filename )
{
	char *text = WS_LoadText( filename );
	char *p;
	int parsed = 0;

	if( !text )
	{
		WS_Printf( "WeaponScript: cannot open %s\n", filename );
		return -1;
	}

	p = text;
	while( true )
	{
		char *t = WS_NextToken( &p );
		if( !t )
			break;

		if( !WS_stricmp( t, "ammoinfo" ) )
		{
			if( gNumAmmoInfo >= MAX_AMMO_TYPES )
			{
				WS_Printf( "WeaponScript: ammo type limit reached\n" );
				break;
			}
			if( WS_ParseKVBlock( &p, &gAmmoInfo[gNumAmmoInfo], WS_ApplyAmmoInfo ) )
				gNumAmmoInfo++;
			parsed++;
		}
		else if( !WS_strnicmp( t, "ammo_", 5 ) )
		{
			ammopickup_t pk;
			memset( &pk, 0, sizeof( pk ) );
			WS_strncpy( pk.classname, t, sizeof( pk.classname ) );
			if( gNumAmmoPickups < MAX_AMMO_TYPES )
			{
				if( WS_ParseKVBlock( &p, &pk, WS_ApplyAmmoPickup ) )
					gAmmoPickups[gNumAmmoPickups++] = pk;
			}
			else
			{
				WS_ParseKVBlock( &p, &pk, WS_ApplyAmmoPickup );
			}
		}
	}

	free( text );
	WS_Printf( "WeaponScript: parsed %d ammo definitions from %s\n", parsed, filename );
	return parsed;
}

int WeaponScript_ParseWeapon( const char *filename )
{
	char *text = WS_LoadText( filename );
	char *p;
	weaponinfo_t w;
	weaponsound_t snd;
	weaponsprite_t spr;
	qboolean haveSoundBlock = false;

	if( !text )
	{
		WS_Printf( "WeaponScript: cannot open %s\n", filename );
		return -1;
	}

	memset( &w, 0, sizeof( w ) );
	memset( &snd, 0, sizeof( snd ) );
	w.sound = snd;

	p = text;
	while( true )
	{
		char *t = WS_NextToken( &p );
		if( !t )
			break;

		if( !WS_stricmp( t, "WeaponData" ) )
		{
			WS_ParseKVBlock( &p, &w, WS_ApplyWeaponData );
		}
		else if( !WS_stricmp( t, "PrimaryAttack" ) )
		{
			WS_ParseKVBlock( &p, &w.primary, WS_ApplyAttack );
		}
		else if( !WS_stricmp( t, "SecondaryAttack" ) )
		{
			WS_ParseKVBlock( &p, &w.secondary, WS_ApplyAttack );
		}
		else if( !WS_stricmp( t, "SoundData" ) )
		{
			memset( &snd, 0, sizeof( snd ) );
			WS_ParseKVBlock( &p, &snd, WS_ApplySound );
			w.sound = snd;
		}
		else if( !WS_stricmp( t, "hudsprite" ) )
		{
			if( w.num_sprites < MAX_WEAPON_SPRITES )
			{
				memset( &spr, 0, sizeof( spr ) );
				WS_ParseKVBlock( &p, &spr, WS_ApplySprite );
				w.sprites[w.num_sprites++] = spr;
			}
			else
			{
				WS_ParseKVBlock( &p, &spr, WS_ApplySprite );
			}
		}
	}

	if( gNumWeaponInfo < MAX_AMMO_TYPES )
	{
		// store the script file basename (e.g. "weapon_mp5") for lookup by name
		const char *fname = filename;
		const char *slash = strrchr( filename, '/' );
		if( slash ) fname = slash + 1;
		WS_strncpy( w.scriptname, fname, sizeof( w.scriptname ) );
		size_t sl = strlen( w.scriptname );
		if( sl > 4 && !WS_stricmp( w.scriptname + sl - 4, ".txt" ) )
			w.scriptname[sl-4] = 0;
		gWeaponInfo[gNumWeaponInfo++] = w;
		WS_Printf( "WeaponScript: loaded weapon (%d sprites)\n", w.num_sprites );
	}

	free( text );
	return 0;
}

void WeaponScript_LoadAll( void )
{
	// Default script locations, relative to the game directory (gamedir).
	// Matches where mods keep them: scripts/weapons/ammodesc.txt and
	// scripts/weapons/weapon_*.txt  (e.g. valve/scripts/weapons/...)
	char gamedir[256];
	GET_GAME_DIR( gamedir );
	std::string base = std::string( gamedir ) + "/scripts/weapons";
	// try both possible locations for ammodesc.txt
	std::string ammoPath = base + "/ammodesc.txt";
	if( !fs::FileExists( ammoPath.c_str() ) )
	{
		std::string alt = std::string( gamedir ) + "/scripts/ammodesc.txt";
		if( fs::FileExists( alt.c_str() ) )
			ammoPath = alt;
	}
	WS_Printf( "WeaponScript: loading ammo desc from %s\n", ammoPath.c_str() );
	WeaponScript_ParseAmmoDesc( ammoPath.c_str() );
	try
	{
		for( auto &entry : std::filesystem::directory_iterator( base ) )
		{
			std::string name = entry.path().filename().string();
			if( name.rfind( "weapon_", 0 ) == 0 && name.size() > 7 && name.substr( name.size()-4 ) == ".txt" )
				WeaponScript_ParseWeapon( entry.path().string().c_str() );
		}
	}
	catch( ... )
	{
		WS_Printf( "WeaponScript: could not scan %s\n", base.c_str() );
	}
}


// ---------------------------------------------------------------------------
// Console commands (Fase 3): lets you test the parser without a mod.
// ---------------------------------------------------------------------------

static void WeaponScript_Reload_f( void )
{
	WeaponScript_LoadAll();
	WS_Printf( "WeaponScript: loaded %d weapons, %d ammo types, %d pickups\n",
		gNumWeaponInfo, gNumAmmoInfo, gNumAmmoPickups );
}

static void WeaponScript_List_f( void )
{
	int i;
	WS_Printf( "WeaponScript: %d weapons\n", gNumWeaponInfo );
	for( i = 0; i < gNumWeaponInfo; i++ )
	{
		weaponinfo_t *w = &gWeaponInfo[i];
		WS_Printf( "  [%d] %s (clip %d, ammo '%s', %d sprites)\n",
			i, w->viewmodel, w->clip_size, w->primary_ammo, w->num_sprites );
	}
	WS_Printf( "WeaponScript: %d ammo types\n", gNumAmmoInfo );
	for( i = 0; i < gNumAmmoInfo; i++ )
	{
		ammoinfo_t *a = &gAmmoInfo[i];
		if( a->MaxCarry )
			WS_Printf( "  [%d] %s (carry %d, pDmg %d, mDmg %d)\n",
				i, a->name, a->MaxCarry, a->PlayerDamage, a->MonsterDamage );
		else
			WS_Printf( "  [%d] %s (dmg %d, shots %d)\n",
				i, a->name, a->Damage, a->NumShots );
	}
	WS_Printf( "WeaponScript: %d ammo pickups\n", gNumAmmoPickups );
	for( i = 0; i < gNumAmmoPickups; i++ )
	{
		WS_Printf( "  [%d] %s -> '%s' x%d\n",
			i, gAmmoPickups[i].classname, gAmmoPickups[i].type, gAmmoPickups[i].count );
	}
}

void WeaponScript_Give_f( void )
{
	const char *name = CMD_ARGV( 1 );
	if( !name || !name[0] )
	{
		WS_Printf( "ws_give: usage: ws_give <classname>\n" );
		return;
	}
	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_FindEntityByClassname( NULL, "player" );
	if( !pPlayer )
	{
		WS_Printf( "ws_give: no player found\n" );
		return;
	}
	WS_Printf( "ws_give: giving %s\n", name );
	pPlayer->GiveNamedItem( name );
}

void WeaponScript_Init( void )
{
	g_engfuncs.pfnAddServerCommand( "weaponscript_reload", WeaponScript_Reload_f );
	g_engfuncs.pfnAddServerCommand( "weaponscript_list", WeaponScript_List_f );
	g_engfuncs.pfnAddServerCommand( "ws_give", WeaponScript_Give_f );
}