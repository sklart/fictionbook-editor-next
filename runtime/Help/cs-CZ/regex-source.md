# Příručka regulárních výrazů — Kód

:::note
Poznámka k překladu: ruská slova a věty v kontrolních příkladech jsou záměrně ponechány beze změny. Jde o testovací data: regulární výrazy, nahrazovací řetězce, bílé znaky a očekávané výsledky odpovídají ruskému referenčnímu vydání. Vysvětlení jsou přeložena; příklady nejsou automaticky přizpůsobeny českým typografickým pravidlům.
:::

Úplná příručka hledání a nahrazování ve zdrojovém XML knihy v FictionBook Editor Next.

Vydání: 2. října 2026. Návrh popisuje regex-design.md. Zdejší postupy patří Kódu; vzory PCRE2 nepřenášejte bez ověření.

## 1. K čemu slouží hledání v Kódu

Kód ukazuje XML značky, atributy, odkazy, entity a text knihy. Hodí se pro audit FB2: prázdné prvky, importované HTML, provizorní odkazy, neobvyklé atributy a zbytky konverze.

:::note
Regex zkoumá text XML, ne parsovaný strom DOM. Strukturu ověřte nástrojem pro XML.
:::

Pro obyčejnou jazykovou korekturu bývá pohodlnější Návrh. Shoda ve zdrojovém XML může být v atributu, komentáři, CDATA, názvu souboru nebo binárním obsahu, nejen v knižním textu. Myslete na to před nahrazením.

„Pouze vyhledání a kontrola“ poskytuje kandidáty, ne důkaz chyby XML či textu. Dokonce mezery mohou být významové; neexistuje univerzálně bezpečná náhrada v libovolném XML.

## 2. Rychlý začátek

Uložte kopii, přejděte do Kódu, otevřete Najít a zapněte Regulární výraz. XML ukázky běžně rozlišují velikost písmen a vypínají Pouze celá slova. Hranice značek a atributů určuje vzor.

Jednoduchý prázdný odstavec:

```regex
<p>[ \t]*</p>
```

Najde `<p></p>` a `<p>   </p>` na jednom fyzickém řádku, nikoli `<p>Текст</p>`. Automaticky nemažte ani nepřevádějte na `<empty-line/>`: jejich strukturální role se může lišit.

:::example
Najít: <p>[ \t]*</p>
Vstup: <p>   </p>
Shoda: <p>   </p>
Očekáváno: Před změnou XML tento odstavec zkontrolujte.
:::

Kopírujte jen vzor, bez `/.../g`, uvozovek řetězce C++ a zdvojení lomítek z JSON.

## 3. Modul: Scintilla, ne PCRE2

FBE používá Scintillu s `SCFIND_REGEXP` a `SCFIND_CXX11REGEX`. Zkoumaná sestava používá C++ implementaci regulárních výrazů s gramatikou ECMAScript. Není to PCRE2 ani úplný moderní JavaScript. [S1, S2]

Scintilla má i starší základní modul s odlišnými pravidly. Nezaměňujte staré zachycovací závorky s escape za režim C++11 FBE: `(слово)` zachycuje, `\(слово\)` vyžaduje skutečné závorky.

Unicode text není zakázán, ale UCP a vlastnosti PCRE2 nejsou k dispozici. Třídy závisejí na standardní knihovně; `\w` neznamená všechna písmena libovolného jazyka. Pro předvídatelné ASCII používejte `[A-Za-z0-9_]`, pro ruštinu `[А-Яа-яЁё]`, které nezahrnuje celou cyrilici.

### 3.1. Hlavní rozdíly proti Návrhu

| Vlastnost | Návrh | Kód |
| --- | --- | --- |
| Hledaný objekt | Textová reprezentace knihy | Zdrojové XML |
| Modul | PCRE2-16 | Scintilla C++11 |
| UCP a Unicode vlastnosti | Dostupné v profilu PCRE2 | Bez syntaxe vlastností PCRE2 |
| Lookbehind | Dostupný s omezeními PCRE2 | Nepodporován |
| První skupina v náhradě | `$1` či `\1` | `\1` |
| Formátování náhrady | Příkazy FBE | Jen XML text |
| Přesah fyzického řádku | Závisí na reprezentaci; náhrady mezi odstavci omezené | V současné cestě nepodporován |

## 4. Hledání po řádcích: zásadní omezení

Ve Scintilla cestě FBE funkce `MatchOnLines` zpracovává vzor na každém fyzickém řádku zvlášť. Řádek může být velmi dlouhý; jeho zalomení na obrazovce není nový fyzický řádek. [S2]

:::warning
Regex ve Zdrojovém režimu nemůže v současné cestě hledat přes fyzické řádky; vzor je nespojí.
:::

