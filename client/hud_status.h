#pragma once

// RTN F10: HUD de status (canto inferior esquerdo) estilo Paranoia 2.
// NAO incluir hud.h aqui (include circular!) - este header e incluido PELO
// hud.h apos a definicao de CHudBase (igual health.h/ammo.h).
//
// Elementos (layout vertical, da esquerda p/ direita):
//   - Silhueta de SOLDADO = VIDA: 2 sprites (contorno vazio + preenchimento
//     vermelho) com MASCARA VERTICAL (scissor) - consumo de cima p/ baixo.
//   - Barra AZUL + icone ESCUDO = ARMOR (mascara horizontal, consumo da direita).
//   - Barra VERDE + icone RAIO = STAMINA (mascara horizontal).
//   - Flash de dano sutil (alpha decrescente) + micro-shake extremamente leve
//     (dispara g_flRTNShakeTime, lido no V_CalcView do r_view.cpp).
//
// Fontes de dados (tudo no client, zero server novo):
//   - vida:    gHUD.m_Health.m_iHealth   (MsgFunc_Health, 0-100)
//   - armor:   gHUD.m_Battery.GetBat()    (MsgFunc_Battery, 0-100)
//   - stamina: GetLocalPlayer()->curstate.fuser2 (sincronizado pelo engine)
class CHudStatus : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	int MsgFunc_Stamina( const char *pszName, int iSize, void *pbuf );  // RTN F10 fix

	// momento do ultimo dano recebido (flash + shake) - exposto p/ debug
	float m_flDamageTime = -100.0f;

private:
	int m_iPrevHealth = 100;    // dirty-check: detecta o dano (queda de vida)
	float m_flStamina = 100.0f; // RTN F10 fix: via mensagem "Stamina" (o
				    // curstate.fuser2 nao chega ao jogador local)

	// sprites .spr convertidos dos TGA (tools/convert_vgui.py)
	SpriteHandle m_hHealthEmpty = 0;
	SpriteHandle m_hHealthFull = 0;
	SpriteHandle m_hHealthFlash = 0;
	SpriteHandle m_hArmorEmpty = 0;
	SpriteHandle m_hArmorFull = 0;
	SpriteHandle m_hArmorFlash = 0;
	SpriteHandle m_hArmorIcon = 0;
	SpriteHandle m_hStaminaEmpty = 0;
	SpriteHandle m_hStaminaFull = 0;
	SpriteHandle m_hStaminaIcon = 0;
};
