# Manual de expressões regulares — Design

Nota sobre a tradução: as palavras e frases russas dos exemplos de controlo são mantidas intencionalmente. São dados de teste: as expressões regulares, os textos de substituição, os espaços e os resultados esperados coincidem com a edição russa de referência. As explicações foram traduzidas; os exemplos não são adaptados automaticamente às convenções tipográficas portuguesas.

Guia completo de pesquisa, substituição e revisão de livros no FictionBook Editor Next.

Edição: 2 de outubro de 2026. O modo Código é tratado no documento regex-source.md. Neste manual, «padrão» significa uma expressão regular; um «modelo incorporado» é um conjunto guardado de expressão e opções no painel Modelos.

## 1. Como utilizar o manual

Uma expressão regular descreve uma regra de pesquisa, em vez de uma única cadeia exata. Por exemplo, `[0-9]+` encontra uma sequência de algarismos de qualquer comprimento e `[ \t]{2,}` encontra dois ou mais espaços normais ou tabulações. A correspondência encontrada e o texto que a substitui são entidades diferentes.

A secção 2 permite obter um primeiro resultado prático. As secções 3–15 explicam a sintaxe; a secção 16 descreve a gramática específica de substituição do FBE. A secção 17 reúne receitas para livros, com condições, texto de teste e avisos. No fim encontram-se problemas comuns, desempenho e fontes.

Copie apenas a expressão para Procurar. As palavras «Procurar», «Substituir», as indicações U+0020 e os delimitadores Markdown não pertencem ao padrão. Não são necessários invólucros JavaScript `/.../g`, aspas de literais C++ nem barras invertidas duplicadas de JSON.

As opções fazem parte da receita. Uma correspondência obtida sem distinguir maiúsculas pode mudar quando a distinção é ativada. Não trate as caixas de verificação como simples apresentação.

«Apenas pesquisa e verificação» indica um diagnóstico. O resultado pode ser correto: repetição intencional, nome estrangeiro, citação, título ou escolha tipográfica. «Substituição após verificação» também não garante segurança para qualquer livro.

## 2. Primeira pesquisa e substituição segura

Guarde uma cópia de trabalho. Mude para Design, abra Procurar ou Substituir e ative Expressão regular. Nos primeiros ensaios, desative Apenas palavras completas: é preferível definir os limites no padrão. Confirme o âmbito e a direção da pesquisa.

Para encontrar vários espaços:

```regex
[ \t]{2,}
```

Em `Он   пришёл`, o resultado é o intervalo de três espaços. Coloque um espaço normal na substituição. Execute primeiro uma única substituição, confirme `Он пришёл` e só depois considere Substituir tudo.

Perante muitos resultados, examine-os primeiro e corrija um ou dois casos representativos. Depois de uma operação global, verifique texto, itálico, negrito, notas e limites de parágrafos. Se o resultado for inesperado, anule antes de começar outra série de alterações.

Aplicar, no painel de modelos, transfere a expressão e as opções para a janela de pesquisa. Não significa corrigir incondicionalmente todos os resultados. A pesquisa e a substituição são executadas pelos respetivos comandos da janela.

## 3. Motor e limites do modo Design

Este modo usa PCRE2-16, com texto e padrões em UTF-16; UTF está sempre ativo. O FBE constrói separadamente o texto de pesquisa e executa a substituição considerando a estrutura do livro. A documentação PCRE2 explica as correspondências, mas não determina todas as ações do FBE. A base técnica encontra-se em D1–D4.

A pesquisa incide na representação textual do livro, não na marcação XML literal. `<strong>` não permite procurar negrito em Design. As etiquetas XML procuram-se em Código; as transformações estruturais usam ferramentas do editor ou scripts especializados.

A mudança visual de linha de um texto comprido não é um carácter de fim de linha. Parágrafos, quebras reais e ajuste à largura são diferentes. O FBE usa âncoras multilinha na representação de pesquisa para refletir os limites de parágrafos. Encontrar uma correspondência entre parágrafos não equivale a poder substituí-la: a implementação considerada rejeita substituições entre parágrafos.

`\A` e `\z` referem-se ao início e ao fim da cadeia fornecida ao motor. Não representam automaticamente todo o FB2: dependem do âmbito e do fragmento preparado pelo FBE.

## 4. Unicode: UTF, UCP e maiúsculas

### 4.1. O que faz UTF

UTF permite tratar caracteres Unicode, incluindo cirílico. Não converte alfabetos, não corrige OCR, não transforma `ё` em `е` e não unifica automaticamente representações canónicas equivalentes.

Uma letra acentuada pode ser um carácter único ou uma letra seguida de um sinal combinante. O aspeto pode ser semelhante, mas a pesquisa carácter a carácter diferente. O PCRE2 não faz normalização NFC/NFD pelo utilizador. Em livros com diacríticos, considere `\p{M}`.

### 4.2. O que faz UCP

Unicode (UCP) altera sobretudo as classes abreviadas `\w`, `\d`, `\s` e os limites `\b` e `\B` que delas dependem. Não é o interruptor que permite o próprio cirílico.

Precisão relativamente a versões anteriores: numa compilação Unicode de PCRE2, as propriedades explícitas `\p{L}`, `\p{N}` e `\P{...}` estão disponíveis sem UCP. Porém, quando uma expressão combina `\p{L}` com `\b`, UCP torna coerentes a noção de letra e a de limite de palavra.

Compare a pesquisa de uma palavra russa:

```regex
\bмир\b
```

Com UCP, os limites seguem a classe de palavra Unicode. Sem UCP, não espere o mesmo comportamento de `\b` para cirílico e texto latino ASCII. A ausência de correspondência não prova a ausência da palavra.

Para exigir algarismos ASCII, prefira `[0-9]` a `\d`: UCP pode alargar esta última classe a algarismos decimais de outras escritas.

