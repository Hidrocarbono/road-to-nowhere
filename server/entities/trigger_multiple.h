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

#pragma once
#include "triggers.h"

/*QUAKED trigger_multiple (.5 .5 .5) ? notouch
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "health" is set, the trigger must be killed to activate each time.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
If notouch is set, the trigger is only fired by other entities, not by touching.
NOTOUCH has been obsoleted by trigger_relay!
sounds
1)      secret
2)      beep beep
3)      large switch
4)
NEW
if a trigger has a NETNAME, that NETNAME will become the TARGET of the triggered object.
*/
class CTriggerMultiple : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerMultiple, CBaseTrigger );
public:
	void Spawn( void );
	// RTN: GetState() antigo daqui reaproveitava pev->nextthink (so existe
	// pra gatear o cooldown de novo toque em ActivateMultiTrigger) como se
	// fosse "ja disparou". Bug real: um trigger NUNCA tocado tem nextthink
	// == 0, e (0 > gpGlobals->time) e false -> reportava ON desde o spawn,
	// antes do jogador sequer encostar. Removido - agora usa o
	// CBaseTrigger::GetState() novo (m_fTriggered, latch de verdade).
};
