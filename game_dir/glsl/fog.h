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

vec3 CalculateFog(vec3 inputColor, vec4 fogParams, float dist)
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
	float fogFactor = saturate(exp2(-fogParams.w * fogDist));
	return mix(fogParams.rgb, inputColor, fogFactor);
}

#endif // FOG_H
