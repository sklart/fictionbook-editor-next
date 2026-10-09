# Handleiding voor reguliere expressies — Code

:::note
Opmerking bij de vertaling: Russische woorden en zinnen in de controlevoorbeelden zijn bewust ongewijzigd gebleven. Het zijn testgegevens: reguliere expressies, vervangteksten, witruimtetekens en verwachte resultaten komen overeen met de Russische referentieversie. De uitleg is vertaald; de voorbeelden zijn niet automatisch aangepast aan Nederlandse typografische regels.
:::

Een volledig overzicht van zoeken en vervangen in de XML-bron van boeken in FictionBook Editor Next.

Editie: 2 oktober 2026. Ontwerp wordt beschreven in regex-design.md. Deze recepten gelden voor Code; neem geen PCRE2-patronen over zonder controle.

## 1. Waarvoor zoeken in Code dient

Code toont XML-tags, attributen, verwijzingen, entiteiten en boektekst. Het is geschikt voor FB2-controle: lege elementen, geïmporteerde HTML-tags, tijdelijke verwijzingen, onverwachte attributen en technische conversieresten.

:::note
Regex onderzoekt XML-tekst, geen geparseerde DOM-boom. Controleer structuur met een XML-hulpmiddel.
:::

Voor gewone tekstcorrectie is Ontwerp vaak handiger. In XML kan een treffer ook in een attribuut, commentaar, CDATA, bestandsnaam of binaire inhoud zitten. Houd daar rekening mee vóór vervanging.

“Alleen zoeken en controleren” geeft kandidaten, geen bewijs dat XML of tekst fout is. Zelfs spaties kunnen inhoudelijk van belang zijn. Geen vervanging is universeel veilig voor willekeurige XML.

## 2. Snel beginnen

Bewaar een kopie, ga naar Code, open Zoeken en schakel Reguliere expressie in. XML-voorbeelden gebruiken normaal hoofdlettergevoeligheid met Alleen hele woorden uit. Tag- en attribuutgrenzen staan in het patroon.

Eenvoudige lege alinea:

```regex
<p>[ \t]*</p>
```

Vindt `<p></p>` en `<p>   </p>` op één fysieke regel, niet `<p>Текст</p>`. Vervang niet automatisch door niets of `<empty-line/>`; de structurele functie kan verschillen.

:::example
Zoeken: <p>[ \t]*</p>
Invoer: <p>   </p>
Overeenkomst: <p>   </p>
Verwacht: Controleer deze alinea voordat u de XML wijzigt.
:::

Plak uitsluitend het patroon, zonder `/.../g`, C++-stringtekens en dubbele JSON-backslashes.

## 3. Engine: Scintilla, niet PCRE2

FBE gebruikt Scintilla met `SCFIND_REGEXP` en `SCFIND_CXX11REGEX`. De onderzochte build gebruikt C++-regex met ECMAScript-grammatica. Dat is geen PCRE2 en omvat niet alle moderne JavaScript-functies. [S1, S2]

Scintilla heeft ook een oudere basisengine met andere conventies. Verwar oude captures met ge-escapete haakjes niet met FBE’s C++11-modus: `(слово)` legt vast, `\(слово\)` vereist letterlijke haakjes.

Unicode-tekst is toegestaan, maar er zijn geen UCP- of PCRE2-eigenschappen. Tekenklassen hangen af van de standaardbibliotheek: `\w` betekent niet automatisch alle letters uit elke taal. Gebruik `[A-Za-z0-9_]` voor voorspelbaar ASCII, of `[А-Яа-яЁё]` voor Russisch, niet voor geheel Cyrillisch.

### 3.1. Belangrijkste verschillen met Ontwerp

| Eigenschap | Ontwerp | Code |
| --- | --- | --- |
| Zoekobject | Tekstweergave van het boek | XML-bron |
| Engine | PCRE2-16 | Scintilla C++11 |
| UCP en Unicode-eigenschappen | Beschikbaar in PCRE2-profiel | Geen PCRE2-eigenschapssyntaxis |
| Lookbehind | Beschikbaar binnen PCRE2-grenzen | Niet ondersteund |
| Eerste groep vervangen | `$1` of `\1` | `\1` |
| Opmaak bij vervangen | FBE-opdrachten | Alleen XML-tekst |
| Fysieke regels oversteken | Afhankelijk van zoekweergave; vervanging tussen alinea’s beperkt | In huidig pad niet ondersteund |

## 4. Regelgewijs zoeken: de belangrijkste beperking

In het gebruikte Scintilla-pad past `MatchOnLines` het patroon afzonderlijk op elke fysieke regel toe. Een regel kan zeer lang zijn; visueel afbreken aan de vensterrand schept geen nieuwe fysieke regel. [S2]

