#pragma once
#include "weapon_context.h"
#include "weapon_layer.h"
#include <utility>

#ifndef CLIENT_DLL
// weaponscript.h lives under server/ and is only on the server's include path
// (server/CMakeLists.txt); the parser itself (weaponscript.cpp) is server-only
// too (uses std::filesystem + GET_GAME_DIR), so script-driven data can't reach
// the client build yet. Client-side prediction keeps using the hardcoded
// fallback models/anims below until that's ported - see m_pScriptInfo.
#include "weaponscript.h"
#endif

#define WEAPON_MP5			4

// RTN weapon-script: id range dynamically handed out to script-only weapons
// (weapon_scripted/CWeaponScripted instances, e.g. weapon_parafal) by
// WeaponScript_GetWeaponID() (server/weaponscript.h/.cpp) - never a fixed
// classic WEAPON_* constant, to avoid colliding with the real weapon that
// owns it in CBaseWeaponContext::ItemInfoArray[m_iId]. Defined here (not just
// in weaponscript.h) because client/weapon_predicting_context.cpp needs the
// range too, to build a predicted context for these ids instead of returning
// null - see GetWeaponContext()'s default case. Keep both copies in sync.
// O teto vem de game_dir/delta.lst (m_iId agora com 6 bits, era 5) - ver o
// comentario longo sobre este par em server/weaponscript.h antes de mexer em
// qualquer uma das duas copias.
#ifndef WEAPON_SCRIPT_ID_BASE
#define WEAPON_SCRIPT_ID_BASE	31
#define WEAPON_SCRIPT_ID_MAX	62
#endif

// item_flags do script. Definidos tambem em server/weaponscript.h (o parser),
// espelhados aqui pelo mesmo motivo de WEAPON_SCRIPT_ID_BASE acima: o cliente
// nao inclui weaponscript.h, mas precisa dos bits para decidir se a arma tem
// mira de ferro durante a predicao. Manter as duas copias em sincronia.
// ATENCAO: colidem em valor com ITEM_FLAG_SELECTONEMPTY/NOAUTORELOAD/
// NOAUTOSWITCHEMPTY (1|2|4, game_shared/item_info.h) e nao tem NADA a ver com
// eles - nunca atribuir um no outro.
#ifndef WIF_IRONSIGHT
#define WIF_IRONSIGHT		(1<<0)
#define WIF_AUTOAIM			(1<<1)
#define WIF_AUTOFIRE		(1<<2)
#define WIF_SELECTONEMPTY	(1<<3)
#define WIF_NOAUTORELOAD	(1<<4)
#define WIF_NOAUTOSWITCH	(1<<5)
#define WIF_UNDERWATER		(1<<6)
#define WIF_MELEE			(1<<7)	// ligado pelo parser quando PrimaryAttack "action" = "swing"
#endif
// Variantes de som por grupo da arma corpo a corpo - mesmo valor de server/weaponscript.h
// (o cliente nao inclui aquele header, mas o membro m_szSwing* existe nos dois lados).
#ifndef WS_MAX_SND_VARIANTS
#define WS_MAX_SND_VARIANTS	4
#endif

#define MP5_WEIGHT			15
#define MP5_MAX_CLIP		50
#define MP5_DEFAULT_AMMO	25
#define MP5_DEFAULT_GIVE	25
#define MP5_CLASSNAME		weapon_mp5

enum mp5_anim_e
{
	MP5_ANIM_IDLE = 0,			// ACT_82
	MP5_ANIM_SHOOT1 = 1,		// ACT_84 1
	MP5_ANIM_SHOOT2 = 2,		// ACT_84 2
	MP5_ANIM_SHOOT3 = 3,		// ACT_84 3
	MP5_ANIM_RELOAD = 4,		// ACT_94 1
	MP5_ANIM_DEPLOY = 5,		// ACT_78 1
	MP5_ANIM_IDLE_AIM = 6,		// ACT_83 1
	MP5_ANIM_SHOOT1_AIM = 7,	// ACT_85 1
	MP5_ANIM_SHOOT2_AIM = 8,	// ACT_85 2
	MP5_ANIM_SHOOT3_AIM = 9,	// ACT_85 3
	MP5_ANIM_RELOAD_AIM = 10,	// ACT_95 1
	MP5_ANIM_AIM_IN = 11,		// ACT_109 1
	MP5_ANIM_AIM_OUT = 12,		// ACT_110 1
	// backward compat aliases for client event code
	MP5_LONGIDLE = 0,
	MP5_IDLE1 = 0,
	MP5_LAUNCH = 2,
	MP5_RELOAD = 4,
	MP5_DEPLOY = 5,
	MP5_FIRE1 = 1,
	MP5_FIRE2 = 2,
	MP5_FIRE3 = 3,
};

