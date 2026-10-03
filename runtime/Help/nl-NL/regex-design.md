# Handleiding voor reguliere expressies — Ontwerp

Opmerking bij de vertaling: Russische woorden en zinnen in de controlevoorbeelden zijn bewust ongewijzigd gebleven. Het zijn testgegevens: reguliere expressies, vervangteksten, witruimtetekens en verwachte resultaten komen overeen met de Russische referentieversie. De uitleg is vertaald; de voorbeelden zijn niet automatisch aangepast aan Nederlandse typografische regels.

Een volledige handleiding voor zoeken, vervangen en het corrigeren van boeken in FictionBook Editor Next.

Editie: 2 oktober 2026. De modus Code wordt afzonderlijk beschreven in regex-source.md. “Patroon” betekent hier een reguliere expressie; een “ingebouwd sjabloon” is een opgeslagen combinatie van expressie en opties in het paneel Sjablonen. Design is de visuele modus Ontwerp.

## 1. Deze handleiding gebruiken

Een reguliere expressie beschrijft een zoekregel in plaats van één exacte tekenreeks. `[0-9]+` vindt bijvoorbeeld een reeks cijfers van willekeurige lengte; `[ \t]{2,}` vindt twee of meer gewone spaties of tabs. De gevonden overeenkomst en de tekst die ervoor in de plaats komt, zijn verschillende zaken.

Hoofdstuk 2 geeft een eerste praktisch resultaat. Hoofdstukken 3–15 behandelen de syntaxis, hoofdstuk 16 de eigen vervangingsgrammatica van FBE. Hoofdstuk 17 bevat recepten voor boekbewerking, met voorwaarden, testteksten en waarschuwingen. Daarna volgen probleemoplossing, prestaties en bronnen.

Kopieer alleen de expressie naar het zoekveld. De labels “Zoeken”, “Vervangen”, aanduidingen als U+0020 en Markdown-backticks horen niet bij het patroon. JavaScript-omsluitingen `/.../g`, aanhalingstekens van C++-strings en verdubbelde backslashes uit JSON zijn niet nodig.

De selectievakjes zijn onderdeel van het recept. Een resultaat zonder hoofdlettergevoeligheid hoeft niet hetzelfde te blijven wanneer die optie wordt ingeschakeld. Beschouw instellingen niet als bijkomstige opmaak.

“Alleen zoeken en controleren” betekent een diagnostisch recept. De treffer kan correct zijn: een bewuste herhaling, buitenlandse naam, citaat, kop of typografische keuze. Ook “vervangen na controle” garandeert geen veiligheid in elk boek.

## 2. Eerste zoekopdracht en veilige vervanging

Sla een werkkopie op. Ga naar Ontwerp, open Zoeken of Vervangen en schakel Reguliere expressie in. Zet voor de eerste proeven Alleen hele woorden uit: leg grenzen liever in het patroon vast. Controleer zoekbereik en richting.

Zoek meerdere spaties met:

```regex
[ \t]{2,}
```

In `Он   пришёл` wordt de tussenruimte van drie spaties gevonden. Zet één gewone spatie in het vervangingsveld. Voer eerst één vervanging uit, controleer `Он пришёл` en overweeg pas daarna Alles vervangen.

Bekijk bij veel treffers eerst de resultaten en corrigeer één of twee representatieve gevallen. Controleer na een bulkbewerking tekst, cursief, vet, voetnoten en alineagrenzen. Maak een onverwacht resultaat ongedaan voordat u verder bewerkt.

Toepassen in het sjabloonpaneel zet de expressie en opties in het zoekvenster. Het is geen opdracht om alle treffers zonder meer te corrigeren. Zoeken en vervangen hebben hun eigen opdrachten in het dialoogvenster.

## 3. Engine en grenzen van de modus Ontwerp

Deze modus gebruikt PCRE2-16 met patronen en tekst in UTF-16; UTF staat altijd aan. FBE bouwt de doorzoekbare tekst afzonderlijk op en voert vervanging uit met inachtneming van de boekstructuur. PCRE2-documentatie verklaart de overeenkomsten, niet alle handelingen van FBE. Technische bronnen staan bij D1–D4.

Het zoekobject is een tekstweergave van het boek, niet de letterlijke XML. `<strong>` is geen manier om vette tekst in Ontwerp te vinden. Zoek XML-tags in Code; voer structurele wijzigingen uit met editorfuncties of gespecialiseerde scripts.

Een lange regel die visueel wordt omgebroken bevat daardoor nog geen regeleindeteken. Alinea’s, echte onderbrekingen en visuele terugloop zijn verschillende dingen. FBE gebruikt meerregelige ankers in de zoekweergave om begin en einde van tekstregels aan alineagrenzen te koppelen. Een overeenkomst over alineagrenzen en zo’n overeenkomst vervangen zijn niet hetzelfde: de onderzochte implementatie weigert vervanging tussen alinea’s.

`\A` en `\z` verwijzen naar begin en einde van de string die de engine ontvangt. Dat is niet automatisch het volledige FB2-bestand; zoekbereik en het door FBE opgebouwde fragment tellen mee.

## 4. Unicode: UTF, UCP en hoofdletters

### 4.1. Wat UTF doet

UTF laat de engine Unicode-tekens verwerken, ook Cyrillisch. Het converteert geen alfabetten, corrigeert geen OCR, maakt van `ё` geen `е` en voegt canoniek equivalente tekenreeksen niet automatisch samen.

Een geaccentueerde letter kan als één teken of als letter plus gecombineerd accent zijn opgeslagen. De weergave kan gelijk zijn terwijl een tekengewijze zoekopdracht verschilt. PCRE2 voert geen NFC/NFD-normalisatie voor u uit. Houd bij diakritische tekens rekening met `\p{M}`.

### 4.2. Wat UCP doet

Unicode (UCP) verandert vooral de verkorte klassen `\w`, `\d`, `\s` en de daarvan afhankelijke grenzen `\b` en `\B`. Het is niet de schakelaar die Cyrillisch zelf mogelijk maakt.

Een verduidelijking van eerdere versies: expliciete eigenschappen `\p{L}`, `\p{N}` en `\P{...}` zijn in een Unicode-build van PCRE2 ook zonder UCP beschikbaar. Combineert een patroon `\p{L}` met `\b`, dan is UCP doorgaans wenselijk voor een consistente interpretatie van letters en woordgrenzen.

