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
#include "cbase.h"
#include "player.h"	// CBasePlayer::m_pActiveItem - ws_give diagnostics
#include "weapons.h"	// CBasePlayerItem definition - player.h only forward-declares it,
			// and the diagnostics below dereference m_pActiveItem

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


ammoinfo_t	gAmmoInfo[WS_MAX_ENTRIES];
int		gNumAmmoInfo = 0;
ammopickup_t	gAmmoPickups[WS_MAX_ENTRIES];
int		gNumAmmoPickups = 0;
weaponinfo_t	gWeaponInfo[WS_MAX_ENTRIES];
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

static void WS_StripTxt( char *dst, const char *src, size_t n )
{
	WS_strncpy( dst, src, n );
	size_t l = strlen( dst );
	if( l > 4 && !WS_stricmp( dst + l - 4, ".txt" ) )
		dst[l-4] = '\0';
}

weaponinfo_t *WeaponScript_FindWeaponByName( const char *scriptname )
{
	char want[64];
	WS_StripTxt( want, scriptname, sizeof( want ) );
	// This used to print one line PER ENTRY on every call - with 18 scripts and
	// several calls per pickup that is 50+ lines that scroll the map-load
	// diagnostics (where the parse results are reported) clean off the screen.
	// Only the failure is worth a line.
	for( int i = 0; i < gNumWeaponInfo; i++ )
	{
		char have[64];
		WS_StripTxt( have, gWeaponInfo[i].scriptname, sizeof( have ) );
		if( !WS_stricmp( have, want ) )
			return &gWeaponInfo[i];
	}
	WS_Printf( "WeaponScript: [%s] nao encontrado entre os %d scripts carregados\n", want, gNumWeaponInfo );
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

// A token has to be NUL-terminated to be comparable with WS_stricmp(). A quoted
// token is terminated in place (the closing quote becomes the NUL), but an
// unquoted one is followed by a delimiter we cannot always overwrite: it may be a
// '{' or '}' that still has to come back as the NEXT token. So those get copied
// out instead. One shared buffer would not do - WS_ParseKVBlock() holds a key and
// a value at the same time - hence a small rotating set.
// 4 bastava para chave+valor. Com chaves de multiplos valores (PunchAngle puxa
// mais dois tokens sem soltar a chave nem o primeiro valor) chegam a ficar 4
// vivos ao mesmo tempo - exatamente o tamanho antigo, ou seja, no limite. 8 da
// folga para a proxima chave de N valores sem que a chave viva seja sobrescrita
// por baixo do laco.
#define WS_TOKEN_RING	8
static char ws_tokenRing[WS_TOKEN_RING][256];
static int  ws_tokenRingPos = 0;

static char *WS_CopyToken( const char *src, size_t len )
{
	char *dst = ws_tokenRing[ws_tokenRingPos];
	ws_tokenRingPos = ( ws_tokenRingPos + 1 ) % WS_TOKEN_RING;
	if( len >= sizeof( ws_tokenRing[0] ) )
		len = sizeof( ws_tokenRing[0] ) - 1;
	memcpy( dst, src, len );
	dst[len] = '\0';
	return dst;
}

// Advance *pp past whitespace and comments; return next token or NULL.
static char *WS_NextToken( char **pp )
{
	char *p = *pp;

	while( *p )
	{
		// skip UTF-8 BOM (EF BB BF)
		if( (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF )
			p += 3;
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
			// The loop below shifts the quoted content one byte left, over the
			// opening quote, so it can be NUL-terminated in place without eating
			// the character that follows the closing quote. The token therefore
			// begins where the opening quote was - NOT one byte after it.
			// This used to be "p + 1", which returned the token minus its first
			// character: every quoted key came out as "iewmodel", "ucket",
			// "lip_size"..., so no key ever matched in WS_ApplyWeaponData() and
			// every quoted field stayed at its zeroed default. Only unquoted
			// tokens (the block names WeaponData/PrimaryAttack/..., and the
			// scriptname, which comes from the FILENAME) ever survived - which is
			// exactly why a weapon could be found by name while every one of its
			// fields read back empty.
			char *start = p;
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
			// This used to "return start" straight into the text, with no NUL:
			// the caller then compared a string that ran on past the token
			// ("WeaponData\n{\n\t\"viewmodel\"..."), so WS_stricmp() came back
			// with the delimiter's value instead of 0 and NO unquoted block name
			// ever matched - WeaponData, PrimaryAttack, SecondaryAttack,
			// SoundData and ammoinfo alike. That is why every weapon parsed to
			// empty fields while the QUOTED "hudsprite" blocks were counted
			// correctly, and why ammodesc.txt reported "parsed 0 ammo definitions".
			char *start = p;
			while( *p && *p != ' ' && *p != '\t' && *p != '\r'
				&& *p != '\n' && *p != '{' && *p != '}' )
				p++;
			*pp = p;
			return WS_CopyToken( start, (size_t)( p - start ) );
		}

		{ char *start = p; p++; *pp = p; return WS_CopyToken( start, 1 ); }
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
// `apply` recebe tambem o cursor (pp) para poder consumir tokens EXTRAS quando a
// chave tiver mais de um valor. E o caso de "PunchAngle" "a..b" "c..d" "e", que
// sao tres valores numa linha: o laco generico aqui so sabe ler pares chave/valor,
// entao antes ele entregava so o primeiro componente e depois lia "c..d" como se
// fosse uma NOVA chave (desconhecida, silenciosamente ignorada) - o coice lateral
// e o vertical simplesmente nao existiam. Quem sabe quantos valores uma chave tem
// e o callback, nao este laco, entao e ele que puxa o resto.
static qboolean WS_ParseKVBlock( char **pp, void *out,
	void (*apply)( void *out, const char *key, const char *val, char **pp ) )
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
		apply( out, k, v, pp );
	}
	return true;
}

static void WS_ApplyAmmoInfo( void *out, const char *key, const char *val, char **pp )
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

static void WS_ApplyAmmoPickup( void *out, const char *key, const char *val, char **pp )
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

static void WS_ApplyWeaponData( void *out, const char *key, const char *val, char **pp )
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
	else if( !WS_stricmp( key, "zoom_fov" ) ) w->zoom_fov = atoi( val );	// RTN F10
	else if( !WS_stricmp( key, "volume" ) ) WS_strncpy( w->volume, val, sizeof( w->volume ) );
	else if( !WS_stricmp( key, "flash" ) ) WS_strncpy( w->flash, val, sizeof( w->flash ) );
}


