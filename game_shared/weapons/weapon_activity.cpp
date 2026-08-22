/*
weapon_activity.cpp - resolucao de animacao de viewmodel por ACTIVITY (RTN)
Copyright (C) 2026 Road to Nowhere - port do padrao do Paranoia 2 (Uncle Mike, XWS)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Ver weapon_activity.h para o porque. Este .cpp e o unico ponto do sistema que
enxerga studio.h - ele e compilado nas DUAS dlls (server/CMakeLists.txt e
client/CMakeLists.txt fazem GLOB de game_shared/weapons/*.cpp), entao servidor e
cliente resolvem activity exatamente com o mesmo codigo.
*/

#include "weapon_activity.h"
#include "studio.h"

int WeaponActivity_Count( const void *hdr, int activity )
{
	const studiohdr_t *pstudiohdr = (const studiohdr_t *)hdr;

	if( !pstudiohdr || activity <= 0 || pstudiohdr->numseq <= 0 )
		return 0;

	const mstudioseqdesc_t *pseqdesc = (const mstudioseqdesc_t *)((const unsigned char *)pstudiohdr + pstudiohdr->seqindex);
	int matches = 0;

	for( int i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( pseqdesc[i].activity == activity )
			matches++;
	}

	return matches;
}

int WeaponActivity_Lookup( const void *hdr, int activity, int variant )
{
	const studiohdr_t *pstudiohdr = (const studiohdr_t *)hdr;
	const int matches = WeaponActivity_Count( hdr, activity );

	if( matches <= 0 )
		return WACT_NOT_AVAILABLE;

	const mstudioseqdesc_t *pseqdesc = (const mstudioseqdesc_t *)((const unsigned char *)pstudiohdr + pstudiohdr->seqindex);

	// da a volta em vez de falhar: pedir a variante 2 num modelo com uma unica
	// sequencia daquela activity devolve essa sequencia.
	if( variant < 0 )
		variant = -variant;
	variant %= matches;

	for( int i = 0; i < pstudiohdr->numseq; i++ )
	{
		if( pseqdesc[i].activity == activity && variant-- == 0 )
			return i;
	}

	return WACT_NOT_AVAILABLE;
}
