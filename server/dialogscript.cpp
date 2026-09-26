/*
dialogscript.cpp - sistema de dialogo com escolhas do jogador (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

Parser linha-a-linha pra game_dir/scripts/dialogs/dialogs.txt. Formato
documentado em dialogscript.h e game_dir/devkit/GUIA_DIALOGOS.md.

Gramatica de cada no:

    <nome>
    {
        repeatable    0|1
        sealed_line   "chave_titles"
        npc_line      "chave_titles"
        option <N>    "chave_titles" -> <no_alvo|END>
        exit_option   "chave_titles"
        give          "classname"
    }

Segue o mesmo estilo de weaponscript.cpp: arrays fixos globais, LOAD_FILE/
FREE_FILE do engine, funcoes Find* com busca linear (poucas dezenas de nos
no total - nao compensa nenhuma estrutura mais esperta).
*/

#include "extdll.h"
#include "enginecallback.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include "dialogscript.h"

dialognode_t	gDialogNodes[DLG_MAX_NODES];
int		gNumDialogNodes = 0;

static void DLG_Printf( const char *fmt, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, fmt );
#ifdef _MSC_VER
	_vsnprintf( buf, sizeof( buf ) - 1, fmt, ap );
#else
	vsnprintf( buf, sizeof( buf ), fmt, ap );
#endif
	va_end( ap );
	buf[sizeof( buf ) - 1] = 0;
	g_engfuncs.pfnServerPrint( buf );
}

static int DLG_stricmp( const char *a, const char *b )
{
	while( *a && *b )
	{
		int ca = tolower( (unsigned char)*a ), cb = tolower( (unsigned char)*b );
		if( ca != cb ) return ca - cb;
		a++; b++;
	}
	return tolower( (unsigned char)*a ) - tolower( (unsigned char)*b );
}

static void DLG_strncpy( char *dst, const char *src, size_t n )
{
	strncpy( dst, src, n - 1 );
	dst[n-1] = 0;
}

dialognode_t *DialogScript_FindNode( const char *name )
{
	if( !name || !name[0] )
		return NULL;

	for( int i = 0; i < gNumDialogNodes; i++ )
	{
		if( !DLG_stricmp( gDialogNodes[i].name, name ) )
			return &gDialogNodes[i];
	}
	return NULL;
}

// Le o arquivo inteiro (VFS do engine) pra um buffer NUL-terminado. free() no
// chamador. pOutSize (opcional) recebe o tamanho ORIGINAL do arquivo, sem
// contar o NUL que adicionamos - o parser precisa disso porque o proprio
// buffer vira uma sequencia de strings separadas por '\0' (ver
// DialogScript_Parse), entao strlen(text) sozinho nao serve mais de limite.
static char *DLG_LoadText( const char *filename, int *pOutSize )
{
	int size;
	char *buf = (char *)LOAD_FILE( filename, &size );
	char *text;

	if( !buf )
		return NULL;

	text = (char *)malloc( size + 1 );
	memcpy( text, buf, size );
	text[size] = '\0';
	FREE_FILE( buf );

	if( pOutSize )
		*pOutSize = size;
	return text;
}

// Tira espaco/tab das pontas e o comentario "// ..." (fora de aspas - nao
// tem nenhum campo do formato que precise de "//" dentro de uma string).
static char *DLG_TrimLine( char *line )
{
	char *slash = strstr( line, "//" );
	if( slash ) *slash = '\0';

	while( *line == ' ' || *line == '\t' )
		line++;

	size_t len = strlen( line );
	while( len > 0 && ( line[len-1] == ' ' || line[len-1] == '\t' || line[len-1] == '\r' || line[len-1] == '\n' ))
		line[--len] = '\0';

	return line;
}

// Extrai o conteudo entre a primeira e a segunda aspas de "line" pra "out".
// Retorna ponteiro pro resto da linha depois da aspas de fechamento (pode
// ser string vazia), ou NULL se nao achou um par de aspas.
static char *DLG_ExtractQuoted( char *line, char *out, size_t outsz )
{
	char *start = strchr( line, '"' );
	if( !start )
		return NULL;
	start++;
	char *end = strchr( start, '"' );
	if( !end )
		return NULL;

	size_t len = (size_t)( end - start );
	if( len >= outsz )
		len = outsz - 1;
	memcpy( out, start, len );
	out[len] = '\0';

	return end + 1;
}

