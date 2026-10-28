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

**RTN F13 (atual):** reescrita completa do formato. Até a versão
anterior deste guia, a estrutura (`dialogs.txt`) e o texto (chaves de
`titles.txt`) viviam em dois arquivos separados. Agora **um único
arquivo por NPC** (`dialog_<npc>.txt`) guarda estrutura, texto e estilo
juntos — `titles.txt` voltou a ser só para mensagens de sistema
(`env_message`/system tip/pickup), nunca mais para diálogo.

## Onde fica cada coisa

| O quê | Onde |
|---|---|
| Estrutura + texto + estilo de cada árvore | `game_dir/scripts/dialogs/dialog_<npc>.txt` (um arquivo por NPC) |
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

O valor é o nome do **nó inicial** da árvore (ver abaixo). Ao apertar
`+USE` nesse NPC (vivo), a conversa abre; se o NPC estiver morto, o
`+USE` cai no comportamento normal da classe (nada acontece de
especial).

**Importante:** `monster_human_grunt` (hgrunt) também funciona — ele
precisou de um `ObjectCaps()` próprio pra reagir ao `+USE`/mostrar a
mãozinha (ver `server/monsters/hgrunt.h`), já que `CSquadMonster` não
declara isso por padrão. Se um novo tipo de monstro não mostrar a
mãozinha mesmo com `dialog_target` setado, é esse o primeiro lugar a
checar.

## Como os arquivos são carregados

Todo arquivo em `game_dir/scripts/dialogs/` cujo nome comece com
`dialog_` e termine em `.txt` é carregado automaticamente
(`DialogScript_Init()`, chamado uma vez de `GameDLLInit()` — não
recarrega por mapa), por **varredura de diretório** — mesmo padrão de
`weapon_*.txt` em `weaponscript.cpp`. Não precisa editar nenhum código
nem listar nome de arquivo em lugar nenhum pra adicionar um NPC novo:
basta criar `dialog_<npc>.txt` seguindo a convenção.

Nomes de **nó** (não de arquivo) precisam ser únicos entre **todos** os
arquivos carregados — é o mesmo espaço de nomes global de antes, só que
agora espalhado por vários arquivos em vez de um só `dialogs.txt`. Um
`option`/`exit_option` de qualquer árvore pode levar a um nó de **outro**
arquivo, se precisar.

## Formato de `dialog_<npc>.txt`

```
dialog_ivo                     // label do arquivo - so documentacao,
{                               // "dialog_target" aponta pro NOME DO NODE,
                                // nao pra este label (ver node abaixo)
    repeatable      0           // vale pra ARVORE INTEIRA (nao por node)
    sealed                      // opcional - mostrado se repeatable 0 e
    {                           // a arvore ja foi selada
        Ivo:
        Tarde demais...
    }

    font            roboto_small // opcional - padrao do cliente se omitido
    fontsize        22          // opcional
    color_speaker   255 210 64  // opcional - RGB 0-255
    color_text      255 255 255 // opcional - RGB 0-255

    node dialog_ivo              // "dialog_target" "dialog_ivo" no editor
    {
        npc_text
        {
            Ivo:
            Você... você não devia ter vindo. Ninguém devia.
        }
        option 1 "Quem é você?" -> dialog_ivo_a
    }

    node dialog_ivo_a
    {
        give "item_antidote"    // opcional - dispara ao ENTRAR no node
        npc_text
        {
            Ivo:
            ...
        }
        option 1 "..." -> END
        exit_option "Vou embora."   // opcional
    }
}
```

**Armadilha de `font`:** use sempre `roboto_small` (bake dedicado a 24px) pra
diálogo, nunca `roboto` (bake de TÍTULO, 63px). `fontsize` é um tamanho
nominal fixo (define só o `lineGap` entre linhas), mas o glifo em si é
minificado a partir do tamanho de bake da fonte escolhida — pedir
`fontsize 22` num atlas de 63px gera texto visivelmente menor que o
esperado (minificação ~2.9x) com o MESMO espaçamento entre linhas de
sempre, dando a impressão de "espaçamento gigante". Esse é o mesmo motivo
pelo qual `roboto_small` existe (ver comentário `RTN F11 fix` em
`client/hud_dialog.cpp`) — já aconteceu de `dialog_medico.txt`/
`dialog_teste.txt`/`dialog_ivo.txt` herdarem `font roboto` por engano ao
copiar a diretiva antiga do `titles.txt` pré-F13.

