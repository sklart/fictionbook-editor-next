# Hilfe zu regulären Ausdrücken — Design

Hinweis zur Übersetzung: Russische Wörter und Sätze in den Kontrollbeispielen bleiben absichtlich unverändert. Sie sind Testdaten: Reguläre Ausdrücke, Ersetzungstexte, Leerraumzeichen und erwartete Ergebnisse entsprechen der russischen Referenzfassung. Die Erläuterungen sind übersetzt; die Beispiele werden nicht automatisch an deutsche typografische Regeln angepasst.

Vollständiges Handbuch zum Suchen, Ersetzen und Korrekturlesen von Büchern in FictionBook Editor Next.

Stand: 2. Oktober 2026. Der Quelltextmodus wird in regex-source.md beschrieben. „Muster“ bezeichnet hier einen regulären Ausdruck; eine „integrierte Vorlage“ ist ein gespeicherter Ausdruck samt Einstellungen im Vorlagenbereich.

## 1. So verwenden Sie dieses Handbuch

Ein regulärer Ausdruck beschreibt eine Suchregel statt einer einzigen genauen Zeichenfolge. Beispielsweise findet `[0-9]+` eine beliebig lange Ziffernfolge, während `[ \t]{2,}` mindestens zwei gewöhnliche Leerzeichen oder Tabulatoren findet. Der Treffer und der Text, durch den er ersetzt werden soll, sind unterschiedliche Dinge.

Abschnitt 2 führt zum ersten praktischen Ergebnis. Die Abschnitte 3–15 erklären die Syntax, Abschnitt 16 die besondere Ersetzungsgrammatik von FBE. Abschnitt 17 enthält fertige Rezepte für die Buchbearbeitung mit Einstellungen, Kontrolltexten und Warnungen. Abschließend folgen Fehlerdiagnose, Leistungshinweise und Quellen.

In das Suchfeld gehört nur der Ausdruck. Die Wörter „Suchen“ und „Ersetzen“, Bezeichnungen wie U+0020 und Markdown-Backticks sind kein Teil des Musters. JavaScript-Begrenzer `/.../g`, Anführungszeichen eines C++-Stringliterals und aus JSON übernommene doppelte Backslashes sind nicht erforderlich.

Die Kontrollkästchen sind wichtig. Ein Treffer ohne Beachtung der Groß-/Kleinschreibung muss bei eingeschalteter Beachtung nicht wieder auftreten. Die angegebenen Einstellungen gehören zum Rezept und sind keine unverbindliche Gestaltung.

„Nur suchen und prüfen“ kennzeichnet ein diagnostisches Rezept. Die Fundstelle kann korrekt sein: eine absichtliche Wiederholung, ein fremdsprachiger Name, ein Zitat, eine Überschrift oder gewollte Typografie. Auch „Nach Prüfung ersetzen“ bedeutet nicht, dass die Änderung für jedes Buch sicher ist.

## 2. Erste Suche und sichere Ersetzung

Speichern Sie eine Arbeitskopie des Buches. Wechseln Sie zu Design, öffnen Sie Suchen oder Ersetzen und aktivieren Sie reguläre Ausdrücke. Schalten Sie für erste Versuche „Nur ganze Wörter“ aus: Grenzen sollten zunächst im Ausdruck selbst angegeben werden. Prüfen Sie Suchbereich und Richtung.

Für mehrere Leerzeichen geben Sie ein:

```regex
[ \t]{2,}
```

In `Он   пришёл` wird die Lücke aus drei Leerzeichen gefunden. Geben Sie als Ersetzung ein gewöhnliches Leerzeichen ein. Ersetzen Sie zunächst einen Treffer, kontrollieren Sie `Он пришёл` und erwägen Sie erst danach „Alle ersetzen“.

Bei vielen Treffern sollten Sie zuerst die Ergebnisliste ansehen und ein oder zwei typische Fälle korrigieren. Prüfen Sie nach einer Sammeloperation Text, Kursiv- und Fettschrift, Anmerkungen und Absatzgrenzen. Machen Sie ein unerwartetes Ergebnis rückgängig, bevor Sie weitere Änderungen beginnen.

„Anwenden“ im Vorlagenbereich übernimmt Ausdruck und Optionen in den Suchdialog. Es ist keine Aufforderung, alle Fundstellen bedingungslos zu korrigieren. Suche und Ersetzung werden mit den entsprechenden Dialogbefehlen ausgeführt.

## 3. Such-Engine und Grenzen des Design-Modus

Hier wird PCRE2-16 verwendet; Text und Muster werden als UTF-16 übergeben, UTF ist stets aktiviert. FBE erstellt die durchsuchbare Textdarstellung und führt Ersetzungen unter Berücksichtigung der Buchstruktur separat aus. Die PCRE2-Dokumentation erklärt daher Treffer, definiert aber nicht sämtliche FBE-Aktionen. Technische Grundlagen: D1–D4.

Durchsucht wird eine Textdarstellung des Buches, nicht das wörtliche XML-Markup der Datei. `<strong>` ist kein Suchmuster für Fettschrift in Design. XML-Tags werden im Quelltextmodus gesucht; strukturelle Änderungen erfolgen mit Editorfunktionen oder spezialisierten Skripten.

Ein Zeilenumbruch aufgrund der Fensterbreite ist kein Zeilenwechselzeichen. Absätze, echte Umbrüche und visueller Umbruch dürfen nicht verwechselt werden. FBE verwendet mehrzeilige Anker in der Suchdarstellung, damit deren Zeilengrenzen Absatzgrenzen abbilden. Absatzübergreifende Treffer und absatzübergreifende Ersetzungen sind nicht dasselbe: Letztere werden in der betrachteten Implementierung abgewiesen.

`\A` und `\z` beziehen sich auf Anfang und Ende des an die Engine übergebenen Suchobjekts. Sie sind nicht automatisch Anfang und Ende der gesamten FB2-Datei: Suchbereich und das von FBE gebildete Fragment sind entscheidend.

## 4. Unicode: UTF, UCP und Groß-/Kleinschreibung

### 4.1. Wirkung von UTF

UTF ermöglicht der Engine die Verarbeitung von Unicode-Zeichen einschließlich Kyrillisch. Es transliteriert nicht, korrigiert keine Texterkennung, wandelt `ё` nicht in `е` um und vereinheitlicht kanonisch gleichwertige Buchstabendarstellungen nicht automatisch.

Ein akzentuierter Buchstabe kann als einzelnes Zeichen oder als Buchstabe mit anschließendem kombinierendem Zeichen gespeichert sein. Beide sehen ähnlich aus, können bei zeichenweiser Suche aber unterschiedlich reagieren. PCRE2 führt keine NFC/NFD-Normalisierung durch. Bei Diakritika kann die Klasse `\p{M}` erforderlich sein.

### 4.2. Wirkung von UCP

„Unicode (UCP)“ verändert die Kurzklassen, vor allem `\w`, `\d` und `\s`, sowie die davon abhängigen Grenzen `\b` und `\B`. Es ist nicht der Schalter, der kyrillischen Text erst ermöglicht.

