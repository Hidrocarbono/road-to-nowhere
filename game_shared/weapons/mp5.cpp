#include "mp5.h"
#include "weapon_activity.h"
#include <cmath>	// tanf (conversao graus -> cone de dispersao)
#include <cstdio>	// snprintf (comando cl_viewmodel_fov montado em runtime)

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
#include "filesystem_utils.h"	// fs::FileExists - precache condicional dos sons do script
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

#ifndef CLIENT_DLL
void CMP5WeaponContext::PrecacheScriptSounds()
{
	m_szShootSound1[0] = '\0';
	m_szShootSound2[0] = '\0';

	if( !m_pScriptInfo )
		return;

	const char *src[2] = { m_pScriptInfo->sound.shootsound1, m_pScriptInfo->sound.shootsound2 };
	char *dst[2] = { m_szShootSound1, m_szShootSound2 };

	for( int i = 0; i < 2; i++ )
	{
		if( !src[i][0] )
			continue;

		// O caminho no script e relativo a sound/ (convencao do EMIT_SOUND), mas
		// a checagem de existencia e no sistema de arquivos do jogo, que precisa
		// do caminho completo.
		char szPath[128];
		snprintf( szPath, sizeof( szPath ), "sound/%s", src[i] );

		if( !fs::FileExists( szPath ) )
		{
			ALERT( at_console, "WeaponScript: som [%s] nao existe - disparo segue com o som padrao\n", src[i] );
			continue;
		}

		PRECACHE_SOUND( src[i] );
		strncpy( dst[i], src[i], 63 );
		dst[i][63] = '\0';
	}

	m_iScriptHasSound = m_szShootSound1[0] ? 1 : 0;
}
#endif