Porovnejte:

```xml
<empty-line/> <empty-line/>
```

s:

```xml
<empty-line/>
<empty-line/>
```

Z hlediska XML může jít o totožné sousedství. Pro současné FBE regex lze první variantu najít jednou shodou, druhá však překročí hranici řádku.

Přidání `\r\n`, `\n`, `\x0D`, `\x0A`, `\s*` nebo třídy „jakýkoli znak“ omezení neodstraní. Modul nedostává oba řádky jako jednu vstupní část. Nutná by byla změna vyhledávací cesty programu.

Hledání po řádcích nebrání vložení konce řádku náhradou. Jde o jinou operaci, ukázanou v K28. Rozlišujte nalezení přes hranici a vložení hranice do výsledku.

### 4.1. Práce s víceřádkovým XML

Atribut na vlastním řádku lze často najít samostatně, bez celé otevírací značky. Pro vztahy mezi řádky použijte strukturální nástroje FBE, validátor, skript nebo nástroj XML.

Nemažte všechny konce řádků kvůli jedinému vzoru. Mohlo by se změnit znění, komentáře, CDATA a čitelnost. Nejdříve zjistěte, zda skutečně potřebujete víceřádkovou shodu.

## 5. Znaky a escapování

Obyčejný znak znamená sám sebe. Metaznak hledaný doslovně začněte zpětným lomítkem.

| Hledaný text | Výraz |
| --- | --- |
| Tečka | `\.` |
| Plus | `\+` |
| Otazník | `\?` |
| Hvězdička | `\*` |
| Kulatá závorka | `\(` nebo `\)` |
| Číslo v hranatých závorkách | `\[[0-9]+\]` |
| Zpětné lomítko | `\\` |

`.` znamená jeden znak dostupného úseku, ne doslovnou tečku. V UTF-8 záleží skutečné zpracování na adaptéru Scintilla a standardní knihovně. Neměřte emoji podle představy „jedna tečka = jeden viditelný znak“.

`\r` a `\n` zapisují CR/LF, ale hledání přes řádky tím nezapnete. Pro mezery a tabulátory použijte `[ \t]`.

PCRE2 `\Q...\E` zde není přenositelné označení doslovného úseku. Escapujte jednotlivé metaznaky.

## 6. Třídy, rozsahy a hranice slov

`[abc]` vybere jeden znak sady, `[a-z]` znak rozsahu a `[^abc]` znak mimo sadu. Opakování lze přidat za třídu: `[0-9]+`.

| Třída | Praktický význam |
| --- | --- |
| `[0-9]` | ASCII číslice |
| `[A-Za-z]` | ASCII latinské písmeno |
| `[ \t]` | Obyčejná mezera nebo tabulátor |
| `[^<]` | Dostupný znak jiný než < |
| `[^"']` | Znak jiný než oba typy rovných uvozovek |
| `\d`, `\D` | Třída číslic a její doplněk podle implementace |
| `\s`, `\S` | Bílý znak a jeho doplněk |
| `\w`, `\W` | Slovní znak a jeho doplněk |

Negovaná třída nerozumí XML. `[^<]*` je užitečné pro jednoduchý obsah mezi značkami, ne úplný parser entit, komentářů a CDATA.

`\b` znamená hranici slova a `\B` její nepřítomnost. U atributů to nestačí: `id` může být za dvojtečkou v jiném názvu. Vhodnější je výslovný oddělovač atributu — začátek řádku, mezera nebo tabulátor.

## 7. Kotvy a opakování

`^` a `$` označují začátek a konec fyzického řádku současné cesty. Pro řádek z vodorovných mezer:

```regex
^[ \t]+$
```

Smazání shody neodstraní samotný řádek: jeho konec nebyl součástí výsledku.

| Kvantifikátor | Počet opakování |
| --- | --- |
| `?` | Nula či jedno |
| `*` | Nula či více |
| `+` | Jedno či více |
| `{3}` | Přesně tři |
| `{3,}` | Alespoň tři |
| `{2,5}` | Dvě až pět |

C++11 dovoluje líné formy jako `*?` a `+?`. U XML atributu je často přehlednější vyloučit uzavírací uvozovku:

```regex
"[^"\r\n]*"
```

To platí pro dvojité uvozovky; jednoduché potřebují odpovídající větev.

Nepřenášejte přivlastňovací kvantifikátory, atomické skupiny ani inline volby PCRE2.

## 8. Skupiny, alternativa a lookahead

`( ... )` zachytí text pro odkazy a náhradu. `(?: ... )` seskupuje bez čísla.

Výběr značek:

```regex
<(?:strong|emphasis)>
```

Číselný odkaz může svázat otevírací a zavírací názvy jednoduchého jednořádkového úseku:

```regex
<(strong|emphasis)>[ \t]*</\1>
```

Vyžaduje shodný název, neověřuje libovolné XML vnoření.

`(?=...)` a `(?!...)` ověřují následující text bez jeho zahrnutí. Hranice za názvem zabrání tomu, aby `<a` znamenalo i začátek `<author>`:

```regex
<a(?=[ \t>])
```

Levý kontext zachyťte a vraťte v náhradě: lookbehind není dostupný. `(?<=...)` z Návrhu sem nekopírujte.

## 9. Nahrazování v Kódu

FBE používá `SCI_REPLACETARGETRE`: shoda již existuje a Scintilla rozvíjí náhradu. Nejde o `std::regex_replace` s `$1` ani o gramatiku náhrad Design. [S1, S2]

### 9.1. Odkazy na skupiny

| V poli náhrady | Význam |
| --- | --- |
| `\0` | Celá shoda |
| `\1` … `\9` | Obsah příslušných skupin |
| `$1` | Není odkazem na skupinu této cesty |

Najít:

```regex
\(([0-9]+)\)
```

Nahradit za:

```text
[\1]
```

`(12)` se změní na `[12]`. Smysl to má jen ve zvoleném kontextu: číslo v kulatých závorkách nemusí být poznámkou.

Dolar ve Source není třeba zdvojovat podle pravidel cizího systému. Zpětné lomítko naopak zůstává řídicím znakem.

### 9.2. Řídicí znaky v náhradě

Zkoumaný kód Scintilla rozvine `\t`, `\n`, `\r` a `\\` na tabulátor, LF, CR a doslovné lomítko. Nezávisí to na nemožnosti hledání přes řádky. [S2]

Pro CRLF použijte `\r\n`, pro LF `\n`, podle dokumentu. Nevytvářejte omylem smíšené konce řádků.

PCRE2 zápis `\x{00A0}` nevloží Unicode znak; pro NBSP je nutný skutečný znak. `\U`, `\L`, `\T`, `\S`, `\E`, `\Q` z Návrhu zde neřídí velikost ani formátování.

### 9.3. Mazání a integrita XML

Prázdné pole smaže nalezený rozsah. Smazání prázdného prvku, atributu nebo odkazu může poškodit dokument i po správném vyhledání. Strukturální postupy jsou proto převážně diagnostické.

Změna `id` na jediném místě může zanechat odkazy na starou hodnotu. U souvisejících objektů používejte přejmenování ID ve FBE, ne nezávislé textové náhrady.

## 10. Jak číst XML postupy

XML rozlišuje velikost: `<p>` a `<P>` jsou jiné názvy. Pořadí atributů neurčuje význam; hodnoty mohou být v jednoduchých nebo dvojitých uvozovkách a kolem `=` jsou přípustné mezery. Příklady pokrývají běžné varianty, pokud to nepřiměřeně nezkomplikuje vzor. [S3]

`l:` a `xlink:` jsou běžné prefixy XLink FB2. Význam určuje deklarace `xmlns`, nikoli samotný název prefixu. Vzor s těmito dvěma názvy nepozná automaticky jiný; upravte jej podle deklarace.

Mnohé ukázky přibližují obsah otevíracího tagu pomocí `[^>]*`. Pro běžné FB2 je to praktické, ale `>` může legálně být uvnitř hodnoty atributu. Také komentáře a CDATA mohou obsahovat text podobný značkám. Jde o výběr kandidátů ke kontrole, ne úplnou validaci či slepou přestavbu XML.

„Prázdný prvek“ není vždy chyba schématu a „číselná entita“ není nutně poškozený znak. Provizorní název, zvláštní identifikátor nebo externí odkaz mohou být oprávněné.

## 11. Praktické postupy FB2/XML

Není-li uvedeno jinak, všechny zúčastněné části musí ležet na jednom fyzickém řádku. Při hledání celé otevírací značky na něj patří i kontrolované atributy. Vzor hledající jen atribut tuto podmínku na celou značku neklade.

### K01. Mezery na konci fyzického řádku

Najít koncové obyčejné mezery a tabulátory.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[ \t]+$
```

Výchozí testovací text:

```text
<p>Текст</p>
```

Tentýž testovací text s viditelnými značkami:

```text
<p>Текст</p>␠␠␠
```

Značka ␠ znamená obyčejnou mezeru, [TAB] tabulátor, [NBSP] U+00A0, [NNBSP] U+202F a [ZWSP] U+200B. Jde o vysvětlující zápis; tyto značky se do knihy nevkládají.

Nahradit za: nechte pole úplně prázdné. Nepište slovo „prázdné“.

Výsledek nahrazení:

```text
<p>Текст</p>
```

Protipříklad, který se nemá najít:

```text
<p>Текст</p>
```


Omezení a poznámky: Nemaže konec řádku. Ve smíšeném XML, CDATA či oblastech významových mezer může být konec součástí textu. Globální čištění neoznačujte za bezpodmínečně bezpečné.

### K02. Vyčistit řádek jen s mezerami

Odstranit bílá místa z řádku bez dalšího obsahu.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
^[ \t]+$
```

