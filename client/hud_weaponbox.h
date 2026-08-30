#pragma once

// RTN F10: HUD de armas (canto inferior DIREITO) estilo Paranoia 2.
// NAO incluir hud.h aqui (include circular!) - incluido PELO hud.h.
//
// Layout VERTICAL: silhueta da arma em cima (branca), nome e municao
// "00 / 000" embaixo (Roboto BOLD - creditsfont_cp1251.fnt agora e o Bold).
//
// ATRIBUICAO DINAMICA DO SPRITE (data-driven, sem recompilar):
//   o sprite da arma e carregado por convencao de nome:
//   sprites/rtn_hud_ammo_<classname>.spr   (.spr v32 truecolor RGBA)
//
// So .spr. O fallback antigo pro .tga (LOAD_TEXTURE + GL_Bind + OrthoQuad)
// foi removido: esse caminho de textura crua gerava GL_INVALID_ENUM todo
// frame neste engine - medido, nao deduzido (ver CHANGELOG_AGENT.md secao 4).
// Pra gerar o .spr a partir do .tga da arma:
//   python3 tools/tga2spr.py game_dir/gfx/vgui/ammo/640_<arma>.tga \
//                            game_dir/sprites/rtn_hud_ammo_<arma>.spr
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
	int m_iLastWeaponId = -1;        // dirty-check p/ recarregar o sprite
	int m_iClip = 0;                 // municao no pente
	int m_iAmmo = 0;                 // municao reserva
	char m_szName[ 32 ];             // nome da arma (limpo, ex: "MP5")
};