Vergelijk een Russisch woord:

```regex
\bмир\b
```

Met UCP volgen de grenzen de Unicode-woordklasse. Verwacht zonder UCP niet dat `\b` zich voor Cyrillisch hetzelfde gedraagt als voor Latijnse ASCII-tekst. Geen treffer betekent hier niet noodzakelijk dat het woord ontbreekt.

Gebruik `[0-9]` wanneer specifiek ASCII-cijfers nodig zijn, niet `\d`: UCP kan die laatste klasse uitbreiden met decimale cijfers uit andere schriften.

### 4.3. Bruikbare eigenschappen

| Expressie | Wat wordt gevonden |
| --- | --- |
| `\p{L}` | Een Unicode-letter |
| `\p{Lu}` | Een hoofdletter bij hoofdlettergevoelig zoeken |
| `\p{Ll}` | Een kleine letter bij hoofdlettergevoelig zoeken |
| `\p{M}` | Een gecombineerd teken |
| `\p{N}` | Een numeriek teken, ruimer dan een decimaal cijfer |
| `\p{Nd}` | Een decimaal cijfer |
| `\p{Latin}` | Een teken uit het Latijnse schrift |
| `\p{Cyrillic}` | Een teken uit het Cyrillische schrift |
| `\P{L}` | Een teken dat geen letter is |

Een reeks letters inclusief accenttekens:

```regex
[\p{L}\p{M}]+
```

Dit is nog geen universele taalkundige definitie van een woord: koppeltekens en apostroffen ontbreken. Voeg die bewust toe.

### 4.4. Hoofdlettergevoeligheid is een aparte instelling

Schakel Hoofdlettergevoelig in voor afwijkingen zoals `строчнаяПрописная`, initialen en patronen met `\p{Lu}`/`\p{Ll}`. Hoofdletters negeren kan de betekenis van de regel tenietdoen. UCP vervangt deze instelling niet.

Hoofdletters negeren vanuit de expressie:

```regex
(?i)глава
```

Beperkt tot een groep:

```regex
(?i:глава)[ \t]+[0-9]+
```

## 5. Letterlijke tekens en escaping

Letters en de meeste tekens staan voor zichzelf. Buiten een tekenklasse kunnen punt, haakjes, sterretje, plus, vraagteken, accolades, ankers en backslash een speciale betekenis hebben.

Zet een backslash voor een metateken om het letterlijk te zoeken:

| Gezochte tekst | Expressie |
| --- | --- |
| Punt | `\.` |
| Vraagteken | `\?` |
| Plus | `\+` |
| Sterretje | `\*` |
| Openend rond haakje | `\(` |
| Sluitend rond haakje | `\)` |
| Getal tussen vierkante haken | `\[[0-9]+\]` |
| De backslash zelf | `\\` |

`(123)` vindt de cijfers `123` en bewaart ze in een groep, maar eist geen haakjes in de tekst. Gebruik ge-escapete haakjes om `(123)` letterlijk te vinden.

Een lang letterlijk fragment kan in PCRE2 tussen `\Q` en `\E` staan:

```regex
\QЦена (руб.) + доставка\E
```

In het vervangingsveld van FBE hebben deze tekens een andere betekenis. Neem zoekescaping niet automatisch over voor vervanging.

## 6. Spaties, tabs en onzichtbare tekens

| Expressie | Betekenis bij PCRE2-zoeken |
| --- | --- |
| `[ ]` | Alleen gewone spatie U+0020 |
| `[ \t]` | Gewone spatie of tab |
| `\t` | Tab U+0009 |
| `\x{00A0}` | Vaste spatie |
| `\x{202F}` | Smalle vaste spatie |
| `\h` | Horizontale witruimte, inclusief diverse Unicode-spaties |
| `\s` | Witruimte, mogelijk ook regeleinden |
| `\r` | Carriage return CR |
| `\n` | Line feed LF |
| `\R` | Unicode-regeleindesequentie |
| `\x{00AD}` | Zacht afbreekteken |
| `\x{200B}` | Spatie met nulbreedte |
| `\x{FEFF}` | FEFF in de tekst zelf |

Kies `[ \t]` voor het opschonen van normale woordspaties, niet `\s`. Die laatste klasse kan ook alineagrenzen en vaste spaties raken. `\h` is nuttig voor diagnose maar te ruim voor onvoorwaardelijke typografische vervanging.

Een vaste spatie verbindt teken en nummer, initialen en achternaam, of getal en eenheid. Alles naar gewone spaties omzetten kan de opmaak verslechteren. FBE kent bovendien een NBSP-tekeninstelling: controleer U+00A0-recepten tegen het boek en de gebruikte build.

U+200C en U+200D kunnen nodig zijn voor bepaalde schriften en samengestelde emoji. Verwijder niet blind alle “onzichtbare” tekens. Detectie is geen bewijs van een fout.

## 7. Tekenklassen en bereiken

Een klasse tussen vierkante haken verbruikt één teken uit de verzameling. `[abc]` is één van drie letters, niet het woord `abc`. `[abc]+` staat voor één of meer letters uit de verzameling.

`[^abc]` verbruikt één teken buiten de verzameling. Het betekent niet “er staat geen abc vóór de tekst”; daarvoor dienen contextcontroles.

Binnen een klasse verliezen punt en de meeste haakjes hun speciale betekenis. Een koppelteken kan een bereik aangeven. Zet het voor een letterlijke betekenis vooraan/achteraan of escape het. Een letterlijke sluitende vierkante haak kan als `\]`.

```regex
[А-Яа-яЁё-]+
```

Dit laat Russische letters en een koppelteken toe, niet heel Cyrillisch. Oekraïense, Wit-Russische en andere letters vragen een ruimere verzameling of Unicode-eigenschap.

Binnen een klasse is `\b` geen woordgrens. Voeg woordgrenzen niet toe aan een gewone tekenverzameling.

## 8. Ankers, grenzen en lege overeenkomsten

Een anker controleert een positie zonder een letter of spatie te verbruiken. `^`, `$` en `\b` kunnen daardoor een overeenkomst van nul tekens opleveren. Die ziet er niet uit als een normaal geselecteerd tekstfragment.

