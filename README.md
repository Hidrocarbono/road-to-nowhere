# PrimeXT (Hidrocarbono Fork) <img align="right" width="128" height="128" src="https://github.com/SNMetamorph/PrimeXT/raw/master/primext/icon.png" alt="PrimeXT icon" />

[![Discord](https://img.shields.io/discord/824538989616824350)](https://discord.gg/BxQUMUescJ)
![GitHub Workflow Status (branch)](https://img.shields.io/github/actions/workflow/status/Hidrocarbono/PrimeXT/nightly-builds.yml?branch=master)
![GitHub release (by tag)](https://img.shields.io/github/downloads/Hidrocarbono/PrimeXT/total)
![GitHub repo size](https://img.shields.io/github/repo-size/Hidrocarbono/PrimeXT)
![GitHub commit activity](https://img.shields.io/github/commit-activity/m/Hidrocarbono/PrimeXT)

[![ModDB Rating](https://button.moddb.com/popularity/medium/mods/56077.png)](https://www.moddb.com/mods/primext)

**Modernized toolkit for Xash3D FWGS engine**, with extended physics, improved graphics and a lot of other new features for mod-makers. Based on XashXT and Spirit Of Half-Life and includes features and entities from it.

---

## 🚀 Fork Features (adicional)

Este fork adiciona funcionalidades personalizadas sobre a base do PrimeXT, mantendo total compatibilidade com o ecossistema GoldSrc/Xash3D:

- **Sistema de armas via script** — adicione novas armas apenas colocando modelos em `models/` e criando arquivos de script em `scripts/weapons/`, sem recompilar o código-fonte. Inspirado no sistema do Uncle Mike para Paranoia 2.
- **Sistema de legendas personalizadas** — exiba textos formatados na tela com fontes `.ttf`, cores personalizadas (título e texto individualmente) e substituição automática de `%player_name%` pelo nome do jogador. Acionado por entidades no mapa (`env_subtitle`), com suporte a arquivo central `messages.txt` e herança de defaults.
- **Sistema de Lean (inclinação)** — mecânica de inclinação do corpo para esquerda/direita (teclas `Q` e `E`), com detecção de colisão com paredes, animação suave e ajuste de hitbox.
- **Integração com o Hermes Agent** — o assistente de IA do projeto é usado para gerenciar código, documentação e builds via GitHub Actions.

---

## Features, brief overview (original + fork)
- HDR rendering support
- Parallax-corrected cubemaps
- Physically based rendering support (in progress)
- Dynamic lighting with shadow mapping (omnidirectional, cascaded, etc.)
- Normal mapping, parallax mapping support
- Advanced post-processing: bloom, depth-of-field, color correction, SSAO
- PhysX engine integration
- Eliminated many of limits that were presented in GoldSrc and vanilla Xash3D
- **Weapon script system** (dynamic loading from external `.txt` files)
- **Custom subtitle system** (formatted text with TTF fonts, colors, and entity-driven activation)
- **Lean system** (Q/E leaning with collision detection and smooth transition)

---

## Projects that are based on PrimeXT
- [Ionization](https://www.moddb.com/mods/ionization)
- [Half-Life: History of Kumertau](https://www.moddb.com/mods/half-life-history-of-kumertau)
- ["Zemlya Rodnaya" in Novy Urengoy](https://www.moddb.com/mods/school-2-in-novy-urengoy-recreated-on-xash3d)
- [Metro 2031: Last Chance](https://www.moddb.com/mods/metro-2031-last-chance)

*(e em breve, o seu próprio mod!)*

---

## Development goals
At this time, project in primal state: it somehow works, but there are a lot of things to fix or implement next. You can discuss with community members and ask questions in our [Discord](https://discord.gg/BxQUMUescJ) server.

We would be very grateful to potential contributors. Main development goals of this project is:
- Optimizing brushes rendering (clustered forward rendering, getting rid of legacy OpenGL code)
- Implementing lighting precomputation in HDR format
- Total reworking of material system
- Implementing particle engine, something like in Source Engine
- Improving physics further: ragdolls, vehicles, fine-tuning, etc.
- Improving cross-platform: developing Android port, supporting other architectures like ARM or RISC-V
- Writing actual documentation, translating existing pages to English
- Code refactoring (where it is really necessary, there is no goal to refactor everything)
- *(Fork-specific)* Completing and polishing the weapon script system, subtitle system, and lean system.
- *(Fork-specific)* Full integration with Hermes Agent for automated code reviews and CI/CD.

You can see the full list of project goals and a detailed description on the [documentation site](https://snmetamorph.github.io/PrimeXT/), but it is still a work in progress. 
So feel free to make suggestions on what should be documented first.

---

## Installation
You can read the detailed installation guide on our documentation site: available on [English](https://snmetamorph.github.io/PrimeXT/docs/eng/installation) and [Russian](https://snmetamorph.github.io/PrimeXT/docs/rus/installation) languages.

---

## Building
> NOTE: Never download sources from GitHub manually, because it doesn't include external depedencies, you SHOULD use Git clone instead.
1) Install [Git](https://git-scm.com/download/win) for cloning project
2) Clone this repository, enter these commands to Git console:
