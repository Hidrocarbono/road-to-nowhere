/*
dialogscript.cpp - sistema de dialogo com escolhas do jogador (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

Parser de scripts/dialogs/dialog_*.txt. Formato documentado em
dialogscript.h e game_dir/devkit/GUIA_DIALOGOS.md.

RTN F13: reescrita completa - texto embutido direto na estrutura (nao mais
chave de titles.txt), um arquivo por NPC carregado por varredura de
diretorio (mesmo padrao de WeaponScript_LoadAll() em weaponscript.cpp pra
weapon_*.txt - GET_GAME_DIR + std::filesystem::directory_iterator, LOAD_FILE
aceita o caminho absoluto resultante).

Dois "modos" de leitura de linha:
  - ESTRUTURA (DLG_TrimLine: tira espaco das pontas E comentario "//") -
    nomes de bloco, "option N ... -> alvo", "font", etc.
  - TEXTO CRU (DLG_TrimLeading, sem tocar em "//") - o corpo de npc_text/
    sealed, onde cada linha e literal ate a linha "}" sozinha fechar o
    bloco. Mesma regra que o parser nativo do engine ja usa pro proprio
    titles.txt (dentro de um bloco { }, nenhuma linha e comentario).
*/

#include "extdll.h"
#include "enginecallback.h"
#include "filesystem_utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <string>
#include <filesystem>
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

// Le o arquivo inteiro pra um buffer NUL-terminado. free() no chamador.
// pOutSize (opcional) recebe o tamanho ORIGINAL do arquivo, sem contar o
// NUL que adicionamos - o parser precisa disso porque o proprio buffer vira
// uma sequencia de strings separadas por '\0' (ver DialogScript_ParseFile),
// entao strlen(text) sozinho nao serve mais de limite. LOAD_FILE aceita
// tanto caminho relativo a VFS do engine quanto absoluto (confirmado - e
// exatamente como WeaponScript_LoadAll ja chama WS_LoadText com o caminho
// absoluto que o directory_iterator devolve).
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
// tem nenhum campo de ESTRUTURA do formato que precise de "//" dentro de
// uma string). NUNCA usar isto dentro de um bloco de texto cru.
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

// So tira espaco/tab das DUAS pontas - sem tocar em "//". Usado dentro de
// bloco de texto cru (npc_text/sealed): a indentacao de editor (o autor
// alinhando visualmente com o resto do arquivo) nao deve virar espaco
// fantasma no inicio/fim da fala, mas "//" no meio de uma frase e so dois
// caracteres literais, nunca comentario.
static char *DLG_TrimLeading( char *line )
{
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

// Junta "line" no final de "dst" (buffer de dstSize), separado por '\n' se
// dst ja tiver conteudo. Trunca em silencio (nunca estoura) se nao couber -
// o mesmo padrao defensivo do resto deste parser (DLG_strncpy).
static void DLG_AppendTextLine( char *dst, size_t dstSize, const char *line )
{
	size_t used = strlen( dst );
	size_t avail = ( dstSize > used ) ? ( dstSize - used ) : 0;
	if( avail <= 1 )
		return; // ja sem espaco nem pro terminador

	if( used > 0 && avail > 1 )
	{
		dst[used++] = '\n';
		dst[used] = '\0';
		avail--;
	}

	strncat( dst, line, avail - 1 );
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

static void DLG_ParseColor( const char *p, int out[3] )
{
	out[0] = atoi( p );
	while( *p && *p != ' ' && *p != '\t' ) p++;
	while( *p == ' ' || *p == '\t' ) p++;
	out[1] = atoi( p );
	while( *p && *p != ' ' && *p != '\t' ) p++;
	while( *p == ' ' || *p == '\t' ) p++;
	out[2] = atoi( p );
}

// Le um bloco de TEXTO CRU - o "{" de abertura ja foi consumido pelo
// chamador, isto le ate achar uma linha cujo conteudo (so com
// DLG_TrimLeading, "//" NAO conta) seja exatamente "}". Cada linha entra
// literal (so com a indentacao das pontas tirada) em dst, separadas por
// '\n'. Mesma convencao de "end como limite real do buffer" que o resto
// deste parser usa (ver comentario em DialogScript_ParseFile).
static void DLG_ParseRawTextBlock( char **ppLine, char *end, int *pLineNo, char *dst, size_t dstSize, const char *context )
{
	dst[0] = '\0';
	char *line;

	while( *ppLine < end )
	{
		line = *ppLine;
		*ppLine = line + strlen( line ) + 1;
		(*pLineNo)++;
		line = DLG_TrimLeading( line );

		if( !strcmp( line, "}" ) )
			return; // fim do bloco

		DLG_AppendTextLine( dst, dstSize, line );
	}

	DLG_Printf( "DialogScript: %s - bloco de texto terminou o arquivo sem '}' de fechamento\n", context );
}

// Confere se a proxima linha nao-vazia (comentarios fora) e exatamente
// "{" - usado antes de entrar num bloco (node/npc_text/sealed). Avanca o
// cursor ate la (ou ate 'end', se nao achar).
static bool DLG_ExpectOpenBrace( char **ppLine, char *end, int *pLineNo, const char *context )
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
		if( line[0] == '{' )
			return true;
		DLG_Printf( "DialogScript: %s - esperava '{', achei '%s' (linha %d)\n", context, line, *pLineNo );
		return false;
	}
	DLG_Printf( "DialogScript: %s - arquivo terminou esperando '{'\n", context );
	return false;
}