Výchozí testovací text:

```text

<p>Text</p>
```

Tentýž testovací text s viditelnými značkami:

```text
␠␠␠[TAB]
<p>Text</p>
```

Značka ␠ znamená obyčejnou mezeru, [TAB] tabulátor, [NBSP] U+00A0, [NNBSP] U+202F a [ZWSP] U+200B. Jde o vysvětlující zápis; tyto značky se do knihy nevkládají.

Nahradit za: nechte pole úplně prázdné. Nepište slovo „prázdné“.

Výsledek nahrazení:

```text

<p>Text</p>
```

Protipříklad, který se nemá najít:

```text
<p>Text</p>
```


Omezení a poznámky: Řádek zůstane prázdný: LF/CRLF nebyly součástí nálezu. Nejde o odstranění všech prázdných fyzických řádků.

### K03. Mezera před koncem prázdného prvku

Najít obyčejný odstup těsně před />.

Akce: nahrazení po kontrole.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
[ \t]+/>
```

Výchozí testovací text:

```text
<empty-line />
```

Nahradit za:

```text
/>
```

Výsledek nahrazení:

```text
<empty-line/>
```

Protipříklad, který se nemá najít:

```text
<empty-line/>
```


Omezení a poznámky: XML dovoluje obě podoby. Jde o kosmetiku, ne povinnou opravu. Sekvence může být v textu či komentáři, takže globální změnu kontrolujte.

### K04. Jednoduchý prázdný odstavec

Zkontrolovat dvojici p bez textu.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
<p>[ \t]*</p>
```

Výchozí testovací text:

```text
<p>  </p><p>Text</p>
```

Očekávaná shoda:

```text
<p>  </p>
```

Protipříklad, který se nemá najít:

```text
<p>Text</p>
```


Omezení a poznámky: Nepokrývá atributy, konce řádků, entitu NBSP nebo vnořenou značku. Prázdný odstavec a empty-line nejsou automaticky zaměnitelné.

### K05. Dvě empty-line na stejném řádku

Najít sousední prázdné FB2 řádky zapsané na jedné fyzické řádce.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>
```

Výchozí testovací text:

```text
<empty-line/> <empty-line />
```

Očekávaná shoda:

```text
<empty-line/> <empty-line />
```

Protipříklad, který se nemá najít:

```text
<empty-line/>
<empty-line/>
```


Omezení a poznámky: Dovoluje jen obyčejné mezery a tabulátory. Konec řádku mezi prvky je záměrně vyloučen. Dvojice prázdných řádků může oddělovat scény.

### K06. Prázdná důležitá metadata

Zkontrolovat několik jednoduchých metadatových polí bez hodnoty.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
<(book-title|first-name|middle-name|last-name|genre|lang)>[ \t]*</\1>
```

Výchozí testovací text:

```text
<book-title> </book-title>
```

Očekávaná shoda:

```text
<book-title> </book-title>
```

Protipříklad, který se nemá najít:

```text
<book-title>Book</book-title>
```


Omezení a poznámky: Ne všechna pole jsou vždy povinná. Regex neověřuje schéma FB2, rodiče ani správnost vyplněných hodnot.

### K07. Prázdné inline formátování

Najít prázdné párové prvky formátování.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
<(strong|emphasis|strikethrough)>[ \t]*</\1>
```

Výchozí testovací text:

```text
<strong> </strong>
```

Očekávaná shoda:

```text
<strong> </strong>
```

Protipříklad, který se nemá najít:

```text
<strong>Text</strong>
```


Omezení a poznámky: NBSP, atributy a vnoření vyžadují jiné pravidlo. Před odstraněním ověřte strukturální význam.

### K08. Stejné formátování vnořené do sebe

Rozlišit strong/strong od správného strong/emphasis.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
<(strong|emphasis)>[ \t]*<\1(?=[ \t>])
```

Výchozí testovací text:

```text
<strong><strong>Text</strong></strong>
```

Očekávaná shoda:

```text
<strong><strong
```

Protipříklad, který se nemá najít:

```text
<strong><emphasis>Text</emphasis></strong>
```


Omezení a poznámky: Druhý výběr není nezávislý: odkaz vyžaduje první název. Nález neobsahuje celý prvek a není určen k prostému smazání.

