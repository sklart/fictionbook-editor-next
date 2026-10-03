# Podręcznik wyrażeń regularnych — Projekt

Uwaga do tłumaczenia: rosyjskie słowa i zdania w przykładach kontrolnych pozostawiono celowo bez zmian. Są to dane testowe: wyrażenia regularne, ciągi zastępujące, białe znaki i oczekiwane wyniki odpowiadają rosyjskiemu wydaniu wzorcowemu. Objaśnienia przetłumaczono; przykładów nie dostosowano automatycznie do polskich zasad typografii.

Pełny przewodnik po wyszukiwaniu, zamianie i korekcie książek w FictionBook Editor Next.

Wydanie: 2 października 2026 r. Tryb Kod opisuje osobny dokument regex-source.md. „Wzorzec” oznacza tutaj wyrażenie regularne, natomiast „wbudowany szablon” — zapisany zestaw wyrażenia i parametrów w panelu Szablony. Nazwa Design oznacza wizualny tryb Projekt.

## 1. Jak korzystać z podręcznika

Wyrażenie regularne opisuje regułę wyszukiwania, a nie jeden dokładny ciąg. Na przykład `[0-9]+` znajduje ciąg cyfr dowolnej długości, a `[ \t]{2,}` — co najmniej dwie zwykłe spacje lub tabulatory. Znalezione dopasowanie i tekst, który ma je zastąpić, to odrębne rzeczy.

Pierwszy praktyczny wynik można uzyskać według rozdziału 2. Rozdziały 3–15 wyjaśniają składnię, a rozdział 16 — szczególną gramatykę zamiany FBE. Rozdział 17 zawiera przepisy do pracy z książkami: warunki, próbki i ostrzeżenia. Na końcu znajdują się typowe problemy, wskazówki wydajnościowe i źródła.

Do pola Znajdź należy kopiować wyłącznie wyrażenie. Etykiety „Znajdź”, „Zamień”, oznaczenia U+0020 i otaczające znaczniki Markdown nie należą do wzorca. Nie są potrzebne delimitery JavaScript `/.../g`, cudzysłowy literału C++ ani podwojone ukośniki odwrotne z JSON.

Stan opcji ma znaczenie. Dopasowanie uzyskane bez uwzględniania wielkości liter może się zmienić po jej włączeniu. Ustawienia podane przy przepisie są jego częścią, nie ozdobnikiem.

„Tylko wyszukiwanie i weryfikacja” oznacza diagnostykę. Znaleziony tekst może być poprawny: zamierzonym powtórzeniem, obcym nazwiskiem, cytatem, tytułem lub świadomą typografią. „Zamiana po sprawdzeniu” również nie gwarantuje bezpieczeństwa w każdej książce.

## 2. Pierwsze wyszukiwanie i bezpieczna zamiana

Zapisz roboczą kopię książki. Przejdź do Projektu, otwórz Znajdź lub Zamień i włącz Wyrażenie regularne. Na początek wyłącz Tylko całe wyrazy: lepiej określać granice w samym wzorcu. Sprawdź zakres i kierunek wyszukiwania.

Wyszukiwanie kilku spacji:

```regex
[ \t]{2,}
```

W tekście `Он   пришёл` zostanie znaleziony odstęp z trzech spacji. W polu zamiany umieść jedną zwykłą spację. Najpierw wykonaj pojedynczą zamianę, sprawdź `Он пришёл`, a dopiero potem rozważ Zamień wszystko.

Przy wielu dopasowaniach najpierw przejrzyj wyniki i popraw jeden lub dwa reprezentatywne przypadki. Po operacji zbiorczej sprawdź tekst, kursywę, pogrubienie, przypisy i granice akapitów. Nieoczekiwany rezultat cofnij przed następną serią zmian.

Przycisk Zastosuj w panelu szablonów przenosi wyrażenie i opcje do okna wyszukiwania. Nie jest poleceniem bezwarunkowego poprawienia wszystkich miejsc. Wyszukiwanie i zamiana mają osobne polecenia w oknie.

## 3. Silnik i granice trybu Projekt

Tryb używa PCRE2-16, przekazując tekst i wzorce jako UTF-16; UTF jest stale włączone. FBE osobno buduje tekst do przeszukania, a osobno wykonuje zamianę z uwzględnieniem struktury książki. Dokumentacja PCRE2 wyjaśnia więc dopasowanie, lecz nie wszystkie działania FBE. Podstawy techniczne podano w źródłach D1–D4.

Przeszukiwana jest tekstowa reprezentacja książki, nie dosłowny XML pliku. `<strong>` nie jest sposobem na znalezienie pogrubienia w Projekcie. Tagi wyszukuje się w Kodzie, a przekształcenia strukturalne wykonuje narzędziami edytora lub specjalnymi skryptami.

Zawijanie długiego wiersza na ekranie nie tworzy znaku końca wiersza. Akapity, rzeczywiste podziały i zawijanie wizualne to różne pojęcia. FBE stosuje wielowierszowe kotwice w reprezentacji wyszukiwania, aby początki i końce wierszy tekstowych odzwierciedlały granice akapitów. Dopasowanie przez granicę akapitu nie oznacza możliwości takiej zamiany: badana implementacja odrzuca zamiany między akapitami.

`\A` i `\z` odnoszą się do początku i końca ciągu przekazanego silnikowi. Nie należy utożsamiać ich automatycznie z całym FB2: liczą się zakres i fragment utworzony przez FBE.

## 4. Unicode: UTF, UCP i wielkość liter

### 4.1. Rola UTF

UTF pozwala przetwarzać znaki Unicode, w tym cyrylicę. Nie zamienia alfabetów, nie poprawia OCR, nie zmienia `ё` na `е` i nie łączy automatycznie kanonicznie równoważnych zapisów liter.

Litera z akcentem może być jednym znakiem albo literą i oddzielnym znakiem łączącym. Wygląd może być podobny, ale wyszukiwanie znakowe inne. PCRE2 nie przeprowadza normalizacji NFC/NFD za użytkownika. Przy znakach diakrytycznych uwzględnij `\p{M}`.

### 4.2. Rola UCP

Unicode (UCP) zmienia przede wszystkim znaczenie klas `\w`, `\d`, `\s` oraz zależnych od nich granic `\b` i `\B`. Nie jest przełącznikiem samej obsługi cyrylicy.

Uściślenie wcześniejszych wersji pomocy: jawne właściwości `\p{L}`, `\p{N}` i `\P{...}` są dostępne w kompilacji Unicode PCRE2 również bez UCP. Wzorzec łączący `\p{L}` z `\b` zwykle warto jednak stosować z UCP, aby litery i granice wyrazu miały spójną interpretację.

Porównaj wyszukiwanie rosyjskiego wyrazu:

```regex
\bмир\b
```