:::warning
In het huidige zoekpad kan regex in Broncode geen fysieke regels overschrijden; geen patroon voegt ze samen.
:::

Vergelijk:

```xml
<empty-line/> <empty-line/>
```

met:

```xml
<empty-line/>
<empty-line/>
```

Voor XML kan dit dezelfde opeenvolging van elementen zijn. Voor de huidige FBE-regex past de eerste in één treffer; de tweede overschrijdt een regelgrens en past niet.

`\r\n`, `\n`, `\x0D`, `\x0A`, `\s*` of een klasse voor “alles” toevoegen heft de beperking niet op. De engine ontvangt de twee regels niet als één fragment. Daarvoor is een codewijziging in het zoekpad nodig.

Regelgewijs zoeken verhindert niet dat een vervanging een regeleinde invoegt. Dat is een andere handeling, geïllustreerd in K28. Onderscheid zoeken over een grens van het invoegen van zo’n grens.

### 4.1. Werken met meerregelige XML

Zoek voor een attribuut op een eigen regel vaak alleen het attribuut, zonder hele openingstag. Gebruik voor relaties tussen regels structurele FBE-functies, validator, script of XML-gereedschap.

Verwijder niet alle regeleinden voor één regex: dat kan tekst, commentaar, CDATA en leesbaarheid veranderen. Bepaal eerst of de taak werkelijk een meerregelige treffer vraagt.

## 5. Tekens en escaping

Een gewoon teken staat voor zichzelf. Gebruik een backslash om een metateken letterlijk te zoeken.

| Gewenste tekst | Expressie |
| --- | --- |
| Punt | `\.` |
| Plus | `\+` |
| Vraagteken | `\?` |
| Sterretje | `\*` |
| Rond haakje | `\(` of `\)` |
| Nummer tussen vierkante haken | `\[[0-9]+\]` |
| Backslash | `\\` |

`.` staat voor één teken in het beschikbare regelfragment. Bij UTF-8 hangt de echte verwerking af van Scintilla’s adapter en de standaardbibliotheek. Meet emoji niet op de aanname dat één punt altijd één zichtbaar teken is.

`\r` en `\n` zijn CR/LF-syntaxis maar laten de huidige zoekopdracht geen regels oversteken. Gebruik `[ \t]` voor gewone spaties en tabs.

PCRE2’s `\Q...\E` is hier geen overdraagbare manier om letterlijke tekst af te bakenen. Escape de speciale tekens afzonderlijk.

## 6. Klassen, bereiken en woordgrenzen

`[abc]` vindt één teken uit de verzameling, `[a-z]` één uit het bereik en `[^abc]` één daarbuiten. Voeg zo nodig herhaling toe: `[0-9]+`.

| Klasse | Praktische betekenis |
| --- | --- |
| `[0-9]` | ASCII-cijfer |
| `[A-Za-z]` | Latijnse ASCII-letter |
| `[ \t]` | Gewone spatie of tab |
| `[^<]` | Beschikbaar teken anders dan < |
| `[^"']` | Teken anders dan beide rechte aanhalingstekens |
| `\d`, `\D` | Cijferklasse en complement volgens de implementatie |
| `\s`, `\S` | Witruimteklasse en complement |
| `\w`, `\W` | Woordklasse en complement |

Een negatieve klasse begrijpt XML niet. `[^<]*` helpt bij eenvoudige inhoud tussen tags maar is geen volledige parser voor entiteiten, commentaar of CDATA.

`\b` is een woordgrens, `\B` juist niet. Voor attributen is dat vaak onvoldoende: `id` kan na een dubbele punt in een andere naam staan. Vereis expliciet een attribuutscheiding: regelbegin, spatie of tab.

## 7. Ankers en herhalingen

`^` en `$` verwijzen in het huidige pad naar begin en einde van de fysieke regel. Een regel met alleen horizontale witruimte:

```regex
^[ \t]+$
```

Verwijderen van de treffer wist de regel zelf niet: het regeleinde was geen onderdeel van de overeenkomst.

| Kwantor | Herhalingen |
| --- | --- |
| `?` | Nul of één |
| `*` | Nul of meer |
| `+` | Eén of meer |
| `{3}` | Precies drie |
| `{3,}` | Minstens drie |
| `{2,5}` | Twee tot vijf |

De C++11-grammatica ondersteunt niet-gulzige varianten zoals `*?` en `+?`. Bij XML-attributen is de sluitende quote uitsluiten vaak duidelijker:

```regex
"[^"\r\n]*"
```

Dit geldt voor dubbele quotes; enkele quotes vragen een overeenkomstig alternatief.

Neem geen possessieve kwantoren, atomaire groepen of PCRE2-inlineopties over.

## 8. Groepen, alternatieven en lookahead

