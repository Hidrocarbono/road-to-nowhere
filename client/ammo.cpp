/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// Ammo.cpp
//
// implementation of CHudAmmo class
//

#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "ammohistory.h"
#include "filesystem_utils.h"	// fs::FileExists - icone da barra de selecao e opcional (RTN)
#include "const.h"		// kRenderTransTexture (DrawSpriteAsPoly)
#include "triangleapi.h"	// RTN: pTriAPI->RenderMode/Color4f - fallback .tga do icone (mesma tripa do hud_weaponbox.cpp)
#include "gl_local.h"		// RTN: GL_Bind - idem
#include "gl_debug.h"		// RTN: GL_CheckForErrors (so com rtn_hud_selectbar_gldebug 1)

// DrawSpriteAsPoly (client/render/tri.cpp) nao tem header proprio - mesmo
// padrao ja usado em hud_titlefont.cpp/hud_textwindow.cpp pras funcoes
// "irmas" dela.
extern void DrawSpriteAsPoly( SpriteHandle hspr, wrect_t *rect, wrect_t *screenpos, int mode, float r, float g, float b, float a );
// definido em client/hud_textwindow.cpp - desenha uma textura crua em 2D (RTN: fallback .tga do icone)
extern void OrthoQuad( int x1, int y1, int x2, int y2 );

int		g_weaponselect = 0;
WEAPON		*gpActiveSel;	// NULL means off, 1 means just the menu bar, otherwise
WEAPON		*gpLastSel;	// Last weapon menu selection
static wrect_t	nullRc;
WeaponsResource	gWR;

int WeaponsResource :: HasAmmo( WEAPON *p )
{
	if( !p )
		return FALSE;

	// weapons with no max ammo can always be selected
	if( p->iMax1 == -1 )
		return TRUE;

	return (p->iAmmoType == -1) || p->iClip > 0 || CountAmmo( p->iAmmoType )
		|| CountAmmo( p->iAmmo2Type ) || ( p->iFlags & WEAPON_FLAGS_SELECTONEMPTY );
}

#define WEAPON_SELECT_BAR_MAX		16	// mais que suficiente - MAX_WEAPONS e 64, mas ninguem carrega 16+ armas ao mesmo tempo
#define WEAPON_SELECT_BAR_VISIBLE	7	// quantos itens cabem desenhados por vez (impar - sobra um no centro)
#define WEAPON_SELECT_BAR_TIMEOUT	2.0f	// segundos parada ate a barra sumir sozinha

// RTN: garante que o icone da arma esteja carregado (uma vez so por arma).
// Mesma convencao/ordem do CHudWeaponBox (client/hud_weaponbox.cpp):
//   1) sprites/rtn_hud_ammo_<classname>.spr
//   2) gfx/vgui/ammo/640_<classname>.tga (textura crua, sem conversao pra .spr)
// Sem os dois, o item aparece so com nome/municao em texto - a barra
// continua usavel, so sem icone.
static void RTN_EnsureBoxIcon( WEAPON *p )
{
	if( !p || p->bBoxIconLoaded )
		return;

	p->bBoxIconLoaded = true;

	char szSpr[64];
	Q_snprintf( szSpr, sizeof( szSpr ), "sprites/rtn_hud_ammo_%s.spr", p->szName );
	p->hBoxSpr = fs::FileExists( szSpr ) ? LoadSprite( szSpr ) : 0;

	if( !p->hBoxSpr )
	{
		char szTga[96];
		Q_snprintf( szTga, sizeof( szTga ), "gfx/vgui/ammo/640_%s.tga", p->szName );
		if( fs::FileExists( szTga ))
		{
			p->hBoxTex = LOAD_TEXTURE( szTga, NULL, 0, TF_CLAMP | TF_IMAGE | TF_HAS_ALPHA );

			// RTN: TextureHandle::Initialized() so confere se existe um
			// handle (indice != 0), NAO se o arquivo carregou pixels de
			// verdade - um .tga que o engine aceitou mas nao decodificou
			// direito (formato/profundidade de cor que o loader nao
			// entende) pode voltar com handle valido e 0x0 de tamanho.
			// Desenhar/bindar isso e terreno arriscado (handle "valido"
			// mas sem textura de verdade por tras) - melhor cair pro
			// nome em texto do que arriscar.
			if( p->hBoxTex.Initialized() && ( p->hBoxTex.GetWidth() == 0 || p->hBoxTex.GetHeight() == 0 ))
			{
				FREE_TEXTURE( p->hBoxTex );
				p->hBoxTex = TextureHandle::Null();
			}
		}
	}
}

// RTN: em 0, a barra de selecao nao desenha nenhum icone (so texto) - valvula
// pra isolar em jogo se o GL_INVALID_ENUM vem do desenho de textura crua.
cvar_t *rtn_hud_selectbar_icons = NULL;

// RTN: em 1, checa o erro de GL DEPOIS DE CADA chamada do desenho de icone.
// Cada checagem esta numa linha diferente, e o GL_CheckForErrors imprime
// "<erro> (at <funcao>:<linha>)" - entao o console passa a dizer exatamente
// QUAL chamada sujou o estado, em vez de so acusar o R_RenderScene:976 (que e
// apenas onde o engine varre a fila; glGetError e sticky e nao aponta origem).
cvar_t *rtn_hud_selectbar_gldebug = NULL;

// RTN: a arma tem algum icone carregado (spr OU tga)? Serve pra separar o
// desenho em dois passos: primeiro so os icones (TriAPI), depois so texto e
// retangulos (2D do engine) - sem intercalar as duas APIs.
static bool RTN_HasBoxIcon( WEAPON *p )
{
	if( rtn_hud_selectbar_icons && rtn_hud_selectbar_icons->value < 1.0f )
		return false;	// icones desligados: tudo cai no fallback de texto

	return ( p->hBoxSpr != 0 ) || p->hBoxTex.Initialized();
}

// RTN: proporcao largura/altura do icone carregado (spr OU tga - o que tiver),
// pra nao esticar/achatar o desenho. false se nao ha icone nenhum ainda.
static bool RTN_GetBoxIconAspect( WEAPON *p, float *outAspect )
{
	if( !RTN_HasBoxIcon( p ))
		return false;	// sem icone: usa a largura nominal do fallback de texto

	if( p->hBoxSpr )
	{
		int sw = SPR_Width( p->hBoxSpr, 0 );
		int sh = SPR_Height( p->hBoxSpr, 0 );
		if( sw > 0 && sh > 0 )
		{
			*outAspect = (float)sw / (float)sh;
			return true;
		}
	}
	else if( p->hBoxTex.Initialized() )
	{
		uint32_t tw = p->hBoxTex.GetWidth();
		uint32_t th = p->hBoxTex.GetHeight();
		if( tw > 0 && th > 0 )
		{
			*outAspect = (float)tw / (float)th;
			return true;
		}
	}
	return false;
}

