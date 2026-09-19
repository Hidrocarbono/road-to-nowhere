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
uniform sampler2D		u_CloudMap;		// RTN: mascara de cobertura de nuvem (clouds.tga), tileavel

uniform vec3		u_LightDir;
uniform vec3		u_LightDiffuse;
uniform vec3		u_ViewOrigin;
uniform vec4		u_FogParams;
uniform vec4		u_FogParams2;	// RTN: aqui .x = forca do gradiente de horizonte (0..1) - campos diferentes de fog.h, ver comentario abaixo
uniform vec4		u_CloudParams;	// RTN: .xy = offset de scroll (UV), .z = tiling, .w = opacidade (0 = camada desligada)

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

	// RTN: camada de nuvem escorregando por cima do skybox pintado. u_CloudParams.w
	// chega 0 quando gl_sky_clouds esta desligado (client/render/gl_sky.cpp) - assim
	// evita o "if" aqui, so custa uma textura fetch e um mix a mais sempre.
	//
	// UV projetado a partir da direcao de visao (nao var_TexCoord, que e o atlas DA
	// FACE - usar ele criaria uma costura visivel em cada quina do cubo, porque cada
	// face tileia a textura de forma independente). Achatando .z (como o
	// R_CloudTexCoord nativo do engine, gl_warp.c, faz pra cobrir a cupula toda com
	// distorcao minima perto do zenite) da uma projecao continua nas 6 faces.
	vec3 cloudDir = normalize( var_Vertex.xyz - u_ViewOrigin );
	vec2 cloudUV = ( cloudDir.xy / max( abs( cloudDir.z ) + 0.15, 0.15 )) * u_CloudParams.z + u_CloudParams.xy;
	float cloudCoverage = colormap2D( u_CloudMap, cloudUV ).a * u_CloudParams.w;
	diffuse.rgb = mix( diffuse.rgb, vec3( 1.0 ), cloudCoverage );

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
		//
		// RTN: gradiente de horizonte. Ate aqui o peso de mistura e o MESMO em
		// qualquer ponto do domo - zenite recebe tanto fog quanto horizonte.
		// Fog atmosferico de verdade nao e assim: perto do horizonte o raio de
		// visao atravessa MUITO mais "espessura" de atmosfera de lado do que
		// olhando reto pra cima, entao a neblina la e sempre mais forte. Sem
		// esse degrade, a transicao geometria->skybox fica visivel como uma
		// linha (o mundo desaparece no fog numa densidade, o ceu logo acima
		// dele continua limpo com a mesma densidade) - e essa linha e
		// literalmente "onde comeca o skybox" ficando obvio.
		//
		// horizonFactor: 1.0 exatamente no horizonte (skyDir.z perto de 0),
		// cai pra 0.0 no zenite/nadir (skyDir.z perto de +-1). u_FogParams2.x
		// (gl_fog_sky_horizon) e a forca desse gradiente: 0 = sem gradiente
		// nenhum (peso igual em qualquer direcao, comportamento antigo), 1 =
		// so o horizonte recebe fog, zenite fica sempre limpo.
		vec3 skyDir = normalize( var_Vertex.xyz - u_ViewOrigin );
		float horizonFactor = 1.0 - abs( skyDir.z );
		float horizonWeight = mix( 1.0, horizonFactor, clamp( u_FogParams2.x, 0.0, 1.0 ));
		float skyBlend = clamp( u_FogParams.w, 0.0, 1.0 ) * horizonWeight;

		diffuse.rgb = mix( diffuse.rgb, u_FogParams.xyz, skyBlend );
	}

	gl_FragColor = vec4(diffuse, 1.0);
}