Wichtige Präzisierung früherer Fassungen: Explizite Unicode-Eigenschaften wie `\p{L}`, `\p{N}` und `\P{...}` sind in einem Unicode-PCRE2-Build auch ohne UCP verfügbar. Enthält ein Ausdruck zugleich `\p{L}` und `\b`, sollte UCP normalerweise eingeschaltet sein, damit Buchstaben und Wortgrenzen zusammenpassen.

Vergleichen Sie die Suche nach einem russischen Wort:

```regex
\bмир\b
```

Mit UCP richten sich Grenzen nach der Unicode-Wortklasse. Ohne UCP darf auf Kyrillisch nicht das gleiche Verhalten von `\b` wie bei lateinischem ASCII-Text erwartet werden. Ein fehlender Treffer beweist hier nicht, dass das Wort fehlt.

Wenn ausdrücklich ASCII-Ziffern benötigt werden, verwenden Sie `[0-9]` statt `\d`: Mit UCP kann Letzteres auch Dezimalziffern anderer Schriftsysteme erfassen.

### 4.3. Nützliche Eigenschaften

| Ausdruck | Gesuchtes Zeichen |
| --- | --- |
| `\p{L}` | Unicode-Buchstabe |
| `\p{Lu}` | Großbuchstabe bei beachteter Groß-/Kleinschreibung |
| `\p{Ll}` | Kleinbuchstabe bei beachteter Groß-/Kleinschreibung |
| `\p{M}` | Kombinierendes Zeichen |
| `\p{N}` | Numerisches Zeichen; weiter gefasst als Dezimalziffer |
| `\p{Nd}` | Dezimalziffer |
| `\p{Latin}` | Zeichen der lateinischen Schrift |
| `\p{Cyrillic}` | Zeichen der kyrillischen Schrift |
| `\P{L}` | Zeichen, das kein Buchstabe ist |

Für eine Buchstabenfolge einschließlich kombinierender Akzente:

```regex
[\p{L}\p{M}]+
```

Das ist noch keine universelle sprachwissenschaftliche Wortdefinition: Bindestriche und Apostrophe fehlen. Fügen Sie sie nur bewusst hinzu.

### 4.4. Groß-/Kleinschreibung ist eine eigene Einstellung

Beachten Sie die Groß-/Kleinschreibung bei Auffälligkeiten wie `строчнаяПрописная`, Initialen und Mustern mit `\p{Lu}` oder `\p{Ll}`. Eine Suche ohne diese Unterscheidung kann den Sinn der Regel aufheben. UCP ersetzt dieses Kontrollkästchen nicht.

Groß-/Kleinschreibung innerhalb eines Ausdrucks vorübergehend ignorieren:

```regex
(?i)глава
```

Auf eine Gruppe begrenzt:

```regex
(?i:глава)[ \t]+[0-9]+
```

## 5. Literale Zeichen und Maskierung

Buchstaben und die meisten Zeichen stehen für sich selbst. Außerhalb einer Zeichenklasse haben unter anderem Punkt, Klammern, Stern, Plus, Fragezeichen, geschweifte Klammern, Anker und Backslash besondere Bedeutung.

Für die wörtliche Suche nach einem Metazeichen stellen Sie einen Backslash davor:

| Gesuchter Text | Ausdruck |
| --- | --- |
| Punkt | `\.` |
| Fragezeichen | `\?` |
| Pluszeichen | `\+` |
| Stern | `\*` |
| Öffnende runde Klammer | `\(` |
| Schließende runde Klammer | `\)` |
| Zahl in eckigen Klammern | `\[[0-9]+\]` |
| Backslash selbst | `\\` |

`(123)` findet die Ziffern `123` und speichert sie in einer Gruppe; Klammern im Text werden nicht verlangt. Für die Zeichenfolge `(123)` müssen die runden Klammern maskiert werden.

Ein längeres Literal kann in einem PCRE2-Muster zwischen `\Q` und `\E` stehen:

```regex
\QЦена (руб.) + доставка\E
```

Im Ersetzungsfeld von FBE haben dieselben Folgen eine andere Bedeutung. Übertragen Sie Maskierungsregeln der Suche nicht automatisch auf die Ersetzung.

## 6. Leerzeichen, Tabulatoren und unsichtbare Zeichen

| Ausdruck | Bedeutung bei PCRE2-Suche |
| --- | --- |
| `[ ]` | Nur gewöhnliches Leerzeichen U+0020 |
| `[ \t]` | Gewöhnliches Leerzeichen oder Tabulator |
| `\t` | Tabulator U+0009 |
| `\x{00A0}` | Geschütztes Leerzeichen |
| `\x{202F}` | Schmales geschütztes Leerzeichen |
| `\h` | Horizontales Leerraumzeichen einschließlich verschiedener Unicode-Leerzeichen |
| `\s` | Leerraumzeichen, möglicherweise einschließlich Zeilenwechseln |
| `\r` | Wagenrücklauf CR |
| `\n` | Zeilenvorschub LF |
| `\R` | Unicode-Zeilenwechselsequenz |
| `\x{00AD}` | Bedingter Trennstrich |
| `\x{200B}` | Leerzeichen ohne Breite |
| `\x{FEFF}` | FEFF innerhalb des Textes |

Für gewöhnliche Wortzwischenräume wählen Sie `[ \t]` statt `\s`. Die zweite Klasse ist weiter und kann Absatzgrenzen sowie geschützte Leerzeichen erfassen. Auch `\h` eignet sich zur Diagnose, ist aber für eine nicht näher bestimmte typografische Ersetzung zu breit.

Ein geschütztes Leerzeichen hält Teile zusammen: Nummernzeichen und Zahl, Initialen und Nachname oder Zahl und Einheit. Alle geschützten Leerzeichen durch normale zu ersetzen kann den Satz verschlechtern. FBE besitzt außerdem eine NBSP-Zeicheneinstellung; Rezepte mit U+00A0 sind mit Buch und Build abzugleichen.

U+200C und U+200D können für Schriften und zusammengesetzte Emoji erforderlich sein. Löschen Sie nicht pauschal sämtliche „unsichtbaren“ Zeichen. Eine Fundstelle ist nicht automatisch ein Fehler.

## 7. Zeichenklassen und Bereiche

Eine Klasse in eckigen Klammern verbraucht ein Zeichen aus ihrer Menge. `[abc]` bedeutet einen von drei Buchstaben, nicht das Wort `abc`. Für einen oder mehrere ergänzen Sie einen Quantifizierer: `[abc]+`.

`[^abc]` verbraucht ein Zeichen außerhalb der Menge. Es prüft nicht, dass „vor dem Text kein abc steht“. Für solche Kontextprüfungen dienen Lookaround-Konstruktionen.

In einer Klasse verlieren Punkt und die meisten Klammern ihre Sonderbedeutung. Ein Bindestrich kann einen Bereich bilden; als Literal gehört er an Anfang oder Ende oder wird maskiert. Eine schließende eckige Klammer lässt sich als `\]` angeben.

```regex
[А-Яа-яЁё-]+
```