`( ... )` bewaart tekst voor terugverwijzing en vervanging. `(?: ... )` groepeert zonder nummer.

Keuze tussen tags:

```regex
<(?:strong|emphasis)>
```

Een numerieke verwijzing kan de openende en sluitende naam in een eenvoudig regelfragment koppelen:

```regex
<(strong|emphasis)>[ \t]*</\1>
```

Dit vereist dezelfde naam maar controleert geen willekeurige XML-nesting.

`(?=...)` en `(?!...)` controleren volgende tekst zonder die mee te nemen. Een geldige grens na de tagnaam voorkomt dat `<a` op het begin van `<author>` past:

```regex
<a(?=[ \t>])
```

Leg context links vast en voeg die bij de vervanging terug: lookbehind ontbreekt hier. Kopieer geen `(?<=...)` uit de Ontwerp-handleiding.

## 9. Vervangen in Code

FBE gebruikt `SCI_REPLACETARGETRE`: de overeenkomst is gevonden en Scintilla werkt de vervangingsstring uit. Dit is niet `std::regex_replace` met `$1`, noch FBE’s Design-grammatica. [S1, S2]

### 9.1. Verwijzingen naar captures

| In het vervangingsveld | Betekenis |
| --- | --- |
| `\0` | Hele overeenkomst |
| `\1` … `\9` | Inhoud van de betreffende groepen |
| `$1` | Niet de groepsverwijzingssyntaxis van dit pad |

Zoeken:

```regex
\(([0-9]+)\)
```

Vervangen door:

```text
[\1]
```

`(12)` wordt `[12]`. Dat is alleen zinvol in de gekozen context: een getal tussen ronde haakjes is niet noodzakelijk een nootnummer.

Een dollarteken in Source hoeft niet volgens een ander systeem te worden verdubbeld. De backslash is wel een stuurteken.

### 9.2. Stuurtekens in de vervanging

De gecontroleerde Scintilla-code verwerkt `\t`, `\n`, `\r` en `\\` als tab, LF, CR en letterlijke backslash. Dit staat los van het ontbreken van zoeken over regels. [S2]

Gebruik `\r\n` voor CRLF en `\n` voor LF, passend bij het document. Voorkom toevallig gemengde regeleinden.

PCRE2-notatie `\x{00A0}` voegt hier geen Unicode-teken in. Gebruik de echte NBSP. Design-opdrachten `\U`, `\L`, `\T`, `\S`, `\E`, `\Q` regelen hier geen letterkast of opmaak.

### 9.3. Verwijderen en XML behouden

Een leeg veld verwijdert het gevonden bereik. Het wissen van een leeg element, attribuut of verwijzing kan het document beschadigen, ondanks een geslaagde zoekopdracht. Structurele recepten zijn daarom vooral diagnostisch.

Een `id` maar op één plaats wijzigen kan verwijzingen naar de oude waarde achterlaten. Gebruik FBE’s ID-hernoeming voor gekoppelde objecten, niet onafhankelijke tekstvervangingen.

## 10. XML-recepten lezen

XML is hoofdlettergevoelig: `<p>` en `<P>` zijn andere namen. Attribuutvolgorde bepaalt hun betekenis niet. Waarden mogen enkele of dubbele quotes hebben en spaties rond `=` zijn toegestaan. De recepten behandelen gangbare vormen voor zover dat niet buitensporig ingewikkeld wordt. [S3]

`l:` en `xlink:` zijn gangbare XLink-prefixen in FB2. De betekenis komt van `xmlns`, niet de prefixnaam zelf. Een recept dat die twee noemt garandeert geen andere prefix; pas het aan na controle van de declaratie.

Veel voorbeelden gebruiken `[^>]*` als benadering van de inhoud van een openingstag. Dat werkt vaak, maar `>` kan legaal in een attribuutwaarde staan. Commentaar en CDATA kunnen ook markup nabootsen. Recepten dienen om kandidaten te bekijken, niet om XML volledig te valideren of blind te verbouwen.

Een “leeg element” is niet altijd een schemafout; een “numerieke entiteit” niet noodzakelijk een beschadigd teken. Ook een tijdelijke naam, afwijkende naam of externe verwijzing kan legitiem zijn.

## 11. Praktische FB2/XML-recepten

Standaard moeten alle betrokken delen op één fysieke regel staan. Wordt de hele openingstag gezocht, dan moeten de gecontroleerde attributen ook op die regel staan. Een recept dat alleen een attribuut zoekt stelt die eis niet aan de gehele tag.

### K01. Spaties aan het einde van een fysieke regel