Regras do formato:

- A **primeira linha não-vazia** do arquivo é o rótulo da árvore — pura
  documentação/cabeçalho, não precisa bater com nenhum nome de nó (e não
  é o que `dialog_target` referencia).
- `repeatable`, `sealed { }`, `font`, `fontsize`, `color_speaker` e
  `color_text` declarados **no topo do arquivo** (antes do primeiro
  `node`) são propriedades da **árvore inteira**:
  - `repeatable 0` (default): depois que o jogador chega num `END`
    escolhendo uma opção, a árvore fica **selada** — o próximo `+USE` só
    mostra o texto de `sealed` (se houver, no mesmo painel 2D do resto
    do diálogo, por alguns segundos, sem opções), sem reabrir a
    conversa. Sair pelo `exit_option` **não** sela (o jogador pode
    tentar de novo).
  - `font`/`fontsize`/`color_speaker`/`color_text` valem pra toda a
    árvore quando declarados no topo, mas **qualquer `node` pode
    sobrescrever só pra si** repetindo a mesma diretiva dentro do seu
    próprio bloco (ex.: uma fala específica em fonte maior). Sem
    `color_speaker`/`color_text`, o cliente usa a cor padrão (amarelo de
    acento pro falante, branco pro texto); sem `font`/`fontsize`, usa a
    fonte/tamanho padrão do painel de diálogo.
- Dentro de um bloco `{ }` de `npc_text`/`sealed`, **toda linha é texto
  cru** — `//` **nunca** é comentário ali dentro (só nas linhas de
  estrutura, fora de `{ }`), mesma regra que o parser nativo do engine
  já usa pro próprio `titles.txt`. Por convenção (não obrigatório pelo
  parser), a **primeira linha** do bloco é o nome do falante e o resto é
  a fala — é como o painel separa "nome em destaque" do "corpo do
  texto".
- `option <N> "<rótulo>" -> <nó_alvo | END>` — `N` de 1 a 8. O rótulo já
  é o texto pronto do botão (não é mais chave de `titles.txt`). O que
  vem depois da seta é o nome de outro nó (deste arquivo ou de
  qualquer outro já carregado), ou a palavra reservada `END`.
- `exit_option "<rótulo>"` sempre ocupa a última tecla do menu daquele
  nó e fecha a conversa sem executar nenhum `give` pendente.
- `give "<classname>"` é a **única** diretiva que não é texto de tela —
  é um classname de item (`item_antidote`, `weapon_9mmhandgun`...),
  repassado direto pra `CBasePlayer::GiveNamedItem()`. Dispara ao
  **entrar** no nó (inclusive o nó inicial), uma única vez por save —
  mesmo se a árvore for `repeatable 1`, o item não é duplicado ao
  reabrir.
- **Reuso de texto não existe** — cada `npc_text`/`sealed` é escrito por
  extenso no seu próprio bloco, mesmo que repita algo de outro nó. É
  intencional (mantém o parser simples, sem variável/snippet).

O servidor avisa no console (`DialogScript: ...`) se algum `option`/
`exit_option` aponta pra um destino inexistente, ou se um nó ficou sem
nenhuma saída (nem `option` nem `exit_option` — o jogador travaria).

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
mouse nem a W/A/S/D nesta branch — é sempre número.

## Protocolo de rede (`gmsgDialogShow`)

Texto e estilo chegam prontos do servidor — o cliente nunca lê nenhum
arquivo nem decide qual é o próximo nó. A mensagem tem um byte de
`msgType` no início:

| `msgType` | Significado | Conteúdo extra |
|---|---|---|
| `0` (CLOSE) | Fecha/esconde o painel | nenhum |
| `1` (NODE) | Mostra um nó com opções | turno (anti-replay), `npc_text`, lista de rótulos, estilo |
| `2` (SEALED) | Mostra o texto de `sealed`, sem opções, some sozinho após alguns segundos | `sealed_text`, estilo |

Ver o comentário no topo de `server/dialogsession.cpp` pro formato exato
byte a byte, e `client/hud_dialog.cpp` (`ReadStyle`) pro lado que lê.

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
- Até `DLG_MAX_NODES` (192) nós no total, somando todos os arquivos
  `dialog_*.txt` carregados.
