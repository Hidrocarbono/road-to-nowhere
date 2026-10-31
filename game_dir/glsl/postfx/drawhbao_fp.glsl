/*
drawhbao_fp.glsl - horizon-based ambient occlusion (passe 2: blur + composicao) (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Le o AO de meia resolucao gerado por genhbao_fp.glsl, faz um blur 4x4 que leva
em conta a profundidade (nao borra o AO por cima de uma quina de objeto, o que
criaria halo escuro) e multiplica na cena.

Roda ANTES do tonemap, na cena em HDR linear (ver RenderHBAO em gl_backend.cpp).

  u_HBAOParams2 = ( texel U do AO, texel V do AO, debug, bias )
  debug 1 = mostra so o AO em tons de cinza
*/

#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_DepthMap;
uniform sampler2D	u_HBAOMap;
uniform float		u_zFar;
uniform vec4		u_HBAOParams2;

varying vec2		var_TexCoord;

#define SKY_DEPTH		0.9999
#define VIEWMODEL_DEPTH		0.3		// mesmo valor de genhbao_fp.glsl

bool IsIgnoredDepth( float rawDepth )
{
	return rawDepth >= SKY_DEPTH || rawDepth < VIEWMODEL_DEPTH;
}

void main( void )
{
	vec2 uv = var_TexCoord;
	vec4 scene = texture2D( u_ScreenMap, uv );
	float rawCenter = texture2D( u_DepthMap, uv ).r;
	float ao = 1.0;

	if( !IsIgnoredDepth( rawCenter ))
	{
		float zc = linearizeDepth( u_zFar, rawCenter );
		vec2 texel = u_HBAOParams2.xy;
		float sum = 0.0;
		float wsum = 0.0;

		// 4x4 taps centrados no pixel: o padrao de rotacao do passo 1 repete a cada 4x4 texels
		for( int y = 0; y < 4; y++ )
		{
			for( int x = 0; x < 4; x++ )
			{
				vec2 tuv = uv + ( vec2( float( x ), float( y )) - 1.5 ) * texel;
				float rawT = texture2D( u_DepthMap, tuv ).r;
				if( IsIgnoredDepth( rawT ))
					continue;

				// peso por similaridade de profundidade, relativo a distancia (longe tolera mais)
				float dz = abs( linearizeDepth( u_zFar, rawT ) - zc ) / ( 0.05 * zc + 2.0 );
				float w = 1.0 / ( 1.0 + dz * dz * 8.0 );

				sum += texture2D( u_HBAOMap, tuv ).r * w;
				wsum += w;
			}
		}

		if( wsum > 0.001 )
			ao = sum / wsum;
	}

	if( u_HBAOParams2.z > 0.5 )
		gl_FragColor = vec4( vec3( ao ), 1.0 );
	else
		gl_FragColor = vec4( scene.rgb * ao, scene.a );
}
