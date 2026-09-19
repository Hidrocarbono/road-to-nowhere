/*
dialogsession.cpp - sistema de dialogo com escolhas do jogador (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

Implementa os metodos de CBasePlayer declarados em player.h (Dialog_Start/
Dialog_Select/Dialog_End/Dialog_Cancel) e o hook de morte do NPC
(Dialog_NotifyNPCDied, declarado em dialogscript.h, chamado por
CBaseMonster::Killed em combat.cpp).

Fluxo:
  CBaseMonster::Use()        (monsters.cpp)  -> Dialog_Start()
  client command "dlgselect" (client.cpp)    -> Dialog_Select()
  CBaseMonster::Killed()     (combat.cpp)    -> Dialog_NotifyNPCDied() -> Dialog_Cancel()

Sessao 100% server-authoritative: o cliente so mostra o que gmsgDialogShow
manda e devolve um numero de 1 a 9 via "dlgselect N" - nunca sabe o nome do
proximo no nem o conteudo de titles.txt referenciado por ele antes do
servidor mandar.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "basemonster.h"
#include "dialogscript.h"
#include "user_messages.h"

// ---------------------------------------------------------------------
// historico por jogador (selados / ja deram give) - arrays fixos, ver
// MAX_DIALOG_SEALED/MAX_DIALOG_GIVEN em dialogscript.h
// ---------------------------------------------------------------------

static bool Dialog_IsSealed( CBasePlayer *pPlayer, const char *pszRoot )
{
	for( int i = 0; i < pPlayer->m_iDialogSealedCount; i++ )
	{
		if( !Q_stricmp( STRING( pPlayer->m_iszDialogSealed[i] ), pszRoot ))
			return true;
	}
	return false;
}

static void Dialog_MarkSealed( CBasePlayer *pPlayer, const char *pszRoot )
{
	if( Dialog_IsSealed( pPlayer, pszRoot ))
		return;

	if( pPlayer->m_iDialogSealedCount >= MAX_DIALOG_SEALED )
	{
		ALERT( at_console, "Dialog: limite de %d arvores seladas por jogador atingido, '%s' nao foi marcada\n",
			MAX_DIALOG_SEALED, pszRoot );
		return;
	}

	pPlayer->m_iszDialogSealed[pPlayer->m_iDialogSealedCount++] = ALLOC_STRING( pszRoot );
}

static bool Dialog_WasGiven( CBasePlayer *pPlayer, const char *pszNode )
{
	for( int i = 0; i < pPlayer->m_iDialogGivenCount; i++ )
	{
		if( !Q_stricmp( STRING( pPlayer->m_iszDialogGiven[i] ), pszNode ))
			return true;
	}
	return false;
}

static void Dialog_MarkGiven( CBasePlayer *pPlayer, const char *pszNode )
{
	if( Dialog_WasGiven( pPlayer, pszNode ))
		return;

	if( pPlayer->m_iDialogGivenCount >= MAX_DIALOG_GIVEN )
	{
		ALERT( at_console, "Dialog: limite de %d itens de dialogo por jogador atingido, '%s' nao foi marcado\n",
			MAX_DIALOG_GIVEN, pszNode );
		return;
	}

	pPlayer->m_iszDialogGiven[pPlayer->m_iDialogGivenCount++] = ALLOC_STRING( pszNode );
}

// ---------------------------------------------------------------------
// rede
// ---------------------------------------------------------------------

// Manda a fala do NPC + ate DLG_MAX_OPTIONS+1 opcoes (as numeradas do
// script, e por ultimo o exit_option se o no tiver um). O cliente resolve
// cada chave em titles.txt sozinho (TextMessageGet) - so viajam nomes.
static void Dialog_SendNode( CBasePlayer *pPlayer, dialognode_t *node )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgDialogShow, NULL, pPlayer->pev );
		WRITE_BYTE( pPlayer->m_iDialogTurn & 0xFF );
		WRITE_STRING( node->npc_line );

		int numSlots = node->num_options + ( node->exit_option_text[0] ? 1 : 0 );
		WRITE_BYTE( numSlots );

		for( int i = 0; i < node->num_options; i++ )
			WRITE_STRING( node->options[i].text[0] ? node->options[i].text : "" );

		if( node->exit_option_text[0] )
			WRITE_STRING( node->exit_option_text );
	MESSAGE_END();
}

// numSlots 0 = esconde o HUD de dialogo no cliente.
static void Dialog_SendClose( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgDialogShow, NULL, pPlayer->pev );
		WRITE_BYTE( 0 );
		WRITE_STRING( "" );
		WRITE_BYTE( 0 );
	MESSAGE_END();
}

// Fala curta ("ja conversamos") quando o jogador reusa um NPC cuja arvore
// ja foi selada. Nao abre sessao nenhuma - reusa UTIL_ShowMessage (mesmo
// canal HudText que env_message/game_text ja usam pra puxar de titles.txt).
static void Dialog_ShowSealedLine( CBasePlayer *pPlayer, const char *pszKey )
{
	char buf[128];
	Q_snprintf( buf, sizeof( buf ), "#%s", pszKey );
	UTIL_ShowMessage( buf, pPlayer );
}

// ---------------------------------------------------------------------
// entrar num no: executa "give" (uma vez so, mesmo com repeatable 1) e
// manda o no pro cliente. Usado tanto pelo primeiro no (Dialog_Start)
// quanto por qualquer transicao (Dialog_Select).
// ---------------------------------------------------------------------
static void Dialog_EnterNode( CBasePlayer *pPlayer, dialognode_t *node )
{
	if( node->give_classname[0] && !Dialog_WasGiven( pPlayer, node->name ))
	{
		pPlayer->GiveNamedItem( node->give_classname );
		Dialog_MarkGiven( pPlayer, node->name );
	}

	pPlayer->m_iszDialogNode = ALLOC_STRING( node->name );
	pPlayer->m_iDialogTurn++;

	Dialog_SendNode( pPlayer, node );
}

// Fecha a sessao: destrava o jogador e esconde o HUD. bSeal so faz sentido
// quando a conversa termina "de verdade" (o jogador chegou num END natural
// escolhendo uma opcao) - sair pelo exit_option ou a sessao ser cancelada
// (NPC morreu) NAO sela, pra nao trancar uma conversa que o jogador nunca
// terminou.
static void Dialog_CloseSession( CBasePlayer *pPlayer, bool bSeal )
{
	if( bSeal && !FStringNull( pPlayer->m_iszDialogRoot ))
	{
		dialognode_t *root = DialogScript_FindNode( STRING( pPlayer->m_iszDialogRoot ));
		if( root && !root->repeatable )
			Dialog_MarkSealed( pPlayer, root->name );
	}

	pPlayer->EnableControl( TRUE );
	pPlayer->m_bInDialog = FALSE;
	pPlayer->m_hDialogNPC = NULL;
	pPlayer->m_iszDialogRoot = iStringNull;
	pPlayer->m_iszDialogNode = iStringNull;

	Dialog_SendClose( pPlayer );
}

// ---------------------------------------------------------------------
// API de CBasePlayer (player.h)
// ---------------------------------------------------------------------

void CBasePlayer::Dialog_Start( CBaseMonster *pNPC, const char *pszStartNode )
{
	if( m_bInDialog )
		return; // ja conversando com alguem - +USE duplo nao reabre por cima

	dialognode_t *node = DialogScript_FindNode( pszStartNode );
	if( !node )
	{
		ALERT( at_console, "Dialog: NPC aponta pra '%s', que nao existe em scripts/dialogs/dialogs.txt\n", pszStartNode );
		return;
	}

	if( !node->repeatable && Dialog_IsSealed( this, pszStartNode ))
	{
		if( node->sealed_line[0] )
			Dialog_ShowSealedLine( this, node->sealed_line );
		return;
	}

	EnableControl( FALSE );
	m_bInDialog = TRUE;
	m_hDialogNPC = pNPC;
	m_iszDialogRoot = ALLOC_STRING( pszStartNode );

	Dialog_EnterNode( this, node );
}

void CBasePlayer::Dialog_Select( int iTurn, int iSlot )
{
	if( !m_bInDialog )
		return;

	// Anti-replay: o cliente so pode ter recebido este "turn" no ultimo
	// DialogShow. Um "dlgselect" atrasado (ex: o jogador clicou, o no
	// avancou, e so DEPOIS o pacote antigo chega) ou reenviado por fora do
	// cliente normal e ignorado em silencio - m_iDialogTurn so muda quando
	// o SERVIDOR manda um no novo (Dialog_EnterNode), nunca por conta do
	// cliente.
	if( iTurn != ( m_iDialogTurn & 0xFF ))
		return;

	CBaseEntity *pNPC = m_hDialogNPC;
	if( !pNPC || !((CBaseMonster *)pNPC)->IsAlive() )
	{
		// o NPC devia ter cancelado a sessao ao morrer (Dialog_NotifyNPCDied) -
		// se chegou aqui mesmo assim, fecha sem selar por seguranca.
		Dialog_CloseSession( this, false );
		return;
	}

	dialognode_t *node = DialogScript_FindNode( STRING( m_iszDialogNode ));
	if( !node )
	{
		Dialog_CloseSession( this, false );
		return;
	}

	// slot invalido (fora do intervalo do no atual, ou um "buraco" de
	// numeracao no script) - ignora em silencio. m_iDialogTurn ja impede um
	// "dlgselect" atrasado de um no ANTERIOR ser aplicado aqui, porque o
	// cliente so manda o slot depois de ter recebido este exato DialogShow.
	if( node->exit_option_text[0] && iSlot == node->num_options + 1 )
	{
		Dialog_End();
		return;
	}

	if( iSlot < 1 || iSlot > node->num_options || !node->options[iSlot-1].text[0] )
		return;

	dialogoption_t *opt = &node->options[iSlot-1];

	if( !Q_stricmp( opt->target, "END" ))
	{
		Dialog_CloseSession( this, true );
		return;
	}

	dialognode_t *next = DialogScript_FindNode( opt->target );
	if( !next )
	{
		ALERT( at_console, "Dialog: no '%s', opcao %d aponta pra '%s', que nao existe\n", node->name, iSlot, opt->target );
		Dialog_CloseSession( this, false );
		return;
	}

	Dialog_EnterNode( this, next );
}

void CBasePlayer::Dialog_End( void )
{
	if( !m_bInDialog )
		return;
	Dialog_CloseSession( this, false ); // saida manual pelo exit_option - nao sela
}

void CBasePlayer::Dialog_Cancel( void )
{
	if( !m_bInDialog )
		return;
	Dialog_CloseSession( this, false ); // sessao interrompida de fora (NPC morreu) - nao sela
}

// ---------------------------------------------------------------------
// hook de morte do NPC (dialogscript.h, chamado por CBaseMonster::Killed)
// ---------------------------------------------------------------------
void Dialog_NotifyNPCDied( CBaseEntity *pNPC )
{
	// RTN: singleplayer only (ver CLAUDE.md) - so existe o jogador do slot 1.
	CBaseEntity *pEnt = CBaseEntity::Instance( INDEXENT( 1 ));
	if( !pEnt || !pEnt->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pEnt;
	if( pPlayer->InDialog() && pPlayer->m_hDialogNPC == pNPC )
		pPlayer->Dialog_Cancel();
}
