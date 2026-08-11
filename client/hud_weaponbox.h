#pragma once

// RTN F10: HUD de armas (canto inferior DIREITO) estilo Paranoia 2.
// NAO incluir hud.h aqui (include circular!) - incluido PELO hud.h.
//
// Layout VERTICAL: silhueta da arma em cima (branca), nome e municao
// "00 / 000" embaixo (Roboto BOLD - creditsfont_cp1251.fnt agora e o Bold).
//
// ATRIBUICAO DINAMICA DO SPRITE (data-driven, sem recompilar):
//   o sprite da arma e carregado por convencao de nome:
//   sprites/rtn_hud_ammo_<classname>.spr  (ex: weapon_mp5 ->
//   rtn_hud_ammo_weapon_mp5.spr, convertido de gfx/vgui/ammo/640_weapon_mp5.tga
//   pelo tools/convert_vgui.py). Para adicionar uma arma nova, basta dropar
//   o .tga na pasta e rodar o conversor - o HUD carrega sozinho.
//
// Fontes: gHUD.m_Ammo.m_pWeapon (WEAPON* ativo: szName, iClip, iAmmoType)
//         + gWR.CountAmmo(iAmmoType) p/ a reserva (igual ao Paranoia 2).
class CHudWeaponBox : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );

private:
	SpriteHandle m_hWeaponSpr = 0;   // silhueta da arma atual
	int m_iLastWeaponId = -1;        // dirty-check p/ recarregar o sprite
	int m_iClip = 0;                 // municao no pente
	int m_iAmmo = 0;                 // municao reserva
	char m_szName[ 32 ];             // nome da arma (limpo, ex: "MP5")
};
