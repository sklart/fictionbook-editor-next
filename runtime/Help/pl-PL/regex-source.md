# Podręcznik wyrażeń regularnych — Kod

:::note
Uwaga do tłumaczenia: rosyjskie słowa i zdania w przykładach kontrolnych pozostawiono celowo bez zmian. Są to dane testowe: wyrażenia regularne, ciągi zastępujące, białe znaki i oczekiwane wyniki odpowiadają rosyjskiemu wydaniu wzorcowemu. Objaśnienia przetłumaczono; przykładów nie dostosowano automatycznie do polskich zasad typografii.
:::

Pełny przewodnik po wyszukiwaniu i zamianie w źródle XML książki w FictionBook Editor Next.

Wydanie: 2 października 2026 r. Tryb Projekt opisano w regex-design.md. Przepisy tego dokumentu odnoszą się do Kodu; modeli PCRE2 nie wolno przenosić bez sprawdzenia.

## 1. Zastosowanie wyszukiwania w Kodzie

Kod pokazuje tagi, atrybuty, odsyłacze, encje i tekst książki. Nadaje się do audytu FB2: pustych elementów, importowanych tagów HTML, odsyłaczy zastępczych, nietypowych atrybutów i pozostałości po konwersji.

:::note
Regex bada tekst XML, nie przeanalizowane drzewo DOM. Strukturę sprawdzaj narzędziem XML.
:::

Do korekty zwykłych słów często wygodniejszy jest Projekt. W XML dopasowanie może wystąpić nie tylko w książce, lecz także w atrybucie, komentarzu, CDATA, nazwie pliku lub danych binarnych. Uwzględnij to przed zamianą.

„Tylko wyszukiwanie i weryfikacja” oznacza listę kandydatów, nie dowód błędnego XML lub tekstu. Nawet zamiana spacji może naruszyć znaczący tekst. Nie ma tu uniwersalnie bezpiecznych zamian w dowolnym XML.

## 2. Szybki start

Zapisz kopię, przejdź do Kodu, otwórz Znajdź i włącz Wyrażenie regularne. Przykłady XML zwykle wymagają Uwzględniaj wielkość liter oraz wyłączonego Tylko całe wyrazy. Granice tagów i atrybutów określa wzorzec.

Prosty pusty akapit:

```regex
<p>[ \t]*</p>
```

Znajduje `<p></p>` i `<p>   </p>` w jednym fizycznym wierszu, nie `<p>Текст</p>`. Nie zamieniaj automatycznie na pusty tekst albo `<empty-line/>`: mogą mieć różne role strukturalne.

:::example
Znajdź: <p>[ \t]*</p>
Wejście: <p>   </p>
Dopasowanie: <p>   </p>
Oczekiwane: Sprawdź ten akapit przed zmianą XML.
:::

Wklej tylko wzorzec, bez `/.../g`, cudzysłowów C++ i podwojonych ukośników z JSON.

## 3. Silnik: Scintilla, nie PCRE2

FBE stosuje Scintillę z `SCFIND_REGEXP` i `SCFIND_CXX11REGEX`. Badana kompilacja używa implementacji C++ z gramatyką ECMAScript. Nie jest to PCRE2 ani wszystkie funkcje współczesnego JavaScript. [S1, S2]

Scintilla ma też starszy silnik podstawowy z innymi konwencjami. Nie mieszaj dawnych przykładów grup z nawiasami poprzedzonymi ucieczką z C++11 w FBE: `(слово)` przechwytuje, a `\(слово\)` wymaga dosłownych nawiasów.

Unicode w XML jest dozwolone, lecz nie ma oddzielnego UCP i właściwości PCRE2. Klasy zależą od biblioteki standardowej; `\w` nie oznacza wszystkich liter dowolnego języka. Dla przewidywalnego ASCII używaj `[A-Za-z0-9_]`; dla rosyjskiego `[А-Яа-яЁё]`, pamiętając, że nie obejmuje całej cyrylicy.

### 3.1. Najważniejsze różnice względem Projektu

| Właściwość | Projekt | Kod |
| --- | --- | --- |
| Przeszukiwany obiekt | Reprezentacja tekstowa książki | Źródło XML |
| Silnik | PCRE2-16 | Scintilla C++11 |
| UCP i właściwości Unicode | Dostępne w profilu PCRE2 | Brak składni właściwości PCRE2 |
| Lookbehind | Dostępny z ograniczeniami PCRE2 | Niedostępny |
| Pierwsza grupa w zamianie | `$1` lub `\1` | `\1` |
| Formatowanie przy zamianie | Polecenia FBE | Tylko tekst XML |
| Przekraczanie fizycznych wierszy | Zależne od reprezentacji; zamiany między akapitami ograniczone | Obecnie nieobsługiwane |

## 4. Wyszukiwanie wiersz po wierszu: najważniejszy limit

W ścieżce Scintilla używanej przez FBE, `MatchOnLines` dopasowuje wzorzec do każdego fizycznego wiersza oddzielnie. Wiersz może być bardzo długi; zawinięcie na ekranie nie tworzy nowego fizycznego wiersza. [S2]

:::warning
W bieżącym wyszukiwaniu regex w Kodzie nie przechodzi przez fizyczne wiersze; żaden wzorzec ich nie połączy.
:::

