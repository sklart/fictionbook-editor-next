# Manual de expressões regulares — Código

:::note
Nota sobre a tradução: as palavras e frases russas dos exemplos de controlo são mantidas intencionalmente. São dados de teste: as expressões regulares, os textos de substituição, os espaços e os resultados esperados coincidem com a edição russa de referência. As explicações foram traduzidas; os exemplos não são adaptados automaticamente às convenções tipográficas portuguesas.
:::

Guia completo de pesquisa e substituição no código-fonte XML de livros no FictionBook Editor Next.

Edição: 2 de outubro de 2026. O modo Design é tratado em regex-design.md. As receitas presentes pertencem a Código; não transfira padrões PCRE2 sem verificação.

## 1. Para que serve a pesquisa em Código

Código mostra etiquetas XML, atributos, referências, entidades e texto. É adequado à análise de FB2: elementos vazios, HTML importado, referências provisórias, atributos inesperados e resíduos técnicos da conversão.

Para revisão de palavras, Design é muitas vezes mais cómodo. Uma correspondência no XML pode estar num atributo, comentário, CDATA, nome de ficheiro ou dados binários, e não apenas no texto do livro. Considere isto antes de substituir.

«Apenas pesquisa e verificação» fornece candidatos, não prova erros no XML ou no texto. Mesmo espaços podem ser significativos; não há substituições universalmente seguras sobre XML arbitrário.

## 2. Início rápido

Guarde uma cópia, passe a Código, abra Procurar e ative Expressão regular. Os exemplos XML normalmente distinguem maiúsculas e desativam Apenas palavras completas: o padrão define os limites das etiquetas e dos atributos.

Um parágrafo simples vazio:

```regex
<p>[ \t]*</p>
```

Encontra `<p></p>` e `<p>   </p>` numa linha física, não `<p>Текст</p>`. Não substitua automaticamente por vazio ou `<empty-line/>`: as funções estruturais podem diferir.

Introduza só o padrão, sem `/.../g`, aspas C++ nem barras duplicadas de JSON.

## 3. Motor: Scintilla, não PCRE2

O FBE usa Scintilla com `SCFIND_REGEXP` e `SCFIND_CXX11REGEX`. A compilação considerada utiliza a implementação C++ de expressões regulares com gramática ECMAScript. Não é PCRE2 nem todo o JavaScript moderno. [S1, S2]

Existe também um antigo motor Scintilla básico com outras convenções. Não confunda exemplos antigos de grupos com parênteses escapados com o modo C++11 FBE: `(слово)` captura, e `\(слово\)` exige parênteses literais.

Unicode não é proibido, mas não existem UCP e propriedades PCRE2. As classes dependem da biblioteca padrão; `\w` não significa todas as letras de qualquer língua. Para ASCII previsível use `[A-Za-z0-9_]`; para russo, `[А-Яа-яЁё]`, que não abrange todo o cirílico.

### 3.1. Diferenças essenciais face a Design

| Propriedade | Design | Código |
| --- | --- | --- |
| Objeto pesquisado | Representação textual do livro | Código-fonte XML |
| Motor | PCRE2-16 | Scintilla C++11 |
| UCP e propriedades Unicode | Disponíveis no perfil PCRE2 | Sem sintaxe de propriedades PCRE2 |
| Lookbehind | Disponível com limites PCRE2 | Não suportado |
| Primeiro grupo na substituição | `$1` ou `\1` | `\1` |
| Formatação ao substituir | Comandos FBE | Apenas texto XML |
| Pesquisa através de linhas físicas | Depende da representação; substituição entre parágrafos limitada | Não suportada no percurso atual |

## 4. Pesquisa linha a linha: a limitação principal

No percurso Scintilla usado pelo FBE, `MatchOnLines` aplica o padrão separadamente ao conteúdo de cada linha física. Uma linha pode ser muito comprida; o ajuste à largura da janela não cria outra linha física. [S2]

Compare:

```xml
<empty-line/> <empty-line/>
```

com:

```xml
<empty-line/>
<empty-line/>
```

Em XML podem representar a mesma adjacência. Na pesquisa FBE atual, a primeira cabe numa correspondência, mas a segunda cruza a fronteira entre linhas e não cabe.

Acrescentar `\r\n`, `\n`, `\x0D`, `\x0A`, `\s*` ou uma classe «qualquer carácter» não retira o limite. O motor não recebe ambas as linhas como um único fragmento. Seria necessário alterar o percurso de pesquisa da aplicação.

