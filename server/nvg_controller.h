#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"

// RTN Fase 8: NVG (visao noturna) - toggle global na tecla N.
// NAO depende de slot/inventario: o comando client "nvg" -> server "nvg_toggle"
// aplica o postfx nativo (gmsgPostFxSettings) no jogador.

class CNVGController
{
public:
	static CNVGController& GetInstance();
	void Toggle( CBaseEntity *pPlayer );
	void Update();  // chamado todo frame (bateria/desligamento)

	bool IsActive() const { return m_bActive; }
	float GetBattery() const { return m_flBattery; }

private:
	CNVGController() = default;
	CNVGController(const CNVGController&) = delete;
	CNVGController &operator=(const CNVGController&) = delete;

	void ApplyState( CBaseEntity *pPlayer, bool active );

	bool m_bActive = false;
	float m_flBattery = 120.0f;  // seconds of use per full charge
};