Porównaj:

```xml
<empty-line/> <empty-line/>
```

oraz:

```xml
<empty-line/>
<empty-line/>
```

Dla XML mogą opisywać takie samo sąsiedztwo. Dla obecnego wyszukiwania FBE pierwsza forma może być jednym dopasowaniem, a druga przekracza granicę wiersza i nie może.

Dodanie `\r\n`, `\n`, `\x0D`, `\x0A`, `\s*` lub klasy „dowolny znak” nie usuwa ograniczenia. Silnik nie dostaje obu wierszy jako jednego fragmentu. Wymagałoby to zmiany ścieżki programu, nie tylko wzorca.

Wyszukiwanie wierszowe nie zabrania wstawienia nowego wiersza w zamianie. To osobna operacja, pokazana w K28. Odróżniaj wyszukiwanie przez granicę wiersza od wstawienia takiej granicy po wyniku.

### 4.1. Praca z wielowierszowym XML

Dla atrybutu w osobnym wierszu często wystarczy szukać samego atrybutu bez całego tagu. Relacje między wierszami sprawdzaj narzędziami strukturalnymi FBE, walidatorem, skryptem lub narzędziem XML.

Nie usuwaj wszystkich nowych wierszy z książki dla jednego wzorca. Możesz zmienić tekst, komentarze, CDATA i czytelność źródła. Najpierw ustal, czy zadanie faktycznie wymaga dopasowania wielowierszowego.

## 5. Znaki i ucieczki

Zwykły znak oznacza siebie. Metaznak szukany dosłownie poprzedź ukośnikiem odwrotnym.

| Szukany tekst | Wyrażenie |
| --- | --- |
| Kropka | `\.` |
| Plus | `\+` |
| Pytajnik | `\?` |
| Gwiazdka | `\*` |
| Nawias okrągły | `\(` lub `\)` |
| Liczba w nawiasach kwadratowych | `\[[0-9]+\]` |
| Ukośnik odwrotny | `\\` |

`.` oznacza jeden znak dostępnego fragmentu, nie dosłowną kropkę. W dokumencie UTF-8 rzeczywista obsługa znaków zależy od adaptera Scintilla i biblioteki standardowej. Nie mierz długości emoji przy założeniu „jedna kropka = jeden widoczny znak”.

`\r` i `\n` zapisują CR i LF, ale nie pozwalają obecnie dopasować tekstu przez granice wierszy. Do spacji i tabulatorów stosuj `[ \t]`.

PCRE2 `\Q...\E` nie jest tutaj przenośną metodą oznaczania tekstu dosłownego. Uciekaj metaznaki pojedynczo.

## 6. Klasy, zakresy i granice wyrazu

`[abc]` znajduje jeden znak ze zbioru, `[a-z]` jeden z zakresu, a `[^abc]` jeden spoza niego. Po klasie można dodać powtórzenie, np. `[0-9]+`.

| Klasa | Znaczenie praktyczne |
| --- | --- |
| `[0-9]` | Cyfra ASCII |
| `[A-Za-z]` | Łacińska litera ASCII |
| `[ \t]` | Zwykła spacja lub tabulator |
| `[^<]` | Dostępny znak inny niż < |
| `[^"']` | Znak inny niż dwa typy prostych cudzysłowów |
| `\d`, `\D` | Klasa cyfr i jej dopełnienie według implementacji |
| `\s`, `\S` | Białe znaki i ich dopełnienie |
| `\w`, `\W` | Znaki wyrazu i ich dopełnienie |

Klasa negatywna nie rozumie XML. `[^<]*` pomaga w prostym tekście między tagami, ale nie jest pełnym parserem encji, komentarzy ani CDATA.

`\b` oznacza granicę wyrazu, `\B` jej brak. Dla atrybutów to często za mało: `id` może wystąpić po dwukropku w innym imieniu. Lepiej jawnie sprawdzić separator atrybutu: początek wiersza, spację lub tabulator.

## 7. Kotwice i powtórzenia

`^` i `$` dotyczą początku i końca fizycznego wiersza w bieżącej ścieżce. Wiersz złożony tylko z poziomych odstępów:

```regex
^[ \t]+$
```

Usunięcie dopasowania nie usuwa samego wiersza: znak końca nie został dopasowany.

| Kwantyfikator | Liczba powtórzeń |
| --- | --- |
| `?` | Zero lub jedno |
| `*` | Zero lub więcej |
| `+` | Jedno lub więcej |
| `{3}` | Dokładnie trzy |
| `{3,}` | Co najmniej trzy |
| `{2,5}` | Od dwóch do pięciu |

C++11 dopuszcza leniwe formy, np. `*?` i `+?`. Dla atrybutu XML zwykle czytelniej wykluczyć zamykający cudzysłów:

```regex
"[^"\r\n]*"
```

Dotyczy podwójnych cudzysłowów; pojedyncze wymagają właściwej odrębnej gałęzi.

Nie przenoś kwantyfikatorów dzierżawczych PCRE2, grup atomowych ani opcji wewnętrznych PCRE2.

## 8. Grupy, alternatywa i lookahead

Grupa `( ... )` przechwytuje tekst dla odwołań i zamiany. `(?: ... )` łączy elementy bez numeru.

Wybór kilku tagów:

```regex
<(?:strong|emphasis)>
```

Odwołanie liczbowe może połączyć nazwy otwarcia i zamknięcia prostego fragmentu jednowierszowego:

```regex
<(strong|emphasis)>[ \t]*</\1>
```

Wymaga tej samej nazwy w obu pozycjach, lecz nie sprawdza dowolnego zagnieżdżenia XML.

Pozytywny `(?=...)` i negatywny `(?!...)` lookahead sprawdzają dalszy tekst bez włączania go do dopasowania. Granica po nazwie tagu zapobiega dopasowaniu `<a` na początku `<author>`:

```regex
<a(?=[ \t>])
```

Kontekst z lewej przechwyć i odtwórz w zamianie: lookbehind nie jest dostępny. Nie kopiuj `(?<=...)` z podręcznika Projektu.

## 9. Zamiana w trybie Kod

FBE używa `SCI_REPLACETARGETRE`: dopasowanie już istnieje, a Scintilla rozwija tekst zamiany według własnych zasad. To nie `std::regex_replace` z `$1` ani gramatyka Design FBE. [S1, S2]

### 9.1. Odwołania do grup

| W polu zamiany | Znaczenie |
| --- | --- |
| `\0` | Całe dopasowanie |
| `\1` … `\9` | Zawartość odpowiednich grup |
| `$1` | Nie używaj jako odwołania: to nie składnia tej ścieżki |

Znajdź:

```regex
\(([0-9]+)\)
```

Zamień na:

```text
[\1]
```

`(12)` staje się `[12]`. Ma to sens tylko we właściwym kontekście: liczba w nawiasie nie musi być odsyłaczem do przypisu.

Dolar w Source nie wymaga podwajania według zasad innego systemu. Ukośnik odwrotny pozostaje znakiem sterującym.

### 9.2. Znaki sterujące w zamianie

Kod Scintilla z badanej wersji rozwija `\t`, `\n`, `\r` i `\\` do tabulatora, LF, CR i dosłownego ukośnika. Nie jest to sprzeczne z brakiem wyszukiwania wielowierszowego. [S2]

CRLF zapisuj `\r\n`, a LF jako `\n`, zgodnie z dokumentem. Nie twórz przypadkowo mieszanych zakończeń wierszy.

PCRE2 `\x{00A0}` nie wstawia tutaj znaku Unicode; do NBSP potrzebny jest rzeczywisty znak. `\U`, `\L`, `\T`, `\S`, `\E`, `\Q` z Design nie sterują tu wielkością ani formatowaniem.

### 9.3. Usuwanie i zachowanie XML

Puste pole usuwa dopasowany zakres. Usunięcie pustego tagu, atrybutu lub odsyłacza może uszkodzić dokument mimo poprawnego wyszukania. Przepisy strukturalne są dlatego głównie diagnostyczne.

Zmiana `id` tylko w jednym miejscu może pozostawić odsyłacze do starej wartości. Używaj wbudowanej zmiany identyfikatora dla obiektów powiązanych zamiast niezależnych zamian tekstowych.

## 10. Czytanie przepisów XML

XML uwzględnia wielkość liter: `<p>` i `<P>` to inne nazwy. Kolejność atrybutów nie określa ich sensu; wartości mogą mieć pojedyncze lub podwójne cudzysłowy, a wokół `=` dozwolone są spacje. Przykłady obejmują typowe warianty, jeżeli nie komplikuje to nadmiernie wzorca. [S3]

`l:` i `xlink:` są częstymi prefiksami XLink w FB2. Znaczenie nadaje deklaracja `xmlns`, nie sama nazwa prefiksu. Przepis wymieniający te dwa warianty nie gwarantuje obsługi innego; dostosuj go po sprawdzeniu deklaracji.

Wiele przykładów przybliża zawartość otwierającego tagu przez `[^>]*`. Jest to wygodne dla zwykłego FB2, lecz `>` może legalnie wystąpić w wartości atrybutu. Komentarze i CDATA również mogą zawierać tekst podobny do znaczników. Przepisy wybierają kandydatów do obejrzenia, nie zastępują pełnej walidacji lub bezpiecznej przebudowy XML.

„Pusty element” nie zawsze oznacza błąd schematu, a „encja liczbowa” nie oznacza uszkodzonego znaku. Atrapa, nietypowa nazwa lub zewnętrzny odsyłacz również mogą być uzasadnione.

## 11. Praktyczne przepisy FB2/XML

Domyślnie części biorące udział w dopasowaniu muszą znajdować się w jednym fizycznym wierszu. Gdy szukany jest cały tag otwierający, wszystkie sprawdzane atrybuty również muszą być w tym wierszu. Wzorzec szukający tylko atrybutu nie nakłada tego wymagania na cały tag.

### K01. Spacje na końcu fizycznego wiersza

