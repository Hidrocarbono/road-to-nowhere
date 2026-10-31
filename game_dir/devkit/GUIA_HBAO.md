# HBAO — oclusão ambiental em espaço de tela

Escurece levemente cantos, frestas e a base de objetos, simulando a luz de
ambiente que não chega ali. **Desligado por padrão** (`gl_hbao 0`) até ser
calibrado em jogo. Não é sombra dinâmica: não projeta sombra de objeto e não
acende luz.

> **Status:** testado em jogo pelo usuário (build do run nº 215, commit `e406dcb`):
> os dois shaders compilam e o efeito funciona. Os valores padrão dos cvars
> (raio 32, intensidade 1.5, bias 0.1) ainda são estimativas — calibre em mapas
> internos e abertos antes de ligar por padrão (`gl_hbao 1`).

## Como ligar e ajustar

| Cvar | Padrão | O que faz |
|---|---|---|
| `gl_hbao` | `0` | 1 liga o efeito |
| `gl_hbao_radius` | `32` | alcance em unidades do mundo (a pessoa tem 72 de altura). Maior = sombras mais largas e mais caras |
| `gl_hbao_intensity` | `1.5` | força do escurecimento |
| `gl_hbao_bias` | `0.1` | folga contra o "ruído" em parede lisa. Se paredes planas aparecerem manchadas, suba (0.15–0.25); se cantos ficarem fracos, desça |
| `gl_hbao_debug` | `0` | 1 mostra só o AO em tons de cinza (não persiste entre sessões) |

Teste rápido: `gl_hbao 1` e depois `gl_hbao_debug 1`. Em debug, cantos e bases de
caixas devem aparecer escuros e superfícies abertas, brancas.

## Como funciona (resumo)

1. **Passe 1** (`glsl/postfx/genhbao_fp.glsl`, meia resolução): reconstrói a
   posição de cada pixel a partir do buffer de profundidade, estima a normal e
   marcha em 6 direções × 5 passos acompanhando o "horizonte". O que o horizonte
   sobe é a oclusão.
2. **Passe 2** (`glsl/postfx/drawhbao_fp.glsl`, resolução cheia): blur 4×4 que
   respeita profundidade (não borra por cima de quina, o que criaria halo) e
   multiplica na cena.

Código: `RenderHBAO()` em `client/render/gl_postprocess.cpp`, chamado de
`gl_backend.cpp` **antes** de `RenderDOF()`/bloom/tonemap — o AO multiplica a
cena HDR linear. Mesmo molde do DOF (`RequestScreenColor`/`RequestScreenDepth`).

## O que o efeito ignora de propósito

- **Céu** (profundidade bruta ≥ 0.9999) — nem recebe nem causa AO.
- **Viewmodel (arma na mão)** — usa o intervalo de profundidade `[0, 0.3]`
  (`R_DrawViewModel`, `gl_studio_draw.cpp`), então profundidade bruta `< 0.3` é
  tratada como "não existe". Sem isso a arma criaria uma auréola escura em volta
  dela. Se o valor `0.3` mudar lá, mude `VIEWMODEL_DEPTH` nos dois shaders.
- **Pixels longe demais** — se o raio projetado na tela tiver menos de 3 px, o
  pixel é pulado (não vale o custo).

## Limitações conhecidas

- É **pós-processo sobre o buffer de profundidade**: objetos que não escrevem
  profundidade (partículas, sprites aditivos, vidro) também são escurecidos
  quando estão na frente de um canto.
- O AO baked do mapa (radiosidade do `pxrad`) já escurece cantos; o ganho do
  HBAO é maior em mapas compilados sem bounce.
- Com `gl_hdr 0` roda, mas a ordem em relação ao tonemap não se aplica.
- Custo: 1 passe de meia resolução (~30 leituras de profundidade/pixel) + 1 passe
  cheio (~32 leituras). Meça FPS antes de subir `gl_hbao_radius`.

## Onde mexer se algo der errado

| Sintoma | Causa provável |
|---|---|
| Console com erro de shader `postfx/genhbao` ou `drawhbao` | os shaders compilaram no teste do run nº 215; se aparecer, é uma regressão — mande o log |
| Manchas em paredes lisas | `gl_hbao_bias` baixo demais |
| Auréola escura em volta de objetos | o peso de profundidade em `drawhbao_fp.glsl` (`0.05 * zc + 2.0`) está frouxo; ou `gl_hbao_radius` grande demais |
| Granulado visível | o blur 4×4 não está cancelando o padrão; verifique se o AO é mesmo de meia resolução (`m_hbaoWidth/Height`) |
| Arma na mão escurecida | `VIEWMODEL_DEPTH` não bate com o depth range do viewmodel |
| Tudo preto/branco no debug | `gl_hbao_intensity` ou a reconstrução de FOV (`tan(fov/2)` em `V_RenderPostEffect`) |
