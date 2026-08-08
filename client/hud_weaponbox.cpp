#include "hud.h"          // primeiro: define CHudBase e inclui hud_weaponbox.h (pos CHudBase)
#include "hud_weaponbox.h"
#include "utils.h"
#include "ammo.h"         // struct WEAPON
#include "ammohistory.h"  // WeaponsResource gWR (reserva de municao)
#include "enginecallback.h"
#include <ctype.h>        // toupper()

// RTN F10: HUD de armas (canto inferior direito) estilo Paranoia 2.
// Silhueta branca da arma + nome + municao "clip / reserva" em Roboto Bold.
// Sprite da arma: data-driven por convencao de nome (ver hud_weaponbox.h).

int CHudWeaponBox::Init( void )
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudWeaponBox::VidInit( void )
{
	m_hWeaponSpr = 0;
	m_iLastWeaponId = -1;
	m_iClip = 0;
	m_iAmmo = 0;
	m_szName[ 0 ] = '\0';
	return 1;
}

void CHudWeaponBox::Reset( void )
{
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
		m_hWeaponSpr = LoadSprite( szSpr );

		// nome limpo: "weapon_mp5" -> "MP5"
		const char *pName = pw->szName;
		const char *pUnd = Q_strstr( pName, "_" );
		if( pUnd && pUnd[ 1 ] )
			pName = pUnd + 1;
		Q_strncpy( m_szName, pName, sizeof( m_szName ) - 1 );
		m_szName[ sizeof( m_szName ) - 1 ] = '\0';
		for( char *c = m_szName; *c; c++ )
			*c = toupper( (unsigned char)*c );
	}

	m_iClip = pw->iClip;
	m_iAmmo = gWR.CountAmmo( pw->iAmmoType );
	if( m_iAmmo < 0 ) m_iAmmo = 0;

	// ---- layout vertical (canto inferior direito) ----
	int w = SPR_Width( m_hWeaponSpr, 0 );
	int h = SPR_Height( m_hWeaponSpr, 0 );
	if( w <= 0 ) w = XRES( 96 );
	if( h <= 0 ) h = YRES( 30 );

	// silhueta da arma (branca) - topo
	int wbX = ScreenWidth - w - XRES( 10 );
	int wbY = ScreenHeight - YRES( 12 ) - h - YRES( 30 );

	if( m_hWeaponSpr )
	{
		SPR_Set( m_hWeaponSpr, 255, 255, 255 );
		SPR_Draw( 0, wbX, wbY, NULL );
	}

	// nome da arma (pequeno, cinza claro)
	int ty = wbY + h + YRES( 2 );
	gHUD.DrawHudString( wbX, ty, wbX + XRES( 140 ), m_szName, 220, 220, 220 );

	// municao "clip / reserva" em Roboto BOLD (creditsfont agora e Bold)
	char szAmmo[ 16 ];
	Q_snprintf( szAmmo, sizeof( szAmmo ), "%d / %d", m_iClip, m_iAmmo );
	ty += gHUD.m_iFontHeight + YRES( 1 );
	// contorno escuro p/ legibilidade
	gHUD.DrawHudString( wbX + 1, ty + 1, wbX + XRES( 140 ) + 1, szAmmo, 0, 0, 0 );
	gHUD.DrawHudString( wbX, ty, wbX + XRES( 140 ), szAmmo, 255, 255, 255 );

	return 1;
}