Znaleźć końcowe zwykłe spacje i tabulatory.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[ \t]+$
```

Wejściowy tekst testowy:

```text
<p>Текст</p>
```

Ten sam tekst testowy z widocznymi oznaczeniami:

```text
<p>Текст</p>␠␠␠
```

Symbol ␠ oznacza zwykłą spację, [TAB] tabulator, [NBSP] U+00A0, [NNBSP] U+202F, a [ZWSP] U+200B. To zapis objaśniający; oznaczeń nie należy wstawiać do książki.

Zamień na: pozostaw pole całkowicie puste. Nie wpisuj słowa „puste”.

Wynik zamiany:

```text
<p>Текст</p>
```

Przykład, który nie powinien zostać znaleziony:

```text
<p>Текст</p>
```


Ograniczenia i uwagi: Nie usuwa końca wiersza. W mieszanej zawartości XML, CDATA i obszarach znaczących spacji końcówka może być częścią tekstu. Nie uznawaj globalnego czyszczenia za bezwarunkowo bezpieczne.

### K02. Oczyścić wiersz złożony wyłącznie z odstępów

Usunąć białe znaki z wiersza bez innej treści.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
^[ \t]+$
```

Wejściowy tekst testowy:

```text

<p>Text</p>
```

Ten sam tekst testowy z widocznymi oznaczeniami:

```text
␠␠␠[TAB]
<p>Text</p>
```

Symbol ␠ oznacza zwykłą spację, [TAB] tabulator, [NBSP] U+00A0, [NNBSP] U+202F, a [ZWSP] U+200B. To zapis objaśniający; oznaczeń nie należy wstawiać do książki.

Zamień na: pozostaw pole całkowicie puste. Nie wpisuj słowa „puste”.

Wynik zamiany:

```text

<p>Text</p>
```

Przykład, który nie powinien zostać znaleziony:

```text
<p>Text</p>
```


Ograniczenia i uwagi: Wiersz pozostanie pusty: LF/CRLF nie były częścią dopasowania. To nie usuwanie wszystkich pustych fizycznych wierszy.

### K03. Spacja przed zamknięciem pustego elementu

Znaleźć zwykły odstęp bezpośrednio przed />.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[ \t]+/>
```

Wejściowy tekst testowy:

```text
<empty-line />
```

Zamień na:

```text
/>
```

Wynik zamiany:

```text
<empty-line/>
```

Przykład, który nie powinien zostać znaleziony:

```text
<empty-line/>
```


Ograniczenia i uwagi: XML dopuszcza oba zapisy. Zmiana jest kosmetyczna, nie obowiązkową naprawą. Sekwencja może wystąpić w tekście lub komentarzu, dlatego zamiana zbiorcza wymaga kontroli.

### K04. Prosty pusty akapit

Sprawdzić parę p bez tekstu.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
<p>[ \t]*</p>
```

Wejściowy tekst testowy:

```text
<p>  </p><p>Text</p>
```

Oczekiwane dopasowanie:

```text
<p>  </p>
```

Przykład, który nie powinien zostać znaleziony:

```text
<p>Text</p>
```


Ograniczenia i uwagi: Nie obejmuje atrybutów, nowego wiersza, encji NBSP ani zagnieżdżonego tagu. Pustego akapitu i empty-line nie traktuj automatycznie zamiennie.

### K05. Dwa empty-line w jednym wierszu

Znaleźć sąsiednie puste wiersze FB2 zapisane w tej samej fizycznej linii.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>
```

Wejściowy tekst testowy:

```text
<empty-line/> <empty-line />
```

Oczekiwane dopasowanie:

```text
<empty-line/> <empty-line />
```

Przykład, który nie powinien zostać znaleziony:

```text
<empty-line/>
<empty-line/>
```


Ograniczenia i uwagi: Dozwolone są tylko zwykłe spacje i tabulatory. Koniec wiersza między elementami celowo nie jest obsługiwany. Dwie puste linie mogą rozdzielać sceny.

### K06. Puste ważne metadane

Sprawdzić kilka prostych pól metadanych bez zawartości.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
<(book-title|first-name|middle-name|last-name|genre|lang)>[ \t]*</\1>
```

Wejściowy tekst testowy:

```text
<book-title> </book-title>
```

Oczekiwane dopasowanie:

```text
<book-title> </book-title>
```

Przykład, który nie powinien zostać znaleziony:

```text
<book-title>Book</book-title>
```


Ograniczenia i uwagi: Nie wszystkie pola są obowiązkowe w każdym kontekście. Regex nie sprawdza schematu FB2, rodzica ani poprawności wypełnionych wartości.

### K07. Puste formatowanie inline

Znaleźć puste pary elementów formatowania.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
<(strong|emphasis|strikethrough)>[ \t]*</\1>
```

Wejściowy tekst testowy:

```text
<strong> </strong>
```

Oczekiwane dopasowanie:

```text
<strong> </strong>
```

Przykład, który nie powinien zostać znaleziony:

```text
<strong>Text</strong>
```


Ograniczenia i uwagi: NBSP, atrybuty i zagnieżdżenie wymagają innego wzorca. Przed usunięciem sprawdź rolę strukturalną.

### K08. To samo formatowanie zagnieżdżone w sobie

Odróżnić strong/strong od prawidłowego strong/emphasis.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
<(strong|emphasis)>[ \t]*<\1(?=[ \t>])
```

Wejściowy tekst testowy:

```text
<strong><strong>Text</strong></strong>
```

Oczekiwane dopasowanie:

```text
<strong><strong
```

Przykład, który nie powinien zostać znaleziony:

```text
<strong><emphasis>Text</emphasis></strong>
```


