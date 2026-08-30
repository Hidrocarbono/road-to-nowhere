/***
*
*	Road to Nowhere - visao noturna
*
*   Item de oculos de visao noturna. Ao ser pego, habilita o NVG no jogador
*   (CBasePlayer::GiveNVG) e recarrega a bateria. O efeito visual e todo
*   client-side - ver client/render/gl_nvg.cpp.
*
****/
#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "items.h"
#include "user_messages.h"

class CItemNVGoggles : public CItem
{
	void Spawn( void );
	void Precache( void );
	BOOL MyTouch( CBasePlayer *pPlayer );
};
