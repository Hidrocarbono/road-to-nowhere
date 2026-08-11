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
	// RTN F10 fix: usa a DISTANCIA LINEAR (view-space Z reconstruida do
	// gl_FragCoord.w = 1/-z_view) em vez do parametro 'dist' (que era
	// gl_FragCoord.z/w = profundidade NON-LINEAR 0-1). Com o dist non-linear
	// e a density do mapa (0.005-0.012), o exp2 quase nunca saturava ->
	// o fogColor nunca dominava -> ao longe a cor virava a da cena (clara,
	// "branco"). Com a distancia real (centenas de unidades), o exp2 satura
	// e a cor do fog permanece solida ate o horizonte.
	float fogDist = 1.0 / gl_FragCoord.w;
	float fogFactor = saturate(exp2(-fogParams.w * fogDist));
	return mix(fogParams.rgb, inputColor, fogFactor);
}

#endif // FOG_H
