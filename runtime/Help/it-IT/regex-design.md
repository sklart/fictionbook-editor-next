# Guida alle espressioni regolari — Design

Nota sulla traduzione: le parole e le frasi russe negli esempi di controllo sono mantenute intenzionalmente. Sono dati di prova: espressioni regolari, stringhe di sostituzione, spazi e risultati attesi corrispondono all’edizione russa di riferimento. Le spiegazioni sono tradotte; gli esempi non sono adattati automaticamente alle convenzioni tipografiche italiane.

Guida completa alla ricerca, alla sostituzione e alla revisione dei libri in FictionBook Editor Next.

Edizione: 2 ottobre 2026. La modalità Codice è trattata separatamente in regex-source.md. In questa guida «modello» indica un’espressione regolare; un «modello predefinito» è invece un insieme salvato di espressione e opzioni nel pannello Modelli.

## 1. Come usare questa guida

Un’espressione regolare descrive una regola di ricerca anziché una sola stringa precisa. Ad esempio, `[0-9]+` trova una sequenza di cifre di qualsiasi lunghezza e `[ \t]{2,}` trova due o più spazi ordinari o tabulazioni. La corrispondenza trovata e il testo destinato a sostituirla sono due cose diverse.

Il capitolo 2 permette di ottenere un primo risultato pratico. I capitoli 3–15 spiegano la sintassi; il capitolo 16 descrive la grammatica particolare delle sostituzioni FBE. Il capitolo 17 raccoglie ricette per i libri, ciascuna con condizioni, testo di prova e avvertenze. Seguono problemi comuni, prestazioni e fonti.

Nel campo Trova va copiata solo l’espressione. Le etichette «Trova», «Sostituisci», U+0020 e i delimitatori Markdown non fanno parte del modello. Non servono involucri JavaScript `/.../g`, virgolette di stringhe C++ o barre inverse raddoppiate come nel JSON.

Le caselle di controllo fanno parte della ricetta. Un risultato ottenuto ignorando le maiuscole può cambiare se la distinzione viene attivata. Non considerare queste opzioni un semplice dettaglio di presentazione.

«Solo ricerca e verifica» indica una ricetta diagnostica. Il testo trovato può essere corretto: una ripetizione voluta, un nome straniero, una citazione, un titolo o una scelta tipografica. Anche «sostituzione dopo la verifica» non significa che la modifica sia sicura per qualsiasi libro.

## 2. Prima ricerca e sostituzione sicura

Salvare una copia di lavoro. Passare a Design, aprire Trova o Sostituisci e attivare Espressione regolare. Per i primi tentativi disattivare Solo parole intere: è preferibile indicare i confini nell’espressione. Controllare ambito e direzione della ricerca.

Per trovare più spazi consecutivi:

```regex
[ \t]{2,}
```

Nel testo `Он   пришёл` viene trovato l’intervallo di tre spazi. Inserire un solo spazio ordinario nel campo della sostituzione. Eseguire prima una sostituzione, verificare il risultato `Он пришёл` e solo dopo valutare Sostituisci tutto.

Con molte corrispondenze, esaminare prima i risultati e correggere uno o due casi rappresentativi. Dopo un’operazione globale controllare testo, corsivo, grassetto, note e confini dei paragrafi. Se il risultato è inatteso, annullare prima di iniziare altre modifiche.

Il pulsante Applica nel pannello Modelli trasferisce espressione e opzioni nella finestra di ricerca. Non è un ordine di correggere automaticamente tutte le corrispondenze. Ricerca e sostituzione si avviano con i rispettivi comandi della finestra.

## 3. Motore e confini della modalità Design

Questa modalità usa PCRE2-16 con testo e modelli in UTF-16; UTF è sempre attivo. FBE costruisce separatamente il testo da cercare e applica le sostituzioni tenendo conto della struttura del libro. La documentazione PCRE2 spiega quindi il riconoscimento delle corrispondenze, non ogni comportamento dell’applicazione. Le fonti tecniche sono D1–D4.

La ricerca riguarda una rappresentazione testuale del libro, non il suo XML letterale. Cercare `<strong>` non permette di trovare il grassetto in Design. I tag XML si cercano in Codice; le trasformazioni strutturali richiedono strumenti dell’editor o script dedicati.

L’andata a capo visiva di una riga lunga non è un carattere di fine riga. Paragrafi, interruzioni reali e adattamento alla larghezza non vanno confusi. FBE usa la modalità multilinea per le ancore della rappresentazione di ricerca, così che inizi e fini delle righe testuali riflettano i confini dei paragrafi. Trovare testo attraverso più paragrafi e sostituirlo sono operazioni diverse: l’implementazione esaminata rifiuta le sostituzioni tra paragrafi.

Le ancore `\A` e `\z` riguardano l’inizio e la fine della stringa passata al motore. Non vanno considerate automaticamente l’inizio e la fine dell’intero FB2: contano l’ambito e il frammento costruito da FBE.

## 4. Unicode: UTF, UCP e maiuscole

### 4.1. Che cosa fa UTF

UTF permette al motore di trattare caratteri Unicode, compreso il cirillico. Non converte un alfabeto in un altro, non corregge l’OCR, non trasforma `ё` in `е` e non unifica automaticamente rappresentazioni canoniche equivalenti.

Una lettera accentata può essere memorizzata come un unico carattere oppure come lettera più segno combinante. Le forme possono apparire uguali ma differire nella ricerca carattere per carattere. PCRE2 non esegue la normalizzazione NFC/NFD al posto dell’utente. Nei libri con segni diacritici considerare `\p{M}`.

### 4.2. Che cosa fa UCP

Unicode (UCP) modifica soprattutto le classi abbreviate `\w`, `\d`, `\s` e i confini `\b` e `\B` che ne dipendono. Non è l’interruttore che abilita il cirillico.

Precisazione rispetto alle prime versioni della guida: in una build Unicode di PCRE2, le proprietà esplicite `\p{L}`, `\p{N}` e `\P{...}` sono disponibili anche senza UCP. Tuttavia, per un’espressione che combina `\p{L}` e `\b`, UCP rende coerenti la nozione di lettera e quella di confine di parola.

Confrontare la ricerca di una parola russa:

```regex
\bмир\b
```

Con UCP i confini seguono la classe di parola Unicode. Senza UCP non si deve attendere che `\b` tratti il cirillico come il testo ASCII latino. L’assenza di una corrispondenza non prova l’assenza della parola.

Per richiedere esattamente cifre ASCII usare `[0-9]`, non `\d`: con UCP quest’ultima classe può includere cifre decimali di altre scritture.

### 4.3. Proprietà utili

| Espressione | Che cosa cerca |
| --- | --- |
| `\p{L}` | Una lettera Unicode |
| `\p{Lu}` | Una maiuscola con ricerca sensibile al caso |
| `\p{Ll}` | Una minuscola con ricerca sensibile al caso |
| `\p{M}` | Un segno combinante |
| `\p{N}` | Un carattere numerico, categoria più ampia delle cifre decimali |
| `\p{Nd}` | Una cifra decimale |
| `\p{Latin}` | Un carattere della scrittura latina |
| `\p{Cyrillic}` | Un carattere della scrittura cirillica |
| `\P{L}` | Un carattere che non è una lettera |

Per sequenze di lettere con segni diacritici:

```regex
[\p{L}\p{M}]+
```

Non è una definizione linguistica universale di parola: trattini e apostrofi non sono inclusi. Aggiungerli solo consapevolmente.

### 4.4. La distinzione del caso è un’opzione separata

Attivare Maiuscole/minuscole quando si cercano anomalie come `строчнаяПрописная`, iniziali o modelli con `\p{Lu}` e `\p{Ll}`. Ignorare il caso può annullare il significato della regola. UCP non sostituisce questa opzione.

Attivazione della ricerca senza distinzione del caso nell’espressione:

```regex
(?i)глава
```

Effetto limitato a un gruppo:

```regex
(?i:глава)[ \t]+[0-9]+
```

## 5. Caratteri letterali ed escape

Le lettere e la maggior parte dei segni rappresentano sé stessi. Fuori da una classe, punto, parentesi, asterisco, più, punto interrogativo, parentesi graffe, ancore e barra inversa possono avere significati speciali.

Per cercare un metacarattere letterale, anteporre una barra inversa:

| Testo desiderato | Espressione |
| --- | --- |
| Un punto | `\.` |
| Un punto interrogativo | `\?` |
| Un più | `\+` |
| Un asterisco | `\*` |
| Una parentesi tonda aperta | `\(` |
| Una parentesi tonda chiusa | `\)` |
| Un numero tra parentesi quadre | `\[[0-9]+\]` |
| La barra inversa stessa | `\\` |

`(123)` cerca le cifre `123` e le cattura in un gruppo, ma non richiede parentesi nel testo. Per trovare `(123)` occorre rendere letterali entrambe le parentesi.

Un frammento letterale lungo può essere racchiuso tra `\Q` e `\E` nel modello PCRE2:

```regex
\QЦена (руб.) + доставка\E
```

Nel campo di sostituzione FBE questi simboli hanno altri significati. Le regole di escape della ricerca non si trasferiscono automaticamente alla sostituzione.

## 6. Spazi, tabulazioni e caratteri invisibili

| Espressione | Significato nella ricerca PCRE2 |
| --- | --- |
| `[ ]` | Solo spazio ordinario U+0020 |
| `[ \t]` | Spazio ordinario o tabulazione |
| `\t` | Tabulazione U+0009 |
| `\x{00A0}` | Spazio non separabile |
| `\x{202F}` | Spazio non separabile stretto |
| `\h` | Spazio orizzontale, compresi diversi spazi Unicode |
| `\s` | Carattere di spaziatura, che può includere fine riga |
| `\r` | Ritorno carrello CR |
| `\n` | Avanzamento riga LF |
| `\R` | Sequenza Unicode di interruzione di riga |
| `\x{00AD}` | Trattino facoltativo |
| `\x{200B}` | Spazio a larghezza zero |
| `\x{FEFF}` | FEFF nel testo |

Per pulire i normali intervalli tra parole scegliere `[ \t]`, non `\s`. La seconda forma è più ampia e può coinvolgere confini di paragrafi e spazi non separabili. Anche `\h` è utile per la diagnostica, ma troppo ampio per una sostituzione tipografica non qualificata.

Lo spazio non separabile lega numero e cifra, iniziali e cognome, valore e unità di misura. Convertirli tutti in spazi normali può peggiorare l’impaginazione. FBE ha inoltre un’impostazione per il carattere NBSP: verificare i casi U+00A0 rispetto al libro e alla build in uso.

U+200C e U+200D possono essere necessari in determinate scritture e nelle emoji composte. Non eliminare indiscriminatamente tutti i caratteri «invisibili». Un ritrovamento non equivale a un errore.

## 7. Classi e intervalli di caratteri

Una classe tra parentesi quadre consuma un carattere dell’insieme. `[abc]` significa una delle tre lettere, non la parola `abc`; `[abc]+` richiede una o più lettere dell’insieme.

`[^abc]` consuma un carattere esterno all’insieme. Non significa «il testo non è preceduto da abc»: per controllare il contesto servono i lookaround.

All’interno di una classe, il punto e molte parentesi perdono il significato speciale. Il trattino può introdurre un intervallo: per renderlo letterale metterlo all’inizio o alla fine, oppure usare l’escape. Per una parentesi quadra chiusa letterale è pratico `\]`.

```regex
[А-Яа-яЁё-]+
```

Il modello ammette lettere russe e trattino, ma non tutto il cirillico. Le lettere ucraine, bielorusse e di altre lingue richiedono un insieme più ampio o una proprietà Unicode.

Dentro una classe `\b` non indica un confine di parola. Non inserire i confini di parola in un normale insieme di caratteri.

## 8. Ancore, confini e corrispondenze vuote

Un’ancora verifica una posizione senza consumare lettere o spazi. `^`, `$` e `\b` possono quindi produrre corrispondenze di lunghezza zero, non visibili come un normale frammento selezionato.