// RTN: desenha o icone da arma no retangulo (x,y,x+w,y+h) - .spr via
// DrawSpriteAsPoly (client/render/tri.cpp), .tga via textura crua (mesma
// tripa TriAPI+OrthoQuad que o CHudWeaponBox ja usa, client/hud_weaponbox.cpp).
// Retorna false se a arma nao tem icone nenhum ainda (chamador cai pro
// texto do nome).
static bool RTN_DrawBoxIcon( WEAPON *p, int x, int y, int w, int h, float r, float g, float b, float alpha )
{
	if( !RTN_HasBoxIcon( p ))
		return false;	// sem icone, ou icones desligados pelo cvar

	if( p->hBoxSpr )
	{
		int sw = SPR_Width( p->hBoxSpr, 0 );
		int sh = SPR_Height( p->hBoxSpr, 0 );
		if( sw <= 0 ) sw = 1;
		if( sh <= 0 ) sh = 1;

		wrect_t srcRect = { 0, 0, sw, sh };
		wrect_t dstRect = { x, y, x + w, y + h };
		// NOTA: DrawSpriteAsPoly mexe em CullFace por conta propria (tri.cpp).
		// Hoje nenhuma arma usa .spr, entao esse caminho nao roda; se voltar a
		// rodar com varios icones, vale medir se esse churn traz de volta o
		// GL_INVALID_ENUM - foi ele que motivou o lote unico do .tga abaixo.
		DrawSpriteAsPoly( p->hBoxSpr, &srcRect, &dstRect, kRenderTransTexture, r, g, b, alpha );
		return true;
	}

	if( p->hBoxTex.Initialized() )
	{
		// RTN: instrumentacao opcional (rtn_hud_selectbar_gldebug 1) - ver o
		// comentario do cvar la em cima. A primeira checagem DRENA a fila de
		// erros que ja vinha de tras, pra nao culpar este codigo por erro dos
		// outros; as seguintes e que apontam de verdade.
		bool dbg = ( rtn_hud_selectbar_gldebug && rtn_hud_selectbar_gldebug->value >= 1.0f );
		if( dbg ) GL_CheckForErrors();	// (dreno) erro anterior, NAO e daqui

		// RTN: NAO mexe em CullFace nem reseta RenderMode aqui - quem chama
		// e que segura o estado em volta do lote inteiro de icones (ver
		// DrawWeaponSelectBar). Fazer isso por icone era o que escalava com
		// a quantidade de armas na barra.
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
		if( dbg ) GL_CheckForErrors();	// culpa: TriAPI RenderMode

		gEngfuncs.pTriAPI->Color4f( r, g, b, alpha );
		if( dbg ) GL_CheckForErrors();	// culpa: TriAPI Color4f

		// RTN: o GL_Bind do render api e CACHEADO (common/render_api.h avisa
		// que ele existe justamente pra manter o estado sincronizado entre
		// engine e client). Como o renderer do mod (client/render/) liga
		// textura direto por pgl* durante a cena, esse cache pode chegar aqui
		// dessincronizado do GL real: ele acha que hBoxTex ja esta ligada, o
		// bind vira no-op, e o quad sai pintado com a ultima textura que
		// ficou ligada de fato - exatamente o sintoma do quadrado que virou a
		// arte do menu depois de abrir o menu.
		//
		// Ligar uma textura conhecida antes forca o cache a mudar de valor,
		// garantindo que o bind seguinte emita um glBindTexture real. Usa a
		// "*white" (mesma que o hud_radio.cpp ja usa) em vez de handle nulo.
		GL_Bind( 0, FIND_TEXTURE( "*white" ));
		if( dbg ) GL_CheckForErrors();	// culpa: GL_Bind da "*white"

		GL_Bind( 0, p->hBoxTex );
		if( dbg ) GL_CheckForErrors();	// culpa: GL_Bind do icone (.tga)

		OrthoQuad( x, y, x + w, y + h );
		if( dbg ) GL_CheckForErrors();	// culpa: OrthoQuad (Begin/TexCoord/Vertex/End)

		return true;
	}

	return false;
}

// RTN: monta a lista de armas que o jogador tem AGORA, ordenada por peso
// (leve -> pesada - campo "weight" do script de arma / ItemInfo::iWeight,
// enviado ao client no WeaponList - ver server/player.cpp). Sem bucket nem
// posicao: e so a ordem natural da lista, o que elimina de vez a classe de
// bug de colisao de slot que o menu antigo (DrawWList/rgSlots) tinha - ver
// o historico de commits deste arquivo. Inclui armas sem municao tambem
// (a barra so pinta elas diferente, igual o menu antigo fazia).
static int BuildOwnedWeaponList( WEAPON *out[], int maxCount )
{
	int count = 0;

	for( int i = 1; i < MAX_WEAPONS && count < maxCount; i++ )
	{
		WEAPON *p = gWR.GetWeapon( i );
		if( p && p->iId && gHUD.HasWeapon( p->iId ))
			out[count++] = p;
	}

	// insertion sort - poucas armas (dezenas no maximo), nao vale a pena nada mais esperto
	for( int i = 1; i < count; i++ )
	{
		WEAPON *key = out[i];
		int j = i - 1;
		while( j >= 0 && out[j]->iWeight > key->iWeight )
		{
			out[j + 1] = out[j];
			j--;
		}
		out[j + 1] = key;
	}

	return count;
}

// RTN: resolve o caminho de um sprite de HUD listado num manifesto
// sprites/<classname>.txt, aceitando as DUAS convencoes que aparecem nesses
// arquivos neste projeto:
//
//   1) nome nu, ex. "weapon_mp5"          (convencao classica do engine -
//      pfnSPR_GetList/hud.txt: quem monta "sprites/" + ".spr" e o CHAMADOR)
//   2) caminho completo, ex. "sprites/weapon_parafal.spr" (convencao do
//      Paranoia 2 - cl_dll/ammo.cpp chama SPR_Load(p->szSprite) direto, sem
//      prefixar/sufixar nada, entao o manifesto do P2 ja traz o caminho todo)
//
// Ports de arma do P2 trazem o manifesto de sprite (scripts/weapons/<nome>.txt
// la, sprites/<nome>.txt aqui) no formato (2) - sem essa deteccao, colar o
// arquivo como veio de la resultava em "sprites/" + "sprites/x.spr" + ".spr"
// (caminho inexistente, SPR_Load falha em silencio, icone da arma some do
// menu de selecao). Com isso, um manifesto de sprite portado do P2 funciona
// SEM EDICAO NENHUMA - so o script de logica (bucket/clip/etc, formato
// diferente, ver weaponscript.cpp) precisa ser adaptado.
static void RTN_ResolveWeaponSpritePath( char *out, size_t outSize, const char *szSprite )
{
	bool hasDir = strchr( szSprite, '/' ) != NULL;
	size_t len = strlen( szSprite );
	bool hasExt = ( len > 4 && !Q_strnicmp( szSprite + len - 4, ".spr", 4 ));

	if( hasDir && hasExt )
		Q_snprintf( out, outSize, "%s", szSprite );		// P2-style: ja e o caminho completo
	else if( hasDir )
		Q_snprintf( out, outSize, "%s.spr", szSprite );
	else if( hasExt )
		Q_snprintf( out, outSize, "sprites/%s", szSprite );
	else
		Q_snprintf( out, outSize, "sprites/%s.spr", szSprite );	// convencao classica (nome nu)
}