Z UCP granice wynikają z klasy wyrazu Unicode. Bez UCP nie oczekuj, że `\b` zachowa się dla cyrylicy tak samo jak dla łacińskiego ASCII. Brak dopasowania nie dowodzi braku wyrazu.

Gdy potrzebne są dokładnie cyfry ASCII, użyj `[0-9]`, nie `\d`: UCP może rozszerzyć tę ostatnią klasę na cyfry dziesiętne innych systemów pisma.

### 4.3. Przydatne właściwości

| Wyrażenie | Co znajduje |
| --- | --- |
| `\p{L}` | Literę Unicode |
| `\p{Lu}` | Wielką literę przy uwzględnianiu wielkości liter |
| `\p{Ll}` | Małą literę przy uwzględnianiu wielkości liter |
| `\p{M}` | Znak łączący |
| `\p{N}` | Znak liczbowy, pojęcie szersze od cyfry dziesiętnej |
| `\p{Nd}` | Cyfrę dziesiętną |
| `\p{Latin}` | Znak pisma łacińskiego |
| `\p{Cyrillic}` | Znak cyrylicy |
| `\P{L}` | Znak niebędący literą |

Sekwencja liter wraz ze znakami diakrytycznymi:

```regex
[\p{L}\p{M}]+
```

To nie uniwersalna definicja językowa wyrazu: łączniki i apostrofy nie są uwzględnione. Dodawaj je świadomie.

### 4.4. Wielkość liter to oddzielne ustawienie

Włącz Uwzględniaj wielkość liter przy anomaliach typu `строчнаяПрописная`, inicjałach i wzorcach `\p{Lu}`/`\p{Ll}`. Tryb niewrażliwy na wielkość może zniweczyć sens reguły. UCP nie zastępuje tej opcji.

Włączenie ignorowania wielkości liter we wzorcu:

```regex
(?i)глава
```

Ograniczenie działania do grupy:

```regex
(?i:глава)[ \t]+[0-9]+
```

## 5. Znaki dosłowne i ucieczki

Litery i większość znaków oznaczają siebie. Poza klasami specjalne znaczenie mogą mieć kropka, nawiasy, gwiazdka, plus, pytajnik, klamry, kotwice i ukośnik odwrotny.

Aby wyszukać metaznak dosłownie, poprzedź go ukośnikiem odwrotnym:

| Szukany tekst | Wyrażenie |
| --- | --- |
| Kropka | `\.` |
| Pytajnik | `\?` |
| Plus | `\+` |
| Gwiazdka | `\*` |
| Nawias otwierający | `\(` |
| Nawias zamykający | `\)` |
| Liczba w nawiasach kwadratowych | `\[[0-9]+\]` |
| Sam ukośnik odwrotny | `\\` |

`(123)` znajduje cyfry `123` i zapisuje je w grupie; nie wymaga nawiasów w tekście. Aby znaleźć `(123)`, nawiasy trzeba potraktować jako dosłowne.

Dłuższy fragment dosłowny w PCRE2 można otoczyć `\Q` i `\E`:

```regex
\QЦена (руб.) + доставка\E
```

Te same oznaczenia mają inne znaczenie w polu zamiany FBE. Nie przenoś automatycznie reguł ucieczki z wyszukiwania do zamiany.

## 6. Spacje, tabulatory i znaki niewidoczne

| Wyrażenie | Znaczenie w wyszukiwaniu PCRE2 |
| --- | --- |
| `[ ]` | Tylko zwykła spacja U+0020 |
| `[ \t]` | Zwykła spacja lub tabulator |
| `\t` | Tabulator U+0009 |
| `\x{00A0}` | Spacja nierozdzielająca |
| `\x{202F}` | Wąska spacja nierozdzielająca |
| `\h` | Poziomy biały znak, w tym różne spacje Unicode |
| `\s` | Biały znak, potencjalnie również koniec wiersza |
| `\r` | Powrót karetki CR |
| `\n` | Nowy wiersz LF |
| `\R` | Sekwencja podziału wiersza Unicode |
| `\x{00AD}` | Miękki łącznik |
| `\x{200B}` | Spacja o zerowej szerokości |
| `\x{FEFF}` | Znak FEFF wewnątrz tekstu |

Do zwykłego czyszczenia odstępów między wyrazami wybieraj `[ \t]`, nie `\s`. Druga klasa jest szersza i może objąć akapity oraz spacje nierozdzielające. `\h` także nadaje się do diagnostyki, lecz jest zbyt szerokie do nieokreślonej zamiany typograficznej.

NBSP wiąże znak numeru z liczbą, inicjały z nazwiskiem oraz liczbę z jednostką. Masowe zamienianie tych spacji na zwykłe może pogorszyć skład. FBE ma też ustawienie znaku NBSP; przepisy U+00A0 sprawdzaj w odniesieniu do konkretnej książki i wersji programu.

U+200C i U+200D mogą być potrzebne w niektórych pismach i złożonych emoji. Nie usuwaj bezwarunkowo wszystkich znaków „niewidocznych”. Znalezienie znaku nie oznacza błędu.

## 7. Klasy i zakresy znaków

Klasa w nawiasach kwadratowych zużywa jeden znak ze zbioru. `[abc]` oznacza jedną z trzech liter, nie słowo `abc`. `[abc]+` oznacza co najmniej jedną literę z tego zbioru.

`[^abc]` zużywa jeden znak spoza zbioru. Nie jest warunkiem „przed tekstem nie ma abc”; do kontekstu służą lookaround.

W klasie kropka i większość nawiasów tracą znaczenie specjalne. Łącznik może oznaczać zakres, dlatego dosłowny umieszczaj na początku lub końcu albo poprzedzaj ucieczką. Dosłowny nawias kwadratowy zamykający można zapisać `\]`.

```regex
[А-Яа-яЁё-]+
```

Przykład dopuszcza rosyjskie litery i łącznik, lecz nie całą cyrylicę. Ukraińskie, białoruskie i inne litery wymagają szerszego zbioru lub właściwości Unicode.

W nawiasach kwadratowych `\b` nie oznacza granicy wyrazu. Nie umieszczaj granic wyrazu w zwykłym zbiorze znaków.

## 8. Kotwice, granice i puste dopasowania

Kotwica sprawdza pozycję bez zużywania litery czy spacji. Samo `^`, `$` lub `\b` może dać dopasowanie zerowej długości, którego interfejs nie pokazuje jak zwykłego zaznaczonego fragmentu.

