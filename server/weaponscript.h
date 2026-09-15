/*
weaponscript.h - data-driven weapon/ammo script system (Xash Weapon System)
Copyright (C) 2026 Brother Hermes - sistema de armas por script, por Hermes e Hidrocarboneto

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This header mirrors the script format used by Uncle Mike in Paranoia 2
(ammodesc.txt and weapon_*.txt) so existing mods can be imported without
changing their script files.
*/

#ifndef WEAPONSCRIPT_H
#define WEAPONSCRIPT_H

#include "const.h"

// Deliberately NOT called MAX_AMMO_TYPES: game_shared/cdll_dll.h already defines
// that name as 32, so the old "#ifndef MAX_AMMO_TYPES / #define 64" here resolved
// to 32 or 64 depending purely on whether extdll.h happened to be included before
// this header in a given translation unit - i.e. the same global arrays were
// declared with different extents across the build (an ODR violation).
// game_shared/weapons/mp5.cpp really does include this header first, so both
// values were live in the same binary.
#define WS_MAX_ENTRIES		64
#ifndef MAX_WEAPON_SPRITES
#define MAX_WEAPON_SPRITES	8
#endif
#define MAX_WEAPON_NAME		64
#define MAX_WEAPON_MODELS	64
#define MAX_AMMO_NAME		32
#define MAX_SOUND_PATH		64
#define MAX_HUDSPRITE_NAME	32

// RTN weaponscript: bits de item_flags que PRECISAM ser identicos nos dois
// lados (cliente prediz a arma sem ter o parser - ver o comentario grande em
// mp5.h sobre m_pScriptInfo). Por isso viajam pela rede dentro de
// weapon_data_t.iuser2 (m_iScriptFlags), que e DT_SIGNED de 10 bits no
// delta.lst (game_dir/delta.lst) - faixa segura de valores POSITIVOS e
// 0..511 (bit 8 e o ultimo garantido). So ganhe um bit novo aqui se o
// comportamento precisar ser identico durante a predicao (ex: bloquear tiro
// debaixo d'agua muda o que o cliente prediz na hora). Comportamento que so
// importa pro SERVIDOR (drop, respawn no mundo, duplicata) vai no OUTRO
// enum, ITEM_FLAG_* de game_shared/item_info.h, guardado em
// weaponinfo_t::inventory_flags - NUNCA misturar os dois (colidem em valor
// de proposito, ver aviso completo mais abaixo e em mp5.h).
#define WIF_IRONSIGHT		(1<<0)	// 1   - mira de ferro (CMP5WeaponContext::HasIronSight)
#define WIF_AUTOAIM			(1<<1)	// 2   - crosshair de auto-mira cosmetico (WeaponIdle) - NUNCA usado em FireBullets, ver F10 fix em mp5.cpp
#define WIF_AUTOFIRE		(1<<2)	// 4   - parseado, sem efeito de disparo ainda (RTN nao tem semi-auto pra distinguir de - ver DEVKIT_WEAPONS.md)
#define WIF_SELECTONEMPTY	(1<<3)	// 8   - reservado; SelectOnEmpty hoje e sempre ligado pra arma de script (ver mp5.cpp GetItemInfo)
#define WIF_NOAUTORELOAD	(1<<4)	// 16  - suprime o auto-reload de CBaseWeaponContext (weapon_context.cpp)
#define WIF_NOAUTOSWITCH	(1<<5)	// 32  - suprime a troca automatica de arma ao zerar municao (weapon_context.cpp)
#define WIF_UNDERWATER		(1<<6)	// 64  - permite atirar debaixo d'agua (mp5.cpp PrimaryAttack, por padrao bloqueado)

typedef struct ammoinfo_s
{
	char	name[MAX_AMMO_NAME];
	int	MaxCarry;
	int	PlayerDamage;
	int	MonsterDamage;
	int	Damage;
	float	Distance;
	int	NumShots;
	char	ShellModel[MAX_WEAPON_MODELS];
	char	Missile[MAX_AMMO_NAME];
	int	count;
} ammoinfo_t;

