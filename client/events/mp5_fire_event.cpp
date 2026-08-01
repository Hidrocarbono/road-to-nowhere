/*
mp5_fire_event.cpp
Copyright (C) 2025 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "mp5_fire_event.h"
#include "game_event_utils.h"
#include "hud.h"
#include "const.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"
#include "weapons/mp5.h"
#include "gl_aurora.h"
#include "r_efx.h"
#include "dlight.h"

CMP5FireEvent::CMP5FireEvent(event_args_t *args) :
	CBaseGameEvent(args)
{
}

void CMP5FireEvent::Execute(bool secondary)
{
	if (!secondary)
		HandleShot();
	else
		HandleGrenadeLaunch();
}

void CMP5FireEvent::HandleShot()
{
	matrix3x3 cameraMatrix(GetAngles());
	Vector up = cameraMatrix.GetUp();
	Vector right = cameraMatrix.GetRight();
	Vector forward = cameraMatrix.GetForward();

	if (IsEventLocal())
	{
		GameEventUtils::SpawnMuzzleflash();

		// RTN: smoke puff on the barrel (Paranoia 2 style) - weapon_hot.aur exists in game_dir/particles
		cl_entity_t *viewEnt = gEngfuncs.GetViewModel();
		if( viewEnt )
			UTIL_CreateAurora( viewEnt, "particles/weapon_hot.aur", 1, 0.4f );

		// RTN: dynamic muzzle light (dlight) - brief flash that lights the wall
		dlight_t *muzzleLight = gEngfuncs.pEfxAPI->CL_AllocDlight( m_arguments->entindex );
		if( muzzleLight )
		{
			Vector lightOrigin = GetOrigin() + forward * 30.0f + up * 2.0f;
			muzzleLight->origin = lightOrigin;
			muzzleLight->radius = 120.0f;
			muzzleLight->color.r = 255;
			muzzleLight->color.g = 200;
			muzzleLight->color.b = 100;
			muzzleLight->die = gEngfuncs.GetClientTime() + 0.08f;
			muzzleLight->decay = 1500.0f;
			muzzleLight->minlight = 16.0f;
		}

		if( m_arguments->bparam1 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_ANIM_SHOOT1_AIM + gEngfuncs.pfnRandomLong(0,2), 2 );
		else
			gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_FIRE1 + gEngfuncs.pfnRandomLong(0,2), 2 );
	}

	int brassModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl");
	Vector shellVelocity = GetVelocity() + right * gEngfuncs.pfnRandomFloat(50, 70) + up * gEngfuncs.pfnRandomFloat(100, 150) + forward * 25.0f;
	Vector shellOrigin = GetOrigin() + up * -12.0f + forward * 20.0f + right * 4.0f;

	GameEventUtils::EjectBrass(shellOrigin, GetAngles(), shellVelocity, brassModelIndex, TE_BOUNCE_SHELL);
	GameEventUtils::FireBullet(m_arguments->entindex, cameraMatrix, GetOrigin(), GetShootDirection(cameraMatrix), 2);

	const char *soundName = gEngfuncs.pfnRandomLong(0, 1) == 0 ? "weapons/hks1.wav" : "weapons/hks2.wav";
	gEngfuncs.pEventAPI->EV_PlaySound( GetEntityIndex(), GetOrigin(), CHAN_WEAPON, soundName, 1.f, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 15));
}

void CMP5FireEvent::HandleGrenadeLaunch()
{
	if (IsEventLocal()) {
		gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_LAUNCH, 2 );
	}

	const char *soundName = gEngfuncs.pfnRandomLong(0, 1) == 0 ? "weapons/glauncher.wav" : "weapons/glauncher2.wav";
	gEngfuncs.pEventAPI->EV_PlaySound( GetEntityIndex(), GetOrigin(), CHAN_WEAPON, soundName, 1.f, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong(0, 15));
}

Vector CMP5FireEvent::GetShootDirection(const matrix3x3 &camera) const
{
	return camera.GetForward() + m_arguments->fparam1 * camera.GetRight() + m_arguments->fparam2 * camera.GetUp();
}
