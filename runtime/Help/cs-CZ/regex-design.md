# Příručka regulárních výrazů — Návrh

Poznámka k překladu: ruská slova a věty v kontrolních příkladech jsou záměrně ponechány beze změny. Jde o testovací data: regulární výrazy, nahrazovací řetězce, bílé znaky a očekávané výsledky odpovídají ruskému referenčnímu vydání. Vysvětlení jsou přeložena; příklady nejsou automaticky přizpůsobeny českým typografickým pravidlům.

Úplná příručka hledání, nahrazování a korektur knih ve FictionBook Editor Next.

Vydání: 2. října 2026. Režim Kód popisuje samostatný soubor regex-source.md. „Vzor“ zde znamená regulární výraz; „vestavěná šablona“ je uložená kombinace výrazu a nastavení v panelu Šablony. Design označuje vizuální režim Návrh.

## 1. Jak příručku používat

Regulární výraz popisuje pravidlo hledání místo jediného přesného řetězce. Například `[0-9]+` najde libovolně dlouhou posloupnost číslic a `[ \t]{2,}` alespoň dvě obyčejné mezery nebo tabulátory. Nalezená shoda a text, který ji má nahradit, jsou dvě různé věci.

Kapitola 2 umožňuje první praktický pokus. Kapitoly 3–15 vysvětlují syntaxi, kapitola 16 zvláštní gramatiku náhrad FBE. Kapitola 17 obsahuje postupy pro knihy s podmínkami, testovacím textem a upozorněními. Následují časté chyby, výkon a zdroje.

Do pole Najít kopírujte pouze výraz. Označení „Najít“, „Nahradit“, U+0020 ani ohraničující značky Markdown nejsou součástí vzoru. Nepotřebujete obal JavaScriptu `/.../g`, uvozovky řetězce C++ ani zdvojená zpětná lomítka z JSON.

Zaškrtávací políčka jsou součástí postupu. Shoda při ignorování velikosti písmen se po zapnutí rozlišování může změnit. Nejde jen o vzhled dialogu.

„Pouze vyhledání a kontrola“ označuje diagnostiku. Nalezený text může být správně: záměrné opakování, cizí jméno, citace, nadpis nebo typografická volba. Ani „nahrazení po kontrole“ nezaručuje bezpečnost pro každou knihu.

## 2. První hledání a bezpečné nahrazení

Uložte pracovní kopii knihy. Přejděte do Návrhu, otevřete Najít nebo Nahradit a zapněte Regulární výraz. Pro první pokusy vypněte Pouze celá slova; hranice je přehlednější vyjádřit přímo ve vzoru. Zkontrolujte rozsah a směr hledání.

Několik mezer najdete takto:

```regex
[ \t]{2,}
```

V textu `Он   пришёл` se najde úsek tří mezer. Do náhrady zadejte jednu obyčejnou mezeru. Nejprve nahraďte jedinou shodu, ověřte `Он пришёл` a teprve potom zvažte Nahradit vše.

Při velkém počtu výsledků si je nejprve prohlédněte a opravte jeden či dva typické případy. Po hromadné změně zkontrolujte text, kurzivu, tučné písmo, poznámky a hranice odstavců. Neočekávaný výsledek vraťte zpět před dalšími úpravami.

Použít v panelu šablon přenese výraz a nastavení do dialogu. Neznamená bezpodmínečně opravit všechny shody. Hledání a nahrazování se spouštějí příslušnými tlačítky zvlášť.

## 3. Modul a hranice režimu Návrh

Režim používá PCRE2-16 s textem i vzory v UTF-16; UTF je vždy zapnuté. FBE samostatně sestavuje hledaný text a provádí náhrady s ohledem na strukturu knihy. Dokumentace PCRE2 proto vysvětluje shody, nikoli všechny operace aplikace. Technické zdroje jsou D1–D4.

Prohledává se textová reprezentace knihy, ne doslovné XML. `<strong>` není způsob hledání tučného písma v Návrhu. XML značky hledejte v Kódu; strukturální změny patří nástrojům editoru nebo specializovaným skriptům.

Vizuální zalomení dlouhého řádku nevkládá znak konce řádku. Odstavce, skutečná přerušení a zalamování podle šířky jsou odlišné. FBE používá víceřádkové kotvy reprezentace hledání pro hranice odstavců. Nalezení shody přes odstavce a její nahrazení však nejsou totéž: zkoumaná implementace náhrady mezi odstavci odmítá.

`\A` a `\z` označují začátek a konec řetězce předaného modulu. Nejde automaticky o celý FB2; záleží na rozsahu a fragmentu vytvořeném FBE.

## 4. Unicode: UTF, UCP a velikost písmen

### 4.1. Co zajišťuje UTF

UTF dovoluje zpracovávat znaky Unicode včetně azbuky. Nepřevádí abecedy, neopravuje OCR, nemění `ё` na `е` a automaticky nesjednocuje kanonicky rovnocenné zápisy znaků.

Písmeno s diakritikou může být jediný znak nebo písmeno a kombinující znaménko. Vzhled může být stejný, hledání po znacích jiné. PCRE2 neprovádí normalizaci NFC/NFD za uživatele. U diakritiky berte v úvahu `\p{M}`.

### 4.2. Co mění UCP

Unicode (UCP) mění především zkrácené třídy `\w`, `\d`, `\s` a na nich závislé hranice `\b` a `\B`. Není to přepínač samotné podpory azbuky.

Upřesnění starších verzí nápovědy: výslovné vlastnosti `\p{L}`, `\p{N}` a `\P{...}` jsou v Unicode sestavení PCRE2 dostupné i bez UCP. Pokud však vzor kombinuje `\p{L}` a `\b`, UCP zajistí soulad významu písmen a hranic slov.

Porovnejte hledání ruského slova:

```regex
\bмир\b
```

S UCP se hranice řídí Unicode třídou slova. Bez UCP neočekávejte, že `\b` bude pro azbuku fungovat stejně jako pro latinku ASCII. Nenalezená shoda nemusí znamenat chybějící slovo.