| Ancora | Significato |
| --- | --- |
| `^` | Inizio di riga con multiline; altrimenti inizio dell’oggetto cercato |
| `$` | Fine di riga con multiline; il trattamento del fine riga finale dipende dal modo |
| `\A` | Solo inizio dell’oggetto cercato |
| `\z` | Fine esatta dell’oggetto cercato |
| `\Z` | Fine dell’oggetto oppure posizione prima dell’ultimo fine riga |
| `\b` | Confine tra carattere di parola e carattere non di parola |
| `\B` | Posizione che non è un confine di parola |
| `\G` | Posizione iniziale della chiamata corrente di riconoscimento |

`\G` non memorizza da sola la «corrispondenza precedente». È legata all’offset iniziale passato dall’applicazione, che in chiamate successive può coincidere con la fine del risultato precedente. In FBE non va considerata un metodo universale per percorrere il libro; per la revisione ordinaria preferire confini espliciti. [D1]

Per cercare una parola e non il suo prefisso in parole più lunghe, usare confini o controlli sulle lettere adiacenti. Con UCP:

```regex
\bтом\b
```

Non corrisponde all’inizio di `томик`. Trattini e apostrofi, tuttavia, possono dividere le parole per il motore diversamente da come le considera il revisore.

Usare con particolare prudenza le corrispondenze vuote in Sostituisci tutto: si inserisce testo in posizioni anziché sostituire caratteri visibili. Per imparare preferire risultati non vuoti.

## 9. Quantificatori: ripetizioni e retrocessione

| Notazione | Ripetizioni dell’elemento precedente |
| --- | --- |
| `?` | Zero o una |
| `*` | Zero o più |
| `+` | Una o più |
| `{3}` | Esattamente tre |
| `{3,}` | Almeno tre |
| `{2,5}` | Da due a cinque |

Il quantificatore agisce sul carattere, sulla classe o sul gruppo precedente. `аб+` ripete solo `б`; `(?:аб)+` ripete la coppia.

### 9.1. Ricerca avida

Testo iniziale:

```text
«первый» и «второй»
```

Modello:

```regex
«.*»
```

Punto e asterisco prendono inizialmente il massimo frammento possibile. Se necessario, il motore restituisce caratteri per soddisfare il resto del modello. Qui il risultato comprende entrambe le coppie di virgolette.

### 9.2. Ricerca non avida

```regex
«.*?»
```

Il motore prova prima il numero minimo di caratteri, ma può comunque ampliare la corrispondenza. La ricerca successiva trova `«первый»` e poi `«второй»`.

Per una semplice coppia di virgolette è spesso più chiaro delimitare esplicitamente il contenuto:

```regex
«[^»\r\n]*»
```

Non analizza virgolette annidate: queste richiedono un controllo editoriale separato.

### 9.3. Quantificatore possessivo

```regex
«.*+»
```

Non significa «virgolette ancora più corrette». `.*+` consuma anche la virgoletta di chiusura senza restituirla; il `»` restante nel modello non trova più un carattere. Nel testo precedente non si ottiene alcun risultato.

Può invece essere appropriata una forma che escluda la chiusura:

```regex
«[^»\r\n]*+»
```

Quantificatori possessivi e gruppi atomici controllano la retrocessione; non sono un’accelerazione universale.

## 10. Gruppi e riferimenti all’indietro

Le parentesi tonde conservano il frammento trovato. I numeri iniziano da 1 e seguono le parentesi di cattura aperte da sinistra a destra, anche con gruppi annidati.

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Su `02.10.2026` i gruppi sono `02`, `10` e `2026`. Il modello controlla il formato, non la validità della data nel calendario.

`(?:...)` raggruppa senza consumare un numero. È utile per mantenere prevedibili `$1` e `$2` nelle sostituzioni complesse.

Un gruppo con nome rende il modello più leggibile:

```regex
(?<word>\p{L}+)[ \t]+\k<word>
```

Un riferimento all’indietro richiede lo stesso testo catturato. Una chiamata di sottoprogramma, descritta più avanti, ripete la regola e può trovare un testo diverso: sono meccanismi distinti.

Per una parola ripetuta nel libro aggiungere confini, opzione del caso e insieme appropriato di spazi. Senza confini un riferimento può corrispondere a parti di parole più lunghe; una ricetta completa è riportata sotto.

I gruppi con nome nella ricerca non abilitano automaticamente sostituzioni con nome in FBE. Per la sostituzione usare i riferimenti numerici confermati.

## 11. Alternativa e gruppi atomici

La barra verticale sceglie tra rami alternativi:

```regex
(?:глава|часть)[ \t]+[0-9]+
```

Il raggruppamento conta: senza di esso il suffisso comune può applicarsi solo all’ultimo ramo. I rami vengono provati nell’ordine scritto; scegliere consapevolmente l’ordine delle forme lunghe e corte.

Un gruppo atomico vieta il ritorno all’interno di un gruppo già completato:

```regex
(?>а|аб)в
```

Su `абв` il primo ramo sceglie `а`, quindi non può tornare a `аб`: non c’è corrispondenza. La forma non atomica può trovarla. È un esempio didattico, non un invito ad aggiungere atomicità senza verifica.

## 12. Controlli sul testo adiacente: lookaround

Un lookaround verifica il contesto senza includerlo nella corrispondenza completa. Permette, ad esempio, di selezionare un numero mantenendo il segno che lo precede:

```regex
(?<=№ )[0-9]+
```

Su `№ 125` si trova solo `125`; lo spazio richiesto è ordinario e singolo.

Controllo a destra:

```regex
[0-9]+(?=[ \t]+руб\.)
```

Su `125 руб.` trova il numero, non `руб.`.

Controllo negativo in avanti:

```regex
\bглава\b(?![ \t]+[0-9])
```

Con UCP cerca la parola «глава» non seguita da una normale numerazione separata da spazio. È un esempio di selezione contestuale, non una regola di correzione.

`(?<=...)` verifica il testo precedente, `(?<!...)` nega tale condizione; `(?=...)` e `(?!...)` verificano o negano il testo successivo.

Il lookbehind ha limiti di lunghezza. PCRE2 moderno ammette alcune lunghezze variabili limitate, non ripetizioni illimitate arbitrarie. Per ricette portabili preferire un contesto breve e fisso, oppure una cattura o `\K`; non fare affidamento su `.*` nel lookbehind.