Ograniczenia i uwagi: Drugi element nie jest wybierany niezależnie: odwołanie wymaga pierwszej nazwy. Dopasowanie nie obejmuje całego elementu i nie jest przeznaczone do prostego usuwania.

### K09. Możliwe tagi HTML po imporcie

Znaleźć popularne nazwy HTML wymagające kontroli w FB2.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
</?(?:b|i|br|div|span|font)(?=[ \t/>])[^>]*>
```

Wejściowy tekst testowy:

```text
<div>Text</div>
```

Oczekiwane dopasowania, kolejno:

```text
<div>
</div>
```

Przykład, który nie powinien zostać znaleziony:

```text
<section><p>Text</p></section>
```


Ograniczenia i uwagi: Tekst w komentarzu lub CDATA również może pasować. Nie zamieniaj b na strong i i na emphasis bez sprawdzenia treści oraz atrybutów.

### K10. Tag wielkimi literami

Znaleźć zwykłe nazwy tagów w całości zapisane wielkimi literami.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
</?[A-Z][A-Z0-9_.:-]*(?=[ \t/>])[^>]*>
```

Wejściowy tekst testowy:

```text
<P>Text</P>
```

Oczekiwane dopasowania, kolejno:

```text
<P>
</P>
```

Przykład, który nie powinien zostać znaleziony:

```text
<p>Text</p>
```


Ograniczenia i uwagi: Uwzględnianie wielkości jest konieczne, inaczej znajdziesz zwykłe p. Wzorzec nie obejmuje wszystkich legalnych nazw Unicode XML i nie sprawdza przestrzeni nazw.

### K11. Encja HTML NBSP

Znaleźć dosłowny zapis &nbsp; w źródle.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
&nbsp;
```

Wejściowy tekst testowy:

```text
<p>&nbsp;</p>
```

Oczekiwane dopasowanie:

```text
&nbsp;
```

Przykład, który nie powinien zostać znaleziony:

```text
<p>&#160;</p>
```


Ograniczenia i uwagi: Bez odpowiedniej deklaracji &nbsp; nie jest jedną z pięciu predefiniowanych encji XML. Sprawdź DTD i komentarze. Nie dekoduj masowo wszystkich encji.

### K12. Liczbowe odwołania do znaków

Znaleźć dziesiętny lub szesnastkowy zapis znaku.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
&#(?:[0-9]+|x[0-9A-Fa-f]+);
```

Wejściowy tekst testowy:

```text
<p>&#160; &#xA0;</p>
```

Oczekiwane dopasowania, kolejno:

```text
&#160;
&#xA0;
```

Przykład, który nie powinien zostać znaleziony:

```text
<p>&amp;</p>
```


Ograniczenia i uwagi: Obie postacie mogą być poprawne. Kontrolowany jest format, nie legalność punktu kodowego. Rozwinięcie &lt; do < w tekście może uszkodzić XML.

### K13. Pusty id

Znaleźć niewypełniony zwykły atrybut id z pojedynczymi lub podwójnymi cudzysłowami.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:""|'')
```

Wejściowy tekst testowy:

```text
<section id="">
```

Oczekiwane dopasowanie:

```text
 id=""
```

Przykład, który nie powinien zostać znaleziony:

```text
<section id="s1">
```


Ograniczenia i uwagi: Dopasowanie zawiera spację przed atrybutem. Nazwy prefiksowe jak xml:id są pomijane. Tworzenie nowego ID musi uwzględnić odsyłacze i unikalność, czego regex nie robi.

### K14. Lista wartości id

Znaleźć wypełnione zwykłe identyfikatory do przeglądu nazewnictwa.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:"[^"]+"|'[^']+')
```

Wejściowy tekst testowy:

```text
<section id="s1">
```

Oczekiwane dopasowanie:

```text
 id="s1"
```

Przykład, który nie powinien zostać znaleziony:

```text
<section id="">
```


Ograniczenia i uwagi: Lista dopasowań nie potwierdza unikalności i dopuszczalności ID. Obiekty powiązane zmieniaj funkcją FBE.

### K15. Zewnętrzne odsyłacze HTTP(S)

Znaleźć href z typowym prefiksem XLink i adresem zewnętrznym.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"https?://[^"]+"|'https?://[^']+')
```

Wejściowy tekst testowy:

```text
<a l:href="https://example.test/book">Text</a>
```

Oczekiwane dopasowanie:

```text
 l:href="https://example.test/book"
```

Przykład, który nie powinien zostać znaleziony:

```text
<a l:href="#note1">1</a>
```


Ograniczenia i uwagi: Szuka atrybutu, nie każdego URL w tekście. Obsługuje l i xlink; inne prefiksy uwzględnij osobno. Nie sprawdza dostępności adresu.

### K16. Lokalne odsyłacze file://

Znaleźć odniesienie do pliku lokalnego, które może nie działać u czytelnika.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"file://[^"]+"|'file://[^']+')
```

Wejściowy tekst testowy:

```text
<a xlink:href="file:///C:/Book/image.png">Text</a>
```

Oczekiwane dopasowanie:

```text
 xlink:href="file:///C:/Book/image.png"
```

Przykład, który nie powinien zostać znaleziony:

```text
<a xlink:href="#image1">Text</a>
```


Ograniczenia i uwagi: Nie otwieraj automatycznie nieznanego adresu. Wynik nie rozstrzyga, czy osadzić plik, zmienić odsyłacz czy go usunąć.