### K09. Možné HTML značky po importu

Najít běžné HTML názvy k ověření ve FB2.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
</?(?:b|i|br|div|span|font)(?=[ \t/>])[^>]*>
```

Výchozí testovací text:

```text
<div>Text</div>
```

Očekávané shody postupně:

```text
<div>
</div>
```

Protipříklad, který se nemá najít:

```text
<section><p>Text</p></section>
```


Omezení a poznámky: Text v komentáři nebo CDATA může také vyhovět. Nenahrazujte b za strong či i za emphasis bez kontroly obsahu a atributů.

### K10. Značka velkými písmeny

Najít běžné názvy značek celé z velkých písmen.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
</?[A-Z][A-Z0-9_.:-]*(?=[ \t/>])[^>]*>
```

Výchozí testovací text:

```text
<P>Text</P>
```

Očekávané shody postupně:

```text
<P>
</P>
```

Protipříklad, který se nemá najít:

```text
<p>Text</p>
```


Omezení a poznámky: Rozlišování velikosti musí být zapnuté, jinak se najde i obyčejné p. Vzor neobsahuje všechny legální XML Unicode názvy a nehodnotí jmenné prostory.

### K11. HTML entita NBSP

Najít doslovné &nbsp; ve zdrojovém souboru.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
&nbsp;
```

Výchozí testovací text:

```text
<p>&nbsp;</p>
```

Očekávaná shoda:

```text
&nbsp;
```

Protipříklad, který se nemá najít:

```text
<p>&#160;</p>
```


Omezení a poznámky: Bez odpovídající deklarace není &nbsp; jednou z pěti předdefinovaných XML entit. Ověřte DTD a komentáře. Nerozvíjejte hromadně všechny entity.

### K12. Číselné odkazy na znaky

Najít desítkový nebo šestnáctkový zápis znaku.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
&#(?:[0-9]+|x[0-9A-Fa-f]+);
```

Výchozí testovací text:

```text
<p>&#160; &#xA0;</p>
```

Očekávané shody postupně:

```text
&#160;
&#xA0;
```

Protipříklad, který se nemá najít:

```text
<p>&amp;</p>
```


Omezení a poznámky: Obě formy mohou být platné. Kontroluje se tvar, ne platnost kódového bodu. Rozvedení &lt; na < v textu může XML poškodit.

### K13. Prázdné id

Najít nevyplněný běžný atribut id v obou typech uvozovek.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:""|'')
```

Výchozí testovací text:

```text
<section id="">
```

Očekávaná shoda:

```text
 id=""
```

Protipříklad, který se nemá najít:

```text
<section id="s1">
```


Omezení a poznámky: Mezera před atributem je součást shody. Prefixová jména jako xml:id se nehledají. Nové ID vyžaduje jedinečnost a aktualizované odkazy, což regex neřeší.

### K14. Seznam hodnot id

Najít vyplněné běžné identifikátory pro kontrolu názvů.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:"[^"]+"|'[^']+')
```

Výchozí testovací text:

```text
<section id="s1">
```

Očekávaná shoda:

```text
 id="s1"
```

Protipříklad, který se nemá najít:

```text
<section id="">
```


Omezení a poznámky: Seznam nepotvrzuje jedinečnost ani přípustnost ID. Související objekty přejmenovávejte funkcí FBE.

### K15. Externí odkazy HTTP(S)

Najít href s běžným XLink prefixem a externí adresou.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"https?://[^"]+"|'https?://[^']+')
```

Výchozí testovací text:

```text
<a l:href="https://example.test/book">Text</a>
```

Očekávaná shoda:

```text
 l:href="https://example.test/book"
```

Protipříklad, který se nemá najít:

```text
<a l:href="#note1">1</a>
```


Omezení a poznámky: Hledá atribut, ne všechny URL v textu. Zahrnuje l a xlink; jiné prefixy doplňte zvlášť. Neověřuje dostupnost adresy.

### K16. Místní odkazy file://

Najít místní souborový odkaz, který nemusí u čtenáře fungovat.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"file://[^"]+"|'file://[^']+')
```

Výchozí testovací text:

```text
<a xlink:href="file:///C:/Book/image.png">Text</a>
```

Očekávaná shoda:

```text
 xlink:href="file:///C:/Book/image.png"
```

Protipříklad, který se nemá najít:

```text
<a xlink:href="#image1">Text</a>
```


Omezení a poznámky: Neotvírejte automaticky neznámé adresy. Nález nerozhodne, zda soubor vložit, odkaz změnit nebo odstranit.

### K17. Prázdný cíl odkazu

Najít běžný prázdný XLink odkaz.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:""|'')
```

Výchozí testovací text:

```text
<a l:href="">Text</a>
```

Očekávaná shoda:

```text
 l:href=""
