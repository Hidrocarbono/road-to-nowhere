/*
rtn_body.cpp - corpo do jogador (pernas + tronco) em primeira pessoa (RTN)

COMO FUNCIONA
-------------
Antes, ao olhar para baixo, o cliente TROCAVA o modelo do viewent (a unica entidade de
primeira pessoa) pelo player_legs.mdl - e a arma sumia, porque era a mesma entidade.

Agora o viewent e sempre a arma, e o corpo e uma entidade propria (s_body), desenhada pelo
CStudioModelRenderer::DrawBodyModel() ANTES da arma, na mesma passada de viewmodel
(gl_studio_draw.cpp) e nunca em espelho/portal/sombra/cubemap (so em RP_NORMALPASS).
A arma e desenhada depois e por cima (o depth hack dela a deixa sempre a frente).

Esta entidade NAO esta na lista de entidades do engine, entao:
  - nao projeta sombra (a sombra do jogador continua sendo a do player.mdl real);
  - nao recebe luz dinamica (lanterna / NVG) - mesma situacao da arma;
  - tem modelhandle proprio (model instance proprio, com blend de sequencia gratis).

O QUE ESTE ARQUIVO CALCULA (a cada frame, ver RTN_Body_Update)
-------------------------------------------------------------
  posicao  - pes no chao (origem do player_legs.mdl esta nos pes: o bone da pelve fica a
             ~38u do origin), recuada para tras do olhar quando se olha para baixo (senao a
             camera fica DENTRO do peito e o tronco some no plano proximo), deslocada com o
             lean e suavizada em degraus;
  yaw      - o corpo acompanha o olhar com deadzone (parado) e segue rapido (andando); ao
             andar de lado/para tras o quadril gira ate 35 graus para o lado do movimento;
  animacao - idle / walk / run / crawl / crouch_idle / jump2 escolhidas pelo ESTADO (nao pela
             gaitsequence), com a velocidade de reproducao presa a velocidade real (pes nao
             "deslizam") e a fase controlada aqui (curstate.frame), nao pelo animtime.

O player_legs.mdl nao tem linearmovement gravado (velocidade de chao), entao a referencia de
cada ciclo e uma cvar (rtn_body_walk_ref / rtn_body_run_ref) - calibrar em jogo.
*/

#include "hud.h"
#include "utils.h"
#include "ref_params.h"
#include "pm_defs.h"
#include "gl_local.h"
#include "entity_types.h"
#include "r_studioint.h"
#include "gl_studio.h"
#include "rtn_body.h"
#include <mathlib.h>

#define RTN_BODY_MODEL		"models/player_legs.mdl"
#define RTN_PI			3.14159265358979f

static cvar_t	*rtn_body;			// 1 liga / 0 desliga
static cvar_t	*rtn_body_back;		// recuo do corpo (unidades) olhando para a frente
static cvar_t	*rtn_body_back_look;	// recuo EXTRA (unidades) olhando 100% para baixo
static cvar_t	*rtn_body_zoffset;		// ajuste vertical dos pes (unidades, + sobe)
static cvar_t	*rtn_body_walk_ref;		// velocidade de chao (u/s) em que o ciclo de walk tem rate 1.0
static cvar_t	*rtn_body_run_ref;		// idem para o ciclo de run
static cvar_t	*rtn_body_crouch_ref;	// idem para o ciclo de crawl (agachado andando)
static cvar_t	*rtn_body_lean;		// quanto do lean (deslocamento e roll) o corpo acompanha; negativo inverte o roll

static cl_entity_t	s_body;			// a entidade desenhada
static bool		s_bActive = false;		// desenhar neste frame?
static bool		s_bHaveState = false;	// o estado abaixo ja foi inicializado?

static int		s_iModelIndex = 0;		// indice do modelo na lista do cliente (0 = ainda nao achou)
static model_t		*s_pModel = NULL;		// modelo para o qual os indices abaixo valem
static int		s_iSeqIdle, s_iSeqWalk, s_iSeqRun, s_iSeqCrouchIdle, s_iSeqCrouchMove, s_iSeqJump;

static float		s_flYaw;			// yaw atual do corpo
static bool		s_bTurning;		// parado e girando de volta para o olhar
static float		s_flZ;			// altura (suavizada) dos pes
static float		s_flPhase;			// fase 0..1 da sequencia atual
static float		s_flAirTime;		// ha quanto tempo esta no ar (histerese do jump)
static int		s_iSeq;			// sequencia atual