// Le "PunchAngle" "a..b" "c..d" "e" - pitch, yaw e roll, cada um podendo ser uma
// faixa. O primeiro componente ja veio no `val` que o WS_ParseKVBlock leu como
// "valor"; os outros dois sao puxados aqui do cursor.
//
// Substitui o antigo WS_ParseTriple(), que tentava separar os tres por aspas
// DENTRO de uma unica string - impossivel, porque o tokenizer ja tinha removido
// as aspas muito antes (ele devolve cada trecho entre aspas como um token
// proprio). Na pratica ele lia so o pitch, e os outros dois tokens voltavam para
// o laco como chave/valor desconhecidos.
//
// Guarda min e max separados em vez do meio da faixa: sortear dentro da faixa a
// cada tiro e o que produz recuo. Com o meio, "-0.5..0.5" vira 0 e o coice
// lateral desaparece por completo.
static void WS_ParsePunch( const char *val, char **pp, float *outMin, float *outMax )
{
	WS_ParseRange( val, &outMin[0], &outMax[0] );

	for( int i = 1; i < 3; i++ )
	{
		if( !pp )
		{
			outMin[i] = outMax[i] = 0.0f;
			continue;
		}

		// Guarda o cursor ANTES de ler: um script que declare PunchAngle com
		// menos de tres componentes traria aqui o '}' de fechamento do bloco, e
		// engoli-lo faria WS_ParseKVBlock continuar lendo para fora do bloco.
		// Restaurar o cursor devolve o token para o laco de fora.
		char *save = *pp;
		char *t = WS_NextToken( pp );

		if( !t || t[0] == '}' || t[0] == '{' )
		{
			*pp = save;
			outMin[i] = outMax[i] = 0.0f;
			continue;
		}

		WS_ParseRange( t, &outMin[i], &outMax[i] );
	}
}