### 4.3. Propriedades úteis

| Expressão | O que procura |
| --- | --- |
| `\p{L}` | Uma letra Unicode |
| `\p{Lu}` | Uma maiúscula com pesquisa sensível à caixa |
| `\p{Ll}` | Uma minúscula com pesquisa sensível à caixa |
| `\p{M}` | Um sinal combinante |
| `\p{N}` | Um carácter numérico, conceito mais amplo do que algarismo decimal |
| `\p{Nd}` | Um algarismo decimal |
| `\p{Latin}` | Um carácter da escrita latina |
| `\p{Cyrillic}` | Um carácter da escrita cirílica |
| `\P{L}` | Um carácter que não é letra |

Sequência de letras e sinais combinantes:

```regex
[\p{L}\p{M}]+
```

Não é uma definição linguística universal de palavra: hífenes e apóstrofos não estão incluídos. Acrescente-os conscientemente.

### 4.4. A distinção de maiúsculas é uma opção separada

Ative Distinguir maiúsculas/minúsculas para anomalias como `строчнаяПрописная`, iniciais e padrões com `\p{Lu}`/`\p{Ll}`. Ignorar a caixa pode destruir o sentido da regra. UCP não substitui esta opção.

Ativação da pesquisa sem distinção dentro da expressão:

```regex
(?i)глава
```

Efeito limitado ao grupo:

```regex
(?i:глава)[ \t]+[0-9]+
```

## 5. Caracteres literais e escape

As letras e a maioria dos sinais representam-se a si próprios. Fora de uma classe, ponto, parênteses, asterisco, mais, interrogação, chavetas, âncoras e barra invertida podem ter significado especial.

Para procurar literalmente um metacarácter, coloque uma barra invertida antes dele:

| Texto pretendido | Expressão |
| --- | --- |
| Ponto | `\.` |
| Interrogação | `\?` |
| Sinal mais | `\+` |
| Asterisco | `\*` |
| Parêntese de abertura | `\(` |
| Parêntese de fecho | `\)` |
| Número entre parênteses retos | `\[[0-9]+\]` |
| A própria barra invertida | `\\` |

`(123)` procura `123` e captura-o num grupo; não exige parênteses no texto. Para encontrar `(123)`, os parênteses devem ser escapados.

Um fragmento literal comprido pode ser delimitado por `\Q` e `\E` no padrão PCRE2:

```regex
\QЦена (руб.) + доставка\E
```

No campo de substituição FBE, os mesmos símbolos significam outra coisa. Não transfira automaticamente as regras de escape da pesquisa para a substituição.

## 6. Espaços, tabulações e caracteres invisíveis

| Expressão | Significado na pesquisa PCRE2 |
| --- | --- |
| `[ ]` | Apenas espaço normal U+0020 |
| `[ \t]` | Espaço normal ou tabulação |
| `\t` | Tabulação U+0009 |
| `\x{00A0}` | Espaço inquebrável |
| `\x{202F}` | Espaço inquebrável estreito |
| `\h` | Espaço horizontal, incluindo vários espaços Unicode |
| `\s` | Carácter de espaço em branco, podendo incluir fim de linha |
| `\r` | Retorno de carro CR |
| `\n` | Avanço de linha LF |
| `\R` | Sequência Unicode de quebra de linha |
| `\x{00AD}` | Hífen opcional |
| `\x{200B}` | Espaço de largura zero |
| `\x{FEFF}` | FEFF no próprio texto |

Para limpar intervalos normais entre palavras, escolha `[ \t]`, não `\s`. Esta última classe é mais ampla e pode atingir limites de parágrafos e espaços inquebráveis. `\h` é útil para diagnóstico, mas demasiado amplo para uma substituição tipográfica sem condições claras.

NBSP mantém unidos sinal e número, iniciais e apelido, valor e unidade. Convertê-los todos em espaços normais pode prejudicar a composição. O FBE também tem uma definição do carácter NBSP; verifique os casos U+00A0 contra a configuração do livro e da compilação.

U+200C e U+200D podem ser necessários para certas escritas e emoji compostos. Não elimine indiscriminadamente todos os caracteres «invisíveis». Detetar não significa encontrar um erro.

## 7. Classes e intervalos de caracteres

Uma classe entre parênteses retos consome um carácter do conjunto. `[abc]` significa uma das três letras, não a palavra `abc`; `[abc]+` pede uma ou mais letras do conjunto.

`[^abc]` consome um carácter que não pertença ao conjunto. Não verifica «não existe abc antes do texto»: para contexto usam-se lookaround.

Dentro de uma classe, o ponto e muitos parênteses deixam de ser especiais. O hífen pode definir um intervalo; para o usar literalmente, coloque-o no início/fim ou escape-o. O fecho de parêntese reto pode escrever-se `\]`.

```regex
[А-Яа-яЁё-]+
```

O exemplo admite letras russas e hífen, não todo o cirílico. Letras ucranianas, bielorrussas e outras exigem um conjunto maior ou uma propriedade Unicode.

Dentro de uma classe `\b` não é um limite de palavra. Não coloque limites de palavra num conjunto normal de caracteres.

## 8. Âncoras, limites e correspondências vazias

Uma âncora verifica uma posição sem consumir letra ou espaço. `^`, `$` e `\b` podem produzir correspondências de comprimento zero, que não aparecem como um fragmento normalmente selecionado.

| Âncora | Significado |
| --- | --- |
| `^` | Início de linha com multiline; caso contrário, início do objeto pesquisado |
| `$` | Fim de linha com multiline; a quebra final depende do modo |
| `\A` | Apenas início do objeto pesquisado |
| `\z` | Fim exato do objeto |
| `\Z` | Fim do objeto ou posição antes da quebra de linha final |
| `\b` | Limite entre carácter de palavra e carácter que não é de palavra |
| `\B` | Posição que não é limite de palavra |
| `\G` | Posição inicial da chamada atual de correspondência |

