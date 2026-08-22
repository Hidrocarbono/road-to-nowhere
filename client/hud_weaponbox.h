#pragma once

// RTN F10: HUD de armas (canto inferior DIREITO) estilo Paranoia 2.
// NAO incluir hud.h aqui (include circular!) - incluido PELO hud.h.
//
// Layout VERTICAL: silhueta da arma em cima (branca), nome e municao
// "00 / 000" embaixo (Roboto BOLD - creditsfont_cp1251.fnt agora e o Bold).
//
// ATRIBUICAO DINAMICA DO SPRITE (data-driven, sem recompilar):
//   o sprite da arma e carregado por convencao de nome:
//   1) sprites/rtn_hud_ammo_<classname>.spr   (se existir)
//   2) gfx/vgui/ammo/640_<classname>.tga       (fallback direto, sem conversao)
//
// O caminho (2) existe porque a conversao para .spr era um passo manual que
// ninguem lembrava de fazer: o .tga da arma nova ficava na pasta certa, o
// script apontava para ele, e o icone simplesmente nao aparecia. O engine ja
// sabe carregar .tga (LOAD_TEXTURE) e o HUD ja desenha textura crua em dois
// outros lugares (hud_textwindow, hud_radio), entao nao ha motivo para exigir
// o .spr.
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
	SpriteHandle m_hWeaponSpr = 0;   // silhueta da arma atual (.spr)
	// Fallback direto para o .tga do VGUI, sem passar por conversao para .spr -
	// ver o comentario no Draw(). Vazio quando nao ha .tga para a arma.
	TextureHandle m_hWeaponTex = TextureHandle::Null();
	int m_iLastWeaponId = -1;        // dirty-check p/ recarregar o sprite
	int m_iClip = 0;                 // municao no pente
	int m_iAmmo = 0;                 // municao reserva
	char m_szName[ 32 ];             // nome da arma (limpo, ex: "MP5")
};