Pro přesně ASCII číslice používejte `[0-9]`, nikoli `\d`: UCP může druhou třídu rozšířit o desetinné číslice jiných písem.

### 4.3. Užitečné vlastnosti

| Výraz | Co hledá |
| --- | --- |
| `\p{L}` | Písmeno Unicode |
| `\p{Lu}` | Velké písmeno při rozlišování velikosti |
| `\p{Ll}` | Malé písmeno při rozlišování velikosti |
| `\p{M}` | Kombinující znaménko |
| `\p{N}` | Číselný znak, širší pojem než desetinná číslice |
| `\p{Nd}` | Desetinná číslice |
| `\p{Latin}` | Znak latinského písma |
| `\p{Cyrillic}` | Znak cyrilice |
| `\P{L}` | Znak, který není písmenem |

Posloupnost písmen s kombinujícími znaménky:

```regex
[\p{L}\p{M}]+
```

Nejde o univerzální jazykovou definici slova: chybějí spojovníky a apostrofy. Přidávejte je vědomě.

### 4.4. Rozlišování velikosti je samostatná volba

Zapněte Rozlišovat velikost písmen u přechodů jako `строчнаяПрописная`, iniciál a vzorů s `\p{Lu}`/`\p{Ll}`. Ignorování velikosti může smysl pravidla zrušit. UCP tuto volbu nenahrazuje.

Ignorování velikosti přímo ve vzoru:

```regex
(?i)глава
```

Účinek omezený na skupinu:

```regex
(?i:глава)[ \t]+[0-9]+
```

## 5. Doslovné znaky a escapování

Písmena a většina značek zastupují sebe sama. Mimo třídu znaků mohou mít zvláštní význam tečka, závorky, hvězdička, plus, otazník, složené závorky, kotvy a zpětné lomítko.

Pro doslovné hledání metaznaku před něj vložte zpětné lomítko:

| Hledaný text | Výraz |
| --- | --- |
| Tečka | `\.` |
| Otazník | `\?` |
| Plus | `\+` |
| Hvězdička | `\*` |
| Otevírací kulatá závorka | `\(` |
| Zavírací kulatá závorka | `\)` |
| Číslo v hranatých závorkách | `\[[0-9]+\]` |
| Samotné zpětné lomítko | `\\` |

`(123)` hledá číslice `123` a zachytí je do skupiny, ale závorky v textu nevyžaduje. Pro nalezení `(123)` musí být obě závorky doslovné.

Delší doslovný fragment lze v PCRE2 obalit `\Q` a `\E`:

```regex
\QЦена (руб.) + доставка\E
```

V náhradě FBE mají stejné značky jiný význam. Pravidla escapování hledání nepřenášejte automaticky do náhrad.

## 6. Mezery, tabulátory a neviditelné znaky

| Výraz | Význam při hledání PCRE2 |
| --- | --- |
| `[ ]` | Jen obyčejná mezera U+0020 |
| `[ \t]` | Obyčejná mezera nebo tabulátor |
| `\t` | Tabulátor U+0009 |
| `\x{00A0}` | Nezlomitelná mezera |
| `\x{202F}` | Úzká nezlomitelná mezera |
| `\h` | Vodorovný bílý znak včetně různých mezer Unicode |
| `\s` | Bílý znak, případně také konec řádku |
| `\r` | Návrat vozíku CR |
| `\n` | Posun řádku LF |
| `\R` | Sekvence zalomení řádku Unicode |
| `\x{00AD}` | Měkký spojovník |
| `\x{200B}` | Mezera s nulovou šířkou |
| `\x{FEFF}` | FEFF v samotném textu |

Pro běžné mezery mezi slovy volte `[ \t]`, ne `\s`. Druhá třída je širší a může zahrnout hranice odstavců a nezlomitelné mezery. Také `\h` je užitečné diagnosticky, ale příliš široké pro neupřesněnou typografickou náhradu.

NBSP drží pohromadě značku a číslo, iniciály a příjmení, hodnotu a jednotku. Převod všech na normální mezery může zhoršit sazbu. FBE má navíc nastavení znaku NBSP; příklady U+00A0 kontrolujte podle knihy a konkrétní sestavy.

U+200C a U+200D mohou být potřebné v některých písmech a složených emoji. Nemažte hromadně všechny „neviditelné“ znaky. Nalezení ještě není chyba.

## 7. Třídy a rozsahy znaků

Třída v hranatých závorkách spotřebuje jeden znak ze sady. `[abc]` znamená jedno ze tří písmen, nikoli slovo `abc`. `[abc]+` požaduje jedno nebo více písmen ze sady.

`[^abc]` spotřebuje znak mimo sadu. Neznamená podmínku „před textem není abc“; pro kontext slouží lookaround.

Ve třídě tečka a většina závorek ztrácejí speciální význam. Spojovník může označovat rozsah; pro doslovnost jej umístěte na kraj nebo escapujte. Zavírací hranatá závorka se pohodlně zapisuje `\]`.

```regex
[А-Яа-яЁё-]+
```

Ukázka dovoluje ruská písmena a spojovník, ne celou cyrilici. Ukrajinská, běloruská a jiná písmena vyžadují širší sadu nebo vlastnost Unicode.

`\b` uvnitř třídy neznamená hranici slova. Hranice slov nedávejte do běžné sady znaků.

## 8. Kotvy, hranice a prázdné shody

Kotva kontroluje pozici, aniž spotřebuje písmeno nebo mezeru. `^`, `$` či `\b` může vrátit shodu nulové délky, kterou rozhraní nezobrazí jako běžný označený úsek.

| Kotva | Význam |
| --- | --- |
| `^` | Začátek řádku při multiline, jinak začátek hledaného objektu |
| `$` | Konec řádku při multiline; zacházení s posledním koncem řádku závisí na režimu |
| `\A` | Jen začátek hledaného objektu |
| `\z` | Přesný konec objektu |
| `\Z` | Konec objektu nebo pozice před posledním koncem řádku |
| `\b` | Hranice mezi slovním a neslovním znakem |
| `\B` | Pozice, která není hranicí slova |
| `\G` | Počáteční pozice aktuálního volání hledání shody |