Das Beispiel erlaubt russische Buchstaben und einen Bindestrich, aber nicht die gesamte kyrillische Schrift. Ukrainische, belarussische und andere Buchstaben erfordern eine weitere Menge oder Unicode-Eigenschaft.

Innerhalb eckiger Klammern ist `\b` keine Wortgrenze. Wortgrenzen gehören nicht in gewöhnliche Zeichenmengen.

## 8. Anker, Grenzen und leere Treffer

Ein Anker prüft eine Position, ohne Buchstaben oder Leerzeichen zu verbrauchen. Daher können `^`, `$` oder `\b` allein einen Treffer der Länge null ergeben, der nicht wie eine gewöhnliche Textmarkierung aussieht.

| Anker | Bedeutung |
| --- | --- |
| `^` | Zeilenanfang bei multiline, sonst Anfang des Suchobjekts |
| `$` | Zeilenende bei multiline; abschließende Zeilenwechsel sind modusabhängig |
| `\A` | Ausschließlich Anfang des Suchobjekts |
| `\z` | Striktes Ende des Suchobjekts |
| `\Z` | Ende des Suchobjekts oder Position vor dem letzten Zeilenwechsel |
| `\b` | Grenze zwischen Wortzeichen und Nichtwortzeichen |
| `\B` | Position, die keine Wortgrenze ist |
| `\G` | Startposition des aktuellen Abgleichaufrufs |

`\G` merkt sich nicht selbstständig den vorherigen Treffer. Es ist an den vom Programm übergebenen Startoffset gebunden. Bei aufeinanderfolgenden Aufrufen kann das das Ende des vorherigen Treffers sein; in FBE ist dies aber keine universelle Methode zum Durchlaufen eines Buches. Verwenden Sie bei gewöhnlicher Korrektur klarere Grenzen. [D1]

Für ein vollständiges Wort statt eines Teilworts verwenden Sie Grenzen oder Prüfungen benachbarter Buchstaben. Mit UCP:

```regex
\bтом\b
```

Das findet nicht den Anfang von `томик`. Bindestriche und Apostrophe können Wörter aus Sicht der Engine jedoch anders trennen als aus redaktioneller Sicht.

Seien Sie bei leeren Treffern und „Alle ersetzen“ besonders vorsichtig: Text wird an Positionen eingefügt statt sichtbare Zeichen zu ersetzen. Beginnen Sie mit Mustern mit nichtleerem Ergebnis.

## 9. Quantifizierer: Wiederholung und Rückverfolgung

| Schreibweise | Wiederholungen des vorherigen Elements |
| --- | --- |
| `?` | Null oder eins |
| `*` | Null oder mehr |
| `+` | Eins oder mehr |
| `{3}` | Genau drei |
| `{3,}` | Mindestens drei |
| `{2,5}` | Zwei bis fünf |

Ein Quantifizierer bezieht sich auf das vorherige Zeichen, die Klasse oder Gruppe. `аб+` wiederholt nur `б`; `(?:аб)+` wiederholt das Buchstabenpaar.

### 9.1. Gierige Suche

Ausgangstext:

```text
«первый» и «второй»
```

Muster:

```regex
«.*»
```

Punkt und Stern nehmen zunächst möglichst viel Text auf. Falls erforderlich, geht die Engine zurück, damit der Rest des Musters passt. Hier umfasst der Treffer beide Anführungszeichenpaare.

### 9.2. Nichtgierige Suche

```regex
«.*?»
```

Zunächst werden möglichst wenige Zeichen versucht; bei Bedarf wird der Treffer erweitert. Nacheinander werden hier `«первый»` und `«второй»` gefunden.

Für ein einfaches Anführungszeichenpaar ist eine explizite Begrenzung oft verständlicher:

```regex
«[^»\r\n]*»
```

Dies analysiert keine verschachtelten Zitate. Diese benötigen eine gesonderte redaktionelle Prüfung.

### 9.3. Possessive Quantifizierer

```regex
«.*+»
```

Dies bedeutet nicht „noch korrektere Anführungszeichen“. `.*+` verbraucht auch das schließende Zeichen und gibt es nicht zurück. Das verbleibende `»` im Muster kann deshalb nicht mehr passen; im Beispiel gibt es keinen Treffer.

Eine Variante, die das schließende Zeichen ausschließt, kann sinnvoll sein:

```regex
«[^»\r\n]*+»
```

Possessive Quantifizierer und atomare Gruppen steuern Backtracking. Sie sind keine universelle Beschleunigung für jedes Muster.

## 10. Gruppen und Rückverweise

Runde Klammern speichern einen Trefferteil. Nummeriert wird ab 1 nach den öffnenden erfassenden Klammern von links nach rechts; Verschachtelung ändert diese Reihenfolge nicht.

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Bei `02.10.2026` enthalten die Gruppen `02`, `10` und `2026`. Geprüft wird die Schreibform, nicht die kalendarische Gültigkeit.

Eine nicht erfassende Gruppe `(?:...)` fasst Teile zusammen, ohne eine Nummer zu beanspruchen. Das hilft bei komplexen Rezepten, deren Ersetzungen verlässliche `$1` und `$2` benötigen.

Benannte Gruppen verbessern die Lesbarkeit:

```regex
(?<word>\p{L}+)[ \t]+\k<word>
```

Ein Rückverweis sucht denselben erfassten Text. Ein später erläuterter Unterprogrammaufruf wiederholt dagegen eine Regel und kann anderen Text finden. Das sind unterschiedliche Mechanismen.

Für Wortwiederholungen in Büchern sind zusätzlich Grenzen, Groß-/Kleinschreibung und passende Zwischenräume nötig; unten steht ein fertiges Rezept. Ein Rückverweis ohne Grenzen kann einen Teil eines längeren Wortes finden.

Benannte Gruppen in der Suche erlauben nicht automatisch benannte Ersetzungen in FBE. Verwenden Sie dort die bestätigten numerischen Verweise.

## 11. Alternativen und atomare Gruppen

Der senkrechte Strich wählt zwischen Zweigen:

```regex
(?:глава|часть)[ \t]+[0-9]+
```

Die Gruppierung ist wichtig. Ohne sie kann sich ein gemeinsamer rechter Teil nur auf den letzten Zweig beziehen. Alternativen werden in Schreibreihenfolge versucht; lange und kurze Formen sollten bewusst angeordnet werden.

Eine atomare Gruppe verhindert die Rückkehr in eine bereits erfolgreich abgeglichene Gruppe:

```regex
(?>а|аб)в
```

Bei `абв` wählt der erste Zweig zunächst `а`. Danach ist die Rückkehr zu `аб` verboten; es gibt keinen Treffer. Ohne Atomarität kann er gefunden werden. Dies ist ein Lehrbeispiel: Ergänzen Sie Atomarität nicht ungeprüft.

## 12. Prüfung benachbarten Textes: Lookaround

Lookaround prüft Kontext, der nicht zum gesamten Treffer gehört. So kann nur die Nummer markiert werden, während das vorangestellte Zeichen erhalten bleibt.

```regex
(?<=№ )[0-9]+
```