| Anker | Betekenis |
| --- | --- |
| `^` | Regelbegin met multiline, anders begin van het zoekobject |
| `$` | Regeleinde met multiline; de laatste regeleindeafhandeling is modusafhankelijk |
| `\A` | Alleen begin van het zoekobject |
| `\z` | Exact einde van het zoekobject |
| `\Z` | Einde of positie vóór het afsluitende regeleinde |
| `\b` | Grens tussen woordteken en niet-woordteken |
| `\B` | Positie die geen woordgrens is |
| `\G` | Startpositie van de huidige matchaanroep |

`\G` onthoudt niet zelfstandig “de vorige overeenkomst”. Het verwijst naar de startoffset die de toepassing doorgeeft. Bij opeenvolgende aanroepen kan dat het einde van een vorige treffer zijn, maar in FBE is dit geen universele manier om een boek te doorlopen. Gebruik bij gewone correctie expliciete grenzen. [D1]

Voor een heel woord in plaats van delen van langere woorden gebruikt u grenzen of controles op naburige letters. Met UCP:

```regex
\bтом\b
```

Dit past niet op het begin van `томик`. De engine kan koppeltekens en apostroffen echter anders als woordgrens behandelen dan een redacteur.

Gebruik lege overeenkomsten voorzichtig met Alles vervangen: de operatie voegt tekst op posities in, in plaats van zichtbare tekens te vervangen. Begin met niet-lege resultaten.

## 9. Kwantoren: herhaling en terugzoeken

| Notatie | Herhalingen van het vorige element |
| --- | --- |
| `?` | Nul of één |
| `*` | Nul of meer |
| `+` | Eén of meer |
| `{3}` | Precies drie |
| `{3,}` | Minstens drie |
| `{2,5}` | Twee tot vijf |

Een kwantor werkt op het voorafgaande teken, de klasse of de groep. `аб+` herhaalt alleen `б`; `(?:аб)+` herhaalt het paar.

### 9.1. Gulzig zoeken

Oorspronkelijke tekst:

```text
«первый» и «второй»
```

Patroon:

```regex
«.*»
```

De punt met sterretje neemt eerst zoveel mogelijk tekst en geeft zo nodig tekens terug om de rest van het patroon te laten passen. Hier omvat de treffer beide paren aanhalingstekens.

### 9.2. Niet-gulzig zoeken

```regex
«.*?»
```

Dit probeert eerst zo weinig mogelijk tekens, maar kan de overeenkomst alsnog uitbreiden. Achtereenvolgens worden `«первый»` en `«второй»` gevonden.

Voor een eenvoudig paar is een expliciete inhoudsgrens vaak duidelijker:

```regex
«[^»\r\n]*»
```

Dit verwerkt geen geneste citaten. Die vragen aparte redactionele controle.

### 9.3. Bezittelijke kwantor

```regex
«.*+»
```

Dit betekent niet “nog betere aanhalingstekens”. `.*+` slokt ook het sluitende teken op en geeft het niet terug. De resterende `»` in het patroon kan daardoor niet passen; er is geen resultaat.

Een variant die het sluitende teken uitsluit kan wel passend zijn:

```regex
«[^»\r\n]*+»
```

Bezittelijke kwantoren en atomaire groepen sturen backtracking; ze zijn geen universele versnelling.

## 10. Groepen en terugverwijzingen

Ronde haakjes bewaren een gevonden fragment. Nummering begint bij 1 en volgt openende capturehaakjes van links naar rechts, ook bij nesting.

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Voor `02.10.2026` bevatten de groepen `02`, `10`, `2026`. Dit controleert het formaat, niet de kalendergeldigheid.

`(?:...)` groepeert zonder groepsnummer. Dat houdt `$1` en `$2` voorspelbaar bij complexe vervangingen.

Een benoemde groep maakt een patroon leesbaarder:

```regex
(?<word>\p{L}+)[ \t]+\k<word>
```

Een terugverwijzing zoekt dezelfde vastgelegde tekst. Een subroutineaanroep herhaalt de regel en kan andere tekst vinden. Dit zijn verschillende mechanismen.

Voeg bij echte herhaalde woorden grenzen, hoofdletterinstelling en geschikte spaties toe. Zonder grenzen kan een verwijzing delen van langere woorden vinden. Een compleet recept staat verderop.

Benoemde zoekgroepen maken benoemde vervangingen niet automatisch beschikbaar in FBE. Gebruik bevestigde numerieke verwijzingen in het vervangingsveld.

## 11. Alternatieven en atomaire groepen

Een verticale streep kiest tussen alternatieven:

```regex
(?:глава|часть)[ \t]+[0-9]+
```

Groepering is belangrijk: zonder groep kan het gezamenlijke achterste deel alleen bij het laatste alternatief horen. Alternatieven worden in de geschreven volgorde geprobeerd. Kies de volgorde van lange en korte vormen bewust.

Een atomaire groep verbiedt terugkeer binnen een al succesvol afgelegde groep:

```regex
(?>а|аб)в
```

Voor `абв` kiest het eerste alternatief `а`. Terugkeer naar `аб` mag dan niet meer en er is geen overeenkomst. De niet-atomaire versie kan die wel vinden. Voeg atomiciteit dus niet zonder resultaatcontrole toe.

## 12. Naburige tekst controleren: lookaround

Lookaround controleert context die niet in de volledige overeenkomst wordt opgenomen. Zo kunt u alleen een nummer selecteren en het voorafgaande teken behouden:

```regex
(?<=№ )[0-9]+
```

Bij `№ 125` wordt alleen `125` gevonden. De spatie is gewoon en enkelvoudig.

Controle rechts:

```regex
[0-9]+(?=[ \t]+руб\.)
```

In `125 руб.` vindt dit het getal zonder `руб.`.

Negatieve controle vooruit:

```regex
\bглава\b(?![ \t]+[0-9])
```

Met UCP wordt “глава” gevonden wanneer er geen gewone nummering na een spatie volgt. Het is een voorbeeld van contextselectie, geen correctieregel.

`(?<=...)` controleert voorafgaande tekst en `(?<!...)` ontkent die voorwaarde. `(?=...)` en `(?!...)` doen hetzelfde voor volgende tekst.