Een staart van gewone spaties en tabs vinden.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[ \t]+$
```

Oorspronkelijke testtekst:

```text
<p>Текст</p>
```

Dezelfde testtekst met zichtbare aanduidingen:

```text
<p>Текст</p>␠␠␠
```

Hier staat ␠ voor een gewone spatie, [TAB] voor een tab, [NBSP] voor U+00A0, [NNBSP] voor U+202F en [ZWSP] voor U+200B. Dit is een toelichtende weergave; deze labels worden niet in het boek ingevoegd.

Vervangen door: laat het veld volledig leeg. Typ niet het woord “leeg”.

Resultaat van de vervanging:

```text
<p>Текст</p>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<p>Текст</p>
```


Beperkingen en opmerkingen: Verwijdert geen regeleinde. In gemengde XML, CDATA of betekenisvolle witruimte kan de staart bij de tekst horen. Noem het opschonen van alle regels niet onvoorwaardelijk veilig.

### K02. Een regel met alleen witruimte leegmaken

Witruimte verwijderen van een regel zonder andere inhoud.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
^[ \t]+$
```

Oorspronkelijke testtekst:

```text

<p>Text</p>
```

Dezelfde testtekst met zichtbare aanduidingen:

```text
␠␠␠[TAB]
<p>Text</p>
```

Hier staat ␠ voor een gewone spatie, [TAB] voor een tab, [NBSP] voor U+00A0, [NNBSP] voor U+202F en [ZWSP] voor U+200B. Dit is een toelichtende weergave; deze labels worden niet in het boek ingevoegd.

Vervangen door: laat het veld volledig leeg. Typ niet het woord “leeg”.

Resultaat van de vervanging:

```text

<p>Text</p>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<p>Text</p>
```


Beperkingen en opmerkingen: De regel blijft leeg: LF/CRLF zat niet in de treffer. Dit verwijdert niet alle lege fysieke regels.

### K03. Spatie vóór het einde van een leeg element

Gewone ruimte direct vóór /> vinden.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[ \t]+/>
```

Oorspronkelijke testtekst:

```text
<empty-line />
```

Vervangen door:

```text
/>
```

Resultaat van de vervanging:

```text
<empty-line/>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<empty-line/>
```


Beperkingen en opmerkingen: Beide schrijfwijzen zijn geldig XML. Het is cosmetisch, geen verplichte foutcorrectie. De reeks kan ook in tekst of commentaar staan; controleer bulkvervanging.

### K04. Eenvoudige lege alinea

Een p-paar zonder tekst controleren.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
<p>[ \t]*</p>
```

Oorspronkelijke testtekst:

```text
<p>  </p><p>Text</p>
```

Verwachte overeenkomst:

```text
<p>  </p>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<p>Text</p>
```


Beperkingen en opmerkingen: Exclusief attributen, regeleinde, NBSP-entiteit of geneste tag. Een lege alinea en empty-line zijn niet automatisch uitwisselbaar.

### K05. Twee empty-line-elementen op één regel

Aangrenzende FB2-lege regels vinden die op dezelfde fysieke bronregel staan.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>
```

Oorspronkelijke testtekst:

```text
<empty-line/> <empty-line />
```

Verwachte overeenkomst:

```text
<empty-line/> <empty-line />
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<empty-line/>
<empty-line/>
```


Beperkingen en opmerkingen: Alleen gewone spaties en tabs zijn toegestaan. Een regeleinde tussen elementen wordt bewust niet ondersteund. Twee lege regels kunnen een bewuste scènescheiding zijn.

### K06. Lege belangrijke metadata

Verschillende eenvoudige metadatavelden zonder inhoud controleren.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
<(book-title|first-name|middle-name|last-name|genre|lang)>[ \t]*</\1>
```

Oorspronkelijke testtekst:

```text
<book-title> </book-title>
```

Verwachte overeenkomst:

```text
<book-title> </book-title>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<book-title>Book</book-title>
```


Beperkingen en opmerkingen: Niet elk veld is in iedere context verplicht. Regex controleert geen FB2-schema, ouderstructuur of geldigheid van ingevulde waarden.

### K07. Lege inline-opmaak

Lege gepaarde opmaakelementen vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
<(strong|emphasis|strikethrough)>[ \t]*</\1>
```

Oorspronkelijke testtekst:

```text
<strong> </strong>
```

Verwachte overeenkomst:

```text
<strong> </strong>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<strong>Text</strong>
```


Beperkingen en opmerkingen: NBSP, attributen en nesting vragen een andere regel. Controleer vóór verwijdering of het element structureel van belang is.

### K08. Dezelfde opmaak in zichzelf genest

Herhaald strong/strong onderscheiden van toegestaan strong/emphasis.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
<(strong|emphasis)>[ \t]*<\1(?=[ \t>])
```

Oorspronkelijke testtekst:

```text
<strong><strong>Text</strong></strong>
```

Verwachte overeenkomst:

```text
<strong><strong
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<strong><emphasis>Text</emphasis></strong>
```