void WeaponsResource :: LoadWeaponSprites( WEAPON *pWeapon )
{
	int i, iRes;

	if( ScreenWidth < 640 )
		iRes = 320;
	else iRes = 640;

	char sz[128];

	if ( !pWeapon ) return;

	memset( &pWeapon->rcActive, 0, sizeof( wrect_t ));
	memset( &pWeapon->rcInactive, 0, sizeof( wrect_t ));
	memset( &pWeapon->rcAmmo, 0, sizeof( wrect_t ));
	memset( &pWeapon->rcAmmo2, 0, sizeof( wrect_t ));
	pWeapon->hInactive = 0;
	pWeapon->hActive = 0;
	pWeapon->hAmmo = 0;
	pWeapon->hAmmo2 = 0;
	
	Q_snprintf( sz, sizeof( sz ), "sprites/%s.txt", pWeapon->szName );
	client_sprite_t *pList = SPR_GetList( sz, &i );

	if( !pList ) return;

	client_sprite_t *p;
	
	p = GetSpriteList( pList, "crosshair", iRes, i );
	if( p )
	{
		RTN_ResolveWeaponSpritePath( sz, sizeof( sz ), p->szSprite );
		pWeapon->hCrosshair = SPR_Load( sz );
		pWeapon->rcCrosshair = p->rc;
	}
	else pWeapon->hCrosshair = 0;

	p = GetSpriteList( pList, "autoaim", iRes, i );
	if( p )
	{
		RTN_ResolveWeaponSpritePath( sz, sizeof( sz ), p->szSprite );
		pWeapon->hAutoaim = SPR_Load( sz );
		pWeapon->rcAutoaim = p->rc;
	}
	else pWeapon->hAutoaim = 0;

	p = GetSpriteList( pList, "zoom", iRes, i );
	if( p )
	{
		RTN_ResolveWeaponSpritePath( sz, sizeof( sz ), p->szSprite );
		pWeapon->hZoomedCrosshair = SPR_Load( sz );
		pWeapon->rcZoomedCrosshair = p->rc;
	}
	else
	{
		pWeapon->hZoomedCrosshair = pWeapon->hCrosshair; // default to non-zoomed crosshair
		pWeapon->rcZoomedCrosshair = pWeapon->rcCrosshair;
	}

	p = GetSpriteList( pList, "zoom_autoaim", iRes, i );
	if( p )
	{
		RTN_ResolveWeaponSpritePath( sz, sizeof( sz ), p->szSprite );
		pWeapon->hZoomedAutoaim = SPR_Load( sz );
		pWeapon->rcZoomedAutoaim = p->rc;
	}
	else
	{
		pWeapon->hZoomedAutoaim = pWeapon->hZoomedCrosshair;  // default to zoomed crosshair
		pWeapon->rcZoomedAutoaim = pWeapon->rcZoomedCrosshair;
	}

	p = GetSpriteList( pList, "weapon", iRes, i );
	if( p )
	{
		RTN_ResolveWeaponSpritePath( sz, sizeof( sz ), p->szSprite );
		pWeapon->hInactive = SPR_Load( sz );
		pWeapon->rcInactive = p->rc;
		gHR.iHistoryGap = Q_max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
	{
		pWeapon->hInactive = gHUD.m_hHudError;
		pWeapon->rcInactive = gHUD.GetSpriteRect( gHUD.m_HUD_error );
		gHR.iHistoryGap = Q_max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}

	p = GetSpriteList( pList, "weapon_s", iRes, i );
	if( p )
	{
		RTN_ResolveWeaponSpritePath( sz, sizeof( sz ), p->szSprite );
		pWeapon->hActive = SPR_Load( sz );
		pWeapon->rcActive = p->rc;
	}
	else
	{
		pWeapon->hActive = gHUD.m_hHudError;
		pWeapon->rcActive = gHUD.GetSpriteRect( gHUD.m_HUD_error );
	}

	p = GetSpriteList( pList, "ammo", iRes, i );
	if( p )
	{
		RTN_ResolveWeaponSpritePath( sz, sizeof( sz ), p->szSprite );
		pWeapon->hAmmo = SPR_Load( sz );
		pWeapon->rcAmmo = p->rc;
		gHR.iHistoryGap = Q_max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else pWeapon->hAmmo = 0;

	p = GetSpriteList( pList, "ammo2", iRes, i );
	if( p )
	{
		RTN_ResolveWeaponSpritePath( sz, sizeof( sz ), p->szSprite );
		pWeapon->hAmmo2 = SPR_Load( sz );
		pWeapon->rcAmmo2 = p->rc;
		gHR.iHistoryGap = Q_max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else pWeapon->hAmmo2 = 0;
}

// Returns the first weapon for a given slot.
WEAPON *WeaponsResource :: GetFirstPos( int iSlot )
{
	WEAPON *pret = NULL;

	for( int i = 0; i < MAX_WEAPON_POSITIONS; i++ )
	{
		if( rgSlots[iSlot][i] && HasAmmo( rgSlots[iSlot][i] ))
		{
			pret = rgSlots[iSlot][i];
			break;
		}
	}
	return pret;
}

WEAPON* WeaponsResource :: GetNextActivePos( int iSlot, int iSlotPos )
{
	if( iSlotPos >= MAX_WEAPON_POSITIONS || iSlot >= MAX_WEAPON_SLOTS )
		return NULL;

	WEAPON *p = gWR.rgSlots[iSlot][iSlotPos+1];
	
	if( !p || !gWR.HasAmmo(p) )
		return GetNextActivePos( iSlot, iSlotPos + 1 );

	return p;
}

int	giBucketHeight;		// Ammo Bar width and height
int	giBucketWidth;
int	giABHeight;
int	giABWidth;

SpriteHandle	ghsprBuckets;		// Sprite for top row of weapons menu

DECLARE_MESSAGE( m_Ammo, CurWeapon  );	// Current weapon and clip
DECLARE_MESSAGE( m_Ammo, WeaponList );	// new weapon type
DECLARE_MESSAGE( m_Ammo, AmmoX );	// update known ammo type's count
DECLARE_MESSAGE( m_Ammo, AmmoPickup );	// flashes an ammo pickup record
DECLARE_MESSAGE( m_Ammo, WeapPickup );	// flashes a weapon pickup record
DECLARE_MESSAGE( m_Ammo, HideWeapon );	// hides the weapon, ammo, and crosshair displays temporarily
DECLARE_MESSAGE( m_Ammo, ItemPickup );

DECLARE_COMMAND( m_Ammo, Slot1 );
DECLARE_COMMAND( m_Ammo, Slot2 );
DECLARE_COMMAND( m_Ammo, Slot3 );
DECLARE_COMMAND( m_Ammo, Slot4 );
DECLARE_COMMAND( m_Ammo, Slot5 );
DECLARE_COMMAND( m_Ammo, Slot6 );
DECLARE_COMMAND( m_Ammo, Slot7 );
DECLARE_COMMAND( m_Ammo, Slot8 );
DECLARE_COMMAND( m_Ammo, Slot9 );
DECLARE_COMMAND( m_Ammo, Slot10 );
DECLARE_COMMAND( m_Ammo, Close );
DECLARE_COMMAND( m_Ammo, NextWeapon );
DECLARE_COMMAND( m_Ammo, PrevWeapon );
DECLARE_COMMAND( m_Ammo, NVG_Toggle );

// width of ammo fonts
#define AMMO_SMALL_WIDTH	10
#define AMMO_LARGE_WIDTH	20
#define HISTORY_DRAW_TIME	"5"

int CHudAmmo::Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( CurWeapon );
	HOOK_MESSAGE( WeaponList );
	HOOK_MESSAGE( AmmoPickup );
	HOOK_MESSAGE( WeapPickup );
	HOOK_MESSAGE( ItemPickup );
	HOOK_MESSAGE( HideWeapon );
	HOOK_MESSAGE( AmmoX );

	HOOK_COMMAND( "slot1", Slot1 );
	HOOK_COMMAND( "slot2", Slot2 );
	HOOK_COMMAND( "slot3", Slot3 );
	HOOK_COMMAND( "slot4", Slot4 );
	HOOK_COMMAND( "slot5", Slot5 );
	HOOK_COMMAND( "slot6", Slot6 );
	HOOK_COMMAND( "slot7", Slot7 );
	HOOK_COMMAND( "slot8", Slot8 );
	HOOK_COMMAND( "slot9", Slot9 );
	HOOK_COMMAND( "slot10", Slot10 );
	HOOK_COMMAND( "cancelselect", Close );
	HOOK_COMMAND( "invnext", NextWeapon );
	HOOK_COMMAND( "invprev", PrevWeapon );
	HOOK_COMMAND( "nvg", NVG_Toggle );

	Reset();

	CVAR_REGISTER( "hud_drawhistory_time", HISTORY_DRAW_TIME, 0 );

	// controls whether or not weapons can be selected in one keypress
	CVAR_REGISTER( "hud_fastswitch", "0", FCVAR_ARCHIVE );

	// RTN: valvula de diagnostico pro caso do GL_INVALID_ENUM. Em 0, a barra
	// de selecao desenha SO texto (nenhum quad de textura crua). Se o erro
	// sumir com rtn_hud_selectbar_icons 0 e voltar com 1, a origem esta
	// confirmada no desenho dos icones; se continuar dos dois jeitos, a
	// origem e outra e nao adianta mexer mais aqui.
	if( !rtn_hud_selectbar_icons )
		rtn_hud_selectbar_icons = gEngfuncs.pfnRegisterVariable( "rtn_hud_selectbar_icons", "1", FCVAR_ARCHIVE );

	// RTN: instrumentacao pra achar QUAL chamada do desenho de icone suja o
	// estado de GL (ver o comentario do cvar no topo do arquivo). Default 0 -
	// so liga na mao pra diagnosticar, porque checar erro de GL a cada chamada
	// e caro (pglGetError sincroniza a pipeline).
	if( !rtn_hud_selectbar_gldebug )
		rtn_hud_selectbar_gldebug = gEngfuncs.pfnRegisterVariable( "rtn_hud_selectbar_gldebug", "0", 0 );

	m_iFlags |= HUD_ACTIVE; //!!!

	gWR.Init();
	gHR.Init();

	return 1;
}

void CHudAmmo::Reset( void )
{
	m_fFade = 0;
	m_iFlags |= HUD_ACTIVE; //!!!

	gpActiveSel = NULL;
	gHUD.m_iHideHUDDisplay = 0;
	m_flSelectMenuTime = 0.0f;

	gWR.Reset();
	gHR.Reset();

	SetCrosshair( 0, nullRc, 0, 0, 0 );	// reset crosshair
	m_pWeapon = NULL;			// reset last weapon
}

int CHudAmmo::VidInit( void )
{
	// Load sprites for buckets (top row of weapon menu)
	m_HUD_bucket0 = gHUD.GetSpriteIndex( "bucket1" );
	m_HUD_selection = gHUD.GetSpriteIndex( "selection" );

	ghsprBuckets = gHUD.GetSprite( m_HUD_bucket0 );
	giBucketWidth = gHUD.GetSpriteRect( m_HUD_bucket0 ).right - gHUD.GetSpriteRect( m_HUD_bucket0 ).left;
	giBucketHeight = gHUD.GetSpriteRect( m_HUD_bucket0 ).bottom - gHUD.GetSpriteRect( m_HUD_bucket0 ).top;

	gHR.iHistoryGap = Q_max( gHR.iHistoryGap, gHUD.GetSpriteRect( m_HUD_bucket0 ).bottom - gHUD.GetSpriteRect( m_HUD_bucket0 ).top );

	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	if( ScreenWidth >= 640 )
	{
		giABWidth = 20;
		giABHeight = 4;
	}
	else
	{
		giABWidth = 10;
		giABHeight = 2;
	}

	return 1;
}

//
// Think:
//  Used for selection of weapon menu item.
//
void CHudAmmo::Think( void )
{
	if( gHUD.m_fPlayerDead )
		return;

	if( memcmp( gHUD.m_iWeaponBits, gWR.iOldWeaponBits, MAX_WEAPON_BYTES ))
	{
		memcpy( gWR.iOldWeaponBits, gHUD.m_iWeaponBits, MAX_WEAPON_BYTES );

		// RTN: mantido so por compatibilidade (rgSlots/PickupWeapon/DropWeapon
		// nao alimentam mais nenhum desenho - a barra de selecao nova, mais
		// abaixo, monta a propria lista via BuildOwnedWeaponList a cada
		// frame). Deixar rodando e mais seguro do que arriscar quebrar algo
		// que ainda leia gWR.GetWeaponSlot()/rgSlots por fora deste arquivo.
		for( int i = MAX_WEAPONS - 1; i > 0; i-- )
		{
			WEAPON *p = gWR.GetWeapon( i );

			if( p )
			{
				if( gHUD.HasWeapon( p->iId ))
					gWR.PickupWeapon( p );
				else
					gWR.DropWeapon( p );
			}
		}
	}

	// RTN: a barra de selecao troca de arma NA HORA a cada giro de rodinha
	// (ver UserCmd_NextWeapon/PrevWeapon) - nao ha mais um estado "destacado
	// mas nao confirmado" pra confirmar com IN_ATTACK, entao o clique de
	// ataque nunca mais e interceptado aqui. So sobra apagar a barra sozinha
	// depois de um tempo parada.
	if( gpActiveSel && ( gEngfuncs.GetClientTime() - m_flSelectMenuTime ) > WEAPON_SELECT_BAR_TIMEOUT )
		gpActiveSel = NULL;
}

//
// Helper function to return a Ammo pointer from id
//
SpriteHandle* WeaponsResource :: GetAmmoPicFromWeapon( int iAmmoId, wrect_t& rect )
{
	for( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if( rgWeapons[i].iAmmoType == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo;
			return &rgWeapons[i].hAmmo;
		}
		else if( rgWeapons[i].iAmmo2Type == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo2;
			return &rgWeapons[i].hAmmo2;
		}
	}
	return NULL;
}


// Menu Selection Code
void WeaponsResource :: SelectSlot( int iSlot, int fAdvance, int iDirection )
{
	if( gHUD.m_Menu.m_fMenuDisplayed && ( fAdvance == FALSE ) && ( iDirection == 1 ))	
	{
		// menu is overriding slot use commands
		gHUD.m_Menu.SelectMenuItem( iSlot + 1 );  // slots are one off the key numbers
		return;
	}

	if( iSlot > MAX_WEAPON_SLOTS )
		return;

	if( gHUD.m_fPlayerDead || gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ))
		return;

	// RTN: a selecao de arma NAO depende mais do traje.
	//
	// No Half-Life o HEV e o "computador" que desenha o HUD, entao todo o menu de
	// armas era bloqueado sem ele. No RTN o traje passa a ser um COLETE - protege,
	// mas nao e o que permite trocar de arma. Sem esta mudanca, a selecao pela
	// rodinha do mouse simplesmente nunca aparecia, porque o jogador nunca pega
	// item_suit.
	if ( !memcmp( gHUD.m_iWeaponBits, nullbits, sizeof( gHUD.m_iWeaponBits )))
		return;

	WEAPON *p = NULL;
	bool fastSwitch = CVAR_GET_FLOAT( "hud_fastswitch" ) != 0;

	if(( gpActiveSel == NULL ) || ( gpActiveSel == (WEAPON *)1 ) || ( iSlot != gpActiveSel->iSlot ))
	{
		PlaySound( "common/wpn_hudon.wav", 1 );
		p = GetFirstPos( iSlot );

		if( p && fastSwitch ) // check for fast weapon switch mode
		{
			// if fast weapon switch is on, then weapons can be selected in a single keypress
			// but only if there is only one item in the bucket
			WEAPON *p2 = GetNextActivePos( p->iSlot, p->iSlotPos );

			if( !p2 )
			{	
				// only one active item in bucket, so change directly to weapon
				ServerCmd( p->szName );
				g_weaponselect = p->iId;
				return;
			}
		}
	}
	else
	{
		PlaySound( "common/wpn_moveselect.wav", 1 );
		if( gpActiveSel )
			p = GetNextActivePos( gpActiveSel->iSlot, gpActiveSel->iSlotPos );
		if( !p )
			p = GetFirstPos( iSlot );
	}

	
	if( !p )  // no selection found
	{
		// just display the weapon list, unless fastswitch is on just ignore it
		if( !fastSwitch )
			gpActiveSel = (WEAPON *)1;
		else
			gpActiveSel = NULL;
	}
	else 
		gpActiveSel = p;
}

//------------------------------------------------------------------------
// Message Handlers
//------------------------------------------------------------------------

//
// AmmoX  -- Update the count of a known type of ammo
// 
int CHudAmmo::MsgFunc_AmmoX( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	gWR.SetAmmo( iIndex, abs( iCount ));

	END_READ();

	return 1;
}

int CHudAmmo::MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int iIndex = READ_BYTE();	// so avanca o cursor de leitura - AddToHistory (unico uso) esta desligado abaixo
	(void)iIndex;
	int iCount = READ_BYTE();

	// RTN: desligado de proposito (igual o Paranoia 2 fez em cl_dll/ammo.cpp)
	// - a mensagem do titles.txt logo abaixo ja mostra o pickup de municao,
	// com a quantidade real (%d). Com os dois ativos ao mesmo tempo, o
	// jogador via a mensagem customizada E o numero padrao do sistema no
	// canto inferior direito (gHR.DrawAmmoHistory, ainda chamado no Draw()
	// mas sem nada pra desenhar - a lista so recebe item por AddToHistory).
	// gHR.AddToHistory( HISTSLOT_AMMO, iIndex, abs( iCount ));

	// RTN F10: pickup message do titles.txt ("!<nome>" - estilo P2 ammo.cpp)
	// Nao mostra se nao achar a entrada no titles.txt (silencioso). iCount
	// vai como iArg - substitui um "%d" no texto da mensagem pela
	// quantidade de verdade (ver client/message.cpp::MessageDrawScan).
	const char *szAmmoName = READ_STRING();
	if( szAmmoName && szAmmoName[0] )
	{
		char msgname[256];
		Q_snprintf( msgname, sizeof( msgname ), "!%s", szAmmoName );
		gHUD.m_Message.MessageAdd( msgname, gEngfuncs.GetClientTime( ), abs( iCount ));
	}

	END_READ();

	return 1;
}