Lookbehind kent lengtegrenzen. Moderne PCRE2 ondersteunt sommige begrensde variabele lengtes, niet willekeurige onbegrensde herhaling. Gebruik voor overdraagbare recepten korte vaste context, een capture of `\K`; reken niet op `.*` in lookbehind.

## 13. Opties binnen het patroon

| Optie | Effect |
| --- | --- |
| `(?i)` | Negeert hoofdletters |
| `(?-i)` | Onderscheidt hoofdletters |
| `(?m)` | Meerregelige begin- en eindankers |
| `(?-m)` | Schakelt meerregelige ankers uit |
| `(?s)` | De punt mag een regeleinde omvatten |
| `(?-s)` | Herstelt het normale puntgedrag |
| `(?x)` | Negeert niet-betekenisvolle spaties en commentaar in het patroon |

`(?m)` laat de punt geen regels oversteken. `(?s)` staat FBE niet toe tussen alinea’s te vervangen. Het zijn aparte zoekopties en een afzonderlijke toepassingsbeperking.

In uitgebreide modus zijn spaties in het patroon soms niet meer letterlijk. Gebruik `[ ]` voor een vereiste spatie; `#` kan buiten een klasse commentaar starten. Meerregelige patronen zijn prettig in documentatie, maar de recepten voor het zoekveld staan op één regel.

## 14. Geavanceerde PCRE2-functies

De meeste correcties vereisen dit hoofdstuk niet. Het helpt patronen van anderen te begrijpen. Syntactische ondersteuning maakt niet elk patroon handig of veilig voor een heel boek.

### 14.1. Het begin van de overeenkomst resetten

```regex
№[ \t]*\K[0-9]+
```

In `№ 125` wordt het voorvoegsel gecontroleerd, maar alleen `125` teruggegeven. Nuttig wanneer het getal moet veranderen, niet het teken. Gebruik `\K` niet ongecontroleerd binnen lookaround: PCRE2 beperkt die combinatie.

### 14.2. Voorwaarde op deelname van een groep

```regex
^(\()?([0-9]+)(?(1)\))$
```

Dit accepteert `12` en `(12)`, niet het ongesloten `(12`. De voorwaarde controleert deelname van groep 1. Maak hiervan geen vervanging met optionele groepen zonder de FBE-adapter te testen.

### 14.3. Gedeelde nummering tussen alternatieven

```regex
(?|глава ([0-9]+)|часть ([0-9]+))
```

In beide takken komt het nummer in groep 1: branch reset. In eenvoudige gevallen is een niet-vastleggende groep met één gezamenlijke capture duidelijker.

### 14.4. Een subroutine aanroepen

```regex
(?<pair>[0-9]{2})-(?&pair)
```

Bij `12-34` voldoen beide delen aan “twee cijfers”, hoewel de getallen verschillen. Een verwijzing `\k<pair>` zou precies `12` opnieuw vereisen.

Recursieve subroutines beschrijven sommige geneste structuren, maar vervangen de XML-parser niet. Gebruik voor `<section>`, `<poem>`, noten en tabellen structurele functies.

### 14.5. Fragmenten overslaan

```regex
«[^»\r\n]*»(*SKIP)(*FAIL)|\bслово\b
```

Met UCP wordt het woord buiten eenvoudige Russische aanhalingstekens gezocht. De eerste tak markeert het citaat om over te slaan, de tweede zoekt het woord. Geneste of ongesloten aanhalingstekens worden niet verwerkt; vertrouw hier niet blind op bij bulkredactie.

## 15. Wat een reguliere expressie niet beslist

Een hoofdletter na een kleine letter, ontbrekende eindinterpunctie en een herhaald woord zijn formele aanwijzingen, geen redactionele beslissing. Regex weet niet of `да да` een fout, opzettelijke dialoog, vers of kop is.

Voeg niet automatisch punten toe, plak niet alle alinea’s met een kleine beginletter aan elkaar, verander niet elke Latijnse letter in Cyrillisch en elk koppelteken in een gedachtestreep. Context en soms meerdere DOM-stappen zijn nodig. FBE-scripts voor opschonen, vastgeplakte woorden en noten lossen zulke complexere taken op.

## 16. De vervangingsgrammatica van FBE

PCRE2 zoekt, maar FBE verwerkt de vervangende tekenreeks. Neem voorbeelden uit JavaScript, Python, .NET, PCRE2 substitute of andere editors niet zonder test over. Hieronder staan opdrachten die door broncode en oorspronkelijke FBE-documentatie zijn bevestigd. [D3, D4]

### 16.1. Overeenkomst en groepen

| In het vervangingsveld | Betekenis |
| --- | --- |
| `$0` of `\0` | Hele overeenkomst |
| `$1` … `$9` | Groep met het betreffende nummer |
| `\1` … `\9` | Alternatieve numerieke verwijzing |
| `$+` of `\+` | Laatste groep die de matchadapter teruggeeft |

“FBE vervangt alleen groepen 1–9” is onvolledig: gewone tekst en de gehele overeenkomst kunnen ook worden ingevoegd. De beperking betreft directe numerieke toegang, niet het aantal PCRE2-groepen.

Gebruik `$10` niet als verwijzing naar groep tien. `${name}` behoort niet tot de bevestigde grammatica. Houd benodigde captures bij de eerste negen groepen; maak hulpgroepen niet-vastleggend.

Test optionele en lege groepen apart: FBE heeft een eigen SubMatches-adapter. Laat nieuwe bulkvervangingen liever niet afhangen van subtiele regels voor overgeslagen groepen of de “laatste groep”; gebruik expliciete verplichte captures.

### 16.2. Groepen opnieuw ordenen

Zoeken:

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Vervangen door:

```text
$3-$2-$1
```

`02.10.2026` wordt `2026-10-02`. Dit demonstreert herordening en is geen advies om alle datums in een boek om te zetten.

### 16.3. Hoofdletters en kleine letters veranderen

| Opdracht | Effect |
| --- | --- |
| `\U` | Zet het ingevoegde fragment in hoofdletters |
| `\L` | Zet het in kleine letters |
| `\T` | Eerste letter van het fragment hoofdletter, de rest klein |
| `\Q` | Reset actieve letterkast- en opmaakopdrachten voor volgende tekst |

Zoeken:

```regex
(иван)
```

Vervangen door:

```text
\T$1\Q
```

Controleresultaat: `Иван`. `\T` is geen uitgebreide taalkundige omzetting van elk woord: één fragment `иван иванов` hoeft niet `Иван Иванов` te worden.

Stapel `\U` en `\L` niet zonder reset. Scheid delen met `\Q` en controleer Cyrillisch en diakritische tekens. FBE past de letterkast aan, niet een PCRE2-Unicode-normalisator.

### 16.4. Vet en cursief

`\S` schakelt vet in voor het ingevoegde fragment, `\E` cursief. `\Q` beëindigt de actieve opdrachten voor volgende tekst.

```text
\S$1\Q
```

Dit maakt de inhoud van groep 1 op. Het zoekt niet naar al vette tekst en wist niet alle bestaande opmaak. Test eerst één fragment en het ongedaan maken.

### 16.5. Dezelfde tekens met verschillende betekenissen

In een zoekpatroon is `\S` een niet-witruimteteken; in FBE-vervanging betekent het vet. `\Q...\E` maakt zoektekst letterlijk, maar in vervanging reset `\Q` opdrachten en schakelt `\E` cursief in.

Typ geen `\n`, `\t`, `\x{00A0}`, `\$` of `$$` in Design-vervanging in de verwachting dat een andere editor dezelfde regels heeft. Ze behoren niet tot de beschreven letterlijke grammatica; onbekende reeksen kunnen verdwijnen. Gebruik het echte NBSP-teken. Bewaar een speciaal teken via een gevonden groep of een gecontroleerde gewone vervanging.

Een leeg veld verwijdert de treffer. Eén spatie vervangt die door één spatie. Labels zoals `<пусто>`, `[NBSP]` en `U+00A0` uit de toelichting moet u niet letterlijk typen.

## 17. Praktische recepten voor boekbewerking en correctie

De voorbeelden zijn zelfstandige scenario’s en hoeven niet exact overeen te komen met ingebouwde sjabloonnamen. Pas een vervanging eerst op één treffer toe. Echte spaties en tabs in de testblokken blijven behouden; negatieve voorbeelden tonen minstens één grens, maar vervangen geen controle van het hele boek.

### D01. Meerdere gewone spaties terugbrengen tot één

Gewone tussenruimtes verdichten zonder één afzonderlijke NBSP te wijzigen.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[ \t]{2,}
```

Oorspronkelijke testtekst:

```text
Он   пришёл.
```

Vervangen door: één gewone spatie U+0020. Dit is één teken, niet het woord “spatie”.

Resultaat van de vervanging:

```text
Он пришёл.
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Он пришёл.
```


Beperkingen en opmerkingen: In poëzie, tabellen en nagebootste inspringing kunnen meerdere spaties bewust zijn. Controleer het zoekbereik; dit herbouwt geen alinea-inspringing.

### D02. Spaties aan het begin van een alinea

Handmatige inspringing vóór gewone tekst verwijderen.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
^[ \t]+
```

Oorspronkelijke testtekst:

```text
   Начало абзаца.
```

Dezelfde testtekst met zichtbare aanduidingen:

```text
␠␠␠Начало␠абзаца.
```

Hier staat ␠ voor een gewone spatie, [TAB] voor een tab, [NBSP] voor U+00A0, [NNBSP] voor U+202F en [ZWSP] voor U+200B. Dit is een toelichtende weergave; deze labels worden niet in het boek ingevoegd.

Vervangen door: laat het veld volledig leeg. Typ niet het woord “leeg”.

Resultaat van de vervanging:

```text
Начало абзаца.
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Начало абзаца.
```


Beperkingen en opmerkingen: Bij poëzie en artistieke opmaak kan inspringing betekenis hebben. Het anker betreft de tekstregel, niet de visuele terugloop.

### D03. Spaties aan het einde van een alinea

Een staart van gewone spaties of tabs verwijderen.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[ \t]+$
```

Oorspronkelijke testtekst:

```text
Конец абзаца.
```

Dezelfde testtekst met zichtbare aanduidingen:

```text
Конец␠абзаца.␠␠␠
```

Hier staat ␠ voor een gewone spatie, [TAB] voor een tab, [NBSP] voor U+00A0, [NNBSP] voor U+202F en [ZWSP] voor U+200B. Dit is een toelichtende weergave; deze labels worden niet in het boek ingevoegd.

Vervangen door: laat het veld volledig leeg. Typ niet het woord “leeg”.

Resultaat van de vervanging:

```text
Конец абзаца.
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Конец абзаца.
```


Beperkingen en opmerkingen: NBSP is bewust uitgesloten. Gebruik afzonderlijke diagnose voor afwijkende spaties.

### D04. Spatie vóór komma en andere leestekens

Het leesteken bewaren en de voorafgaande ruimte verwijderen.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[ \t]+([,;:!?])
```

Oorspronkelijke testtekst:

```text
Слово , другое !
```

Vervangen door:

```text
$1
```

Resultaat van de vervanging:

```text
Слово, другое!
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Слово, другое!
```


Beperkingen en opmerkingen: Deze regel is bedoeld voor gewone Russische tekst. Franse typografie staat bijzondere spaties vóór sommige tekens toe; pas hem niet zonder aanpassing op zulke boeken toe.

### D05. Spatie na een openingsteken