### K17. Pusty cel odsyłacza

Znaleźć zwykły pusty odsyłacz XLink.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:""|'')
```

Wejściowy tekst testowy:

```text
<a l:href="">Text</a>
```

Oczekiwane dopasowanie:

```text
 l:href=""
```

Przykład, który nie powinien zostać znaleziony:

```text
<a l:href="#note1">Text</a>
```


Ograniczenia i uwagi: Osobno sprawdzaj pustą treść widoczną i brak atrybutu href — to inne przypadki.

### K18. Cel #undefined

Znaleźć dosłowny odsyłacz zastępczy.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')
```

Wejściowy tekst testowy:

```text
<a xlink:href="#undefined">Text</a>
```

Oczekiwane dopasowanie:

```text
 xlink:href="#undefined"
```

Przykład, który nie powinien zostać znaleziony:

```text
<a xlink:href="#note1">Text</a>
```


Ograniczenia i uwagi: Sprawdź istnienie obiektu. Nazwa undefined nie jest sama w sobie zabroniona przez XML, choć często sygnalizuje niedokończone powiązanie.

### K19. Odsyłacze z prefiksem bookmark

Znaleźć typowe cele bookmark konwertera, nie wszystkie odsyłacze wewnętrzne.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#bookmark[^"]*"|'#bookmark[^']*')
```

Wejściowy tekst testowy:

```text
<a l:href="#bookmark12">Text</a>
```

Oczekiwane dopasowanie:

```text
l:href="#bookmark12"
```

Przykład, który nie powinien zostać znaleziony:

```text
<a l:href="#note1">Text</a>
```


Ograniczenia i uwagi: href="#..." bez dalszego ograniczenia znajduje dowolny odsyłacz wewnętrzny. Obecność bookmark nie dowodzi zbędności linku; usuwać dopiero po kontroli.

### K20. Odsyłacze powrotne Word/FBD

Znaleźć popularne _ftnref i _ednref w celach odsyłaczy.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#_(?:ftnref|ednref)[^"]*"|'#_(?:ftnref|ednref)[^']*')
```

Wejściowy tekst testowy:

```text
<a l:href="#_ftnref1">Back</a>
```

Oczekiwane dopasowanie:

```text
l:href="#_ftnref1"
```

Przykład, który nie powinien zostać znaleziony:

```text
<a l:href="#note1">Text</a>
```


Ograniczenia i uwagi: Odsyłacz powrotny może być potrzebny do nawigacji. Nie usuwaj go tylko z powodu pochodzenia nazwy.

### K21. Odsyłacze przypisów niezależne od kolejności atrybutów

