# Guida alle espressioni regolari — Codice

:::note
Nota sulla traduzione: le parole e le frasi russe negli esempi di controllo sono mantenute intenzionalmente. Sono dati di prova: espressioni regolari, stringhe di sostituzione, spazi e risultati attesi corrispondono all’edizione russa di riferimento. Le spiegazioni sono tradotte; gli esempi non sono adattati automaticamente alle convenzioni tipografiche italiane.
:::

Guida completa alla ricerca e alla sostituzione nel sorgente XML di un libro in FictionBook Editor Next.

Edizione: 2 ottobre 2026. Design è descritto in regex-design.md. Le ricette qui appartengono a Codice: non trasferire modelli PCRE2 in questa modalità senza verificarli.

## 1. A cosa serve la ricerca in Codice

Codice mostra tag XML, attributi, riferimenti, entità e testo del libro. È adatto all’analisi di FB2: elementi vuoti, tag HTML importati, collegamenti segnaposto, attributi inattesi e residui tecnici di conversione.

:::note
La regex esamina il testo XML, non un albero DOM analizzato. Verificare la struttura con uno strumento XML.
:::

Per la revisione delle parole è spesso più comodo Design. Nel sorgente XML una corrispondenza può trovarsi in un attributo, commento, CDATA, nome di file o dati binari oltre che nel testo del libro. Considerarlo prima di sostituire.

«Solo ricerca e verifica» restituisce candidati, non prove di XML o testo errati. Anche una pulizia di spazi può modificare contenuto significativo: non esistono sostituzioni universalmente sicure su qualsiasi XML.

## 2. Avvio rapido

Salvare una copia, passare a Codice, aprire Trova e attivare Espressione regolare. Negli esempi XML il caso è normalmente significativo e Solo parole intere è disattivo: il modello definisce i confini di tag e attributi.

Per un semplice paragrafo vuoto:

```regex
<p>[ \t]*</p>
```

Trova `<p></p>` e `<p>   </p>` sulla stessa riga fisica, non `<p>Текст</p>`. Non sostituire automaticamente il risultato con niente o `<empty-line/>`: possono avere funzioni strutturali differenti.

:::example
Trova: <p>[ \t]*</p>
Input: <p>   </p>
Corrispondenza: <p>   </p>
Atteso: Esaminare questo paragrafo prima di modificare l’XML.
:::

Copiare soltanto il modello, senza `/.../g`, virgolette C++ o barre inverse raddoppiate del JSON.

## 3. Motore: Scintilla, non PCRE2

FBE usa Scintilla con `SCFIND_REGEXP` e `SCFIND_CXX11REGEX`. Nella build esaminata si tratta dell’implementazione C++ di regex con grammatica ECMAScript. Non è PCRE2 e non comprende tutte le funzioni del JavaScript moderno. [S1, S2]

Scintilla possiede anche un vecchio motore di base con altre convenzioni. Non confondere i vecchi gruppi creati con parentesi precedute da escape con la modalità C++11 FBE: qui `(слово)` cattura, mentre `\(слово\)` richiede parentesi letterali.

Il testo Unicode non è vietato, ma non sono disponibili UCP e proprietà PCRE2. Le classi dipendono dalla libreria standard: `\w` non equivale necessariamente a tutte le lettere di tutte le lingue. Per ASCII prevedibile usare `[A-Za-z0-9_]`; per il russo `[А-Яа-яЁё]`, che non comprende tutto il cirillico.

### 3.1. Differenze principali rispetto a Design

| Proprietà | Design | Codice |
| --- | --- | --- |
| Oggetto della ricerca | Rappresentazione testuale del libro | Sorgente XML |
| Motore | PCRE2-16 | Scintilla C++11 |
| UCP e proprietà Unicode | Disponibili nel profilo PCRE2 | Non supportate come sintassi PCRE2 |
| Lookbehind | Disponibile con limiti PCRE2 | Non supportato |
| Primo gruppo nella sostituzione | `$1` o `\1` | `\1` |
| Formattazione nella sostituzione | Comandi FBE | Solo testo XML, senza comandi FBE |
| Ricerca attraverso righe fisiche | Dipende dalla rappresentazione; sostituzione tra paragrafi limitata | Non supportata dal percorso attuale |

## 4. Ricerca riga per riga: il limite principale

Nel percorso Scintilla usato da FBE, `MatchOnLines` applica il modello separatamente al contenuto di ogni riga fisica. Una riga può essere lunghissima; il suo adattamento alla larghezza della finestra non crea altre righe fisiche. [S2]

:::warning
Nella ricerca attuale la regex in Codice non può attraversare righe fisiche; nessun modello le unisce.
:::

Confrontare:

```xml
<empty-line/> <empty-line/>
```

con:

```xml
<empty-line/>
<empty-line/>
```

Per XML possono descrivere gli stessi elementi adiacenti. Per la ricerca regex FBE attuale la prima forma è una singola corrispondenza possibile, la seconda attraversa il confine di riga e non lo è.

Aggiungere `\r\n`, `\n`, `\x0D`, `\x0A`, `\s*` o una classe di «qualsiasi carattere» non elimina il limite: il motore non riceve entrambe le righe come un unico frammento. Servirebbe una modifica del codice di ricerca, non soltanto del modello.

La ricerca per righe non impedisce invece di inserire un fine riga nella sostituzione. È un’operazione diversa, illustrata in K28. Non confondere «trovare attraverso un fine riga» con «inserire un fine riga dopo il testo trovato».

### 4.1. Lavorare con XML multilinea

Per un attributo su una riga separata spesso basta cercare l’attributo senza includere l’intero tag iniziale. Per relazioni tra righe usare strumenti strutturali FBE, il validatore, uno script o uno strumento XML dedicato.

Non eliminare tutti i fine riga del libro per una regex: si possono alterare testo, commenti, CDATA e leggibilità. Stabilire prima se il compito richiede davvero una corrispondenza multilinea.

## 5. Caratteri ed escape

Un carattere ordinario rappresenta sé stesso. Anteporre una barra inversa ai metacaratteri da cercare letteralmente.

| Testo cercato | Espressione |
| --- | --- |
| Punto | `\.` |
| Più | `\+` |
| Punto interrogativo | `\?` |
| Asterisco | `\*` |
| Parentesi tonda | `\(` oppure `\)` |
| Numero tra parentesi quadre | `\[[0-9]+\]` |
| Barra inversa | `\\` |

Il punto `.` indica un carattere nel frammento di riga disponibile, non il punto letterale. In un documento UTF-8 il trattamento reale dipende dall’adattatore Scintilla e dalla libreria standard: non misurare la lunghezza delle emoji assumendo «un punto = un simbolo visibile».

`\r` e `\n` sono notazioni per CR e LF, ma non permettono il passaggio tra righe nella ricerca attuale. Per spazi e tabulazioni usare `[ \t]`.

`\Q...\E` di PCRE2 non è una forma di escape portabile qui. Rendere letterali i metacaratteri uno alla volta.

## 6. Classi, intervalli e confini di parola

`[abc]` trova un carattere dell’insieme, `[a-z]` un carattere dell’intervallo e `[^abc]` un carattere esterno. Un quantificatore può seguire la classe: `[0-9]+`.

| Classe | Significato pratico |
| --- | --- |
| `[0-9]` | Cifra ASCII |
| `[A-Za-z]` | Lettera latina ASCII |
| `[ \t]` | Spazio ordinario o tabulazione |
| `[^<]` | Un carattere disponibile diverso da < |
| `[^"']` | Un carattere diverso dai due tipi di virgolette dritte |
| `\d`, `\D` | Classe delle cifre e complemento secondo l’implementazione |
| `\s`, `\S` | Classe di spaziatura e complemento |
| `\w`, `\W` | Classe di parola e complemento |

Una classe negata non comprende XML. `[^<]*` è utile per un semplice contenuto tra tag, ma non analizza completamente entità, commenti o CDATA.

`\b` indica un confine di parola e `\B` il contrario. Per un attributo spesso non basta: `id` può comparire dopo i due punti in un altro nome. È più affidabile richiedere un separatore ammesso, cioè inizio di riga, spazio o tabulazione.

## 7. Ancore e ripetizioni

Nel percorso attuale `^` e `$` indicano inizio e fine della riga fisica. Per una riga composta solo da spazi orizzontali:

```regex
^[ \t]+$
```

Si possono eliminare i caratteri trovati, ma la riga fisica rimane: il fine riga non faceva parte della corrispondenza.

| Quantificatore | Ripetizioni |
| --- | --- |
| `?` | Zero o una |
| `*` | Zero o più |
| `+` | Una o più |
| `{3}` | Esattamente tre |
| `{3,}` | Almeno tre |
| `{2,5}` | Da due a cinque |

La grammatica C++11 permette le forme non avide come `*?` e `+?`. Per un attributo XML è spesso più chiaro escludere la virgoletta delimitatrice:

```regex
"[^"\r\n]*"
```

Vale per virgolette doppie; quelle singole richiedono il relativo ramo.

Non trasferire qui quantificatori possessivi, gruppi atomici o opzioni interne PCRE2.

## 8. Gruppi, alternativa e lookahead

Un gruppo catturante `( ... )` conserva il testo per riferimenti e sostituzioni. `(?: ... )` unisce parti che non devono ricevere un numero.

Scelta tra tag:

```regex
<(?:strong|emphasis)>
```