Een gewone spatie na een haakje of Russisch openingsaanhalingsteken verwijderen.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
([(\[«„])[ \t]+
```

Oorspronkelijke testtekst:

```text
« слово» ( пример)
```

Vervangen door:

```text
$1
```

Resultaat van de vervanging:

```text
«слово» (пример)
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
«слово» (пример)
```


Beperkingen en opmerkingen: Spaties in formules of voorbeelden blijven ongemoeid als ze niet direct na een genoemd teken staan. Controleer de context toch.

### D06. Spatie vóór een sluitingsteken

Een gewone spatie vóór haakje of sluitend aanhalingsteken verwijderen.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[ \t]+([)\]»”])
```

Oorspronkelijke testtekst:

```text
«слово » (пример )
```

Vervangen door:

```text
$1
```

Resultaat van de vervanging:

```text
«слово» (пример)
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
«слово» (пример)
```


Beperkingen en opmerkingen: Dit normaliseert niet alle soorten aanhalingstekens en wiskundige haakjes.

### D07. Precies drie punten vervangen door een beletselteken

Drie opeenvolgende punten omzetten en langere reeksen behouden.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?<!\.)\.{3}(?!\.)
```

Oorspronkelijke testtekst:

```text
Он подумал... и ответил.
```

Vervangen door:

```text
…
```

Resultaat van de vervanging:

```text
Он подумал… и ответил.
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Содержание.....12
```


Beperkingen en opmerkingen: Controleer het redactionele beleid. Vier punten en opvulpunten in inhoudsopgaven blijven bewust buiten beschouwing.

### D08. Uit elkaar geschreven beletselteken

Drie punten met gewone spaties ertussen samenvoegen.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?<!\.)\.[ \t]*\.[ \t]*\.(?!\.)
```

Oorspronkelijke testtekst:

```text
Он подумал. . . и ответил.
```

Vervangen door:

```text
…
```

Resultaat van de vervanging:

```text
Он подумал… и ответил.
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Слово. Другое.
```


Beperkingen en opmerkingen: Vindt ook drie aansluitende punten. Niet gebruiken voor opvulregels of weglatingen in citaten met een eigen conventie.

### D09. Vaste spatie na №

Het nummerteken aan het volgende getal koppelen.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
№[ \t]+([0-9]+)
```

Oorspronkelijke testtekst:

```text
№ 125
```

Vervangen door:

```text
№ $1
```

Resultaat van de vervanging:

```text
№ 125
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
№125
```


Beperkingen en opmerkingen: Tussen № en $1 in de vervanging staat echt U+00A0. Vervang dit niet door de afgedrukte reeks \x{00A0}. Het patroon voegt geen geheel ontbrekende spatie toe.

### D10. Vaste spatie na §

Het paragraafteken aan het nummer koppelen.

Actie: vervangen na controle.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
§[ \t]+([0-9]+)
```

Oorspronkelijke testtekst:

```text
§ 12
```

Vervangen door:

```text
§ $1
```

Resultaat van de vervanging:

```text
§ 12
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
§12
```


Beperkingen en opmerkingen: Tussen § en $1 staat een echte NBSP. Controleer bij samengestelde nummering of het juiste deel is gevonden.

### D11. Mogelijk herhaald aangrenzend woord

Herhaling over horizontale spaties vinden zonder alinea’s over te slaan.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” uit.

Zoeken:

```regex
\b(\p{L}+)[ \t\x{00A0}]+\1\b
```

Oorspronkelijke testtekst:

```text
Это это уже было.
```

Verwachte overeenkomst:

```text
Это это
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Это уже было.
```


Beperkingen en opmerkingen: «Да да» en vergelijkbare herhalingen kunnen van de auteur zijn. Verwijder niet automatisch het tweede woord; misschien is een komma nodig of helemaal geen wijziging. Apostroffen en koppeltekens zijn niet volledig verwerkt.

### D12. Latijns en Cyrillisch binnen hetzelfde woord

Een OCR-mengvorm binnen één doorlopend woord vinden, niet iedere tweetalige regel.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?<![\p{L}\p{M}])(?=[\p{L}\p{M}]*\p{Latin})(?=[\p{L}\p{M}]*\p{Cyrillic})[\p{L}\p{M}]+(?![\p{L}\p{M}])
```

Oorspronkelijke testtekst:

```text
В слове Тeст латинская e.
```

Verwachte overeenkomst:

```text
Тeст
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Он прочитал Latin.
```


Beperkingen en opmerkingen: In Тeст is e Latijns. Een geheel Latijns woord naast Russisch telt niet als mengvorm. Formules, merknamen en typografisch spel kunnen legitieme treffers geven. Er wordt niets getranslitereerd.

### D13. Kleine letter direct vóór hoofdletter

Mogelijk vastgeplakte woorden of een fout in letterkast vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
\p{Ll}\p{Lu}
```

Oorspronkelijke testtekst:

```text
ОнвышелИздома.
```

Verwachte overeenkomst:

```text
лИ
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Он вышел из дома.
```


Beperkingen en opmerkingen: Hoofdlettergevoeligheid is noodzakelijk. Namen zoals McDonald kunnen correct zijn. De overgang wordt gevonden, niet automatisch de bedoelde woordgrens.

### D14. Cijfer tussen letters

Een typische OCR-verwisseling van letter naar cijfer vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
\p{L}+[0-9]+\p{L}+
```

Oorspronkelijke testtekst:

```text
Это сл0во.
```

Verwachte overeenkomst:

```text
сл0во
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
В главе 10 текст.
```


Beperkingen en opmerkingen: H2O en andere formules kunnen correct zijn. Verander niet alle 0 in о en alle 3 in з.

### D15. Leesteken binnen een letterfragment

Een teken controleren dat direct aan beide kanten door letters wordt omgeven.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
\p{L}+[.,;:!?]\p{L}+
```

Oorspronkelijke testtekst:

```text
Он,сказал слово.
```

Verwachte overeenkomst:

```text
Он,сказал
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Он, сказав слово, ушёл.
```


Beperkingen en opmerkingen: Afkortingen, adressen en domeinen kunnen ook passen. Beperk voor komma’s de klasse tot [,]. Voeg niet in alle treffers zonder controle een spatie toe.

### D16. Alinea begint met kleine letter

Een mogelijk overbodige alineaonderbreking vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
^[ \t]*\p{Ll}
```

Oorspronkelijke testtekst:

```text
продолжение предложения.
```

Verwachte overeenkomst:

```text
п
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Начало предложения.
```


Beperkingen en opmerkingen: Verzen, bijschriften, lijsten en citaten beginnen vaak terecht met kleine letters. Het patroon voegt geen alinea’s samen en kent de naburige context niet.

### D17. Eindinterpunctie ontbreekt vóór sluitende aanhalingstekens

Een letter- of cijfereinde vinden met daarna alleen sluitingstekens en spaties.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[\p{L}\p{N}][»”")\]}]*[ \t\x{00A0}]*$
```

Oorspronkelijke testtekst:

```text
«Он пришёл»
```

Verwachte overeenkomst:

```text
л»
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
«Он пришёл!»
```