int CHudAmmo::MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int iIndex = READ_BYTE();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_WEAP, iIndex );

	// RTN F10: pickup message da arma ("!weapon_<nome>" - o titles.txt - P2)
	// o server envia o NOME (string) - nao depende do iIndex (o m_iId do
	// script nao e o iIndex do WeaponList - bug antigo: nada aparecia)
	const char *szWeaponName = READ_STRING();
	if ( szWeaponName && szWeaponName[0] )
	{
		char msgname[256];
		Q_snprintf( msgname, sizeof( msgname ), "!%s", szWeaponName );
		gHUD.m_Message.MessageAdd( msgname, gEngfuncs.GetClientTime() );
	}

	END_READ();

	return 1;
}

int CHudAmmo::MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	const char *szName = READ_STRING();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_ITEM, szName );

	END_READ();

	return 1;
}

int CHudAmmo::MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	
	gHUD.m_iHideHUDDisplay = READ_BYTE();

	if(( m_pWeapon == NULL ) || ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL )))
	{
		gpActiveSel = NULL;
		SetCrosshair( 0, nullRc, 0, 0, 0 );
	}
	else
	{
		SetCrosshair( m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255 );
	}

	END_READ();

	return 1;
}

// 
//  CurWeapon: Update hud state with the current weapon and clip count. Ammo
//  counts are updated with AmmoX. Server assures that the Weapon ammo type 
//  numbers match a real ammo type.
//
int CHudAmmo::MsgFunc_CurWeapon(const char *pszName, int iSize, void *pbuf )
{
	int fOnTarget = FALSE;

	BEGIN_READ( pszName, pbuf, iSize );

	int iState = READ_BYTE();
	int iId = READ_CHAR();
	int iClip = READ_CHAR();

	// detect if we're also on target
	if( iState > 1 )
	{
		fOnTarget = TRUE;
	}

	if( iId < 1 )
	{
		SetCrosshair( 0, nullRc, 0, 0, 0 );
		m_pWeapon = NULL;
		return 0;
	}

	// Is player dead???
	if(( iId == -1 ) && ( iClip == -1 ))
	{
		gHUD.m_fPlayerDead = TRUE;
		gpActiveSel = NULL;
		return 1;
	}

	gHUD.m_fPlayerDead = FALSE;

	WEAPON *pWeapon = gWR.GetWeapon( iId );

	if( !pWeapon )
		return 0;

	if( iClip < -1 )
		pWeapon->iClip = abs( iClip );
	else
		pWeapon->iClip = iClip;


	if( iState == 0 )	// we're not the current weapon, so update no more
		return 1;

	m_pWeapon = pWeapon;

	if( gHUD.m_iFOV >= 90 )
	{ 
		// normal crosshairs
		if( fOnTarget && m_pWeapon->hAutoaim )
			SetCrosshair( m_pWeapon->hAutoaim, m_pWeapon->rcAutoaim, 255, 255, 255 );
		else SetCrosshair( m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255 );
	}
	else
	{	// zoomed crosshairs
		if( fOnTarget && m_pWeapon->hZoomedAutoaim )
			SetCrosshair( m_pWeapon->hZoomedAutoaim, m_pWeapon->rcZoomedAutoaim, 255, 255, 255 );
		else SetCrosshair( m_pWeapon->hZoomedCrosshair, m_pWeapon->rcZoomedCrosshair, 255, 255, 255 );

	}

	m_fFade = 200.0f; //!!!
	m_iFlags |= HUD_ACTIVE;

	END_READ();
	
	return 1;
}