`\G` sama nepamatuje „předchozí shodu“. Závisí na počátečním offsetu předaném aplikací; při dalším volání to může být konec předchozího výsledku. Ve FBE nejde o univerzální procházení knihy. Pro běžné korektury volte výslovné hranice. [D1]

Pro celé slovo místo částí delších slov použijte hranice nebo kontroly sousedních písmen. S UCP:

```regex
\bтом\b
```

Neodpovídá začátku `томик`. Spojovníky a apostrofy však může modul považovat za hranice jinak než redaktor.

Prázdné shody v Nahradit vše používejte opatrně: vkládají text do pozic, místo aby měnily viditelné znaky. Pro první pokusy volte neprázdné výsledky.

## 9. Kvantifikátory: opakování a návraty

| Zápis | Opakování předchozího prvku |
| --- | --- |
| `?` | Nula nebo jedno |
| `*` | Nula nebo více |
| `+` | Jedno nebo více |
| `{3}` | Přesně tři |
| `{3,}` | Alespoň tři |
| `{2,5}` | Dvě až pět |

Kvantifikátor působí na předchozí znak, třídu či skupinu. `аб+` opakuje pouze `б`; `(?:аб)+` opakuje dvojici.

### 9.1. Hladové hledání

Výchozí text:

```text
«первый» и «второй»
```

Vzor:

```regex
«.*»
```

Tečka s hvězdičkou nejprve vezme nejdelší možný úsek. Podle potřeby vrací znaky, aby uspěl zbytek vzoru. Zde shoda zahrne oba páry uvozovek.

### 9.2. Líné hledání

```regex
«.*?»
```

Nejprve zkusí nejméně znaků, ale stále může shodu rozšířit. Postupné hledání vrátí `«первый»` a pak `«второй»`.

Pro jednoduchý pár bývá přehlednější výslovně omezit obsah:

```regex
«[^»\r\n]*»
```

Neanalyzuje vnořené uvozovky; ty vyžadují samostatné redakční posouzení.

### 9.3. Přivlastňovací kvantifikátor bez vracení

```regex
«.*+»
```

Neznamená „ještě lepší uvozovky“. `.*+` spotřebuje i zavírací uvozovku a nevrátí ji. Zbývající `»` ve vzoru pak nenajde znak, takže na ukázce nebude shoda.

Smysl může mít vyloučení uzavíracího znaku:

```regex
«[^»\r\n]*+»
```

Přivlastňovací kvantifikátory a atomické skupiny řídí backtracking; nejsou univerzálním urychlením každého vzoru.

## 10. Skupiny a zpětné odkazy

Kulaté závorky zachovávají nalezený fragment. Číslování začíná od 1 a jde po otevíracích zachycovacích závorkách zleva doprava, i při vnoření.

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Na `02.10.2026` skupiny obsahují `02`, `10` a `2026`. Je to kontrola formátu, ne kalendářní platnosti.

`(?:...)` seskupuje bez čísla. U složitých náhrad pomáhá udržet předvídatelné `$1` a `$2`.

Pojmenovaná skupina zlepší čitelnost:

```regex
(?<word>\p{L}+)[ \t]+\k<word>
```

Zpětný odkaz vyžaduje stejný zachycený text. Volání podprogramu opakuje pravidlo a může najít jiný text. Jsou to různé mechanismy.

Pro opakované slovo v knize přidejte hranice, nastavení velikosti a vhodnou sadu mezer. Bez hranic může odkaz najít části delších slov; úplný postup je uveden níže.

Pojmenované skupiny při hledání automaticky neznamenají pojmenované náhrady FBE. V náhradách používejte ověřené číselné odkazy.

## 11. Alternativa a atomické skupiny

Svislá čára volí mezi větvemi:

```regex
(?:глава|часть)[ \t]+[0-9]+
```

Seskupení je důležité: bez něj může společná pravá část patřit jen k poslední alternativě. Větve se zkoušejí v pořadí zápisu; dlouhé a krátké varianty řaďte vědomě.

Atomická skupina zakáže návrat do již úspěšně dokončené skupiny:

```regex
(?>а|аб)в
```

Na `абв` první větev zvolí `а` a už nelze přejít na `аб`. Shoda nebude, ačkoli neatomická verze ji najít může. Nepřidávejte tedy atomicitu bez kontroly výsledků.

## 12. Kontrola okolního textu: lookaround

Lookaround kontroluje kontext, který se nezahrne do celé shody. Můžete vybrat pouze číslo a zachovat jeho značku:

```regex
(?<=№ )[0-9]+
```

Na `№ 125` najde pouze `125`. Požaduje přesně jednu obyčejnou mezeru.

Kontrola napravo:

```regex
[0-9]+(?=[ \t]+руб\.)
```

V `125 руб.` najde číslo bez `руб.`.

Negativní kontrola dopředu:

```regex
\bглава\b(?![ \t]+[0-9])
```

S UCP najde slovo «глава», po němž nenásleduje běžné číslování oddělené mezerou. Je to příklad výběru podle kontextu, ne pravidlo oprav.

`(?<=...)` kontroluje text před shodou a `(?<!...)` podmínku neguje; `(?=...)` a `(?!...)` se týkají textu za ní.

Lookbehind má omezení délky. Moderní PCRE2 podporuje některé omezené proměnlivé délky, ne libovolné neomezené opakování. Pro přenositelné postupy použijte krátký pevný kontext, zachycení či `\K`; nepředpokládejte funkčnost `.*` v lookbehind.

## 13. Volby uvnitř vzoru