## 13. Opzioni interne all’espressione

| Opzione | Effetto |
| --- | --- |
| `(?i)` | Ignora maiuscole e minuscole |
| `(?-i)` | Distingue maiuscole e minuscole |
| `(?m)` | Ancore di inizio e fine multilinea |
| `(?-m)` | Disattiva le ancore multilinea |
| `(?s)` | Il punto può includere un fine riga |
| `(?-s)` | Ripristina il comportamento ordinario del punto |
| `(?x)` | Ignora spazi non significativi e commenti nel modello |

`(?m)` non fa attraversare le righe al punto. `(?s)` non autorizza le sostituzioni FBE tra paragrafi. Sono due opzioni di ricerca e un limite distinto dell’applicazione.

Nel modo esteso gli spazi del modello possono non essere letterali. Per uno spazio richiesto usare `[ ]`; fuori da una classe, `#` può iniziare un commento. I modelli multilinea sono leggibili nella documentazione, ma le ricette per il campo a riga singola vengono fornite su una sola riga.

## 14. Funzioni avanzate di PCRE2

La maggior parte delle correzioni non richiede questo capitolo. Serve a comprendere costrutti presenti in modelli altrui. Il supporto sintattico non rende automaticamente un’espressione pratica o sicura per tutto il libro.

### 14.1. Reimpostare l’inizio della corrispondenza

```regex
№[ \t]*\K[0-9]+
```

Su `№ 125` si verifica il prefisso, ma il testo trovato è solo `125`. È utile se occorre cambiare il numero e non il simbolo. Non usare `\K` nei lookaround senza verifica: la combinazione ha restrizioni PCRE2.

### 14.2. Condizione sulla partecipazione di un gruppo

```regex
^(\()?([0-9]+)(?(1)\))$
```

Ammette `12` e `(12)`, non `(12` senza chiusura. La condizione verifica se ha partecipato il primo gruppo. Prima di usarla per sostituire con gruppi opzionali, verificare il comportamento dell’adattatore FBE.

### 14.3. Numerazione comune nei rami alternativi

```regex
(?|глава ([0-9]+)|часть ([0-9]+))
```

In entrambi i rami il numero è nel gruppo 1: è il branch reset. Nei casi semplici, un gruppo non catturante e una cattura condivisa sono spesso più chiari.

### 14.4. Chiamare un sottoprogramma

```regex
(?<pair>[0-9]{2})-(?&pair)
```

Su `12-34` entrambe le parti rispettano la regola «due cifre», pur essendo diverse. Un riferimento `\k<pair>` richiederebbe invece di ripetere esattamente `12`.

I sottoprogrammi ricorsivi possono descrivere alcune strutture annidate, ma non sostituiscono il parser XML FBE. Per `<section>`, `<poem>`, note e tabelle usare gli strumenti strutturali.

### 14.5. Saltare frammenti

```regex
«[^»\r\n]*»(*SKIP)(*FAIL)|\bслово\b
```

Con UCP cerca la parola fuori da semplici coppie di virgolette russe. Il primo ramo marca il frammento citato da saltare; il secondo cerca la parola. Non analizza virgolette annidate o non chiuse: non affidargli ciecamente un’intera revisione.

## 15. Che cosa un’espressione regolare non decide

«Maiuscola dopo minuscola», «manca un segno alla fine del paragrafo» e «parola ripetuta» sono indizi formali, non decisioni editoriali. Regex non sa se `да да` è un refuso, una battuta voluta, un verso o un titolo.

Non aggiungere automaticamente punti, non unire tutti i paragrafi che iniziano con minuscola, non convertire ogni lettera latina in cirillica e non trasformare tutti i trattini in lineette. Operazioni complesse richiedono contesto e talvolta più passaggi sul DOM. Gli script FBE per pulizia, parole unite e note risolvono proprio questi compiti più ampi.

## 16. Grammatica delle sostituzioni FBE

PCRE2 esegue la ricerca; FBE interpreta la stringa sostitutiva. Esempi presi da JavaScript, Python, .NET, PCRE2 substitute o altri editor vanno quindi verificati. Qui si descrivono i comandi confermati dal codice e dalla guida originaria FBE. [D3, D4]

### 16.1. Corrispondenza e gruppi

| Nel campo di sostituzione | Significato |
| --- | --- |
| `$0` o `\0` | Intera corrispondenza |
| `$1` … `$9` | Gruppo con quel numero |
| `\1` … `\9` | Forma alternativa del riferimento numerico |
| `$+` o `\+` | Ultimo gruppo restituito dall’adattatore della corrispondenza |

«FBE sostituisce solo i gruppi 1–9» è impreciso: si possono inserire anche testo normale e corrispondenza completa. Il limite riguarda l’accesso numerico diretto, non il numero di gruppi supportato da PCRE2.

Non usare `$10` per il decimo gruppo. Forme con nome come `${name}` non appartengono alla grammatica confermata. Mantenere le catture necessarie nei primi nove gruppi e rendere non catturanti quelli ausiliari.

Provare separatamente gruppi opzionali o vuoti: FBE usa un proprio adattatore SubMatches. Per nuove sostituzioni globali evitare dipendenze sottili dai gruppi omessi o dall’«ultimo gruppo»; preferire catture esplicite obbligatorie.

### 16.2. Riordinare i gruppi

Trova:

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Sostituisci con:

```text
$3-$2-$1
```

`02.10.2026` diventa `2026-10-02`. È una dimostrazione di riordino, non la raccomandazione di cambiare tutte le date del libro.

### 16.3. Cambiare maiuscole e minuscole

| Comando | Effetto |
| --- | --- |
| `\U` | Attiva le maiuscole per il frammento inserito |
| `\L` | Attiva le minuscole |
| `\T` | Prima lettera del frammento maiuscola, altre minuscole |
| `\Q` | Disattiva i comandi di caso e formattazione per il testo successivo |

Trova:

```regex
(иван)
```

Sostituisci con:

```text
\T$1\Q
```

Risultato di controllo: `Иван`. `\T` non è una complessa capitalizzazione linguistica di ogni parola: `иван иванов` come unico frammento non deve necessariamente diventare `Иван Иванов`.