//
// WeaponList -- Tells the hud about a new weapon type.
//
int CHudAmmo::MsgFunc_WeaponList( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	
	WEAPON Weapon;

	Q_strcpy( Weapon.szName, READ_STRING() );
	Weapon.iAmmoType = (int)READ_CHAR();	
	
	Weapon.iMax1 = READ_BYTE();
	if( Weapon.iMax1 == 255 )
		Weapon.iMax1 = -1;

	Weapon.iAmmo2Type = READ_CHAR();
	Weapon.iMax2 = READ_BYTE();
	if( Weapon.iMax2 == 255 )
		Weapon.iMax2 = -1;

	Weapon.iSlot = READ_CHAR();
	Weapon.iSlotPos = READ_CHAR();
	Weapon.iId = READ_CHAR();
	Weapon.iFlags = READ_BYTE();
	Weapon.iClip = 0;
	// RTN: peso (ordena a barra de selecao nova - ver DrawWeaponSelectBar) e
	// o icone dela (carregado sob demanda, na primeira vez que a arma aparece
	// na barra - nao aqui, pra nao pagar SPR_Load em toda troca de mapa/nivel
	// pra arma que o jogador pode nunca pegar).
	Weapon.iWeight = READ_BYTE();
	Weapon.hBoxSpr = 0;
	Weapon.hBoxTex = TextureHandle::Null();
	Weapon.bBoxIconLoaded = false;

	gWR.AddWeapon( &Weapon );

	END_READ();

	return 1;

}

//------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------
// RTN F8: NVG on/off (bind: bind n "nvg") - envia comando ao server que aplica o postfx
void CHudAmmo::UserCmd_NVG_Toggle( void )
{
	gEngfuncs.pfnServerCmd( "nvg_toggle" );
}

// Slot button pressed
// RTN: aposentado - a barra de selecao nova nao tem categoria/slot, so a
// rodinha do mouse move o destaque (ver UserCmd_NextWeapon/PrevWeapon). As
// teclas slot1..slot10 continuam existindo (bind do jogador pode estar
// configurado nelas) mas nao fazem mais nada aqui.
void CHudAmmo::SlotInput( int iSlot )
{
	(void)iSlot;
}

void CHudAmmo::UserCmd_Slot1( void )
{
	SlotInput( 0 );
}

void CHudAmmo::UserCmd_Slot2( void )
{
	SlotInput( 1 );
}

void CHudAmmo::UserCmd_Slot3( void )
{
	SlotInput( 2 );
}

void CHudAmmo::UserCmd_Slot4( void )
{
	SlotInput( 3 );
}

void CHudAmmo::UserCmd_Slot5( void )
{
	SlotInput( 4 );
}

void CHudAmmo::UserCmd_Slot6( void )
{
	SlotInput( 5 );
}

void CHudAmmo::UserCmd_Slot7( void )
{
	SlotInput( 6 );
}

void CHudAmmo::UserCmd_Slot8( void )
{
	SlotInput( 7 );
}

void CHudAmmo::UserCmd_Slot9( void )
{
	SlotInput( 8 );
}

void CHudAmmo::UserCmd_Slot10( void )
{
	SlotInput( 9 );
}