```

Protipříklad, který se nemá najít:

```text
<a l:href="#note1">Text</a>
```


Omezení a poznámky: Zvlášť kontrolujte prázdný viditelný text a zcela chybějící atribut href: jde o odlišné případy.

### K18. Cíl #undefined

Najít doslovný provizorní odkaz.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')
```

Výchozí testovací text:

```text
<a xlink:href="#undefined">Text</a>
```

Očekávaná shoda:

```text
 xlink:href="#undefined"
```

Protipříklad, který se nemá najít:

```text
<a xlink:href="#note1">Text</a>
```


Omezení a poznámky: Ověřte skutečnou existenci objektu. undefined není XML zakázaný název, i když často signalizuje nedokončené připojení.

### K19. Odkazy s prefixem bookmark

Najít typické konverzní cíle bookmark, nikoli všechny vnitřní odkazy.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#bookmark[^"]*"|'#bookmark[^']*')
```

Výchozí testovací text:

```text
<a l:href="#bookmark12">Text</a>
```

Očekávaná shoda:

```text
l:href="#bookmark12"
```

Protipříklad, který se nemá najít:

```text
<a l:href="#note1">Text</a>
```


Omezení a poznámky: Samotné href="#..." najde libovolný vnitřní odkaz. bookmark nedokazuje nadbytečnost; mazat až po posouzení.

### K20. Zpětné odkazy Word/FBD

Najít obvyklá _ftnref a _ednref v cílech.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#_(?:ftnref|ednref)[^"]*"|'#_(?:ftnref|ednref)[^']*')
```

Výchozí testovací text:

```text
<a l:href="#_ftnref1">Back</a>
```

Očekávaná shoda:

```text
l:href="#_ftnref1"
```

Protipříklad, který se nemá najít:

```text
<a l:href="#note1">Text</a>
```


Omezení a poznámky: Zpětný odkaz může sloužit navigaci. Neodstraňujte jej jen podle původu názvu.

### K21. Odkazy poznámek bez závislosti na pořadí atributů