`\G` não guarda por si a «correspondência anterior». Depende do deslocamento inicial fornecido pela aplicação, que pode ser o fim do resultado anterior nas chamadas seguintes. No FBE não é uma forma universal de percorrer o livro. Para revisão normal, prefira limites explícitos. [D1]

Para uma palavra inteira e não fragmentos de palavras maiores, use limites ou verificações das letras vizinhas. Com UCP:

```regex
\bтом\b
```

Não corresponde ao início de `томик`. Hífenes e apóstrofos podem, contudo, separar palavras de forma diferente para o motor e para o revisor.

Tenha especial cuidado com correspondências vazias em Substituir tudo: inserem texto em posições, em vez de trocar caracteres visíveis. Para começar, prefira resultados não vazios.

## 9. Quantificadores: repetições e retrocesso

| Notação | Repetições do elemento anterior |
| --- | --- |
| `?` | Zero ou uma |
| `*` | Zero ou mais |
| `+` | Uma ou mais |
| `{3}` | Exatamente três |
| `{3,}` | Pelo menos três |
| `{2,5}` | De duas a cinco |

O quantificador aplica-se ao carácter, classe ou grupo anterior. `аб+` repete apenas `б`; `(?:аб)+` repete o par.

### 9.1. Pesquisa gananciosa

Texto inicial:

```text
«первый» и «второй»
```

Padrão:

```regex
«.*»
```

O ponto com asterisco toma primeiro o maior fragmento possível. Se necessário, devolve caracteres para satisfazer o resto do padrão. Aqui o resultado abrange os dois pares de aspas.

### 9.2. Pesquisa não gananciosa

```regex
«.*?»
```

Começa pelo menor número de caracteres, mas pode alargar a correspondência. A pesquisa sucessiva encontra `«первый»`, depois `«второй»`.

Para um par simples, costuma ser mais claro limitar explicitamente o conteúdo:

```regex
«[^»\r\n]*»
```

Não analisa aspas aninhadas. Citações dentro de citações exigem revisão separada.

### 9.3. Quantificador possessivo

```regex
«.*+»
```

Não significa «aspas ainda mais corretas». `.*+` consome também o fecho e não o devolve. O `»` restante no padrão já não pode corresponder; no exemplo não há resultado.

Pode ser apropriado excluir o fecho do conteúdo:

```regex
«[^»\r\n]*+»
```

Quantificadores possessivos e grupos atómicos controlam o retrocesso; não aceleram universalmente qualquer expressão.

## 10. Grupos e referências anteriores

Os parênteses capturam o fragmento encontrado. A numeração começa em 1 e segue as aberturas dos grupos de captura da esquerda para a direita, mesmo com aninhamento.

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Em `02.10.2026`, os grupos contêm `02`, `10` e `2026`. O padrão verifica a forma, não a validade de calendário.

`(?:...)` agrupa sem consumir um número. É útil para manter `$1` e `$2` previsíveis em substituições complexas.

Um grupo com nome facilita a leitura:

```regex
(?<word>\p{L}+)[ \t]+\k<word>
```

Uma referência anterior exige o mesmo texto capturado. Uma chamada de sub-rotina, explicada adiante, repete a regra e pode encontrar texto diferente. Não são mecanismos equivalentes.

Para palavras repetidas numa obra real, acrescente limites, configuração de maiúsculas e um conjunto adequado de espaços. Sem limites, a referência pode encontrar partes de palavras maiores. Há uma receita completa abaixo.

Grupos com nome na pesquisa não criam automaticamente substituições com nome no FBE. Use referências numéricas confirmadas no campo de substituição.

## 11. Alternância e grupos atómicos

A barra vertical escolhe entre alternativas:

```regex
(?:глава|часть)[ \t]+[0-9]+
```

Agrupar é importante: sem o grupo, a parte direita comum pode aplicar-se apenas à última alternativa. As alternativas são tentadas pela ordem escrita; ordene formas longas e curtas deliberadamente.

Um grupo atómico impede voltar ao interior de um grupo já concluído:

```regex
(?>а|аб)в
```

Em `абв`, a primeira alternativa escolhe `а` e não pode voltar a `аб`; não há correspondência. A variante não atómica poderia encontrá-la. É um exemplo didático, não uma recomendação de acrescentar atomicidade sem teste.

## 12. Verificações de contexto: lookaround

Lookaround verifica texto vizinho que não entra na correspondência completa. Assim pode selecionar apenas um número, preservando o sinal anterior:

```regex
(?<=№ )[0-9]+
```

Em `№ 125`, encontra apenas `125`. O espaço é normal e único.

Condição à direita:

```regex
[0-9]+(?=[ \t]+руб\.)
```

Em `125 руб.`, encontra o número sem `руб.`.

Condição negativa à frente:

```regex
\bглава\b(?![ \t]+[0-9])
```

Com UCP procura «глава» sem a numeração normal separada por espaço. É um exemplo de seleção contextual, não uma regra de correção.

`(?<=...)` verifica atrás e `(?<!...)` nega essa condição; `(?=...)` e `(?!...)` verificam ou negam o texto à frente.

Lookbehind tem limites de comprimento. O PCRE2 moderno permite algumas extensões variáveis limitadas, não repetições arbitrárias ilimitadas. Para receitas portáveis, use contexto curto fixo, captura ou `\K`; não suponha que `.*` funciona dentro de lookbehind.

## 13. Opções dentro da expressão