Beperkingen en opmerkingen: De tweede naam wordt niet onafhankelijk gekozen: een terugverwijzing eist de eerste. De treffer omvat niet het hele element en is niet bedoeld om direct te wissen.

### K09. Mogelijke HTML-tags na import

Gangbare HTML-namen vinden die in FB2 controle vragen.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
</?(?:b|i|br|div|span|font)(?=[ \t/>])[^>]*>
```

Oorspronkelijke testtekst:

```text
<div>Text</div>
```

Verwachte overeenkomsten, achtereenvolgens:

```text
<div>
</div>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<section><p>Text</p></section>
```


Beperkingen en opmerkingen: Tekst in commentaar of CDATA kan ook passen. Verander b niet blind in strong en i in emphasis zonder inhoud en attributen te controleren.

### K10. Tag volledig in hoofdletters

Gangbare volledig in hoofdletters geschreven tagnamen vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
</?[A-Z][A-Z0-9_.:-]*(?=[ \t/>])[^>]*>
```

Oorspronkelijke testtekst:

```text
<P>Text</P>
```

Verwachte overeenkomsten, achtereenvolgens:

```text
<P>
</P>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<p>Text</p>
```


Beperkingen en opmerkingen: Hoofdlettergevoeligheid is verplicht, anders past ook gewone p. Dit omvat niet alle legale Unicode-XML-namen en beoordeelt geen namespace.

### K11. HTML-entiteit NBSP

De letterlijke notatie &nbsp; in de bron vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
&nbsp;
```

Oorspronkelijke testtekst:

```text
<p>&nbsp;</p>
```

Verwachte overeenkomst:

```text
&nbsp;
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<p>&#160;</p>
```


Beperkingen en opmerkingen: Zonder passende declaratie is &nbsp; niet een van de vijf voorgedefinieerde XML-entiteiten. Controleer DTD en commentaar. Decodeer niet alle entiteiten in bulk.

### K12. Numerieke tekenverwijzingen

Decimale of hexadecimale notatie van een teken vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
&#(?:[0-9]+|x[0-9A-Fa-f]+);
```

Oorspronkelijke testtekst:

```text
<p>&#160; &#xA0;</p>
```

Verwachte overeenkomsten, achtereenvolgens:

```text
&#160;
&#xA0;
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<p>&amp;</p>
```


Beperkingen en opmerkingen: Beide kunnen correct zijn. Het patroon controleert vorm, niet de geldigheid van het codepunt. &lt; als < in tekst uitschrijven kan XML beschadigen.

### K13. Lege id

Een leeg gewoon id-attribuut vinden met beide soorten quotes.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:""|'')
```

Oorspronkelijke testtekst:

```text
<section id="">
```

Verwachte overeenkomst:

```text
 id=""
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<section id="s1">
```


Beperkingen en opmerkingen: De voorafgaande spatie hoort bij de treffer. Namen zoals xml:id vallen erbuiten. Nieuwe ID’s vragen uniciteit en bijgewerkte verwijzingen, wat regex niet regelt.

### K14. Lijst van id-waarden

Gevulde gewone id-attributen vinden om naamgeving te beoordelen.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:"[^"]+"|'[^']+')
```

Oorspronkelijke testtekst:

```text
<section id="s1">
```

Verwachte overeenkomst:

```text
 id="s1"
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<section id="">
```


Beperkingen en opmerkingen: Een lijst bewijst geen uniciteit of geldigheid van alle ID’s. Gebruik de ingebouwde hernoemfunctie voor gekoppelde objecten.

### K15. Externe HTTP(S)-verwijzingen

href met gebruikelijke XLink-prefix en extern adres vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"https?://[^"]+"|'https?://[^']+')
```

Oorspronkelijke testtekst:

```text
<a l:href="https://example.test/book">Text</a>
```

Verwachte overeenkomst:

```text
 l:href="https://example.test/book"
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<a l:href="#note1">1</a>
```


Beperkingen en opmerkingen: Zoekt het attribuut, niet ieder URL in de tekst. Alleen l en xlink zijn opgenomen; andere prefixen vragen aanpassing. Bereikbaarheid van het adres wordt niet getest.

### K16. Lokale file://-verwijzingen

Een lokaal bestandspad vinden dat bij de lezer mogelijk niet werkt.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"file://[^"]+"|'file://[^']+')
```

Oorspronkelijke testtekst:

```text
<a xlink:href="file:///C:/Book/image.png">Text</a>
```

Verwachte overeenkomst:

```text
 xlink:href="file:///C:/Book/image.png"
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<a xlink:href="#image1">Text</a>
```


Beperkingen en opmerkingen: Open onbekende adressen niet automatisch. Een treffer bepaalt niet of u moet insluiten, wijzigen of verwijderen.

