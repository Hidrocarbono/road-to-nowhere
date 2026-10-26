/*
dialogscript.h - sistema de dialogo com escolhas do jogador (RTN)
Copyright (C) 2026 Hermes e Hidrocarboneto

Parser server-only (o cliente nao tem acesso a este arquivo - mesmo motivo
de weaponscript.cpp ser server-only: o servidor precisa ser autoritativo
sobre qual e o proximo no da conversa). Um arquivo por NPC,
scripts/dialogs/dialog_<npc>.txt, carregados por VARREDURA de diretorio
(mesmo padrao de weaponscript.cpp pra weapon_*.txt - nao precisa editar
codigo nem listar nome de arquivo em lugar nenhum pra adicionar um NPC
novo, so criar o .txt seguindo a convencao de nome).

RTN F13: reescrita completa - ate o commit anterior, o TEXTO exibido
(npc_line/option/sealed_line) era sempre uma CHAVE de titles.txt, nunca o
texto em si (dois arquivos separados pra uma mesma arvore). Agora o texto
mora direto aqui, embutido na propria estrutura - titles.txt volta a ser
so pra mensagens (env_message/system tip/pickup), nao mais pra dialogo.

Formato (ver game_dir/devkit/GUIA_DIALOGOS.md para a referencia completa):

    dialog_ivo                     // label do arquivo - so documentacao,
    {                               // "dialog_target" aponta pro NOME DO NODE,
                                    // nao pra este label (ver node abaixo)
        repeatable      0           // vale pra ARVORE INTEIRA (nao por node)
        sealed                      // opcional - mostrado se repeatable 0 e
        {                           // a arvore ja foi selada
            Tarde demais...
        }

        font            roboto_small   // opcional - padrao do cliente se omitido
        fontsize        16             // opcional
        color_speaker   255 210 64     // opcional
        color_text      255 255 255    // opcional

        node ivo_intro              // "dialog_target" "ivo_intro" no editor
        {
            npc_text
            {
                Ivo:
                Você... você não devia ter vindo. Ninguém devia.
            }
            option 1 "Por que você está aqui?" -> ivo_a
        }

        node ivo_a
        {
            give "item_antidote"    // opcional - dispara ao ENTRAR no node
            npc_text { ... }
            option 1 "..." -> END
            exit_option "Vou embora."   // opcional
        }
    }

Dentro de um bloco { } de npc_text/sealed, TODA linha e texto cru - "//"
NUNCA e comentario ali dentro (so fora, nas linhas de estrutura), mesma
regra que o parser nativo do engine ja usa pro proprio titles.txt.
font/fontsize/color_speaker/color_text valem pro arquivo inteiro quando
declarados no topo (antes do primeiro "node"), e qualquer "node" pode
sobrescrever só pra si repetindo a mesma diretiva dentro do seu bloco.
"give" e o unico campo que NAO e texto de tela: e um classname de item
(ex.: "item_antidote"), repassado direto pra CBasePlayer::GiveNamedItem().
*/

#ifndef DIALOGSCRIPT_H
#define DIALOGSCRIPT_H

// RTN: subido de 64 pra 192 em preparacao pra instalar dialogo em todos os
// inimigos principais - ver historico no commit que mudou isso.
#define DLG_MAX_NODES		192
#define DLG_MAX_OPTIONS		8	// teclas 1-8 (numeros do menu nativo, ver client/ammo.cpp SelectSlot)
#define DLG_MAX_NAME		48
#define DLG_MAX_CLASSNAME	64
#define DLG_MAX_FONT_NAME	32

// RTN F13: antes guardava CHAVE de titles.txt (curta, DLG_MAX_TITLE_KEY=64
// bastava). Agora guarda o TEXTO de verdade - 512 da folga confortavel
// (~2.4x a fala mais longa que ja existe hoje, a intro do Ivo) sem chegar
// perto do teto de mensagem de rede (MAX_USERMSG_LENGTH=2048, engine).
#define DLG_MAX_TEXT		512
// Rotulo de opcao/exit_option - texto curto (um botao de menu), nao precisa
// do mesmo tamanho do corpo da fala.
#define DLG_MAX_LABEL		128

// Tamanho do historico por jogador (server/player.h) de arvores seladas e de
// nos que ja deram "give" - poucos NPCs-chave por design (ver CLAUDE.md),
// nao precisa de mais que isso.
#define MAX_DIALOG_SEALED	8
#define MAX_DIALOG_GIVEN	16

// RTN F13: estilo visual (fonte/cor) de um node - tudo "vazio"/zerado
// significa "sem override, usa o padrao do cliente ou o que a arvore ja
// tinha acumulado" (ver DialogScript_ParseFile, resolve a heranca em tempo
// de parse: cada node nasce com uma COPIA do estilo corrente da arvore,
// depois aplica so os campos que ELE MESMO declarar por cima).
typedef struct dialogstyle_s
{
	char	font[DLG_MAX_FONT_NAME];	// vazio = fonte padrao do cliente (roboto_small)
	int	fontsize;			// 0 = tamanho padrao do cliente (14)
	int	color_speaker[3];		// -1 -1 -1 = cor padrao do cliente (amarelo de acento)
	int	color_text[3];			// -1 -1 -1 = cor padrao do cliente (branco)
} dialogstyle_t;

typedef struct dialogoption_s
{
	char	label[DLG_MAX_LABEL];		// texto do botao, ja pronto pra tela
	char	target[DLG_MAX_NAME];		// nome de outro node (deste ou de outro arquivo), ou "END"
} dialogoption_t;

typedef struct dialognode_s
{
	char	name[DLG_MAX_NAME];

	char	npc_text[DLG_MAX_TEXT];		// obrigatorio - ja pronto pra tela (linha 1 = nome do falante, por convencao)
	char	give_classname[DLG_MAX_CLASSNAME];	// opcional - vazio = sem give
	char	exit_option_text[DLG_MAX_LABEL];	// opcional - vazio = sem exit_option

	// RTN: so tem efeito relevante no PRIMEIRO node de cada arquivo (o que
	// "dialog_target" aponta) - igual sealed_text, sao propriedades da
	// ARVORE, nao de cada node, mas moram aqui por simplicidade (o parser
	// so preenche de verdade no primeiro node de cada arquivo; replicar
	// pra TODOS os nodes de uma arvore seria gasto de memoria sem uso).
	bool	repeatable;
	char	sealed_text[DLG_MAX_TEXT];	// opcional, so com repeatable == false, so no 1o node do arquivo

	dialogstyle_t	style;

	int	num_options;
	dialogoption_t	options[DLG_MAX_OPTIONS];
} dialognode_t;

extern dialognode_t	gDialogNodes[DLG_MAX_NODES];
extern int		gNumDialogNodes;

// Varre scripts/dialogs/ procurando dialog_*.txt e carrega TODOS os que
// achar (chamado de GameDLLInit(), igual WeaponScript_Init() -
// server/game.cpp). Nao recarrega por mapa. Nao precisa editar este
// arquivo nem listar nome em lugar nenhum pra adicionar um NPC novo.
void DialogScript_Init( void );

// Busca um no pelo nome (case-insensitive), em QUALQUER arquivo ja
// carregado - nomes de node precisam ser unicos entre todos os arquivos
// (mesmo espaco de nomes global, igual dialogs.txt unico de antes). NULL
// se nao existir - quem chama decide se isso e erro silencioso ou
// barulhento (ver DialogSession em player.cpp, que sempre loga no console
// do servidor).
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