| Opção | Efeito |
| --- | --- |
| `(?i)` | Ignora maiúsculas/minúsculas |
| `(?-i)` | Distingue maiúsculas/minúsculas |
| `(?m)` | Âncoras de início/fim multilinha |
| `(?-m)` | Desativa âncoras multilinha |
| `(?s)` | Permite que o ponto corresponda a fim de linha |
| `(?-s)` | Restaura o comportamento normal do ponto |
| `(?x)` | Ignora espaços não significativos e comentários no padrão |

`(?m)` não faz o ponto atravessar linhas. `(?s)` não autoriza o FBE a substituir entre parágrafos. São duas opções de pesquisa e uma restrição separada da aplicação.

No modo alargado, os espaços no padrão podem deixar de ser literais. Use `[ ]` para um espaço obrigatório; `#` fora de uma classe pode iniciar um comentário. Padrões multilinha são legíveis na documentação, mas as receitas para o campo de uma linha são dadas numa única linha.

## 14. Funcionalidades avançadas de PCRE2

A maioria das correções não precisa desta secção. Ela explica construções encontradas em padrões de terceiros. O suporte do motor não torna qualquer padrão cómodo ou seguro para um livro inteiro.

### 14.1. Reiniciar o início da correspondência

```regex
№[ \t]*\K[0-9]+
```

Em `№ 125` verifica o prefixo, mas devolve só `125`. É útil para modificar o número e não o sinal. Não use `\K` dentro de lookaround sem verificar as restrições dessa combinação.

### 14.2. Condição de participação de um grupo

```regex
^(\()?([0-9]+)(?(1)\))$
```

Admite `12` ou `(12)`, mas não `(12` sem fecho. A condição testa a participação do primeiro grupo. Não converta automaticamente o exemplo numa substituição com grupos opcionais sem testar o adaptador FBE.

### 14.3. Numeração partilhada entre alternativas

```regex
(?|глава ([0-9]+)|часть ([0-9]+))
```

O número fica no grupo 1 em ambos os ramos: é branch reset. Em casos simples, um grupo sem captura e uma captura comum são geralmente mais claros.

### 14.4. Chamada de sub-rotina

```regex
(?<pair>[0-9]{2})-(?&pair)
```

Em `12-34`, ambas as partes cumprem a regra «dois algarismos», embora sejam diferentes. Uma referência `\k<pair>` exigiria repetir `12`.

Sub-rotinas recursivas descrevem algumas estruturas aninhadas, mas não substituem o parser XML FBE. Para `<section>`, `<poem>`, notas e tabelas use ferramentas estruturais.

### 14.5. Ignorar fragmentos

```regex
«[^»\r\n]*»(*SKIP)(*FAIL)|\bслово\b
```

Com UCP, a palavra é procurada fora de pares simples de aspas russas. O primeiro ramo marca a citação a ignorar; o segundo procura a palavra. Não analisa aspas aninhadas nem por fechar. Não confie apenas nele para uma revisão global.

## 15. O que uma expressão regular não decide

«Maiúscula depois de minúscula», «falta de pontuação final» e «palavra repetida» são indícios formais, não decisões editoriais. Regex não sabe se `да да` é uma gralha, fala intencional, verso ou título.

Não acrescente pontos automaticamente, não una todos os parágrafos com inicial minúscula, não troque todas as letras latinas por cirílicas nem todos os hífenes por travessões. É necessário contexto e, por vezes, vários passos no DOM. Os scripts FBE de limpeza, palavras coladas e notas tratam precisamente tarefas mais complexas.

## 16. Gramática de substituição FBE

O PCRE2 pesquisa, mas o FBE interpreta a substituição. Não transfira exemplos de JavaScript, Python, .NET, PCRE2 substitute ou outro editor sem testar. Os comandos seguintes estão confirmados pelo código e pela documentação original FBE. [D3, D4]

### 16.1. Correspondência e grupos

| No campo de substituição | Significado |
| --- | --- |
| `$0` ou `\0` | Correspondência completa |
| `$1` … `$9` | Grupo com o número correspondente |
| `\1` … `\9` | Notação alternativa do grupo numérico |
| `$+` ou `\+` | Último grupo devolvido pelo adaptador |

«O FBE substitui apenas grupos 1–9» é impreciso: também se pode inserir texto normal e a correspondência inteira. O limite é de acesso numérico direto, não do número de grupos PCRE2.

Não use `$10` para referir o décimo grupo. `${name}` não pertence à gramática confirmada. Mantenha as capturas necessárias nos primeiros nove grupos e faça os grupos auxiliares sem captura.

Grupos opcionais e vazios exigem uma prova separada: o FBE tem o seu próprio SubMatches. Para novas substituições globais, evite depender dos pormenores de grupos omitidos ou do «último grupo»; use capturas explícitas obrigatórias.

### 16.2. Reordenar grupos

Procurar:

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Substituir por:

```text
$3-$2-$1
```

`02.10.2026` passa a `2026-10-02`. Demonstra reordenação, não recomenda mudar todas as datas do livro.

### 16.3. Alterar maiúsculas/minúsculas

| Comando | Efeito |
| --- | --- |
| `\U` | Ativa maiúsculas para o fragmento inserido |
| `\L` | Ativa minúsculas |
| `\T` | Primeira letra do fragmento maiúscula e restantes minúsculas |
| `\Q` | Repõe os comandos de caixa e formatação para o texto seguinte |

Procurar:

```regex
(иван)
```

Substituir por:

```text
\T$1\Q
```

Resultado de controlo: `Иван`. `\T` não é uma capitalização linguística avançada de cada palavra: `иван иванов` num único fragmento não tem de se tornar `Иван Иванов`.

Não sobreponha `\U` e `\L` sem reposição. Separe partes com `\Q` e verifique cirílico e diacríticos. Quem altera a caixa é o FBE, não um normalizador Unicode PCRE2.

### 16.4. Negrito e itálico

