/*
hud_spriteutil.h - desenho de imagem 2D do HUD via .spr (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

POR QUE EXISTE
--------------
O caminho de textura crua (.tga via LOAD_TEXTURE + GL_Bind + OrthoQuad) gera
GL_INVALID_ENUM todo frame neste engine (medido, ver CHANGELOG_AGENT.md secao 4).
O caminho que funciona e .spr v32 (truecolor RGBA) desenhado por
DrawSpriteAsPoly. Este header concentra o prototipo (que antes era repetido
por extern em cada .cpp) e dois helpers usados por hud_radio, hud_systemtip e
hud_textwindow.

Gerar o .spr a partir de um .tga:
    python3 tools/tga2spr.py <in.tga> <out.spr>
.spr v2/indexado aparece como caixa preta - tem que ser o v32 do tga2spr.py.

Header-only de proposito (nao exige mexer no CMakeLists).
*/

#ifndef HUD_SPRITEUTIL_H
#define HUD_SPRITEUTIL_H

#include "filesystem_utils.h"	// fs::FileExists

// client/render/tri.cpp - sem header proprio, so este prototipo
extern void DrawSpriteAsPoly( SpriteHandle hspr, wrect_t *rect, wrect_t *screenpos, int mode, float r, float g, float b, float a );

// Carrega um .spr de HUD SE ele existir. SPR_Load imprime erro vermelho no
// console para todo sprite ausente; imagem decorativa e opcional, entao
// checamos antes. Devolve 0 se faltar.
static inline SpriteHandle RTN_LoadOptionalSprite( const char *pszPath )
{
	if( !pszPath || !pszPath[0] || !fs::FileExists( pszPath ))
		return 0;
	return LoadSprite( pszPath );
}

// Desenha o quadro 0 do sprite INTEIRO esticado em (x1,y1)-(x2,y2).
// r,g,b,a em 0..1. Sprite invalido (0) nao desenha nada.
static inline void RTN_DrawSpriteStretched( SpriteHandle hspr, int x1, int y1, int x2, int y2, float r, float g, float b, float a )
{
	if( !hspr )
		return;

	// wrect_t e { left, right, top, bottom } (common/wrect.h) - preencher campo
	// a campo, NUNCA por lista posicional.
	wrect_t src;
	src.left = 0;
	src.top = 0;
	src.right = SPR_Width( hspr, 0 );
	src.bottom = SPR_Height( hspr, 0 );

	wrect_t dst;
	dst.left = x1;
	dst.top = y1;
	dst.right = x2;
	dst.bottom = y2;

	DrawSpriteAsPoly( hspr, &src, &dst, kRenderTransTexture, r, g, b, a );
}

#endif // HUD_SPRITEUTIL_H
