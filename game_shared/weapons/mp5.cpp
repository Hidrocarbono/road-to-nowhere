#include "mp5.h"

#ifdef CLIENT_DLL
#else
#include "extdll.h"
#include "enginecallback.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#include "ggrenade.h"
#endif

CMP5WeaponContext::CMP5WeaponContext(std::unique_ptr<IWeaponLayer> &&layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_MP5;
	m_iDefaultAmmo = MP5_DEFAULT_GIVE;
	m_usEvent1 = m_pLayer->PrecacheEvent("events/mp5.sc");
	m_usEvent2 = m_pLayer->PrecacheEvent("events/mp52.sc");
	m_bInIronSight = false;
}

int CMP5WeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(MP5_CLASSNAME);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = "ARgrenades";
	p->iMaxAmmo2 = M203_GRENADE_MAX_CARRY;
	p->iMaxClip = MP5_MAX_CLIP;
	p->iSlot = 3;        // consistente com server/weapon_mp5.cpp (slot 3)
	p->iPosition = 1;    // pos 1 (padrao HL; pos 6 quebrava o scroll)
	p->iFlags = ITEM_FLAG_SELECTONEMPTY;
	p->iId = m_iId;
	p->iWeight = MP5_WEIGHT;
	return 1;
}

int CMP5WeaponContext::SecondaryAmmoIndex()
{
	return m_iSecondaryAmmoType;
}

bool CMP5WeaponContext::Deploy()
{
	m_bInIronSight = false;
	m_bFOVLerpActive = false;
	m_pLayer->SetPlayerFOV( 90 );
#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	if( player )
		g_engfuncs.pfnClientCommand( player->edict(), "cl_viewmodel_fov 64\n" );
#endif
	return DefaultDeploy( "models/v_mp5.mdl", "models/p_mp5.mdl", MP5_ANIM_DEPLOY, "mp5" );
}

void CMP5WeaponContext::PrimaryAttack()
{
	// don't fire underwater
	if (m_pLayer->GetPlayerWaterlevel() == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.15f);
		return;
	}

	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.15f);
		return;
	}

	m_iClip--;

	Vector vecSrc = m_pLayer->GetGunPosition();
	matrix3x3 cameraTransform = m_pLayer->GetCameraOrientation();
	// RTN F10 fix: NUNCA usar autoaim aqui. A MP5 nao tem ITEM_FLAG_USEAUTOAIM
	// (igual Half-Life classico / Paranoia 2: so usam autoaim se a flag existir).
	// O GetAutoaimVector puxava o tiro ate 25 graus para grudar em qualquer entidade
	// (inimigo OU aliado), causando o tiro mirado sair deslocado (esquerda/cima).
	// GetCameraOrientation() ja retorna v_angle + punchangle (direcao pura da mira).
	// pull gun origin back 12u when not aiming (user: 15u was too close, -3 => 12u)
	if( !m_bInIronSight )
		vecSrc = vecSrc - cameraTransform.GetForward() * 12.0f;

	// RTN F5 RIG UNIFICADO: o offset lateral do lean NÃO é aplicado aqui.
	// O GetGunPosition() já retorna o olho deslocado (view_ofs do server, ±12u rotacionado pelo yaw).
	// Aplicar offset duplo aqui fazia o tiro sair a 24u (12u view_ofs + 12u extra) - bug do "tiro solto".
#ifndef CLIENT_DLL
	// DEBUG temporario: verificar se o lean chega ao server e onde o tiro sai (remover depois)
	{
		CBasePlayer *pDebug = m_pLayer->GetWeaponEntity()->m_pPlayer;
		if( pDebug && (pDebug->pev->button & (IN_ALT1 | IN_CANCEL)) )
		{
			g_engfuncs.pfnServerPrint( va( "[RTN] lean btn=0x%x view_ofs=(%.1f,%.1f,%.1f) vecSrc=(%.1f,%.1f,%.1f)\n",
				pDebug->pev->button & (IN_ALT1 | IN_CANCEL),
				pDebug->pev->view_ofs.x, pDebug->pev->view_ofs.y, pDebug->pev->view_ofs.z,
				vecSrc.x, vecSrc.y, vecSrc.z ) );
		}
	}