// Le o corpo de um "node { ... }" (o "{" ja foi consumido). pStyle entra com
// o estilo HERDADO do arquivo (copiado pro node ja de saida - qualquer
// diretiva de estilo aqui dentro so sobrescreve os campos que ela mesma
// declarar). bFirstNode diz se repeatable/sealed_text (propriedades da
// ARVORE, nao do node) devem ser aplicados aqui.
static void DLG_ParseNodeBody( char **ppLine, char *end, int *pLineNo, dialognode_t *node,
	const dialogstyle_t *inheritedStyle, bool bFirstNode, bool treeRepeatable, const char *treeSealedText )
{
	node->style = *inheritedStyle;
	if( bFirstNode )
	{
		node->repeatable = treeRepeatable;
		DLG_strncpy( node->sealed_text, treeSealedText, sizeof( node->sealed_text ));
	}

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
			return; // fim do node

		char value[DLG_MAX_LABEL];
		char *rest;
		int slot;

		if( !strncmp( line, "npc_text", 8 ) )
		{
			if( !DLG_ExpectOpenBrace( ppLine, end, pLineNo, node->name ))
				return;
			DLG_ParseRawTextBlock( ppLine, end, pLineNo, node->npc_text, sizeof( node->npc_text ), node->name );
		}
		else if( !strncmp( line, "exit_option", 11 ) )
		{
			if( DLG_ExtractQuoted( line, value, sizeof( value )))
				DLG_strncpy( node->exit_option_text, value, sizeof( node->exit_option_text ));
		}
		else if( !strncmp( line, "give", 4 ) && ( line[4] == ' ' || line[4] == '\t' ))
		{
			char classname[DLG_MAX_CLASSNAME];
			if( DLG_ExtractQuoted( line, classname, sizeof( classname )))
				DLG_strncpy( node->give_classname, classname, sizeof( node->give_classname ));
		}
		else if( !strncmp( line, "fontsize", 8 ) )
		{
			const char *p = line + 8;
			while( *p == ' ' || *p == '\t' ) p++;
			node->style.fontsize = atoi( p );
		}
		else if( !strncmp( line, "font", 4 ) && ( line[4] == ' ' || line[4] == '\t' ))
		{
			const char *p = line + 4;
			while( *p == ' ' || *p == '\t' ) p++;
			DLG_strncpy( node->style.font, p, sizeof( node->style.font ));
		}
		else if( !strncmp( line, "color_speaker", 13 ) )
		{
			DLG_ParseColor( line + 13, node->style.color_speaker );
		}
		else if( !strncmp( line, "color_text", 10 ) )
		{
			DLG_ParseColor( line + 10, node->style.color_text );
		}
		else if( DLG_MatchOptionLine( line, &slot ) )
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

			DLG_strncpy( node->options[idx].label, value, sizeof( node->options[idx].label ));
			DLG_strncpy( node->options[idx].target, target, sizeof( node->options[idx].target ));
		}
		else
		{
			DLG_Printf( "DialogScript: no '%s' - linha nao reconhecida: '%s'\n", node->name, line );
		}
	}

	DLG_Printf( "DialogScript: no '%s' terminou o arquivo sem '}' de fechamento\n", node->name );
}

