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
uniform vec2		u_DirtParams;		// x = limiar de brilho, y = modo debug
uniform vec2		u_ScreenSizeInv;

varying vec2		var_TexCoord;

/*
POR QUE A MASCARA MUDOU (o motivo de o efeito nunca ter aparecido)
------------------------------------------------------------------
A versao anterior mascarava pelo brilho DO PROPRIO PIXEL:

    float bright = max( GetLuminance( screen ) - 0.7, 0.0 );
    outColor = screen + dirt * bright * u_DirtScale;

Isso tem dois problemas, e os dois se multiplicam:

1) Mascara pontual. Sujeira de lente de verdade acende porque a luz se ESPALHA
   dentro da lente - e por isso que a mascara certa e o glow/bloom, nao a
   luminancia do pixel. Mascarando pixel a pixel, a sujeira so pode aparecer
   exatamente EM CIMA dos pixels ja estourados de luz, que e justamente onde
   nada mais e visivel. Nas bordas ao redor da lampada - onde o efeito deveria
   viver - a mascara valia zero.

2) Limiar alto demais para o jogo. A cena chega aqui tonemapeada em 0..1, e num
   jogo escuro quase nada passa de 0.7. Na pratica bright ficava zerado o tempo
   todo.

Somado a textura antiga (pico de luminancia 0.094), o teto matematico do efeito
era 0.094 * 0.3 * 0.5 = 0.014, ou seja ~3.6 de 255. Invisivel por construcao.

Agora o brilho e amostrado numa vizinhanca larga (aproximacao barata do bloom),
o limiar e um cvar, e a transicao usa smoothstep para nao ter borda dura.
*/

// Aproximacao de glow: media ponderada do brilho num raio grande. Nao substitui
// o bloom de verdade, mas custa 13 amostras num unico passe fullscreen e da o
// espalhamento que a mascara precisa.
float SampleGlow( vec2 uv )
{
	// Raio em PIXELS, convertido para UV - assim o espalhamento tem o mesmo
	// tamanho aparente em qualquer resolucao.
	vec2 r1 = u_ScreenSizeInv * 16.0;
	vec2 r2 = u_ScreenSizeInv * 40.0;

	float sum = GetLuminance( texture2D( u_ScreenMap, uv ).rgb ) * 2.0;
	float weight = 2.0;

	// anel interno (6 amostras)
	for( int i = 0; i < 6; i++ )
	{
		float a = float( i ) * 1.0472;	// 60 graus
		vec2 o = vec2( cos( a ), sin( a ) ) * r1;
		sum += GetLuminance( texture2D( u_ScreenMap, uv + o ).rgb ) * 1.0;
		weight += 1.0;
	}

	// anel externo (6 amostras, peso menor)
	for( int i = 0; i < 6; i++ )
	{
		float a = float( i ) * 1.0472 + 0.5236;	// deslocado 30 graus
		vec2 o = vec2( cos( a ), sin( a ) ) * r2;
		sum += GetLuminance( texture2D( u_ScreenMap, uv + o ).rgb ) * 0.5;
		weight += 0.5;
	}

	return sum / weight;
}

void main( void )
{
	vec3 screen = texture2D( u_ScreenMap, var_TexCoord ).rgb;

	// A textura e quadrada; sem correcao os bokehs saem ovais numa tela 16:9.
	// Corrige comprimindo o eixo X (usa a faixa central da textura) em vez de
	// esticar, que faria a UV sair de 0..1 e borrar nas bordas pelo TF_CLAMP.
	float aspect = u_ScreenSizeInv.y / u_ScreenSizeInv.x;
	vec2 dirtUV = vec2(( var_TexCoord.x - 0.5 ) / max( aspect, 1.0 ) + 0.5, var_TexCoord.y );
	vec3 dirt = texture2D( u_DirtMap, dirtUV ).rgb;

	// Modo debug (gl_lensdirt_debug 1): mostra a sujeira em cima da cena sem
	// mascara nenhuma. Serve para responder de uma vez "a textura carregou?" -
	// que era impossivel saber quando o efeito era invisivel de qualquer jeito.
	if( u_DirtParams.y > 0.5 )
	{
		gl_FragColor = vec4( screen + dirt * u_DirtScale, 1.0 );
		return;
	}

	float glow = SampleGlow( var_TexCoord );

	// smoothstep em vez de max(): a borda dura do max fazia a sujeira "ligar" de
	// um pixel para o outro conforme a camera mexia.
	float threshold = u_DirtParams.x;
	float mask = smoothstep( threshold, min( threshold + 0.35, 1.0 ), glow );

	// Realca o contraste da mascara: sujeira quase apagada em luz media parece
	// vela suja na tela; queremos que ela salte quando a luz e forte de verdade.
	mask = mask * mask;

	gl_FragColor = vec4( screen + dirt * mask * u_DirtScale, 1.0 );
}