Isto não impede inserir uma quebra de linha numa substituição. É uma operação distinta, demonstrada em K28. Separe «encontrar através de linhas» de «inserir uma nova linha depois do resultado».

### 4.1. Trabalhar com XML multilinha

Para um atributo numa linha separada, pode bastar procurar só o atributo sem toda a etiqueta inicial. Para relações entre linhas use ferramentas estruturais FBE, validador, script ou ferramenta XML própria.

Não elimine todas as quebras do livro por causa de uma regex: pode alterar texto, comentários, CDATA e legibilidade. Determine primeiro se a tarefa realmente exige uma correspondência multilinha.

## 5. Caracteres e escape

Um carácter normal representa-se a si próprio. Anteponha barra invertida a metacaracteres pretendidos literalmente.

| Texto pretendido | Expressão |
| --- | --- |
| Ponto | `\.` |
| Mais | `\+` |
| Interrogação | `\?` |
| Asterisco | `\*` |
| Parêntese | `\(` ou `\)` |
| Número entre parênteses retos | `\[[0-9]+\]` |
| Barra invertida | `\\` |

`.` significa um carácter no fragmento disponível, não o ponto literal. Em UTF-8, o tratamento efetivo depende do adaptador Scintilla e da biblioteca padrão. Não conte emoji presumindo «um ponto = um símbolo visível».

`\r` e `\n` representam CR/LF, mas não permitem atravessar linhas na pesquisa atual. Use `[ \t]` para espaços e tabulações.

`\Q...\E` PCRE2 não é um escape portável neste modo. Escape os metacaracteres individualmente.

## 6. Classes, intervalos e limites de palavra

`[abc]` encontra um carácter do conjunto, `[a-z]` um do intervalo e `[^abc]` um de fora. Pode acrescentar repetição, como `[0-9]+`.

| Classe | Significado prático |
| --- | --- |
| `[0-9]` | Algarismo ASCII |
| `[A-Za-z]` | Letra latina ASCII |
| `[ \t]` | Espaço normal ou tabulação |
| `[^<]` | Um carácter disponível diferente de < |
| `[^"']` | Um carácter que não é nenhum dos dois tipos de aspas direitas |
| `\d`, `\D` | Classe de algarismos e complemento segundo a implementação |
| `\s`, `\S` | Espaços em branco e complemento |
| `\w`, `\W` | Caracteres de palavra e complemento |

Uma classe negativa não entende XML. `[^<]*` é útil para conteúdo simples entre etiquetas, não para analisar integralmente entidades, comentários e CDATA.

`\b` é limite de palavra, `\B` não-limite. Para atributos pode ser insuficiente: `id` pode aparecer depois de dois pontos noutro nome. É mais fiável exigir início de linha, espaço ou tabulação como separador de atributo.

## 7. Âncoras e repetições

`^` e `$` correspondem ao início e ao fim da linha física no percurso atual. Para uma linha apenas com espaços horizontais:

```regex
^[ \t]+$
```

Eliminar os caracteres encontrados não remove a linha: a sua terminação não estava na correspondência.

| Quantificador | Repetições |
| --- | --- |
| `?` | Zero ou uma |
| `*` | Zero ou mais |
| `+` | Uma ou mais |
| `{3}` | Exatamente três |
| `{3,}` | Pelo menos três |
| `{2,5}` | De duas a cinco |

A gramática C++11 admite versões não gananciosas, como `*?` e `+?`. Para atributos XML é frequentemente mais claro excluir a aspa delimitadora:

```regex
"[^"\r\n]*"
```

Aplica-se a aspas duplas; aspas simples exigem a alternativa correspondente.

Não transfira quantificadores possessivos, grupos atómicos ou opções inline PCRE2 para este modo.

## 8. Grupos, alternância e lookahead

`( ... )` captura um fragmento para referências e substituição. `(?: ... )` agrupa sem número.

Escolha de etiquetas:

```regex
<(?:strong|emphasis)>
```

Uma referência numérica pode ligar nomes de abertura e fecho num fragmento simples da mesma linha:

```regex
<(strong|emphasis)>[ \t]*</\1>
```

Exige o mesmo nome, mas não verifica aninhamento XML arbitrário.

`(?=...)` e `(?!...)` verificam ou negam o texto seguinte sem o incluir. Uma fronteira válida depois do nome evita confundir `<a` com o início de `<author>`:

```regex
<a(?=[ \t>])
```