// "option N" -> devolve N (1-based) ou 0 se a linha nao comecar com "option".
static int DLG_MatchOptionLine( const char *line, int *pSlot )
{
	if( strncmp( line, "option", 6 ) != 0 )
		return 0;

	const char *p = line + 6;
	while( *p == ' ' || *p == '\t' )
		p++;
	if( !isdigit( (unsigned char)*p ) )
		return 0;

	*pSlot = atoi( p );
	return 1;
}

// end e o limite REAL do buffer (ver DialogScript_Parse) - depois da troca
// de \n por \0, o buffer inteiro e uma sequencia de C-strings dentro de um
// unico malloc, entao um ponteiro so vira NULL de verdade se alguem
// atribuir isso explicitamente. Comparar contra end (nao contra NULL) e
// o que impede estourar o buffer quando o arquivo termina sem o '}' de
// fechamento do ultimo no.
static void DLG_ParseNodeBody( char **ppLine, char *end, int *pLineNo, dialognode_t *node )
{
	char *line;

	while( *ppLine < end )
	{
		line = *ppLine;
		*ppLine = line + strlen( line ) + 1;
		(*pLineNo)++;
		line = DLG_TrimLine( line );

		if( !line[0] )
			continue;

		if( line[0] == '}' )
			return; // fim do bloco

		char value[DLG_MAX_TITLE_KEY];
		char *rest;
		int slot;

		if( !strncmp( line, "repeatable", 10 ) )
		{
			const char *p = line + 10;
			while( *p == ' ' || *p == '\t' ) p++;
			node->repeatable = ( atoi( p ) != 0 );
		}
		else if( !strncmp( line, "sealed_line", 11 ) )
		{
			if( DLG_ExtractQuoted( line, value, sizeof( value )))
				DLG_strncpy( node->sealed_line, value, sizeof( node->sealed_line ));
		}
		else if( !strncmp( line, "npc_line", 8 ) )
		{
			if( DLG_ExtractQuoted( line, value, sizeof( value )))
				DLG_strncpy( node->npc_line, value, sizeof( node->npc_line ));
		}
		else if( !strncmp( line, "exit_option", 11 ) )
		{
			if( DLG_ExtractQuoted( line, value, sizeof( value )))
				DLG_strncpy( node->exit_option_text, value, sizeof( node->exit_option_text ));
		}
		else if( !strncmp( line, "give", 4 ) && ( line[4] == ' ' || line[4] == '\t' ))
		{
			if( DLG_ExtractQuoted( line, value, sizeof( value )))
				DLG_strncpy( node->give_classname, value, sizeof( node->give_classname ));
		}
		else if( DLG_MatchOptionLine( line, &slot ))
		{
			if( slot < 1 || slot > DLG_MAX_OPTIONS )
			{
				DLG_Printf( "DialogScript: no '%s' - opcao %d fora do intervalo 1-%d, ignorada\n",
					node->name, slot, DLG_MAX_OPTIONS );
				continue;
			}

			rest = DLG_ExtractQuoted( line, value, sizeof( value ));
			if( !rest )
			{
				DLG_Printf( "DialogScript: no '%s' - 'option %d' sem texto entre aspas, ignorada\n", node->name, slot );
				continue;
			}

			char *arrow = strstr( rest, "->" );
			if( !arrow )
			{
				DLG_Printf( "DialogScript: no '%s' - 'option %d' sem '-> destino', ignorada\n", node->name, slot );
				continue;
			}

			char *target = DLG_TrimLine( arrow + 2 );
			if( !target[0] )
			{
				DLG_Printf( "DialogScript: no '%s' - 'option %d' com destino vazio, ignorada\n", node->name, slot );
				continue;
			}

			int idx = slot - 1;
			if( idx >= node->num_options )
				node->num_options = idx + 1;

			DLG_strncpy( node->options[idx].text, value, sizeof( node->options[idx].text ));
			DLG_strncpy( node->options[idx].target, target, sizeof( node->options[idx].target ));
		}
		else if( line[0] )
		{
			DLG_Printf( "DialogScript: no '%s' - linha nao reconhecida: '%s'\n", node->name, line );
		}
	}

	DLG_Printf( "DialogScript: no '%s' terminou o arquivo sem '}' de fechamento\n", node->name );
}