`\S` ativa negrito no fragmento inserido, `\E` itálico. `\Q` termina os comandos ativos para o texto que se segue.

```text
\S$1\Q
```

Formata o conteúdo do grupo 1. Não procura texto já em negrito nem apaga toda a formatação existente. Teste um fragmento e a anulação primeiro.

### 16.5. Símbolos iguais com sentidos diferentes

Na pesquisa, `\S` é um carácter que não é espaço em branco; na substituição FBE é negrito. `\Q...\E` na pesquisa protege texto literal; na substituição `\Q` repõe comandos e `\E` ativa itálico.

Não introduza `\n`, `\t`, `\x{00A0}`, `\$` ou `$$` na substituição Design, esperando o comportamento de outro editor. Não fazem parte da gramática de literais aqui descrita; uma sequência desconhecida pode ser eliminada. Para NBSP use o carácter real; para preservar um sinal de controlo, use uma captura encontrada ou uma substituição não regex após verificação.

Campo vazio elimina a correspondência; campo com um espaço substitui por um espaço. Não escreva literalmente as etiquetas `<пусто>`, `[NBSP]` ou `U+00A0` usadas nas explicações.

## 17. Receitas práticas de edição e revisão

Os exemplos são cenários independentes, não uma promessa de correspondência exata com os nomes do catálogo. Aplique primeiro uma substituição a um único resultado. Os espaços e tabulações reais dos exemplos foram conservados; os contraexemplos mostram pelo menos um limite, mas não dispensam a verificação do livro.

### D01. Vários espaços normais passam a um

Compactar intervalos normais sem alterar um NBSP isolado.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[ \t]{2,}
```

Texto de teste inicial:

```text
Он   пришёл.
```

Substituir por: um único espaço normal U+0020. É um carácter, não a palavra «espaço».

Resultado da substituição:

```text
Он пришёл.
```

Contraexemplo que não deve ser encontrado:

```text
Он пришёл.
```


Limitações e observações: Em poesia, tabelas e simulações de recuo, vários espaços podem ser intencionais. Verifique o âmbito; não é uma forma de reconstruir recuos de parágrafos.

### D02. Espaços no início do parágrafo

Remover o recuo manual antes de texto normal.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
^[ \t]+
```

Texto de teste inicial:

```text
   Начало абзаца.
```

O mesmo texto de teste com os caracteres invisíveis assinalados:

```text
␠␠␠Начало␠абзаца.
```

Aqui, ␠ representa um espaço normal, [TAB] uma tabulação, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. Esta é uma representação explicativa; as etiquetas não devem ser inseridas no livro.

Substituir por: deixar o campo completamente vazio. Não escrever a palavra «vazio».

Resultado da substituição:

```text
Начало абзаца.
```

Contraexemplo que não deve ser encontrado:

```text
Начало абзаца.
```


Limitações e observações: Recuos manuais podem ser significativos em poesia e composição artística. A âncora refere-se à linha textual, não ao ajuste visual à largura.

### D03. Espaços no fim do parágrafo

Remover espaços normais ou tabulações finais.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[ \t]+$
```

Texto de teste inicial:

```text
Конец абзаца.
```

O mesmo texto de teste com os caracteres invisíveis assinalados:

```text
Конец␠абзаца.␠␠␠
```

Aqui, ␠ representa um espaço normal, [TAB] uma tabulação, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. Esta é uma representação explicativa; as etiquetas não devem ser inseridas no livro.

Substituir por: deixar o campo completamente vazio. Não escrever a palavra «vazio».

Resultado da substituição:

```text
Конец абзаца.
```

Contraexemplo que não deve ser encontrado:

```text
Конец абзаца.
```


Limitações e observações: NBSP foi excluído intencionalmente. Para espaços não convencionais, use diagnóstico separado.

### D04. Espaço antes de vírgula e outros sinais

Preservar o sinal e eliminar o intervalo anterior.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[ \t]+([,;:!?])
```

Texto de teste inicial:

```text
Слово , другое !
```

Substituir por:

```text
$1
```

Resultado da substituição:

```text
Слово, другое!
```

Contraexemplo que não deve ser encontrado:

```text
Слово, другое!
```


Limitações e observações: É uma regra para texto russo normal. A tipografia francesa admite espaços especiais antes de alguns sinais; não a aplique a esses livros sem adaptação.

### D05. Espaço depois de um sinal de abertura