static void WS_ApplyAttack( void *out, const char *key, const char *val, char **pp )
{
	weaponattack_t *at = (weaponattack_t *)out;
	if( !WS_stricmp( key, "action" ) ) WS_strncpy( at->action, val, sizeof( at->action ) );
	else if( !WS_stricmp( key, "nextattack" ) ) at->nextattack = atof( val );
	else if( !WS_stricmp( key, "PunchAngle" ) )
	{
		WS_ParsePunch( val, pp, at->PunchAngleMin, at->PunchAngleMax );
	}
	else if( !WS_stricmp( key, "PunchAngleIS" ) )
	{
		WS_ParsePunch( val, pp, at->PunchAngleISMin, at->PunchAngleISMax );
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

static void WS_ApplySound( void *out, const char *key, const char *val, char **pp )
{
	weaponsound_t *s = (weaponsound_t *)out;
	if( !WS_stricmp( key, "shootsound1" ) )
	{
		if( !s->shootsound1[0] ) WS_strncpy( s->shootsound1, val, sizeof( s->shootsound1 ) );
		else WS_strncpy( s->shootsound2, val, sizeof( s->shootsound2 ) );
	}
	else if( !WS_stricmp( key, "emptysound" ) ) WS_strncpy( s->emptysound, val, sizeof( s->emptysound ) );
}

static void WS_ApplySprite( void *out, const char *key, const char *val, char **pp )
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
			if( gNumAmmoInfo >= WS_MAX_ENTRIES )
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
			if( gNumAmmoPickups < WS_MAX_ENTRIES )
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
	w.id = -1; // unassigned - see WeaponScript_GetWeaponID()

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

	// Paranoia 2 compatibility clamp. The scripts we import are written against
	// Uncle Mike's HUD, which has MAX_WEAPON_SLOTS 10 (P2 game_shared/cdll_dll.h);
	// PrimeXT's is 5 (game_shared/cdll_dll.h here), and MAX_WEAPON_POSITIONS is
	// defined as MAX_WEAPON_SLOTS on the client (client/ammohistory.h). Those
	// values reach the client's WeaponsResource::rgSlots[6][6] unchecked, via
	// the WeaponList message and PickupWeapon() - so an out-of-range bucket or
	// bucket_position both writes out of bounds AND lands the weapon where
	// GetFirstPos()/GetNextActivePos() (which only walk 0..MAX_WEAPON_POSITIONS-1)
	// can never find it again: the weapon becomes unselectable in the HUD.
	// weapon_parafal.txt is a real example - it carries P2's bucket_position 6.
	// Clamping here (instead of editing the scripts) keeps stock P2 scripts
	// importable as-is, which is the whole point of the format compatibility.
	if( w.bucket < 0 || w.bucket >= MAX_WEAPON_SLOTS )
	{
		WS_Printf( "WeaponScript: bucket %d out of range (0..%d), clamping - script written for a wider HUD?\n",
			w.bucket, MAX_WEAPON_SLOTS - 1 );
		w.bucket = ( w.bucket < 0 ) ? 0 : MAX_WEAPON_SLOTS - 1;
	}
	if( w.bucket_position < 0 || w.bucket_position >= MAX_WEAPON_SLOTS )
	{
		WS_Printf( "WeaponScript: bucket_position %d out of range (0..%d), clamping - script written for a wider HUD?\n",
			w.bucket_position, MAX_WEAPON_SLOTS - 1 );
		w.bucket_position = ( w.bucket_position < 0 ) ? 0 : MAX_WEAPON_SLOTS - 1;
	}

	if( gNumWeaponInfo < WS_MAX_ENTRIES )
	{
		// store the script file basename (e.g. "weapon_mp5") for lookup by name
		// use std::filesystem::path to strip directory regardless of / or \ separator
		std::string baseName = std::filesystem::path( filename ).filename().string();
		WS_strncpy( w.scriptname, baseName.c_str(), sizeof( w.scriptname ) );
		size_t sl = strlen( w.scriptname );
		if( sl > 4 && !WS_stricmp( w.scriptname + sl - 4, ".txt" ) )
			w.scriptname[sl-4] = 0;
		gWeaponInfo[gNumWeaponInfo++] = w;
		// Report what was actually READ from the file, not just that the file was
		// found. These two are very different failures that look identical from
		// the outside: the scriptname above comes from the FILENAME, so a weapon
		// whose contents parsed to nothing still shows up "indexed" and findable
		// by name, with every field silently zeroed.
		WS_Printf( "WeaponScript: [%s] vm=[%s] bucket=%d pos=%d clip=%d ammo1=[%s] sprites=%d\n",
			w.scriptname, w.viewmodel, w.bucket, w.bucket_position,
			w.clip_size, w.primary_ammo, w.num_sprites );
		if( !w.viewmodel[0] || !w.primary_ammo[0] || w.clip_size <= 0 )
			WS_Printf( "WeaponScript: AVISO - [%s] tem campos vazios; o arquivo foi lido mas o conteudo nao foi interpretado\n",
				w.scriptname );
	}

	free( text );
	return 0;
}

void WeaponScript_LoadAll( void )
{
	gNumWeaponInfo = 0;
	gNumAmmoInfo = 0;
	gNumAmmoPickups = 0;
	// Default script locations, relative to the game directory (gamedir).
	// Matches where mods keep them: scripts/weapons/ammodesc.txt and
	// scripts/weapons/weapon_*.txt  (e.g. valve/scripts/weapons/...)
	char gamedir[256];
	GET_GAME_DIR( gamedir );
	WS_Printf( "WeaponScript: gamedir = [%s]\n", gamedir );
	std::string base = std::string( gamedir ) + "/scripts/weapons";
	WS_Printf( "WeaponScript: scanning dir [%s]\n", base.c_str() );
	// try both possible locations for ammodesc.txt
	std::string ammoPath = base + "/ammodesc.txt";
	if( !fs::FileExists( ammoPath.c_str() ) )
	{
		std::string alt = std::string( gamedir ) + "/scripts/ammodesc.txt";
		if( fs::FileExists( alt.c_str() ) )
		{
			ammoPath = alt;
			base = std::string( gamedir ) + "/scripts";
			WS_Printf( "WeaponScript: using alt ammo path [%s]\n", ammoPath.c_str() );
		}
	}
	WS_Printf( "WeaponScript: loading ammo desc from %s\n", ammoPath.c_str() );
	WeaponScript_ParseAmmoDesc( ammoPath.c_str() );
	int found = 0;
	try
	{
		for( auto &entry : std::filesystem::directory_iterator( base ) )
		{
			std::string name = entry.path().filename().string();
			if( name.rfind( "weapon_", 0 ) == 0 && name.size() > 7 && name.substr( name.size()-4 ) == ".txt" )
			{
				WeaponScript_ParseWeapon( entry.path().string().c_str() );
				found++;
			}
		}
	}
	catch( ... )
	{
		WS_Printf( "WeaponScript: could not scan %s\n", base.c_str() );
	}
	WS_Printf( "WeaponScript: %d weapon_*.txt found in [%s]\n", found, base.c_str() );
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
	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
	if( !pPlayer )
	{
		WS_Printf( "ws_give: no player found\n" );
		return;
	}
	WS_Printf( "ws_give: creating %s\n", name );
	CBaseEntity *pEnt = CreateEntityByName( name );
	if( !pEnt )
	{
		WS_Printf( "ws_give: CreateEntityByName failed for %s\n", name );
		return;
	}
	pEnt->SetAbsOrigin( pPlayer->GetAbsOrigin() );
	pEnt->pev->spawnflags |= SF_NORESPAWN;
	DispatchSpawn( pEnt->edict() );
	DispatchTouch( pEnt->edict(), pPlayer->edict() );
	WS_Printf( "ws_give: %s given to player\n", name );

	// Post-mortem of the whole give->pickup->deploy chain in one line, because
	// every failure mode downstream of here looks identical in game ("pickup
	// sound, no weapon") while having completely different causes:
	//   active != the weapon we just gave -> SwitchWeapon() bailed, i.e. either
	//     CanDeploy() was false (no ammo AND empty clip) or FShouldSwitchWeapon()
	//     refused because the currently held weapon would not holster;
	//   viewmodel empty/wrong  -> Deploy() ran but DefaultDeploy() did not set it;
	//   modelindex 0           -> the model was never precached, so the client
	//                             resolves it to "no model" and draws an empty
	//                             hand no matter how correct everything else is;
	//   everything correct     -> the server is fine and the weapon is being lost
	//                             on the client (prediction/renderer).
	CBasePlayer *plr = static_cast<CBasePlayer *>( pPlayer );
	const char *activeName = ( plr && plr->m_pActiveItem )
		? STRING( plr->m_pActiveItem->pev->classname ) : "NONE";
	const char *viewModel = ( plr && plr->pev->viewmodel ) ? STRING( plr->pev->viewmodel ) : "";
	WS_Printf( "ws_give: active=[%s] viewmodel=[%s] modelindex=%d weaponmodel=[%s]\n",
		activeName, viewModel, viewModel[0] ? MODEL_INDEX( viewModel ) : 0,
		( plr && plr->pev->weaponmodel ) ? STRING( plr->pev->weaponmodel ) : "" );

	CBasePlayerWeapon *wpn = dynamic_cast<CBasePlayerWeapon *>( pEnt );
	if( !plr || !wpn || !wpn->m_pWeaponContext )
		return;

	// CRASH (build 146): dar de novo uma arma que o jogador JA tem cai no ramo de
	// duplicata de CBasePlayer::AddPlayerItem() (server/player.cpp) - ele credita a
	// municao via AddDuplicate(), agenda a entidade para remocao e retorna FALSE
	// SEM nunca chamar AddToPlayer(), que e o unico lugar que preenche m_pPlayer.
	// Esta entidade fica portanto com m_pPlayer == NULL, e o diagnostico abaixo
	// chama wpn->CanDeploy() -> CBaseWeaponContext::CanDeploy() ->
	// m_pLayer->GetPlayerAmmo() -> m_pWeapon->m_pPlayer->m_rgAmmo[...] -> deref de
	// NULL -> Sys_Crash C0000005 dentro do Cmd_ExecuteString do console.
	//
	// Nao e erro: e o caminho NORMAL de "peguei mais municao". A munica ja foi
	// creditada em pInsert (a arma que o jogador realmente carrega) antes de
	// chegarmos aqui, entao so ha o que relatar - nada a diagnosticar nesta
	// entidade, que ja esta morta.
	if( !wpn->m_pPlayer )
	{
		WS_Printf( "ws_give: [%s] ja estava no inventario - municao creditada na arma existente (esta copia foi descartada)\n", name );
		return;
	}

	CBaseWeaponContext *ctx = wpn->m_pWeaponContext.get();
	// ItemInfoArray[] is the shared table W_Precache() fills; CanDeploy() and the
	// ammo bookkeeping read the weapon's stats from it, NOT from the script - so
	// an empty row here means the weapon was never registered and every one of
	// those reads silently returns zero/NULL.
	const ItemInfo &reg = CBaseWeaponContext::ItemInfoArray[ctx->m_iId];
	WS_Printf( "ws_give: ctx id=%d clip=%d defaultammo=%d ammotype=%d slot=%d candeploy=%d inventory=%d\n",
		ctx->m_iId, ctx->m_iClip, ctx->m_iDefaultAmmo, ctx->m_iPrimaryAmmoType,
		wpn->iItemSlot(), wpn->CanDeploy() ? 1 : 0, plr->HasPlayerItem( wpn ) ? 1 : 0 );
	WS_Printf( "ws_give: ItemInfoArray[%d] name=[%s] ammo1=[%s] maxclip=%d maxammo1=%d id=%d\n",
		ctx->m_iId, reg.pszName ? reg.pszName : "NULL", reg.pszAmmo1 ? reg.pszAmmo1 : "NULL",
		reg.iMaxClip, reg.iMaxAmmo1, reg.iId );

	// The parsed script entry itself, read straight out of gWeaponInfo instead of
	// inferred from ItemInfoArray. Everything above is a COPY made at Spawn time;
	// if the two disagree the copy is stale, and if they agree that the fields are
	// empty then the parser never filled them - which points at the script file,
	// not at the weapon code.
	const weaponinfo_t *src = WeaponScript_FindWeaponByName( name );
	if( src )
	{
		WS_Printf( "ws_give: gWeaponInfo[%s] vm=[%s] pm=[%s] wm=[%s]\n",
			src->scriptname, src->viewmodel, src->playermodel, src->worldmodel );
		WS_Printf( "ws_give: gWeaponInfo[%s] bucket=%d pos=%d clip=%d defammo=%d ammo1=[%s] id=%d\n",
			src->scriptname, src->bucket, src->bucket_position, src->clip_size,
			src->defaultammo, src->primary_ammo, src->id );
	}
	else
	{
		WS_Printf( "ws_give: gWeaponInfo NAO tem entrada para [%s]\n", name );
	}

	// Last-resort equip. CanDeploy() gates SwitchWeapon() on "has any ammo at
	// all", and a script weapon whose clip never got filled fails it silently -
	// picked up, never equipped, no console word about it. Refill from the
	// script's own clip_size and equip directly, so the weapon reaches the
	// screen even when the ammo path is still wrong. This is a diagnostic
	// crutch on a debug-only console command (ws_give), not a fix for the
	// underlying bookkeeping - the printout above is what tells us what to fix.
	if( !plr->m_pActiveItem && plr->HasPlayerItem( wpn ) )
	{
		if( ctx->m_iClip <= 0 && wpn->iMaxClip() > 0 )
		{
			ctx->m_iClip = wpn->iMaxClip();
			WS_Printf( "ws_give: clip estava vazio - preenchido com %d do script\n", ctx->m_iClip );
		}
		if( !plr->SwitchWeapon( wpn ) )
		{
			WS_Printf( "ws_give: SwitchWeapon recusou (CanDeploy=%d) - equipando na marra\n",
				wpn->CanDeploy() ? 1 : 0 );
			plr->m_pActiveItem = wpn;
			wpn->Deploy();
		}
		const char *vm2 = plr->pev->viewmodel ? STRING( plr->pev->viewmodel ) : "";
		WS_Printf( "ws_give: apos forcar -> active=[%s] viewmodel=[%s] modelindex=%d\n",
			plr->m_pActiveItem ? STRING( plr->m_pActiveItem->pev->classname ) : "NONE",
			vm2, vm2[0] ? MODEL_INDEX( vm2 ) : 0 );
	}
}

void WeaponScript_Init( void )
{
	// Stamp which server.dll is actually loaded. Two test rounds were spent on a
	// log that looked unchanged, with no way to tell "the fix did not work" apart
	// from "the new DLL was never loaded" - this removes that ambiguity for good.
	// XASH_BUILD_COMMIT is the git describe, injected by the root CMakeLists.txt.
#ifdef XASH_BUILD_COMMIT
	WS_Printf( "WeaponScript: server.dll build [%s]\n", XASH_BUILD_COMMIT );
#else
	WS_Printf( "WeaponScript: server.dll build [desconhecido]\n" );
#endif
	g_engfuncs.pfnAddServerCommand( "weaponscript_reload", WeaponScript_Reload_f );
	g_engfuncs.pfnAddServerCommand( "weaponscript_list", WeaponScript_List_f );
	g_engfuncs.pfnAddServerCommand( "ws_give", WeaponScript_Give_f );
	WeaponScript_LoadAll();
	WS_Printf( "WeaponScript: auto-loaded %d weapons at startup\n", gNumWeaponInfo );
}

// defined in server/weapons.cpp, no header declares it - it's only ever called
// from W_Precache() (same file) and from here.
extern void AddAmmoNameToAmmoRegistry( const char *szAmmoname );

void WeaponScript_RegisterAmmoTypes( void )
{
	int registered = 0;
	for( int i = 0; i < gNumAmmoInfo; i++ )
	{
		if( gAmmoInfo[i].name[0] )
		{
			AddAmmoNameToAmmoRegistry( gAmmoInfo[i].name );
			registered++;
		}
	}
	WS_Printf( "WeaponScript: registered %d ammo types from ammodesc.txt into the engine ammo registry\n", registered );
}

int WeaponScript_GetWeaponID( weaponinfo_t *info )
{
	static int nextId = WEAPON_SCRIPT_ID_BASE;

	if( !info )
		return WEAPON_SCRIPT_ID_BASE; // shouldn't happen - safe fallback bucket

	if( info->id >= 0 )
		return info->id; // already assigned (Paranoia2's FindWeaponID() equivalent)

	if( nextId > WEAPON_SCRIPT_ID_MAX )
	{
		WS_Printf( "WeaponScript: out of script weapon IDs (max %d), reusing %d for [%s]\n",
			WEAPON_SCRIPT_ID_MAX - WEAPON_SCRIPT_ID_BASE + 1, WEAPON_SCRIPT_ID_MAX, info->scriptname );
		info->id = WEAPON_SCRIPT_ID_MAX;
		return info->id;
	}

	info->id = nextId++;
	WS_Printf( "WeaponScript: assigned id %d to [%s]\n", info->id, info->scriptname );
	return info->id;
}