class CMP5WeaponContext : public CBaseWeaponContext
{
public:
	CMP5WeaponContext() = delete;
	~CMP5WeaponContext() = default;
	CMP5WeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 3; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	int SecondaryAmmoIndex() override;
	bool Deploy() override;
	void Reload() override;
	void WeaponIdle() override;

	bool m_bInIronSight = false;
	bool m_bFOVLerpActive = false;
	float m_fFOVLerpStart = 0.0f;
	float m_fFOVFrom = 90.0f;
	float m_fFOVTo = 90.0f;

	// ---------------------------------------------------------------------
	// Parametros vindos do script que PRECISAM existir nos dois lados.
	//
	// m_pScriptInfo (abaixo) e server-only: o parser usa std::filesystem e
	// GET_GAME_DIR, e nao ha porte dele para o cliente. Mas o PrimeXT PREDIZ
	// arma no cliente (client/weapon_predicting_context.cpp) - diferente do
	// Paranoia 2, que nao tem predicao nenhuma de arma no cl_dll e por isso
	// podia deixar tudo no servidor.
	//
	// Consequencia: cadencia e dispersao calculadas so no servidor divergem da
	// predicao do cliente - o tiro "borracha", a arma dispara em ritmo diferente
	// do que a tela mostra. Entao estes campos sao NEUTROS quanto a lado, e o
	// servidor os manda pelo weapon_data_t (fuser1..3/iuser1..2, que ja existem
	// no delta.lst e ja estao plumbados) - ver server/client.cpp (GetWeaponData)
	// e client/weapon_predicting_context.cpp (Read/WriteWeaponSpecificData).
	//
	// Zero = "sem script", e o codigo cai no valor hardcoded da MP5. Isso mantem
	// a MP5 de verdade e qualquer arma sem .txt exatamente como estavam.
	// ---------------------------------------------------------------------
	float m_flScriptNextAttack = 0.0f;	// PrimaryAttack/nextattack (segundos)
	float m_flScriptSpread = 0.0f;		// SpreadRange, em graus, no quadril
	float m_flScriptSpreadIS = 0.0f;	// SpreadRangeIS, em graus, mirando
	int m_iScriptZoomFOV = 0;		// zoom_fov (0 = arma sem mira de ferro)
	int m_iScriptFlags = 0;			// WIF_IRONSIGHT|WIF_AUTOAIM|WIF_AUTOFIRE

	// ---------------------------------------------------------------------
	// RTN: dispersao por estado + firmeza em ADS (debate "combate a longa
	// distancia"). Calculado em UpdateSpreadState(), chamado em PrimaryAttack()
	// (pro tiro em si) e em WeaponIdle() (pra ficar fresco mesmo sem atirar -
	// e o que o crosshair dinamico/client/rtn_crosshair_state.h le).
	//
	// DE PROPOSITO sem nenhum acumulador de tempo (tipo "segundos parado ate
	// a firmeza maxima"): m_flScriptSpread* so chega identico nos dois lados
	// porque trafega no weapon_data_t (comentario acima) - um campo local novo
	// que precisasse "lembrar" de frames anteriores correria o risco de nao
	// sobreviver a reconstrucao do contexto de arma durante replay de
	// predicao (o mesmo tipo de bug que o comentario de cima descreve pro
	// tracante). Por isso tanto o multiplicador de postura quanto a firmeza
	// sao funcoes PURAS do frame atual (GetPlayerVelocity(), IN_DUCK,
	// m_bInIronSight) - zero estado entre chamadas, zero risco de
	// dessincronia de replay. Firmeza fica "liga/desliga" (sem rampa
	// gradual) por esse motivo - se um dia quisermos a rampa, precisa antes
	// confirmar como o contexto de arma sobrevive (ou nao) ao replay de
	// predicao.
	// ---------------------------------------------------------------------
	float m_flStateSpreadMul = 1.0f;	// multiplicador por postura/velocidade (1.0 parado .. ~2.0 correndo)
	float m_flCurrentSpreadTan = 0.0f;	// ultimo cone calculado (tan do meio-angulo) - o que PrimaryAttack usa de verdade