Para contexto à esquerda, capture-o e restitua-o na substituição: lookbehind não está disponível. Não copie `(?<=...)` da ajuda Design.

## 9. Substituição no modo Código

O FBE usa `SCI_REPLACETARGETRE`: a correspondência já foi encontrada e Scintilla interpreta a substituição. Não é `std::regex_replace` com `$1` nem a gramática de substituição Design FBE. [S1, S2]

### 9.1. Referências a capturas

| No campo de substituição | Significado |
| --- | --- |
| `\0` | Correspondência completa |
| `\1` … `\9` | Conteúdo dos grupos correspondentes |
| `$1` | Não é a referência de grupo utilizada neste percurso |

Procurar:

```regex
\(([0-9]+)\)
```

Substituir por:

```text
[\1]
```

`(12)` passa a `[12]`. Só faz sentido editorial no contexto selecionado: um número entre parênteses não é necessariamente marcador de nota.

Um dólar encontrado em Source não precisa de ser duplicado segundo regras de outro sistema. A barra invertida, pelo contrário, é um carácter de controlo.

### 9.2. Caracteres de controlo na substituição

Na revisão Scintilla verificada, `\t`, `\n`, `\r` e `\\` tornam-se tabulação, LF, CR e barra literal. Isto é independente da ausência de pesquisa entre linhas. [S2]

Use `\r\n` para CRLF e `\n` para LF, em conformidade com o documento. Evite terminações mistas acidentais.

`\x{00A0}` PCRE2 não insere um carácter Unicode neste campo; NBSP deve ser real. `\U`, `\L`, `\T`, `\S`, `\E`, `\Q` de Design não controlam aqui caixa nem formatação.

### 9.3. Eliminação e integridade XML

Um campo vazio elimina o intervalo encontrado. Remover uma etiqueta, atributo ou referência pode danificar o documento apesar da pesquisa correta. Por isso, as receitas estruturais são principalmente diagnósticas.

Mudar um `id` isoladamente pode deixar referências para o valor antigo. Para objetos relacionados use a função FBE de mudança de identificador, não substituições textuais independentes.

## 10. Como interpretar as receitas XML

XML distingue a caixa: `<p>` e `<P>` são nomes diferentes. A ordem dos atributos não lhes determina o sentido; os valores podem usar aspas simples ou duplas e há espaços admissíveis em torno de `=`. As receitas cobrem variantes comuns quando isso não torna o padrão excessivamente complexo. [S3]

`l:` e `xlink:` são prefixos XLink comuns em FB2. O significado resulta de `xmlns`, não do nome do prefixo. Uma receita que enumera estes dois não garante outro: adapte-a após verificar a declaração.

Muitos exemplos aproximam o conteúdo de uma etiqueta inicial com `[^>]*`. É prático em FB2 normal, mas `>` pode existir legalmente num valor de atributo. Comentários e CDATA também podem imitar marcação. As receitas servem para rever candidatos, não para validar integralmente ou reestruturar XML às cegas.

«Elemento vazio» não significa sempre erro do esquema, nem «entidade numérica» carácter danificado. Referências provisórias, nomes invulgares e endereços externos podem ter uma finalidade legítima.

## 11. Receitas práticas FB2/XML

Por omissão, as partes envolvidas devem estar na mesma linha física. Se o padrão procura a etiqueta inicial completa, todos os atributos verificados também precisam de estar nessa linha. Uma receita que procura apenas o atributo não impõe essa condição à etiqueta inteira.

### K01. Espaços no fim de uma linha física

Encontrar espaços normais e tabulações no fim.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[ \t]+$
```

Texto de teste inicial:

```text
<p>Текст</p>
```

O mesmo texto de teste com os caracteres invisíveis assinalados:

```text
<p>Текст</p>␠␠␠
```

Aqui, ␠ representa um espaço normal, [TAB] uma tabulação, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. Esta é uma representação explicativa; as etiquetas não devem ser inseridas no livro.

Substituir por: deixar o campo completamente vazio. Não escrever a palavra «vazio».

Resultado da substituição:

```text
<p>Текст</p>
```

Contraexemplo que não deve ser encontrado:

```text
<p>Текст</p>
```


Limitações e observações: Não remove a quebra. Em XML misto, CDATA ou áreas com espaços significativos, a cauda pode pertencer ao texto. Não considere a limpeza global incondicionalmente segura.

### K02. Limpar uma linha só com espaços

Eliminar espaços em branco numa linha sem outro conteúdo.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
^[ \t]+$
```