### K17. Leeg verwijzingsdoel

Een gewone lege XLink-verwijzing vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:""|'')
```

Oorspronkelijke testtekst:

```text
<a l:href="">Text</a>
```

Verwachte overeenkomst:

```text
 l:href=""
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<a l:href="#note1">Text</a>
```


Beperkingen en opmerkingen: Controleer lege zichtbare linktekst en een geheel ontbrekend href-attribuut apart; dat zijn andere gevallen.

### K18. Doel #undefined

Een letterlijke tijdelijke verwijzing vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')
```

Oorspronkelijke testtekst:

```text
<a xlink:href="#undefined">Text</a>
```

Verwachte overeenkomst:

```text
 xlink:href="#undefined"
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<a xlink:href="#note1">Text</a>
```


Beperkingen en opmerkingen: Controleer of het object werkelijk bestaat. undefined is op zichzelf geen verboden XML-naam, maar wijst in de praktijk vaak op onvoltooide koppeling.

### K19. Verwijzingen met bookmark-prefix

Typische bookmark-doelen van conversie vinden, niet alle interne links.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#bookmark[^"]*"|'#bookmark[^']*')
```

Oorspronkelijke testtekst:

```text
<a l:href="#bookmark12">Text</a>
```

Verwachte overeenkomst:

```text
l:href="#bookmark12"
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<a l:href="#note1">Text</a>
```


Beperkingen en opmerkingen: href="#..." vindt zonder extra beperking iedere interne verwijzing. bookmark bewijst niet dat de link overbodig is; controleer vóór verwijderen.

### K20. Word/FBD-terugverwijzingen

Gebruikelijke _ftnref en _ednref in verwijzingsdoelen vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#_(?:ftnref|ednref)[^"]*"|'#_(?:ftnref|ednref)[^']*')
```

Oorspronkelijke testtekst:

```text
<a l:href="#_ftnref1">Back</a>
```

Verwachte overeenkomst:

```text
l:href="#_ftnref1"
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<a l:href="#note1">Text</a>
```


Beperkingen en opmerkingen: Een teruglink kan voor navigatie nodig zijn. Verwijder hem niet alleen vanwege de herkomst van de naam.

### K21. Nootverwijzingen ongeacht attribuutvolgorde