	// RTN: primeiro-tiro vs sustentado (SpreadExpand/SpreadTime do script -
	// existiam no parser desde sempre, nunca tinham sido lidos, ver comentario
	// antigo em CacheScriptParams()). ESTES dois SAO estado entre frames (ao
	// contrario do resto acima) - decaimento ancorado em m_flLastFireTime, que
	// ja e um campo pre-existente da classe base (weapon_context.h/.cpp,
	// usado no ajuste de cadencia) e portanto ja provado seguro atraves de
	// replay de predicao neste projeto; nao introduz uma categoria nova de
	// risco, so reusa uma ja validada.
	float m_flSpreadHeat = 0.0f;		// 0 = frio (cone base), 1 = cone maximo (rajada sustentada)
	float m_flSpreadHeatAtLastShot = 0.0f;	// snapshot do heat no instante do ultimo tiro - base do decaimento

	// true = parado (ou agachado) E em ADS agora mesmo - aplica o piso de firmeza
	bool IsSteadyAiming() const { return m_bInIronSight && m_pLayer->GetPlayerVelocity().Length() < 10.0f; }

	// Recalcula m_flStateSpreadMul e m_flCurrentSpreadTan a partir do estado
	// atual do jogador (velocidade, agachado, ADS). Definida em mp5.cpp
	// (precisa de IN_DUCK/tanf/VECTOR_CONE_*, ja incluidos la).
	void UpdateSpreadState();

	// 1 = o servidor tem o .wav do SoundData e toca o tiro ele mesmo; o evento do
	// cliente entao NAO deve tocar o som hardcoded por cima. 0 = o som do script
	// nao existe no disco, e o cliente segue tocando o som padrao.
	//
	// Precisa viajar: quem sabe se o arquivo existe e o servidor
	// (PrecacheScriptSounds), mas quem decide tocar ou nao o som hardcoded e o
	// evento no cliente. Sem isso, uma arma cujo .wav faltasse ficaria MUDA -
	// o servidor nao toca porque nao tem o arquivo, e o cliente nao toca porque
	// acha que o servidor tocou.
	int m_iScriptHasSound = 0;

	// true quando a arma declara WIF_IRONSIGHT no script. Para arma sem script
	// (m_iScriptFlags == 0) devolve true, preservando o comportamento atual da
	// MP5 hardcoded, que sempre teve mira no botao direito.
	bool HasIronSight() const { return m_iScriptFlags == 0 || ( m_iScriptFlags & WIF_IRONSIGHT ) != 0; }

	// true quando o script declara PrimaryAttack { "action" "swing" } (o parser liga
	// WIF_MELEE, que viaja em m_iScriptFlags). Arma corpo a corpo: faca, soco...
	bool IsMelee() const { return ( m_iScriptFlags & WIF_MELEE ) != 0; }

	// Golpe da arma corpo a corpo (substitui o disparo em PrimaryAttack()). O
	// SERVIDOR faz o trace, o dano, os sons e escolhe a animacao de acerto/erro;
	// o CLIENTE so prediz a animacao de erro e a cadencia (o trace e so-servidor
	// no crowbar classico tambem). Ver o comentario em mp5.cpp.
	void Swing();
	int m_iSwingCount = 0;	// alterna as variantes de animacao (hit_small1/2, miss_small1/2)

	// FOV da mira: do script quando houver, senao o 65 que estava fixo no codigo.
	float IronSightFOV() const { return m_iScriptZoomFOV > 0 ? (float)m_iScriptZoomFOV : 65.0f; }
	bool ShouldWeaponIdle() override { return true; }  // so WeaponIdle runs every frame (FOV lerp)
	uint16_t m_usEvent1;
	uint16_t m_usEvent2;

#ifndef CLIENT_DLL
	// populated server-side (Spawn/Precache) from WeaponScript_FindWeaponByName();
	// stays null when no matching scripts/weapons/<classname>.txt was loaded,
	// in which case Deploy()/GetItemInfo() keep the classic hardcoded MP5 values.
	// Used by the REAL weapon_mp5 entity (CMP5) - never touches m_iId, so it
	// stays WEAPON_MP5 always, even if a future weapon_mp5.txt gets added.
	void SetScriptInfo( const weaponinfo_t *info ) { m_pScriptInfo = info; CacheScriptParams(); }

	// Used by CWeaponScripted (any weapon_<name> that isn't the real MP5): same
	// as SetScriptInfo(), but also gives the context its own dynamic m_iId via
	// WeaponScript_GetWeaponID() - see the big comment on WEAPON_SCRIPT_ID_BASE
	// above for why this must NOT stay WEAPON_MP5. Falls back to WEAPON_MP5 when
	// info is null (no script found), matching the entity's own model/stat
	// fallback to the classic MP5 in that case - consistent id for a consistent
	// fallback identity.
	void SetScriptInfoWithDynamicId( const weaponinfo_t *info )
	{
		m_pScriptInfo = info;
		m_iId = info ? WeaponScript_GetWeaponID( const_cast<weaponinfo_t *>( info ) ) : WEAPON_MP5;
		CacheScriptParams();
	}