// diferenca de angulos normalizada para -180..180 (a - b)
static float BodyAngleDiff( float a, float b )
{
	float d = fmodf( a - b, 360.0f );
	if( d > 180.0f ) d -= 360.0f;
	else if( d < -180.0f ) d += 360.0f;
	return d;
}

static int BodyFindSeq( studiohdr_t *hdr, const char *label )
{
	mstudioseqdesc_t *pseq = (mstudioseqdesc_t *)((byte *)hdr + hdr->seqindex);

	for( int i = 0; i < hdr->numseq; i++ )
	{
		if( !Q_stricmp( pseq[i].label, label ))
			return i;
	}

	return -1;
}

void RTN_Body_Init( void )
{
	rtn_body		= CVAR_REGISTER( "rtn_body", "1", FCVAR_ARCHIVE );
	rtn_body_back	= CVAR_REGISTER( "rtn_body_back", "8.0", FCVAR_ARCHIVE );
	rtn_body_back_look	= CVAR_REGISTER( "rtn_body_back_look", "8.0", FCVAR_ARCHIVE );
	rtn_body_zoffset	= CVAR_REGISTER( "rtn_body_zoffset", "0.0", FCVAR_ARCHIVE );
	rtn_body_walk_ref	= CVAR_REGISTER( "rtn_body_walk_ref", "150", FCVAR_ARCHIVE );
	rtn_body_run_ref	= CVAR_REGISTER( "rtn_body_run_ref", "320", FCVAR_ARCHIVE );
	rtn_body_crouch_ref	= CVAR_REGISTER( "rtn_body_crouch_ref", "100", FCVAR_ARCHIVE );
	rtn_body_lean	= CVAR_REGISTER( "rtn_body_lean", "0.4", FCVAR_ARCHIVE );

	memset( &s_body, 0, sizeof( s_body ));
	s_body.modelhandle = INVALID_HANDLE;
	s_body.hCachedMatrix = WORLD_MATRIX;
}

void RTN_Body_Hide( void )
{
	s_bActive = false;
}

void RTN_Body_OnNewMap( void )
{
	// o model instance foi destruido junto com os demais - nao reaproveitar o handle
	s_body.modelhandle = INVALID_HANDLE;
	s_bActive = false;
	s_bHaveState = false;
	s_pModel = NULL;
	s_iModelIndex = 0;
}

cl_entity_t *RTN_Body_GetEntity( void )
{
	return s_bActive ? &s_body : NULL;
}