| Kotwica | Znaczenie |
| --- | --- |
| `^` | Początek wiersza w multiline, inaczej początek przeszukiwanego obiektu |
| `$` | Koniec wiersza w multiline; szczegóły końcowego podziału zależą od trybu |
| `\A` | Wyłącznie początek przeszukiwanego obiektu |
| `\z` | Dokładny koniec obiektu |
| `\Z` | Koniec obiektu albo pozycja przed końcowym znakiem nowego wiersza |
| `\b` | Granica między znakiem wyrazu a znakiem niewyrazowym |
| `\B` | Pozycja niebędąca granicą wyrazu |
| `\G` | Pozycja początkowa bieżącego wywołania dopasowania |

`\G` sama nie pamięta poprzedniego wyniku. Odnosi się do początkowego przesunięcia przekazanego przez aplikację. Przy kolejnych wywołaniach może to być koniec poprzedniego dopasowania, ale w FBE nie jest to uniwersalna metoda przechodzenia po książce. W korekcie używaj bardziej jawnych granic. [D1]

Dla całego wyrazu, a nie jego fragmentów w dłuższych słowach, zastosuj granice lub sprawdzenie sąsiednich liter. Z UCP:

```regex
\bтом\b
```

Nie dopasuje początku `томик`. Łączniki i apostrofy mogą jednak dzielić wyrazy inaczej dla silnika niż dla redaktora.

Puste dopasowania w Zamień wszystko wymagają ostrożności: wstawiają tekst w pozycjach zamiast zastępować widoczne znaki. Do nauki wybieraj wyniki niepuste.

## 9. Kwantyfikatory: liczba powtórzeń i nawracanie

| Zapis | Powtórzenia poprzedniego elementu |
| --- | --- |
| `?` | Zero lub jedno |
| `*` | Zero lub więcej |
| `+` | Jedno lub więcej |
| `{3}` | Dokładnie trzy |
| `{3,}` | Co najmniej trzy |
| `{2,5}` | Od dwóch do pięciu |

Kwantyfikator dotyczy poprzedniego znaku, klasy lub grupy. `аб+` powtarza tylko `б`; `(?:аб)+` powtarza parę.

### 9.1. Dopasowanie zachłanne

Tekst początkowy:

```text
«первый» и «второй»
```

Wzorzec:

```regex
«.*»
```

Kropka z gwiazdką najpierw pochłania możliwie długi fragment. W razie potrzeby silnik oddaje znaki, aby spełnić dalszą część wzorca. Tutaj wynik obejmuje obie pary cudzysłowów.

### 9.2. Dopasowanie leniwe

```regex
«.*?»
```

Najpierw próbuje minimalnej liczby znaków, ale nadal może rozszerzać dopasowanie. Kolejne wyszukania zwracają `«первый»`, potem `«второй»`.

Dla prostej pary często czytelniej jest jawnie ograniczyć zawartość:

```regex
«[^»\r\n]*»
```

To nie analizator zagnieżdżonych cytatów; wymagają osobnej kontroli redakcyjnej.

### 9.3. Kwantyfikator dzierżawczy, bez oddawania znaków

```regex
«.*+»
```

Nie oznacza „jeszcze lepszej obsługi cudzysłowów”. `.*+` pochłania także zamykający znak i nie oddaje go. Pozostałe `»` nie ma już czego dopasować; na podanym tekście brak wyniku.

Wariant wykluczający znak zamykający może mieć sens:

```regex
«[^»\r\n]*+»
```

Kwantyfikatory dzierżawcze i grupy atomowe służą do sterowania nawracaniem, nie do uniwersalnego przyspieszania każdego wzorca.

## 10. Grupy i odwołania wsteczne

Nawiasy okrągłe zachowują znaleziony fragment. Numeracja zaczyna się od 1 i biegnie po otwierających nawiasach przechwytujących od lewej do prawej, również przy zagnieżdżeniu.

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Na `02.10.2026` grupy zawierają `02`, `10`, `2026`. To kontrola formatu, nie poprawności kalendarzowej.

`(?:...)` grupuje bez nadawania numeru. Pomaga utrzymać przewidywalne `$1` i `$2` w złożonych zamianach.

Grupa nazwana ułatwia czytanie:

```regex
(?<word>\p{L}+)[ \t]+\k<word>
```

Odwołanie wsteczne wymaga tego samego przechwyconego tekstu. Wywołanie podprogramu opisane dalej powtarza regułę i może dopasować inny tekst. To dwa różne mechanizmy.

W książce do powtórzenia wyrazu dodaj granice, właściwe ustawienie wielkości liter i zbiór odstępów. Bez granic można znaleźć części dłuższych słów; gotowy przepis podano niżej.

Nazwane grupy wyszukiwania nie dodają automatycznie nazwanych podstawień w FBE. W zamianie używaj potwierdzonych odwołań liczbowych.

## 11. Alternatywa i grupy atomowe

Pionowa kreska oznacza wybór gałęzi:

```regex
(?:глава|часть)[ \t]+[0-9]+
```

Grupowanie ma znaczenie: bez niego wspólna prawa część może dotyczyć tylko ostatniej gałęzi. Gałęzie są próbowane w zapisanej kolejności; rozważ kolejność krótkich i długich form.

Grupa atomowa zabrania powrotu do wnętrza grupy już pomyślnie dopasowanej:

```regex
(?>а|аб)в
```

Na `абв` pierwsza gałąź wybierze `а`, po czym powrót do `аб` będzie zabroniony. Wyniku nie będzie, choć wariant bez atomowości mógłby go znaleźć. To przykład edukacyjny, nie zachęta do bezmyślnego dodawania atomowości.

## 12. Sprawdzanie sąsiedniego tekstu: lookaround

Lookaround sprawdza kontekst, nie włączając go do pełnego dopasowania. Można więc wybrać sam numer i zachować poprzedzający symbol:

```regex
(?<=№ )[0-9]+
```

Na `№ 125` znajduje tylko `125`. Wymagana jest dokładnie jedna zwykła spacja.

Warunek po prawej:

```regex
[0-9]+(?=[ \t]+руб\.)
```

Na `125 руб.` znajduje liczbę, nie `руб.`.

Negatywny warunek z przodu:

```regex
\bглава\b(?![ \t]+[0-9])
```

Z UCP szuka wyrazu «глава», po którym nie następuje zwykła numeracja oddzielona spacją. To przykład wyboru kontekstu, nie reguła poprawiania książki.

`(?<=...)` sprawdza tekst z tyłu, `(?<!...)` neguje ten warunek; `(?=...)` i `(?!...)` dotyczą tekstu z przodu.

Lookbehind ma ograniczenia długości. Współczesny PCRE2 dopuszcza niektóre ograniczone długości zmienne, nie dowolne nieograniczone powtórzenia. Do przenośnych przepisów wybieraj krótki stały kontekst, przechwycenie lub `\K`; nie zakładaj działania `.*` w lookbehind.

## 13. Opcje wewnątrz wzorca