Un riferimento numerico può legare i nomi dei tag di un semplice frammento sulla stessa riga:

```regex
<(strong|emphasis)>[ \t]*</\1>
```

Richiede lo stesso nome in entrambe le posizioni; non verifica l’annidamento XML arbitrario.

`(?=...)` e `(?!...)` verificano o negano il testo successivo senza includerlo. Si può richiedere un confine valido dopo il nome di tag per non confondere `<a` con l’inizio di `<author>`:

```regex
<a(?=[ \t>])
```

Per il contesto sinistro usare una cattura e reinserirla nella sostituzione: lookbehind non è disponibile. Non copiare `(?<=...)` dalla guida Design.

## 9. Sostituzione nella modalità Codice

FBE usa `SCI_REPLACETARGETRE`: la corrispondenza è già trovata e Scintilla interpreta la sostituzione. Non è `std::regex_replace` con `$1`, né la grammatica della sostituzione Design. [S1, S2]

### 9.1. Riferimenti alle catture

| Nel campo di sostituzione | Significato |
| --- | --- |
| `\0` | Intera corrispondenza |
| `\1` … `\9` | Contenuto dei gruppi corrispondenti |
| `$1` | Non è il riferimento di gruppo usato da questo percorso |

Trova:

```regex
\(([0-9]+)\)
```

Sostituisci con:

```text
[\1]
```

`(12)` diventa `[12]`. Ha senso editoriale solo nel contesto scelto: un numero tra parentesi tonde non è necessariamente un richiamo di nota.

Un dollaro trovato nel testo Source non richiede raddoppio secondo le regole di altri sistemi. La barra inversa, invece, è un carattere di controllo.

### 9.2. Caratteri di controllo nella sostituzione

Il codice Scintilla della revisione esaminata interpreta `\t`, `\n`, `\r` e `\\` come tabulazione, LF, CR e barra inversa letterale. Questo è distinto dall’assenza di ricerca multilinea. [S2]

Per CRLF usare `\r\n`, per LF `\n`, coerentemente con il documento. Evitare terminazioni miste accidentali.

Sequenze PCRE2 come `\x{00A0}` non inseriscono un carattere Unicode in questo campo: per NBSP usare il carattere reale. `\U`, `\L`, `\T`, `\S`, `\E`, `\Q` di Design non impostano caso o formattazione qui.

### 9.3. Eliminazione e integrità XML

Un campo vuoto elimina il frammento trovato. Eliminare un tag vuoto, attributo o collegamento può danneggiare il documento anche quando la ricerca è corretta. Per questo le ricette strutturali sono soprattutto diagnostiche.

Rinominare un `id` solo in una posizione può lasciare riferimenti al vecchio valore. Per oggetti collegati usare la rinomina di identificatori FBE, non sostituzioni testuali indipendenti.

## 10. Come leggere le ricette XML

XML distingue il caso: `<p>` e `<P>` sono nomi diversi. L’ordine degli attributi non ne determina il significato; i valori possono usare virgolette singole o doppie e sono ammessi spazi attorno a `=`. Le ricette includono le varianti comuni quando non rendono il modello eccessivamente complesso. [S3]

`l:` e `xlink:` sono prefissi XLink comuni in FB2. Il significato viene dalla dichiarazione `xmlns`, non dal prefisso stesso. Un modello che elenca questi due nomi non garantisce il riconoscimento di un terzo: adattarlo verificando la dichiarazione.

Molti esempi usano `[^>]*` come approssimazione del contenuto di un tag iniziale. È comodo per FB2 ordinari, ma `>` può comparire legalmente in un valore di attributo. Commenti e CDATA possono inoltre imitare la sintassi XML. Le ricette servono a esaminare candidati, non a convalidare o ricostruire ciecamente XML.

Un «elemento vuoto» non è sempre un errore di schema. Una «entità numerica» non è un carattere danneggiato. Anche segnaposto, nomi insoliti o collegamenti esterni possono avere una funzione lecita.

## 11. Ricette pratiche FB2/XML

Salvo indicazione diversa, le parti coinvolte devono trovarsi sulla stessa riga fisica. Se si cerca l’intero tag iniziale, anche tutti gli attributi verificati devono essere su quella riga. Una ricetta che cerca solo un attributo non impone questa condizione all’intero tag.

### K01. Spazi alla fine di una riga fisica