Remover o espaço normal depois de parêntese ou aspa russa de abertura.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
([(\[«„])[ \t]+
```

Texto de teste inicial:

```text
« слово» ( пример)
```

Substituir por:

```text
$1
```

Resultado da substituição:

```text
«слово» (пример)
```

Contraexemplo que não deve ser encontrado:

```text
«слово» (пример)
```


Limitações e observações: Não altera espaços em fórmulas ou exemplos que não sigam imediatamente um dos sinais enumerados. O contexto continua a exigir análise.

### D06. Espaço antes de um sinal de fecho

Remover o espaço normal antes de parêntese ou aspa de fecho.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[ \t]+([)\]»”])
```

Texto de teste inicial:

```text
«слово » (пример )
```

Substituir por:

```text
$1
```

Resultado da substituição:

```text
«слово» (пример)
```

Contraexemplo que não deve ser encontrado:

```text
«слово» (пример)
```


Limitações e observações: Não normaliza todos os estilos possíveis de aspas e parênteses matemáticos.

### D07. Exatamente três pontos passam a reticências

Converter três pontos consecutivos sem tocar em sequências mais longas.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?<!\.)\.{3}(?!\.)
```

Texto de teste inicial:

```text
Он подумал... и ответил.
```

Substituir por:

```text
…
```

Resultado da substituição:

```text
Он подумал… и ответил.
```

Contraexemplo que não deve ser encontrado:

```text
Содержание.....12
```


Limitações e observações: Confirme a política editorial. Quatro pontos e pontilhados de índices ficam intencionalmente intactos.

### D08. Reticências com espaços

Juntar três pontos separados por espaços normais.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?<!\.)\.[ \t]*\.[ \t]*\.(?!\.)
```

Texto de teste inicial:

```text
Он подумал. . . и ответил.
```

Substituir por:

```text
…
```

Resultado da substituição:

```text
Он подумал… и ответил.
```

Contraexemplo que não deve ser encontrado:

```text
Слово. Другое.
```


Limitações e observações: Também encontra três pontos compactos. Não use em pontilhados ou omissões de citações sujeitos a regras próprias.

### D09. Espaço inquebrável depois de №

Ligar o sinal de número ao valor seguinte.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
№[ \t]+([0-9]+)
```

Texto de teste inicial:

```text
№ 125
```

Substituir por:

```text
№ $1
```

Resultado da substituição:

```text
№ 125
```

Contraexemplo que não deve ser encontrado:

```text
№125
```


Limitações e observações: Entre № e $1 na substituição existe um U+00A0 real. Não o troque pela sequência escrita \x{00A0}. O padrão não acrescenta um espaço inexistente.

### D10. Espaço inquebrável depois de §

Ligar o sinal de parágrafo ao número.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
§[ \t]+([0-9]+)
```

Texto de teste inicial:

```text
§ 12
```

Substituir por:

```text
§ $1
```

Resultado da substituição:

```text
§ 12
```

Contraexemplo que não deve ser encontrado:

```text
§12
```


Limitações e observações: Entre § e $1 existe um NBSP real. Para numerações complexas, confirme que foi encontrado o fragmento pretendido.

### D11. Possível repetição de palavra adjacente

Encontrar repetição através de espaços horizontais, sem atravessar parágrafos.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — desativada.

Procurar:

```regex
\b(\p{L}+)[ \t\x{00A0}]+\1\b
```

Texto de teste inicial:

```text
Это это уже было.
```

Correspondência esperada:

```text
Это это
```

Contraexemplo que não deve ser encontrado:

```text
Это уже было.
```


Limitações e observações: Repetições como «Да да» podem ser do autor. Não elimine a segunda palavra automaticamente: pode ser necessária uma vírgula ou nenhuma alteração. Apóstrofos e hífenes não estão totalmente tratados.

### D12. Latino e cirílico na mesma palavra

Detetar mistura OCR numa palavra contínua, não qualquer linha bilingue.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?<![\p{L}\p{M}])(?=[\p{L}\p{M}]*\p{Latin})(?=[\p{L}\p{M}]*\p{Cyrillic})[\p{L}\p{M}]+(?![\p{L}\p{M}])
```

Texto de teste inicial:

```text
В слове Тeст латинская e.
```

Correspondência esperada:

```text
Тeст
```

Contraexemplo que não deve ser encontrado:

```text
Он прочитал Latin.
```


Limitações e observações: Em Тeст, e é latina. Uma palavra totalmente latina junto de russo não é mistura. Fórmulas, marcas e jogos tipográficos podem ser legítimos. A regra não translitera.

### D13. Minúscula imediatamente antes de maiúscula

Encontrar possíveis palavras coladas ou erros de caixa.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
\p{Ll}\p{Lu}
```

Texto de teste inicial:

```text
ОнвышелИздома.
```

Correspondência esperada:

```text
лИ
```

Contraexemplo que não deve ser encontrado:

```text
Он вышел из дома.
```


Limitações e observações: É indispensável distinguir maiúsculas. Nomes como McDonald podem estar corretos. Encontra-se a transição, não uma fronteira de palavras reconstruída automaticamente.

### D14. Algarismo entre letras

Encontrar uma troca OCR típica de letra por algarismo.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
\p{L}+[0-9]+\p{L}+
```

Texto de teste inicial:

```text
Это сл0во.
```

Correspondência esperada:

```text
сл0во
```

Contraexemplo que não deve ser encontrado:

```text
В главе 10 текст.
```


Limitações e observações: H2O e outras fórmulas podem ser corretas. Não substitua todos os 0 por о nem todos os 3 por з.

### D15. Pontuação dentro de um fragmento de letras

Verificar um sinal encostado a letras dos dois lados.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
\p{L}+[.,;:!?]\p{L}+
```

Texto de teste inicial:

```text
Он,сказал слово.
```

Correspondência esperada:

```text
Он,сказал
```

Contraexemplo que não deve ser encontrado:

```text
Он, сказав слово, ушёл.
```


Limitações e observações: Abreviaturas, endereços e domínios podem corresponder. Para verificar apenas vírgulas, restrinja a classe a [,]. Não insira espaços em todos os resultados sem análise.

### D16. Parágrafo iniciado por minúscula

Encontrar uma possível quebra de parágrafo supérflua.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
^[ \t]*\p{Ll}
```

Texto de teste inicial:

```text
продолжение предложения.
```

Correspondência esperada:

```text
п
```

Contraexemplo que não deve ser encontrado:

```text
Начало предложения.
```


Limitações e observações: Versos, legendas, listas e citações podem começar corretamente por minúscula. O padrão não junta parágrafos nem conhece o contexto vizinho.

### D17. Falta de pontuação final antes das aspas de fecho

Encontrar uma terminação alfabética ou numérica seguida apenas de fechos e espaços.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[\p{L}\p{N}][»”")\]}]*[ \t\x{00A0}]*$
```

Texto de teste inicial:

```text
«Он пришёл»
```

Correspondência esperada:

```text
л»
```

Contraexemplo que não deve ser encontrado:

```text
«Он пришёл!»
```