Een openingstag a met type=note en interne XLink-href in iedere volgorde vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
<a(?=[ \t>])(?=[^>]*[ \t]type[ \t]*=[ \t]*(?:"note"|'note'))(?=[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+'))[^>]*>
```

Oorspronkelijke testtekst:

```text
<a l:href="#note1" type="note">1</a>
```

Verwachte overeenkomst:

```text
<a l:href="#note1" type="note">
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<a type="link" l:href="#note1">1</a>
```


Beperkingen en opmerkingen: Ook type vóór href, xlink en enkele quotes zijn toegestaan. Alle gecontroleerde attributen moeten op één fysieke regel staan. > in waarden, commentaar en afwijkende markup vragen XML-parsing. De doelnoot wordt niet gecontroleerd.

### K22. Mogelijke numerieke nootmarkeringen

Getallen tussen vierkante, ronde of accoladehaken vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
\[[0-9]+\]|\{[0-9]+\}|\([0-9]+\)
```

Oorspronkelijke testtekst:

```text
<p>Text [12], {3}, (4).</p>
```

Verwachte overeenkomsten, achtereenvolgens:

```text
[12]
{3}
(4)
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<p>[note]</p>
```


Beperkingen en opmerkingen: Dit kunnen literatuurverwijzingen, formulenummers, toelichtingen of gewone tekst zijn. Het patroon maakt geen noten en controleert geen unieke nummering.

### K23. Achtergebleven Your/Name-waarden

Typische Engelse tijdelijke namen in metadata controleren.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
<first-name>Your</first-name>|<last-name>Name</last-name>
```

Oorspronkelijke testtekst:

```text
<first-name>Your</first-name>
```

Verwachte overeenkomst:

```text
<first-name>Your</first-name>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<first-name>John</first-name>
```


Beperkingen en opmerkingen: Controleer auteur/makercontext en echte naam. Vervang niet automatisch door gegevens van een andere editie.

### K24. Ongekoppelde afbeelding #undefined

image vinden die naar een gebruikelijke placeholder verwijst.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
<image(?=[ \t/>])[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')[^>]*>
```

Oorspronkelijke testtekst:

```text
<image l:href="#undefined"/>
```

Verwachte overeenkomst:

```text
<image l:href="#undefined"/>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<image l:href="#cover"/>
```


Beperkingen en opmerkingen: FB2 gebruikt doorgaans XLink-href, niet HTML-src. Controleer binary met het juiste ID afzonderlijk. Een ontbrekende afbeelding en verkeerde link zijn andere oorzaken.

### K25. windows-1251-declaratie

Een oude codering in de XML-declaratie vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
<\?xml[ \t]+[^?]*encoding[ \t]*=[ \t]*(?:"windows-1251"|'windows-1251')[^?]*\?>
```

Oorspronkelijke testtekst:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Verwachte overeenkomst:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<?xml version="1.0" encoding="utf-8"?>
```


Beperkingen en opmerkingen: Niet vanzelf een fout. windows-1251 tekstueel in utf-8 veranderen codeert geen bytes om. Gebruik een opslag- of conversiehandeling die bij de declaratie past.

### K26. Ampersand zonder bekende standaardentiteit

& vinden vóór een reeks die niet op de standaardvormen lijkt.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
&(?!(?:amp|lt|gt|apos|quot);|#[0-9]+;|#x[0-9A-Fa-f]+;)
```

Oorspronkelijke testtekst:

```text
<p>A & B</p>
```

Verwachte overeenkomst:

```text
&
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<p>A &amp; B</p>
```


Beperkingen en opmerkingen: & mag in CDATA en commentaar staan; een DTD kan andere entiteiten definiëren. Dit is een filter, geen XML-validator. Blind & → &amp; geeft dubbele escaping.

### K27. Gewone interne verwijzingen

Een lokaal #id-doel vinden zonder dit met bookmark te verwarren.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+')
```

Oorspronkelijke testtekst:

```text
<a l:href="#note1">1</a>
```

Verwachte overeenkomst:

```text
l:href="#note1"
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<a l:href="https://example.test">Text</a>
```


Beperkingen en opmerkingen: Regex toont de verwijzing, niet dat precies één doel-id bestaat. Ontbrekende of dubbele doelen vragen documentanalyse.

### K28. Twee empty-line-elementen scheiden na een zoekopdracht op één regel

Het verschil tonen tussen een zoekbeperking en regeleinden invoegen bij vervangen.

Actie: één gecontroleerde vervanging.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(<empty-line[ \t]*/>)[ \t]*(<empty-line[ \t]*/>)
```

Oorspronkelijke testtekst:

```text
<empty-line/> <empty-line/>
```

Vervangen door:

```text
\1\r\n\2
```

Resultaat van de vervanging:

```text
<empty-line/>
<empty-line/>
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<empty-line/>
<empty-line/>
```


Beperkingen en opmerkingen: Scintilla werkt de vervangingsreeksen uit tot CRLF. Gebruik \1\n\2 voor een LF-document. Dit is geen algemene XML-formatter en schakelt geen meerregelig zoeken in.

### K29. Dubbele spaties in eenvoudige XML-tekst

Een tekststuk tussen tags met meerdere gewone spaties vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
>[^<]*[ \t]{2,}[^<]*<
```

Oorspronkelijke testtekst:

```text
<p>one  two</p>
```

Verwachte overeenkomst:

```text
>one  two<
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
<p>one two</p>
```


Beperkingen en opmerkingen: De treffer bevat hoekdelimiters en het hele eenvoudige fragment. Voor woorden is Ontwerp vaak beter. Vervang het volledige XML-fragment niet door één spatie.

## 12. Waarom regex XML-validatie niet vervangt

Een expressie kan tonen dat tekst op de bedoelde tekenreeks lijkt. Ze bewijst niet dat het hele document klopt: nesting, FB2-schema, namespaces, unieke ID’s en bestaande verwijzingsdoelen.

Twee gelijke `id`-waarden op verschillende fysieke regels zijn niet betrouwbaar met één huidige regexvergelijking te ontdekken. Ook een ontbrekend doel elders in het boek vraagt FBE-controle en structurele analyse.

Decodeer niet alle XML-entiteiten: `&lt;` en `&amp;` beschermen tekst vaak tegen verandering in markup. Verwijder niet alles wat op HTML lijkt uit CDATA, commentaar of codecitaten.

Een tagvervanging moet paren, attributen en inhoud behouden. Alleen `<strong>` wijzigen laat de oude sluiting staan. Een voorbeeld met terugverwijzing maakt niet iedere vervolgbewerking structureel veilig.

## 13. Veelvoorkomende fouten

### 13.1. Kleine tags worden gevonden in plaats van hoofdletters

Schakel Hoofdlettergevoelig in. Voor XML is dit niet cosmetisch: `[A-Z]` met genegeerde hoofdletters verliest zijn betekenis als controle op hoofdletters.

### 13.2. Er verschijnt letterlijk $1 in de vervanging

Gebruik `\1`. Source-vervanging loopt via Scintilla, niet via JavaScript of de gewone `$1` van een andere API.

### 13.3. \p, \K of lookbehind werkt niet

Dit hoort bij een ander profiel. Gebruik Ontwerp voor tekstwerk of herschrijf XML-patronen met expliciete klassen, contextcaptures, niet-vastleggende groepen en lookahead.

### 13.4. Twee aangrenzende tags worden niet gevonden

Controleer fysieke regeleinden, spaties voor `/>`, enkele quotes, extra attributen en namespaceprefixen. Aangrenzend in opgemaakte bron is niet hetzelfde als aangrenzend in de XML-boom.

### 13.5. Een noot wordt niet gevonden bij andere attribuutvolgorde

“Eerst type, dan href” is volgordeafhankelijk. K21 controleert beide kenmerken met afzonderlijke lookahead. Staat de tag op meerdere regels, zoek dan het attribuut of gebruik de documentstructuur.

### 13.6. strong/emphasis wordt als foutieve nesting gezien

Verschillende geldige stijlen mogen genest zijn. Gebruik een terugverwijzing zoals K08 om herhaling van dezelfde naam te controleren, niet twee onafhankelijke groepen met dezelfde alternatieven.

### 13.7. “Bookmark” vindt alle nootverwijzingen

Een interne `#`-link is niet automatisch een bookmarkrest. Houd algemene interne links K27 gescheiden van de specifieke prefix K19.

### 13.8. De vervanging beschadigt de zichtbare tekst

In Code kunnen spaties en fragmenten in tekst, attributen, commentaar of binaire gegevens staan. Maak de wijziging ongedaan en verklein bereik of patroon. Een sjabloonnaam “veilige vervanging” heft de XML-context niet op.

## 14. Beperkingen van het Source-profiel

Gebruik geen UCP, PCRE2-eigenschappen, lookbehind, moderne JavaScript/PCRE2-named groups, atomaire groepen, possessieve kwantoren, branch reset, subroutines, PCRE2-verbs, `\K`, `\G` of opmaakopdrachten uit Design-vervanging.

PCRE2-inlineopties vervangen niet de selectievakjes van Code. De nieuwste browser-JavaScriptfuncties zijn niet automatisch in C++11-ECMAScript aanwezig.

MatchOnLines blijft regelgewijs werken, ongeacht de genoteerde regeleindesequentie. Succesvol compileren met een andere engine bevestigt geen FBE-compatibiliteit.

Gebruik Ontwerp voor Unicode-woorden en ingewikkelde boekcorrectie, en XML/FB2-functies voor structuur. Probeer niet alles met één onbeperkte `.*` op te lossen.

## 15. Prestaties en lange XML-regels

Een kort patroon is niet noodzakelijk snel. Onbegrensde herhalingen en lange concurrerende alternatieven kunnen enorme regels vertragen, zeker wanneer één regel het hele document of veel binary bevat.

Begin waar mogelijk met een concrete tag- of attribuutnaam en gebruik begrensde klassen. Afzonderlijke recepten zijn gemakkelijker te controleren en ongedaan te maken dan alle mogelijke fouten tegelijk zoeken.

Voeg niet alle bronregels samen om MatchOnLines te omzeilen. Dat verandert het document en garandeert geen aanvaardbare snelheid.

## 16. Controle na de vervanging

Vergelijk verwacht en werkelijk matchbereik. Controleer bij groepen quotes, prefixen, hoekhaken en sluitende tags. Let erop dat commentaar, CDATA en binary niet onbedoeld zijn gewijzigd.

Valideer FB2, sla op en heropen na belangrijke wijzigingen. Controleer Code ↔ Ontwerp, voetnoten, afbeeldingen en zichtbare tekst. Controleer bij ID-hernoeming zowel het object als alle gekoppelde verwijzingen.

## 17. Bronnen en geldigheidsgrenzen

Deze handleiding breidt het aangeleverde regex-source.md uit. De volgorde engine → syntaxis → vervanging → XML → beperkingen blijft behouden, maar te brede recepten zijn aangescherpt. Toelichtingen en tests zijn herschreven en technische verschillen met primaire bronnen gecontroleerd.

[S1] Officiële Scintilla-documentatie, Searching: C++11-modus, zoekvlaggen en SCI_REPLACETARGETRE.

`https://www.scintilla.org/ScintillaDoc.html#Searching`

[S2] Scintilla in FBE Next-revisie d2257405d95b0328649acee64b38829b40a4314b: Document.cxx, Cxx11RegexFindText, MatchOnLines, BuiltinRegex::SubstituteByPosition. Dit onderscheidt over regels zoeken van CR/LF invoegen bij vervanging.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/third_party/scintilla/src/Document.cxx`

[S3] W3C XML 1.0 en Namespaces in XML: namen, attributen, entiteiten en namespaces.

`https://www.w3.org/TR/xml/`

`https://www.w3.org/TR/xml-names/`
