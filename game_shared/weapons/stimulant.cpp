#include "stimulant.h"

#ifdef CLIENT_DLL
#else
#include "extdll.h"
#include "enginecallback.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "client.h"
// RTN F9: atualiza o HUD lateral de doses (declarada em server/client.cpp)
void SendRTNItemsHUD( CBasePlayer *pPlayer );
#endif

CStimulantWeaponContext::CStimulantWeaponContext(std::unique_ptr<IWeaponLayer> &&layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_STIMULANT;  // real ID (1<<-1 was UB and corrupted weapons bitfield!)
	m_iClip = 1; // ready to use (1 use)
	m_iPrimaryAmmoType = -1;
	m_iSecondaryAmmoType = -1;
}

int CStimulantWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = "item_stimulant";
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 1;
	p->iSlot = 1;
	p->iPosition = 5;  // pos 6 quebrava o ciclo do scroll (padrao HL: 1-5)
	p->iFlags = ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iId = WEAPON_STIMULANT;
	p->iWeight = 5;
	return 1;
}

bool CStimulantWeaponContext::Deploy()
{
	// RTN: viewmodel bem afastada do rosto (user pediu 80)
#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	if( player )
		g_engfuncs.pfnClientCommand( player->edict(), "cl_viewmodel_fov 80\n" );
#endif
	return DefaultDeploy( "models/v_antidote.mdl", "models/w_antidote.mdl", 2, "medkit" );  // anim 2 = draw
}

void CStimulantWeaponContext::PrimaryAttack()
{
	if( m_bUseInProgress )
		return;  // already animating

	m_bPendingUse = false;  // consumida
	m_bUsedThisFire = true;

	// play the use animation (hitme_1 = anim 1)
	SendWeaponAnim( 1 );

	// play sound immediately
#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	if( player )
		EMIT_SOUND( ENT(player), CHAN_ITEM, "items/smallmedkit1.wav", 1.0, ATTN_NORM );
#endif

	// effects applied AFTER the animation finishes (hitme_1 @90fps ~0.3s)
	m_bUseInProgress = true;
	m_flUseFinishTime = m_pLayer->GetTime() + 0.35f;
	m_flNextPrimaryAttack = m_pLayer->GetTime() + 0.5f;
	m_flTimeWeaponIdle = m_pLayer->GetTime();
}

void CStimulantWeaponContext::WeaponIdle()
{
	ResetEmptySound();

	// RTN F9: uso via tecla V - o ClientCommand setou m_bPendingUse -> dispara o uso
	// (o deploy ja completou, pois WeaponIdle roda apos o DefaultDeploy)
	if( m_bPendingUse && !m_bUseInProgress )
	{
#ifndef CLIENT_DLL
		g_engfuncs.pfnServerPrint( "[RTN] stim WeaponIdle: PENDING -> PrimaryAttack\n" );
#endif
		PrimaryAttack();
		return;
	}

	// RTN F6 fix: ShouldWeaponIdle()=true -> o ItemPostFrame base chama WeaponIdle()
	// TODO frame (inclusive com o botao de ataque pressionado - catch-all no final).
	// Assim os efeitos do uso sao processados mesmo segurando o clique.
	if( m_bUseInProgress )
	{
		if( m_pLayer->GetTime() >= m_flUseFinishTime )
		{
			m_bUseInProgress = false;
#ifndef CLIENT_DLL
			CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
			if( player )
			{
				// Heal 50 HP
				player->TakeHealth( 50, DMG_GENERIC );

				// Restore stamina - fuser2 is stamina in Xash/PrimeXT
				player->pev->fuser2 = 100.0f;

				// Screen flash: longer (100ms fade, 200ms hold) - drug kick effect
				UTIL_ScreenFade( player, Vector(255, 255, 200), 0.1f, 0.2f, 160, 0 );
				// secondary warm pulse (drug "wave") - fades out over 0.6s
				UTIL_ScreenFade( player, Vector(255, 180, 60), 0.3f, 0.6f, 60, 0 );

				// RTN cumulative: consume 1 dose; only remove when out of doses
				m_iClip--;
				if( m_iClip <= 0 )
				{
					player->RemovePlayerItem( m_pLayer->GetWeaponEntity() );
					UTIL_Remove( (CBaseEntity *)m_pLayer->GetWeaponEntity() );
					player->SelectLastItem();  // RTN F9: volta pra arma anterior
				}
				else
				{
					player->SelectLastItem();  // RTN F9: ainda tem dose, mas volta pra arma
				}
				SendRTNItemsHUD( player );  // RTN F9: atualiza contador lateral
			}
#endif
		}
		else
		{
			// keep checking while animation plays - RETURN pra nao deixar a
			// linha "+5.0f" abaixo sobrescrever (bug: animacao nunca completava)
			m_flTimeWeaponIdle = m_pLayer->GetTime();
			return;
		}
	}

	m_flTimeWeaponIdle = m_pLayer->GetTime() + 5.0f;
}