Trovare una coda di spazi ordinari e tabulazioni.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
[ \t]+$
```

Testo di prova iniziale:

```text
<p>Текст</p>
```

Lo stesso testo di prova con i caratteri invisibili indicati:

```text
<p>Текст</p>␠␠␠
```

Qui ␠ indica uno spazio ordinario, [TAB] una tabulazione, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. È una rappresentazione esplicativa: queste etichette non vanno inserite nel libro.

Sostituisci con: lasciare il campo completamente vuoto. Non digitare la parola «vuoto».

Risultato della sostituzione:

```text
<p>Текст</p>
```

Controesempio che non deve essere trovato:

```text
<p>Текст</p>
```


Limiti e osservazioni: Non elimina il fine riga. In contenuto XML misto, CDATA o zone a spazi significativi, la coda può essere testo. Non dichiarare sicura la pulizia indiscriminata di tutte le righe.

### K02. Svuotare una riga di soli spazi

Eliminare la spaziatura di una riga priva di altro contenuto.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
^[ \t]+$
```

Testo di prova iniziale:

```text

<p>Text</p>
```

Lo stesso testo di prova con i caratteri invisibili indicati:

```text
␠␠␠[TAB]
<p>Text</p>
```

Qui ␠ indica uno spazio ordinario, [TAB] una tabulazione, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. È una rappresentazione esplicativa: queste etichette non vanno inserite nel libro.

Sostituisci con: lasciare il campo completamente vuoto. Non digitare la parola «vuoto».

Risultato della sostituzione:

```text

<p>Text</p>
```

Controesempio che non deve essere trovato:

```text
<p>Text</p>
```


Limiti e osservazioni: La riga resta vuota: LF/CRLF non facevano parte del risultato. Non elimina tutte le righe fisiche vuote.

### K03. Spazio prima della chiusura di un elemento vuoto

Trovare l’intervallo ordinario immediatamente prima di />.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
[ \t]+/>
```

Testo di prova iniziale:

```text
<empty-line />
```

Sostituisci con:

```text
/>
```

Risultato della sostituzione:

```text
<empty-line/>
```

Controesempio che non deve essere trovato:

```text
<empty-line/>
```


Limiti e osservazioni: XML ammette entrambe le grafie. È una modifica cosmetica, non la correzione di un errore obbligatorio. La sequenza può comparire nel testo o nei commenti: controllare prima della sostituzione globale.

### K04. Paragrafo semplice vuoto

Controllare una coppia p senza testo.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
<p>[ \t]*</p>
```

Testo di prova iniziale:

```text
<p>  </p><p>Text</p>
```

Corrispondenza attesa:

```text
<p>  </p>
```

Controesempio che non deve essere trovato:

```text
<p>Text</p>
```


Limiti e osservazioni: Esclude paragrafi con attributi, fine riga, entità NBSP o tag annidati. Paragrafo vuoto ed empty-line non sono automaticamente intercambiabili.

### K05. Due empty-line sulla stessa riga

Trovare due righe vuote FB2 adiacenti scritte su una sola riga fisica.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>
```

Testo di prova iniziale:

```text
<empty-line/> <empty-line />
```

Corrispondenza attesa:

```text
<empty-line/> <empty-line />
```

Controesempio che non deve essere trovato:

```text
<empty-line/>
<empty-line/>
```


Limiti e osservazioni: Sono ammessi solo spazi ordinari e tabulazioni. Un fine riga tra gli elementi è volutamente escluso. Due righe vuote possono separare scene per scelta dell’autore.

### K06. Metadati importanti vuoti

Controllare diversi campi semplici di metadati senza contenuto.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
<(book-title|first-name|middle-name|last-name|genre|lang)>[ \t]*</\1>
```

Testo di prova iniziale:

```text
<book-title> </book-title>
```

Corrispondenza attesa:

```text
<book-title> </book-title>
```

Controesempio che non deve essere trovato:

```text
<book-title>Book</book-title>
```


Limiti e osservazioni: Non tutti i campi sono obbligatori in ogni contesto. Regex non verifica schema FB2, elemento padre o correttezza dei valori compilati.

### K07. Formattazione inline vuota

Trovare elementi accoppiati di formattazione privi di contenuto.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
<(strong|emphasis|strikethrough)>[ \t]*</\1>
```

Testo di prova iniziale:

```text
<strong> </strong>
```

Corrispondenza attesa:

```text
<strong> </strong>
```

Controesempio che non deve essere trovato:

```text
<strong>Text</strong>
```


Limiti e osservazioni: NBSP, attributi ed elementi annidati richiedono altre regole. Verificare l’eventuale funzione strutturale prima di eliminare.

### K08. La stessa formattazione annidata in sé stessa

Distinguere strong/strong dal valido strong/emphasis.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
<(strong|emphasis)>[ \t]*<\1(?=[ \t>])
```

Testo di prova iniziale:

```text
<strong><strong>Text</strong></strong>
```

Corrispondenza attesa:

```text
<strong><strong
```

Controesempio che non deve essere trovato:

```text
<strong><emphasis>Text</emphasis></strong>
```


Limiti e osservazioni: La seconda scelta non è indipendente: un riferimento richiede il primo nome. Il frammento trovato non comprende l’intero elemento e non è destinato all’eliminazione diretta.