| Volba | Účinek |
| --- | --- |
| `(?i)` | Ignoruje velikost písmen |
| `(?-i)` | Rozlišuje velikost písmen |
| `(?m)` | Víceřádkové kotvy začátku a konce |
| `(?-m)` | Vypne víceřádkové kotvy |
| `(?s)` | Tečka může zahrnout konec řádku |
| `(?-s)` | Obnoví běžné chování tečky |
| `(?x)` | Ignoruje nevýznamové mezery a komentáře ve vzoru |

`(?m)` nezpůsobí přechod tečky přes řádky. `(?s)` nedovolí FBE náhradu mezi odstavci. Jde o dvě možnosti vyhledání a oddělené omezení aplikace.

V rozšířeném režimu nemusí být mezery ve vzoru doslovné. Povinnou mezeru zapisujte `[ ]`; `#` mimo třídu může začít komentář. Víceřádkové vzory jsou čitelné v dokumentaci, ale postupy pro jednořádkové pole se uvádějí na jednom řádku.

## 14. Pokročilé možnosti PCRE2

Většina korektur tuto kapitolu nepotřebuje. Pomáhá číst cizí vzory. Dostupná syntaxe neznamená, že je každý takový vzor praktický nebo bezpečný pro celou knihu.

### 14.1. Změna začátku výsledné shody

```regex
№[ \t]*\K[0-9]+
```

V `№ 125` ověří předponu, ale vrátí jen `125`. Užitečné při změně čísla bez jeho značky. `\K` v lookaround používejte jen po ověření omezení PCRE2.

### 14.2. Podmínka účasti skupiny

```regex
^(\()?([0-9]+)(?(1)\))$
```

Povolí `12` nebo `(12)`, nikoli neuzavřené `(12`. Podmínka kontroluje účast první skupiny. Před použitím volitelných skupin v náhradě otestujte adaptér FBE.

### 14.3. Společné číslování větví

```regex
(?|глава ([0-9]+)|часть ([0-9]+))
```

Číslo je v obou větvích ve skupině 1: branch reset. V jednoduchých situacích bývá srozumitelnější nezachycovací skupina se společným zachycením čísla.

### 14.4. Volání podprogramu

```regex
(?<pair>[0-9]{2})-(?&pair)
```

U `12-34` obě části splňují pravidlo „dvě číslice“, i když jsou různé. Odkaz `\k<pair>` by vyžadoval zopakovat přesně `12`.

Rekurzivní podprogramy umějí popsat určité vnořené struktury, ale nenahrazují XML parser FBE. Pro `<section>`, `<poem>`, poznámky a tabulky použijte strukturální nástroje.

### 14.5. Přeskakování fragmentů

```regex
«[^»\r\n]*»(*SKIP)(*FAIL)|\bслово\b
```

S UCP se slovo hledá mimo jednoduché ruské uvozovky. První větev vyznačí citaci k přeskočení, druhá hledá slovo. Vnořené a neuzavřené uvozovky nejsou analyzovány; nepoužívejte to slepě pro hromadné korektury.

## 15. Co regulární výraz nerozhodne

Velké písmeno po malém, chybějící koncové znaménko nebo opakované slovo jsou formální indicie, nikoli redakční rozhodnutí. Regex neví, zda `да да` je překlep, záměrná replika, verš nebo nadpis.

Nepřidávejte automaticky tečky, nespojujte všechny odstavce začínající malým písmenem, nepřevádějte každou latinku na azbuku a každý spojovník na pomlčku. Nutný je kontext a někdy několik operací na DOM. Skripty FBE pro čištění, slepená slova a poznámky řeší právě tyto širší úkoly.

## 16. Gramatika náhrad FBE

PCRE2 hledá, ale řetězec náhrady interpretuje FBE. Příklady z JavaScriptu, Pythonu, .NET, PCRE2 substitute nebo jiného editoru přenášejte jen po testu. Níže jsou příkazy potvrzené kódem a původní dokumentací FBE. [D3, D4]

### 16.1. Shoda a skupiny

| V poli náhrady | Význam |
| --- | --- |
| `$0` nebo `\0` | Celá shoda |
| `$1` … `$9` | Skupina s příslušným číslem |
| `\1` … `\9` | Alternativní číselný odkaz |
| `$+` nebo `\+` | Poslední skupina vrácená adaptérem shody |

Tvrzení „FBE nahrazuje jen skupiny 1–9“ je nepřesné: lze vložit i obyčejný text a celou shodu. Omezený je přímý číselný přístup, ne počet skupin PCRE2.

Nepoužívejte `$10` pro desátou skupinu. `${name}` nepatří do ověřené gramatiky. Potřebná zachycení ponechte mezi prvními devíti a pomocné skupiny tvořte jako nezachycovací.

Volitelné a prázdné skupiny otestujte zvlášť: FBE má vlastní adaptér SubMatches. Nové hromadné náhrady nestavte na jemných rozdílech vynechaných skupin nebo „poslední skupiny“; upřednostněte jasná povinná zachycení.

### 16.2. Změna pořadí skupin

Najít:

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Nahradit za:

```text
$3-$2-$1
```

`02.10.2026` se změní na `2026-10-02`. Je to ukázka změny pořadí, ne doporučení převést všechna data v knize.

### 16.3. Změna velikosti písmen

| Příkaz | Účinek |
| --- | --- |
| `\U` | Zapne velká písmena vkládaného fragmentu |
| `\L` | Zapne malá písmena |
| `\T` | První písmeno fragmentu velké, ostatní malá |
| `\Q` | Vypne aktivní příkazy velikosti a formátování pro následující text |

Najít:

```regex
(иван)
```

Nahradit za:

```text
\T$1\Q
```

Kontrolní výsledek: `Иван`. `\T` není jazykově pokročilá kapitalizace každého slova. Jediný fragment `иван иванов` se nemusí změnit na `Иван Иванов`.

Nekombinujte `\U` a `\L` bez resetu. Části oddělte `\Q` a ověřte cyrilici i diakritiku. Změnu velikosti dělá FBE, ne Unicode normalizátor PCRE2.

### 16.4. Tučné písmo a kurziva