| Opcja | Działanie |
| --- | --- |
| `(?i)` | Ignoruje wielkość liter |
| `(?-i)` | Uwzględnia wielkość liter |
| `(?m)` | Wielowierszowe kotwice początku i końca |
| `(?-m)` | Wyłącza kotwice wielowierszowe |
| `(?s)` | Pozwala kropce obejmować koniec wiersza |
| `(?-s)` | Przywraca zwykłe zachowanie kropki |
| `(?x)` | Ignoruje nieistotne spacje i komentarze we wzorcu |

`(?m)` nie pozwala kropce przechodzić przez wiersze. `(?s)` nie zezwala FBE na zamianę między akapitami. To dwie opcje dopasowania i osobne ograniczenie aplikacji.

W trybie rozszerzonym spacje we wzorcu mogą przestać być dosłowne. Wymaganą spację zapisz `[ ]`; `#` poza klasą może rozpocząć komentarz. Wielowierszowe modele są czytelne w dokumentacji, lecz przepisy do jednowierszowego pola podano w jednym wierszu.

## 14. Zaawansowane możliwości PCRE2

Większość korekt nie wymaga tego rozdziału. Wyjaśnia on konstrukcje spotykane w cudzych modelach. Obsługa składni nie oznacza, że każdy taki wzorzec jest wygodny lub bezpieczny dla całej książki.

### 14.1. Zresetowanie początku dopasowania

```regex
№[ \t]*\K[0-9]+
```

Na `№ 125` prefiks zostaje sprawdzony, ale dopasowany tekst to tylko `125`. Przydatne, gdy ma się zmienić liczba, nie oznaczenie. Nie używaj `\K` wewnątrz lookaround bez osobnej próby — PCRE2 nakłada ograniczenia.

### 14.2. Warunek udziału grupy

```regex
^(\()?([0-9]+)(?(1)\))$
```

Dopuszcza `12` lub `(12)`, ale nie niezamknięte `(12`. Warunek sprawdza udział pierwszej grupy. Przed zamianą opartą na opcjonalnych grupach sprawdź zachowanie adaptera FBE.

### 14.3. Wspólna numeracja w alternatywach

```regex
(?|глава ([0-9]+)|часть ([0-9]+))
```

W obu gałęziach liczba trafia do grupy 1; to branch reset. W prostych przypadkach grupa nieprzechwytująca ze wspólną pojedynczą grupą jest czytelniejsza.

### 14.4. Wywołanie podprogramu

```regex
(?<pair>[0-9]{2})-(?&pair)
```

Na `12-34` obie części spełniają regułę „dwie cyfry”, choć są różne. Odwołanie `\k<pair>` wymagałoby powtórzenia dokładnie `12`.

Podprogramy rekurencyjne opisują pewne struktury zagnieżdżone, ale nie zastępują parsera XML FBE. Do `<section>`, `<poem>`, przypisów i tabel używaj narzędzi strukturalnych.

### 14.5. Pomijanie fragmentów

```regex
«[^»\r\n]*»(*SKIP)(*FAIL)|\bслово\b
```

Z UCP znajduje wyraz poza prostymi rosyjskimi cudzysłowami. Pierwsza gałąź wyznacza cytat do pominięcia, druga szuka wyrazu. Zagnieżdżone i niezamknięte cytaty nie są analizowane; nie polegaj na tym bez kontroli przy redakcji masowej.

## 15. Czego wyrażenie regularne nie rozstrzyga

Wielka litera po małej, brak znaku końcowego i powtórzenie wyrazu to cechy formalne, nie decyzje redaktora. Regex nie wie, czy `да да` jest pomyłką, zamierzonym dialogiem, wierszem czy tytułem.

Nie dodawaj automatycznie kropek, nie sklejaj wszystkich akapitów zaczynających się małą literą, nie zamieniaj każdej litery łacińskiej na cyrylicę i każdego łącznika na myślnik. Potrzebny jest kontekst, a czasami kilka operacji na DOM. Skrypty FBE do czyszczenia, sklejonych słów i przypisów rozwiązują właśnie bardziej złożone zadania.

## 16. Gramatyka zamiany FBE

PCRE2 wyszukuje, lecz ciąg zamiany interpretuje FBE. Przykładów z JavaScript, Pythona, .NET, PCRE2 substitute i innych edytorów nie przenoś bez sprawdzenia. Poniższe polecenia potwierdzają kod FBE i pierwotna pomoc. [D3, D4]

### 16.1. Dopasowanie i grupy

| W polu zamiany | Znaczenie |
| --- | --- |
| `$0` albo `\0` | Całe dopasowanie |
| `$1` … `$9` | Grupa o podanym numerze |
| `\1` … `\9` | Alternatywny zapis numerowanej grupy |
| `$+` albo `\+` | Ostatnia grupa zwracana przez adapter dopasowania |

Stwierdzenie „FBE zamienia tylko grupy 1–9” wymaga uściślenia: można wstawiać także zwykły tekst i całe dopasowanie. Ograniczony jest bezpośredni dostęp liczbowy, nie liczba grup PCRE2.

Nie stosuj `$10` jako odwołania do grupy dziesiątej. `${name}` nie należy do potwierdzonej gramatyki. Potrzebne przechwycenia trzymaj w pierwszych dziewięciu grupach, a pomocnicze twórz jako nieprzechwytujące.

Grupy opcjonalne i puste wymagają osobnej próby: FBE ma własny adapter SubMatches. Nowych masowych zamian nie opieraj na subtelnościach pominiętych grup lub „ostatniej grupy”; preferuj obowiązkowe przechwycenia.

### 16.2. Zmiana kolejności grup

Znajdź:

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Zamień na:

```text
$3-$2-$1
```

`02.10.2026` zmieni się w `2026-10-02`. To demonstracja przestawiania, nie zalecenie zmiany wszystkich dat w książce.

### 16.3. Zmiana wielkości liter

| Polecenie | Działanie |
| --- | --- |
| `\U` | Włącza wielkie litery we wstawianym fragmencie |
| `\L` | Włącza małe litery |
| `\T` | Pierwsza litera fragmentu wielka, pozostałe małe |
| `\Q` | Resetuje aktywne polecenia wielkości i formatowania dla następnego tekstu |

Znajdź:

```regex
(иван)
```

Zamień na:

```text
\T$1\Q
```

Wynik kontrolny: `Иван`. `\T` nie jest językowo zaawansowaną kapitalizacją każdego wyrazu: pojedynczy fragment `иван иванов` nie musi zmienić się w `Иван Иванов`.

Nie nakładaj `\U` i `\L` bez resetu. Rozdzielaj części przez `\Q` i sprawdzaj cyrylicę oraz diakrytykę. Wielkość zmienia FBE, nie normalizator Unicode PCRE2.

### 16.4. Pogrubienie i kursywa

`\S` włącza pogrubienie wstawianego fragmentu, `\E` kursywę. `\Q` kończy działanie aktywnych poleceń na dalszy tekst.

