/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#ifndef AMMO_H
#define AMMO_H

#include "texture_handle.h"	// RTN: TextureHandle - fallback .tga do icone da barra de selecao

#define MAX_WEAPON_NAME		128
#define MAX_WEAPON_SLOTS		5 // hud item selection slots
#define WEAPON_FLAGS_SELECTONEMPTY	1

#define WEAPON_IS_ONTARGET		0x40

struct WEAPON
{
	char	szName[MAX_WEAPON_NAME];
	int	iAmmoType;
	int	iAmmo2Type;
	int	iMax1;
	int	iMax2;
	int	iSlot;
	int	iSlotPos;
	int	iFlags;
	int	iId;
	int	iClip;
	int	iWeight;	// RTN: peso (script "weight" / ItemInfo::iWeight) - ordena a barra de selecao nova (leve->pesada)

	int	iCount;		// # of itesm in plist

	// RTN: icone da barra de selecao nova (DrawWeaponSelectBar, client/ammo.cpp),
	// mesma convencao/ordem de fallback do CHudWeaponBox:
	//   1) sprites/rtn_hud_ammo_<classname>.spr
	//   2) gfx/vgui/ammo/640_<classname>.tga (textura crua, sem precisar converter pra .spr)
	// Sem os dois, mostra so o nome em texto. Carregado uma vez so, na
	// primeira vez que a arma aparece na barra.
	SpriteHandle	hBoxSpr;
	TextureHandle	hBoxTex;
	bool	bBoxIconLoaded;

	SpriteHandle	hActive;
	wrect_t	rcActive;
	SpriteHandle	hInactive;
	wrect_t	rcInactive;
	SpriteHandle	hAmmo;
	wrect_t	rcAmmo;
	SpriteHandle	hAmmo2;
	wrect_t	rcAmmo2;
	SpriteHandle	hCrosshair;
	wrect_t	rcCrosshair;
	SpriteHandle	hAutoaim;
	wrect_t	rcAutoaim;
	SpriteHandle	hZoomedCrosshair;
	wrect_t	rcZoomedCrosshair;
	SpriteHandle	hZoomedAutoaim;
	wrect_t	rcZoomedAutoaim;
};

#endif//AMMO_H