`\S` zapne tučné písmo vkládaného fragmentu, `\E` kurzivu. `\Q` ukončí působení aktivních příkazů na další text.

```text
\S$1\Q
```

Formátuje obsah první skupiny. Nehledá už tučný text a neruší veškeré předchozí formátování. Nejprve otestujte jeden fragment a vrácení změny.

### 16.5. Stejné značky s odlišným významem

V hledání je `\S` nebílý znak, v náhradě FBE tučné písmo. `\Q...\E` v hledání chrání doslovný fragment, zatímco v náhradě `\Q` resetuje příkazy a `\E` zapíná kurzivu.

Do náhrady Design nepište `\n`, `\t`, `\x{00A0}`, `\$` nebo `$$` s očekáváním pravidel jiného editoru. Nejsou součástí zde popsané gramatiky literálů; neznámá sekvence může zmizet. Pro NBSP vložte skutečný znak. Zvláštní znak zachovejte zachycením nebo ověřenou běžnou náhradou.

Prázdné pole shodu smaže; pole s jednou mezerou ji nahradí mezerou. Značky `<пусто>`, `[NBSP]` a `U+00A0` z vysvětlení se nemají psát doslovně.

## 17. Praktické postupy pro úpravu a korekturu knih

Příklady jsou samostatné scénáře, nikoli příslib totožných názvů vestavěných šablon. Náhradu nejprve použijte na jediný výsledek. Skutečné mezery a tabulátory v ukázkách zůstávají zachovány; negativní příklady ukazují alespoň jednu hranici, ale nenahrazují kontrolu celé knihy.

### D01. Několik obyčejných mezer na jednu

Zkrátit běžné mezery, aniž se změní samostatná NBSP.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[ \t]{2,}
```

Výchozí testovací text:

```text
Он   пришёл.
```

Nahradit za: jednu obyčejnou mezeru U+0020. Je to jeden znak, nikoli slovo „mezera“.

Výsledek nahrazení:

```text
Он пришёл.
```

Protipříklad, který se nemá najít:

```text
Он пришёл.
```


Omezení a poznámky: V poezii, tabulkách a simulovaném odsazení mohou být násobné mezery záměrné. Ověřte rozsah; nejde o obnovení odsazení odstavců.

### D02. Mezery na začátku odstavce

Odstranit ruční odsazení před běžným textem.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
^[ \t]+
```

Výchozí testovací text:

```text
   Начало абзаца.
```

Tentýž testovací text s viditelnými značkami:

```text
␠␠␠Начало␠абзаца.
```

Značka ␠ znamená obyčejnou mezeru, [TAB] tabulátor, [NBSP] U+00A0, [NNBSP] U+202F a [ZWSP] U+200B. Jde o vysvětlující zápis; tyto značky se do knihy nevkládají.

Nahradit za: nechte pole úplně prázdné. Nepište slovo „prázdné“.

Výsledek nahrazení:

```text
Начало абзаца.
```

Protipříklad, který se nemá najít:

```text
Начало абзаца.
```


Omezení a poznámky: V poezii a umělecké sazbě může být odsazení významové. Kotva se vztahuje na textový řádek, ne jeho vizuální zalomení.

### D03. Mezery na konci odstavce

Odstranit koncové obyčejné mezery nebo tabulátory.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[ \t]+$
```

Výchozí testovací text:

```text
Конец абзаца.
```

Tentýž testovací text s viditelnými značkami:

```text
Конец␠абзаца.␠␠␠
```

Značka ␠ znamená obyčejnou mezeru, [TAB] tabulátor, [NBSP] U+00A0, [NNBSP] U+202F a [ZWSP] U+200B. Jde o vysvětlující zápis; tyto značky se do knihy nevkládají.

Nahradit za: nechte pole úplně prázdné. Nepište slovo „prázdné“.

Výsledek nahrazení:

```text
Конец абзаца.
```

Protipříklad, který se nemá najít:

```text
Конец абзаца.
```


Omezení a poznámky: NBSP je záměrně vynechána. Nestandardní mezery kontrolujte jiným postupem.

### D04. Mezera před čárkou a další interpunkcí

Zachovat znaménko a smazat mezeru před ním.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[ \t]+([,;:!?])
```

Výchozí testovací text:

```text
Слово , другое !
```

Nahradit za:

```text
$1
```

Výsledek nahrazení:

```text
Слово, другое!
```

Protipříklad, který se nemá najít:

```text
Слово, другое!
```


Omezení a poznámky: Jde o pravidlo pro běžný ruský text. Francouzská typografie před některými znaménky používá zvláštní mezery; tam postup bez úpravy neaplikujte.

### D05. Mezera za otevíracím znaménkem

