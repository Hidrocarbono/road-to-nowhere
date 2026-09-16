/*
fog.h - fog implementation
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef FOG_H
#define FOG_H

// RTN: fogParams2 da FORMA a curva de densidade, em vez de so escala-la
// inteira como gl_fog_density_scale ja fazia. Dois efeitos independentes,
// somados na densidade final antes do exp2 de sempre:
//
//   fogParams2.x = fogStart      distancia (unidades) sem nenhum fog antes
//                                de comecar a curva - "ve limpo ate aqui,
//                                so dai fecha". 0 = identico a antes (fog
//                                comeca no olho da camera).
//
//   fogParams2.y = heightDensity densidade EXTRA que so existe perto do
//                                chao e cai com a altura (ver heightFactor
//                                abaixo). 0 = fog de altura desligado.
//   fogParams2.z = heightStart   Z do mundo onde o fog de altura esta no
//                                maximo (normalmente o piso).
//   fogParams2.w = heightFalloff unidades de altura acima de heightStart
//                                pra densidade de altura cair a ~37% (1/e) -
//                                controla se a "camada" de nevoa e rasteira
//                                ou alta.
//
// heightFactor usa so a altura do FRAGMENTO, nao integra ao longo do raio
// camera->fragmento (isso exigiria resolver a integral da densidade no
// trajeto, caro demais pro ganho visual aqui) - e a mesma aproximacao barata
// que a maioria dos motores usa pra fog de altura em tempo real.
vec3 CalculateFog(vec3 inputColor, vec4 fogParams, vec4 fogParams2, float worldZ)
{
	// Usa a distancia de view diretamente (gl_FragCoord.w = 1/w_clip, e w_clip e
	// a distancia), em vez do parametro 'dist' que os shaders passam.
	//
	// CORRECAO DE COMENTARIO: a versao anterior dizia que o 'dist' recebido
	// (gl_FragCoord.z / gl_FragCoord.w) era "profundidade NON-LINEAR 0-1" e que
	// por isso o fog clareava ao longe. Isso esta ERRADO - a divisao por w ja
	// lineariza. Fazendo a conta, com n = Z_NEAR e f = far:
	//
	//     gl_FragCoord.z = f/(f-n) * (1 - n/d)
	//     gl_FragCoord.w = 1/d
	//     z/w = f/(f-n) * (d - n)  ~=  d - n
	//
	// ou seja, o valor antigo JA era a distancia linear, apenas deslocada pelo
	// near plane (Z_NEAR = 4 unidades, ~10 cm). Trocar por 1/w muda o fog em 10
	// centimetros - correto, porem irrelevante.
	//
	// Fica registrado porque o clareamento ao longe tinha outra causa (a cor do
	// fog era convertida com a gamma invertida no C++ e chegava quase branca) e
	// levou a uma compensacao de densidade x10 que sufocou a cena inteira. Ver
	// SKY_FOG_DENSITY_FACTOR em client/render/gl_rmisc.cpp.
	float fogDist = 1.0 / gl_FragCoord.w;

	// RTN: distancia inicial - nao ha nenhuma atenuacao antes de fogStart, so
	// o trecho depois dela entra na curva exponencial.
	float effectiveDist = max(0.0, fogDist - fogParams2.x);

	// RTN: fog de altura - fator 1.0 no chao (heightStart), cai
	// exponencialmente com a altura acima dele. Some como densidade EXTRA em
	// cima da densidade base, nao a substitui.
	float heightAboveFloor = max(0.0, worldZ - fogParams2.z);
	float heightFactor = exp(-heightAboveFloor / max(fogParams2.w, 1.0));
	float totalDensity = fogParams.w + fogParams2.y * heightFactor;

	float fogFactor = saturate(exp2(-totalDensity * effectiveDist));
	return mix(fogParams.rgb, inputColor, fogFactor);
}

#endif // FOG_H
