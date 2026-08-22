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
#include "weapons/weapon_activity.h"
#include "const.h"

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
		// RTN: efeitos de tiro (flash 3D + dlight + fumaça) sao 100% nativos via
		// evento 5001 no QC do viewmodel (HUD_StudioEvent -> HUD_MuzzleFlash + DlightFlash + GunSmoke),
		// igual ao Paranoia 2. SpawnMuzzleflash() = flash de sprite 2D complementar.
		GameEventUtils::SpawnMuzzleflash();

		// RTN weapon-script: a animacao de disparo e resolvida por ACTIVITY no
		// modelo que esta na tela, nao mais por indice fixo do v_mp5.mdl.
		//
		// Isto e o que faz o muzzle flash e a fumaca voltarem: o evento 5001 esta
		// gravado nas sequencias de shoot do .mdl (HUD_StudioEvent -> HUD_MuzzleFlash
		// + DlightFlash + GunSmoke, client/entity.cpp). Com o indice fixo,
		// MP5_FIRE1(=1) caia no RELOAD do v_parafal.mdl e o 5001 nunca disparava -
		// so o som saia, porque o som e um EV_PlaySound direto, fora da animacao.
		//
		// O sorteio usa a quantidade REAL de variantes daquela activity no modelo
		// (a FAL tem 3 shoot e 3 shoot_aim, mas outra arma pode ter 1 ou 5) em vez
		// do random(0,2) fixo, que so estava certo por coincidencia na MP5.
		const int shootActivity = m_arguments->bparam1 ? WACT_SHOOT_AIM : WACT_SHOOT;
		const int fallbackSeq = m_arguments->bparam1 ? MP5_ANIM_SHOOT1_AIM : MP5_FIRE1;

		// IEngineStudio ja chega aqui por mp5_fire_event.h -> base_game_event.h -> gl_local.h
		const void *vmHeader = NULL;
		cl_entity_t *vm = gEngfuncs.GetViewModel();
		if( vm && vm->model && IEngineStudio.Mod_Extradata )
			vmHeader = IEngineStudio.Mod_Extradata( vm->model );

		const int variants = WeaponActivity_Count( vmHeader, shootActivity );
		int shootSeq;

		if( variants > 0 )
			shootSeq = WeaponActivity_Lookup( vmHeader, shootActivity, gEngfuncs.pfnRandomLong( 0, variants - 1 ));
		else
			shootSeq = fallbackSeq + gEngfuncs.pfnRandomLong( 0, 2 );	// modelo classico sem activity (v_mp5.mdl)

		gEngfuncs.pEventAPI->EV_WeaponAnimation( shootSeq, 2 );
	}

	int brassModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex("models/shell.mdl");
	Vector shellVelocity = GetVelocity() + right * gEngfuncs.pfnRandomFloat(50, 70) + up * gEngfuncs.pfnRandomFloat(100, 150) + forward * 25.0f;
	Vector shellOrigin = GetOrigin() + up * -12.0f + forward * 20.0f + right * 4.0f;

	GameEventUtils::EjectBrass(shellOrigin, GetAngles(), shellVelocity, brassModelIndex, TE_BOUNCE_SHELL);

	// RTN F10 fix: origem do TRACANTE = ponta do cano (attachment[0] do viewmodel).
	// Antes o tracante nascia no olho (centro da tela) - agora nasce no mesmo
	// ponto da fumaca/flash (evento 5001), alinhado no ironsight e no lean.
	Vector muzzleOrigin = GetOrigin();
	cl_entity_t *viewModel = gEngfuncs.GetViewModel();
	if (viewModel && viewModel->model)
	{
		// attachment[0] esta em coordenadas de mundo (calculado pelo renderer)
		if (viewModel->attachment[0].Length() > 0.01f)
			muzzleOrigin = Vector(viewModel->attachment[0]);
	}
	// fallback: se o modelo nao tem attachment, offset classico do HL (direita+baixo)
	if (muzzleOrigin == GetOrigin())
		muzzleOrigin = GetOrigin() + cameraMatrix.GetForward() * 8.0f + cameraMatrix.GetRight() * 8.0f + cameraMatrix.GetUp() * -4.0f;

	// RTN DEBUG ironsight: mostra o que o tracante usa (remover depois)
	if( IsEventLocal() && viewModel && viewModel->model )
	{
		gEngfuncs.pfnConsolePrint( va( "[RTN] tracer origin=(%.1f,%.1f,%.1f) attach=(%.1f,%.1f,%.1f) len=%.2f fov=%.0f\n",
			muzzleOrigin.x, muzzleOrigin.y, muzzleOrigin.z,
			viewModel->attachment[0].x, viewModel->attachment[0].y, viewModel->attachment[0].z,
			viewModel->attachment[0].Length(),
			gEngfuncs.pfnGetCvarFloat( "cl_viewmodel_fov" ) ) );
	}


	GameEventUtils::FireBullet(m_arguments->entindex, cameraMatrix, GetOrigin(), muzzleOrigin, GetShootDirection(cameraMatrix), 2);

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
