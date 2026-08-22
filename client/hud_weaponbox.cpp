#include "hud.h"          // primeiro: define CHudBase e inclui hud_weaponbox.h (pos CHudBase)
#include "hud_weaponbox.h"
#include "utils.h"
#include "ammo.h"         // struct WEAPON
#include "ammohistory.h"  // WeaponsResource gWR (reserva de municao)
#include "enginecallback.h"
#include "filesystem_utils.h"  // fs::FileExists - sprite de arma e opcional
#include "gl_local.h"          // TextureHandle / GL_Bind (icone .tga do VGUI)
#include <ctype.h>        // toupper()

// definido em client/hud_textwindow.cpp - desenha uma textura crua em 2D
extern void OrthoQuad( int x1, int y1, int x2, int y2 );

// RTN F10: HUD de armas (canto inferior direito) estilo Paranoia 2.
// Silhueta branca da arma + nome + municao "clip / reserva" em Roboto Bold.
// Sprite da arma: data-driven por convencao de nome (ver hud_weaponbox.h).
//
// NOMES DE EXIBICAO: mapa classname -> nome mostrado no HUD (o classname do
// jogo pode nao bater com o nome real da arma, ex: weapon_mp5 exibe "FN FAL").
// Para adicionar, basta inserir uma linha na tabela (sem recompilar nada alem).
static const struct
{
	const char *szClass;
	const char *szDisplay;
} s_RTNWeaponDisplayNames[] =
{
	{ "weapon_mp5", "FN FAL" },   // a "mp5" do RTN e na verdade uma FN FAL
	// futuras: { "weapon_shotgun", "SPAS-12" },
	//          { "weapon_python",  "M1911"   },
};

// devolve o nome de exibicao da arma (ou NULL se nao ha mapeamento)
static const char *RTN_GetDisplayName( const char *szClassname )
{
	for( int i = 0; i < (int)ARRAYSIZE( s_RTNWeaponDisplayNames ); i++ )
	{
		if( !Q_strcmp( szClassname, s_RTNWeaponDisplayNames[i].szClass ) )
			return s_RTNWeaponDisplayNames[i].szDisplay;
	}
	return NULL;
}

int CHudWeaponBox::Init( void )
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudWeaponBox::VidInit( void )
{
	// m_iLastWeaponId = -1 abaixo forca o recarregamento no proximo Draw, entao
	// o handle antigo (invalido apos troca de modo de video) so precisa ser
	// esquecido, nao liberado.
	m_hWeaponTex = TextureHandle::Null();
	m_hWeaponSpr = 0;
	m_iLastWeaponId = -1;
	m_iClip = 0;
	m_iAmmo = 0;
	m_szName[ 0 ] = '\0';
	return 1;
}

void CHudWeaponBox::Reset( void )
{
	if( m_hWeaponTex.Initialized( ))
	{
		FREE_TEXTURE( m_hWeaponTex );
		m_hWeaponTex = TextureHandle::Null();
	}
	m_hWeaponSpr = 0;
	m_iLastWeaponId = -1;
	m_iClip = 0;
	m_iAmmo = 0;
	m_szName[ 0 ] = '\0';
}