Limitações e observações: Títulos e legendas dispensam frequentemente ponto. Uma referência de nota depois da pontuação correta pode gerar um falso positivo. Ao contrário da verificação do último », esta receita não assinala «Он пришёл!».

### D18. Minúscula depois do fim de frase

Encontrar provável erro de caixa depois de ponto, interrogação ou exclamação.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[.!?…][ \t]+[«„“"(\[]?\p{Ll}
```

Texto de teste inicial:

```text
Он пришёл. потом ушёл.
```

Correspondência esperada:

```text
. п
```

Contraexemplo que não deve ser encontrado:

```text
Он пришёл. Потом ушёл.
```


Limitações e observações: Pontos de abreviaturas e reticências do autor nem sempre terminam uma frase. É apenas uma lista de candidatos a rever.

### D19. Possível ponto em falta

Encontrar uma passagem por espaço de final minúsculo para palavra iniciada por maiúscula.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
\p{Ll}[»”]?[ \t]+[«„“]?\p{Lu}\p{Ll}+
```

Texto de teste inicial:

```text
Он пришёл Потом ушёл.
```

Correspondência esperada:

```text
л Потом
```

Contraexemplo que não deve ser encontrado:

```text
Он пришёл потом ушёл.
```


Limitações e observações: Nomes próprios e títulos no meio de uma frase produzem muitas correspondências legítimas. A receita não decide entre ponto, vírgula ou ausência de alteração.

### D20. Pares de aspas direitas

Encontrar um fragmento simples entre aspas duplas direitas numa só linha.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
"([^"\r\n]+)"
```

Texto de teste inicial:

```text
Он сказал "да".
```

Correspondência esperada:

```text
"да"
```

Contraexemplo que não deve ser encontrado:

```text
Он сказал «да».
```


Limitações e observações: Verifique aninhamento, polegadas, código e sistema de aspas. «$1» só é uma substituição apropriada em contextos selecionados, não uma normalização universal.

### D21. Hífen ou travessão entre números

Encontrar um possível intervalo que exige decisão editorial.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?<![0-9])[0-9]+[ \t]*[-—][ \t]*[0-9]+(?![0-9])
```

Texto de teste inicial:

```text
Страницы 12 - 15.
```

Correspondência esperada:

```text
12 - 15
```

Contraexemplo que não deve ser encontrado:

```text
Страницы 12–15.
```


Limitações e observações: Datas como 2026-10-02, números negativos e subtrações também podem corresponder. Não os converta automaticamente em intervalos. – é meia-risca, — travessão, - hífen.

### D22. Duas iniciais antes do apelido

Encontrar uma forma simples com duas iniciais para verificar os espaços.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
\b(\p{Lu})\.[ \t]*(\p{Lu})\.[ \t]+(\p{Lu}\p{Ll}+)\b
```

Texto de teste inicial:

```text
И.О. Иванов
```

Correspondência esperada:

```text
И.О. Иванов
```

Contraexemplo que não deve ser encontrado:

```text
Иванов Иван
```


Limitações e observações: Não abrange todos os apelidos compostos nem os diacríticos. Depois da seleção, pode usar $1., NBSP, $2., NBSP, $3, inserindo os caracteres reais e não os seus nomes.

### D23. Apelido antes de duas iniciais

Encontrar a ordem inversa de escrita do nome.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
\b(\p{Lu}\p{Ll}+)[ \t]+(\p{Lu})\.[ \t]*(\p{Lu})\.
```

Texto de teste inicial:

```text
Иванов И.О.
```

Correspondência esperada:

```text
Иванов И.О.
```

Contraexemplo que não deve ser encontrado:

```text
Иванов Иван
```


Limitações e observações: É controlo de formato, não identificação de pessoas. Apelidos compostos, partículas e três iniciais requerem outra regra.

### D24. Caracteres invisíveis a verificar

Encontrar hífen opcional, espaço de largura zero ou FEFF.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[\x{00AD}\x{200B}\x{FEFF}]
```

Texto de teste inicial:

```text
сло​во
```

O mesmo texto de teste com os caracteres invisíveis assinalados:

```text
сло[ZWSP]во
```

Aqui, ␠ representa um espaço normal, [TAB] uma tabulação, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. Esta é uma representação explicativa; as etiquetas não devem ser inseridas no livro.

Correspondência esperada:

```text
​
```

Contraexemplo que não deve ser encontrado:

```text
слово
```


Limitações e observações: O resultado no bloco é invisível: existe U+200B entre о e в. Só altere depois de esclarecer a função. O BOM físico do ficheiro e FEFF dentro do texto são situações distintas.

### D25. Espaços Unicode invulgares

Encontrar espaços estreitos, largos e outros especiais.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]
```

Texto de teste inicial:

```text
10 000
```

O mesmo texto de teste com os caracteres invisíveis assinalados:

```text
10[NNBSP]000
```

Aqui, ␠ representa um espaço normal, [TAB] uma tabulação, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. Esta é uma representação explicativa; as etiquetas não devem ser inseridas no livro.

Correspondência esperada:

```text
 
```

Contraexemplo que não deve ser encontrado:

```text
10 000
```


Limitações e observações: Um espaço inquebrável estreito entre grupos de algarismos pode ser totalmente correto. A receita deteta inconsistência, não considera todos esses caracteres erros.

### D26. Interrogações e exclamações repetidas

Encontrar pontuação expressiva ou duplicada por engano.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[!?]{2,}
```

Texto de teste inicial:

```text
Что?! Правда!!!
```

Correspondências esperadas, por ordem:

```text
?!
!!!
```

Contraexemplo que não deve ser encontrado:

```text
Что? Правда!
```


Limitações e observações: ?! e repetições do autor podem ser intencionais. A ação normal é rever, não reduzir todas as sequências a um sinal.

### D27. Х cirílica junto de algarismos romanos

Encontrar uma Х russa em notação formada por símbolos romanos latinos.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — desnecessária para esta expressão. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?<=[IVXLCDM])Х|Х(?=[IVXLCDM])
```

