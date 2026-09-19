# Diálogo com escolhas (RTN)

Sistema de diálogo ramificado para NPCs-chave (poucos no jogo, pra amarrar
lore): o NPC fala, o jogador escolhe entre respostas prontas (nunca digita
nada), o NPC responde de acordo, e a conversa pode ramificar de novo ou
terminar — podendo entregar um item no final.

Documentação técnica de referência (arquitetura completa de decisões):
esta é a versão resumida para quem escreve conteúdo. Para o design
completo (trade-offs, mensagens de rede, hooks server-side), ver os
comentários em `server/dialogscript.h`, `server/dialogsession.cpp` e
`server/basemonster.h`.

## Onde fica cada coisa

| O quê | Onde |
|---|---|
| Estrutura da árvore (nós, opções, `give`) | `game_dir/scripts/dialogs/dialogs.txt` |
| Texto exibido (fala do NPC, rótulo de cada opção) | `game_dir/titles.txt` |
| Parser (server-only) | `server/dialogscript.cpp` / `.h` |
| Sessão por jogador (estado, rede, freeze) | `server/dialogsession.cpp` |
| Hook no NPC (`+USE`) | `server/basemonster.h` / `server/monsters.cpp` (`CBaseMonster::Use`) |
| HUD do cliente | `client/hud_dialog.cpp` / `.h` |

## Como amarrar um NPC no editor

Qualquer entidade `monster_*` (não só cientista/barney — funciona em
qualquer monstro, já que o gancho está em `CBaseMonster`, não em
`CTalkMonster`) ganha a keyvalue:

```
dialog_target   dialog_medico
```

O valor é o nome do **nó inicial** da árvore em `dialogs.txt` — por
convenção, `dialog_<npc>`. Ao apertar `+USE` nesse NPC (vivo), a conversa
abre; se o NPC estiver morto, o `+USE` cai no comportamento normal da
classe (nada acontece de especial).

## Formato de `dialogs.txt`

```
<nome_do_no>
{
    repeatable    0 | 1          // so no NO INICIAL, default 0
    sealed_line   "<chave>"      // so no NO INICIAL, opcional
    npc_line      "<chave>"      // obrigatorio
    option <N>    "<chave>" -> <no_alvo | END>   // N = 1 a 8
    exit_option   "<chave>"      // opcional
    give          "<classname>"  // opcional
}
```

- **`repeatable 0`** (default): depois que o jogador chega num `END`
  escolhendo uma opção, a árvore fica **selada** — o próximo `+USE` só
  toca `sealed_line` (se houver), sem reabrir a conversa. Sair pelo
  `exit_option` **não** sela (o jogador pode tentar de novo).
- **`give`** dispara ao **entrar** no nó (inclusive o nó inicial), uma
  única vez por save — mesmo se a árvore for `repeatable 1`, o item não é
  duplicado ao reabrir.
- **`exit_option`** sempre ocupa a última tecla do menu daquele nó e
  fecha a conversa sem executar nenhum `give` pendente.
- Toda string entre aspas antes de uma seta `->` (em `npc_line`,
  `option`, `exit_option`, `sealed_line`) é **chave de `titles.txt`**,
  nunca o texto em si. O que vem depois da seta é o nome de outro nó
  deste mesmo arquivo, ou a palavra reservada `END`.
- `give` é a **única** exceção: é um classname de item
  (`item_antidote`, `weapon_9mmhandgun`...), repassado direto pra
  `CBasePlayer::GiveNamedItem()` — não tem nada a ver com `titles.txt`.

O arquivo inteiro (todas as árvores do jogo) é um único
`game_dir/scripts/dialogs/dialogs.txt`, carregado uma vez no início
(`DialogScript_Init()`, chamado de `GameDLLInit()` — não recarrega por
mapa). O servidor avisa no console (`DialogScript: ...`) se algum nó tem
`option`/`exit_option` apontando pra um destino inexistente, ou se ficou
sem nenhuma saída (nem `option` nem `exit_option` — o jogador travaria).

## Formato em `titles.txt`

`npc_line` usa a convenção normal de `titles.txt`: **linha 1** ($color) é
o **nome do falante** (o que aparece destacado no HUD), **linha 2+**
($color2) é a fala em si:

```
Medico_Intro
{
Medico
Vejo que sobreviveu ate aqui. Precisa de ajuda?
}
```

As entradas de `option`/`exit_option` (rótulo do botão) são só **uma
linha** — sem nome de falante:

```
Opcao_PerguntarVacina
{
Perguntar sobre a vacina
}
```

## Movimento e tiro travados

Durante a conversa o jogador fica travado (`CBasePlayer::EnableControl`,
o mesmo mecanismo nativo de `trigger_playerfreeze`) — não anda, não
atira. Por isso **todo** nó precisa ter uma saída (`option` levando a
`END`, ou `exit_option`) — sem isso o jogador fica preso. O parser avisa
no console quando detecta um nó sem saída, mas confirme visualmente no
jogo antes de publicar a árvore.

Se o NPC morrer no meio da conversa (por qualquer causa), a sessão fecha
sozinha e o jogador é destravado (`Dialog_NotifyNPCDied`, chamado de
`CBaseMonster::Killed`) — sem selar a árvore, já que a conversa não
terminou de verdade.

## Seleção da resposta

Teclas numéricas (1 a 9 — o mesmo sistema `slotN` que já troca de arma no
HL1), desviadas para o menu de diálogo enquanto ele estiver na tela
(`WeaponsResource::SelectSlot`, `client/ammo.cpp`). Não tem suporte a
mouse nem a W/A/S/D — é sempre número.

## Limites atuais (v1)

- Até 8 opções numeradas por nó, mais o `exit_option` (9 no total —
  teclas 1 a 9).
- Sem opções condicionais (toda `option` escrita sempre aparece — não há
  checagem de flag/quest nesta versão).
- Singleplayer only — a sessão assume sempre o jogador do slot 1
  (`INDEXENT(1)`), sem lock nem suporte a dois jogadores.
- Histórico por jogador limitado a `MAX_DIALOG_SEALED` (8) árvores
  seladas e `MAX_DIALOG_GIVEN` (16) itens já entregues — mais que
  suficiente pro punhado de NPCs-chave a que este sistema se propõe.
