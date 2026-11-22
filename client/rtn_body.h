/*
rtn_body.h - corpo do jogador (pernas + tronco) em primeira pessoa (RTN)

O corpo e uma SEGUNDA entidade de studio (models/player_legs.mdl), desenhada em uma
passada propria ANTES da arma (CStudioModelRenderer::DrawBodyModel). O viewent
continua sendo sempre a arma/braco, entao a arma nao some mais ao olhar para baixo.

Ver game_dir/devkit/GUIA_CORPO.md (cvars rtn_body_*) e o comentario grande em rtn_body.cpp.
*/

#pragma once

struct ref_params_s;
struct cl_entity_s;

// Registra as cvars rtn_body_* (V_Init).
void RTN_Body_Init( void );

// Marca o corpo como NAO desenhado neste frame. Chamado no inicio de V_CalcRefdef:
// camera de terceira pessoa, intermission, spectator etc. nao desenham o corpo.
void RTN_Body_Hide( void );

// Calcula o estado do corpo (posicao, yaw, animacao). Chamado no FIM de
// V_CalcFirstPersonRefdef, quando vieworg e o lean deste frame ja estao prontos.
void RTN_Body_Update( struct ref_params_s *pparams, float flLeanRoll, float flLeanOffset );

// A entidade do corpo para desenhar neste frame, ou NULL (desligado / escondido).
struct cl_entity_s *RTN_Body_GetEntity( void );

// Esquece o model instance da entidade. Chamado quando o renderer destroi TODOS os
// model instances (troca de mapa, vid_restart): o handle guardado ficaria pendurado.
void RTN_Body_OnNewMap( void );
