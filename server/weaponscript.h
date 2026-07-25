/*
 * weaponscript.h - data-driven weapon/ammo script system (Xash Weapon System)
 * Copyright (C) 2026 Brother Hermes - sistema de armas por script, por Hermes e Hidrocarboneto
 * Road to Nowhere project (Paranoia 2 based mod)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This header mirrors the script format used by Uncle Mike in Paranoia 2
 * (ammodesc.txt and weapon_*.txt) so existing mods can be imported without
 * changing their script files.
 */

#ifndef WEAPONSCRIPT_H
#define WEAPONSCRIPT_H

#include <cstdint>

#define MAX_AMMO_TYPES		64
#define MAX_WEAPON_SPRITES	8

typedef struct ammoinfo_s
{
	char name[32];
	int	MaxCarry;
	int	PlayerDamage;
	int	MonsterDamage;
	int	Damage;
	float Distance;
	int	NumShots;
	char ShellModel[64];
	char Missile[32];
	int	count;
} ammoinfo_t;

typedef struct ammopickup_s
{
	char classname[64];
	char type[32];
	int	count;
} ammopickup_t;

typedef struct
{
	float SpreadRange[2];
	float SpreadExpand;
	float SpreadRangeIS[2];
	float SpreadExpandIS;
	float PunchAngle[3];
	float PunchAngleIS[3];
} weaponattack_t;

typedef struct
{
	char action[64];
	float nextattack;
	weaponattack_t at;
} weaponsound_t;

typedef struct
{
	char name[32];
	char file[64];
	int	x, y, width, height;
} weaponsprite_t;

typedef struct
{
	char viewmodel[64];
	char playermodel[64];
	char worldmodel[64];
	char anim_prefix[16];
	int	bucket;
	int	bucket_position;
	int	clip_size;
	int	defaultammo;
	char primary_ammo[32];
	char secondary_ammo[32];
	int	weight;
	float SpreadTime;
	int	item_flags;
	float MaxSpeed;
	float MaxSpeedIS;
	int	num_sprites;
	weaponsprite_t sprites[MAX_WEAPON_SPRITES];
	weaponattack_t primary;
	weaponattack_t secondary;
	weaponsound_t sound;
} weaponinfo_t;

extern weaponinfo_t gWeaponInfo[MAX_AMMO_TYPES];
extern ammoinfo_t gAmmoInfo[MAX_AMMO_TYPES];
extern ammopickup_t gAmmoPickups[MAX_AMMO_TYPES];
extern int gNumWeaponInfo;
extern int gNumAmmoInfo;
extern int gNumAmmoPickups;

ammoinfo_t *WeaponScript_FindAmmo( const char *name );
weaponinfo_t *WeaponScript_FindWeapon( const char *name );
void WeaponScript_Init( void );
void WeaponScript_LoadAll( void );

#endif // WEAPONSCRIPT_H
