#include "nvg.h"

#ifdef CLIENT_DLL
#else
#include "extdll.h"
#include "enginecallback.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "user_messages.h"
#endif

CNVGWeaponContext::CNVGWeaponContext(std::unique_ptr<IWeaponLayer> &&layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_NVG;
	m_iClip = 1;             // 1 "charge" per pickup
	m_iPrimaryAmmoType = -1;
	m_iSecondaryAmmoType = -1;
	m_flBattery = NVG_BATTERY_MAX;
}

int CNVGWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = "item_nvg";
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 1;
	p->iSlot = 1;
	p->iPosition = 4;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iId = WEAPON_NVG;
	p->iWeight = 5;
	return 1;
}

bool CNVGWeaponContext::Deploy()
{
	// NVG: no fancy viewmodel needed, just the effect. Use a generic deploy.
#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	if( player )
	{
		g_engfuncs.pfnClientCommand( player->edict(), "cl_viewmodel_fov 80\n" );
	}
#endif
	return DefaultDeploy( "models/v_antidote.mdl", "models/w_antidote.mdl", 2, "medkit" );
}

// Toggle NVG on/off
void CNVGWeaponContext::PrimaryAttack()
{
	ApplyNVGState( !m_bNVGActive );

	// brief cooldown so one click = one toggle
	m_flNextPrimaryAttack = m_pLayer->GetTime() + 0.35f;
	m_flTimeWeaponIdle = m_pLayer->GetTime() + 0.5f;
}

// Drain battery while active; auto-shutoff when empty
void CNVGWeaponContext::ItemPostFrame()
{
	if( m_bNVGActive )
	{
#ifndef CLIENT_DLL
		m_flBattery -= gpGlobals->frametime;
#else
		m_flBattery -= 0.01f;
#endif
		if( m_flBattery <= 0.0f )
		{
			m_flBattery = 0.0f;
			ApplyNVGState( false );  // battery dead -> shutdown
		}
	}

	CBaseWeaponContext::ItemPostFrame();
}

void CNVGWeaponContext::WeaponIdle()
{
	ResetEmptySound();
	m_flTimeWeaponIdle = m_pLayer->GetTime() + 5.0f;
}

// RTN Fase 8: sends post-FX params to the client (native postprocessing shader).
// NVG look: desaturated + green palette + aggressive contrast + gain + film grain + vignette.
void CNVGWeaponContext::ApplyNVGState(bool active)
{
#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	if( !player )
		return;

	if( active && m_flBattery <= 0.0f )
		return;  // dead battery, can't enable

	m_bNVGActive = active;

	MESSAGE_BEGIN( MSG_ONE, gmsgPostFxSettings, NULL, player->edict() );

	if( active )
	{
		// fade-in time
		WRITE_FLOAT( 0.25f );
		// brightness (IR gain)
		WRITE_FLOAT( 0.12f );
		// saturation 0 = full desaturation (monochrome)
		WRITE_FLOAT( 0.0f );
		// contrast (crush blacks, boost highlights)
		WRITE_FLOAT( 1.45f );
		// red level (kill red -> green palette)
		WRITE_FLOAT( 0.15f );
		// green level (dominant)
		WRITE_FLOAT( 1.15f );
		// blue level (kill blue)
		WRITE_FLOAT( 0.2f );
		// vignette (lens edge darkening)
		WRITE_FLOAT( 1.0f );
		// film grain (analog sensor noise, animated in shader)
		WRITE_FLOAT( 0.8f );
		// color accent scale (0 = no accent)
		WRITE_FLOAT( 0.0f );
		// accent color (unused, white)
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
	}
	else
	{
		// restore defaults (no effect)
		WRITE_FLOAT( 0.25f );
		WRITE_FLOAT( 0.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 0.0f );
		WRITE_FLOAT( 0.0f );
		WRITE_FLOAT( 0.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
		WRITE_FLOAT( 1.0f );
	}

	MESSAGE_END();
#endif
}