Bei `№ 125` wird nur `125` gefunden. Hier steht genau ein gewöhnliches Leerzeichen.

Prüfung rechts:

```regex
[0-9]+(?=[ \t]+руб\.)
```

Bei `125 руб.` wird die Zahl gefunden, aber nicht `руб.`.

Negative Vorausschau:

```regex
\bглава\b(?![ \t]+[0-9])
```

Mit UCP wird das Wort „глава“ gefunden, wenn danach keine übliche Nummerierung mit Leerzeichen folgt. Dies ist ein Beispiel zur Kontextauswahl, keine automatische Korrekturregel.

Lookbehind wird `(?<=...)`, negatives Lookbehind `(?<!...)` geschrieben. Lookahead ist `(?=...)`, negatives Lookahead `(?!...)`.

Lookbehind unterliegt Längenbeschränkungen. Modernes PCRE2 erlaubt bestimmte begrenzte variable Längen, aber keine beliebige unbeschränkte Wiederholung. Verwenden Sie für portable Rezepte kurzen festen Kontext oder Erfassung/`\K`; verlassen Sie sich nicht auf `.*` im Lookbehind.

## 13. Inline-Optionen

| Option | Wirkung |
| --- | --- |
| `(?i)` | Groß-/Kleinschreibung ignorieren |
| `(?-i)` | Groß-/Kleinschreibung beachten |
| `(?m)` | Mehrzeilige Anfangs- und Endanker |
| `(?-m)` | Mehrzeilige Anker ausschalten |
| `(?s)` | Punkt darf Zeilenwechsel erfassen |
| `(?-s)` | Übliches Punktverhalten wiederherstellen |
| `(?x)` | Unwesentliche Leerzeichen und Kommentare im Muster ignorieren |

`(?m)` lässt den Punkt nicht über Zeilen hinweg suchen. `(?s)` erlaubt FBE keine absatzübergreifende Ersetzung. Es sind zwei Suchoptionen und eine gesonderte Programmbeschränkung.

Im erweiterten Modus sind Leerzeichen im Ausdruck möglicherweise nicht mehr wörtlich. Für ein erforderliches Leerzeichen verwenden Sie `[ ]`; `#` kann außerhalb einer Klasse einen Kommentar beginnen. Mehrzeilig gestaltete Muster sind in Dokumentationen nützlich, doch die folgenden Rezepte passen einzeilig in das Suchfeld.

## 14. Erweiterte PCRE2-Funktionen

Für die meisten Korrekturen ist dieser Abschnitt nicht erforderlich. Er erklärt Konstruktionen, die in fremden Mustern vorkommen können. Syntaxunterstützung bedeutet nicht, dass jedes entsprechende Muster bequem oder sicher auf ein ganzes Buch angewendet werden kann.

### 14.1. Trefferanfang zurücksetzen

```regex
№[ \t]*\K[0-9]+
```

Bei `№ 125` wird das Präfix geprüft, als Treffer aber nur `125` gemeldet. Das ist nützlich, wenn die Zahl statt des Zeichens geändert werden soll. `\K` innerhalb von Lookaround benötigt gesonderte Prüfung; PCRE2 beschränkt diese Kombination.

### 14.2. Bedingung anhand einer beteiligten Gruppe

```regex
^(\()?([0-9]+)(?(1)\))$
```

Erlaubt sind `12` oder `(12)`, nicht aber das ungeschlossene `(12`. Die Bedingung prüft, ob Gruppe 1 beteiligt war. Verwenden Sie dieses Suchbeispiel nicht ungeprüft für Ersetzungen mit optionalen Gruppen; die FBE-Umhüllung ist zu berücksichtigen.

### 14.3. Gemeinsame Gruppennummern in Alternativen

```regex
(?|глава ([0-9]+)|часть ([0-9]+))
```

In beiden Zweigen landet die Nummer in Gruppe 1. Das ist Branch Reset. Für einfache Fälle ist eine nicht erfassende Gruppe mit einem gemeinsamen Capture meist verständlicher.

### 14.4. Unterprogrammaufruf

```regex
(?<pair>[0-9]{2})-(?&pair)
```

Bei `12-34` erfüllen beide Teile dieselbe Regel „zwei Ziffern“, obwohl ihre Werte verschieden sind. Ein Rückverweis `\k<pair>` würde erneut `12` verlangen.

Rekursive Unterprogramme können einige verschachtelte Strukturen beschreiben, ersetzen aber nicht den XML-Parser von FBE. Verwenden Sie für `<section>`, `<poem>`, Anmerkungen und Tabellen strukturelle Funktionen.

### 14.5. Fragmente überspringen

```regex
«[^»\r\n]*»(*SKIP)(*FAIL)|\bслово\b
```

Mit UCP wird das Wort außerhalb einfacher russischer Anführungszeichenpaare gesucht. Der erste Zweig markiert ein Zitat zum Überspringen, der zweite sucht das Wort. Verschachtelte oder ungeschlossene Zitate werden nicht analysiert; verlassen Sie sich bei Massenänderungen nicht bedingungslos darauf.

## 15. Was ein regulärer Ausdruck nicht entscheiden kann

„Großbuchstabe nach Kleinbuchstabe“, „kein Satzzeichen am Absatzende“ und „Wortwiederholung“ sind formale Hinweise, keine redaktionellen Entscheidungen. Regex weiß nicht, ob `да да` ein Tippfehler, eine absichtliche Replik, ein Gedichtbestandteil oder eine Überschrift ist.

Fügen Sie nicht automatisch Punkte hinzu, verbinden Sie nicht alle klein beginnenden Absätze, ersetzen Sie nicht sämtliche lateinischen durch kyrillische Buchstaben und nicht jeden Bindestrich durch einen Gedankenstrich. Komplexe Operationen benötigen Kontext und manchmal mehrere DOM-Schritte. Die FBE-Skripte für Bereinigung, zusammengeklebte Wörter und Anmerkungen bearbeiten solche Aufgaben.

## 16. Ersetzungsgrammatik von FBE

PCRE2 sucht, FBE interpretiert die Ersetzungszeichenfolge. Beispiele aus JavaScript, Python, .NET, PCRE2 substitute oder anderen Editoren dürfen deshalb nicht ungeprüft übernommen werden. Die folgenden Befehle sind durch FBE-Quellcode und Ausgangshandbuch belegt. [D3, D4]

### 16.1. Treffer und Gruppen

| Im Ersetzungsfeld | Bedeutung |
| --- | --- |
| `$0` oder `\0` | Gesamter Treffer |
| `$1` … `$9` | Gruppe mit der entsprechenden Nummer |
| `\1` … `\9` | Alternative Schreibweise einer numerischen Gruppe |
| `$+` oder `\+` | Letzte von der Trefferumhüllung zurückgegebene Gruppe |

„FBE ersetzt nur Gruppen 1–9“ ist zu ungenau: Gewöhnlicher Text und der gesamte Treffer können ebenfalls eingefügt werden. Beschränkt ist der direkte numerische Gruppenzugriff, nicht die Gruppenzahl von PCRE2.