Texto de teste inicial:

```text

<p>Text</p>
```

O mesmo texto de teste com os caracteres invisíveis assinalados:

```text
␠␠␠[TAB]
<p>Text</p>
```

Aqui, ␠ representa um espaço normal, [TAB] uma tabulação, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. Esta é uma representação explicativa; as etiquetas não devem ser inseridas no livro.

Substituir por: deixar o campo completamente vazio. Não escrever a palavra «vazio».

Resultado da substituição:

```text

<p>Text</p>
```

Contraexemplo que não deve ser encontrado:

```text
<p>Text</p>
```


Limitações e observações: A linha fica vazia: LF/CRLF não integravam o resultado. Não é a eliminação de todas as linhas físicas vazias.

### K03. Espaço antes do fecho de elemento vazio

Encontrar o intervalo normal imediatamente antes de />.

Ação: substituição após verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
[ \t]+/>
```

Texto de teste inicial:

```text
<empty-line />
```

Substituir por:

```text
/>
```

Resultado da substituição:

```text
<empty-line/>
```

Contraexemplo que não deve ser encontrado:

```text
<empty-line/>
```


Limitações e observações: XML admite ambas as formas. É cosmético, não uma correção obrigatória. A sequência pode estar em texto ou comentários; verifique antes de substituir globalmente.

### K04. Parágrafo simples vazio

Verificar um par p sem texto.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
<p>[ \t]*</p>
```

Texto de teste inicial:

```text
<p>  </p><p>Text</p>
```

Correspondência esperada:

```text
<p>  </p>
```

Contraexemplo que não deve ser encontrado:

```text
<p>Text</p>
```


Limitações e observações: Não abrange atributos, quebras, entidade NBSP ou etiqueta aninhada. Parágrafo vazio e empty-line não são automaticamente equivalentes.

### K05. Dois empty-line na mesma linha

Encontrar linhas vazias FB2 adjacentes escritas numa só linha física.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>
```

Texto de teste inicial:

```text
<empty-line/> <empty-line />
```

Correspondência esperada:

```text
<empty-line/> <empty-line />
```

Contraexemplo que não deve ser encontrado:

```text
<empty-line/>
<empty-line/>
```


Limitações e observações: Só admite espaços normais e tabulações. Uma quebra entre elementos está intencionalmente excluída. Duas linhas vazias podem separar cenas por opção do autor.

### K06. Metadados importantes vazios

Verificar vários campos simples sem conteúdo.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
<(book-title|first-name|middle-name|last-name|genre|lang)>[ \t]*</\1>
```

Texto de teste inicial:

```text
<book-title> </book-title>
```

Correspondência esperada:

```text
<book-title> </book-title>
```

Contraexemplo que não deve ser encontrado:

```text
<book-title>Book</book-title>
```


Limitações e observações: Nem todos os campos são obrigatórios em todos os contextos. Regex não verifica esquema FB2, elemento pai ou validade de valores preenchidos.

### K07. Formatação inline vazia

Encontrar elementos emparelhados de formatação sem texto.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
<(strong|emphasis|strikethrough)>[ \t]*</\1>
```

Texto de teste inicial:

```text
<strong> </strong>
```

Correspondência esperada:

```text
<strong> </strong>
```

Contraexemplo que não deve ser encontrado:

```text
<strong>Text</strong>
```


Limitações e observações: NBSP, atributos e elementos aninhados exigem outra regra. Antes de eliminar, verifique a função estrutural.

### K08. A mesma formatação aninhada em si

Distinguir strong/strong da combinação válida strong/emphasis.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
<(strong|emphasis)>[ \t]*<\1(?=[ \t>])
```

Texto de teste inicial:

```text
<strong><strong>Text</strong></strong>
```

Correspondência esperada:

```text
<strong><strong
```

Contraexemplo que não deve ser encontrado:

```text
<strong><emphasis>Text</emphasis></strong>
```


Limitações e observações: O segundo nome não é escolhido independentemente: a referência exige o primeiro. O resultado não abrange todo o elemento e não deve ser simplesmente eliminado.

### K09. Possíveis etiquetas HTML após importação