void RTN_Body_Update( struct ref_params_s *pparams, float flLeanRoll, float flLeanOffset )
{
	s_bActive = false;

	if( !rtn_body || rtn_body->value == 0.0f )
		return;

	// janela de documento aberta: o viewmodel some (V_CalcGunAngle), o corpo tambem
	if( gHUD.m_TextWindow.IsOpen( ))
		return;

	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if( !local )
		return;

	// sem corpo: morto, noclip, escada (MOVETYPE_FLY), na agua, spectator
	const int movetype = local->curstate.movetype;
	if( pparams->health <= 0 || pparams->spectator || pparams->intermission ||
		movetype == MOVETYPE_NOCLIP || movetype == MOVETYPE_FLY || pparams->waterlevel >= 2 )
	{
		s_bHaveState = false;	// ao voltar, recomeca alinhado ao olhar
		return;
	}

	// ---- modelo (precacheado pelo servidor - server/client.cpp) ----
	// o indice so muda quando o mapa muda (RTN_Body_OnNewMap zera) - evita a busca por nome todo frame
	if( s_iModelIndex <= 0 )
		s_iModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( RTN_BODY_MODEL );

	const int iModelIndex = s_iModelIndex;
	if( iModelIndex <= 0 )
		return;

	model_t *pModel = IEngineStudio.GetModelByIndex( iModelIndex );
	if( !pModel )
		return;

	studiohdr_t *pHdr = (studiohdr_t *)IEngineStudio.Mod_Extradata( pModel );
	if( !pHdr )
		return;

	if( pModel != s_pModel )
	{
		// modelo novo (ou recarregado): acha as sequencias pelo LABEL - a ordem do
		// player_legs.mdl nao bate com a do player.mdl
		s_pModel = pModel;
		s_iSeqIdle = BodyFindSeq( pHdr, "idle" );
		s_iSeqWalk = BodyFindSeq( pHdr, "walk2handed" );
		s_iSeqRun = BodyFindSeq( pHdr, "run2" );
		s_iSeqCrouchIdle = BodyFindSeq( pHdr, "crouch_idle" );
		s_iSeqCrouchMove = BodyFindSeq( pHdr, "crawl" );
		s_iSeqJump = BodyFindSeq( pHdr, "jump2" );
		s_bHaveState = false;
	}

	if( s_iSeqIdle < 0 )
		return;	// modelo sem a sequencia minima

	const float dt = bound( 0.0f, pparams->frametime, 0.1f );
	const float flViewYaw = pparams->cl_viewangles[YAW];
	const float flPitch = pparams->cl_viewangles[PITCH];	// + = olhando para baixo

	Vector vel = pparams->simvel;
	const float flSpeed = vel.Length2D();
	const bool bDuck = pparams->viewheight[2] < 20.0f;	// 28 em pe, 12 agachado
	const bool bOnGround = local->curstate.onground != -1;

	if( !s_bHaveState )
	{
		s_flYaw = flViewYaw;
		s_bTurning = false;
		s_flZ = pparams->simorg[2] - ( bDuck ? 18.0f : 36.0f );
		s_flPhase = 0.0f;
		s_flAirTime = 0.0f;
		s_iSeq = -1;
		s_bHaveState = true;
	}

	// ---- yaw do corpo ----
	// Andando: o corpo segue o olhar, com o quadril girando ate 35 graus para o lado
	// do movimento (strafe). Para tras: o corpo continua olhando para a frente e a
	// animacao roda ao contrario. Parado: deadzone de 50 graus antes de girar.
	float flMoveOfs = 0.0f;
	bool bBackward = false;

	if( flSpeed > 20.0f )
	{
		const float flMoveYaw = atan2f( vel.y, vel.x ) * ( 180.0f / RTN_PI );
		float flRel = BodyAngleDiff( flMoveYaw, flViewYaw );	// 0 = em frente, + = para a esquerda

		if( fabsf( flRel ) > 90.0f )
		{
			bBackward = true;
			flRel += ( flRel > 0.0f ) ? -180.0f : 180.0f;	// dobra para -90..90
		}

		flMoveOfs = bound( -35.0f, flRel * 0.5f, 35.0f );
	}

	const float flTargetYaw = flViewYaw + flMoveOfs;
	const float flYawDiff = BodyAngleDiff( flTargetYaw, s_flYaw );

	if( flSpeed > 20.0f )
	{
		const float flStep = 720.0f * dt;
		s_flYaw += bound( -flStep, flYawDiff, flStep );
		s_bTurning = false;
	}
	else
	{
		if( fabsf( flYawDiff ) > 50.0f ) s_bTurning = true;
		else if( fabsf( flYawDiff ) < 5.0f ) s_bTurning = false;

		if( s_bTurning )
		{
			const float flStep = 300.0f * dt;
			s_flYaw += bound( -flStep, flYawDiff, flStep );
		}
	}
	s_flYaw = BodyAngleDiff( s_flYaw, 0.0f );	// normaliza

	// ---- animacao: escolhida pelo estado ----
	if( bOnGround ) s_flAirTime = 0.0f;
	else s_flAirTime += dt;

	int iSeq;
	float flRate = 1.0f;

	if( s_flAirTime > 0.15f && s_iSeqJump >= 0 )
	{
		iSeq = s_iSeqJump;	// no ar (histerese de 0.15s: degrau nao dispara o pulo)
	}
	else if( bDuck )
	{
		if( flSpeed > 15.0f && s_iSeqCrouchMove >= 0 )
		{
			iSeq = s_iSeqCrouchMove;
			flRate = flSpeed / Q_max( rtn_body_crouch_ref->value, 1.0f );
		}
		else iSeq = ( s_iSeqCrouchIdle >= 0 ) ? s_iSeqCrouchIdle : s_iSeqIdle;
	}
	else if( flSpeed > 15.0f && ( s_iSeqWalk >= 0 || s_iSeqRun >= 0 ))
	{
		const float flWalkRef = Q_max( rtn_body_walk_ref->value, 1.0f );
		const float flRunRef = Q_max( rtn_body_run_ref->value, 1.0f );

		// escolhe o ciclo cujo ritmo natural esta mais perto da velocidade real
		const bool bRun = s_iSeqRun >= 0 && ( s_iSeqWalk < 0 || flSpeed > ( flWalkRef + flRunRef ) * 0.5f );
		iSeq = bRun ? s_iSeqRun : s_iSeqWalk;
		flRate = flSpeed / ( bRun ? flRunRef : flWalkRef );
	}
	else iSeq = s_iSeqIdle;

	flRate = bound( 0.4f, flRate, 2.5f );
	if( bBackward && flSpeed > 15.0f ) flRate = -flRate;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pHdr + pHdr->seqindex) + iSeq;
	const bool bLooping = ( pseqdesc->flags & STUDIO_LOOPING ) != 0;

	if( iSeq != s_iSeq )
	{
		// troca de sequencia: o renderer faz o blend (AddBlendSequence) sozinho, porque
		// esta entidade passa pelo caminho NORMAL (o do viewmodel zera o blend)
		s_iSeq = iSeq;
		s_flPhase = 0.0f;
	}

	// fase controlada aqui: curstate.frame (0..255) + framerate 0 -> o renderer nao soma
	// tempo por conta propria, entao mudar a velocidade no meio do ciclo nao "pula"
	if( pseqdesc->numframes > 1 )
		s_flPhase += dt * flRate * pseqdesc->fps / (float)pseqdesc->numframes;

	if( bLooping )
	{
		s_flPhase -= floorf( s_flPhase );	// 0..1
	}
	else s_flPhase = bound( 0.0f, s_flPhase, 0.999f);

	// ---- posicao ----
	// pes: origem do hull - meia altura. O player_legs.mdl tem a origem NOS PES.
	const float flFeetZ = pparams->simorg[2] - ( bDuck ? 18.0f : 36.0f ) + rtn_body_zoffset->value;

	if( !bOnGround || fabsf( flFeetZ - s_flZ ) > 24.0f )
		s_flZ = flFeetZ;	// no ar / teleporte / queda grande: sem suavizar
	else s_flZ += ( flFeetZ - s_flZ ) * bound( 0.0f, dt * 25.0f, 1.0f );	// degraus sem "pop"

	// recuo: a camera fica a "back" unidades NA FRENTE do eixo do corpo. Olhando para
	// baixo aumenta (senao o olho entra no peito e o plano proximo recorta o tronco).
	const float flLook = bound( 0.0f, ( flPitch - 15.0f ) / 50.0f, 1.0f );
	const float flBack = rtn_body_back->value + rtn_body_back_look->value * flLook;
	const float flYawRad = s_flYaw * ( RTN_PI / 180.0f );
	const float flFwdX = cosf( flYawRad );
	const float flFwdY = sinf( flYawRad );

	// lean: o olho vai flLeanOffset para o lado; o corpo acompanha uma fracao (o quadril
	// se desloca menos que a cabeca). pparams->right e o da camera.
	const float flLeanK = rtn_body_lean->value;

	Vector org;
	org.x = pparams->simorg[0] - flFwdX * flBack + pparams->right[0] * flLeanOffset * flLeanK;
	org.y = pparams->simorg[1] - flFwdY * flBack + pparams->right[1] * flLeanOffset * flLeanK;
	org.z = s_flZ;

	// ---- preenche a entidade ----
	s_body.index = 0;
	s_body.player = false;
	s_body.model = pModel;
	s_body.curstate.modelindex = iModelIndex;
	s_body.curstate.entityType = ET_NORMAL;
	s_body.curstate.movetype = MOVETYPE_NONE;	// sem suavizacao de degrau do renderer (ja fazemos a nossa)
	s_body.curstate.body = 0;			// Player_Top + Player_Bottom
	s_body.curstate.skin = 0;
	s_body.curstate.scale = 0.0f;
	s_body.curstate.effects = 0;
	s_body.curstate.rendermode = kRenderNormal;
	s_body.curstate.renderamt = 255;
	s_body.curstate.renderfx = kRenderFxNone;
	s_body.curstate.iuser1 = 0;
	s_body.curstate.weaponmodel = 0;			// nunca desenha arma na mao do corpo (a arma e o viewmodel)
	s_body.curstate.gaitsequence = 0;
	s_body.curstate.sequence = s_iSeq;
	s_body.curstate.frame = s_flPhase * 255.0f;
	s_body.curstate.framerate = 0.0f;			// a fase e nossa (ver acima)
	s_body.curstate.animtime = pparams->time;
	for( int i = 0; i < 4; i++ )
	{
		s_body.curstate.controller[i] = 127;
		s_body.curstate.blending[i] = 0;
	}

	s_body.origin = org;
	s_body.curstate.origin = org;
	s_body.angles[PITCH] = 0.0f;			// o corpo nao inclina com o olhar
	s_body.angles[YAW] = s_flYaw;
	s_body.angles[ROLL] = flLeanRoll * flLeanK;	// sinal: se sair invertido, rtn_body_lean negativo
	s_body.curstate.angles = s_body.angles;

	s_bActive = true;
}
