/*
trigger_textwindow.h - entity that shows a document window (Paranoia 2 style)
Copyright (C) 2026 Hermes e Hidrocarboneto (RTN)

The entity sends the file name (pev->message) to the client, which opens
texts/<name>.txt and shows the document window (image + close button).
*/

#ifndef TRIGGER_TEXTWINDOW_H
#define TRIGGER_TEXTWINDOW_H

#include "cbase.h"

class CTriggerTextWindow : public CBaseEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

#endif // TRIGGER_TEXTWINDOW_H