static void DialogScript_Parse( const char *filename )
{
	int fileLen;
	char *text = DLG_LoadText( filename, &fileLen );
	if( !text )
	{
		DLG_Printf( "DialogScript: nao achei '%s' (nenhuma arvore de dialogo carregada)\n", filename );
		return;
	}

	// Troca todo \n/\r\n por separadores de string C consecutivos, pra dar
	// pra andar com ponteiro (mesmo truque simples de strtok, sem perder o
	// texto entre chamadas como strtok faria). fileLen guarda o limite real
	// do buffer, ja que depois disso o texto vira uma sequencia de strings
	// coladas e strlen() sozinho nao serve mais pra saber onde ele acaba.
	for( char *p = text; *p; p++ )
	{
		if( *p == '\n' || *p == '\r' )
			*p = '\0';
	}

	char *cursor = text;
	char *end = text + fileLen;
	int lineNo = 0;

	while( cursor < end )
	{
		char *line = cursor;
		cursor += strlen( cursor ) + 1;
		lineNo++;
		line = DLG_TrimLine( line );

		if( !line[0] )
			continue;

		// Nome do no (qualquer linha nao vazia que nao seja "{"/"}" e nao
		// esteja dentro de um bloco). A linha seguinte precisa ser "{".
		char nodeName[DLG_MAX_NAME];
		DLG_strncpy( nodeName, line, sizeof( nodeName ));

		char *brace = NULL;
		while( cursor < end )
		{
			brace = cursor;
			cursor += strlen( cursor ) + 1;
			lineNo++;
			brace = DLG_TrimLine( brace );
			if( !brace[0] )
				continue;
			break;
		}

		if( !brace || brace[0] != '{' )
		{
			DLG_Printf( "DialogScript: '%s' - esperava '{' depois de '%s' (linha %d)\n", filename, nodeName, lineNo );
			break;
		}

		if( gNumDialogNodes >= DLG_MAX_NODES )
		{
			DLG_Printf( "DialogScript: limite de %d nos atingido, '%s' e o resto do arquivo foram ignorados\n",
				DLG_MAX_NODES, nodeName );
			break;
		}

		dialognode_t *node = &gDialogNodes[gNumDialogNodes];
		memset( node, 0, sizeof( *node ));
		DLG_strncpy( node->name, nodeName, sizeof( node->name ));

		if( DialogScript_FindNode( nodeName ))
			DLG_Printf( "DialogScript: aviso - no '%s' duplicado (a ultima definicao vence)\n", nodeName );

		DLG_ParseNodeBody( &cursor, end, &lineNo, node );
		gNumDialogNodes++;
	}

	free( text );
}

// Confere se toda option/exit_option leva a um no que existe (ou END), e se
// todo no tem pelo menos uma saida (option ou exit_option) - sem isso o
// jogador fica preso com o movimento travado (ver EnableControl em
// player.cpp). So avisa no console, nao impede o mapa de carregar.
static void DialogScript_Validate( void )
{
	for( int i = 0; i < gNumDialogNodes; i++ )
	{
		dialognode_t *node = &gDialogNodes[i];

		if( !node->npc_line[0] )
			DLG_Printf( "DialogScript: aviso - no '%s' sem npc_line\n", node->name );

		if( node->num_options == 0 && !node->exit_option_text[0] )
			DLG_Printf( "DialogScript: aviso - no '%s' nao tem nenhuma saida (sem option nem exit_option) - jogador fica preso\n", node->name );

		for( int j = 0; j < node->num_options; j++ )
		{
			if( !node->options[j].text[0] )
			{
				// buraco na numeracao (ex: definiu "option 1" e "option 3" e
				// pulou "option 2") - o cliente recebe uma opcao vazia nesse
				// slot, entao vale avisar em vez de deixar passar batido.
				DLG_Printf( "DialogScript: aviso - no '%s', 'option %d' nunca foi definida (buraco na numeracao)\n",
					node->name, j + 1 );
				continue;
			}

			if( !DLG_stricmp( node->options[j].target, "END" ))
				continue;

			if( !DialogScript_FindNode( node->options[j].target ))
			{
				DLG_Printf( "DialogScript: erro - no '%s', opcao %d aponta pra '%s', que nao existe\n",
					node->name, j + 1, node->options[j].target );
			}
		}
	}
}

void DialogScript_Init( void )
{
	gNumDialogNodes = 0;
	DialogScript_Parse( "scripts/dialogs/dialogs.txt" );
	DialogScript_Validate();
	DLG_Printf( "DialogScript: %d no(s) de dialogo carregado(s)\n", gNumDialogNodes );
}