Znaleźć otwierający a z type=note i wewnętrznym XLink-href w dowolnej kolejności.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
<a(?=[ \t>])(?=[^>]*[ \t]type[ \t]*=[ \t]*(?:"note"|'note'))(?=[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+'))[^>]*>
```

Wejściowy tekst testowy:

```text
<a l:href="#note1" type="note">1</a>
```

Oczekiwane dopasowanie:

```text
<a l:href="#note1" type="note">
```

Przykład, który nie powinien zostać znaleziony:

```text
<a type="link" l:href="#note1">1</a>
```


Ograniczenia i uwagi: Dopuszcza też type przed href, xlink i pojedyncze cudzysłowy. Wszystkie sprawdzane atrybuty muszą być w jednym wierszu. Wartości z >, komentarze i nietypowe znaczniki wymagają parsera XML. Cel przypisu nie jest sprawdzany.

### K22. Możliwe liczbowe oznaczenia przypisów

Znaleźć liczby w nawiasach kwadratowych, klamrach lub nawiasach okrągłych.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
\[[0-9]+\]|\{[0-9]+\}|\([0-9]+\)
```

Wejściowy tekst testowy:

```text
<p>Text [12], {3}, (4).</p>
```

Oczekiwane dopasowania, kolejno:

```text
[12]
{3}
(4)
```

Przykład, który nie powinien zostać znaleziony:

```text
<p>[note]</p>
```


Ograniczenia i uwagi: Mogą to być bibliografia, numery wzorów, objaśnienia lub zwykły tekst. Wzorzec nie tworzy przypisów i nie sprawdza unikalności numeracji.

### K23. Pozostawione wartości Your/Name

Sprawdzić typowe angielskie atrapy nazwiska w metadanych.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
<first-name>Your</first-name>|<last-name>Name</last-name>
```

Wejściowy tekst testowy:

```text
<first-name>Your</first-name>
```

Oczekiwane dopasowanie:

```text
<first-name>Your</first-name>
```

Przykład, który nie powinien zostać znaleziony:

```text
<first-name>John</first-name>
```


Ograniczenia i uwagi: Sprawdź kontekst autora lub twórcy pliku i prawdziwe dane. Nie zastępuj automatycznie danymi innego wydania.

### K24. Niepowiązana ilustracja #undefined

Znaleźć image odwołujący się do typowej atrapy.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
<image(?=[ \t/>])[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')[^>]*>
```

Wejściowy tekst testowy:

```text
<image l:href="#undefined"/>
```

Oczekiwane dopasowanie:

```text
<image l:href="#undefined"/>
```

Przykład, który nie powinien zostać znaleziony:

```text
<image l:href="#cover"/>
```


Ograniczenia i uwagi: FB2 używa zwykle XLink-href, nie HTML-src. Obecność binary o danym ID sprawdza się osobno. Brak obrazu i błędny odsyłacz mają różne przyczyny.

### K25. Deklaracja windows-1251

Znaleźć wskazanie starego kodowania w deklaracji XML.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
<\?xml[ \t]+[^?]*encoding[ \t]*=[ \t]*(?:"windows-1251"|'windows-1251')[^?]*\?>
```

Wejściowy tekst testowy:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Oczekiwane dopasowanie:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Przykład, który nie powinien zostać znaleziony:

```text
<?xml version="1.0" encoding="utf-8"?>
```


Ograniczenia i uwagi: To nie błąd sam w sobie. Zamiana napisu windows-1251 na utf-8 nie przekodowuje bajtów. Kodowanie zmieniaj przez zapis lub konwersję spójną z deklaracją.

### K26. Ampersand bez znanej standardowej encji

Znaleźć & przed ciągiem niepasującym do standardowych form odwołania do znaku.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
&(?!(?:amp|lt|gt|apos|quot);|#[0-9]+;|#x[0-9A-Fa-f]+;)
```

Wejściowy tekst testowy:

```text
<p>A & B</p>
```

Oczekiwane dopasowanie:

```text
&
```

Przykład, który nie powinien zostać znaleziony:

```text
<p>A &amp; B</p>
```


Ograniczenia i uwagi: W CDATA i komentarzach & jest dozwolone, a DTD może definiować inne encje. To filtr, nie walidator XML. Automatyczne & → &amp; bez kontekstu prowadzi do podwójnego kodowania.

### K27. Zwykłe odsyłacze wewnętrzne

Znaleźć lokalny cel #id bez utożsamiania go z bookmark.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+')
```

Wejściowy tekst testowy:

```text
<a l:href="#note1">1</a>
```

Oczekiwane dopasowanie:

```text
l:href="#note1"
```

Przykład, który nie powinien zostać znaleziony:

```text
<a l:href="https://example.test">Text</a>
```


Ograniczenia i uwagi: Regex pokazuje zapis odsyłacza, nie dowodzi istnienia dokładnie jednego celu. Wiszące i powielone odsyłacze wymagają analizy dokumentu.

### K28. Rozdzielić dwa empty-line po wyszukaniu jednowierszowym

Pokazać różnicę między ograniczeniem wyszukiwania a wstawieniem nowego wiersza w zamianie.

Działanie: pojedyncza kontrolowana zamiana.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(<empty-line[ \t]*/>)[ \t]*(<empty-line[ \t]*/>)
```

Wejściowy tekst testowy:

```text
<empty-line/> <empty-line/>
```

Zamień na:

```text
\1\r\n\2
```

Wynik zamiany:

```text
<empty-line/>
<empty-line/>
```

Przykład, który nie powinien zostać znaleziony:

```text
<empty-line/>
<empty-line/>
```


Ograniczenia i uwagi: Sekwencje zamiany Scintilla rozwijają się w CRLF. Dla pliku LF użyj \1\n\2. To nie uniwersalny formater XML ani włączenie wyszukiwania wielowierszowego.

### K29. Podwójne spacje w prostym tekście XML

Znaleźć prosty fragment między tagami z wielokrotnymi spacjami.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
>[^<]*[ \t]{2,}[^<]*<
```

Wejściowy tekst testowy:

```text
<p>one  two</p>
```

Oczekiwane dopasowanie:

```text
>one  two<
```

Przykład, który nie powinien zostać znaleziony:

```text
<p>one two</p>
```


Ograniczenia i uwagi: Wynik zawiera delimitery kątowe i cały prosty fragment. Do poprawiania słów zwykle wygodniejszy jest Projekt. Nie zamieniaj całego fragmentu XML na jedną spację.

## 12. Dlaczego regex nie zastępuje walidacji XML

Wyrażenie może dowieść, że fragment przypomina określoną sekwencję znaków. Nie dowodzi poprawności całego dokumentu: zagnieżdżenia, zgodności ze schematem FB2, przestrzeni nazw, unikalności ID i istnienia celów odsyłaczy.

Dwa takie same `id` w różnych fizycznych wierszach nie są wiarygodnie wykrywalne jednym porównaniem obecnego regex. Podobnie z odsyłaczem, którego cel jest nieobecny w innej części książki. Potrzebna jest kontrola FBE i analiza struktury.

Nie rozwijaj wszystkich encji XML: `&lt;` i `&amp;` często chronią tekst przed staniem się znacznikami. Nie usuwaj wszystkiego, co wygląda jak HTML, wewnątrz CDATA, komentarza lub cytatu kodu.

Zamiana tagu musi zachować parę, atrybuty i treść. Zmiana samego otwierającego `<strong>` pozostawia stare zamknięcie. Przykład z odwołaniem wstecznym nie gwarantuje bezpieczeństwa każdej dalszej zamiany.

## 13. Typowe błędy

### 13.1. Małe tagi znajdowane zamiast wielkich

Włącz Uwzględniaj wielkość liter. W XML to nie kosmetyka: `[A-Z]` przy ignorowaniu wielkości traci sens kontroli wielkich liter.

### 13.2. Dosłowny $1 w wyniku zamiany

Użyj `\1`. Zamiana Source FBE korzysta ze Scintilli, nie z JavaScript ani formatu `$1` innego API.

### 13.3. Nie działa \p, \K lub lookbehind

To inny profil. Do pracy tekstowej przejdź do Projektu albo przepisz zadanie XML na jawne klasy, przechwycenie kontekstu, grupy nieprzechwytujące i lookahead.

### 13.4. Nie znajdują się sąsiednie tagi

Sprawdź fizyczny nowy wiersz, spację przed `/>`, pojedyncze cudzysłowy, dodatkowe atrybuty i prefiks przestrzeni nazw. Sąsiedztwo w sformatowanym źródle różni się od sąsiedztwa w drzewie XML.

### 13.5. Przypis znika przy innym porządku atrybutów

Wzorzec „najpierw type, potem href” zależy od kolejności. K21 sprawdza warunki oddzielnymi lookahead. Gdy tag ma kilka wierszy, wyszukanie całego tagu nadal nie zadziała; szukaj atrybutu albo skorzystaj ze struktury.

### 13.6. Kontrola zagnieżdżenia uznaje strong/emphasis za błąd

Różne poprawne style mogą być zagnieżdżone. Do powtórzenia tej samej nazwy potrzeba odwołania jak w K08, nie dwóch niezależnych grup z tą samą listą.

### 13.7. „Bookmark” znajduje wszystkie przypisy

Odsyłacz z `#` nie jest jeszcze pozostałością bookmark. Oddziel ogólną kontrolę odsyłaczy K27 od konkretnego prefiksu w K19.

### 13.8. Zamiana psuje widoczny tekst

W Kodzie spacje i fragmenty mogą należeć do tekstu, atrybutu, komentarza lub danych binarnych. Cofnij zmianę i ogranicz zakres albo wzorzec. Nazwa „bezpieczna zamiana” nie unieważnia kontekstu XML.

## 14. Ograniczenia profilu Source

Nie używaj UCP, właściwości PCRE2, lookbehind, nazwanych grup współczesnego JavaScript/PCRE2, grup atomowych, kwantyfikatorów dzierżawczych, branch reset, podprogramów, czasowników PCRE2, `\K`, `\G` i poleceń formatowania zamiany Design.

Opcje inline PCRE2 nie zastępują opcji okna Source. Funkcje najnowszego JavaScript nie pojawiają się automatycznie w implementacji ECMAScript C++11.

MatchOnLines działa wierszowo niezależnie od zapisanej sekwencji końca wiersza. Udana kompilacja innym silnikiem nie dowodzi zgodności z FBE.

Dla wyrazów Unicode i złożonej korekty stosuj Projekt. Dla struktury używaj narzędzi XML/FB2 zamiast jednego nieograniczonego `.*`.

## 15. Wydajność i długie wiersze XML

Krótki wzorzec nie musi być szybki. Nieograniczone powtórzenia i długie konkurujące alternatywy spowalniają duże wiersze, szczególnie gdy jedna linia zawiera cały dokument lub duże binary.

Zaczynaj od konkretnej nazwy tagu lub atrybutu, a zamiast „dowolnych znaków” wybieraj ograniczoną klasę. Nie szukaj naraz wszystkich możliwych błędów; osobne przepisy łatwiej sprawdzać i cofać.

Nie sklejaj całego XML w jeden wiersz dla obejścia MatchOnLines: zmienia to dokument i nie gwarantuje szybkości.

## 16. Sprawdzenie po zamianie

Porównaj oczekiwany i rzeczywisty zakres. Przy grupach sprawdź zachowanie cudzysłowów, prefiksów, nawiasów kątowych i zamknięć tagów. Upewnij się, że komentarze, CDATA i binary nie zostały zmienione.

Uruchom walidację FB2, zapisz i po istotnych zmianach otwórz ponownie. Sprawdź przejście Kod ↔ Projekt, przypisy, ilustracje i tekst. Przy zmianie identyfikatora kontroluj obiekt oraz wszystkie powiązane odsyłacze.

## 17. Źródła i zakres stosowania

Podręcznik rozwija dostarczony regex-source.md. Zachowano układ silnik → składnia → zamiana → XML → ograniczenia, zastępując nieprecyzyjne ogólne przepisy węższymi. Wyjaśnienia i próbki są nowe, a różnice techniczne porównano ze źródłami pierwotnymi.

[S1] Oficjalna dokumentacja Scintilla, Searching: tryb C++11, opcje wyszukiwania i SCI_REPLACETARGETRE.

`https://www.scintilla.org/ScintillaDoc.html#Searching`

[S2] Scintilla w wersji FBE Next d2257405d95b0328649acee64b38829b40a4314b: Document.cxx, Cxx11RegexFindText, MatchOnLines, BuiltinRegex::SubstituteByPosition. Kod odróżnia wyszukiwanie przez wiersze od wstawiania CR/LF w zamianie.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/third_party/scintilla/src/Document.cxx`

[S3] W3C XML 1.0 i Namespaces in XML: nazwy, atrybuty, encje i przestrzenie nazw.

`https://www.w3.org/TR/xml/`

`https://www.w3.org/TR/xml-names/`