int CHudWeaponBox::Draw( float flTime )
{
	extern cvar_t *rtn_hud_style;
	if( !rtn_hud_style || rtn_hud_style->value < 1.0f )
		return 0;  // HUD classico ativo

	WEAPON *pw = gHUD.m_Ammo.GetWeapon();
	if( !pw || !pw->szName[ 0 ] )
		return 0;

	if( gHUD.m_fPlayerDead )
		return 0;

	// ---- dados ----
	if( pw->iId != m_iLastWeaponId )
	{
		// arma mudou: recarrega o sprite (data-driven por classname)
		m_iLastWeaponId = pw->iId;
		char szSpr[ 64 ];
		Q_snprintf( szSpr, sizeof( szSpr ), "sprites/rtn_hud_ammo_%s.spr", pw->szName );
		// Checar antes de carregar: SPR_Load imprime "Could not load HUD sprite"
		// em VERMELHO para todo sprite ausente, e agora que qualquer arma da
		// pasta de scripts pode ser equipada, cada uma sem esse sprite proprio
		// enchia o console de erro. O icone e opcional - o resto da caixa (nome,
		// municao) desenha sem ele.
		m_hWeaponSpr = fs::FileExists( szSpr ) ? LoadSprite( szSpr ) : 0;

		// Sem .spr, tenta o .tga do VGUI direto. E o arquivo que o script da
		// arma ja referencia ("hudsprite" name "ammo"), e ate agora ele so
		// funcionava depois de uma conversao manual para .spr - passo que
		// ninguem lembrava de fazer, deixando a arma sem icone com o arquivo
		// certo na pasta certa.
		if( m_hWeaponTex.Initialized( ))
		{
			FREE_TEXTURE( m_hWeaponTex );
			m_hWeaponTex = TextureHandle::Null();
		}

		if( !m_hWeaponSpr )
		{
			char szTga[ 96 ];
			Q_snprintf( szTga, sizeof( szTga ), "gfx/vgui/ammo/640_%s.tga", pw->szName );
			if( fs::FileExists( szTga ))
				m_hWeaponTex = LOAD_TEXTURE( szTga, NULL, 0, TF_CLAMP | TF_IMAGE | TF_HAS_ALPHA );
		}

		// nome: mapa de exibicao (weapon_mp5 -> "FN FAL") ou fallback
		// classname limpo ("weapon_shotgun" -> "SHOTGUN")
		const char *pDisplay = RTN_GetDisplayName( pw->szName );
		if( pDisplay )
		{
			Q_strncpy( m_szName, pDisplay, sizeof( m_szName ) - 1 );
		}
		else
		{
			const char *pName = pw->szName;
			const char *pUnd = Q_strstr( pName, "_" );
			if( pUnd && pUnd[ 1 ] )
				pName = pUnd + 1;
			Q_strncpy( m_szName, pName, sizeof( m_szName ) - 1 );
			for( char *c = m_szName; *c; c++ )
				*c = toupper( (unsigned char)*c );
		}
		m_szName[ sizeof( m_szName ) - 1 ] = '\0';
	}

	m_iClip = pw->iClip;
	m_iAmmo = gWR.CountAmmo( pw->iAmmoType );
	if( m_iAmmo < 0 ) m_iAmmo = 0;

	// ---- layout vertical (canto inferior direito) ----
	int w = SPR_Width( m_hWeaponSpr, 0 );
	int h = SPR_Height( m_hWeaponSpr, 0 );
	// O .tga nao passa pelo SPR_Width; usa o mesmo tamanho nominal do .spr do
	// mp5 (96x30 em 640), que e como a arte do VGUI foi desenhada.
	if( m_hWeaponTex.Initialized( ))
	{
		w = XRES( 96 );
		h = YRES( 30 );
	}
	if( w <= 0 ) w = XRES( 96 );
	if( h <= 0 ) h = YRES( 30 );

	// silhueta da arma (branca) - o SPRITE desce junto com a linha (abaixo
	// dela, no canto inferior direito). A linha (nome + municao) fica NA
	// MESMA ALTURA da barra de stamina (ScreenHeight - YRES(12)).
	int wbX = ScreenWidth - w - XRES( 10 );
	int ty  = ScreenHeight - YRES( 12 );          // altura da barra de stamina
	int wbY = ty - h - YRES( 2 );                 // sprite acima da linha (espacamento mantido)

	if( m_hWeaponSpr )
	{
		SPR_Set( m_hWeaponSpr, 255, 255, 255 );
		SPR_Draw( 0, wbX, wbY, NULL );
	}
	else if( m_hWeaponTex.Initialized( ))
	{
		// mesmo caminho que hud_textwindow/hud_radio usam para desenhar
		// textura crua em 2D
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
		GL_Bind( 0, m_hWeaponTex );
		OrthoQuad( wbX, wbY, wbX + w, wbY + h );
	}

	// nome da arma (pequeno, cinza claro) + municao "00/000" NA MESMA LINHA
	// (logo apos o nome - o formato que o user pediu: NOME DA ARMA  00/000)
	char szAmmo[ 16 ];
	Q_snprintf( szAmmo, sizeof( szAmmo ), "%02d/%03d", m_iClip, m_iAmmo );

	int nw = 0;   // largura do nome em pixels (charWidths da fonte atual)
	for( const char *p = m_szName; *p; p++ )
		nw += gHUD.m_scrinfo.charWidths[(unsigned char)*p];

	gHUD.DrawHudString( wbX + 1, ty + 1, wbX + XRES( 140 ) + 1, m_szName, 0, 0, 0 );
	gHUD.DrawHudString( wbX, ty, wbX + XRES( 140 ), m_szName, 220, 220, 220 );

	int ammoX = wbX + nw + XRES( 8 );
	// contorno escuro p/ legibilidade
	gHUD.DrawHudString( ammoX + 1, ty + 1, ammoX + XRES( 80 ) + 1, szAmmo, 0, 0, 0 );
	gHUD.DrawHudString( ammoX, ty, ammoX + XRES( 80 ), szAmmo, 255, 255, 255 );

	return 1;
}
