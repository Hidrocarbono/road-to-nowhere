/*
lensdirt_fp.glsl - lens dirt effect (RTN F10)
Sujeira de lente estilo Battlefield: uma textura de sujeira (manchas, riscos,
poeira) e mascarada pelo brilho da cena - so aparece onde a luz bate forte
(sol, luzes acesas, explosoes). A cena jah vem tonemapeada (0..1).

Copyright (C) 2026 Hermes e Hidrocarboneto (Road to Nowhere mod)
*/

#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_DirtMap;
uniform float		u_DirtScale;

varying vec2		var_TexCoord;

void main( void )
{
	vec3 screen = texture2D( u_ScreenMap, var_TexCoord ).rgb;
	vec3 dirt   = texture2D( u_DirtMap,   var_TexCoord ).rgb;

	// bright pass: somente as areas brilhantes viram mascara (a cena esta
	// tonemapeada 0..1; abaixo de 0.7 = luz fraca demais p/ sujar a lente)
	float lum = GetLuminance( screen );
	float bright = max( lum - 0.7, 0.0 );

	// sujeira modulada pelo brilho, adicionada sobre a cena
	vec3 outColor = screen + dirt * bright * u_DirtScale;

	gl_FragColor = vec4( outColor, 1.0 );
}