Non sovrapporre `\U` e `\L` senza reimpostazione. Separare le porzioni con `\Q` e verificare cirillico e diacritici. Il caso è cambiato da FBE, non da un normalizzatore Unicode PCRE2.

### 16.4. Grassetto e corsivo

`\S` attiva il grassetto del frammento inserito, `\E` il corsivo. `\Q` interrompe i comandi attivi per il testo successivo.

```text
\S$1\Q
```

Serve a formattare il contenuto del gruppo 1. Non cerca testo già in grassetto e non cancella tutta la formattazione esistente. Controllare prima un frammento e l’annullamento.

### 16.5. Stessi simboli, significati diversi

Nella ricerca `\S` è un carattere non di spaziatura; nella sostituzione FBE è grassetto. Nella ricerca `\Q...\E` rende letterale un frammento; nella sostituzione `\Q` reimposta i comandi e `\E` attiva il corsivo.

Non digitare `\n`, `\t`, `\x{00A0}`, `\$` o `$$` nella sostituzione Design presumendo il comportamento di altri editor. Non appartengono alla grammatica di letterali descritta qui; una sequenza sconosciuta può essere scartata. Per NBSP usare il carattere reale; per conservare un segno di controllo usare una cattura già trovata oppure una sostituzione non regex, dopo verifica.

Un campo vuoto elimina il testo trovato; un campo con uno spazio lo sostituisce con uno spazio. Le etichette `<пусто>`, `[NBSP]` e `U+00A0` nelle spiegazioni non devono essere digitate letteralmente.

## 17. Ricette pratiche per l’editing e la revisione

Gli esempi sono scenari indipendenti, non una promessa di identità con i nomi del catalogo predefinito. Provare prima ogni sostituzione su un solo risultato. Spazi e tabulazioni reali nei campioni sono conservati; i controesempi mostrano almeno un limite, ma non sostituiscono la verifica dell’intero libro.

### D01. Più spazi ordinari diventano uno

Compattare gli intervalli ordinari senza toccare un NBSP isolato.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
[ \t]{2,}
```

Testo di prova iniziale:

```text
Он   пришёл.
```

Sostituisci con: un solo spazio ordinario U+0020. È un carattere, non la parola «spazio».

Risultato della sostituzione:

```text
Он пришёл.
```

Controesempio che non deve essere trovato:

```text
Он пришёл.
```


Limiti e osservazioni: Nei versi, nelle tabelle e nelle rientranze simulate, più spazi possono essere voluti. Verificare l’ambito; non è un modo per ricostruire i rientri dei paragrafi.

### D02. Spazi all’inizio del paragrafo

Eliminare il rientro manuale prima del testo ordinario.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
^[ \t]+
```

Testo di prova iniziale:

```text
   Начало абзаца.
```

Lo stesso testo di prova con i caratteri invisibili indicati:

```text
␠␠␠Начало␠абзаца.
```

Qui ␠ indica uno spazio ordinario, [TAB] una tabulazione, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. È una rappresentazione esplicativa: queste etichette non vanno inserite nel libro.

Sostituisci con: lasciare il campo completamente vuoto. Non digitare la parola «vuoto».

Risultato della sostituzione:

```text
Начало абзаца.
```

Controesempio che non deve essere trovato:

```text
Начало абзаца.
```


Limiti e osservazioni: Nei versi e nell’impaginazione artistica i rientri manuali possono avere un significato. L’ancora riguarda la riga testuale, non il suo adattamento visivo alla larghezza.

### D03. Spazi alla fine del paragrafo

Eliminare la coda di spazi ordinari o tabulazioni.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
[ \t]+$
```

Testo di prova iniziale:

```text
Конец абзаца.
```

Lo stesso testo di prova con i caratteri invisibili indicati:

```text
Конец␠абзаца.␠␠␠
```

Qui ␠ indica uno spazio ordinario, [TAB] una tabulazione, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. È una rappresentazione esplicativa: queste etichette non vanno inserite nel libro.

Sostituisci con: lasciare il campo completamente vuoto. Non digitare la parola «vuoto».

Risultato della sostituzione:

```text
Конец абзаца.
```

Controesempio che non deve essere trovato:

```text
Конец абзаца.
```


Limiti e osservazioni: NBSP è volutamente escluso. Per spazi non standard usare una diagnostica separata.

### D04. Spazio prima della virgola e di altri segni

Conservare il segno eliminando l’intervallo che lo precede.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
[ \t]+([,;:!?])
```

Testo di prova iniziale:

```text
Слово , другое !
```

Sostituisci con:

```text
$1
```

Risultato della sostituzione:

```text
Слово, другое!
```

Controesempio che non deve essere trovato:

```text
Слово, другое!
```


Limiti e osservazioni: La regola è pensata per testo russo ordinario. La tipografia francese ammette spazi speciali prima di alcuni segni; non applicarla a quei libri senza adattamento.

### D05. Spazio dopo un segno di apertura