Verwenden Sie `$10` nicht als Verweis auf Gruppe 10. Benannte Ersetzungen wie `${name}` gehören nicht zur bestätigten FBE-Grammatik. Halten Sie benötigte Captures unter den ersten neun und machen Sie Hilfsgruppen nicht erfassend.

Optionale und leere Gruppen verlangen einen eigenen Versuch: FBE besitzt eine SubMatches-Umhüllung. Neue Sammelersetzungen sollten nicht von Feinheiten übersprungener Gruppen oder „letzter Gruppe“ abhängen; verwenden Sie explizite notwendige Captures.

### 16.2. Gruppen umordnen

Suchen:

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Ersetzen durch:

```text
$3-$2-$1
```

Aus `02.10.2026` wird `2026-10-02`. Dies veranschaulicht die Umordnung und empfiehlt nicht, sämtliche Datumsangaben im Buch umzuformatieren.

### 16.3. Groß-/Kleinschreibung ändern

| Befehl | Wirkung |
| --- | --- |
| `\U` | Eingefügtes Fragment in Großbuchstaben umwandeln |
| `\L` | In Kleinbuchstaben umwandeln |
| `\T` | Ersten Buchstaben groß, übrige klein schreiben |
| `\Q` | Aktive Schreibungs-/Formatbefehle für das folgende Fragment zurücksetzen |

Suchen:

```regex
(иван)
```

Ersetzen durch:

```text
\T$1\Q
```

Kontrollergebnis: `Иван`. `\T` ist keine sprachlich umfassende Großschreibung jedes Wortes; aus dem gemeinsamen Fragment `иван иванов` wird nicht zwangsläufig `Иван Иванов`.

Überlagern Sie `\U` und `\L` nicht ohne Zurücksetzen. Trennen Sie Abschnitte mit `\Q` und prüfen Sie Kyrillisch und Diakritika. Die Schreibweise ändert FBE, nicht ein Unicode-Normalisierer von PCRE2.

### 16.4. Fett- und Kursivschrift

`\S` aktiviert Fettschrift für das eingefügte Fragment, `\E` Kursivschrift. `\Q` beendet die aktiven Befehle für nachfolgend eingefügten Text.

```text
\S$1\Q
```

Die Ersetzung formatiert Gruppe 1. Sie sucht keine bereits vorhandene Fettschrift und entfernt nicht sämtliche bestehende Buchformatierung. Prüfen Sie erst ein Fragment und die Rückgängig-Funktion.

### 16.5. Unterschiedliche Bedeutungen in Suche und Ersetzung

In der Suche bedeutet `\S` ein Nichtleerraumzeichen, in FBE-Ersetzungen Fettschrift. In der Suche maskiert `\Q...\E` ein Literal; in der Ersetzung setzt `\Q` Befehle zurück und `\E` aktiviert Kursivschrift.

Geben Sie `\n`, `\t`, `\x{00A0}`, `\$` oder `$$` nicht in eine Design-Ersetzung ein und erwarten das Verhalten eines anderen Editors. Sie gehören nicht zur oben beschriebenen Literalgrammatik; unbekannte Steuerfolgen können entfallen. Für NBSP verwenden Sie das echte Zeichen, zur Erhaltung eines Sonderzeichens eine erfasste Gruppe oder eine geprüfte gewöhnliche Ersetzung.

Ein leeres Ersetzungsfeld löscht den Treffer. Ein Feld mit einem Leerzeichen ersetzt ihn durch ein Leerzeichen. Erläuterungen wie `<пусто>`, `[NBSP]` und `U+00A0` werden nicht wörtlich eingegeben.

## 17. Praktische Rezepte zur Bearbeitung und Korrektur

Die Beispiele sind eigenständige Szenarien; ihre Namen müssen nicht genau dem integrierten Katalog entsprechen. Ersetzungsrezepte werden zunächst an einem Treffer erprobt. Echte Leerzeichen und Tabulatoren bleiben in den Kontrollblöcken erhalten. Gegenbeispiele zeigen mindestens eine Anwendungsgrenze, ersetzen aber keine Prüfung des gesamten Buches.

### D01. Mehrere gewöhnliche Leerzeichen zusammenfassen

Gewöhnliche Zwischenräume vereinheitlichen, ohne ein einzelnes NBSP zu verändern.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[ \t]{2,}
```

Kontrolltext:

```text
Он   пришёл.
```

Ersetzen durch: ein gewöhnliches Leerzeichen U+0020. Gemeint ist ein Zeichen, nicht das Wort „Leerzeichen“.

Ergebnis der Ersetzung:

```text
Он пришёл.
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Он пришёл.
```


Einschränkungen und Hinweise: In Gedichten, Tabellen und simulierten Einzügen können mehrere Leerzeichen beabsichtigt sein. Prüfen Sie den Suchbereich; dies ändert keine Absatzformatierung.

### D02. Leerzeichen am Absatzanfang

Manuellen Einzug vor gewöhnlichem Text entfernen.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
^[ \t]+
```

Kontrolltext:

```text
   Начало абзаца.
```

Derselbe Kontrolltext mit sichtbaren Zeichen:

```text
␠␠␠Начало␠абзаца.
```

Dabei steht ␠ für ein gewöhnliches Leerzeichen, [TAB] für einen Tabulator, [NBSP] für U+00A0, [NNBSP] für U+202F und [ZWSP] für U+200B. Dies ist eine erläuternde Darstellung; die Bezeichnungen werden nicht in das Buch eingefügt.

Ersetzen durch: das Feld vollständig leer lassen. Nicht das Wort „leer“ eingeben.

Ergebnis der Ersetzung:

```text
Начало абзаца.
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Начало абзаца.
```


Einschränkungen und Hinweise: In Gedichten und künstlerischem Satz können manuelle Einzüge sinnvoll sein. Der Anker bezeichnet eine Textzeile, nicht den visuellen Fensterumbruch.

### D03. Leerzeichen am Absatzende

Nachgestellte gewöhnliche Leerzeichen oder Tabulatoren entfernen.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[ \t]+$
```

Kontrolltext:

```text
Конец абзаца.
```

Derselbe Kontrolltext mit sichtbaren Zeichen:

```text
Конец␠абзаца.␠␠␠
```

Dabei steht ␠ für ein gewöhnliches Leerzeichen, [TAB] für einen Tabulator, [NBSP] für U+00A0, [NNBSP] für U+202F und [ZWSP] für U+200B. Dies ist eine erläuternde Darstellung; die Bezeichnungen werden nicht in das Buch eingefügt.

Ersetzen durch: das Feld vollständig leer lassen. Nicht das Wort „leer“ eingeben.

Ergebnis der Ersetzung:

```text
Конец абзаца.
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Конец абзаца.
```


Einschränkungen und Hinweise: NBSP ist bewusst ausgeschlossen. Verwenden Sie für Sonderleerzeichen eigene Diagnosen.

### D04. Leerzeichen vor Komma und anderen Satzzeichen

Das Satzzeichen erhalten und den Zwischenraum davor entfernen.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[ \t]+([,;:!?])
```

Kontrolltext:

```text
Слово , другое !
```

Ersetzen durch:

```text
$1
```