typedef struct ammopickup_s
{
	char	classname[MAX_WEAPON_NAME];
	char	model[MAX_WEAPON_MODELS];
	char	sound[MAX_SOUND_PATH];
	char	type[MAX_AMMO_NAME];
	int	count;
} ammopickup_t;

typedef struct weaponattack_s
{
	char	action[32];
	float	nextattack;
	// Recuo como FAIXA, nao como valor unico. O script escreve
	// "PunchAngle" "1..1.2" "-0.5..0.5" "0" - tres componentes, cada um podendo
	// ser uma faixa. Guardar so o meio da faixa (o que se fazia antes) apagava
	// justamente a variacao por tiro, que e o que da sensacao de recuo: o
	// "-0.5..0.5" do yaw tem meio ZERO, ou seja, o coice lateral sumia inteiro.
	float	PunchAngleMin[3];
	float	PunchAngleMax[3];
	float	PunchAngleISMin[3];
	float	PunchAngleISMax[3];
	float	SpreadRange[2];
	float	SpreadExpand;
	float	SpreadRangeIS[2];
	float	SpreadExpandIS;
} weaponattack_t;

typedef struct weaponsound_s
{
	char	shootsound1[MAX_SOUND_PATH];
	char	shootsound2[MAX_SOUND_PATH];
	char	emptysound[MAX_SOUND_PATH];
} weaponsound_t;

typedef struct weaponsprite_s
{
	char	name[MAX_HUDSPRITE_NAME];
	char	file[MAX_SOUND_PATH];
	int	x, y, width, height;
} weaponsprite_t;

typedef struct weaponinfo_s
{
	char	viewmodel[MAX_WEAPON_MODELS];
	char	playermodel[MAX_WEAPON_MODELS];
	char	worldmodel[MAX_WEAPON_MODELS];
	char	anim_prefix[32];
	int	bucket;
	int	bucket_position;
	int	clip_size;
	int	defaultammo;
	char	primary_ammo[MAX_AMMO_NAME];
	char	secondary_ammo[MAX_AMMO_NAME];
	int	weight;
	float	SpreadTime;
	int	item_flags;		// WIF_* (weaponscript.h) - bits que precisam de predicao identica nos dois lados
	int	inventory_flags;	// ITEM_FLAG_* (item_info.h) - bits so-servidor (drop/respawn/duplicata), NUNCA misturar com item_flags
	float	MaxSpeed;
	float	MaxSpeedIS;
	int	zoom_fov;			// RTN F10: FOV da mira de ferro (0 = sem mira)
	char	volume[16];
	char	flash[16];

	weaponattack_t	primary;
	weaponattack_t	secondary;
	weaponsound_t	sound;
	weaponsprite_t	sprites[MAX_WEAPON_SPRITES];
	int	num_sprites;
	char	scriptname[64];

	// Dynamic WEAPON_* id (see WeaponScript_GetWeaponID()/WEAPON_SCRIPT_ID_BASE
	// below) - -1 until first assigned, then stable for the process lifetime
	// (gWeaponInfo is only reloaded once, at WeaponScript_Init(), not per map).
	int	id;
} weaponinfo_t;

extern ammoinfo_t		gAmmoInfo[WS_MAX_ENTRIES];
extern int			gNumAmmoInfo;
extern ammopickup_t		gAmmoPickups[WS_MAX_ENTRIES];
extern int			gNumAmmoPickups;
extern weaponinfo_t		gWeaponInfo[WS_MAX_ENTRIES];
extern int			gNumWeaponInfo;

ammoinfo_t *WeaponScript_FindAmmo( const char *name );
int  WeaponScript_ParseAmmoDesc( const char *filename );
int  WeaponScript_ParseWeapon( const char *filename );
void WeaponScript_LoadAll( void );

ammoinfo_t *WeaponScript_FindAmmo( const char *name );
weaponinfo_t *WeaponScript_FindWeapon( const char *name );
weaponinfo_t *WeaponScript_FindWeaponByName( const char *scriptname );
void WeaponScript_Init( void );