### K09. Possibili tag HTML dopo l’importazione

Trovare nomi HTML comuni da controllare in FB2.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
</?(?:b|i|br|div|span|font)(?=[ \t/>])[^>]*>
```

Testo di prova iniziale:

```text
<div>Text</div>
```

Corrispondenze attese, in successione:

```text
<div>
</div>
```

Controesempio che non deve essere trovato:

```text
<section><p>Text</p></section>
```


Limiti e osservazioni: Testo tra parentesi angolari in commenti o CDATA può corrispondere. Non trasformare b in strong e i in emphasis con una sostituzione indiscriminata senza controllare contenuto e attributi.

### K10. Tag in maiuscolo

Trovare nomi di tag comuni scritti tutti in maiuscolo.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
</?[A-Z][A-Z0-9_.:-]*(?=[ \t/>])[^>]*>
```

Testo di prova iniziale:

```text
<P>Text</P>
```

Corrispondenze attese, in successione:

```text
<P>
</P>
```

Controesempio che non deve essere trovato:

```text
<p>Text</p>
```


Limiti e osservazioni: Il caso deve essere significativo, altrimenti si trova anche p normale. Non comprende tutti i nomi Unicode XML leciti né valuta gli spazi dei nomi.

### K11. Entità HTML NBSP

Trovare la scrittura letterale &nbsp; nel file sorgente.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
&nbsp;
```

Testo di prova iniziale:

```text
<p>&nbsp;</p>
```

Corrispondenza attesa:

```text
&nbsp;
```

Controesempio che non deve essere trovato:

```text
<p>&#160;</p>
```


Limiti e osservazioni: Senza dichiarazione appropriata, &nbsp; non è una delle cinque entità XML predefinite. Verificare DTD e commenti. Non decodificare globalmente tutte le entità.

### K12. Riferimenti numerici ai caratteri

Trovare la notazione decimale o esadecimale di un carattere.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
&#(?:[0-9]+|x[0-9A-Fa-f]+);
```

Testo di prova iniziale:

```text
<p>&#160; &#xA0;</p>
```

Corrispondenze attese, in successione:

```text
&#160;
&#xA0;
```

Controesempio che non deve essere trovato:

```text
<p>&amp;</p>
```


Limiti e osservazioni: Entrambe le forme possono essere corrette. Il modello verifica la forma, non la validità del punto di codice. Decodificare &lt; in < nel testo può danneggiare XML.

### K13. id vuoto

Trovare un attributo id ordinario non compilato, con virgolette singole o doppie.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:""|'')
```

Testo di prova iniziale:

```text
<section id="">
```

Corrispondenza attesa:

```text
 id=""
```

Controesempio che non deve essere trovato:

```text
<section id="s1">
```


Limiti e osservazioni: La corrispondenza include lo spazio precedente. Nomi con prefisso come xml:id non sono coperti. La creazione di un ID richiede unicità e aggiornamento dei riferimenti, che regex non gestisce.

### K14. Elenco dei valori id

Trovare id ordinari compilati per esaminarne i nomi.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:"[^"]+"|'[^']+')
```

Testo di prova iniziale:

```text
<section id="s1">
```

Corrispondenza attesa:

```text
 id="s1"
```

Controesempio che non deve essere trovato:

```text
<section id="">
```


Limiti e osservazioni: L’elenco non prova unicità e validità di tutti gli ID. Per oggetti collegati usare la rinomina FBE.

### K15. Collegamenti esterni HTTP(S)

Trovare href con prefisso XLink comune e indirizzo esterno.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"https?://[^"]+"|'https?://[^']+')
```

Testo di prova iniziale:

```text
<a l:href="https://example.test/book">Text</a>
```

Corrispondenza attesa:

```text
 l:href="https://example.test/book"
```

Controesempio che non deve essere trovato:

```text
<a l:href="#note1">1</a>
```


Limiti e osservazioni: Cerca l’attributo, non tutti gli URL del testo. Sono previsti l e xlink; altri prefissi vanno gestiti separatamente. Non verifica che l’indirizzo sia raggiungibile.

### K16. Collegamenti locali file://

Trovare un riferimento locale che potrebbe non funzionare per il lettore.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"file://[^"]+"|'file://[^']+')
```

Testo di prova iniziale:

```text
<a xlink:href="file:///C:/Book/image.png">Text</a>
```

Corrispondenza attesa:

```text
 xlink:href="file:///C:/Book/image.png"
```

Controesempio che non deve essere trovato:

```text
<a xlink:href="#image1">Text</a>
```


Limiti e osservazioni: Non aprire automaticamente indirizzi sconosciuti. Il risultato non decide se incorporare il file, cambiare il collegamento o eliminarlo.