```text
\S$1\Q
```

Formatuje zawartość grupy 1. Nie wyszukuje już pogrubionego tekstu i nie usuwa całego istniejącego formatowania. Najpierw sprawdź jeden fragment i cofnięcie.

### 16.5. Te same znaki, różne znaczenia

W wyszukiwaniu `\S` to znak niebędący białym znakiem, w zamianie FBE — pogrubienie. `\Q...\E` w wyszukiwaniu oznacza tekst dosłowny; w zamianie `\Q` resetuje polecenia, a `\E` włącza kursywę.

Nie wpisuj `\n`, `\t`, `\x{00A0}`, `\$` ani `$$` do zamiany Design, zakładając zasady innego edytora. Nie należą do opisanej gramatyki literałów FBE; nieznana sekwencja może zostać odrzucona. NBSP wstawiaj jako rzeczywisty znak. Znak specjalny zachowaj przez przechwyconą grupę albo sprawdzoną zwykłą zamianę.

Puste pole zamiany usuwa dopasowanie. Jedna spacja zastępuje je jedną spacją. Etykiet `<пусто>`, `[NBSP]` i `U+00A0` z objaśnień nie wpisuj dosłownie.

## 17. Praktyczne przepisy do redakcji i korekty

Każdy przykład to samodzielny scenariusz, nie gwarancja zgodności z nazwami wbudowanych szablonów. Zamianę najpierw stosuj do jednego dopasowania. Rzeczywiste spacje i tabulatory w próbkach zostały zachowane; przykłady negatywne pokazują co najmniej jedną granicę działania, ale nie zastępują kontroli całej książki.

### D01. Kilka zwykłych spacji zamienianych na jedną

Zagęścić zwykłe odstępy bez zmieniania pojedynczego NBSP.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[ \t]{2,}
```

Wejściowy tekst testowy:

```text
Он   пришёл.
```

Zamień na: jedną zwykłą spację U+0020. Jest to jeden znak, nie słowo „spacja”.

Wynik zamiany:

```text
Он пришёл.
```

Przykład, który nie powinien zostać znaleziony:

```text
Он пришёл.
```


Ograniczenia i uwagi: W poezji, tabelach i symulowanych wcięciach wielokrotne spacje mogą być zamierzone. Sprawdź zakres; nie służy to przebudowie wcięć akapitowych.

### D02. Spacje na początku akapitu

Usunąć ręczne wcięcie przed zwykłym tekstem.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
^[ \t]+
```

Wejściowy tekst testowy:

```text
   Начало абзаца.
```

Ten sam tekst testowy z widocznymi oznaczeniami:

```text
␠␠␠Начало␠абзаца.
```

Symbol ␠ oznacza zwykłą spację, [TAB] tabulator, [NBSP] U+00A0, [NNBSP] U+202F, a [ZWSP] U+200B. To zapis objaśniający; oznaczeń nie należy wstawiać do książki.

Zamień na: pozostaw pole całkowicie puste. Nie wpisuj słowa „puste”.

Wynik zamiany:

```text
Начало абзаца.
```

Przykład, który nie powinien zostać znaleziony:

```text
Начало абзаца.
```


Ograniczenia i uwagi: W poezji i składzie artystycznym ręczne wcięcia bywają znaczące. Kotwica dotyczy wiersza tekstowego, nie zawinięcia na ekranie.

### D03. Spacje na końcu akapitu

Usunąć końcowe zwykłe spacje lub tabulatory.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[ \t]+$
```

Wejściowy tekst testowy:

```text
Конец абзаца.
```

Ten sam tekst testowy z widocznymi oznaczeniami:

```text
Конец␠абзаца.␠␠␠
```

Symbol ␠ oznacza zwykłą spację, [TAB] tabulator, [NBSP] U+00A0, [NNBSP] U+202F, a [ZWSP] U+200B. To zapis objaśniający; oznaczeń nie należy wstawiać do książki.

Zamień na: pozostaw pole całkowicie puste. Nie wpisuj słowa „puste”.

Wynik zamiany:

```text
Конец абзаца.
```

Przykład, który nie powinien zostać znaleziony:

```text
Конец абзаца.
```


Ograniczenia i uwagi: NBSP celowo wykluczono. Nietypowe spacje sprawdzaj osobnym przepisem.

### D04. Spacja przed przecinkiem i innymi znakami

Zachować znak, usuwając odstęp przed nim.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[ \t]+([,;:!?])
```

Wejściowy tekst testowy:

```text
Слово , другое !
```

Zamień na:

```text
$1
```

Wynik zamiany:

```text
Слово, другое!
```

Przykład, który nie powinien zostać znaleziony:

```text
Слово, другое!
```


Ograniczenia i uwagi: Reguła dotyczy zwykłego tekstu rosyjskiego. Typografia francuska dopuszcza specjalne spacje przed niektórymi znakami; nie stosuj przepisu do takiej książki bez dostosowania.

### D05. Spacja po znaku otwierającym