// Registers every ammo type parsed from ammodesc.txt (gAmmoInfo) into the
// engine's own ammo registry (AmmoInfoArray, via AddAmmoNameToAmmoRegistry -
// see server/weapons.cpp), so CBasePlayer::GetAmmoIndex()/GiveAmmo() actually
// recognize script-only ammo names (e.g. "7.62", not just the classic hardcoded
// ones like "9mm"). Without this, any weapon whose primary_ammo isn't already
// registered by one of the classic hardcoded weapons gets ammo type index -1:
// GiveAmmo() silently fails (reserve ammo never gets credited, HUD ammo count
// stays blank), even though the weapon can still fire whatever's in the clip.
// Must run AFTER W_Precache() on every map load, since W_Precache() wipes
// AmmoInfoArray clean each time - see the call site in server/world.cpp.
void WeaponScript_RegisterAmmoTypes( void );

// Assigns (or returns the already-assigned) unique WEAPON_* id for a parsed
// script weapon - mirrors Paranoia2_original's CBasePlayerItem::GenerateID()/
// FindWeaponID() (dlls/weapons.cpp). Script weapons must NOT reuse a classic
// hardcoded WEAPON_* constant (e.g. WEAPON_MP5): doing so makes them collide
// in CBaseWeaponContext::ItemInfoArray[m_iId] with whichever real weapon owns
// that id (last one precached each map load wins), which is what broke
// CanDeploy()/pszAmmo1()/pszAmmo2()/iItemPosition() for every script weapon
// reusing CMP5WeaponContext. The range below starts right after the highest
// classic WEAPON_* (WEAPON_STIMULANT=30, see game_shared/weapons/stimulant.h)
// and stays clear of WEAPON_SUIT (MAX_WEAPONS-1=63, reserved).
// Defined identically in game_shared/weapons/mp5.h (client + server visible,
// since client/weapon_predicting_context.cpp also needs the range) - keep
// both definitions in sync if this ever changes.
#ifndef WEAPON_SCRIPT_ID_BASE
#define WEAPON_SCRIPT_ID_BASE	31
// O teto vinha do PROTOCOLO, nao do engine: game_dir/delta.lst codificava
// clientdata_t.m_iId e weapon_data_t.m_iId com 5 bits, entao 31 era o maior id
// que sobrevivia na rede - e como a BASE tambem e 31, so cabia UMA arma de
// script. Todas as demais caiam no ramo "out of script weapon IDs, reusing 31" e
// dividiam a mesma linha de ItemInfoArray: quem fosse precacheado por ultimo
// vencia, e as outras liam clip/municao/slot da arma errada.
//
// Os dois DEFINE_DELTA foram ampliados para 6 bits, o que casa com o engine
// (MAX_WEAPON_BITS 6 -> 64 armas previsiveis, verificado em
// engine/common/protocol.h do fork). Agora cabem 32 armas de script (31..62);
// 63 e WEAPON_SUIT e fica reservado.
//
// ATENCAO: delta.lst e DADO DO MOD, nao do engine. Cliente e servidor precisam
// carregar a MESMA versao do arquivo - se divergirem, todo campo depois do
// m_iId desliza um bit e o sintoma nao parece "id errado", e sim vida, camera,
// FOV e inclinacao errados ao mesmo tempo. Ao distribuir, mandar game_dir e
// dlls juntos.
#define WEAPON_SCRIPT_ID_MAX	62
#endif
int WeaponScript_GetWeaponID( weaponinfo_t *info );

// Registra cada arma parseada como uma CLASSE DE ENTIDADE, para
// CreateEntityByName("weapon_<nome>") funcionar sem um LINK_ENTITY_TO_CLASS
// escrito a mao para cada uma - ver o comentario grande em
// server/weapons/weapon_scripted.cpp. Chamada uma vez, logo apos o parse.
void WeaponScript_RegisterEntities( void );

#endif // WEAPONSCRIPT_H
