/*
genhbao_fp.glsl - horizon-based ambient occlusion (passe 1: gera o AO) (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Metodo: HBAO (Bavoil & Sainz, "Image-Space Horizon-Based Ambient Occlusion",
NVIDIA, SIGGRAPH 2008), implementacao propria e simplificada. Para cada pixel:
  1) reconstroi a posicao em view-space a partir do buffer de profundidade;
  2) estima a normal pelas diferencas de posicao dos vizinhos;
  3) marcha em N direcoes na tela e, em cada uma, acompanha o "horizonte"
     (seno do maior angulo acima do plano tangente); o que o horizonte sobe
     entre um passo e outro, ponderado por distancia, e a oclusao.

Roda em METADE da resolucao num alvo proprio. O padrao de rotacao/jitter repete
a cada 4x4 texels de proposito: o blur 4x4 do passo 2 (drawhbao_fp.glsl)
cancela esse padrao por construcao.

Parametros (ver gl_postprocess.cpp, UT_HBAOPARAMS / UT_HBAOPARAMS2):
  u_HBAOParams  = ( tan(fovx/2), tan(fovy/2), raio em unidades do mundo, intensidade )
  u_HBAOParams2 = ( texel U do AO, texel V do AO, debug, bias )
*/

#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_DepthMap;
uniform float		u_zFar;
uniform vec2		u_ScreenSizeInv;	// resolucao cheia
uniform vec4		u_HBAOParams;
uniform vec4		u_HBAOParams2;

varying vec2		var_TexCoord;

#define NUM_DIRECTIONS		6
#define NUM_STEPS		5
#define SKY_DEPTH		0.9999		// profundidade bruta >= isto = ceu / nada desenhado
#define VIEWMODEL_DEPTH		0.3		// o viewmodel usa o intervalo [0, 0.3] do depth range (gl_studio_draw.cpp, R_DrawViewModel)
#define MAX_FOOTPRINT		0.12		// no maximo 12% da altura da tela como alcance do AO
#define MIN_FOOTPRINT_PIXELS	3.0		// abaixo disso o AO nao vale o custo (pixel muito longe)

// 1 = pixel que nao participa do AO (ceu, ou arma na mao)
bool IsIgnoredDepth( float rawDepth )
{
	return rawDepth >= SKY_DEPTH || rawDepth < VIEWMODEL_DEPTH;
}

vec3 ViewPos( vec2 uv, float z )
{
	vec2 ndc = uv * 2.0 - 1.0;
	return vec3( ndc * u_HBAOParams.xy * z, z );	// z positivo = distancia a frente da camera
}

vec2 ClampUV( vec2 uv )
{
	return clamp( uv, u_ScreenSizeInv * 0.5, 1.0 - u_ScreenSizeInv * 0.5 );
}

vec3 PosAt( vec2 uv )
{
	float raw = texture2D( u_DepthMap, ClampUV( uv )).r;
	return ViewPos( uv, linearizeDepth( u_zFar, raw ));
}

void main( void )
{
	vec2 uv = var_TexCoord;
	float rawCenter = texture2D( u_DepthMap, uv ).r;

	if( IsIgnoredDepth( rawCenter ))
	{
		gl_FragColor = vec4( 1.0 );
		return;
	}

	float z = linearizeDepth( u_zFar, rawCenter );
	vec3 P = ViewPos( uv, z );

	float radius = u_HBAOParams.z;

	// alcance do raio projetado na tela, em UV (1 UV de altura = 2*tanY*z unidades)
	vec2 radiusUV = vec2( radius / ( 2.0 * u_HBAOParams.x * z ), radius / ( 2.0 * u_HBAOParams.y * z ));

	// limite de custo/qualidade: perto da camera o raio projetado vira gigante
	float footprint = radiusUV.y;
	if( footprint > MAX_FOOTPRINT )
		radiusUV *= MAX_FOOTPRINT / footprint;

	// longe demais: menos de MIN_FOOTPRINT_PIXELS de alcance, nao vale amostrar
	if( radiusUV.y * ( 1.0 / u_ScreenSizeInv.y ) < MIN_FOOTPRINT_PIXELS )
	{
		gl_FragColor = vec4( 1.0 );
		return;
	}

	// normal por diferencas de posicao; escolhe o vizinho mais proximo em cada eixo
	// para nao esticar a normal sobre descontinuidades de profundidade
	vec3 Pr = PosAt( uv + vec2( u_ScreenSizeInv.x, 0.0 ));
	vec3 Pl = PosAt( uv - vec2( u_ScreenSizeInv.x, 0.0 ));
	vec3 Pu = PosAt( uv + vec2( 0.0, u_ScreenSizeInv.y ));
	vec3 Pd = PosAt( uv - vec2( 0.0, u_ScreenSizeInv.y ));
	vec3 dx = ( abs( Pr.z - P.z ) < abs( P.z - Pl.z )) ? ( Pr - P ) : ( P - Pl );
	vec3 dy = ( abs( Pu.z - P.z ) < abs( P.z - Pd.z )) ? ( Pu - P ) : ( P - Pd );
	vec3 N = normalize( cross( dx, dy ));
	if( dot( N, P ) > 0.0 )
		N = -N;		// a normal precisa apontar para a camera

	// rotacao e jitter por pixel num padrao 4x4 (sequencias de baixa discrepancia)
	vec2 cell = floor( gl_FragCoord.xy );
	float idx = mod( cell.x, 4.0 ) * 4.0 + mod( cell.y, 4.0 );
	float rot = fract( idx * 0.6180339887 );
	float jit = fract( idx * 0.7548776662 + 0.5 );

	float bias = u_HBAOParams2.w;
	float total = 0.0;

	for( int k = 0; k < NUM_DIRECTIONS; k++ )
	{
		float angle = ( float( k ) + rot ) * ( 6.2831853 / float( NUM_DIRECTIONS ));
		vec2 dir = vec2( cos( angle ), sin( angle ));

		float maxSin = bias;		// o horizonte comeca no plano tangente (+ folga)
		float occ = 0.0;

		for( int i = 0; i < NUM_STEPS; i++ )
		{
			float t = ( float( i ) + jit + 0.5 ) / float( NUM_STEPS + 1 );
			vec2 suv = uv + dir * radiusUV * t;

			if( suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0 )
				continue;

			float rawS = texture2D( u_DepthMap, suv ).r;
			if( IsIgnoredDepth( rawS ))
				continue;	// ceu e viewmodel nao ocluem

			vec3 D = ViewPos( suv, linearizeDepth( u_zFar, rawS )) - P;
			float len = length( D );
			if( len < 0.001 || len > radius )
				continue;	// longe demais: nao e vizinho, e outro objeto

			float sinH = dot( N, D ) / len;
			if( sinH > maxSin )
			{
				float f = len / radius;
				occ += ( sinH - maxSin ) * ( 1.0 - f * f );
				maxSin = sinH;
			}
		}

		total += occ;
	}

	float ao = 1.0 - clamp( total / float( NUM_DIRECTIONS ) * u_HBAOParams.w, 0.0, 1.0 );
	gl_FragColor = vec4( ao, ao, ao, 1.0 );
}