Usunąć zwykłą spację po nawiasie lub rosyjskim cudzysłowie otwierającym.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
([(\[«„])[ \t]+
```

Wejściowy tekst testowy:

```text
« слово» ( пример)
```

Zamień na:

```text
$1
```

Wynik zamiany:

```text
«слово» (пример)
```

Przykład, który nie powinien zostać znaleziony:

```text
«слово» (пример)
```


Ograniczenia i uwagi: Nie zmienia spacji we wzorach i przykładach, o ile nie występują bezpośrednio po wymienionych znakach. Kontekst nadal wymaga kontroli.

### D06. Spacja przed znakiem zamykającym

Usunąć zwykłą spację przed nawiasem lub cudzysłowem zamykającym.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[ \t]+([)\]»”])
```

Wejściowy tekst testowy:

```text
«слово » (пример )
```

Zamień na:

```text
$1
```

Wynik zamiany:

```text
«слово» (пример)
```

Przykład, który nie powinien zostać znaleziony:

```text
«слово» (пример)
```


Ograniczenia i uwagi: Nie normalizuje wszystkich systemów cudzysłowów i nawiasów matematycznych.

### D07. Dokładnie trzy kropki na wielokropek

Zamienić trzy kolejne kropki, pozostawiając dłuższe ciągi.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?<!\.)\.{3}(?!\.)
```

Wejściowy tekst testowy:

```text
Он подумал... и ответил.
```

Zamień na:

```text
…
```

Wynik zamiany:

```text
Он подумал… и ответил.
```

Przykład, który nie powinien zostać znaleziony:

```text
Содержание.....12
```


Ograniczenia i uwagi: Sprawdź zasady redakcyjne. Cztery kropki i kropkowane prowadnice spisu treści są celowo wyłączone.

### D08. Wielokropek rozdzielony spacjami

Połączyć trzy kropki rozdzielone zwykłymi spacjami.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?<!\.)\.[ \t]*\.[ \t]*\.(?!\.)
```

Wejściowy tekst testowy:

```text
Он подумал. . . и ответил.
```

Zamień na:

```text
…
```

Wynik zamiany:

```text
Он подумал… и ответил.
```

Przykład, który nie powinien zostać znaleziony:

```text
Слово. Другое.
```


Ograniczenia i uwagi: Pasuje także do trzech kropek bez spacji. Nie stosuj do prowadnic ani pominięć w cytatach opisanych odrębnymi zasadami.

### D09. Spacja nierozdzielająca po №

Powiązać znak numeru z następującą liczbą.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
№[ \t]+([0-9]+)
```

Wejściowy tekst testowy:

```text
№ 125
```

Zamień na:

```text
№ $1
```

Wynik zamiany:

```text
№ 125
```

Przykład, który nie powinien zostać znaleziony:

```text
№125
```


Ograniczenia i uwagi: Między № a $1 w zamianie znajduje się rzeczywisty U+00A0. Nie zastępuj go zapisem \x{00A0}. Wzorzec nie dodaje spacji, jeśli jej całkowicie brakuje.

### D10. Spacja nierozdzielająca po §

Powiązać znak paragrafu z jego numerem.

Działanie: zamiana po sprawdzeniu.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
§[ \t]+([0-9]+)
```

Wejściowy tekst testowy:

```text
§ 12
```

Zamień na:

```text
§ $1
```

Wynik zamiany:

```text
§ 12
```

Przykład, który nie powinien zostać znaleziony:

```text
§12
```


Ograniczenia i uwagi: Między § a $1 w zamianie jest prawdziwy NBSP. Przy złożonej numeracji sprawdź, czy znaleziono właściwy fragment.

### D11. Możliwe powtórzenie sąsiedniego wyrazu

Znajdować powtórzenie przez poziome odstępy, bez przeskakiwania między akapitami.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — wyłączone.

Znajdź:

```regex
\b(\p{L}+)[ \t\x{00A0}]+\1\b
```

Wejściowy tekst testowy:

```text
Это это уже было.
```

Oczekiwane dopasowanie:

```text
Это это
```

Przykład, który nie powinien zostać znaleziony:

```text
Это уже было.
```


Ograniczenia i uwagi: Powtórzenia typu «Да да» mogą być autorskie. Nie usuwaj drugiego wyrazu automatycznie; ustal, czy potrzebne jest usunięcie, przecinek czy brak zmiany. Apostrofy i łączniki nie są w pełni obsługiwane.

### D12. Łacina i cyrylica w jednym wyrazie

Znaleźć mieszankę OCR w jednym ciągłym wyrazie, nie każdą dwujęzyczną linię.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?<![\p{L}\p{M}])(?=[\p{L}\p{M}]*\p{Latin})(?=[\p{L}\p{M}]*\p{Cyrillic})[\p{L}\p{M}]+(?![\p{L}\p{M}])
```

Wejściowy tekst testowy:

```text
В слове Тeст латинская e.
```

Oczekiwane dopasowanie:

```text
Тeст
```

Przykład, który nie powinien zostać znaleziony:

```text
Он прочитал Latin.
```


Ograniczenia i uwagi: W Тeст litera e jest łacińska. Całkowicie łaciński wyraz obok rosyjskiego nie jest mieszany. Wzory, marki i zabawa typografią mogą dać poprawne wyniki. Reguła niczego nie transliteruje.

### D13. Mała litera bezpośrednio przed wielką

Znaleźć możliwe sklejenie wyrazów lub błąd wielkości liter.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
\p{Ll}\p{Lu}
```

Wejściowy tekst testowy:

```text
ОнвышелИздома.
```

Oczekiwane dopasowanie:

```text
лИ
```

Przykład, który nie powinien zostać znaleziony:

```text
Он вышел из дома.
```


Ograniczenia i uwagi: Koniecznie uwzględniaj wielkość liter. Marki i nazwiska typu McDonald mogą być poprawne. Znajdowany jest punkt przejścia, nie automatycznie odtworzona granica wyrazów.

### D14. Cyfra między literami

Znaleźć typową pomyłkę OCR, w której cyfrą zastąpiono literę.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
\p{L}+[0-9]+\p{L}+
```

Wejściowy tekst testowy:

```text
Это сл0во.
```

Oczekiwane dopasowanie:

```text
сл0во
```

Przykład, który nie powinien zostać znaleziony:

```text
В главе 10 текст.
```


Ograniczenia i uwagi: H2O i inne wzory mogą być prawidłowe. Nie zamieniaj wszystkich 0 na о ani wszystkich 3 na з.

### D15. Interpunkcja wewnątrz ciągu liter

Sprawdzić znak bezpośrednio otoczony literami.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
\p{L}+[.,;:!?]\p{L}+
```

Wejściowy tekst testowy:

```text
Он,сказал слово.
```

Oczekiwane dopasowanie:

```text
Он,сказал
```

Przykład, który nie powinien zostać znaleziony:

```text
Он, сказав слово, ушёл.
```


Ograniczenia i uwagi: Skróty, adresy i domeny również mogą pasować. Do kontroli przecinków ogranicz klasę do [,]. Nie dodawaj spacji we wszystkich wynikach jedną komendą.

### D16. Akapit zaczynający się małą literą

Znaleźć możliwy zbędny podział akapitu.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
^[ \t]*\p{Ll}
```

Wejściowy tekst testowy:

```text
продолжение предложения.
```

Oczekiwane dopasowanie:

```text
п
```

Przykład, który nie powinien zostać znaleziony:

```text
Начало предложения.
```


Ograniczenia i uwagi: Wiersze, podpisy, listy i cytaty mogą prawidłowo zaczynać się małą literą. Wzorzec nie skleja akapitów i nie analizuje sąsiedztwa.

### D17. Brak końcowego znaku przed zamykającymi cudzysłowami

Znaleźć koniec literowy lub liczbowy, po którym są tylko zamknięcia i spacje.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[\p{L}\p{N}][»”")\]}]*[ \t\x{00A0}]*$
```

Wejściowy tekst testowy:

```text
«Он пришёл»
```

Oczekiwane dopasowanie:

```text
л»
```

Przykład, który nie powinien zostać znaleziony:

```text
«Он пришёл!»
```


Ograniczenia i uwagi: Tytuły i podpisy często nie mają kropki. Odsyłacz do przypisu za poprawną interpunkcją może dać fałszywy wynik. W odróżnieniu od kontroli ostatniego », przepis nie oznacza «Он пришёл!» jako podejrzanego.

### D18. Mała litera po końcu zdania