Encontrar nomes HTML comuns a verificar em FB2.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
</?(?:b|i|br|div|span|font)(?=[ \t/>])[^>]*>
```

Texto de teste inicial:

```text
<div>Text</div>
```

Correspondências esperadas, por ordem:

```text
<div>
</div>
```

Contraexemplo que não deve ser encontrado:

```text
<section><p>Text</p></section>
```


Limitações e observações: Comentários ou CDATA com texto angular podem corresponder. Não troque b por strong e i por emphasis sem verificar conteúdo e atributos.

### K10. Etiqueta em maiúsculas

Encontrar nomes comuns de etiquetas inteiramente em maiúsculas.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
</?[A-Z][A-Z0-9_.:-]*(?=[ \t/>])[^>]*>
```

Texto de teste inicial:

```text
<P>Text</P>
```

Correspondências esperadas, por ordem:

```text
<P>
</P>
```

Contraexemplo que não deve ser encontrado:

```text
<p>Text</p>
```


Limitações e observações: É obrigatório distinguir a caixa, caso contrário também encontra p normal. Não cobre todos os nomes Unicode XML válidos nem verifica espaços de nomes.

### K11. Entidade HTML NBSP

Encontrar o texto literal &nbsp; no ficheiro-fonte.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
&nbsp;
```

Texto de teste inicial:

```text
<p>&nbsp;</p>
```

Correspondência esperada:

```text
&nbsp;
```

Contraexemplo que não deve ser encontrado:

```text
<p>&#160;</p>
```


Limitações e observações: Sem declaração adequada, &nbsp; não é uma das cinco entidades XML predefinidas. Verifique DTD e comentários. Não descodifique todas as entidades em massa.

### K12. Referências numéricas a caracteres

Encontrar a forma decimal ou hexadecimal de um carácter.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
&#(?:[0-9]+|x[0-9A-Fa-f]+);
```

Texto de teste inicial:

```text
<p>&#160; &#xA0;</p>
```

Correspondências esperadas, por ordem:

```text
&#160;
&#xA0;
```

Contraexemplo que não deve ser encontrado:

```text
<p>&amp;</p>
```


Limitações e observações: Ambas podem ser válidas. O padrão verifica a forma, não a validade do ponto de código. Transformar &lt; em < no texto pode estragar XML.

### K13. id vazio

Encontrar um atributo id normal por preencher, com aspas simples ou duplas.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:""|'')
```

Texto de teste inicial:

```text
<section id="">
```

Correspondência esperada:

```text
 id=""
```

Contraexemplo que não deve ser encontrado:

```text
<section id="s1">
```


Limitações e observações: O espaço anterior integra a correspondência. xml:id e outros nomes com prefixo não são cobertos. Gerar IDs exige referências e unicidade, que regex não garante.

### K14. Lista dos valores id

Encontrar id normais preenchidos para rever os nomes.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:"[^"]+"|'[^']+')
```

Texto de teste inicial:

```text
<section id="s1">
```

Correspondência esperada:

```text
 id="s1"
```

Contraexemplo que não deve ser encontrado:

```text
<section id="">
```


Limitações e observações: A lista não comprova unicidade ou validade dos IDs. Para renomear objetos ligados use a função do FBE.

### K15. Referências externas HTTP(S)

Encontrar href com prefixo XLink comum e endereço externo.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"https?://[^"]+"|'https?://[^']+')
```

Texto de teste inicial:

```text
<a l:href="https://example.test/book">Text</a>
```

Correspondência esperada:

```text
 l:href="https://example.test/book"
```

Contraexemplo que não deve ser encontrado:

```text
<a l:href="#note1">1</a>
```


Limitações e observações: Procura o atributo, não todos os URL do texto. Suporta l e xlink; outros prefixos exigem adaptação. Não verifica a disponibilidade do endereço.

### K16. Referências locais file://

Encontrar uma ligação local que pode não funcionar para o leitor.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"file://[^"]+"|'file://[^']+')
```

Texto de teste inicial:

```text
<a xlink:href="file:///C:/Book/image.png">Text</a>
```

Correspondência esperada:

```text
 xlink:href="file:///C:/Book/image.png"
```

Contraexemplo que não deve ser encontrado:

```text
<a xlink:href="#image1">Text</a>
```


Limitações e observações: Não abra automaticamente um endereço desconhecido. Encontrá-lo não decide se é necessário incorporar o ficheiro, alterar a referência ou removê-la.

### K17. Destino de referência vazio

Encontrar uma referência XLink normalmente vazia.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:""|'')
```

Texto de teste inicial:

```text
<a l:href="">Text</a>
```

Correspondência esperada:

```text
 l:href=""
