/*
dialogscript.h - sistema de dialogo com escolhas do jogador (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

Parser server-only (o cliente nao tem acesso a este arquivo - mesmo motivo
de weaponscript.cpp ser server-only: o servidor precisa ser autoritativo
sobre qual e o proximo no da conversa) para
game_dir/scripts/dialogs/dialogs.txt.

Formato (ver game_dir/devkit/GUIA_DIALOGOS.md para a referencia completa):

    dialog_doutor
    {
        repeatable    0
        sealed_line   "Doutor_JaConversamos"
        npc_line      "Doutor_Intro"
        option 1      "Opcao_PerguntarVacina" -> dialog_doutor_vacina
        option 2      "Opcao_PerguntarSaida"  -> dialog_doutor_saida
        exit_option   "Opcao_Encerrar"
    }

Todo TEXTO exibido (npc_line, option, exit_option, sealed_line) e uma CHAVE
de game_dir/titles.txt, nunca o texto em si - o parser so guarda o nome.
"give" e o unico campo que NAO e chave de titles.txt: e um classname de
item (ex.: "item_antidote"), repassado direto pra CBasePlayer::GiveNamedItem().
*/

#ifndef DIALOGSCRIPT_H
#define DIALOGSCRIPT_H

#define DLG_MAX_NODES		64
#define DLG_MAX_OPTIONS		8	// teclas 1-8 (numeros do menu nativo, ver client/ammo.cpp SelectSlot)
#define DLG_MAX_NAME		48
#define DLG_MAX_TITLE_KEY	64
#define DLG_MAX_CLASSNAME	64

// Tamanho do historico por jogador (server/player.h) de arvores seladas e de
// nos que ja deram "give" - poucos NPCs-chave por design (ver CLAUDE.md),
// nao precisa de mais que isso.
#define MAX_DIALOG_SEALED	8
#define MAX_DIALOG_GIVEN	16

typedef struct dialogoption_s
{
	char	text[DLG_MAX_TITLE_KEY];	// chave de titles.txt
	char	target[DLG_MAX_NAME];		// nome de outro no deste arquivo, ou "END"
} dialogoption_t;

typedef struct dialognode_s
{
	char	name[DLG_MAX_NAME];

	char	npc_line[DLG_MAX_TITLE_KEY];		// obrigatorio
	char	give_classname[DLG_MAX_CLASSNAME];	// opcional - vazio = sem give
	char	exit_option_text[DLG_MAX_TITLE_KEY];	// opcional - vazio = sem exit_option

	// So tem efeito relevante no no que a keyvalue "dialog_target" do NPC
	// aponta (o "no inicial" da arvore) - ver DialogScript_IsSealedFor().
	bool	repeatable;
	char	sealed_line[DLG_MAX_TITLE_KEY];	// opcional, so com repeatable == false

	int	num_options;
	dialogoption_t	options[DLG_MAX_OPTIONS];
} dialognode_t;

extern dialognode_t	gDialogNodes[DLG_MAX_NODES];
extern int		gNumDialogNodes;

// Le game_dir/scripts/dialogs/dialogs.txt uma unica vez (chamado de
// GameDLLInit(), igual WeaponScript_Init() - server/game.cpp). Nao recarrega
// por mapa.
void DialogScript_Init( void );

// Busca um no pelo nome (case-insensitive). NULL se nao existir - quem
// chama decide se isso e erro silencioso ou barulhento (ver DialogSession
// em player.cpp, que sempre loga no console do servidor).
dialognode_t *DialogScript_FindNode( const char *name );

// Chamado de CBaseMonster::Killed() (server/combat.cpp) pra fechar (sem
// "give" pendente) a conversa do jogador com esse NPC, se ele morrer no
// meio dela - sem isso o jogador ficaria travado pra sempre (EnableControl
// FALSE nunca desfeito). Implementado em server/dialogsession.cpp.
void Dialog_NotifyNPCDied( class CBaseEntity *pNPC );

// Chamado de CBaseMonster::RunAI() (server/monsterstate.cpp) pra pausar a
// IA (Look/Listen/inimigo/schedule) do NPC enquanto ele estiver
// conversando com o jogador - sem isso, som/inimigo no ambiente troca o
// schedule dele no meio da conversa (ex.: "scared" -> ACT_CROUCHIDLE) e a
// animacao fica alternando enquanto o jogador so consegue ler o menu.
// Implementado em server/dialogsession.cpp.
bool Dialog_IsTalkingToPlayer( class CBaseEntity *pNPC );

#endif // DIALOGSCRIPT_H