Znaleźć możliwy błąd wielkości po kropce, pytajniku lub wykrzykniku.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[.!?…][ \t]+[«„“"(\[]?\p{Ll}
```

Wejściowy tekst testowy:

```text
Он пришёл. потом ушёл.
```

Oczekiwane dopasowanie:

```text
. п
```

Przykład, który nie powinien zostać znaleziony:

```text
Он пришёл. Потом ушёл.
```


Ograniczenia i uwagi: Kropki skrótów i autorskie wielokropki często nie kończą zdania. To wyłącznie lista kandydatów do przejrzenia.

### D19. Możliwa brakująca kropka

Znaleźć przejście przez spację od małej końcówki do wyrazu z wielkiej litery.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
\p{Ll}[»”]?[ \t]+[«„“]?\p{Lu}\p{Ll}+
```

Wejściowy tekst testowy:

```text
Он пришёл Потом ушёл.
```

Oczekiwane dopasowanie:

```text
л Потом
```

Przykład, który nie powinien zostać znaleziony:

```text
Он пришёл потом ушёл.
```


Ograniczenia i uwagi: Imiona i nazwy w środku zdania dają wiele poprawnych dopasowań. Przepis nie rozstrzyga, czy potrzebna jest kropka, przecinek czy nic.

### D20. Pary prostych cudzysłowów

Znaleźć prosty fragment w podwójnych prostych cudzysłowach w jednym wierszu.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
"([^"\r\n]+)"
```

Wejściowy tekst testowy:

```text
Он сказал "да".
```

Oczekiwane dopasowanie:

```text
"да"
```

Przykład, który nie powinien zostać znaleziony:

```text
Он сказал «да».
```


Ograniczenia i uwagi: Sprawdź zagnieżdżenie, oznaczenia cali, kod i przyjęty styl. Zamiana na «$1» jest dopuszczalna tylko w wybranym kontekście, nie jako uniwersalna typografia.

### D21. Łącznik lub pauza między liczbami

Znaleźć możliwy zakres wymagający decyzji redakcyjnej.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?<![0-9])[0-9]+[ \t]*[-—][ \t]*[0-9]+(?![0-9])
```

Wejściowy tekst testowy:

```text
Страницы 12 - 15.
```

Oczekiwane dopasowanie:

```text
12 - 15
```

Przykład, który nie powinien zostać znaleziony:

```text
Страницы 12–15.
```


Ograniczenia i uwagi: Data 2026-10-02, liczba ujemna lub odejmowanie też mogą pasować. Nie zmieniaj automatycznie w zakres. – to półpauza, — pauza, - łącznik.

### D22. Dwa inicjały przed nazwiskiem

Znaleźć prosty zapis dwóch inicjałów do kontroli odstępów.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
\b(\p{Lu})\.[ \t]*(\p{Lu})\.[ \t]+(\p{Lu}\p{Ll}+)\b
```

Wejściowy tekst testowy:

```text
И.О. Иванов
```

Oczekiwane dopasowanie:

```text
И.О. Иванов
```

Przykład, który nie powinien zostać znaleziony:

```text
Иванов Иван
```


Ograniczenia i uwagi: Nie obejmuje wszystkich nazwisk złożonych i diakrytyki. Po weryfikacji można użyć $1., NBSP, $2., NBSP, $3; w polu wstawiaj prawdziwe spacje nierozdzielające, nie ich nazwy.

### D23. Nazwisko przed dwoma inicjałami

Znaleźć odwrotną kolejność zapisu nazwiska.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
\b(\p{Lu}\p{Ll}+)[ \t]+(\p{Lu})\.[ \t]*(\p{Lu})\.
```

Wejściowy tekst testowy:

```text
Иванов И.О.
```

Oczekiwane dopasowanie:

```text
Иванов И.О.
```

Przykład, który nie powinien zostać znaleziony:

```text
Иванов Иван
```


Ograniczenia i uwagi: To kontrola formatu, nie identyfikacja osoby. Nazwiska złożone, partykuły i trzy inicjały wymagają osobnego wzorca.

### D24. Niewidoczne znaki do weryfikacji

Znaleźć miękki łącznik, spację zerowej szerokości lub FEFF.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[\x{00AD}\x{200B}\x{FEFF}]
```

Wejściowy tekst testowy:

```text
сло​во
```

Ten sam tekst testowy z widocznymi oznaczeniami:

```text
сло[ZWSP]во
```

Symbol ␠ oznacza zwykłą spację, [TAB] tabulator, [NBSP] U+00A0, [NNBSP] U+202F, a [ZWSP] U+200B. To zapis objaśniający; oznaczeń nie należy wstawiać do książki.

Oczekiwane dopasowanie:

```text
​
```

Przykład, który nie powinien zostać znaleziony:

```text
слово
```


Ograniczenia i uwagi: W próbce dopasowanie jest niewidoczne: między о i в znajduje się U+200B. Zmieniać lub usuwać dopiero po ustaleniu funkcji. Fizyczny BOM pliku i FEFF w tekście to różne przypadki.

### D25. Nietypowe spacje Unicode

Znaleźć wąskie, szerokie i inne specjalne spacje.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]
```

Wejściowy tekst testowy:

```text
10 000
```

Ten sam tekst testowy z widocznymi oznaczeniami:

```text
10[NNBSP]000
```

Symbol ␠ oznacza zwykłą spację, [TAB] tabulator, [NBSP] U+00A0, [NNBSP] U+202F, a [ZWSP] U+200B. To zapis objaśniający; oznaczeń nie należy wstawiać do książki.

Oczekiwane dopasowanie:

```text
 
```

Przykład, który nie powinien zostać znaleziony:

```text
10 000
```


Ograniczenia i uwagi: Wąska spacja nierozdzielająca między grupami cyfr może być poprawna. Przepis wykrywa niejednolitość, nie uznaje wszystkich takich znaków za błędy.

### D26. Powtórzone pytajniki i wykrzykniki

Znaleźć ekspresyjną albo przypadkowo powieloną interpunkcję.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
[!?]{2,}
```

Wejściowy tekst testowy:

```text
Что?! Правда!!!
```

Oczekiwane dopasowania, kolejno:

```text
?!
!!!
```

Przykład, który nie powinien zostać znaleziony:

```text
Что? Правда!
```


Ograniczenia i uwagi: ?! i autorskie powtórzenia mogą być zamierzone. Domyślna czynność to przegląd, nie skrócenie każdej sekwencji do jednego znaku.

### D27. Cyrylickie Х przy cyfrach rzymskich

Znaleźć rosyjskie Х w zapisie złożonym z łacińskich symboli rzymskich.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — niewymagane dla tego wyrażenia. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
(?<=[IVXLCDM])Х|Х(?=[IVXLCDM])
```

Wejściowy tekst testowy:

```text
Глава IХ
```

Oczekiwane dopasowanie:

```text
Х
```

