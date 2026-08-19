# Aviso de colaboração — múltiplos agentes neste repositório

A partir de 2026-08-19, este repositório passou a receber mudanças de **dois agentes distintos**, ambos operando com as credenciais da conta `Hidrocarbono`:

- **Hermes** (Hermes + DeepSeek Flash v4) — autor histórico da maior parte do código até a tag `v1.0.0` (commit `c56c243`).
- **Claude** (Anthropic) — passou a atuar neste repositório a partir desta data, a pedido do Hidrocarboneto.

## Estado no momento deste aviso

- `master` e a tag `v1.0.0` apontam para o mesmo commit (`c56c243`) — sem mudanças pendentes de nenhum dos dois lados.
- Nenhuma proteção de branch foi aplicada na `master` (decisão consciente — ver abaixo). Push direto continua funcionando normalmente.

## Por que este arquivo existe

Como os dois agentes usam a mesma credencial e podem rodar em momentos próximos, existe risco de:
- **Conflito de merge** se ambos editarem os mesmos arquivos ao mesmo tempo.
- **Conflito semântico** (sem erro de git, mas quebra de build) se um alterar algo que o outro depende — comum em headers/defines compartilhados no engine Xash3D.
- **Sobrescrita silenciosa** em caso de `push --force` sem antes dar `pull`.

Optamos por **não** ativar branch protection (`enforce_admins`) porque, como as duas frentes compartilham a mesma credencial admin, isso travaria o fluxo de trabalho atual em vez de só isolar riscos — decisão do Hidrocarboneto.

## Convenção pedida para reduzir atrito

- Sempre dar `git pull` (ou `git pull --rebase`) antes de commitar/pushar.
- Evitar `push --force` na `master` sem necessidade clara.
- Preferir commits pequenos e focados por arquivo/módulo.
- Se estiver no meio de uma mudança grande/arriscada, considere trabalhar em uma branch de feature antes de subir para a `master`.

Sem issues habilitadas neste repo — este arquivo é o canal combinado para deixar esse tipo de aviso registrado no histórico.
