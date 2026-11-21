/*
rtn_crosshair_state.h - RTN

Ponte simples, SO DO LADO CLIENTE, entre o calculo de dispersao da arma
(game_shared/weapons/mp5.cpp::UpdateSpreadState(), roda predito no cliente
a cada PrimaryAttack()/WeaponIdle()) e o desenho do crosshair dinamico
(client/ammo.cpp::DrawDynamicCrosshair(), toggle via cvar
rtn_crosshair_dynamic). Sem isso o HUD nao teria como ler o cone de tiro
atual: a barra de selecao e o contador de municao trabalham so com a WEAPON
(struct de icones/HUD), nao com a CBaseWeaponContext viva que calcula a
dispersao - esse state global pequeno e o mesmo padrao ja usado pro NVG
(ver client/render/gl_nvg.h, RTN_NVG_IsActive()/RTN_NVG_GetBattery()).
*/
#pragma once
#ifndef RTN_CROSSHAIR_STATE_H
#define RTN_CROSSHAIR_STATE_H

// flSpreadTan: tan() do meio-angulo do cone atual (mesma unidade que o
// "spread" usado em FireBullets - ver mp5.cpp). bIronSight: so cosmetico,
// pra o desenho poder diferenciar estilo hip/ADS se quiser no futuro.
void RTN_SetCurrentSpread( float flSpreadTan, bool bIronSight );

float RTN_GetCurrentSpreadTan();
bool RTN_IsCurrentSpreadIronSight();

// false ate a arma atual rodar PrimaryAttack()/WeaponIdle() pelo menos uma
// vez (ex: logo no spawn, antes do primeiro tick de arma) - DrawDynamicCrosshair()
// nao desenha nada enquanto isso for false, pra nao mostrar lixo de outra arma/estado.
bool RTN_HasCurrentSpread();

#endif//RTN_CROSSHAIR_STATE_H