### K17. Destinazione di collegamento vuota

Trovare un normale riferimento XLink vuoto.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:""|'')
```

Testo di prova iniziale:

```text
<a l:href="">Text</a>
```

Corrispondenza attesa:

```text
 l:href=""
```

Controesempio che non deve essere trovato:

```text
<a l:href="#note1">Text</a>
```


Limiti e osservazioni: Controllare separatamente etichetta testuale vuota e attributo href assente: sono casi diversi.

### K18. Destinazione #undefined

Trovare un riferimento segnaposto letterale.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')
```

Testo di prova iniziale:

```text
<a xlink:href="#undefined">Text</a>
```

Corrispondenza attesa:

```text
 xlink:href="#undefined"
```

Controesempio che non deve essere trovato:

```text
<a xlink:href="#note1">Text</a>
```


Limiti e osservazioni: Verificare l’esistenza reale dell’oggetto. Il nome undefined non è vietato da XML; spesso, nella pratica, indica un collegamento non completato.

### K19. Collegamenti con prefisso bookmark

Trovare destinazioni bookmark tipiche di un convertitore, non tutti i riferimenti interni.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#bookmark[^"]*"|'#bookmark[^']*')
```

Testo di prova iniziale:

```text
<a l:href="#bookmark12">Text</a>
```

Corrispondenza attesa:

```text
l:href="#bookmark12"
```

Controesempio che non deve essere trovato:

```text
<a l:href="#note1">Text</a>
```


Limiti e osservazioni: href="#..." da solo trova qualsiasi riferimento interno. Il prefisso bookmark non prova che il collegamento sia superfluo: verificarlo prima di rimuoverlo.

### K20. Collegamenti di ritorno Word/FBD

Trovare i comuni _ftnref e _ednref nelle destinazioni.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#_(?:ftnref|ednref)[^"]*"|'#_(?:ftnref|ednref)[^']*')
```

Testo di prova iniziale:

```text
<a l:href="#_ftnref1">Back</a>
```

Corrispondenza attesa:

```text
l:href="#_ftnref1"
```

Controesempio che non deve essere trovato:

```text
<a l:href="#note1">Text</a>
```


Limiti e osservazioni: Un collegamento di ritorno può servire alla navigazione. Non eliminarlo solo per l’origine del nome.

### K21. Collegamenti alle note indipendenti dall’ordine degli attributi