Texto de teste inicial:

```text
Глава IХ
```

Correspondência esperada:

```text
Х
```

Contraexemplo que não deve ser encontrado:

```text
Глава IX
```


Limitações e observações: Х é cirílica e X latina. Verifique caixa e contexto; não valida o número romano completo.

### D28. Várias maiúsculas antes de minúscula

Encontrar possível erro OCR de caixa no início de palavra.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Unicode (UCP)» — ativa. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
\p{Lu}{2,}\p{Ll}+
```

Texto de teste inicial:

```text
Он сказал ПРИвет.
```

Correspondência esperada:

```text
ПРИвет
```

Contraexemplo que não deve ser encontrado:

```text
Он сказал Привет.
```


Limitações e observações: Nomes, siglas com sufixos e abreviaturas latinas podem estar corretos. Não altere a caixa globalmente sem revisão.

## 18. Erros frequentes e diagnóstico

### 18.1. O texto está visível, mas não é encontrado

Confirme Design, a opção regex, caixa, âmbito, direção e caracteres reais. A `a` latina e a `а` russa parecem iguais, mas não são; NBSP difere de espaço normal e aspas tipográficas diferem das direitas.

Verifique UCP para limites de palavras russas e a distinção de caixa para maiúsculas/minúsculas. Não combine Apenas palavras completas com limites próprios complexos sem necessidade.

### 18.2. O resultado é demasiado extenso

Suspeite primeiro de `.*` ganancioso ou de uma classe negativa demasiado ampla. Troque «qualquer texto» por um conjunto explícito e limite comprimento e fronteiras. Verifique dotall.

### 18.3. Os espaços abrangem parágrafos

`\s+` não é sinónimo de espaço. Comece por `[ \t]+` para intervalos entre palavras e acrescente NBSP apenas quando necessário.

### 18.4. Apareceram algarismos ou desapareceram barras na substituição

Compare a gramática FBE com a secção 16. `${name}`, `$10`, `\n` e `\x{...}` não seguem automaticamente as regras da pesquisa ou de outro editor. Confirme que a captura existe e não é opcional noutra alternativa.

### 18.5. Mistura de alfabetos num texto bilingue normal

Um exemplo antigo procurava ambos os alfabetos em qualquer ponto da linha, encontrando `Он прочитал Latin.`. D12 restringe as duas verificações à mesma palavra. Esta é a diferença essencial entre erro OCR e frase bilingue.

### 18.6. «Falta de ponto» assinala uma citação correta

Não basta verificar o último carácter: `!` pode ser seguido de `»`. D17 considera aspas e parênteses finais, mas títulos e referências de notas continuam a exigir revisão.

### 18.7. A estrutura pretendida não é encontrada

A pesquisa textual não vê o DOM como um script estrutural. Etiquetas, aninhamento, IDs ausentes e deslocação de notas são tarefas diferentes. Pode precisar de Código, do validador FB2 ou de um script, não de uma expressão ainda mais complexa.

## 19. Desempenho e livros grandes

Comece por uma condição estreita: sinal, classe, palavra ou início de parágrafo. Evite repetições ilimitadas muito aninhadas e vários «qualquer texto» concorrentes. Uma pesquisa sem resultado pode explorar uma quantidade enorme de alternativas.

A não ganância não resolve universalmente a lentidão; também tenta alternativas. Normalmente ajudam a exclusão explícita do separador, o limite de comprimento e a redução do âmbito.

Se a pesquisa demorar, não sobreponha outras substituições. Simplifique o padrão e teste-o numa amostra curta. Um erro de limite de recursos PCRE2 não significa ausência de resultados.

Para processamento com parágrafos e etiquetas vizinhas, um script costuma ser mais claro e seguro. Não reúna toda a revisão numa alternativa gigante; regras separadas deixam perceber a causa de cada correspondência.

## 20. Verificação antes de guardar

Examine o início, meio e fim do fragmento alterado. Verifique exemplos de cada tipo, sobretudo aspas, intervalos, iniciais, notas e formatação conservada. Confirme que não desapareceram NBSP significativos nem se alteraram parágrafos inesperadamente.

Após alterações sensíveis à estrutura, execute a verificação FBE. Guarde, reabra se necessário e compare o resultado. Uma pesquisa bem-sucedida não substitui a validação FB2 nem a revisão editorial.

## 21. Fontes e âmbito de aplicação

Este manual reformula o regex-design.md fornecido, com explicações e amostras reescritas. As imprecisões sobre UCP, objeto de pesquisa, alfabetos mistos e gramática de substituição foram esclarecidas por fontes primárias. As receitas da secção 17 são cenários editoriais originais, não citações do manual PCRE2.

[D1] Sintaxe oficial PCRE2: âncoras, grupos, propriedades Unicode, retrocesso e opções.

`https://www.pcre.org/current/doc/html/pcre2pattern.html`

[D2] Documentação oficial Unicode PCRE2.

`https://www.pcre.org/current/doc/html/pcre2unicode.html`

[D3] FBE Next: compatibilidade PCRE2 e adaptador. Revisão d2257405d95b0328649acee64b38829b40a4314b.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/docs/pcre2-compatibility.md`

[D4] FBE Next: GetReplStr, PrepareRegexReplacementText e pesquisa/substituição em FBEview.cpp; SearchPresetCatalog.cpp e search-preset-design-fixtures.cpp da mesma revisão.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/src/fbe/FBEview.cpp`

Sintaxe do motor, capacidades da interface e decisão editorial correta são níveis diferentes. Um teste local de amostras não garante qualquer compilação FBE com qualquer documento. O estado das verificações consta do README do arquivo.