int CMP5WeaponContext::GetItemInfo(ItemInfo *p) const
{
#ifndef CLIENT_DLL
	// script-driven overrides (RTN weapon-script): only clip/ammo-name/slot/weight
	// are wired here. iMaxAmmo1/iMaxAmmo2 (max carry) would need a second lookup
	// into gAmmoInfo via WeaponScript_FindAmmo() - not done yet, kept hardcoded.
	// iFlags is NOT wired to m_pScriptInfo->item_flags on purpose: that field holds
	// WIF_IRONSIGHT/WIF_AUTOAIM/WIF_AUTOFIRE bits (weaponscript.h), a different bit
	// layout than ItemInfo::iFlags' ITEM_FLAG_* bits (weapons.h) - assigning it
	// directly would set the wrong flags.
	if( m_pScriptInfo )
	{
		p->pszName = m_pScriptInfo->scriptname[0] ? m_pScriptInfo->scriptname : CLASSNAME_STR(MP5_CLASSNAME);
		p->pszAmmo1 = m_pScriptInfo->primary_ammo[0] ? m_pScriptInfo->primary_ammo : "9mm";
		p->iMaxAmmo1 = _9MM_MAX_CARRY;
		p->pszAmmo2 = ( m_pScriptInfo->secondary_ammo[0] && stricmp( m_pScriptInfo->secondary_ammo, "none" ) )
			? m_pScriptInfo->secondary_ammo : NULL;
		p->iMaxAmmo2 = M203_GRENADE_MAX_CARRY;
		p->iMaxClip = m_pScriptInfo->clip_size > 0 ? m_pScriptInfo->clip_size : MP5_MAX_CLIP;
		p->iSlot = m_pScriptInfo->bucket > 0 ? m_pScriptInfo->bucket : 3;
		p->iPosition = m_pScriptInfo->bucket_position;
		p->iFlags = ITEM_FLAG_SELECTONEMPTY;
		p->iId = m_iId;
		p->iWeight = m_pScriptInfo->weight > 0 ? m_pScriptInfo->weight : MP5_WEIGHT;
		return 1;
	}
#endif
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

	// script-driven models take over once scripts/weapons/<classname>.txt loaded
	// (m_pScriptInfo set by the entity's Spawn(), see weapon_mp5.cpp/weapon_scripted.cpp).
	// "playermodel" is the p_ model shown on other players (pev->weaponmodel);
	// "worldmodel" is the w_ model of the pickup lying on the ground, set by the
	// entity itself (weapon_scripted.cpp). weapon_parafal.txt had the two swapped
	// until now - fixed there, not worked around here.
	// m_szViewModel/m_szPlayerModel, NAO os caminhos crus do script: so contem os
	// modelos que existem e foram precacheados (ver mp5.h). Um playermodel
	// ausente passado ao DefaultDeploy vira pev->weaponmodel invalido e o engine
	// reclama "not precached" a cada frame. Com o campo vazio o jogador
	// simplesmente nao mostra arma na mao - feio, mas silencioso e sem erro.
	if( m_pScriptInfo && m_szViewModel[0] )
	{
		bool deployed = DefaultDeploy( m_szViewModel, m_szPlayerModel, MP5_ANIM_DEPLOY, "mp5" );

		if( !m_szPlayerModel[0] && m_pScriptInfo->playermodel[0] )
		{
			ALERT( at_console, "WeaponScript Deploy [%s]: playermodel [%s] ausente - jogador fica sem arma na mao\n",
				m_pScriptInfo->scriptname, m_pScriptInfo->playermodel );
		}
		// A viewmodel that was never precached resolves to model index 0 and the
		// client draws nothing - which looks identical to "the weapon never
		// deployed". Print both facts so the console tells them apart instead of
		// us guessing from the symptom.
		ALERT( at_console, "WeaponScript Deploy [%s]: deployed=%d clip=%d viewmodel=[%s] modelindex=%d\n",
			m_pScriptInfo->scriptname, deployed ? 1 : 0, m_iClip,
			m_pScriptInfo->viewmodel, MODEL_INDEX( m_pScriptInfo->viewmodel ) );

		// Diagnostico da resolucao por activity: diz de uma vez se o servidor
		// enxergou o studiohdr do viewmodel e quais sequencias REAIS ele achou.
		// Tudo -1 (ou igual aos indices da MP5) significa que caiu no fallback -
		// ou o modelo nao tem activity marcada, ou o header veio nulo. Sem isto a
		// unica pista seria a animacao errada na tela, que foi exatamente o que
		// nos custou varias builds para diagnosticar.
		ALERT( at_console, "WeaponScript Anim [%s]: hdr=%s draw=%d idle=%d reload=%d shoot=%d(x%d) aim_in=%d aim_out=%d\n",
			m_pScriptInfo->scriptname,
			m_pLayer->GetViewmodelStudioHeader() ? "ok" : "NULL",
			WeaponActivity_Lookup( m_pLayer->GetViewmodelStudioHeader(), WACT_DRAW ),
			WeaponActivity_Lookup( m_pLayer->GetViewmodelStudioHeader(), WACT_IDLE ),
			WeaponActivity_Lookup( m_pLayer->GetViewmodelStudioHeader(), WACT_RELOAD ),
			WeaponActivity_Lookup( m_pLayer->GetViewmodelStudioHeader(), WACT_SHOOT ),
			WeaponActivity_Count( m_pLayer->GetViewmodelStudioHeader(), WACT_SHOOT ),
			WeaponActivity_Lookup( m_pLayer->GetViewmodelStudioHeader(), WACT_AIM_IN ),
			WeaponActivity_Lookup( m_pLayer->GetViewmodelStudioHeader(), WACT_AIM_OUT ) );
		return deployed;
	}
#endif
#ifdef CLIENT_DLL
	// A script weapon's viewmodel path lives in scripts/weapons/<name>.txt, which
	// only the server parses - this build has no parser, so the hardcoded MP5
	// models below are simply the wrong models for it. They are also not harmless:
	// SetPlayerViewmodel() writes into the PREDICTED clientdata, and
	// HUD_TxferLocalOverrides() (client/entity.cpp) copies that straight into
	// gHUD.m_iViewModelIndex - the very index the renderer draws
	// (gl_studio_draw.cpp). So predicting a deploy here overwrites the correct
	// viewmodel the server already sent us: with the MP5's model, or with nothing
	// at all, since CL_LoadModel() yields index 0 for a path it can't resolve and
	// index 0 draws an empty hand.
	// Do everything DefaultDeploy() does EXCEPT touching the models, so the
	// server's authoritative viewmodel/weaponmodel survive prediction.
	if( m_iId >= WEAPON_SCRIPT_ID_BASE && m_iId <= WEAPON_SCRIPT_ID_MAX )
	{
		if( !CanDeploy() )
			return false;
		// Aqui o viewmodel NAO e trocado de proposito (ver comentario acima), entao
		// o modelo que ResolveWeaponAnim() consulta ja e o que o servidor mandou -
		// nao ha o problema de ordem que existe dentro de DefaultDeploy().
		SendWeaponAnimAct( WACT_DRAW, MP5_ANIM_DEPLOY );
		m_pLayer->SetPlayerNextAttackTime( m_pLayer->GetWeaponTimeBase( UsePredicting() ) + 0.5 );
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase( UsePredicting() ) + 1.0;
		m_flLastFireTime = 0.0f;
		return true;
	}
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

	// Dispersao: do script quando houver (SpreadRange/SpreadRangeIS, em graus),
	// senao os valores fixos da MP5. Os dois lados chegam ao mesmo numero porque
	// m_flScriptSpread* trafega no weapon_data_t - ver o comentario grande em
	// mp5.h. Fosse so no servidor, o tracante predito pelo cliente sairia numa
	// direcao e o dano em outra.
	float flScriptSpread = m_bInIronSight ? m_flScriptSpreadIS : m_flScriptSpread;
	float flSpread;

	if( flScriptSpread > 0.0f )
	{
		// Os VECTOR_CONE_*DEGREES do HL sao tan(graus/2) - VECTOR_CONE_3DEGREES
		// e 0.02618 = tan(1.5deg). Converter na mao mantem o script na mesma
		// unidade que o Paranoia 2 usa (graus) sem precisar de uma tabela.
		// constante literal em vez de M_PI: o MSVC so define M_PI com
		// _USE_MATH_DEFINES, que este projeto nao liga.
		flSpread = tanf( flScriptSpread * 0.5f * ( 3.14159265f / 180.0f ));
	}
	else if( m_bInIronSight )
	{
		flSpread = VECTOR_CONE_1DEGREES.x;
	}
	else
	{
		flSpread = m_pLayer->IsMultiplayer() ? VECTOR_CONE_6DEGREES.x : VECTOR_CONE_3DEGREES.x;
	}

	// Dano por tiro: do ammodesc.txt (PlayerDamage do tipo de municao). So o
	// servidor aplica dano, entao isto nao precisa viajar - o cliente passa 0 e
	// o FireBullets dele so gera efeito visual.
	int iDamage = 0;
#ifndef CLIENT_DLL
	if( m_pScriptInfo && m_pScriptInfo->primary_ammo[0] )
	{
		const ammoinfo_t *ammo = WeaponScript_FindAmmo( m_pScriptInfo->primary_ammo );
		if( ammo && ammo->PlayerDamage > 0 )
			iDamage = ammo->PlayerDamage;
	}
#endif

	Vector vecDir = m_pLayer->FireBullets(1, vecSrc, cameraTransform, 8192, flSpread, BULLET_PLAYER_MP5, m_pLayer->GetRandomSeed(), iDamage);

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
	// iparam1 != 0 avisa o evento do cliente que esta e uma arma de script: o som
	// de tiro sai do SERVIDOR (EMIT_SOUND abaixo, com o .wav do SoundData) e o
	// cliente NAO deve tocar o hks1/hks2 hardcoded da MP5 por cima.
	//
	// Por que o som fica no servidor e o resto continua predito: o cliente nao
	// tem o parser de script, entao nao conhece o nome do .wav. Daria para
	// mandar o indice do som pelo weapon_data_t (e o que o Paranoia 2 faz via
	// args->bparam2 + EV_SoundForIndex), mas os seis campos livres de
	// weapon_data_t ja estao todos ocupados pelos parametros que a PREDICAO
	// precisa - e som atrasado em um frame num servidor local e imperceptivel,
	// enquanto cadencia e dispersao dessincronizadas nao sao. Animacao, muzzle
	// flash e tracante continuam preditos, ou seja, instantaneos.
	params.iparam1 = m_iScriptHasSound;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	player->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	player->m_iWeaponFlash = NORMAL_GUN_FLASH;
	player->pev->effects = (int)(player->pev->effects) | EF_MUZZLEFLASH;
	player->SetAnimation(PLAYER_ATTACK1);

	// Som de tiro do script (SoundData/shootsound1|2), alternando entre os dois
	// quando o .txt declara dois. Fica no servidor pelo motivo explicado no
	// comentario de params.iparam1 acima.
	// m_szShootSound* (nao o script cru): so contem os .wav que existem de fato -
	// ver PrecacheScriptSounds(). Vazio significa que o som nao veio junto com o
	// script, e o cliente toca o som padrao dele.
	if( m_szShootSound1[0] )
	{
		const char *snd = m_szShootSound1;
		if( m_szShootSound2[0] && RANDOM_LONG( 0, 1 ) )
			snd = m_szShootSound2;

		EMIT_SOUND_DYN( player->edict(), CHAN_WEAPON, snd, 1.0f, ATTN_NORM, 0,
			94 + RANDOM_LONG( 0, 15 ) );
	}

	// Recuo (PunchAngle do script), sorteado dentro da faixa declarada -
	// "1..1.2" nao e "1.1 sempre", e a variacao por tiro que da a sensacao de
	// recuo. Aplicado so aqui de proposito: pev->punchangle ja e um campo
	// networkado (delta.lst), entao o cliente recebe o resultado pronto - somar
	// de novo no lado predito dobraria o coice.
	{
		const weaponattack_t *atk = m_pScriptInfo ? &m_pScriptInfo->primary : NULL;
		if( atk )
		{
			const float *lo = m_bInIronSight ? atk->PunchAngleISMin : atk->PunchAngleMin;
			const float *hi = m_bInIronSight ? atk->PunchAngleISMax : atk->PunchAngleMax;

			if( lo[0] != 0.0f || hi[0] != 0.0f || lo[1] != 0.0f || hi[1] != 0.0f )
			{
				m_pLayer->AddPlayerPunchangle(
					RANDOM_FLOAT( lo[0], hi[0] ),
					RANDOM_FLOAT( lo[1], hi[1] ),
					RANDOM_FLOAT( lo[2], hi[2] ) );
			}
		}
	}

	if (!m_iClip && player->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		player->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#endif

	// Cadencia do script (PrimaryAttack/nextattack). Trafega no weapon_data_t
	// para o cliente prever a mesma taxa de tiro - ver mp5.h.
	m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(
		m_flScriptNextAttack > 0.0f ? m_flScriptNextAttack : 0.1f );
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
}
void CMP5WeaponContext::SecondaryAttack()
{
	// Arma de script sem WIF_IRONSIGHT no item_flags nao tem mira de ferro -
	// antes qualquer arma mirava, porque a mira era hardcoded na MP5. Arma sem
	// script (m_iScriptFlags == 0) continua com mira, preservando a MP5 classica.
	if( !HasIronSight() )
	{
		m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.3f;
		return;
	}

	// Toggle ironsight on right-click
	m_bInIronSight = !m_bInIronSight;

	// Transicao suave de FOV. Os campos m_fFOVLerp* existiam desde o inicio,
	// declarados e zerados, mas nunca foram ligados a nada - o FOV pulava do 90
	// para o da mira num frame. O lerp roda em WeaponIdle(), que ja e chamado
	// todo frame porque ShouldWeaponIdle() devolve true.
	//
	// Isto responde ao problema que voce levantou: a transicao NAO precisa estar
	// animada no .mdl. O motor faz a aproximacao; a animacao idle_ins/idle_out do
	// modelo (quando existe, como na FAL) acompanha por cima.
	m_fFOVFrom = m_pLayer->GetPlayerFOV();
	if( m_fFOVFrom <= 0.0f )
		m_fFOVFrom = 90.0f;	// 0 = "padrao" na convencao do engine
	m_fFOVTo = m_bInIronSight ? IronSightFOV() : 90.0f;
	// GetTime(), NAO GetWeaponTimeBase(): com predicao ligada o time base vale
	// 0.0f (server_weapon_layer_impl.cpp:330), porque os temporizadores de arma
	// sao contagens REGRESSIVAS relativas, nao relogio. Usando o time base, o
	// "instante inicial" e o "agora" valiam os dois zero, o tempo decorrido dava
	// sempre 0 e o lerp ficava eternamente parado no FOV de partida - ou seja, o
	// zoom da mira nunca acontecia. GetTime() e absoluto nos dois lados.
	m_fFOVLerpStart = m_pLayer->GetTime();
	m_bFOVLerpActive = true;

	// RTN F10 IRONSIGHT (plano do user: "enganacao do modelo" em vez de zoom da camera):
	// A CAMERA NAO MUDA (FOV fixo 90). O que se aproxima e o VIEWMODEL via
	// cl_viewmodel_fov ALTO (110 = arma grande, parece colada no rosto).
	// Por que resolve: o engine compensa o viewmodel com flFOVOffset = 90 - fov_camera.
	// Com camera 90, offset = 0 -> viewmodel FOV puro -> o attachment (fumaca) e o
	// tracante usam a MESMA projecao -> alinhados por construcao (sem desvio esq/cima).
	// idle_ins(109) / idle_out(110): a transicao de mira ja vem animada no modelo
	// no padrao Paranoia 2 (v_parafal.mdl tem as duas, sequencias 14 e 13).
	if( m_bInIronSight )
		SendWeaponAnimAct( WACT_AIM_IN, MP5_ANIM_AIM_IN );
	else
		SendWeaponAnimAct( WACT_AIM_OUT, MP5_ANIM_AIM_OUT );

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
		// O cl_viewmodel_fov acompanha o FOV da camera para a arma nao mudar de
		// tamanho: o engine soma flFOVOffset = 90 - fov_camera ao viewmodel.
		// Com o zoom vindo do script, o valor mirado passa a ser calculado em vez
		// de ser o 25 fixo que so casava com o zoom 65 hardcoded.
		if( m_bInIronSight )
		{
			int vmFov = (int)( 64.0f - ( 90.0f - IronSightFOV() ));
			if( vmFov < 5 ) vmFov = 5;
			char szCmd[64];
			snprintf( szCmd, sizeof( szCmd ), "cl_viewmodel_fov %d\n", vmFov );
			g_engfuncs.pfnClientCommand( player->edict(), szCmd );
			// O SetPlayerFOV do alvo NAO e aplicado aqui: quem move o FOV agora e
			// o lerp em WeaponIdle(). Setar direto voltaria a dar o salto seco.
		}
		else
		{
			g_engfuncs.pfnClientCommand( player->edict(), "cl_viewmodel_fov 64\n" );
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
	// iMaxClip() le ItemInfoArray[m_iId].iMaxClip, que para uma arma de script vem
	// do "clip_size" do .txt (30 na FAL). Era MP5_MAX_CLIP fixo (50): a recarga
	// enchia o pente ate 50 numa arma de 30, e o HUD passava a mentir.
	// Para a MP5 de verdade o valor da tabela e MP5_MAX_CLIP, entao nada muda la.
	int iClipSize = iMaxClip() > 0 ? iMaxClip() : MP5_MAX_CLIP;

	if( m_bInIronSight )
		DefaultReload( iClipSize, ResolveWeaponAnim( WACT_RELOAD_AIM, MP5_ANIM_RELOAD_AIM ), 1.5 );
	else
		DefaultReload( iClipSize, ResolveWeaponAnim( WACT_RELOAD, MP5_ANIM_RELOAD ), 1.5 );
}

void CMP5WeaponContext::WeaponIdle()
{
	ResetEmptySound();
	m_pLayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	// Transicao suave de FOV entre quadril e mira. Roda todo frame (o
	// ShouldWeaponIdle() desta arma devolve true justamente para isso) e e
	// independente da animacao do modelo - e o motor fazendo a aproximacao, que
	// era o pedido: dar a transicao sem precisar anima-la no .mdl.
	if( m_bFOVLerpActive )
	{
		const float flDuration = 0.15f;	// mesma ordem do tempo de troca de arma
		// GetTime() pelo mesmo motivo do SecondaryAttack - ver comentario la.
		float flElapsed = m_pLayer->GetTime() - m_fFOVLerpStart;
		float t = ( flDuration > 0.0f ) ? ( flElapsed / flDuration ) : 1.0f;

		if( t >= 1.0f )
		{
			t = 1.0f;
			m_bFOVLerpActive = false;
		}
		else if( t < 0.0f )
		{
			t = 0.0f;	// o relogio pode recuar num rollback de predicao
		}

		// suavizacao nas pontas (smoothstep): sem ela o movimento comeca e para
		// de forma abrupta mesmo sendo continuo.
		float s = t * t * ( 3.0f - 2.0f * t );
		float flFOV = m_fFOVFrom + ( m_fFOVTo - m_fFOVFrom ) * s;

		// 90 e o padrao; mandar 0 no fim evita deixar o FOV "preso" num valor
		// explicito quando a arma for trocada ou largada.
		m_pLayer->SetPlayerFOV( ( !m_bFOVLerpActive && m_fFOVTo >= 90.0f ) ? 0.0f : flFOV );

#ifdef CLIENT_DLL
		// Sobe o viewmodel na MESMA curva 's' do FOV, para as duas transicoes
		// (zoom e levantar a arma) chegarem ao fim juntas - ver
		// cl_ironsight_raise em r_view.cpp para o motivo (o tiro sempre sai da
		// camera, entao isto e so cosmetico, para o alho de mira bater com o
		// tracante). 's' interpola RUMO ao alvo atual: se estamos entrando na
		// mira, sobe (0->1); se estamos saindo, desce (1->0) - dai o espelhar
		// quando m_bInIronSight e falso.
		extern float g_flIronSightRaise;
		g_flIronSightRaise = m_bInIronSight ? s : ( 1.0f - s );
#endif
	}
#ifdef CLIENT_DLL
	else
	{
		// Sem lerp ativo (arma acabou de ser sacada, ou a transicao ja
		// terminou ha frames): mantem o valor parado no estado atual, em vez
		// de deixar o ultimo numero escrito por outra arma sobrando aqui.
		extern float g_flIronSightRaise;
		g_flIronSightRaise = m_bInIronSight ? 1.0f : 0.0f;
	}
#endif

	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	if( m_bInIronSight )
		SendWeaponAnimAct( WACT_IDLE_AIM, MP5_ANIM_IDLE_AIM );
	else
		SendWeaponAnimAct( WACT_IDLE, MP5_ANIM_IDLE );

	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
}