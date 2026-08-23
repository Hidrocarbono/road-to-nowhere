/*
skybox_fp.glsl - draw sun & skycolor
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "const.h"
#include "mathlib.h"
#include "texfetch.h"

uniform sampler2D		u_ColorMap;

uniform vec3		u_LightDir;
uniform vec3		u_LightDiffuse;
uniform vec3		u_ViewOrigin;
uniform vec4		u_FogParams;

varying vec4		var_Vertex;
varying vec2		var_TexCoord;

void main()
{
	vec3 sky_color = colormap2D( u_ColorMap, var_TexCoord ).rgb;
	vec3 eye = normalize( u_ViewOrigin - var_Vertex.xyz );
	vec3 sun = normalize( -u_LightDir );
		
	float day_factor = max( sun.z, 0.0 ) + 0.1;
	float dotv = max( -dot( eye, sun ), 0.0 );
	vec3 sun_color = vec3( u_LightDiffuse * 3.0);

	float pow_factor = day_factor * 512.0;
	float sun_factor = clamp( pow( dotv, 1536.0 ), 0.0, 1.0 );	// keep sun constant size

	// under horizon line
	if( sun.z < -0.5 ) sun_factor = 0.0;
#ifdef SKYBOX_DAYTIME
	sky_color *= day_factor;
#endif
	vec3 diffuse = sky_color + sun_color * sun_factor;

	if( bool( u_FogParams.w > 0.0 ))
	{
		// Mistura o ceu com a cor do fog, em vez de SUBSTITUIR.
		//
		// Antes esta linha era 'diffuse.rgb = u_FogParams.xyz' - substituicao
		// pura e simples, sem distancia e sem peso. Foi escrita para corrigir um
		// horizonte que ficava branco, mas o branco vinha da cor do fog ser
		// convertida com a gamma invertida no C++ (ver gl_rmisc.cpp), nao do ceu.
		// Resultado: com QUALQUER densidade de fog, ate a minima, o skybox
		// desaparecia por completo - nao havia valor de gl_fog_density_scale que
		// o trouxesse de volta, porque a densidade nunca entrou nesta conta.
		//
		// Agora u_FogParams.w chega aqui como PESO DE MISTURA (0..1), calculado
		// em client/render/gl_sky.cpp a partir de gl_fog_sky_blend - e o unico
		// lugar do renderer onde .w nao e densidade, justamente porque o ceu esta
		// no infinito e nao ha distancia com que calcular um fator.
		//   1.0 = comportamento antigo, ceu totalmente coberto
		//   0.85 = padrao: fog domina, mas o ceu ainda se insinua
		//   0.0 = ceu limpo, sem fog nenhum
		diffuse.rgb = mix( diffuse.rgb, u_FogParams.xyz, clamp( u_FogParams.w, 0.0, 1.0 ));
	}

	gl_FragColor = vec4(diffuse, 1.0);
}