Smazat obyčejnou mezeru za závorkou nebo ruskou otevírací uvozovkou.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
([(\[«„])[ \t]+
```

Výchozí testovací text:

```text
« слово» ( пример)
```

Nahradit za:

```text
$1
```

Výsledek nahrazení:

```text
«слово» (пример)
```

Protipříklad, který se nemá najít:

```text
«слово» (пример)
```


Omezení a poznámky: Nezasahuje mezery ve vzorcích a ukázkách, pokud přímo nenásledují uvedené značky. Kontext přesto ověřte.

### D06. Mezera před zavíracím znaménkem

Smazat obyčejnou mezeru před závorkou nebo zavírací uvozovkou.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[ \t]+([)\]»”])
```

Výchozí testovací text:

```text
«слово » (пример )
```

Nahradit za:

```text
$1
```

Výsledek nahrazení:

```text
«слово» (пример)
```

Protipříklad, který se nemá najít:

```text
«слово» (пример)
```


Omezení a poznámky: Nesjednocuje všechny systémy uvozovek a matematických závorek.

### D07. Právě tři tečky na výpustku

Převést tři souvislé tečky a ponechat delší posloupnosti.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?<!\.)\.{3}(?!\.)
```

Výchozí testovací text:

```text
Он подумал... и ответил.
```

Nahradit za:

```text
…
```

Výsledek nahrazení:

```text
Он подумал… и ответил.
```

Protipříklad, který se nemá najít:

```text
Содержание.....12
```


Omezení a poznámky: Ověřte redakční konvenci. Čtyři tečky a tečkované vodicí linky obsahu jsou záměrně vyloučené.

### D08. Výpustka s mezerami

Spojit tři tečky oddělené obyčejnými mezerami.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?<!\.)\.[ \t]*\.[ \t]*\.(?!\.)
```

Výchozí testovací text:

```text
Он подумал. . . и ответил.
```

Nahradit za:

```text
…
```

Výsledek nahrazení:

```text
Он подумал… и ответил.
```

Protipříklad, který se nemá najít:

```text
Слово. Другое.
```


Omezení a poznámky: Najde i tři tečky bez mezer. Nepoužívat pro vodicí tečky a vynechávky v citacích řízené zvláštními pravidly.

### D09. Nezlomitelná mezera za №

Svázat znak čísla s následující hodnotou.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
№[ \t]+([0-9]+)
```

Výchozí testovací text:

```text
№ 125
```

Nahradit za:

```text
№ $1
```

Výsledek nahrazení:

```text
№ 125
```

Protipříklad, který se nemá najít:

```text
№125
```


Omezení a poznámky: Mezi № a $1 je v náhradě skutečný U+00A0. Nezaměňujte jej za napsanou sekvenci \x{00A0}. Vzor nepřidá mezeru tam, kde zcela chybí.

### D10. Nezlomitelná mezera za §

Svázat paragrafový znak s číslem.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
§[ \t]+([0-9]+)
```

Výchozí testovací text:

```text
§ 12
```

Nahradit za:

```text
§ $1
```

Výsledek nahrazení:

```text
§ 12
```

Protipříklad, který se nemá najít:

```text
§12
```


Omezení a poznámky: Mezi § a $1 je skutečná NBSP. U složitých číselných označení ověřte vybraný fragment.

### D11. Možné opakování sousedního slova

Najít opakování přes vodorovné mezery bez přeskočení odstavce.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ vypnuto.

Najít:

```regex
\b(\p{L}+)[ \t\x{00A0}]+\1\b
```

Výchozí testovací text:

```text
Это это уже было.
```

Očekávaná shoda:

```text
Это это
```

Protipříklad, který se nemá najít:

```text
Это уже было.
```


Omezení a poznámky: Opakování jako «Да да» může být autorské. Nemažte automaticky druhé slovo; rozhodněte, zda patří čárka, odstranění nebo žádná změna. Apostrofy a spojovníky se neanalyzují úplně.

### D12. Latinka a cyrilice uvnitř jednoho slova

Najít OCR směs v jediném souvislém slově, ne každou dvojjazyčnou řádku.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?<![\p{L}\p{M}])(?=[\p{L}\p{M}]*\p{Latin})(?=[\p{L}\p{M}]*\p{Cyrillic})[\p{L}\p{M}]+(?![\p{L}\p{M}])
```

Výchozí testovací text:

```text
В слове Тeст латинская e.
```

Očekávaná shoda:

```text
Тeст
```

Protipříklad, který se nemá najít:

```text
Он прочитал Latin.
```


Omezení a poznámky: V Тeст je e latinské. Plně latinské slovo vedle ruštiny není směs. Vzorce, značky a typografické hříčky mohou být správně. Pravidlo nic nepřepisuje do jiné abecedy.

### D13. Malé písmeno přímo před velkým

Najít možná slepená slova nebo chybu velikosti.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
\p{Ll}\p{Lu}
```

Výchozí testovací text:

```text
ОнвышелИздома.
```

Očekávaná shoda:

```text
лИ
```

Protipříklad, který se nemá najít:

```text
Он вышел из дома.
```


Omezení a poznámky: Nutně zapněte rozlišování velikosti. Jména typu McDonald mohou být správná. Najde se přechod, ne automaticky rekonstruovaná mezera.

### D14. Číslice mezi písmeny

Najít typické OCR nahrazení písmena číslicí.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
\p{L}+[0-9]+\p{L}+
```

Výchozí testovací text:

```text
Это сл0во.
```

Očekávaná shoda:

```text
сл0во
```

Protipříklad, který se nemá najít:

```text
В главе 10 текст.
```


Omezení a poznámky: H2O a jiné vzorce mohou být správně. Nezaměňujte všechna 0 za о ani všechna 3 za з.

### D15. Interpunkce uvnitř písmenového fragmentu

Zkontrolovat značku obklopenou písmeny z obou stran.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
\p{L}+[.,;:!?]\p{L}+
```

Výchozí testovací text:

```text
Он,сказал слово.
```

Očekávaná shoda:

```text
Он,сказал
```

Protipříklad, který se nemá najít:

```text
Он, сказав слово, ушёл.
```


Omezení a poznámky: Zkratky, adresy a domény mohou také vyhovět. Pro kontrolu jen čárek zúžte třídu na [,]. Nevkládejte hromadně mezery bez posouzení.

### D16. Odstavec začínající malým písmenem

Najít možný nadbytečný konec předchozího odstavce.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
^[ \t]*\p{Ll}
```

Výchozí testovací text:

```text
продолжение предложения.
```

Očekávaná shoda:

```text
п
```

Protipříklad, který se nemá najít:

```text
Начало предложения.
```


Omezení a poznámky: Verše, popisky, seznamy a citace mohou správně začínat malým písmenem. Vzor odstavce nespojuje a nezná sousední kontext.

### D17. Chybějící koncová interpunkce před uzavřením citace

Najít konec tvořený písmenem či číslicí, po němž jsou jen uzavírací značky a mezery.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[\p{L}\p{N}][»”")\]}]*[ \t\x{00A0}]*$
```

Výchozí testovací text:

```text
«Он пришёл»
```

Očekávaná shoda:

```text
л»
```

Protipříklad, který se nemá najít:

```text
«Он пришёл!»
```


