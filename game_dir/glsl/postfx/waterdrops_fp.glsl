/*
waterdrops_fp.glsl - gotas de chuva na tela (RTN)

Efeito de "agua no vidro": gotas em grade, com vida propria (nascem e somem), que
refratam a cena. Roda em espaco de tela sobre a imagem ja tonemapeada.

Shader original: Elie Michel, "Rain drops on screen", julho de 2017,
shadertoy.com/view/ldSBWW - licenca CC BY 3.0 (https://creativecommons.org/licenses/by/3.0/).
Adaptado para o PrimeXT (Road to Nowhere): uniforms do projeto, GLSL 1.20
(sem round()/texture()), laco com contador inteiro e UV travado em 0..1.
O mesmo shader aparece no mod Diffusion (Aynekko), de onde veio a ideia de uso.

Diferenca deliberada para o original: o fundo NAO e amostrado com mip bias.
No original a tela inteira ficava borrada enquanto o efeito estava ativo; aqui a
cena fica nitida e so as gotas refratam (ruim de jogar com a imagem borrada).
*/

#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_ScreenMap;
uniform vec2		u_ScreenSizeInv;
uniform float		u_RealTime;
uniform vec4		u_WaterDropsParams;	// x = visibilidade (0..1), y = densidade de gotas (0..1)

varying vec2		var_TexCoord;

vec2 wd_rand( vec2 c )
{
	const mat2 m = mat2( 12.9898, 0.16180, 78.233, 0.31415 );
	return fract( sin( m * c ) * vec2( 43758.5453, 14142.1 ));
}

vec2 wd_noise( vec2 p )
{
	vec2 co = floor( p );
	vec2 mu = fract( p );
	mu = 3.0 * mu * mu - 2.0 * mu * mu * mu;
	vec2 a = wd_rand( co + vec2( 0.0, 0.0 ));
	vec2 b = wd_rand( co + vec2( 1.0, 0.0 ));
	vec2 c = wd_rand( co + vec2( 0.0, 1.0 ));
	vec2 d = wd_rand( co + vec2( 1.0, 1.0 ));
	return mix( mix( a, b, mu.x ), mix( c, d, mu.x ), mu.y );
}

void main( void )
{
	vec2 iResolution = 1.0 / u_ScreenSizeInv;
	vec2 u = var_TexCoord;

	// pequeno deslocamento por pixel, para as bordas das gotas nao ficarem retas
	vec2 n = wd_noise( u * 0.1 * 200.0 );

	vec4 inColor = texture2D( u_ScreenMap, u );
	vec4 f = inColor;

	// quatro tamanhos de gota, do menor (r = 4) ao maior (r = 1)
	for( int i = 0; i < 4; i++ )
	{
		float r = 4.0 - float( i );

		vec2 x = 1.5 * iResolution * r * 0.015;			// quantidade de gotas possiveis (grade)
		vec2 p = 6.28 * u * x + ( n - 0.5 ) * 2.0;
		vec2 s = sin( p );

		// propriedades da gota atual; coordenada arredondada para ser a mesma em
		// todos os fragmentos da mesma gota (round() nao existe em GLSL 1.20)
		vec2 v = floor( u * x - 0.25 + 0.5 ) / x;
		vec4 d = vec4( wd_noise( v * 200.0 ), wd_noise( v ));

		// forma e vida da gota
		float t = ( s.x + s.y ) * max( 0.0, 1.0 - fract( u_RealTime * ( d.b + 0.1 ) + d.g ) * 2.0 );

		// so uma fracao das gotas fica "ligada", crescendo com a densidade e com o tamanho
		if( d.r < u_WaterDropsParams.y * ( 5.0 - r ) * 0.08 && t > 0.5 )
		{
			vec3 nv = normalize( -vec3( cos( p ), mix( 0.2, 2.0, t - 0.5 )));

			// refracao barata: amostra a cena deslocada pela normal da gota
			f = texture2D( u_ScreenMap, clamp( u - nv.xy * 0.3, 0.0, 1.0 ));
		}
	}

	gl_FragColor = vec4( mix( inColor.rgb, f.rgb, u_WaterDropsParams.x ), 1.0 );
}