Eliminare lo spazio ordinario dopo una parentesi o una virgoletta russa di apertura.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
([(\[«„])[ \t]+
```

Testo di prova iniziale:

```text
« слово» ( пример)
```

Sostituisci con:

```text
$1
```

Risultato della sostituzione:

```text
«слово» (пример)
```

Controesempio che non deve essere trovato:

```text
«слово» (пример)
```


Limiti e osservazioni: Non tocca gli spazi nelle formule o negli esempi se non seguono immediatamente uno dei segni elencati. Il contesto va comunque controllato.

### D06. Spazio prima di un segno di chiusura

Eliminare lo spazio ordinario prima di una parentesi o virgoletta di chiusura.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
[ \t]+([)\]»”])
```

Testo di prova iniziale:

```text
«слово » (пример )
```

Sostituisci con:

```text
$1
```

Risultato della sostituzione:

```text
«слово» (пример)
```

Controesempio che non deve essere trovato:

```text
«слово» (пример)
```


Limiti e osservazioni: Non normalizza tutti gli stili possibili di virgolette o parentesi matematiche.

### D07. Esattamente tre punti diventano un’ellissi

Convertire tre punti consecutivi senza toccare sequenze più lunghe.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?<!\.)\.{3}(?!\.)
```

Testo di prova iniziale:

```text
Он подумал... и ответил.
```

Sostituisci con:

```text
…
```

Risultato della sostituzione:

```text
Он подумал… и ответил.
```

Controesempio che non deve essere trovato:

```text
Содержание.....12
```


Limiti e osservazioni: Verificare le convenzioni editoriali. Quattro punti e i puntini di riempimento degli indici sono esclusi intenzionalmente.

### D08. Puntini di sospensione distanziati

Riunire tre punti separati da spazi ordinari.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?<!\.)\.[ \t]*\.[ \t]*\.(?!\.)
```

Testo di prova iniziale:

```text
Он подумал. . . и ответил.
```

Sostituisci con:

```text
…
```

Risultato della sostituzione:

```text
Он подумал… и ответил.
```

Controesempio che non deve essere trovato:

```text
Слово. Другое.
```


Limiti e osservazioni: Trova anche tre punti compatti. Non usare per righe di riempimento o omissioni nelle citazioni regolate da convenzioni particolari.

### D09. Spazio non separabile dopo №

Legare il segno di numero al valore seguente.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
№[ \t]+([0-9]+)
```

Testo di prova iniziale:

```text
№ 125
```

Sostituisci con:

```text
№ $1
```

Risultato della sostituzione:

```text
№ 125
```

Controesempio che non deve essere trovato:

```text
№125
```


Limiti e osservazioni: Tra № e $1 nella sostituzione c’è un vero U+00A0. Non sostituirlo con la sequenza stampata \x{00A0}. Il modello non aggiunge uno spazio dove manca del tutto.

### D10. Spazio non separabile dopo §

Legare il segno di paragrafo al numero.

Azione: sostituzione dopo la verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
§[ \t]+([0-9]+)
```

Testo di prova iniziale:

```text
§ 12
```

Sostituisci con:

```text
§ $1
```

Risultato della sostituzione:

```text
§ 12
```

Controesempio che non deve essere trovato:

```text
§12
```


Limiti e osservazioni: Tra § e $1 c’è un vero NBSP. Con numerazioni complesse verificare che sia stato trovato il frammento desiderato.

### D11. Possibile ripetizione di una parola adiacente

Trovare una ripetizione separata da spazi orizzontali senza saltare paragrafi.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» disattivo.

Trova:

```regex
\b(\p{L}+)[ \t\x{00A0}]+\1\b
```

Testo di prova iniziale:

```text
Это это уже было.
```

Corrispondenza attesa:

```text
Это это
```

Controesempio che non deve essere trovato:

```text
Это уже было.
```


Limiti e osservazioni: Ripetizioni come «Да да» possono essere intenzionali. Non eliminare automaticamente la seconda parola: potrebbe servire una virgola oppure nessuna modifica. Apostrofi e trattini non sono analizzati completamente.

### D12. Latino e cirillico nella stessa parola

Individuare una mescolanza OCR in una parola continua, non qualsiasi riga bilingue.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?<![\p{L}\p{M}])(?=[\p{L}\p{M}]*\p{Latin})(?=[\p{L}\p{M}]*\p{Cyrillic})[\p{L}\p{M}]+(?![\p{L}\p{M}])
```

Testo di prova iniziale:

```text
В слове Тeст латинская e.
```

Corrispondenza attesa:

```text
Тeст
```

Controesempio che non deve essere trovato:

```text
Он прочитал Latin.
```


Limiti e osservazioni: In Тeст la e è latina. Una parola interamente latina accanto al russo non è mista. Formule, marchi e giochi tipografici possono produrre risultati leciti. La regola non traslittera nulla.

### D13. Minuscola immediatamente prima di maiuscola

Trovare possibili parole unite o errori di caso.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
\p{Ll}\p{Lu}
```

Testo di prova iniziale:

```text
ОнвышелИздома.
```

Corrispondenza attesa:

```text
лИ
```

Controesempio che non deve essere trovato:

```text
Он вышел из дома.
```


Limiti e osservazioni: Distinguere obbligatoriamente il caso. Marchi e nomi come McDonald possono essere corretti. Si trova il punto di transizione, non una separazione ricostruita automaticamente.

### D14. Cifra tra lettere

Trovare una tipica sostituzione OCR di una lettera con una cifra.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
\p{L}+[0-9]+\p{L}+
```

Testo di prova iniziale:

```text
Это сл0во.
```

Corrispondenza attesa:

```text
сл0во
```

Controesempio che non deve essere trovato:

```text
В главе 10 текст.
```


Limiti e osservazioni: H2O e altre formule possono essere corretti. Non sostituire globalmente tutti gli 0 con о o tutti i 3 con з.

### D15. Punteggiatura dentro un frammento di lettere

Controllare un segno adiacente a lettere su entrambi i lati.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
\p{L}+[.,;:!?]\p{L}+
```

Testo di prova iniziale:

```text
Он,сказал слово.
```

Corrispondenza attesa:

```text
Он,сказал
```

Controesempio che non deve essere trovato:

```text
Он, сказав слово, ушёл.
```


Limiti e osservazioni: Abbreviazioni, indirizzi e domini possono corrispondere. Per controllare solo le virgole restringere la classe a [,]. Non inserire spazi in tutti i risultati con una sola operazione.

### D16. Paragrafo che inizia con minuscola

Trovare una possibile interruzione di paragrafo superflua.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
^[ \t]*\p{Ll}
```

Testo di prova iniziale:

```text
продолжение предложения.
```

Corrispondenza attesa:

```text
п
```

Controesempio che non deve essere trovato:

```text
Начало предложения.
```


Limiti e osservazioni: Versi, didascalie, elenchi e citazioni possono iniziare correttamente con minuscola. Il modello non unisce paragrafi e non conosce il contesto adiacente.

### D17. Manca il segno finale prima delle virgolette di chiusura

Trovare una fine alfabetica o numerica seguita solo da segni di chiusura e spazi.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
[\p{L}\p{N}][»”")\]}]*[ \t\x{00A0}]*$
```

Testo di prova iniziale:

```text
«Он пришёл»
```

Corrispondenza attesa:

```text
л»
```

Controesempio che non deve essere trovato:

```text
«Он пришёл!»
```


