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
	// RTN F10 fix: iMaxClip = 99 (doses maximas) - era 1 e o HUD da arma
	// mostrava clip errado. O m_iClip e usado como DOSES do estimulante.
	p->iMaxClip = 99;
	p->iSlot = 1;
	p->iPosition = 5;  // pos 6 quebrava o ciclo do scroll (padrao HL: 1-5)
	p->iFlags = ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iId = WEAPON_STIMULANT;
	// RTN F10 fix: peso BAIXO (5) p/ o FShouldSwitchWeapon NAO equipar o
	// estimulante ao adquirir (o jogador escolhe equipar via slot 1, como
	// qualquer arma). Antes era 60 e o item trocava a arma automaticamente.
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
	// RTN FIX: indice de sequencia conferido contra o QC decompilado de
	// v_antidote.mdl (ordem dos $sequence, 0-based): 0=idle_1 1=idle_2
	// 2=idle_3 3=draw 4=hitme_1 5=hitme_2 ... O indice 2 usado antes e
	// "idle_3" (looping) - visualmente indistinguivel de nao ter acontecido
	// nada, ja que o viewmodel ja estava em idle. O certo pra saque e 3.
	bool bResult = DefaultDeploy( "models/v_antidote.mdl", "models/w_antidote.mdl", 3, "medkit" );  // anim 3 = draw

	// RTN FIX (bug critico): DefaultDeploy() faz
	//   SetPlayerNextAttackTime( GetWeaponTimeBase(UsePredicting()) + 0.5 )
	// Para UsePredicting()==true (toda outra arma), GetWeaponTimeBase() e sempre 0.0f,
	// entao isso grava um valor RELATIVO pequeno (0.5) em CBasePlayer::m_flNextAttack.
	// CBasePlayer::UpdatePlayerTimers() (player.cpp) trata esse campo como CONTAGEM
	// REGRESSIVA relativa (-= frametime a cada frame).
	// O estimulante e a UNICA arma de jogador com UsePredicting()==false (ver .h), entao
	// GetWeaponTimeBase(false) retorna gpGlobals->time (timestamp ABSOLUTO, ex: 400.5 se o
	// servidor esta de pe ha 400s). Gravar isso em m_flNextAttack faz CBasePlayer::
	// ItemPostFrame()/ItemPreFrame() (que fazem "if (m_flNextAttack > 0.0f) return;")
	// ficarem bloqueados por ~gpGlobals->time SEGUNDOS, nao 0.5s -- e enquanto isso o
	// WeaponIdle() do estimulante (que dispara o uso pendente e fecha a animacao) nunca
	// roda. Sintoma: item equipa mas "V"/tiro nao fazem nada, so volta a funcionar minutos
	// depois, e quando finalmente destrava aplica cura/consumo de uma vez sem animar.
	// Corrige sobrescrevendo com o valor relativo pequeno que UpdatePlayerTimers espera.
	m_pLayer->SetPlayerNextAttackTime( 0.5f );

	return bResult;
}

void CStimulantWeaponContext::Holster()
{
	CBaseWeaponContext::Holster();

#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	if( player )
		// RTN FIX: restaura o FOV padrao do viewmodel (90 = default de cl_viewmodel_fov,
		// ver gl_studio_init.cpp). Sem isso o 80 setado no Deploy() vazava pra qualquer
		// arma seguinte que nao mexe nessa cvar (a MP5 se auto-corrige no proprio Deploy,
		// mas crowbar/python/etc nao tocam nela e ficavam com a viewmodel afastada pra sempre).
		g_engfuncs.pfnClientCommand( player->edict(), "cl_viewmodel_fov 90\n" );
#endif

	// RTN FIX: cancela o uso em progresso ao trocar de arma no meio da animacao. Sem
	// isso, m_bUseInProgress/m_flUseFinishTime ficavam presos e, ao reequipar depois do
	// tempo da animacao ja ter passado, o WeaponIdle() aplicava cura+consumo+remocao de
	// uma vez so, sem tocar a animacao de novo (efeito "fantasma").
	m_bUseInProgress = false;
	m_bPendingUse = false;
}

void CStimulantWeaponContext::PrimaryAttack()
{
	if( m_bUseInProgress )
		return;  // already animating

	m_bPendingUse = false;  // consumida

	// RTN FIX: era SendWeaponAnim(1), que no QC decompilado de v_antidote.mdl
	// e "idle_2" (looping) - nao "hitme_1". Indice 1 de idle pra idle e
	// visualmente identico a nao tocar animacao nenhuma (o sintoma reportado:
	// "fica na animacao idle"). hitme_1 e o indice 4 (ver ordem no Deploy()
	// acima). hitme_2 (indice 5) existe como variante, nao usada aqui.
	SendWeaponAnim( 4 );

	// play sound immediately
#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	if( player )
		EMIT_SOUND( ENT(player), CHAN_ITEM, "items/smallmedkit1.wav", 1.0, ATTN_NORM );
#endif

	// effects applied AFTER the animation finishes. NOTA: o QC decompilado
	// mostra hitme_1 em "fps 90", nao "30fps" como o comentario original
	// assumia - o QC nao lista contagem de frames, entao a duracao real da
	// sequencia (frames/fps) nao da pra confirmar so pelo texto do QC. O
	// 1.0f abaixo e a mesma estimativa antiga; se a animacao terminar antes/
	// depois disso no jogo, ajustar aqui (idealmente medindo em pxmv/hlmv).
	// Historico: antes usava 0.35s e o flash + troca de arma (SelectLastItem)
	// cortavam a animacao no meio (bug "flash antes do fim").
	m_bUseInProgress = true;
	m_flUseFinishTime = m_pLayer->GetTime() + 1.0f;
	m_flNextPrimaryAttack = m_pLayer->GetTime() + 1.1f;
	m_flTimeWeaponIdle = m_pLayer->GetTime();
}

void CStimulantWeaponContext::WeaponIdle()
{
	ResetEmptySound();

	// RTN F10: uso via tecla V - o handler equipou (SelectItem) e marcou o
	// pending; o WeaponIdle (apos o deploy completar) dispara o PrimaryAttack
	// AUTOMATICAMENTE -> 1 clique no V = seringa na mao + anim + efeitos.
	if( m_bPendingUse && !m_bUseInProgress )
	{
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

				// RTN F10 fix: consume 1 dose; so volta pra arma anterior quando
				// as doses ACABAM. Com doses restantes o jogador FICA com o
				// estimulante equipado (pode atirar de novo - cooldown 1.1s).
				m_iClip--;
				if( m_iClip <= 0 )
				{
					player->RemovePlayerItem( m_pLayer->GetWeaponEntity() );
					UTIL_Remove( (CBaseEntity *)m_pLayer->GetWeaponEntity() );
					player->SelectLastItem();  // volta pra arma anterior
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