Omezení a poznámky: Nadpisy a popisky často nemají tečku. Odkaz na poznámku za správnou interpunkcí může dát falešný nález. Oproti kontrole samotného » postup neoznačí «Он пришёл!».

### D18. Malé písmeno po konci věty

Najít pravděpodobnou chybu velikosti po tečce, otazníku nebo vykřičníku.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[.!?…][ \t]+[«„“"(\[]?\p{Ll}
```

Výchozí testovací text:

```text
Он пришёл. потом ушёл.
```

Očekávaná shoda:

```text
. п
```

Protipříklad, který se nemá najít:

```text
Он пришёл. Потом ушёл.
```


Omezení a poznámky: Tečky zkratek a autorské výpustky nemusí končit větu. Jde jen o kandidáty ke kontrole.

### D19. Možná chybějící tečka

Najít přechod od malé koncovky přes mezeru ke slovu s velkým písmenem.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
\p{Ll}[»”]?[ \t]+[«„“]?\p{Lu}\p{Ll}+
```

Výchozí testovací text:

```text
Он пришёл Потом ушёл.
```

Očekávaná shoda:

```text
л Потом
```

Protipříklad, který se nemá najít:

```text
Он пришёл потом ушёл.
```


Omezení a poznámky: Vlastní jména a názvy uvnitř vět způsobí mnoho správných shod. Postup nerozhodne mezi tečkou, čárkou a žádnou změnou.

### D20. Dvojice rovných uvozovek

Najít jednoduchý úsek v rovných dvojitých uvozovkách na jednom řádku.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
"([^"\r\n]+)"
```

Výchozí testovací text:

```text
Он сказал "да".
```

Očekávaná shoda:

```text
"да"
```

Protipříklad, který se nemá najít:

```text
Он сказал «да».
```


Omezení a poznámky: Zkontrolujte vnoření, palcové značky, kód a zvolenou typografii. «$1» je vhodná náhrada jen ve vybraném kontextu, ne univerzální převod.

### D21. Spojovník nebo dlouhá pomlčka mezi čísly

Najít možný číselný rozsah k redakční kontrole.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?<![0-9])[0-9]+[ \t]*[-—][ \t]*[0-9]+(?![0-9])
```

Výchozí testovací text:

```text
Страницы 12 - 15.
```

Očekávaná shoda:

```text
12 - 15
```

Protipříklad, který se nemá najít:

```text
Страницы 12–15.
```


Omezení a poznámky: Datum 2026-10-02, záporné číslo i odčítání mohou vyhovět. Nepřevádějte automaticky na rozsah. – je krátká pomlčka, — dlouhá, - spojovník.

### D22. Dvě iniciály před příjmením

Najít jednoduché dvě iniciály pro kontrolu mezer.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
\b(\p{Lu})\.[ \t]*(\p{Lu})\.[ \t]+(\p{Lu}\p{Ll}+)\b
```

Výchozí testovací text:

```text
И.О. Иванов
```

Očekávaná shoda:

```text
И.О. Иванов
```

Protipříklad, který se nemá najít:

```text
Иванов Иван
```


Omezení a poznámky: Nezahrnuje všechna složená příjmení a diakritiku. Po posouzení lze použít $1., NBSP, $2., NBSP, $3 se skutečnými nezlomitelnými mezerami, ne jejich názvy.

### D23. Příjmení před dvěma iniciálami

Najít opačné pořadí zápisu jména.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
\b(\p{Lu}\p{Ll}+)[ \t]+(\p{Lu})\.[ \t]*(\p{Lu})\.
```

Výchozí testovací text:

```text
Иванов И.О.
```

Očekávaná shoda:

```text
Иванов И.О.
```

Protipříklad, který se nemá najít:

```text
Иванов Иван
```


Omezení a poznámky: Kontroluje formát, ne identitu osoby. Složená příjmení, částice a tři iniciály vyžadují další pravidlo.

### D24. Neviditelné znaky ke kontrole

Najít měkký spojovník, nulovou mezeru nebo FEFF.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[\x{00AD}\x{200B}\x{FEFF}]
```

Výchozí testovací text:

```text
сло​во
```

Tentýž testovací text s viditelnými značkami:

```text
сло[ZWSP]во
```

Značka ␠ znamená obyčejnou mezeru, [TAB] tabulátor, [NBSP] U+00A0, [NNBSP] U+202F a [ZWSP] U+200B. Jde o vysvětlující zápis; tyto značky se do knihy nevkládají.

Očekávaná shoda:

```text
​
```

Protipříklad, který se nemá najít:

```text
слово
```


Omezení a poznámky: V ukázce je shoda neviditelná: mezi о a в je U+200B. Změnit až po určení jeho funkce. Fyzický BOM souboru a FEFF v textu jsou jiné případy.

### D25. Neobvyklé mezery Unicode

Najít úzké, široké a jiné zvláštní mezery.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]
```

Výchozí testovací text:

```text
10 000
```

Tentýž testovací text s viditelnými značkami:

```text
10[NNBSP]000
```

Značka ␠ znamená obyčejnou mezeru, [TAB] tabulátor, [NBSP] U+00A0, [NNBSP] U+202F a [ZWSP] U+200B. Jde o vysvětlující zápis; tyto značky se do knihy nevkládají.

Očekávaná shoda:

```text
 
```

Protipříklad, který se nemá najít:

```text
10 000
```


Omezení a poznámky: Úzká nezlomitelná mezera mezi skupinami číslic může být správně. Postup upozorňuje na nejednotnost, neoznačuje všechny takové znaky za chybu.

### D26. Opakované otazníky a vykřičníky

Najít expresivní nebo náhodně zdvojenou interpunkci.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[!?]{2,}
```

Výchozí testovací text:

```text
Что?! Правда!!!
```

Očekávané shody postupně:

```text
?!
!!!
```

Protipříklad, který se nemá najít:

```text
Что? Правда!
```


Omezení a poznámky: ?! a autorské opakování mohou být záměrné. Výchozí akcí je kontrola, ne zkrácení každé sekvence na znak.

### D27. Cyrilské Х u římských číslic

Najít ruské Х vedle latinských římských značek.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ není pro tento výraz nutné. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?<=[IVXLCDM])Х|Х(?=[IVXLCDM])
```