Ergebnis der Ersetzung:

```text
Слово, другое!
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Слово, другое!
```


Einschränkungen und Hinweise: Die Regel gilt für gewöhnlichen russischen Text. Französische Typografie erlaubt vor einigen Zeichen besondere Leerzeichen; dort darf das Rezept nicht unangepasst verwendet werden.

### D05. Leerzeichen nach einem öffnenden Zeichen

Gewöhnliche Leerzeichen nach Klammern oder russischen öffnenden Anführungszeichen entfernen.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
([(\[«„])[ \t]+
```

Kontrolltext:

```text
« слово» ( пример)
```

Ersetzen durch:

```text
$1
```

Ergebnis der Ersetzung:

```text
«слово» (пример)
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
«слово» (пример)
```


Einschränkungen und Hinweise: Leerzeichen in Formeln und Beispielen bleiben unberührt, sofern sie nicht direkt einem aufgeführten Zeichen folgen. Der Kontext ist dennoch zu prüfen.

### D06. Leerzeichen vor einem schließenden Zeichen

Gewöhnliche Leerzeichen vor Klammern oder schließenden Anführungszeichen entfernen.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[ \t]+([)\]»”])
```

Kontrolltext:

```text
«слово » (пример )
```

Ersetzen durch:

```text
$1
```

Ergebnis der Ersetzung:

```text
«слово» (пример)
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
«слово» (пример)
```


Einschränkungen und Hinweise: Kein allgemeiner Normalisierer für alle Anführungszeichen- und mathematischen Klammerkonventionen.

### D07. Genau drei Punkte als Auslassungszeichen

Drei aufeinanderfolgende Punkte umwandeln, längere Folgen aber nicht.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?<!\.)\.{3}(?!\.)
```

Kontrolltext:

```text
Он подумал... и ответил.
```

Ersetzen durch:

```text
…
```

Ergebnis der Ersetzung:

```text
Он подумал… и ответил.
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Содержание.....12
```


Einschränkungen und Hinweise: Beachten Sie die redaktionellen Vorgaben. Vier Punkte und Füllpunkte im Inhaltsverzeichnis werden absichtlich nicht geändert.

### D08. Auseinandergezogene Auslassungspunkte

Drei durch gewöhnliche Leerzeichen getrennte Punkte zusammenfassen.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?<!\.)\.[ \t]*\.[ \t]*\.(?!\.)
```

Kontrolltext:

```text
Он подумал. . . и ответил.
```

Ersetzen durch:

```text
…
```

Ergebnis der Ersetzung:

```text
Он подумал… и ответил.
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Слово. Другое.
```


Einschränkungen und Hinweise: Erfasst auch drei unmittelbar benachbarte Punkte. Nicht für Füllpunktzeilen oder speziell geregelte Auslassungen in Zitaten verwenden.

### D09. Geschütztes Leerzeichen nach №

Nummernzeichen und folgende Zahl zusammenhalten.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
№[ \t]+([0-9]+)
```

Kontrolltext:

```text
№ 125
```

Ersetzen durch:

```text
№ $1
```

Ergebnis der Ersetzung:

```text
№ 125
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
№125
```


Einschränkungen und Hinweise: Zwischen № und $1 steht ein echtes U+00A0. Nicht durch die sichtbare Folge \x{00A0} ersetzen. Ein fehlendes Leerzeichen wird nicht eingefügt.

### D10. Geschütztes Leerzeichen nach §

Paragrafenzeichen und Nummer verbinden.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
§[ \t]+([0-9]+)
```

Kontrolltext:

```text
§ 12
```

Ersetzen durch:

```text
§ $1
```

Ergebnis der Ersetzung:

```text
§ 12
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
§12
```


Einschränkungen und Hinweise: Zwischen § und $1 steht ein echtes NBSP. Bei komplexer Nummerierung prüfen, ob das richtige Fragment gefunden wurde.

### D11. Mögliche unmittelbare Wortwiederholung

Wiederholungen über horizontale Zwischenräume finden, ohne Absätze zu überqueren.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — aus.

Suchen:

```regex
\b(\p{L}+)[ \t\x{00A0}]+\1\b
```

Kontrolltext:

```text
Это это уже было.
```

Erwarteter Treffer:

```text
Это это
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Это уже было.
```


Einschränkungen und Hinweise: „Да да“ und ähnliche Wiederholungen können beabsichtigt sein. Nicht automatisch die zweite Instanz löschen; erst zwischen Löschen, Komma und unverändertem Text entscheiden. Apostroph- und Bindestrichwörter sind nicht vollständig abgedeckt.

### D12. Lateinisch und Kyrillisch innerhalb eines Wortes

OCR-Mischung innerhalb eines zusammenhängenden Wortes finden, nicht jede zweisprachige Zeile.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?<![\p{L}\p{M}])(?=[\p{L}\p{M}]*\p{Latin})(?=[\p{L}\p{M}]*\p{Cyrillic})[\p{L}\p{M}]+(?![\p{L}\p{M}])
```

Kontrolltext:

```text
В слове Тeст латинская e.
```

Erwarteter Treffer:

```text
Тeст
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Он прочитал Latin.
```


Einschränkungen und Hinweise: In Тeст ist e lateinisch. Ein rein lateinisches Wort neben einem russischen gilt nicht als gemischt. Formeln, Produktnamen und gewollte Schrifteffekte können gültige Treffer ergeben. Keine Transliteration.

### D13. Kleinbuchstabe direkt vor Großbuchstabe

Mögliche zusammengeklebte Wörter oder Schreibungsfehler finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
\p{Ll}\p{Lu}
```

Kontrolltext:

```text
ОнвышелИздома.
```

Erwarteter Treffer:

```text
лИ
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Он вышел из дома.
```


Einschränkungen und Hinweise: Groß-/Kleinschreibung unbedingt beachten. Marken und Namen wie McDonald können korrekt sein. Gefunden wird der Übergang, nicht automatisch eine rekonstruierte Wortgrenze.

### D14. Ziffer zwischen Buchstaben

Typische OCR-Verwechslung eines Buchstabens mit einer Ziffer finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
\p{L}+[0-9]+\p{L}+
```

Kontrolltext:

```text
Это сл0во.
```

Erwarteter Treffer:

```text
сл0во
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
В главе 10 текст.
```


Einschränkungen und Hinweise: H2O und andere Formeln können korrekt sein. Nicht jedes 0 in о oder jedes 3 in з umwandeln.

### D15. Satzzeichen innerhalb einer Buchstabenfolge

Ein Zeichen prüfen, an das beidseitig direkt Buchstaben grenzen.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
\p{L}+[.,;:!?]\p{L}+
```

Kontrolltext:

```text
Он,сказал слово.
```

Erwarteter Treffer:

```text
Он,сказал
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Он, сказав слово, ушёл.
```


Einschränkungen und Hinweise: Abkürzungen, Adressen und Domains können ebenfalls passen. Für reine Kommaprüfung die Klasse auf [,] begrenzen. Nicht in alle Treffer automatisch Leerzeichen einsetzen.

### D16. Absatz beginnt klein