```

Contraexemplo que não deve ser encontrado:

```text
<a l:href="#note1">Text</a>
```


Limitações e observações: Verifique em separado texto visível vazio e atributo href ausente: são casos distintos.

### K18. Destino #undefined

Encontrar uma referência provisória literal.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')
```

Texto de teste inicial:

```text
<a xlink:href="#undefined">Text</a>
```

Correspondência esperada:

```text
 xlink:href="#undefined"
```

Contraexemplo que não deve ser encontrado:

```text
<a xlink:href="#note1">Text</a>
```


Limitações e observações: Confirme a existência do objeto. undefined não é um nome proibido por XML, embora frequentemente indique uma ligação incompleta.

### K19. Referências com prefixo bookmark

Encontrar destinos bookmark típicos do conversor, não todas as referências internas.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#bookmark[^"]*"|'#bookmark[^']*')
```

Texto de teste inicial:

```text
<a l:href="#bookmark12">Text</a>
```

Correspondência esperada:

```text
l:href="#bookmark12"
```

Contraexemplo que não deve ser encontrado:

```text
<a l:href="#note1">Text</a>
```


Limitações e observações: href="#..." simples encontra qualquer destino interno. bookmark não prova que a ligação seja supérflua; a remoção exige revisão.

### K20. Referências de retorno Word/FBD

Encontrar _ftnref e _ednref comuns nos destinos.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#_(?:ftnref|ednref)[^"]*"|'#_(?:ftnref|ednref)[^']*')
```

Texto de teste inicial:

```text
<a l:href="#_ftnref1">Back</a>
```

Correspondência esperada:

```text
l:href="#_ftnref1"
```

Contraexemplo que não deve ser encontrado:

```text
<a l:href="#note1">Text</a>
```


Limitações e observações: A referência de retorno pode ser necessária à navegação. Não a elimine só pela origem do nome.

### K21. Referências a notas sem depender da ordem dos atributos