	// Copia do script para os campos neutros de lado (declarados acima), que sao
	// os unicos que o cliente vai enxergar - via weapon_data_t. Chamada sempre
	// que m_pScriptInfo muda.
	void CacheScriptParams()
	{
		if( !m_pScriptInfo )
		{
			m_flScriptNextAttack = 0.0f;
			m_flScriptSpread = 0.0f;
			m_flScriptSpreadIS = 0.0f;
			m_iScriptZoomFOV = 0;
			m_iScriptFlags = 0;
			return;
		}

		m_flScriptNextAttack = m_pScriptInfo->primary.nextattack;

		// SpreadRange vem como faixa ("1..2" graus). Usamos o meio da faixa como
		// cone base; a variacao por tiro (SpreadExpand/SpreadTime) ainda nao esta
		// portada, entao um valor unico e o mais honesto por enquanto.
		m_flScriptSpread = ( m_pScriptInfo->primary.SpreadRange[0] + m_pScriptInfo->primary.SpreadRange[1] ) * 0.5f;
		m_flScriptSpreadIS = ( m_pScriptInfo->primary.SpreadRangeIS[0] + m_pScriptInfo->primary.SpreadRangeIS[1] ) * 0.5f;

		m_iScriptZoomFOV = m_pScriptInfo->zoom_fov;
		m_iScriptFlags = m_pScriptInfo->item_flags;
	}

	const weaponinfo_t *m_pScriptInfo = nullptr;

	// Sons de tiro do script que EXISTEM de fato no disco. Vazio = nao ha, e o
	// disparo cai no som hardcoded do cliente. Preenchidos por
	// PrecacheScriptSounds() (weapon_scripted.cpp chama no Precache()).
	char m_szShootSound1[64] = { 0 };
	char m_szShootSound2[64] = { 0 };

	// Sons da arma corpo a corpo (so os .wav que existem - mesmo criterio acima).
	// Corpo = shootsound1[], mundo = shootsound2[], erro = emptysound.
	char m_szSwingBody[WS_MAX_SND_VARIANTS][64] = {};
	int m_nSwingBody = 0;
	char m_szSwingWorld[WS_MAX_SND_VARIANTS][64] = {};
	int m_nSwingWorld = 0;
	char m_szSwingMiss[64] = { 0 };

	// Modelos do script que foram REALMENTE precacheados. Vazio = o arquivo nao
	// existe e o modelo nao pode ser usado.
	//
	// Existem porque o Deploy nao pode passar direto o caminho do script para o
	// DefaultDeploy: ele grava pev->weaponmodel SEM verificar nada, e um modelo
	// nao precacheado gera "Cannot get index for model X: not precached" a cada
	// frame em que o jogador e desenhado. Era o que acontecia com um script
	// cujo playermodel apontava para um .mdl ausente - o precache ja pulava o
	// arquivo corretamente, mas o Deploy setava o modelo assim mesmo.
	char m_szViewModel[64] = { 0 };
	char m_szPlayerModel[64] = { 0 };

	void SetPrecachedModels( const char *viewmodel, const char *playermodel )
	{
		strncpy( m_szViewModel, viewmodel ? viewmodel : "", sizeof( m_szViewModel ) - 1 );
		m_szViewModel[sizeof( m_szViewModel ) - 1] = '\0';
		strncpy( m_szPlayerModel, playermodel ? playermodel : "", sizeof( m_szPlayerModel ) - 1 );
		m_szPlayerModel[sizeof( m_szPlayerModel ) - 1] = '\0';
	}

	// Precacha os .wav do SoundData, mas SO os que existem.
	//
	// Os scripts importados do Paranoia 2 referenciam sons que nao vieram junto:
	// weapon_parafal.txt pede weapons/parafal_fire1.wav, que nao esta no
	// repositorio. Precachear arquivo ausente aborta o carregamento do mapa -
	// trocar "som errado" por "mapa nao abre" seria um pessimo negocio. Entao o
	// que falta simplesmente nao e registrado, e a arma segue com o som antigo
	// ate os arquivos aparecerem, sem exigir nenhuma outra mudanca de codigo.
	void PrecacheScriptSounds();
#endif
};

template<>
struct CBaseWeaponContext::AssignedWeaponID<CMP5WeaponContext> {
	static constexpr int32_t value = WEAPON_MP5;
};