Möglichen überflüssigen Absatzumbruch finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
^[ \t]*\p{Ll}
```

Kontrolltext:

```text
продолжение предложения.
```

Erwarteter Treffer:

```text
п
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Начало предложения.
```


Einschränkungen und Hinweise: Gedichte, Bildunterschriften, Listen und Zitate beginnen oft berechtigt klein. Das Muster verbindet keine Absätze und kennt den Nachbarkontext nicht.

### D17. Kein Schlusszeichen vor schließenden Anführungszeichen

Ein Buchstaben- oder Ziffernende finden, auf das nur schließende Zeichen und Leerraum folgen.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[\p{L}\p{N}][»”")\]}]*[ \t\x{00A0}]*$
```

Kontrolltext:

```text
«Он пришёл»
```

Erwarteter Treffer:

```text
л»
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
«Он пришёл!»
```


Einschränkungen und Hinweise: Überschriften und Beschriftungen brauchen oft keinen Punkt. Anmerkungsverweise nach einem korrekten Schlusszeichen können falsch anschlagen. Anders als die Prüfung nur des letzten » wird «Он пришёл!» nicht beanstandet.

### D18. Kleinbuchstabe nach Satzende

Wahrscheinlichen Schreibungsfehler nach Punkt, Frage- oder Ausrufezeichen finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[.!?…][ \t]+[«„“"(\[]?\p{Ll}
```

Kontrolltext:

```text
Он пришёл. потом ушёл.
```

Erwarteter Treffer:

```text
. п
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Он пришёл. Потом ушёл.
```


Einschränkungen und Hinweise: Abkürzungspunkte und autorielle Auslassungspunkte beenden nicht immer einen Satz. Daher nur Kandidaten zur Prüfung.

### D19. Möglicherweise fehlender Punkt

Übergang von kleinem Wortende zu groß beginnendem Wort über ein Leerzeichen finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
\p{Ll}[»”]?[ \t]+[«„“]?\p{Lu}\p{Ll}+
```

Kontrolltext:

```text
Он пришёл Потом ушёл.
```

Erwarteter Treffer:

```text
л Потом
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Он пришёл потом ушёл.
```


Einschränkungen und Hinweise: Namen und Titel innerhalb eines Satzes liefern viele gültige Treffer. Das Rezept entscheidet nicht, ob Punkt, Komma oder nichts erforderlich ist.

### D20. Gerade Anführungszeichenpaare

Ein einfaches Fragment in geraden doppelten Anführungszeichen auf einer Zeile finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
"([^"\r\n]+)"
```

Kontrolltext:

```text
Он сказал "да".
```

Erwarteter Treffer:

```text
"да"
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Он сказал «да».
```


Einschränkungen und Hinweise: Verschachtelung, Zollzeichen, Code und das gewählte Anführungszeichensystem prüfen. «$1» ist nur in ausgewähltem Kontext eine geeignete Ersetzung, keine universelle Typografie.

### D21. Bindestrich oder Geviertstrich zwischen Zahlen

Möglichen Zahlenbereich zur redaktionellen Prüfung finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?<![0-9])[0-9]+[ \t]*[-—][ \t]*[0-9]+(?![0-9])
```

Kontrolltext:

```text
Страницы 12 - 15.
```

Erwarteter Treffer:

```text
12 - 15
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Страницы 12–15.
```


Einschränkungen und Hinweise: Auch 2026-10-02, negative Zahlen oder Subtraktion können passen. Nicht automatisch in Bereiche umwandeln. – ist Halbgeviertstrich, — Geviertstrich, - Bindestrich.

### D22. Zwei Initialen vor dem Nachnamen

Einfache Schreibweise mit zwei Initialen zur Abstandsprüfung finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
\b(\p{Lu})\.[ \t]*(\p{Lu})\.[ \t]+(\p{Lu}\p{Ll}+)\b
```

Kontrolltext:

```text
И.О. Иванов
```

Erwarteter Treffer:

```text
И.О. Иванов
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Иванов Иван
```


Einschränkungen und Hinweise: Nicht alle zusammengesetzten Namen und Diakritika erfasst. Nach Prüfung kann $1., NBSP, $2., NBSP, $3 verwendet werden; echte geschützte Leerzeichen einfügen, nicht deren Namen.

### D23. Nachname vor zwei Initialen

Die umgekehrte Namensreihenfolge finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
\b(\p{Lu}\p{Ll}+)[ \t]+(\p{Lu})\.[ \t]*(\p{Lu})\.
```

Kontrolltext:

```text
Иванов И.О.
```

Erwarteter Treffer:

```text
Иванов И.О.
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Иванов Иван
```


Einschränkungen und Hinweise: Formatprüfung, keine Identifizierung einer Person. Zusammengesetzte Namen, Partikeln und drei Initialen benötigen eine eigene Regel.

### D24. Unsichtbare Zeichen prüfen

Bedingten Trennstrich, Zero Width Space oder FEFF im Text finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[\x{00AD}\x{200B}\x{FEFF}]
```

Kontrolltext:

```text
сло​во
```

Derselbe Kontrolltext mit sichtbaren Zeichen:

```text
сло[ZWSP]во
```

Dabei steht ␠ für ein gewöhnliches Leerzeichen, [TAB] für einen Tabulator, [NBSP] für U+00A0, [NNBSP] für U+202F und [ZWSP] für U+200B. Dies ist eine erläuternde Darstellung; die Bezeichnungen werden nicht in das Buch eingefügt.

Erwarteter Treffer:

```text
​
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
слово
```


Einschränkungen und Hinweise: Der Treffer ist unsichtbar: Zwischen о und в steht U+200B. Erst nach Klärung des Zwecks einfügen oder löschen. Datei-BOM und FEFF im Text sind verschiedene Fälle.

### D25. Ungewöhnliche Unicode-Leerzeichen

Schmale, breite und andere Sonderleerzeichen finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]
```

Kontrolltext:

```text
10 000
```

Derselbe Kontrolltext mit sichtbaren Zeichen:

```text
10[NNBSP]000
```

Dabei steht ␠ für ein gewöhnliches Leerzeichen, [TAB] für einen Tabulator, [NBSP] für U+00A0, [NNBSP] für U+202F und [ZWSP] für U+200B. Dies ist eine erläuternde Darstellung; die Bezeichnungen werden nicht in das Buch eingefügt.

Erwarteter Treffer:

```text
 
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
10 000
```


Einschränkungen und Hinweise: Ein schmales geschütztes Leerzeichen zwischen Zifferngruppen kann völlig richtig sein. Das Rezept deckt Uneinheitlichkeit auf, erklärt nicht alle solchen Zeichen zum Fehler.

### D26. Wiederholte Frage- und Ausrufezeichen

Expressive oder versehentlich doppelte Satzzeichen finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[!?]{2,}
```

Kontrolltext:

```text
Что?! Правда!!!
```

Erwartete Treffer nacheinander:

```text
?!
!!!
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Что? Правда!
```


Einschränkungen und Hinweise: ?! und autorielle Wiederholungen können gewollt sein. Vorgesehen ist Prüfung, nicht Kürzung jeder Folge auf ein Zeichen.

### D27. Kyrillisches Х neben römischen Zahlenzeichen

Russisches Х in ansonsten lateinischer römischer Nummerierung finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — für diesen Ausdruck nicht erforderlich. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?<=[IVXLCDM])Х|Х(?=[IVXLCDM])
```

