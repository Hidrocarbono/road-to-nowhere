/*
weapon_activity.h - resolucao de animacao de viewmodel por ACTIVITY (RTN)
Copyright (C) 2026 Road to Nowhere - port do padrao do Paranoia 2 (Uncle Mike, XWS)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

POR QUE ISSO EXISTE
-------------------
As armas classicas do Half-Life referenciam animacoes por INDICE literal dentro
do .mdl (MP5_ANIM_SHOOT1 = 1 significa "a segunda sequencia de v_mp5.mdl"). Isso
so funciona enquanto todo modelo respeitar exatamente a mesma ordem de sequencias.

Modelos no padrao do Paranoia 2 nao respeitam - cada um tem a sua ordem, e em vez
disso marca cada sequencia com um numero de ACTIVITY no QC. Exemplo real
(game_dir/models/v_parafal.mdl):

    seq 0 idle(82)   seq 1 reload(94)   seq 2 draw(78)   seq 3-5 shoot(84)
    seq 6 reload_tac(94)   seq 7 idle_aim(83)   seq 8 reload_aim(95)
    seq 9 reload_tac_aim(95)   seq 10-12 shoot_aim(85)
    seq 13 idle_out(110)   seq 14 idle_ins(109)

Com o esquema por indice, pedir MP5_ANIM_SHOOT1 (=1) nessa arma toca o RELOAD.
Era exatamente esse o bug de "a arma alterna entre idle e reload" - e tambem o
motivo de nao haver muzzle flash: o evento 5001 esta gravado nas sequencias de
shoot (3..5), que nunca chegavam a ser tocadas.

A resolucao aqui e sempre por activity, com o indice hardcoded servindo apenas de
fallback para modelos classicos cujas sequencias nao tem activity marcada (todas
com activity 0). Ou seja: modelos do HL continuam funcionando sem alteracao.

NOTA DE INCLUDE: este header NAO inclui engine/studio.h de proposito. Ele e
incluido por codigo compartilhado (game_shared/weapons/mp5.cpp) que e compilado
antes de extdll.h no lado do servidor, e studio.h arrasta mathlib.h/shader.h -
misturar isso com a ordem de include do servidor e a mesma classe de problema que
ja quebrou MAX_AMMO_TYPES aqui (ver server/weaponscript.h). Por isso o ponteiro de
modelo trafega como const void* e o cast fica dentro do .cpp, que e o unico lugar
que enxerga studio.h.
*/

#pragma once

// Numeracao de activity do Paranoia 2 (Uncle Mike's Xash Weapon System). Nao sao
// os ACT_* do Half-Life (que vao ate ~70 e descrevem monstros, nao viewmodels):
// e uma faixa propria, acima deles, so para armas. Os valores abaixo foram lidos
// diretamente das sequencias de v_parafal.mdl e batem com os comentarios que ja
// existiam em game_shared/weapons/mp5.h.
enum weapon_activity_e
{
	WACT_DRAW		= 78,	// deploy / saque
	WACT_IDLE		= 82,	// parado, quadril
	WACT_IDLE_AIM		= 83,	// parado, mirando (iron sight)
	WACT_SHOOT		= 84,	// disparo, quadril (varias variantes)
	WACT_SHOOT_AIM		= 85,	// disparo, mirando (varias variantes)
	WACT_RELOAD		= 94,	// recarga, quadril
	WACT_RELOAD_AIM		= 95,	// recarga, mirando
	WACT_AIM_IN		= 109,	// transicao quadril -> mira  (idle_ins)
	WACT_AIM_OUT		= 110,	// transicao mira -> quadril  (idle_out)
};

// Retornado quando o modelo nao tem nenhuma sequencia com a activity pedida.
// Mesmo valor/semantica de ACTIVITY_NOT_AVAILABLE (server/activity.h), repetido
// aqui porque este header e compartilhado com o cliente, que nao inclui aquele.
#define WACT_NOT_AVAILABLE	(-1)

//
// Procura no .mdl a sequencia marcada com `activity`. `hdr` e um studiohdr_t*
// (const void* aqui pelo motivo explicado na NOTA DE INCLUDE acima).
//
// `variant` escolhe entre multiplas sequencias com a MESMA activity (os tres
// shoot_1/2/3 marcados 84, por exemplo). E indice, nao sorteio: quem chama
// decide como sortear, para que servidor e cliente possam chegar ao mesmo
// numero quando isso importar (predicao). O valor da a volta (modulo) na
// quantidade de variantes encontradas, entao pedir a variante 2 num modelo que
// so tem uma sequencia daquela activity devolve essa unica sequencia em vez de
// falhar.
//
// Devolve WACT_NOT_AVAILABLE quando nao ha nenhuma - inclusive para hdr nulo
// (viewmodel ainda nao carregado / modelo nao-studio), que e o caso normal
// durante o primeiro frame apos o deploy.
//
int WeaponActivity_Lookup( const void *hdr, int activity, int variant = 0 );

// Quantas sequencias o modelo tem com essa activity (0 = nenhuma). Usado para
// sortear a variante de disparo dentro do numero real de animacoes do modelo,
// em vez do "random(0,2)" fixo que assumia exatamente tres.
int WeaponActivity_Count( const void *hdr, int activity );
