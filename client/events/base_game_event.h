/*
base_game_event.h - base class for game events
Copyright (C) 2024 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#include "event_args.h"
#include "vector.h"
#include "gl_local.h"   // RTN F10 fix: GetVieworg (o olho da camera p/ o trace dos gibs)
#include <stdint.h>

class CBaseGameEvent
{
public:
	CBaseGameEvent(event_args_s *args);
	~CBaseGameEvent() = default;

protected:
	Vector GetOrigin() const
	{
		// RTN F10 fix: o origin do evento 5001 vem do engine como o pev->origin
		// (CENTRO do jogador - cl_events.c:446 'VectorCopy(state->origin...'
		// quando o QC nao passa o origin). SEM o view_ofs, o trace dos GIBs
		// saia do centro da tela (sem o offset do LEAN) -> impacto na parede
		// errada (onde o jogador esta encostado). Usa o GetVieworg() (o olho
		// real da camera - inclinado pelo lean) p/ o trace do client bater
		// com o FireBullets do server (pev->origin + view_ofs).
		//
		// RTN: crash confirmado em jogo (CRpgFireEvent::Execute, access
		// violation) rastreado ate aqui via .pdb - GetVieworg() desreferencia
		// RI (ref_instance_t* global do renderer) sem checagem. RI so e valido
		// durante um frame de render de verdade; um evento reproduzido fora
		// dessa janela (fila atrasada, replay fora de ordem) lia RI nulo/stale
		// e derrubava o client. m_arguments->origin e sempre seguro (vem direto
		// do pacote de rede, nunca aponta pro renderer) - usa ele como
		// fallback nesse caso em vez de crashar.
		if( IsEventLocal( ) && RI != NULL )
			return GetVieworg();
		return Vector(m_arguments->origin);
	};
	Vector GetAngles() const { return Vector(m_arguments->angles); };
	Vector GetVelocity() const { return Vector(m_arguments->velocity); };
	int32_t GetEntityIndex() const { return m_arguments->entindex; };
	bool IsEventLocal() const;
	bool IsEntityPlayer() const;

	event_args_t *m_arguments;
};