// Le o corpo de "dialog_<npc> { ... }" (o "{" ja foi consumido).
static void DLG_ParseTreeBody( char **ppLine, char *end, int *pLineNo, const char *filename, const char *treeLabel )
{
	dialogstyle_t style;
	memset( &style, 0, sizeof( style ));
	style.color_speaker[0] = style.color_speaker[1] = style.color_speaker[2] = -1;
	style.color_text[0] = style.color_text[1] = style.color_text[2] = -1;

	bool treeRepeatable = false;
	char treeSealedText[DLG_MAX_TEXT];
	treeSealedText[0] = '\0';
	bool bFirstNode = true;

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
			return; // fim da arvore

		if( !strncmp( line, "repeatable", 10 ) )
		{
			const char *p = line + 10;
			while( *p == ' ' || *p == '\t' ) p++;
			treeRepeatable = ( atoi( p ) != 0 );
		}
		else if( !strncmp( line, "sealed", 6 ) && ( line[6] == '\0' || line[6] == ' ' || line[6] == '\t' ))
		{
			if( !DLG_ExpectOpenBrace( ppLine, end, pLineNo, treeLabel ))
				return;
			DLG_ParseRawTextBlock( ppLine, end, pLineNo, treeSealedText, sizeof( treeSealedText ), treeLabel );
		}
		else if( !strncmp( line, "fontsize", 8 ) )
		{
			const char *p = line + 8;
			while( *p == ' ' || *p == '\t' ) p++;
			style.fontsize = atoi( p );
		}
		else if( !strncmp( line, "font", 4 ) && ( line[4] == ' ' || line[4] == '\t' ))
		{
			const char *p = line + 4;
			while( *p == ' ' || *p == '\t' ) p++;
			DLG_strncpy( style.font, p, sizeof( style.font ));
		}
		else if( !strncmp( line, "color_speaker", 13 ) )
		{
			DLG_ParseColor( line + 13, style.color_speaker );
		}
		else if( !strncmp( line, "color_text", 10 ) )
		{
			DLG_ParseColor( line + 10, style.color_text );
		}
		else if( !strncmp( line, "node", 4 ) && ( line[4] == ' ' || line[4] == '\t' ))
		{
			const char *p = line + 4;
			while( *p == ' ' || *p == '\t' ) p++;
			char nodeName[DLG_MAX_NAME];
			DLG_strncpy( nodeName, p, sizeof( nodeName ));

			if( !DLG_ExpectOpenBrace( ppLine, end, pLineNo, nodeName ))
				return;

			if( gNumDialogNodes >= DLG_MAX_NODES )
			{
				DLG_Printf( "DialogScript: limite de %d nos atingido, '%s' e o resto de '%s' foram ignorados\n",
					DLG_MAX_NODES, nodeName, filename );
				return;
			}

			if( DialogScript_FindNode( nodeName ))
				DLG_Printf( "DialogScript: aviso - no '%s' duplicado (a ultima definicao vence)\n", nodeName );

			dialognode_t *node = &gDialogNodes[gNumDialogNodes];
			memset( node, 0, sizeof( *node ));
			DLG_strncpy( node->name, nodeName, sizeof( node->name ));

			DLG_ParseNodeBody( ppLine, end, pLineNo, node, &style, bFirstNode, treeRepeatable, treeSealedText );
			gNumDialogNodes++;
			bFirstNode = false;
		}
		else
		{
			DLG_Printf( "DialogScript: '%s' - linha nao reconhecida no bloco de '%s': '%s'\n", filename, treeLabel, line );
		}
	}

	DLG_Printf( "DialogScript: '%s' - '%s' terminou o arquivo sem '}' de fechamento\n", filename, treeLabel );
}

static void DialogScript_ParseFile( const char *filename )
{
	int fileLen;
	char *text = DLG_LoadText( filename, &fileLen );
	if( !text )
	{
		DLG_Printf( "DialogScript: nao consegui ler '%s'\n", filename );
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

	// Primeira linha nao-vazia = label da arvore (so documentacao - nao
	// precisa bater com o nome do arquivo nem com nenhum node). Segunda
	// linha nao-vazia tem que ser "{".
	char *line = NULL;
	while( cursor < end )
	{
		line = cursor;
		cursor += strlen( cursor ) + 1;
		lineNo++;
		line = DLG_TrimLine( line );
		if( line[0] )
			break;
	}

	if( !line || !line[0] )
	{
		DLG_Printf( "DialogScript: '%s' esta vazio\n", filename );
		free( text );
		return;
	}

	char treeLabel[DLG_MAX_NAME];
	DLG_strncpy( treeLabel, line, sizeof( treeLabel ));

	if( !DLG_ExpectOpenBrace( &cursor, end, &lineNo, treeLabel ))
	{
		free( text );
		return;
	}

	DLG_ParseTreeBody( &cursor, end, &lineNo, filename, treeLabel );

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

		if( !node->npc_text[0] )
			DLG_Printf( "DialogScript: aviso - no '%s' sem npc_text\n", node->name );

		if( node->num_options == 0 && !node->exit_option_text[0] )
			DLG_Printf( "DialogScript: aviso - no '%s' nao tem nenhuma saida (sem option nem exit_option) - jogador fica preso\n", node->name );

		for( int j = 0; j < node->num_options; j++ )
		{
			if( !node->options[j].label[0] )
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

	char gamedir[256];
	GET_GAME_DIR( gamedir );
	std::string base = std::string( gamedir ) + "/scripts/dialogs";
	DLG_Printf( "DialogScript: varrendo [%s] por dialog_*.txt\n", base.c_str() );

	int found = 0;
	try
	{
		for( auto &entry : std::filesystem::directory_iterator( base ) )
		{
			std::string name = entry.path().filename().string();
			if( name.rfind( "dialog_", 0 ) == 0 && name.size() > 7 && name.substr( name.size()-4 ) == ".txt" )
			{
				DialogScript_ParseFile( entry.path().string().c_str() );
				found++;
			}
		}
	}
	catch( ... )
	{
		DLG_Printf( "DialogScript: nao consegui varrer %s (pasta nao existe?)\n", base.c_str() );
	}

	DialogScript_Validate();
	DLG_Printf( "DialogScript: %d arquivo(s) dialog_*.txt, %d no(s) de dialogo carregado(s)\n", found, gNumDialogNodes );
}