Limiti e osservazioni: Titoli e didascalie spesso non hanno punto. Un richiamo di nota dopo un segno corretto può causare un falso positivo. A differenza del solo controllo di », la ricetta non segnala «Он пришёл!».

### D18. Minuscola dopo la fine della frase

Trovare un probabile errore di caso dopo punto, domanda o esclamazione.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
[.!?…][ \t]+[«„“"(\[]?\p{Ll}
```

Testo di prova iniziale:

```text
Он пришёл. потом ушёл.
```

Corrispondenza attesa:

```text
. п
```

Controesempio che non deve essere trovato:

```text
Он пришёл. Потом ушёл.
```


Limiti e osservazioni: I punti delle abbreviazioni e le ellissi d’autore non chiudono sempre una frase. È soltanto un elenco di candidati da controllare.

### D19. Possibile punto mancante

Trovare il passaggio da una desinenza minuscola a una parola maiuscola separata da spazio.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
\p{Ll}[»”]?[ \t]+[«„“]?\p{Lu}\p{Ll}+
```

Testo di prova iniziale:

```text
Он пришёл Потом ушёл.
```

Corrispondenza attesa:

```text
л Потом
```

Controesempio che non deve essere trovato:

```text
Он пришёл потом ушёл.
```


Limiti e osservazioni: Nomi propri e titoli nella frase daranno molte corrispondenze lecite. La ricetta non decide se serva un punto, una virgola o niente.

### D20. Coppie di virgolette dritte

Trovare un semplice frammento tra virgolette doppie dritte in una sola riga.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
"([^"\r\n]+)"
```

Testo di prova iniziale:

```text
Он сказал "да".
```

Corrispondenza attesa:

```text
"да"
```

Controesempio che non deve essere trovato:

```text
Он сказал «да».
```


Limiti e osservazioni: Controllare annidamento, pollici, codice e sistema di virgolette adottato. «$1» è una sostituzione possibile solo in contesti selezionati, non una normalizzazione universale.

### D21. Trattino o lineetta lunga tra numeri

Trovare un possibile intervallo numerico da valutare.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?<![0-9])[0-9]+[ \t]*[-—][ \t]*[0-9]+(?![0-9])
```

Testo di prova iniziale:

```text
Страницы 12 - 15.
```

Corrispondenza attesa:

```text
12 - 15
```

Controesempio che non deve essere trovato:

```text
Страницы 12–15.
```


Limiti e osservazioni: Date come 2026-10-02, numeri negativi e sottrazioni possono corrispondere. Non convertirli automaticamente in intervalli. – è una lineetta media, — lunga, - un trattino.

### D22. Due iniziali prima del cognome

Trovare una forma semplice con due iniziali per controllare gli spazi.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
\b(\p{Lu})\.[ \t]*(\p{Lu})\.[ \t]+(\p{Lu}\p{Ll}+)\b
```

Testo di prova iniziale:

```text
И.О. Иванов
```

Corrispondenza attesa:

```text
И.О. Иванов
```

Controesempio che non deve essere trovato:

```text
Иванов Иван
```


Limiti e osservazioni: Non copre tutti i cognomi composti e i diacritici. Dopo la selezione si possono usare $1., NBSP, $2., NBSP, $3, inserendo veri spazi non separabili e non i loro nomi.

### D23. Cognome prima di due iniziali

Trovare l’ordine inverso del nome.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
\b(\p{Lu}\p{Ll}+)[ \t]+(\p{Lu})\.[ \t]*(\p{Lu})\.
```

Testo di prova iniziale:

```text
Иванов И.О.
```

Corrispondenza attesa:

```text
Иванов И.О.
```

Controesempio che non deve essere trovato:

```text
Иванов Иван
```


Limiti e osservazioni: Controlla un formato, non identifica una persona. Cognomi composti, particelle e tre iniziali richiedono altre regole.

### D24. Caratteri invisibili da verificare

Trovare trattino facoltativo, spazio a larghezza zero o FEFF nel testo.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
[\x{00AD}\x{200B}\x{FEFF}]
```

Testo di prova iniziale:

```text
сло​во
```

Lo stesso testo di prova con i caratteri invisibili indicati:

```text
сло[ZWSP]во
```

Qui ␠ indica uno spazio ordinario, [TAB] una tabulazione, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. È una rappresentazione esplicativa: queste etichette non vanno inserite nel libro.

Corrispondenza attesa:

```text
​
```

Controesempio che non deve essere trovato:

```text
слово
```


Limiti e osservazioni: La corrispondenza nel campione è invisibile: tra о e в c’è U+200B. Inserire o eliminare solo dopo averne chiarito il ruolo. Il BOM fisico del file e FEFF nel testo sono casi distinti.

### D25. Spazi Unicode insoliti

Trovare spazi stretti, larghi o speciali.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]
```

Testo di prova iniziale:

```text
10 000
```

Lo stesso testo di prova con i caratteri invisibili indicati:

```text
10[NNBSP]000
```

Qui ␠ indica uno spazio ordinario, [TAB] una tabulazione, [NBSP] U+00A0, [NNBSP] U+202F e [ZWSP] U+200B. È una rappresentazione esplicativa: queste etichette non vanno inserite nel libro.

Corrispondenza attesa:

```text
 
```

Controesempio che non deve essere trovato:

```text
10 000
```


Limiti e osservazioni: Uno spazio non separabile stretto tra gruppi di cifre può essere corretto. La ricetta evidenzia disomogeneità, non classifica ogni carattere speciale come errore.

### D26. Punti interrogativi ed esclamativi ripetuti