Trovare un tag a iniziale con type=note e XLink-href interno in qualsiasi ordine.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
<a(?=[ \t>])(?=[^>]*[ \t]type[ \t]*=[ \t]*(?:"note"|'note'))(?=[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+'))[^>]*>
```

Testo di prova iniziale:

```text
<a l:href="#note1" type="note">1</a>
```

Corrispondenza attesa:

```text
<a l:href="#note1" type="note">
```

Controesempio che non deve essere trovato:

```text
<a type="link" l:href="#note1">1</a>
```


Limiti e osservazioni: Sono ammessi type prima di href, xlink e virgolette singole. Tutti gli attributi verificati devono stare sulla stessa riga. Valori contenenti >, commenti o forme insolite richiedono un parser XML. La nota di destinazione non viene verificata.

### K22. Possibili richiami numerici di nota

Trovare numeri tra parentesi quadre, graffe o tonde.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
\[[0-9]+\]|\{[0-9]+\}|\([0-9]+\)
```

Testo di prova iniziale:

```text
<p>Text [12], {3}, (4).</p>
```

Corrispondenze attese, in successione:

```text
[12]
{3}
(4)
```

Controesempio che non deve essere trovato:

```text
<p>[note]</p>
```


Limiti e osservazioni: Possono essere riferimenti bibliografici, numeri di formule, spiegazioni o testo normale. Non crea note né verifica l’unicità della numerazione.

### K23. Valori Your/Name dimenticati

Controllare i tipici segnaposto inglesi del nome nei metadati.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
<first-name>Your</first-name>|<last-name>Name</last-name>
```

Testo di prova iniziale:

```text
<first-name>Your</first-name>
```

Corrispondenza attesa:

```text
<first-name>Your</first-name>
```

Controesempio che non deve essere trovato:

```text
<first-name>John</first-name>
```


Limiti e osservazioni: Controllare il contesto autore/creatore del libro e il nome effettivo. Non sostituire automaticamente con dati di un’altra edizione.

### K24. Illustrazione non collegata #undefined

Trovare image riferito al segnaposto comune.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
<image(?=[ \t/>])[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')[^>]*>
```

Testo di prova iniziale:

```text
<image l:href="#undefined"/>
```

Corrispondenza attesa:

```text
<image l:href="#undefined"/>
```

Controesempio che non deve essere trovato:

```text
<image l:href="#cover"/>
```


Limiti e osservazioni: In FB2 il riferimento è normalmente XLink-href, non HTML-src. Verificare a parte il binary con lo stesso ID. Immagine assente e riferimento errato sono cause diverse.

### K25. Dichiarazione windows-1251

Trovare l’indicazione della vecchia codifica nella dichiarazione XML.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
<\?xml[ \t]+[^?]*encoding[ \t]*=[ \t]*(?:"windows-1251"|'windows-1251')[^?]*\?>
```

Testo di prova iniziale:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Corrispondenza attesa:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Controesempio che non deve essere trovato:

```text
<?xml version="1.0" encoding="utf-8"?>
```


Limiti e osservazioni: Non è un errore di per sé. Sostituire la stringa windows-1251 con utf-8 non ricodifica i byte del file. Usare un salvataggio o una conversione coerente con la dichiarazione.

### K26. E commerciale senza entità standard nota

Trovare & prima di una sequenza non riconducibile alle forme standard.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
&(?!(?:amp|lt|gt|apos|quot);|#[0-9]+;|#x[0-9A-Fa-f]+;)
```

Testo di prova iniziale:

```text
<p>A & B</p>
```

Corrispondenza attesa:

```text
&
```

Controesempio che non deve essere trovato:

```text
<p>A &amp; B</p>
```


Limiti e osservazioni: & è ammessa in CDATA e commenti e una DTD può dichiarare altre entità. È un filtro diagnostico, non un validatore XML. & → &amp; senza contesto provoca doppio escape.

### K27. Riferimenti interni ordinari

Trovare una destinazione locale #id senza confonderla con bookmark.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+')
```

Testo di prova iniziale:

```text
<a l:href="#note1">1</a>
```

Corrispondenza attesa:

```text
l:href="#note1"
```

Controesempio che non deve essere trovato:

```text
<a l:href="https://example.test">Text</a>
```


Limiti e osservazioni: Mostra la scrittura del riferimento, non prova l’esistenza di un unico id di destinazione. Riferimenti sospesi o duplicati richiedono un’analisi documentale.

### K28. Separare due empty-line dopo una ricerca sulla stessa riga

Dimostrare la differenza tra limite della ricerca e inserimento di fine riga nella sostituzione.

Azione: una singola sostituzione controllata.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(<empty-line[ \t]*/>)[ \t]*(<empty-line[ \t]*/>)
```

Testo di prova iniziale:

```text
<empty-line/> <empty-line/>
```

Sostituisci con:

```text
\1\r\n\2
```

Risultato della sostituzione:

```text
<empty-line/>
<empty-line/>
```

Controesempio che non deve essere trovato:

```text
<empty-line/>
<empty-line/>
```


Limiti e osservazioni: Scintilla espande la sostituzione in CRLF. Per un documento LF usare \1\n\2. Non è un formattatore XML universale né l’abilitazione della ricerca multilinea.

### K29. Spazi doppi nel testo XML semplice

Trovare un tratto di testo tra tag con più spazi ordinari.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
>[^<]*[ \t]{2,}[^<]*<
```

Testo di prova iniziale:

```text
<p>one  two</p>
```

Corrispondenza attesa:

```text
>one  two<
```

Controesempio che non deve essere trovato:

```text
<p>one two</p>
```


Limiti e osservazioni: La corrispondenza comprende i delimitatori angolari e l’intero frammento semplice. Per correggere parole è spesso migliore Design. Non sostituire l’intero risultato XML con un solo spazio.

## 12. Perché regex non sostituisce la convalida XML

Un’espressione prova che un tratto assomiglia a una sequenza di caratteri richiesta. Non prova correttezza dell’intero documento: annidamento, schema FB2, dichiarazioni degli spazi dei nomi, unicità degli ID ed esistenza delle destinazioni.

Due `id` uguali su righe fisiche diverse non sono rilevabili in modo affidabile con un solo confronto regex nel percorso attuale. Lo stesso vale per una destinazione assente altrove nel libro. Servono la verifica FBE e un’analisi strutturale.

Non espandere globalmente tutte le entità XML: `&lt;` e `&amp;` proteggono spesso il testo dalla trasformazione in markup. Non eliminare tutto ciò che sembra HTML dentro CDATA, commenti o citazioni di codice.

Una sostituzione di tag deve conservare accoppiamento, attributi e contenuto. Cambiare solo `<strong>` lascia invariata la chiusura. Un esempio con riferimento all’indietro non rende sicura qualsiasi sostituzione successiva.

## 13. Errori frequenti

### 13.1. Vengono trovati tag minuscoli invece dei maiuscoli

Attivare Maiuscole/minuscole. In XML non è cosmetico: `[A-Z]` senza distinzione del caso perde il significato di controllo delle maiuscole.

### 13.2. Nella sostituzione appare $1 letterale

Usare `\1`. La sostituzione Source FBE passa per Scintilla, non per JavaScript o per il normale formato `$1` di altre API.

### 13.3. Non funzionano \p, \K o lookbehind

Appartengono a un altro profilo. Per un compito testuale passare a Design; altrimenti riscrivere con classi esplicite, cattura del contesto, gruppi non catturanti e lookahead.

### 13.4. Non vengono trovati tag adiacenti

Controllare fine riga fisici, spazio prima di `/>`, virgolette singole, attributi aggiuntivi e prefisso dello spazio dei nomi. Adiacenza nel sorgente formattato e adiacenza nell’albero XML non sono uguali.

### 13.5. Una nota non viene trovata con attributi in ordine diverso

Una sequenza «prima type poi href» dipende dall’ordine. K21 verifica entrambe le condizioni con lookahead separati. Se il tag è spezzato su righe, la ricerca dell’intero tag non basta: cercare l’attributo o usare la struttura.

### 13.6. strong/emphasis viene segnalato come annidamento errato

Stili diversi e validi possono essere annidati. Per ripetere lo stesso nome serve un riferimento come in K08, non due scelte indipendenti dallo stesso elenco.

### 13.7. «Bookmark» trova tutte le note

Un riferimento interno con `#` non è necessariamente un residuo bookmark. Distinguere l’analisi generale K27 dal prefisso specifico in K19.

### 13.8. La sostituzione danneggia il testo visibile

In Codice spazi, segni e frammenti possono essere in testo, attributi, commenti o dati binari. Annullare e restringere ambito o modello. Il nome «sostituzione sicura» di uno scenario non annulla il contesto XML.

## 14. Limiti del profilo Source

Non usare UCP, proprietà PCRE2, lookbehind, gruppi con nome moderni JavaScript/PCRE2, gruppi atomici, quantificatori possessivi, branch reset, sottoprogrammi, verbi PCRE2, `\K`, `\G` o comandi di formattazione della sostituzione Design.

Le opzioni interne PCRE2 non sostituiscono le caselle della finestra Source. Funzioni del JavaScript più recente non sono automaticamente presenti nell’ECMAScript C++11.

MatchOnLines continua a lavorare per righe indipendentemente dalla notazione di fine riga scritta nel modello. Una compilazione locale con un altro motore non dimostra compatibilità FBE.

Per parole Unicode e revisione complessa usare Design. Per XML usare gli strumenti XML/FB2 anziché compensare ogni limite con un `.*` illimitato.

## 15. Prestazioni e lunghe righe XML

Un modello corto non è necessariamente veloce. Ripetizioni illimitate e alternative lunghe in concorrenza possono rallentare righe enormi, soprattutto quando una sola riga contiene il documento intero o grandi dati binary.

Iniziare possibilmente dal nome preciso di tag o attributo e preferire classi limitate a «qualsiasi carattere». Ricette separate sono più facili da verificare e annullare di una ricerca di tutti i possibili errori insieme.

Non unire tutto il sorgente in una riga per aggirare MatchOnLines: modifica il documento e non garantisce tempi accettabili.

## 16. Verifica dopo la sostituzione

Confrontare intervallo atteso e reale. Con i gruppi controllare virgolette, prefissi, parentesi angolari e tag finali. Verificare che commenti, CDATA e binary non siano stati modificati involontariamente.

Convalidare FB2, salvare e riaprire dopo modifiche importanti. Controllare passaggio Codice ↔ Design, note, immagini e testo visibile. Una rinomina di identificatore richiede la verifica dell’oggetto e di tutti i suoi riferimenti.

## 17. Fonti e ambito di validità

Questa guida amplia il regex-source.md dell’utente. Mantiene il percorso motore → sintassi → sostituzione → esempi XML → limiti, sostituendo ricette generiche imprecise con varianti più mirate. Spiegazioni e campioni sono riscritti; le differenze tecniche sono confrontate con fonti primarie.

[S1] Documentazione ufficiale Scintilla, sezione Searching: modalità C++11, opzioni di ricerca e SCI_REPLACETARGETRE.

`https://www.scintilla.org/ScintillaDoc.html#Searching`

[S2] Scintilla nella revisione FBE Next d2257405d95b0328649acee64b38829b40a4314b: Document.cxx, Cxx11RegexFindText, MatchOnLines, BuiltinRegex::SubstituteByPosition. Il codice distingue la ricerca multilinea dall’inserimento CR/LF nella sostituzione.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/third_party/scintilla/src/Document.cxx`

[S3] W3C XML 1.0 e Namespaces in XML: nomi, attributi, entità e spazi dei nomi.

`https://www.w3.org/TR/xml/`

`https://www.w3.org/TR/xml-names/`