Beperkingen en opmerkingen: Koppen en bijschriften hebben vaak geen punt. Een nootverwijzing na correcte interpunctie kan een vals-positief geven. Anders dan een controle op alleen », vindt dit «Он пришёл!» niet verdacht.

### D18. Kleine letter na zinseinde

Een mogelijke letterkastfout na punt, vraagteken of uitroepteken vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[.!?…][ \t]+[«„“"(\[]?\p{Ll}
```

Oorspronkelijke testtekst:

```text
Он пришёл. потом ушёл.
```

Verwachte overeenkomst:

```text
. п
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Он пришёл. Потом ушёл.
```


Beperkingen en opmerkingen: Afkortingspunten en beletseltekens sluiten niet altijd een zin af. Het is uitsluitend een lijst om te beoordelen.

### D19. Mogelijk ontbrekende punt

Een overgang over een spatie van een kleine eindletter naar een woord met hoofdletter vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
\p{Ll}[»”]?[ \t]+[«„“]?\p{Lu}\p{Ll}+
```

Oorspronkelijke testtekst:

```text
Он пришёл Потом ушёл.
```

Verwachte overeenkomst:

```text
л Потом
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Он пришёл потом ушёл.
```


Beperkingen en opmerkingen: Namen en titels midden in een zin geven veel correcte treffers. Het recept beslist niet of een punt, komma of niets nodig is.

### D20. Paren rechte aanhalingstekens

Een eenvoudig fragment tussen rechte dubbele aanhalingstekens op één regel vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
"([^"\r\n]+)"
```

Oorspronkelijke testtekst:

```text
Он сказал "да".
```

Verwachte overeenkomst:

```text
"да"
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Он сказал «да».
```


Beperkingen en opmerkingen: Controleer nesting, inch-aanduidingen, code en de gekozen citaatstijl. «$1» kan alleen in geselecteerde context geschikt zijn; geen universele typografische omzetting.

### D21. Koppelteken of lange streep tussen getallen

Een mogelijk getalbereik vinden dat redactionele beoordeling vraagt.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?<![0-9])[0-9]+[ \t]*[-—][ \t]*[0-9]+(?![0-9])
```

Oorspronkelijke testtekst:

```text
Страницы 12 - 15.
```

Verwachte overeenkomst:

```text
12 - 15
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Страницы 12–15.
```


Beperkingen en opmerkingen: Een datum als 2026-10-02, negatieve waarde en aftrekking kunnen ook passen. Maak er niet automatisch een bereik van. – is een en-streep, — een em-streep, - een koppelteken.

### D22. Twee initialen vóór een achternaam

Een eenvoudige notatie met twee initialen vinden om de spaties te beoordelen.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
\b(\p{Lu})\.[ \t]*(\p{Lu})\.[ \t]+(\p{Lu}\p{Ll}+)\b
```

Oorspronkelijke testtekst:

```text
И.О. Иванов
```

Verwachte overeenkomst:

```text
И.О. Иванов
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Иванов Иван
```


Beperkingen en opmerkingen: Dekt niet alle samengestelde namen en diakritische tekens. Na selectie zijn $1., NBSP, $2., NBSP, $3 bruikbaar; voeg de echte vaste spaties in, niet hun namen.

### D23. Achternaam vóór twee initialen

De omgekeerde naamvolgorde vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
\b(\p{Lu}\p{Ll}+)[ \t]+(\p{Lu})\.[ \t]*(\p{Lu})\.
```

Oorspronkelijke testtekst:

```text
Иванов И.О.
```

Verwachte overeenkomst:

```text
Иванов И.О.
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Иванов Иван
```


Beperkingen en opmerkingen: Dit controleert een formaat, niet de identiteit van een persoon. Samengestelde namen, tussenvoegsels en drie initialen vereisen een andere regel.

### D24. Onzichtbare tekens ter controle

Zacht afbreekteken, nulbreedtespatie of FEFF vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[\x{00AD}\x{200B}\x{FEFF}]
```

Oorspronkelijke testtekst:

```text
сло​во
```

Dezelfde testtekst met zichtbare aanduidingen:

```text
сло[ZWSP]во
```

Hier staat ␠ voor een gewone spatie, [TAB] voor een tab, [NBSP] voor U+00A0, [NNBSP] voor U+202F en [ZWSP] voor U+200B. Dit is een toelichtende weergave; deze labels worden niet in het boek ingevoegd.

Verwachte overeenkomst:

```text
​
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
слово
```


Beperkingen en opmerkingen: De treffer is onzichtbaar: U+200B staat tussen о en в. Pas wijzigen na vaststelling van de functie. Een BOM aan het begin van het bestand en FEFF in de tekst zijn andere gevallen.

### D25. Ongebruikelijke Unicode-spaties

Smalle, brede en andere speciale spaties vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]
```

Oorspronkelijke testtekst:

```text
10 000
```

Dezelfde testtekst met zichtbare aanduidingen:

```text
10[NNBSP]000
```

Hier staat ␠ voor een gewone spatie, [TAB] voor een tab, [NBSP] voor U+00A0, [NNBSP] voor U+202F en [ZWSP] voor U+200B. Dit is een toelichtende weergave; deze labels worden niet in het boek ingevoegd.

Verwachte overeenkomst:

```text
 
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
10 000
```


Beperkingen en opmerkingen: Een smalle vaste spatie tussen cijfergroepen kan volkomen correct zijn. Het recept toont inconsistentie, niet dat alle speciale spaties fouten zijn.

### D26. Herhaalde vraag- en uitroeptekens

Expressieve of per ongeluk dubbele interpunctie vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
[!?]{2,}
```

Oorspronkelijke testtekst:

```text
Что?! Правда!!!
```

Verwachte overeenkomsten, achtereenvolgens:

```text
?!
!!!
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Что? Правда!
```


Beperkingen en opmerkingen: ?! en auteursherhalingen kunnen opzettelijk zijn. De normale actie is beoordeling, niet elke reeks tot één teken verkorten.

### D27. Cyrillische Х naast Romeinse cijfers

Een Russische Х in een Latijnse Romeinse aanduiding vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” is voor deze expressie niet nodig. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
(?<=[IVXLCDM])Х|Х(?=[IVXLCDM])
```

Oorspronkelijke testtekst:

```text
Глава IХ
```

Verwachte overeenkomst:

```text
Х
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Глава IX
```


Beperkingen en opmerkingen: Х is Cyrillisch, X Latijns. Controleer hoofdletters en context; het hele Romeinse nummer wordt niet gevalideerd.

### D28. Meerdere hoofdletters vóór een kleine letter

Een mogelijke OCR-fout in de letterkast aan het woordbegin vinden.

Actie: alleen zoeken en controleren.

“Reguliere expressie” aan; “Alleen hele woorden” uit. “Unicode (UCP)” aan. “Hoofdlettergevoelig” aan.

Zoeken:

```regex
\p{Lu}{2,}\p{Ll}+
```

Oorspronkelijke testtekst:

```text
Он сказал ПРИвет.
```

Verwachte overeenkomst:

```text
ПРИвет
```

Tegenvoorbeeld dat niet gevonden mag worden:

```text
Он сказал Привет.
```


Beperkingen en opmerkingen: Namen, afkortingen met achtervoegsels en Latijnse afkortingen kunnen correct zijn. Verander de letterkast niet in bulk zonder controle.

## 18. Veelvoorkomende fouten en diagnose

### 18.1. Tekst is zichtbaar maar er is geen treffer

Controleer Ontwerp, de regexoptie, hoofdletters, bereik, richting en werkelijke tekens. Latijnse `a` en Russische `а` lijken op elkaar maar verschillen. NBSP is geen gewone spatie en typografische aanhalingstekens zijn geen rechte.

Controleer UCP bij Russische woordgrenzen en hoofdlettergevoeligheid bij letterkastpatronen. Combineer Alleen hele woorden niet onnodig met ingewikkelde eigen grenzen.

### 18.2. Er wordt een te groot fragment gevonden

Verdacht zijn vooral gulzige `.*` en te brede negatieve klassen. Vervang “willekeurige tekst” door toegestane tekens en begrens lengte en bereik. Controleer dotall.

### 18.3. Spaties nemen alinea’s mee

`\s+` is geen synoniem voor gewone spaties. Begin voor woordtussenruimtes met `[ \t]+` en voeg NBSP alleen bewust toe.

### 18.4. Cijfers verschijnen of backslashes verdwijnen in de vervanging

Vergelijk de FBE-grammatica in hoofdstuk 16. `${name}`, `$10`, `\n` en `\x{...}` volgen niet automatisch de zoekregels of die van een andere editor. Controleer dat de capture bestaat en niet optioneel is in een andere tak.

### 18.5. Gemengde alfabetten worden in een normale tweetalige zin gevonden

Een oud voorbeeld controleerde beide alfabetten ergens in de hele regel en vond ook `Он прочитал Latin.`. D12 beperkt beide voorwaarden tot één woord: het onderscheid tussen OCR-diagnose en een tweetalige regel.

### 18.6. “Punt ontbreekt” markeert een correct citaat

Alleen het laatste teken controleren is onvoldoende: op `!` kan `»` volgen. D17 houdt rekening met sluitende haakjes en aanhalingstekens, maar koppen en nootverwijzingen blijven aandacht vragen.

### 18.7. De gewenste structuur wordt niet gevonden

Tekstregex ziet de DOM niet zoals een structureel script. Tags, nesting, ontbrekende ID-doelen en nootverplaatsing zijn andere taken. Code, een FB2-validator of een script kan geschikter zijn dan een complexere expressie.

## 19. Prestaties en grote boeken

Begin met een specifieke voorwaarde: teken, klasse, woord of alineabegin. Vermijd diep geneste onbegrensde herhalingen en meerdere concurrerende “willekeurige tekst”-delen. Een mislukte zoekopdracht kan anders zeer veel varianten aflopen.

Niet-gulzigheid is geen universele oplossing voor traagheid; ook luie kwantoren proberen alternatieven. Meestal helpen expliciete scheidingstekens uitsluiten, lengte begrenzen en een kleiner bereik.

Start bij een langdurige zoekopdracht geen nieuwe vervangingen eroverheen. Vereenvoudig en test op korte tekst. Een PCRE2-resourcelimietfout betekent niet “geen treffers”.

Bij naburige alinea’s en tags is een script vaak begrijpelijker en veiliger. Voeg niet alle correctieregels samen tot een reusachtig alternatief: afzonderlijke regels maken de reden van een treffer duidelijk.

## 20. Controle vóór opslaan

Bekijk begin, midden en einde van het bewerkte deel. Controleer voorbeelden van elk type, vooral citaten, bereiken, initialen, noten en behouden opmaak. Let op verdwenen betekenisvolle NBSP en onverwacht gewijzigde alinea’s.

Voer na structureel gevoelige wijzigingen FBE-documentcontrole uit. Sla op, open zo nodig opnieuw en vergelijk. Een geslaagde regexzoekopdracht vervangt geen FB2-validatie of redactionele correctie.

## 21. Bronnen en geldigheidsgrenzen

Deze handleiding bewerkt het aangeleverde regex-design.md met herschreven toelichting en testvoorbeelden. Onnauwkeurigheden over UCP, zoekobjectgrenzen, gemengde alfabetten en vervanging zijn met primaire bronnen verduidelijkt. De recepten in hoofdstuk 17 zijn eigen redactionele scenario’s, geen citaten uit PCRE2-documentatie.

[D1] Officiële PCRE2-syntaxis: ankers, groepen, Unicode-eigenschappen, backtracking en opties.

`https://www.pcre.org/current/doc/html/pcre2pattern.html`

[D2] Officiële PCRE2-Unicode-documentatie.

`https://www.pcre.org/current/doc/html/pcre2unicode.html`

[D3] FBE Next: PCRE2-compatibiliteit en adapter, revisie d2257405d95b0328649acee64b38829b40a4314b.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/docs/pcre2-compatibility.md`

[D4] FBE Next: GetReplStr, PrepareRegexReplacementText en zoeken/vervangen in FBEview.cpp; SearchPresetCatalog.cpp en search-preset-design-fixtures.cpp uit dezelfde revisie.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/src/fbe/FBEview.cpp`

Enginesyntaxis, interfacemogelijkheden en de juistheid van een redactionele keuze zijn verschillende niveaus. Lokale voorbeeldtests garanderen niet iedere FBE-build met ieder document. De teststatus staat in de README van het archief.