Kontrolltext:

```text
Глава IХ
```

Erwarteter Treffer:

```text
Х
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Глава IX
```


Einschränkungen und Hinweise: Х ist kyrillisch, X lateinisch. Groß-/Kleinschreibung und Kontext prüfen; der gesamte römische Zahlenwert wird nicht validiert.

### D28. Mehrere Großbuchstaben vor Kleinbuchstaben

Möglichen OCR-Schreibungsfehler am Wortanfang finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Unicode (UCP)“ — ein. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
\p{Lu}{2,}\p{Ll}+
```

Kontrolltext:

```text
Он сказал ПРИвет.
```

Erwarteter Treffer:

```text
ПРИвет
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
Он сказал Привет.
```


Einschränkungen und Hinweise: Namen, Abkürzungen mit Endungen und lateinische Kürzel können korrekt sein. Keine globale Schreibungsänderung ohne Prüfung.

## 18. Häufige Fehler und Diagnose

### 18.1. Text ist sichtbar, aber es gibt keinen Treffer

Prüfen Sie Design-Modus, Regex-Kästchen, Groß-/Kleinschreibung, Bereich, Richtung und tatsächliche Zeichen. Lateinisches `a` und kyrillisches `а` sehen ähnlich aus, sind aber verschieden. NBSP ist kein gewöhnliches Leerzeichen; typografische und gerade Anführungszeichen sind nicht gleich.

Prüfen Sie UCP für russische Wortgrenzen und die Schreibungsoption für Groß-/Kleinbuchstabenmuster. Kombinieren Sie „Nur ganze Wörter“ nicht unnötig mit eigenen komplexen Grenzen.

### 18.2. Ein zu großes Fragment wird gefunden

Verdächtig sind zunächst gieriges Punkt-Stern oder eine zu breite negative Klasse. Ersetzen Sie „beliebiger Text“ durch eine explizite erlaubte Menge und begrenzen Sie Länge und Grenzen. Prüfen Sie dotall.

### 18.3. Leerraum erfasst Absätze

Verwenden Sie `\s+` nicht als Synonym für Leerzeichen. Beginnen Sie für Wortzwischenräume mit `[ \t]+` und ergänzen Sie NBSP nur bei Bedarf.

### 18.4. Ziffern erscheinen, Backslashes verschwinden oder Gruppennamen funktionieren nicht

Vergleichen Sie die FBE-Ersetzungsgrammatik in Abschnitt 16. `${name}`, `$10`, `\n` und `\x{...}` folgen nicht automatisch den Suchregeln oder denen anderer Editoren. Der Capture muss tatsächlich vorhanden sein und darf nicht bloß optional in einem anderen Zweig vorkommen.

### 18.5. Gemischte Alphabete in einem normalen zweisprachigen Satz

Ein früheres Beispiel prüfte Kyrillisch und Lateinisch irgendwo in einer ganzen Zeile und fand daher auch `Он прочитал Latin.`. D12 begrenzt beide Prüfungen auf ein Wort. Das unterscheidet OCR-Fehlersuche wesentlich von der Suche nach zweisprachigen Zeilen.

### 18.6. „Fehlender Punkt“ schlägt bei korrektem Zitat an

Nur das letzte Zeichen zu prüfen reicht nicht: Auf `!` kann `»` folgen. D17 berücksichtigt schließende Anführungszeichen und Klammern; Überschriften und Anmerkungen bleiben dennoch prüfbedürftig.

### 18.7. Die gesuchte Struktur wird nicht gefunden

Ein Textregex sieht das DOM nicht wie ein strukturelles Skript. Tagsuche, Verschachtelungsprüfung, fehlende ID-Ziele und Verschieben von Anmerkungen sind eigene Aufgaben. Der nächste richtige Schritt kann Quelltextmodus, FB2-Validator oder Skript sein statt eines noch komplizierteren Ausdrucks.

## 19. Leistung und große Bücher

Beginnen Sie eng: mit einem bestimmten Zeichen, einer Klasse, einem Wort oder Absatzanfang. Vermeiden Sie mehrfach verschachtelte unbegrenzte Wiederholungen und konkurrierende „beliebiger Text“-Teile. Bei erfolgloser Suche können sie sehr viele Möglichkeiten durchlaufen.

Nichtgierigkeit ist kein Allheilmittel gegen langsame Muster: Auch sie probiert Alternativen. Häufig helfen expliziter Trennzeichenausschluss, Längenbegrenzung und kleinerer Suchbereich.

Starten Sie während einer langwierigen Suche nicht weitere Ersetzungen. Vereinfachen und prüfen Sie den Ausdruck zuerst an kurzem Kontrolltext. Ein PCRE2-Ressourcenfehler bedeutet nicht „keine Treffer“.

Für mehrstufige Bearbeitung mit Nachbarabsätzen und Tags ist ein Skript oft verständlicher und sicherer. Fassen Sie nicht sämtliche Korrekturregeln in einer riesigen Alternative zusammen; einzelne Regeln erklären jeden Treffer besser.

## 20. Kontrolle vor dem Speichern

Sehen Sie Anfang, Mitte und Ende des bearbeiteten Bereichs durch. Prüfen Sie mehrere Treffer jeder Art, insbesondere Zitate, Zahlenbereiche, Initialen, Anmerkungen und erhaltene Formatierung. Wichtige NBSP dürfen nicht verschwunden und Absätze nicht unerwartet verändert sein.

Führen Sie nach strukturell empfindlichen Änderungen die FBE-Dokumentprüfung aus. Speichern Sie, öffnen Sie bei Bedarf erneut und vergleichen Sie. Erfolgreiche Regex-Suche ersetzt weder FB2-Validierung noch Korrekturlesen.

## 21. Quellen und Geltungsbereich

Diese Übersetzung beruht auf dem erweiterten russischen Handbuch zum bereitgestellten regex-design.md. Erklärungen und Kontrollbeispiele wurden neu verfasst; ungenaue Aussagen zu UCP, Suchobjektgrenzen, gemischten Alphabeten und Ersetzungsgrammatik anhand von Primärquellen präzisiert. Abschnitt 17 enthält redaktionelle Szenarien, keine Zitate aus dem PCRE2-Handbuch.

[D1] Offizielle PCRE2-Syntaxdokumentation: Anker, Gruppen, Unicode-Eigenschaften, Backtracking und Optionen.

`https://www.pcre.org/current/doc/html/pcre2pattern.html`

[D2] Offizielle Unicode-Dokumentation von PCRE2.

`https://www.pcre.org/current/doc/html/pcre2unicode.html`

[D3] FBE Next: PCRE2-Kompatibilität und Umhüllung. Repository-Stand d2257405d95b0328649acee64b38829b40a4314b.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/docs/pcre2-compatibility.md`