void CHudAmmo::UserCmd_Close( void )
{
	if( gpActiveSel )
	{
		gpActiveSel = NULL;
		PlaySound( "common/wpn_hudoff.wav", 1 );
	}
	else
		ClientCmd( "escape" ); // go into menu
}

// RTN: barra de selecao nova - troca IMEDIATA a cada giro de rodinha, sem
// clique de confirmacao (ver o comentario grande em Think()). gpActiveSel
// so serve pra saber qual item destacar/desenhar (DrawWeaponSelectBar) - no
// instante em que muda, ja manda o comando pro server, igual apertar attack
// fazia no menu antigo.
void CHudAmmo::UserCmd_NextWeapon( void )
{
	if( gHUD.m_fPlayerDead || ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL )))
		return;

	WEAPON *list[WEAPON_SELECT_BAR_MAX];
	int count = BuildOwnedWeaponList( list, WEAPON_SELECT_BAR_MAX );
	if( count == 0 )
		return;

	WEAPON *pCurrent = gpActiveSel ? gpActiveSel : m_pWeapon;
	int idx = 0;

	if( pCurrent )
	{
		for( int i = 0; i < count; i++ )
		{
			if( list[i] == pCurrent )
			{
				idx = ( i + 1 ) % count;
				break;
			}
		}
	}

	gpActiveSel = list[idx];
	m_flSelectMenuTime = gEngfuncs.GetClientTime();
	ServerCmd( gpActiveSel->szName );
	g_weaponselect = gpActiveSel->iId;
	PlaySound( "common/wpn_moveselect.wav", 1 );
}

void CHudAmmo::UserCmd_PrevWeapon( void )
{
	if( gHUD.m_fPlayerDead || ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL )))
		return;

	WEAPON *list[WEAPON_SELECT_BAR_MAX];
	int count = BuildOwnedWeaponList( list, WEAPON_SELECT_BAR_MAX );
	if( count == 0 )
		return;

	WEAPON *pCurrent = gpActiveSel ? gpActiveSel : m_pWeapon;
	int idx = 0;

	if( pCurrent )
	{
		for( int i = 0; i < count; i++ )
		{
			if( list[i] == pCurrent )
			{
				idx = ( i - 1 + count ) % count;
				break;
			}
		}
	}

	gpActiveSel = list[idx];
	m_flSelectMenuTime = gEngfuncs.GetClientTime();
	ServerCmd( gpActiveSel->szName );
	g_weaponselect = gpActiveSel->iId;
	PlaySound( "common/wpn_moveselect.wav", 1 );
}

//-------------------------------------------------------------------------
// Drawing code
//-------------------------------------------------------------------------
int CHudAmmo::Draw( float flTime )
{
	int a, x, y, r, g, b;
	int AmmoWidth;

	// RTN: sem checagem de traje - ver o comentario em SlotInput() acima.
	if(( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL )))
		return 1;

	// Draw Weapon Menu
	//
	// DrawWeaponSelectBar() e a barra de selecao que aparece ao girar a
	// rodinha do mouse - nao e a mesma coisa que o contador de municao
	// classico la embaixo. O gate de rtn_hud_style fica DEPOIS desta chamada
	// (ver abaixo), pra ativar o HUD novo (CHudWeaponBox, canto inferior
	// direito) nao apagar de brinde a barra de selecao - o CHudWeaponBox
	// nunca substituiu essa funcao, so o contador de municao.
	//
	// Superou o DrawWList()/menu de baldes-e-posicao antigo (que ainda existe
	// no arquivo, so nunca mais e chamado) - motivo no historico de commits:
	// PrimeXT tem so 5 slots x 5 posicoes (P2, de onde os scripts de arma sao
	// portados, tem 10x10), e slots como o de armas pesadas ja vinham lotados
	// so com as armas classicas do jogo. Toda arma nova de script que caia
	// ali tinha chance real de colidir com outra e sumir do HUD em silencio.
	// A barra nova nao tem bucket/posicao nenhum - so lista, em ordem de
	// peso, as armas que o jogador tem agora - entao essa classe de bug
	// deixa de poder acontecer.
	DrawWeaponSelectBar( flTime );

	// Draw ammo pickup history
	gHR.DrawAmmoHistory( flTime );

	// RTN F10: com o HUD novo (rtn_hud_style 1), o CONTADOR classico de
	// municao (o resto desta funcao, dai pra baixo) some - o CHudWeaponBox
	// mostra no canto inferior direito (estilo Paranoia 2) em seu lugar.
	// O menu de selecao acima de nenhuma forma depende deste gate.
	extern cvar_t *rtn_hud_style;
	if( rtn_hud_style && rtn_hud_style->value >= 1.0f )
		return 1;

	if( !( m_iFlags & HUD_ACTIVE ))
		return 0;

	if( !m_pWeapon )
	{
		return 0;
	}

	WEAPON *pw = m_pWeapon; // shorthand

	// SPR_Draw Ammo
	if(( pw->iAmmoType < 0 ) && ( pw->iAmmo2Type < 0 ))
		return 0;


	int iFlags = DHN_DRAWZERO; // draw 0 values

	AmmoWidth = gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).right - gHUD.GetSpriteRect( gHUD.m_HUD_number_0 ).left;

	a = (int)Q_max( MIN_ALPHA, m_fFade );

	if( m_fFade > 0 )
		m_fFade -= (gHUD.m_flTimeDelta * 20);

	r = gHUD.m_color.r;
	g = gHUD.m_color.g;
	b = gHUD.m_color.b;

	ScaleColors( r, g, b, a );

	// Does this weapon have a clip?
	y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2;

	// Does weapon have any ammo at all?
	if( m_pWeapon->iAmmoType > 0 )
	{
		int iIconWidth = m_pWeapon->rcAmmo.right - m_pWeapon->rcAmmo.left;
		
		if( pw->iClip >= 0 )
		{
			// room for the number and the '|' and the current ammo
			x = ScreenWidth - ( 8 * AmmoWidth ) - iIconWidth;
			x = gHUD.DrawHudNumber( x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b );

			wrect_t rc;
			rc.top = 0;
			rc.left = 0;
			rc.right = AmmoWidth;
			rc.bottom = 100;

			int iBarWidth =  AmmoWidth / 10;

			x += AmmoWidth / 2;

			r = gHUD.m_color.r;
			g = gHUD.m_color.g;
			b = gHUD.m_color.b;

			// draw the | bar
			FillRGBA( x, y, iBarWidth, gHUD.m_iFontHeight, r, g, b, a );

			x += iBarWidth + AmmoWidth / 2;

			// GL Seems to need this
			ScaleColors( r, g, b, a );
			x = gHUD.DrawHudNumber( x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo( pw->iAmmoType ), r, g, b );		


		}
		else
		{
			// SPR_Draw a bullets only line
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo( pw->iAmmoType ), r, g, b );
		}

		// Draw the ammo Icon
		int iOffset = ( m_pWeapon->rcAmmo.bottom - m_pWeapon->rcAmmo.top ) / 8;
		SPR_Set( m_pWeapon->hAmmo, r, g, b );
		SPR_DrawAdditive( 0, x, y - iOffset, &m_pWeapon->rcAmmo );
	}

	// Does weapon have seconday ammo?
	if( pw->iAmmo2Type > 0 ) 
	{
		int iIconWidth = m_pWeapon->rcAmmo2.right - m_pWeapon->rcAmmo2.left;

		// Do we have secondary ammo?
		if(( pw->iAmmo2Type != 0 ) && ( gWR.CountAmmo(pw->iAmmo2Type ) > 0))
		{
			y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight / 4;
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber( x, y, iFlags|DHN_3DIGITS, gWR.CountAmmo( pw->iAmmo2Type ), r, g, b );

			// Draw the ammo Icon
			SPR_Set( m_pWeapon->hAmmo2, r, g, b );
			int iOffset = ( m_pWeapon->rcAmmo2.bottom - m_pWeapon->rcAmmo2.top ) / 8;
			SPR_DrawAdditive( 0, x, y - iOffset, &m_pWeapon->rcAmmo2 );
		}
	}
	return 1;
}