Najít otevírací a s type=note a vnitřním XLink-href v libovolném pořadí.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
<a(?=[ \t>])(?=[^>]*[ \t]type[ \t]*=[ \t]*(?:"note"|'note'))(?=[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+'))[^>]*>
```

Výchozí testovací text:

```text
<a l:href="#note1" type="note">1</a>
```

Očekávaná shoda:

```text
<a l:href="#note1" type="note">
```

Protipříklad, který se nemá najít:

```text
<a type="link" l:href="#note1">1</a>
```


Omezení a poznámky: Povoluje i type před href, xlink a jednoduché uvozovky. Všechny kontrolované atributy musí být na stejném fyzickém řádku. Hodnoty s >, komentáře a neobvyklé značky potřebují XML parser. Cíl poznámky se nekontroluje.

### K22. Možné číselné značky poznámek

Najít čísla v hranatých, složených nebo kulatých závorkách.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
\[[0-9]+\]|\{[0-9]+\}|\([0-9]+\)
```

Výchozí testovací text:

```text
<p>Text [12], {3}, (4).</p>
```

Očekávané shody postupně:

```text
[12]
{3}
(4)
```

Protipříklad, který se nemá najít:

```text
<p>[note]</p>
```


Omezení a poznámky: Mohou to být bibliografické odkazy, čísla vzorců, vysvětlení nebo obyčejný text. Postup nevytváří poznámky ani nehlídá jedinečné číslování.

### K23. Zapomenuté hodnoty Your/Name

Zkontrolovat běžné anglické zástupné hodnoty jména v metadatech.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
<first-name>Your</first-name>|<last-name>Name</last-name>
```

Výchozí testovací text:

```text
<first-name>Your</first-name>
```

Očekávaná shoda:

```text
<first-name>Your</first-name>
```

Protipříklad, který se nemá najít:

```text
<first-name>John</first-name>
```


Omezení a poznámky: Ověřte autora/tvůrce souboru a pravé jméno. Automaticky nepřebírejte údaje jiné edice.

### K24. Nepřipojená ilustrace #undefined

Najít image odkazující na obvyklý placeholder.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
<image(?=[ \t/>])[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')[^>]*>
```

Výchozí testovací text:

```text
<image l:href="#undefined"/>
```

Očekávaná shoda:

```text
<image l:href="#undefined"/>
```

Protipříklad, který se nemá najít:

```text
<image l:href="#cover"/>
```


Omezení a poznámky: FB2 zpravidla používá XLink-href, ne HTML-src. Binary s daným ID ověřte zvlášť. Chybějící obraz a chybný odkaz mají jiné příčiny.

### K25. Deklarace windows-1251

Najít údaj staršího kódování v XML deklaraci.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
<\?xml[ \t]+[^?]*encoding[ \t]*=[ \t]*(?:"windows-1251"|'windows-1251')[^?]*\?>
```

Výchozí testovací text:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Očekávaná shoda:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Protipříklad, který se nemá najít:

```text
<?xml version="1.0" encoding="utf-8"?>
```


Omezení a poznámky: Sama o sobě to není chyba. Přepsání windows-1251 na utf-8 nepřekóduje bajty. Použijte uložení či konverzi odpovídající deklaraci.

### K26. Ampersand bez známé standardní entity

Najít & před zápisem, který neodpovídá běžným standardním odkazům.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
&(?!(?:amp|lt|gt|apos|quot);|#[0-9]+;|#x[0-9A-Fa-f]+;)
```

Výchozí testovací text:

```text
<p>A & B</p>
```

Očekávaná shoda:

```text
&
```

Protipříklad, který se nemá najít:

```text
<p>A &amp; B</p>
```


Omezení a poznámky: V CDATA a komentáři je & dovolen a DTD může definovat další entity. Jde o diagnostický filtr, ne validátor. & → &amp; bez kontextu způsobí dvojité escapování.

### K27. Běžné vnitřní odkazy

Najít místní cíl #id bez záměny za bookmark.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+')
```

Výchozí testovací text:

```text
<a l:href="#note1">1</a>
```

Očekávaná shoda:

```text
l:href="#note1"
```

Protipříklad, který se nemá najít:

```text
<a l:href="https://example.test">Text</a>
```


Omezení a poznámky: Ukazuje zápis odkazu, nepotvrzuje právě jeden cílový id. Chybějící a duplicitní cíle potřebují analýzu dokumentu.

### K28. Rozdělit dvě empty-line po jednořádkovém hledání

Ukázat rozdíl mezi omezením hledání a vložením nového řádku náhradou.

Akce: jedno kontrolované nahrazení.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
(<empty-line[ \t]*/>)[ \t]*(<empty-line[ \t]*/>)
```

Výchozí testovací text:

```text
<empty-line/> <empty-line/>
```

Nahradit za:

```text
\1\r\n\2
```

Výsledek nahrazení:

```text
<empty-line/>
<empty-line/>
```

Protipříklad, který se nemá najít:

```text
<empty-line/>
<empty-line/>
```


Omezení a poznámky: Scintilla rozvine náhradu do CRLF. Pro dokument LF použijte \1\n\2. Nejde o obecný XML formátovač ani zapnutí víceřádkového hledání.

### K29. Dvojité mezery v jednoduchém XML textu

Najít jednoduchý úsek mezi tagy s násobnými mezerami.

Akce: pouze vyhledání a kontrola.

„Regulární výraz“ zapnuto; „Pouze celá slova“ vypnuto. „Rozlišovat velikost písmen“ zapnuto.

Najít:

```regex
>[^<]*[ \t]{2,}[^<]*<
```

Výchozí testovací text:

```text
<p>one  two</p>
```

Očekávaná shoda:

```text
>one  two<
```

Protipříklad, který se nemá najít:

```text
<p>one two</p>
```


Omezení a poznámky: Shoda obsahuje úhlové oddělovače a celý jednoduchý fragment. Pro slova je obvykle vhodnější Návrh. Nenahrazujte celý úsek XML jedinou mezerou.

## 12. Proč regex nenahrazuje validaci XML

Výraz může ukázat, že fragment vypadá jako požadovaná posloupnost znaků. Neověří správnost celého dokumentu: vnoření, schéma FB2, deklarace jmenných prostorů, jedinečnost ID a existenci cílových prvků.

Dvě stejná `id` na odlišných fyzických řádcích nelze spolehlivě odhalit jedním srovnáním současného regex. Totéž platí pro chybějící cíl jinde v knize. Použijte kontrolu FBE a strukturální analýzu.

Nerozvíjejte všechny XML entity: `&lt;` a `&amp;` často chrání text před přeměnou na značky. Nemažte vše podobné HTML uvnitř CDATA, komentářů a citací kódu.

Změna značky musí zachovat pár, atributy a obsah. Přepsání samotného otevíracího `<strong>` nechá staré uzavření. Příklad se zpětným odkazem nezaručuje bezpečnost každé následné náhrady.

## 13. Časté chyby

### 13.1. Místo velkých značek se nacházejí malé

Zapněte Rozlišovat velikost písmen. V XML to není kosmetika; `[A-Z]` bez rozlišování přestává kontrolovat právě velká písmena.

### 13.2. V náhradě se objeví doslovné $1

Použijte `\1`. Source náhrada FBE vede přes Scintillu, ne JavaScript nebo formát `$1` jiného API.

### 13.3. Nefunguje \p, \K nebo lookbehind

To patří k jinému profilu. Textový úkol přesuňte do Návrhu, XML přepište pomocí výslovných tříd, zachycení kontextu, nezachycovacích skupin a lookahead.

### 13.4. Sousední značky se nenajdou

Zkontrolujte fyzický konec řádku, mezeru před `/>`, jednoduché uvozovky, doplňující atributy a prefix jmenného prostoru. Sousedství v sazbě zdroje není stejné jako sousedství ve stromu XML.

### 13.5. Poznámka se nenajde při jiném pořadí atributů

Vzor „nejprve type, potom href“ na pořadí závisí. K21 ověřuje obě podmínky oddělenými lookahead. Je-li značka víceřádková, hledejte samotný atribut nebo pracujte se strukturou.

### 13.6. strong/emphasis je chybně považováno za vadné vnoření

Různé platné styly se smějí vnořovat. Pro opakování stejného názvu je nutný odkaz jako K08, ne dvě nezávislé volby ze stejného seznamu.

### 13.7. „Bookmark“ nachází všechny poznámky

Vnitřní odkaz s `#` není sám o sobě konverzní bookmark. Oddělte obecnou kontrolu K27 od konkrétního prefixu K19.

### 13.8. Náhrada poškodí viditelný text

V Kódu mohou mezery a fragmenty patřit textu, atributu, komentáři nebo binárním datům. Vraťte změnu a zúžte rozsah či vzor. Název „bezpečná náhrada“ neruší potřebu XML kontextu.

## 14. Omezení profilu Source

Nepoužívejte UCP, PCRE2 vlastnosti, lookbehind, moderní pojmenované skupiny JavaScriptu/PCRE2, atomické skupiny, přivlastňovací kvantifikátory, branch reset, podprogramy, PCRE2 verby, `\K`, `\G` a formátovací příkazy náhrad Design.

Inline volby PCRE2 nenahrazují nastavení dialogu Kódu. Funkce novějšího JavaScriptu nejsou automaticky součástí C++11 ECMAScript.

MatchOnLines zpracovává jednotlivé řádky bez ohledu na zápis konce řádku ve vzoru. Kompilace jiným lokálním modulem nedokazuje kompatibilitu FBE.

Pro Unicode slova a složitou korekturu použijte Návrh. Pro strukturu jsou určeny XML/FB2 nástroje, nikoli neomezené `.*` kompenzující všechny limity.

## 15. Výkon a dlouhé řádky XML

Krátký vzor není nutně rychlý. Neomezená opakování a dlouhé soupeřící alternativy mohou zpomalit obrovské řádky, zvlášť když jediná řádka obsahuje celý dokument nebo velké binary.

Začínejte konkrétním názvem značky či atributu a preferujte omezené třídy. Samostatné postupy se lépe kontrolují a vracejí než hledání všech myslitelných chyb najednou.

Nespojujte celé XML do jednoho řádku kvůli obcházení MatchOnLines. Změníte dokument a nezaručíte přijatelnou rychlost.

## 16. Kontrola po nahrazení

Porovnejte očekávaný a skutečný rozsah. U skupin ověřte uvozovky, prefixy, úhlové závorky a uzavírací značky. Zkontrolujte, že se nedotkly komentáře, CDATA a binary.

Validujte FB2, uložte a po významných změnách znovu otevřete. Ověřte přepínání Kód ↔ Návrh, poznámky, obrázky a viditelný text. Při přejmenování ID kontrolujte objekt i všechny související odkazy.

## 17. Zdroje a rozsah platnosti

Příručka rozšiřuje dodaný regex-source.md. Zachovává postup modul → syntaxe → náhrady → XML → omezení a zpřesňuje příliš obecné příklady. Vysvětlení i ukázky jsou přepracované a technické rozdíly ověřené primárními zdroji.

[S1] Oficiální dokumentace Scintilla, Searching: C++11 režim, vyhledávací volby a SCI_REPLACETARGETRE.

`https://www.scintilla.org/ScintillaDoc.html#Searching`

[S2] Scintilla v revizi FBE Next d2257405d95b0328649acee64b38829b40a4314b: Document.cxx, Cxx11RegexFindText, MatchOnLines, BuiltinRegex::SubstituteByPosition. Rozlišuje hledání přes řádky od vložení CR/LF náhradou.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/third_party/scintilla/src/Document.cxx`

[S3] W3C XML 1.0 a Namespaces in XML: názvy, atributy, entity a jmenné prostory.

`https://www.w3.org/TR/xml/`

`https://www.w3.org/TR/xml-names/`
