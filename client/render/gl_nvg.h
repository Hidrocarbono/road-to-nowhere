/*
gl_nvg.h - Road to Nowhere: visao noturna (client-side)

O NVG do RTN e feito de duas partes, ambas no cliente:

  1. GANHO DE EXPOSICAO  - o auto-exposure que ja roda todo frame
     (postfx/generate_exposure) tem o teto de exposicao levantado enquanto o
     NVG esta ligado. Isso e amplificacao de luz de verdade: multiplica a cena
     em HDR ANTES do tonemap comprimir, entao o detalhe do escuro volta com
     contraste. Custo: nenhum passe novo, so uniforms.

  2. ILUMINADOR IR       - uma dlight omni presa ao jogador, sem sombra e sem
     bump, raio curto. E o que da visibilidade onde o lightmap e literalmente
     zero (ganho multiplicativo nao cria luz do nada). Custo: um passe aditivo
     da luz - por isso o raio e curto por padrao.

O tint verde/grain/vinheta sai por cima disso modulando os parametros de
postfx que ja existem (sem tocar no estado global do g_PostFxController).

O estado vem do servidor por gmsgNVG (client/hud_msg.cpp) - o servidor e dono
do "tem o item" e da bateria; aqui so se decide como aquilo aparece.
*/

#pragma once
#include "cl_dlight.h"

struct cl_entity_s;
class CPostFxParameters;

// estado vindo do servidor (gmsgNVG): ativo + bateria 0..100
void RTN_NVG_SetState( bool bActive, int iBattery );
void RTN_NVG_Reset( void );

// 0..1 - rampa de aquecimento/desligamento do tubo, com tremulacao quando a
// bateria esta acabando. Zero = NVG sem nenhum efeito sobre o render.
float RTN_NVG_GetIntensity( void );
bool  RTN_NVG_IsActive( void );

// parametros do auto-exposure (x = teto de exposicao, y = escala,
// z = taxa de adaptacao ao escuro, w = taxa de adaptacao ao claro).
// Devolve os valores nativos do jogo quando o NVG esta desligado.
void RTN_NVG_GetExposureParams( float *pOut4 );

// ganho aplicado nos color levels quando o tonemap esta desligado (r_tonemap 0),
// unico caminho em que o ganho de exposicao nao existe. 1.0 = sem compensacao.
float RTN_NVG_GetFallbackGain( void );

// aplica tint verde / grain / vinheta por cima do estado de postfx do frame
// (nao toca no estado global do g_PostFxController)
void RTN_NVG_ApplyPostFx( CPostFxParameters &fx );

// iluminador IR - chamado do R_AddEntity para o jogador local
void RTN_NVG_SetupPlayerLight( struct cl_entity_s *ent );

void RTN_NVG_RegisterCvars( void );