Encontrar uma abertura a com type=note e XLink-href interno em qualquer ordem.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
<a(?=[ \t>])(?=[^>]*[ \t]type[ \t]*=[ \t]*(?:"note"|'note'))(?=[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+'))[^>]*>
```

Texto de teste inicial:

```text
<a l:href="#note1" type="note">1</a>
```

Correspondência esperada:

```text
<a l:href="#note1" type="note">
```

Contraexemplo que não deve ser encontrado:

```text
<a type="link" l:href="#note1">1</a>
```


Limitações e observações: Admite type antes de href, xlink e aspas simples. Os atributos verificados devem estar na mesma linha física. Valores com >, comentários e marcação invulgar exigem análise XML. O destino da nota não é verificado.

### K22. Possíveis marcadores numéricos de notas

Encontrar números entre parênteses retos, chavetas ou parênteses curvos.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
\[[0-9]+\]|\{[0-9]+\}|\([0-9]+\)
```

Texto de teste inicial:

```text
<p>Text [12], {3}, (4).</p>
```

Correspondências esperadas, por ordem:

```text
[12]
{3}
(4)
```

Contraexemplo que não deve ser encontrado:

```text
<p>[note]</p>
```


Limitações e observações: Podem ser bibliografia, números de fórmulas, explicações ou texto normal. Não cria notas nem verifica unicidade da numeração.

### K23. Valores Your/Name esquecidos

Verificar substitutos ingleses típicos nos metadados do nome.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
<first-name>Your</first-name>|<last-name>Name</last-name>
```

Texto de teste inicial:

```text
<first-name>Your</first-name>
```

Correspondência esperada:

```text
<first-name>Your</first-name>
```

Contraexemplo que não deve ser encontrado:

```text
<first-name>John</first-name>
```


Limitações e observações: Confirme o contexto de autor/criador e o nome real. Não substitua automaticamente por dados de outra edição.

### K24. Ilustração por ligar #undefined

Encontrar image associado ao destino provisório habitual.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
<image(?=[ \t/>])[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')[^>]*>
```

Texto de teste inicial:

```text
<image l:href="#undefined"/>
```

Correspondência esperada:

```text
<image l:href="#undefined"/>
```

Contraexemplo que não deve ser encontrado:

```text
<image l:href="#cover"/>
```


Limitações e observações: FB2 usa normalmente XLink-href, não HTML-src. A presença do binary com esse ID verifica-se separadamente. Imagem ausente e referência errada são causas distintas.

### K25. Declaração windows-1251

Encontrar indicação da antiga codificação na declaração XML.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
<\?xml[ \t]+[^?]*encoding[ \t]*=[ \t]*(?:"windows-1251"|'windows-1251')[^?]*\?>
```

Texto de teste inicial:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Correspondência esperada:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Contraexemplo que não deve ser encontrado:

```text
<?xml version="1.0" encoding="utf-8"?>
```


Limitações e observações: Não é um erro por si só. Alterar windows-1251 para utf-8 no texto não recodifica os bytes. Use uma operação de gravação ou conversão consistente com a declaração.

### K26. E comercial sem entidade padrão conhecida

Encontrar & antes de uma sequência que não corresponde às formas padrão.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
&(?!(?:amp|lt|gt|apos|quot);|#[0-9]+;|#x[0-9A-Fa-f]+;)
```

Texto de teste inicial:

```text
<p>A & B</p>
```

Correspondência esperada:

```text
&
```

Contraexemplo que não deve ser encontrado:

```text
<p>A &amp; B</p>
```


Limitações e observações: & é admissível em CDATA e comentários; DTD pode declarar outras entidades. É um filtro diagnóstico, não um validador. & → &amp; sem contexto pode produzir escape duplicado.

### K27. Referências internas normais

Encontrar o destino local #id, distinguindo-o da pesquisa bookmark.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+')
```

Texto de teste inicial:

```text
<a l:href="#note1">1</a>
```

Correspondência esperada:

```text
l:href="#note1"
```

Contraexemplo que não deve ser encontrado:

```text
<a l:href="https://example.test">Text</a>
```


Limitações e observações: Mostra a referência, não prova a existência de um único destino. Referências pendentes ou duplicadas exigem análise do documento.

### K28. Separar dois empty-line depois de pesquisa na mesma linha

Demonstrar a diferença entre limite de pesquisa e inserção de quebra na substituição.

Ação: uma substituição única e controlada.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
(<empty-line[ \t]*/>)[ \t]*(<empty-line[ \t]*/>)
```

Texto de teste inicial:

```text
<empty-line/> <empty-line/>
```

Substituir por:

```text
\1\r\n\2
```

Resultado da substituição:

```text
<empty-line/>
<empty-line/>
```

Contraexemplo que não deve ser encontrado:

```text
<empty-line/>
<empty-line/>
```


Limitações e observações: Scintilla expande a substituição para CRLF. Num documento LF use \1\n\2. Não é um formatador XML universal nem ativa pesquisa multilinha.

### K29. Espaços duplicados em texto XML simples

Encontrar um intervalo textual entre etiquetas com vários espaços normais.

Ação: apenas pesquisa e verificação.

«Expressão regular» — ativa; «Apenas palavras completas» — desativada. «Distinguir maiúsculas/minúsculas» — ativa.

Procurar:

```regex
>[^<]*[ \t]{2,}[^<]*<
```

Texto de teste inicial:

```text
<p>one  two</p>
```

Correspondência esperada:

```text
>one  two<
```

Contraexemplo que não deve ser encontrado:

```text
<p>one two</p>
```


Limitações e observações: O resultado inclui delimitadores angulares e o fragmento simples completo. Para corrigir palavras costuma ser melhor Design. Não substitua todo o fragmento XML por um espaço.

## 12. Porque regex não substitui a validação XML

Uma expressão mostra que um fragmento se parece com uma sequência de caracteres pedida. Não prova a correção do documento inteiro: aninhamento, esquema FB2, declarações de espaços de nomes, unicidade de IDs ou existência dos destinos.

Dois `id` iguais em linhas físicas diferentes não podem ser identificados com fiabilidade por uma única comparação do regex atual. O mesmo problema surge com um destino ausente noutra parte do livro. São necessárias a verificação FBE e a análise estrutural.

Não expanda todas as entidades XML: `&lt;` e `&amp;` protegem texto de se tornar marcação. Não elimine tudo o que pareça HTML dentro de CDATA, comentários ou citações de código.

Uma substituição de etiqueta tem de preservar pares, atributos e conteúdo. Alterar só a abertura `<strong>` deixa o fecho anterior. Uma referência de grupo no exemplo não torna estruturalmente segura qualquer substituição posterior.

## 13. Erros frequentes

### 13.1. Encontram-se etiquetas minúsculas em vez de maiúsculas

Ative Distinguir maiúsculas/minúsculas. Em XML não é cosmético: `[A-Z]` sem essa distinção perde o sentido de verificação das maiúsculas.

### 13.2. Aparece $1 literal na substituição

Use `\1`. A substituição Source FBE passa por Scintilla, não por JavaScript ou pelo formato `$1` de outra API.

### 13.3. \p, \K ou lookbehind não funciona

São construções de outro perfil. Para texto, passe a Design; para XML, reescreva usando classes explícitas, captura do contexto, grupos sem captura e lookahead.

### 13.4. Duas etiquetas adjacentes não são encontradas

Verifique quebra física, espaço antes de `/>`, aspas simples, atributos adicionais e prefixo do espaço de nomes. Adjacência no texto formatado não equivale a adjacência na árvore XML.

### 13.5. A nota deixa de ser encontrada com outra ordem de atributos

«Primeiro type, depois href» depende da ordem. K21 verifica ambas as propriedades com lookahead separado. Se a etiqueta ocupa várias linhas, a pesquisa da etiqueta inteira continua a não chegar: procure o atributo ou utilize a estrutura.

### 13.6. O aninhamento strong/emphasis é considerado erro

Estilos diferentes válidos podem aninhar-se. Para verificar repetição do mesmo nome, use referência como K08, não dois grupos independentes com as mesmas alternativas.

### 13.7. «Bookmark» encontra todas as notas

Uma referência com `#` não é automaticamente um resíduo bookmark. Separe a análise geral K27 do prefixo específico K19.

### 13.8. A substituição danifica o texto visível

Em Código, espaços e fragmentos podem estar em texto, atributos, comentários ou dados binários. Anule e restrinja âmbito ou padrão. O nome «substituição segura» não anula o contexto XML.

## 14. Limitações do perfil Source

Não use UCP, propriedades PCRE2, lookbehind, grupos com nome do JavaScript moderno/PCRE2, grupos atómicos, quantificadores possessivos, branch reset, sub-rotinas, verbos PCRE2, `\K`, `\G` ou comandos de formatação Design.

As opções inline PCRE2 não substituem as caixas do diálogo Source. As funções do JavaScript mais recente não aparecem automaticamente no ECMAScript C++11.

MatchOnLines continua a trabalhar por linhas independentemente da sequência de quebra escrita no padrão. Uma compilação local por outro motor não confirma compatibilidade FBE.

Para palavras Unicode e revisão complexa use Design. Para a estrutura use ferramentas XML/FB2, em vez de um `.*` ilimitado para compensar todos os limites.

## 15. Desempenho e linhas XML compridas

Um padrão curto não é necessariamente rápido. Repetições ilimitadas e alternativas compridas concorrentes podem tornar lentas linhas enormes, sobretudo uma única linha com todo o documento ou grande conteúdo binary.

Comece, se possível, por um nome concreto de etiqueta/atributo e use uma classe limitada em vez de «qualquer carácter». Receitas separadas são mais fáceis de confirmar e anular do que procurar todos os erros ao mesmo tempo.

Não una todas as linhas para contornar MatchOnLines: altera o documento e não garante um tempo aceitável.

## 16. Verificação após substituir

Compare o intervalo esperado com o real. Nas capturas, verifique aspas, prefixos, delimitadores angulares e fechos. Confirme que comentários, CDATA e binary não foram alterados involuntariamente.

Valide FB2, guarde e reabra após mudanças significativas. Verifique a passagem Código ↔ Design, notas, ilustrações e texto visível. Ao mudar um identificador, verifique o objeto e todas as referências associadas.

## 17. Fontes e âmbito de aplicação

Este manual amplia o regex-source.md fornecido. Preserva o percurso motor → sintaxe → substituição → XML → limites, trocando receitas genéricas imprecisas por variantes mais estreitas. As explicações e amostras foram reescritas; as diferenças técnicas foram verificadas com fontes primárias.

[S1] Documentação oficial Scintilla, Searching: modo C++11, opções de pesquisa e SCI_REPLACETARGETRE.

`https://www.scintilla.org/ScintillaDoc.html#Searching`

[S2] Scintilla na revisão FBE Next d2257405d95b0328649acee64b38829b40a4314b: Document.cxx, Cxx11RegexFindText, MatchOnLines, BuiltinRegex::SubstituteByPosition. Distingue pesquisa através de linhas de inserção CR/LF na substituição.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/third_party/scintilla/src/Document.cxx`

[S3] W3C XML 1.0 e Namespaces in XML: nomes, atributos, entidades e espaços de nomes.

`https://www.w3.org/TR/xml/`

`https://www.w3.org/TR/xml-names/`