#include <mathlib.h>

//
// Draws the ammo bar on the hud
//
int DrawBar( int x, int y, int width, int height, float f )
{
	int r, g, b;

	f = bound( 0.0f, f, 1.0f );

	if( f )
	{
		int w = f * width;

		// Always show at least one pixel if we have ammo.
		if( w <= 0 ) w = 1;

		UnpackRGB( r, g, b, RGB_GREENISH );
		FillRGBA( x, y, w, height, r, g, b, 255 );
		x += w;
		width -= w;
	}

	r = gHUD.m_color.r;
	g = gHUD.m_color.g;
	b = gHUD.m_color.b;

	FillRGBA( x, y, width, height, r, g, b, 128 );

	return (x + width);
}

void DrawAmmoBar( WEAPON *p, int x, int y, int width, int height )
{
	if( !p )
		return;
	
	if( p->iAmmoType != -1 )
	{
		if( !gWR.CountAmmo( p->iAmmoType ))
			return;

		float f = (float)gWR.CountAmmo(p->iAmmoType) / (float)p->iMax1;
		
		x = DrawBar( x, y, width, height, f );

		// Do we have secondary ammo too?
		if( p->iAmmo2Type != -1 )
		{
			f = (float)gWR.CountAmmo(p->iAmmo2Type) / (float)p->iMax2;

			x += 5; //!!!
			DrawBar( x, y, width, height, f );
		}
	}
}

//
// RTN: barra de selecao horizontal, sem bucket/posicao - ver o comentario
// grande em Draw() e em BuildOwnedWeaponList(). Aparece logo abaixo da
// mira (nao no centro exato da tela, pra nao tampar a visada), controlada
// so pela rodinha (UserCmd_NextWeapon/PrevWeapon ja trocam a arma na hora -
// isto aqui so DESENHA o estado atual) e some sozinha depois de parada
// (Think() zera gpActiveSel apos WEAPON_SELECT_BAR_TIMEOUT).
//
int CHudAmmo::DrawWeaponSelectBar( float flTime )
{
	if( !gpActiveSel )
		return 0;

	WEAPON *list[WEAPON_SELECT_BAR_MAX];
	int count = BuildOwnedWeaponList( list, WEAPON_SELECT_BAR_MAX );
	if( count == 0 )
		return 0;

	int selIdx = -1;
	for( int i = 0; i < count; i++ )
	{
		if( list[i] == gpActiveSel )
		{
			selIdx = i;
			break;
		}
	}
	if( selIdx < 0 )
		return 0;	// a arma destacada sumiu da lista (largada?) entre um frame e outro

	// sumir suavemente depois de um tempo parado, em vez de piscar do nada
	float elapsed = gEngfuncs.GetClientTime() - m_flSelectMenuTime;
	const float SELECT_BAR_FADE_TIME = 0.4f;
	float fade = 1.0f;
	if( elapsed > ( WEAPON_SELECT_BAR_TIMEOUT - SELECT_BAR_FADE_TIME ))
		fade = 1.0f - ( elapsed - ( WEAPON_SELECT_BAR_TIMEOUT - SELECT_BAR_FADE_TIME )) / SELECT_BAR_FADE_TIME;
	fade = bound( 0.0f, fade, 1.0f );
	if( fade <= 0.0f )
		return 0;

	// janela visivel: WEAPON_SELECT_BAR_VISIBLE itens, centrada no destaque
	int half = WEAPON_SELECT_BAR_VISIBLE / 2;
	int start = selIdx - half;
	int end = selIdx + half;
	if( start < 0 )
	{
		end += -start;
		start = 0;
	}
	if( end > count - 1 )
	{
		start -= ( end - ( count - 1 ));
		end = count - 1;
	}
	if( start < 0 )
		start = 0;	// lista menor que a janela inteira

	bool moreLeft = start > 0;
	bool moreRight = end < count - 1;

	// tamanhos: item destacado maior que os outros. Altura fixa por estado,
	// LARGURA calculada da proporcao real do sprite (icone bem mais largo
	// que alto, tipo o do MP5 - esticar pra uma caixa quadrada fixa
	// achatava/distorcia o desenho). Sem icone, usa a largura "nominal" so
	// pra sobrar espaco decente pro texto do nome.
	const int NORMAL_H = YRES( 26 );
	const int SEL_H = YRES( 38 );
	const int NOMINAL_W = XRES( 56 );	// fallback (sem icone) e ponto de partida antes de saber a proporcao
	const int GAP = XRES( 8 );

	int visCount = end - start + 1;
	int itemW[WEAPON_SELECT_BAR_VISIBLE];
	int itemH[WEAPON_SELECT_BAR_VISIBLE];
	int totalW = 0;
	bool anyIcon = false;	// nenhum item com icone => nao encosta na TriAPI

	for( int i = start; i <= end; i++ )
	{
		WEAPON *p = list[i];
		bool isSelected = ( i == selIdx );
		int h = isSelected ? SEL_H : NORMAL_H;
		int w = ( isSelected ? NOMINAL_W * 3 / 2 : NOMINAL_W );

		RTN_EnsureBoxIcon( p );
		if( RTN_HasBoxIcon( p ))
			anyIcon = true;

		float aspect;
		if( RTN_GetBoxIconAspect( p, &aspect ))
			w = (int)( h * aspect + 0.5f );

		int k = i - start;
		itemW[k] = w;
		itemH[k] = h;
		totalW += w;
	}
	totalW += GAP * ( visCount - 1 );

	int centerX = ScreenWidth / 2;
	int barY = ScreenHeight / 2 + YRES( 46 );	// logo abaixo da mira, sem tampar a visada
	int startX = centerX - totalW / 2;

	// RTN: o desenho vai em DOIS passos, e isso e proposital.
	//
	// Antes era um loop so, intercalando por item: quad de textura crua
	// (TriAPI) -> FillRGBA/DrawHudString (2D do engine) -> quad de novo, e
	// ainda ligando/desligando CullFace e RenderMode A CADA icone. Com uma
	// arma so isso passava batido; com duas ou mais o churn de estado se
	// repetia e vinha o GL_INVALID_ENUM - que e exatamente a assinatura
	// reportada em teste ("so aparece com mais de uma arma").
	//
	// Os dois lugares do projeto que ja desenhavam VARIAS texturas cruas por
	// frame sem erro (hud_radio.cpp e hud_textwindow.cpp) fazem o oposto:
	// setam o estado UMA vez em volta do lote todo, nao encostam em CullFace
	// e nao intercalam com o 2D do engine. E esse padrao que se segue aqui.

	// ---- passo 1: so os icones (TriAPI), estado setado uma vez ----
	// Se nenhum item tem icone (ou rtn_hud_selectbar_icons esta em 0), nao se
	// encosta na TriAPI de jeito nenhum - e o que torna o cvar um teste de
	// isolamento honesto.
	if( anyIcon )
	{
		bool dbgBatch = ( rtn_hud_selectbar_gldebug && rtn_hud_selectbar_gldebug->value >= 1.0f );

		if( dbgBatch ) GL_CheckForErrors();	// (dreno) erro de antes da barra
		GL_Blend( GL_TRUE );	// sem isso o alpha nao e aplicado (mesma nota do hud_radio)
		if( dbgBatch ) GL_CheckForErrors();	// culpa: GL_Blend(TRUE)

		int ix = startX;
		for( int i = start; i <= end; i++ )
		{
			WEAPON *p = list[i];
			bool isSelected = ( i == selIdx );
			int k = i - start;
			int w = itemW[k];
			int h = itemH[k];
			int y = barY + ( SEL_H - h );	// alinha pela base (itens menores "sentam" na mesma linha)

			int alpha = (int)(( isSelected ? 255 : 150 ) * fade );
			int r = 255, g = 255, b = 255;
			if( !gWR.HasAmmo( p ))
				UnpackRGB( r, g, b, RGB_REDISH );	// sem municao - mesmo aviso visual do menu antigo

			RTN_DrawBoxIcon( p, ix, y, w, h, r / 255.0f, g / 255.0f, b / 255.0f, alpha / 255.0f );
			ix += w + GAP;
		}

		gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
		if( dbgBatch ) GL_CheckForErrors();	// culpa: RenderMode(kRenderNormal) final

		GL_Blend( GL_FALSE );	// nao vazar blend ligado pro resto do frame
		if( dbgBatch ) GL_CheckForErrors();	// culpa: GL_Blend(FALSE)
	}

	// ---- passo 2: retangulos e texto (2D do engine), ja fora da TriAPI ----
	int x = startX;
	for( int i = start; i <= end; i++ )
	{
		WEAPON *p = list[i];
		bool isSelected = ( i == selIdx );
		int k = i - start;
		int w = itemW[k];
		int h = itemH[k];
		int y = barY + ( SEL_H - h );

		int alpha = (int)(( isSelected ? 255 : 150 ) * fade );
		int r = 255, g = 255, b = 255;
		if( !gWR.HasAmmo( p ))
			UnpackRGB( r, g, b, RGB_REDISH );

		if( !RTN_HasBoxIcon( p ))
		{
			// sem icone (nem .spr nem .tga) - so um retangulo discreto com
			// o nome, pra barra continuar legivel mesmo pra arma que ainda
			// nao ganhou icone proprio.
			FillRGBA( x, y, w, h, 40, 40, 40, alpha / 2 );

			char szShort[24];
			const char *pName = p->szName;
			const char *pUnd = Q_strstr( pName, "_" );
			if( pUnd && pUnd[1] )
				pName = pUnd + 1;
			Q_strncpy( szShort, pName, sizeof( szShort ) - 1 );
			szShort[sizeof( szShort ) - 1] = '\0';
			for( char *c = szShort; *c; c++ )
				*c = toupper( (unsigned char)*c );

			gHUD.DrawHudString( x + 2, y + h / 2 - 4, x + w - 2, szShort, r, g, b );
		}

		if( isSelected )
		{
			char szAmmo[16];
			if( p->iAmmoType >= 0 )
				Q_snprintf( szAmmo, sizeof( szAmmo ), "%d / %d", p->iClip, gWR.CountAmmo( p->iAmmoType ));
			else
				szAmmo[0] = '\0';

			if( szAmmo[0] )
			{
				int tw = 0;
				for( const char *c = szAmmo; *c; c++ )
					tw += gHUD.m_scrinfo.charWidths[(unsigned char)*c];

				int tx = x + ( w - tw ) / 2;
				int ty = y + h + YRES( 2 );
				gHUD.DrawHudString( tx + 1, ty + 1, tx + tw + 1, szAmmo, 0, 0, 0 );
				gHUD.DrawHudString( tx, ty, tx + tw, szAmmo, 255, 255, 255 );
			}
		}

		x += w + GAP;
	}

	// indicadores de "tem mais pra ca/pra la" quando a lista nao cabe inteira
	int arrowAlpha = (int)( 200 * fade );
	if( moreLeft )
	{
		int lx = centerX - totalW / 2 - XRES( 16 );
		gHUD.DrawHudString( lx, barY + SEL_H / 2 - 6, lx + XRES( 14 ), "<", arrowAlpha, arrowAlpha, arrowAlpha );
	}
	if( moreRight )
	{
		int rx = centerX + totalW / 2 + XRES( 4 );
		gHUD.DrawHudString( rx, barY + SEL_H / 2 - 6, rx + XRES( 14 ), ">", arrowAlpha, arrowAlpha, arrowAlpha );
	}

	return 1;
}