Trovare punteggiatura espressiva o duplicata per errore.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
[!?]{2,}
```

Testo di prova iniziale:

```text
Что?! Правда!!!
```

Corrispondenze attese, in successione:

```text
?!
!!!
```

Controesempio che non deve essere trovato:

```text
Что? Правда!
```


Limiti e osservazioni: ?! e ripetizioni d’autore possono essere voluti. L’azione normale è controllare, non ridurre tutte le sequenze a un segno.

### D27. Х cirillica accanto a numeri romani

Trovare una Х russa in una notazione composta da simboli romani latini.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» non necessario per questa espressione. «Maiuscole/minuscole» attivo.

Trova:

```regex
(?<=[IVXLCDM])Х|Х(?=[IVXLCDM])
```

Testo di prova iniziale:

```text
Глава IХ
```

Corrispondenza attesa:

```text
Х
```

Controesempio che non deve essere trovato:

```text
Глава IX
```


Limiti e osservazioni: Х è cirillica, X latina. Verificare caso e contesto; il modello non convalida l’intero numero romano.

### D28. Più maiuscole prima di una minuscola

Trovare un possibile errore OCR nel caso all’inizio di una parola.

Azione: solo ricerca e verifica.

«Espressione regolare» attivo; «Solo parole intere» disattivo. «Unicode (UCP)» attivo. «Maiuscole/minuscole» attivo.

Trova:

```regex
\p{Lu}{2,}\p{Ll}+
```

Testo di prova iniziale:

```text
Он сказал ПРИвет.
```

Corrispondenza attesa:

```text
ПРИвет
```

Controesempio che non deve essere trovato:

```text
Он сказал Привет.
```


Limiti e osservazioni: Nomi, sigle con suffissi e abbreviazioni latine possono essere corretti. Non cambiare il caso globalmente senza verifica.

## 18. Errori frequenti e diagnosi

### 18.1. Il testo è visibile ma non viene trovato

Controllare modalità Design, casella regex, caso, ambito, direzione e caratteri reali. La `a` latina e la `а` russa sono simili ma diverse; NBSP non è uno spazio ordinario e le virgolette tipografiche non sono quelle dritte.

Per i confini delle parole russe controllare UCP; per maiuscole e minuscole controllare il caso. Non aggiungere Solo parole intere a confini già complessi senza una ragione.

### 18.2. Viene trovato un frammento troppo grande

Sospettare prima `.*` avido o una classe negata troppo ampia. Sostituire il generico «qualsiasi testo» con caratteri ammessi espliciti e limitare lunghezza e confini. Controllare dotall.

### 18.3. Gli spazi coinvolgono i paragrafi

`\s+` non è sinonimo di spazio ordinario. Per un intervallo tra parole partire da `[ \t]+` e aggiungere NBSP solo se necessario.

### 18.4. Nella sostituzione compaiono cifre o scompaiono barre inverse

Confrontare la grammatica FBE con il capitolo 16. `${name}`, `$10`, `\n` e `\x{...}` non seguono automaticamente le regole della ricerca o di altri editor. Verificare che il gruppo esista e non sia opzionale in un altro ramo.

### 18.5. La ricerca di alfabeti misti segnala una normale frase bilingue

Un vecchio esempio verificava latino e cirillico nell’intera riga, segnalando anche `Он прочитал Latin.`. D12 limita entrambe le verifiche alla stessa parola: è la differenza tra un indizio OCR e una riga bilingue.

### 18.6. «Punto mancante» segnala una citazione corretta

L’ultimo carattere non basta: `!` può essere seguito da `»`. D17 considera virgolette e parentesi finali, ma titoli e richiami di nota vanno comunque verificati.

### 18.7. Non viene trovata la struttura desiderata

La ricerca testuale non vede il DOM come uno script strutturale. Tag, annidamento, riferimenti a ID assenti e trasferimento delle note sono compiti distinti. Può servire Codice, il validatore FB2 o uno script fornito, non una regex ancora più complessa.

## 19. Prestazioni e libri grandi

Partire da un vincolo preciso: un segno, una classe, una parola o un inizio di paragrafo. Evitare ripetizioni illimitate annidate e più segmenti concorrenti di «qualsiasi testo»: una ricerca senza risultato può esplorare moltissime possibilità.

La non avidità non è una cura universale per la lentezza; anche un quantificatore non avido prova alternative. Spesso aiutano l’esclusione esplicita di un delimitatore, un limite di lunghezza e un ambito più piccolo.

Se la ricerca diventa lunga, non avviare altre sostituzioni sopra di essa. Semplificare il modello e provarlo su testo breve. Un errore di limite delle risorse PCRE2 non significa «nessuna corrispondenza».

Per elaborazioni con paragrafi e tag adiacenti uno script è spesso più chiaro e sicuro di una sola regex. Regole separate permettono di capire il motivo di ciascun risultato; non riunire tutta la revisione in un’enorme alternativa.

## 20. Controllo prima del salvataggio

Esaminare inizio, centro e fine della parte elaborata. Controllare esempi di ogni tipo, soprattutto virgolette, intervalli, iniziali, note e formattazione da conservare. Verificare che NBSP significativi non siano scomparsi e che i paragrafi non siano cambiati inaspettatamente.

Dopo modifiche sensibili alla struttura, eseguire la verifica del documento FBE. Salvare, riaprire se necessario e confrontare il risultato. Una ricerca riuscita non sostituisce la convalida FB2 né la revisione editoriale.

## 21. Fonti e ambito di validità

Questa guida rielabora il regex-design.md fornito dall’utente con spiegazioni e campioni riscritti. Le imprecisioni su UCP, confini dell’oggetto, alfabeti misti e sostituzioni sono state chiarite con fonti primarie. Le ricette del capitolo 17 sono scenari editoriali originali, non citazioni del manuale PCRE2.

[D1] Descrizione ufficiale della sintassi PCRE2: ancore, gruppi, proprietà Unicode, retrocessione e opzioni.

`https://www.pcre.org/current/doc/html/pcre2pattern.html`

[D2] Documentazione ufficiale Unicode di PCRE2.

`https://www.pcre.org/current/doc/html/pcre2unicode.html`

[D3] FBE Next: compatibilità PCRE2 e adattatore. Revisione d2257405d95b0328649acee64b38829b40a4314b.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/docs/pcre2-compatibility.md`

[D4] FBE Next: GetReplStr, PrepareRegexReplacementText e ricerca/sostituzione in FBEview.cpp; SearchPresetCatalog.cpp e search-preset-design-fixtures.cpp della stessa revisione.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/src/fbe/FBEview.cpp`

Sintassi del motore, capacità dell’interfaccia e validità della scelta editoriale sono tre livelli diversi. Prove locali sui campioni non garantiscono qualsiasi build FBE e documento. Lo stato delle verifiche è nel README dell’archivio.