#endif

	// spread depends on ironsight: normal = 3deg, aiming = 1deg (more accurate)
	Vector spread;
	if( m_bInIronSight )
		spread = m_pLayer->IsMultiplayer() ? VECTOR_CONE_1DEGREES : VECTOR_CONE_1DEGREES;
	else
		spread = m_pLayer->IsMultiplayer() ? VECTOR_CONE_6DEGREES : VECTOR_CONE_3DEGREES;

	Vector vecDir = m_pLayer->FireBullets(1, vecSrc, cameraTransform, 8192, spread.x, BULLET_PLAYER_MP5, m_pLayer->GetRandomSeed());

	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usEvent1;
	params.delay = 0.0f;
	params.origin = vecSrc;
	params.angles = cameraTransform.GetAngles();
	params.fparam1 = vecDir.x;
	params.fparam2 = vecDir.y;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = m_bInIronSight ? 1 : 0;  // tells client which shoot anim to play
	params.bparam2 = 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	player->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	player->m_iWeaponFlash = NORMAL_GUN_FLASH;
	player->pev->effects = (int)(player->pev->effects) | EF_MUZZLEFLASH;
	player->SetAnimation(PLAYER_ATTACK1);

	if (!m_iClip && player->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		player->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#endif

	m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.1f);
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
}
void CMP5WeaponContext::SecondaryAttack()
{
	// Toggle ironsight on right-click
	m_bInIronSight = !m_bInIronSight;

	// RTN F10 IRONSIGHT (plano do user: "enganacao do modelo" em vez de zoom da camera):
	// A CAMERA NAO MUDA (FOV fixo 90). O que se aproxima e o VIEWMODEL via
	// cl_viewmodel_fov ALTO (110 = arma grande, parece colada no rosto).
	// Por que resolve: o engine compensa o viewmodel com flFOVOffset = 90 - fov_camera.
	// Com camera 90, offset = 0 -> viewmodel FOV puro -> o attachment (fumaca) e o
	// tracante usam a MESMA projecao -> alinhados por construcao (sem desvio esq/cima).
	if( m_bInIronSight )
		SendWeaponAnim( MP5_ANIM_AIM_IN );
	else
		SendWeaponAnim( MP5_ANIM_AIM_OUT );

#ifndef CLIENT_DLL
	extern int gmsgIronSight;	// RTN F10 (server/user_messages.h)
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	if( player )
	{
		// RTN F10 v3 (user): alem do viewmodel aproximado, a CAMERA aproxima
		// (FOV 90 -> 65) p/ dar a sensacao de mira. O cl_viewmodel_fov mirado
		// cai p/ 25: o engine soma flFOVOffset (90-65=25) ao viewmodel FOV,
		// entao 25+25=50 = o MESMO tamanho de arma de antes (a arma nao muda,
		// so o mundo aproxima). ATENCAO: o attachment (fumaca) usa o FOV do
		// viewmodel (50) e o tracante a camera (65) - pode desalinhar um pouco;
		// se aparecer desvio, reduzir o zoom ou subir o cl_viewmodel_fov.
		if( m_bInIronSight )
		{
			g_engfuncs.pfnClientCommand( player->edict(), "cl_viewmodel_fov 25\n" );
			m_pLayer->SetPlayerFOV( 65.0f );
		}
		else
		{
			g_engfuncs.pfnClientCommand( player->edict(), "cl_viewmodel_fov 64\n" );
			m_pLayer->SetPlayerFOV( 0.0f );	// default (90)
		}

		// RTN F10: avisa o client do estado da mira (1 byte) - o client
		// ativa/desativa o DOF (foco no alvo, fundo desfocado)
		MESSAGE_BEGIN( MSG_ONE, gmsgIronSight, NULL, player->edict() );
		WRITE_BYTE( m_bInIronSight ? 1 : 0 );
		MESSAGE_END();
	}
#endif

	m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.3f;
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 2.0f;
}

void CMP5WeaponContext::Reload()
{
	if( m_bInIronSight )
		DefaultReload( MP5_MAX_CLIP, MP5_ANIM_RELOAD_AIM, 1.5 );
	else
		DefaultReload( MP5_MAX_CLIP, MP5_ANIM_RELOAD, 1.5 );
}

void CMP5WeaponContext::WeaponIdle()
{
	ResetEmptySound();
	m_pLayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	// RTN F10: camera fixa (sem FOV lerp) - o ironsight aproxima so o viewmodel.
	// A transicao suave vem da animacao AIM_IN/AIM_OUT do modelo.
	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	if( m_bInIronSight )
		SendWeaponAnim( MP5_ANIM_IDLE_AIM );
	else
		SendWeaponAnim( MP5_ANIM_IDLE );

	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
}