Výchozí testovací text:

```text
Глава IХ
```

Očekávaná shoda:

```text
Х
```

Protipříklad, který se nemá najít:

```text
Глава IX
```


Omezení a poznámky: Х je cyrilské, X latinské. Ověřte velikost a kontext; celý římský zápis vzor nevaliduje.

### D28. Několik velkých písmen před malým

Najít možnou OCR chybu velikosti na začátku slova.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Unicode (UCP)“ zapnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
\p{Lu}{2,}\p{Ll}+
```

Výchozí testovací text:

```text
Он сказал ПРИвет.
```

Očekávaná shoda:

```text
ПРИвет
```

Protipříklad, který se nemá najít:

```text
Он сказал Привет.
```


Omezení a poznámky: Názvy, zkratková slova s příponami a latinské zkratky mohou být správně. Neměňte velikost plošně bez ověření.

## 18. Časté chyby a jejich rozpoznání

### 18.1. Text je viditelný, ale hledání ho nenajde

Zkontrolujte Návrh, volbu regex, velikost písmen, rozsah, směr a skutečné znaky. Latinské `a` a ruské `а` jsou podobné, ale různé; NBSP není obyčejná mezera a typografické uvozovky nejsou rovné.

U ruských hranic slov ověřte UCP, u velkých a malých písmen příslušnou volbu. Nekombinujte zbytečně Pouze celá slova se složitými vlastními hranicemi.

### 18.2. Shoda je příliš dlouhá

Nejprve podezírejte hladové `.*` nebo příliš širokou negovanou třídu. Nahraďte „jakýkoli text“ konkrétní sadou a omezte délku a hranice. Ověřte dotall.

### 18.3. Mezery zahrnují odstavce

`\s+` není synonymem mezery. Pro interval mezi slovy začněte `[ \t]+` a NBSP přidejte jen vědomě.

### 18.4. V náhradě se objevují číslice nebo mizí lomítka

Porovnejte gramatiku FBE v kapitole 16. `${name}`, `$10`, `\n` a `\x{...}` automaticky nedědí pravidla hledání ani jiného editoru. Ověřte, že skupina existuje a není volitelná v jiné větvi.

### 18.5. Smíšené abecedy označují běžnou dvojjazyčnou větu

Starší příklad hledal latinku a azbuku kdekoli na řádku a našel i `Он прочитал Latin.`. D12 omezuje obě podmínky na stejné slovo — podstatný rozdíl mezi OCR anomálií a dvojjazyčným řádkem.

### 18.6. „Chybí tečka“ označí správnou citaci

Poslední znak nestačí: po `!` může následovat `»`. D17 zahrnuje uzavírací závorky a uvozovky, ale nadpisy a poznámkové odkazy stále potřebují ověření.

### 18.7. Požadovaná struktura se nenajde

Textová regex nevidí DOM jako strukturální skript. Tagy, vnoření, odkazy na chybějící ID a přesuny poznámek jsou jiné úkoly. Vhodnější může být Kód, validátor FB2 nebo skript než složitější výraz.

## 19. Výkon a rozsáhlé knihy

Začněte konkrétním omezením: značkou, třídou, slovem nebo začátkem odstavce. Vyhněte se hluboce vnořeným neomezeným opakováním a několika soupeřícím úsekům „libovolný text“. Neúspěšné hledání pak může zkoušet obrovské množství možností.

Líný kvantifikátor není univerzální lék na pomalost; také prochází alternativy. Obvykle pomůže výslovné vyloučení oddělovače, omezení délky a menší rozsah.

Při dlouhém hledání nespouštějte další náhrady. Zjednodušte vzor a zkuste krátký text. Chyba limitu prostředků PCRE2 neznamená „žádná shoda“.

U sousedních odstavců a tagů je skript často přehlednější i bezpečnější. Nespojujte všechny korektury do obrovské alternativy: samostatná pravidla ukazují důvod nálezu.

## 20. Kontrola před uložením

Prohlédněte začátek, střed a konec zpracované části. Ověřte vzorky každého typu, především citace, rozsahy, iniciály, poznámky a zachované formátování. Zkontrolujte, zda nezmizely významové NBSP nebo se nepřeskupily odstavce.

Po změnách citlivých na strukturu spusťte kontrolu dokumentu FBE. Uložte, podle potřeby znovu otevřete a porovnejte. Úspěšné hledání nenahrazuje validaci FB2 ani redakční korekturu.

## 21. Zdroje a rozsah platnosti

Příručka přepracovává dodaný regex-design.md s novými vysvětleními a ukázkami. Nepřesnosti v UCP, hranicích hledaného objektu, smíšených abecedách a náhradách byly upřesněny primárními zdroji. Postupy v kapitole 17 jsou samostatné redakční scénáře, nikoli citace dokumentace PCRE2.

[D1] Oficiální syntaxe PCRE2: kotvy, skupiny, Unicode vlastnosti, backtracking a volby.

`https://www.pcre.org/current/doc/html/pcre2pattern.html`

[D2] Oficiální Unicode dokumentace PCRE2.

`https://www.pcre.org/current/doc/html/pcre2unicode.html`

[D3] FBE Next: kompatibilita PCRE2 a adaptér, revize d2257405d95b0328649acee64b38829b40a4314b.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/docs/pcre2-compatibility.md`

[D4] FBE Next: GetReplStr, PrepareRegexReplacementText a hledání/náhrady ve FBEview.cpp; SearchPresetCatalog.cpp a search-preset-design-fixtures.cpp stejné revize.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/src/fbe/FBEview.cpp`

Syntaxe modulu, možnosti rozhraní a správnost redakční volby jsou různé úrovně. Lokální test příkladů negarantuje každou sestavu FBE s každým dokumentem. Stav kontrol je popsán v README archivu.
