#include "rtn_crosshair_state.h"

static float s_flSpreadTan = 0.0f;
static bool  s_bIronSight = false;
static bool  s_bValid = false;

void RTN_SetCurrentSpread( float flSpreadTan, bool bIronSight )
{
	s_flSpreadTan = flSpreadTan;
	s_bIronSight = bIronSight;
	s_bValid = true;
}

float RTN_GetCurrentSpreadTan()
{
	return s_flSpreadTan;
}

bool RTN_IsCurrentSpreadIronSight()
{
	return s_bIronSight;
}

bool RTN_HasCurrentSpread()
{
	return s_bValid;
}
