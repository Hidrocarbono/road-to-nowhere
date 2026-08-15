/*
radio_sentence.h - radio message entity (Paranoia 2 style, RTN F10)
Copyright (C) 2026 Hermes e Hidrocarboneto

Triggers a radio sentence: plays the sound (message) and shows the
speaker icon + talker name on the HUD (gmsgRadioIcon).

Keyvalues (Paranoia 2 format):
  message     = sound file (wav/ogg), '!' prefix = don't precache
  netname     = talker name shown on the HUD (or "!entry" of titles.txt)
  health      = hold time in seconds
  rendercolor = panel color (RGB)
  renderamt   = panel transparency
*/

#ifndef RADIO_SENTENCE_H
#define RADIO_SENTENCE_H

#include "cbase.h"

class CRadio : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

#endif // RADIO_SENTENCE_H