//
// Draw Weapon Menu
//
int CHudAmmo::DrawWList( float flTime )
{
	int r, g, b, a;
	int x, y, i;

	if( !gpActiveSel )
		return 0;

	int iActiveSlot;

	if( gpActiveSel == (WEAPON *)1 )
		iActiveSlot = -1;	// current slot has no weapons
	else 
		iActiveSlot = gpActiveSel->iSlot;

	x = 10; //!!!
	y = 10; //!!!

	// Ensure that there are available choices in the active slot
	if( iActiveSlot > 0 )
	{
		if( !gWR.GetFirstPos( iActiveSlot ))
		{
			gpActiveSel = (WEAPON *)1;
			iActiveSlot = -1;
		}
	}
		
	// Draw top line
	for( i = 0; i < MAX_WEAPON_SLOTS; i++ )
	{
		int iWidth;

		r = gHUD.m_color.r;
		g = gHUD.m_color.g;
		b = gHUD.m_color.b;
	
		if( iActiveSlot == i )
			a = 255;
		else
			a = 192;

		ScaleColors( r, g, b, 255 );
		SPR_Set( gHUD.GetSprite( m_HUD_bucket0 + i ), r, g, b );

		// make active slot wide enough to accomodate gun pictures
		if( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos(iActiveSlot);

			if( p )
				iWidth = p->rcActive.right - p->rcActive.left;
			else
				iWidth = giBucketWidth;
		}
		else
			iWidth = giBucketWidth;

		SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( m_HUD_bucket0 + i ));
		
		x += iWidth + 5;
	}

	a = 128; //!!!
	x = 10;

	// Draw all of the buckets
	for( i = 0; i < MAX_WEAPON_SLOTS; i++ )
	{
		y = giBucketHeight + 10;

		// If this is the active slot, draw the bigger pictures,
		// otherwise just draw boxes
		if( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos( i );
			int iWidth = giBucketWidth;

			if( p )
				iWidth = p->rcActive.right - p->rcActive.left;

			for( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				p = gWR.GetWeaponSlot( i, iPos );

				if( !p || !p->iId )
					continue;

				r = gHUD.m_color.r;
				g = gHUD.m_color.g;
				b = gHUD.m_color.b;
			
				// if active, then we must have ammo.
				if( gpActiveSel == p )
				{
					SPR_Set( p->hActive, r, g, b );
					SPR_DrawAdditive( 0, x, y, &p->rcActive );

					SPR_Set( gHUD.GetSprite( m_HUD_selection ), r, g, b );
					SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect( m_HUD_selection ));
				}
				else
				{
					// Draw Weapon if Red if no ammo
					if( gWR.HasAmmo( p ))
					{
						ScaleColors( r, g, b, 192 );
					}
					else
					{
						UnpackRGB( r, g, b, RGB_REDISH );
						ScaleColors( r, g, b, 128 );
					}

					SPR_Set( p->hInactive, r, g, b );
					SPR_DrawAdditive( 0, x, y, &p->rcInactive );
				}

				// Draw Ammo Bar
				DrawAmmoBar( p, x + giABWidth / 2, y, giABWidth, giABHeight );
				y += p->rcActive.bottom - p->rcActive.top + 5;
			}

			x += iWidth + 5;

		}
		else
		{
			// Draw Row of weapons.
			r = gHUD.m_color.r;
			g = gHUD.m_color.g;
			b = gHUD.m_color.b;

			for( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				WEAPON *p = gWR.GetWeaponSlot( i, iPos );
				
				if( !p || !p->iId )
					continue;

				if( gWR.HasAmmo( p ))
				{
					r = gHUD.m_color.r;
					g = gHUD.m_color.g;
					b = gHUD.m_color.b;
					a = 128;
				}
				else
				{
					UnpackRGB( r, g, b, RGB_REDISH );
					a = 96;
				}

				FillRGBA( x, y, giBucketWidth, giBucketHeight, r, g, b, a );
				y += giBucketHeight + 5;
			}
			x += giBucketWidth + 5;
		}
	}	

	return 1;

}