Przykład, który nie powinien zostać znaleziony:

```text
Глава IX
```


Ograniczenia i uwagi: Х jest cyrylickie, X łacińskie. Sprawdź wielkość i kontekst; całej liczby rzymskiej wzorzec nie waliduje.

### D28. Kilka wielkich liter przed małą

Znaleźć możliwy błąd OCR na początku wyrazu.

Działanie: tylko wyszukiwanie i weryfikacja.

„Wyrażenie regularne” — włączone; „Tylko całe wyrazy” — wyłączone. „Unicode (UCP)” — włączone. „Uwzględniaj wielkość liter” — włączone.

Znajdź:

```regex
\p{Lu}{2,}\p{Ll}+
```

Wejściowy tekst testowy:

```text
Он сказал ПРИвет.
```

Oczekiwane dopasowanie:

```text
ПРИвет
```

Przykład, który nie powinien zostać znaleziony:

```text
Он сказал Привет.
```


Ograniczenia i uwagi: Nazwy, skrótowce z przyrostkami i skróty łacińskie mogą być poprawne. Nie zmieniaj wielkości masowo bez sprawdzenia.

## 18. Typowe błędy i diagnostyka

### 18.1. Tekst jest widoczny, ale nie ma dopasowania

Sprawdź Projekt, opcję regex, wielkość liter, zakres, kierunek i rzeczywiste znaki. Łacińskie `a` i rosyjskie `а` wyglądają podobnie, ale są różne. NBSP nie równa się zwykłej spacji, a cudzysłowy typograficzne — prostym.

Dla rosyjskich granic wyrazu sprawdź UCP, dla małych i wielkich liter odpowiednią opcję. Nie łącz bez potrzeby Tylko całe wyrazy z rozbudowanymi własnymi granicami.

### 18.2. Znajdowany fragment jest zbyt duży

Najpierw podejrzewaj zachłanne `.*` lub zbyt szeroką klasę negatywną. Zastąp „dowolny tekst” jawnymi dozwolonymi znakami i ogranicz długość oraz granice. Sprawdź dotall.

### 18.3. Spacje obejmują akapity

`\s+` nie jest synonimem spacji. Dla odstępów między wyrazami zacznij od `[ \t]+`, a NBSP dodaj tylko świadomie.

### 18.4. W zamianie pojawiają się cyfry albo znikają ukośniki

Porównaj gramatykę FBE z rozdziałem 16. `${name}`, `$10`, `\n` i `\x{...}` nie podlegają automatycznie zasadom wyszukiwania lub innego edytora. Sprawdź, czy grupa istnieje i nie jest opcjonalna w innym odgałęzieniu.

### 18.5. Mieszane alfabety wykrywane są w poprawnym zdaniu dwujęzycznym

Wcześniejszy przykład sprawdzał łacinę i cyrylicę gdziekolwiek w całym wierszu, znajdując także `Он прочитал Latin.`. D12 ogranicza oba warunki do jednego wyrazu. To podstawowa różnica między błędem OCR a dwujęzycznym wierszem.

### 18.6. „Brak kropki” wskazuje poprawny cytat

Ostatni znak akapitu nie wystarcza: po `!` może wystąpić `»`. D17 uwzględnia końcowe nawiasy i cudzysłowy, ale tytuły i odsyłacze nadal wymagają sprawdzenia.

### 18.7. Nie udaje się znaleźć struktury

Regex tekstowy nie widzi DOM jak skrypt strukturalny. Tagi, zagnieżdżenie, odsyłacze do brakujących ID i przenoszenie przypisów to osobne zadania. Potrzebny może być Kod, walidator FB2 lub skrypt, nie coraz bardziej skomplikowana regex.

## 19. Wydajność i duże książki

Zacznij od konkretnego znaku, klasy, wyrazu lub początku akapitu. Unikaj wielokrotnie zagnieżdżonych nieograniczonych powtórzeń i kilku konkurujących fragmentów „dowolny tekst”. Nieudane wyszukiwanie może wtedy sprawdzać ogromną liczbę wariantów.

Leniwość nie jest uniwersalnym lekarstwem na wolny wzorzec — także próbuje wiele wariantów. Zwykle lepiej wykluczyć separator, ograniczyć długość i zmniejszyć zakres.

Gdy wyszukiwanie się przeciąga, nie uruchamiaj kolejnych zamian. Uprość wzorzec i sprawdź krótką próbkę. Błąd limitu zasobów PCRE2 nie oznacza braku dopasowań.

Dla operacji na sąsiednich akapitach i tagach skrypt bywa czytelniejszy i bezpieczniejszy. Nie łącz wszystkich reguł korekty w gigantyczną alternatywę: oddzielne reguły ujawniają przyczynę każdego wyniku.

## 20. Kontrola przed zapisaniem

Przejrzyj początek, środek i koniec opracowanego fragmentu. Sprawdź kilka wyników każdego typu, szczególnie cudzysłowy, zakresy, inicjały, przypisy i zachowywane formatowanie. Upewnij się, że nie zniknęły znaczące NBSP i że akapity nie zmieniły się niespodziewanie.

Po zmianach wrażliwych strukturalnie uruchom kontrolę dokumentu FBE. Zapisz, w razie potrzeby otwórz ponownie i porównaj. Poprawne wyszukiwanie regex nie zastępuje walidacji FB2 ani korekty redakcyjnej.

## 21. Źródła i zakres stosowania

To opracowanie dostarczonego regex-design.md z nowymi objaśnieniami i przykładami. Nieścisłości dotyczące UCP, granic obiektu, mieszanych alfabetów i zamiany doprecyzowano według źródeł pierwotnych. Przepisy z rozdziału 17 są autorskimi scenariuszami, nie cytatami z dokumentacji PCRE2.

[D1] Oficjalny opis składni PCRE2: kotwice, grupy, właściwości Unicode, nawracanie i opcje.

`https://www.pcre.org/current/doc/html/pcre2pattern.html`

[D2] Oficjalna dokumentacja Unicode PCRE2.

`https://www.pcre.org/current/doc/html/pcre2unicode.html`

[D3] FBE Next: kompatybilność PCRE2 i adapter. Wersja repozytorium d2257405d95b0328649acee64b38829b40a4314b.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/docs/pcre2-compatibility.md`

[D4] FBE Next: GetReplStr, PrepareRegexReplacementText i operacje wyszukiwania/zamiany w FBEview.cpp; SearchPresetCatalog.cpp oraz search-preset-design-fixtures.cpp tej samej wersji.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/src/fbe/FBEview.cpp`

Składnia silnika, możliwości interfejsu i poprawność decyzji redakcyjnej to trzy różne poziomy. Test próbek nie gwarantuje działania dowolnej kompilacji FBE z dowolną książką. Stan weryfikacji podano w README archiwum.
