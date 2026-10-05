# Hilfe zu regulären Ausdrücken — Quelltext

Hinweis zur Übersetzung: Russische Wörter und Sätze in den Kontrollbeispielen bleiben absichtlich unverändert. Sie sind Testdaten: Reguläre Ausdrücke, Ersetzungstexte, Leerraumzeichen und erwartete Ergebnisse entsprechen der russischen Referenzfassung. Die Erläuterungen sind übersetzt; die Beispiele werden nicht automatisch an deutsche typografische Regeln angepasst.

Vollständiges Handbuch zum Suchen und Ersetzen im XML-Quelltext eines Buches in FictionBook Editor Next.

Stand: 2. Oktober 2026. Design wird in regex-design.md beschrieben. Diese Rezepte gelten für den Quelltextmodus; PCRE2-Ausdrücke dürfen nicht ungeprüft übernommen werden.

## 1. Zweck der Suche im Quelltextmodus

Hier sind XML-Tags, Attribute, Links, Entitäten und Buchtext sichtbar. Der Modus eignet sich zur FB2-Prüfung: leere Elemente, importierte HTML-Tags, Platzhalterlinks, unerwartete Attribute und technische Konvertierungsreste.

Für literarisches Korrekturlesen gewöhnlicher Wörter ist Design oft bequemer. Im XML kann ein Treffer auch in Attributen, Kommentaren, CDATA, Dateinamen oder Binärdaten liegen. Das ist vor jeder Ersetzung zu berücksichtigen.

„Nur suchen und prüfen“ liefert Kandidaten und beweist keinen XML- oder Textfehler. Selbst einfache Leerzeichenersetzungen können bedeutenden Inhalt ändern; über beliebiges XML gibt es keine universell sichere Ersetzung.

## 2. Schnellstart

Speichern Sie eine Kopie, wechseln Sie zum Quelltext, öffnen Sie Suchen und aktivieren Sie reguläre Ausdrücke. Die XML-Beispiele beachten normalerweise Groß-/Kleinschreibung und deaktivieren „Nur ganze Wörter“. Tag- und Attributgrenzen stehen im Muster.

Für einen einfachen leeren Absatz:

```regex
<p>[ \t]*</p>
```

Gefunden werden `<p></p>` und `<p>   </p>` auf einer physischen Zeile, nicht `<p>Текст</p>`. Ersetzen Sie das Element nicht automatisch durch nichts oder `<empty-line/>`: Absatz und Leerzeilenelement können unterschiedliche strukturelle Aufgaben haben.

Fügen Sie nur das Muster ein. JavaScript `/.../g`, C++-Stringanführungszeichen und doppelte JSON-Backslashes sind unnötig.

## 3. Die Engine: Scintilla statt PCRE2

FBE verwendet Scintilla mit `SCFIND_REGEXP` und `SCFIND_CXX11REGEX`. Im betrachteten Build ist dies eine C++-Regex-Implementierung mit ECMAScript-Grammatik, weder PCRE2 noch der vollständige Funktionsumfang modernen JavaScripts. [S1, S2]

Scintilla hat außerdem einen älteren Basismodus mit anderen Regeln. Alte Beispiele, die Gruppen mit maskierten Klammern bilden, gehören nicht zu FBEs C++11-Modus: Hier erfasst `(слово)`, während `\(слово\)` wörtliche Klammern verlangt.

Unicode im XML ist nicht verboten, doch UCP und PCRE2-Eigenschaften fehlen. Zeichenklassen hängen von der Standardbibliothek ab; `\w` ist nicht automatisch jede Sprache. Für verlässliches ASCII verwenden Sie `[A-Za-z0-9_]`. `[А-Яа-яЁё]` kann russischen Text erfassen, deckt aber nicht ganz Kyrillisch ab.

### 3.1. Wichtigste Unterschiede zu Design

| Eigenschaft | Design | Quelltext |
| --- | --- | --- |
| Suchobjekt | Textdarstellung des Buches | XML-Quelltext |
| Engine | PCRE2-16 | Scintilla C++11 |
| UCP und Unicode-Eigenschaften | Im PCRE2-Profil verfügbar | Nicht als PCRE2-Syntax unterstützt |
| Lookbehind | Mit PCRE2-Beschränkungen | Nicht unterstützt |
| Erste Gruppe ersetzen | `$1` oder `\1` | `\1` |
| Formatierung bei Ersetzung | FBE-Befehle | Nur XML-Text, keine FBE-Befehle |
| Suche über physische Zeilen | Abhängig von Suchdarstellung; absatzübergreifende Ersetzung beschränkt | Im aktuellen Pfad nicht unterstützt |

## 4. Zeilenweise Suche: die wichtigste Einschränkung

Im verwendeten Scintilla-Pfad gleicht `MatchOnLines` den Ausdruck separat mit jeder physischen Zeile ab. Eine Zeile kann sehr lang sein; visueller Fensterumbruch erzeugt keine neue physische Zeile. [S2]

Beispiel:

```xml
<empty-line/> <empty-line/>
```

und:

```xml
<empty-line/>
<empty-line/>
```

Im XML können beide dasselbe Nachbarschaftsverhältnis beschreiben. Für die aktuelle Regex-Suche sind sie verschieden: Das erste Paar passt in einen Treffer, das zweite überschreitet eine Zeilengrenze.

`\r\n`, `\n`, `\x0D`, `\x0A`, `\s*` oder eine Klasse für beliebige Zeichen heben dies nicht auf. Die Engine erhält die Zeilen nicht als gemeinsames Suchobjekt. Dafür wäre eine Änderung des Suchpfads nötig, nicht bloß ein anderes Muster.

Eine Ersetzung darf dagegen einen Zeilenwechsel einfügen. Das ist eine andere Operation, illustriert in K28. „Über Zeilengrenzen suchen“ und „nach einem Treffer eine Zeilengrenze einfügen“ sind zu unterscheiden.

### 4.1. Mehrzeiliges XML bearbeiten

Für ein allein stehendes Attribut genügt oft die Suche nach dem Attribut ohne gesamten öffnenden Tag. Beziehungen über mehrere Zeilen benötigen FBE-Strukturwerkzeuge, Validator, Skript oder XML-Werkzeug.

Entfernen Sie nicht sämtliche Zeilenwechsel, um einen Regex zu ermöglichen. Das kann Text, Kommentare, CDATA und Lesbarkeit beeinflussen. Prüfen Sie zunächst, ob mehrzeiliger Abgleich wirklich nötig ist.

## 5. Zeichen und Maskierung

Ein gewöhnliches Zeichen steht für sich selbst. Für ein wörtliches Metazeichen dient der Backslash.

| Gesuchter Text | Ausdruck |
| --- | --- |
| Punkt | `\.` |
| Plus | `\+` |
| Fragezeichen | `\?` |
| Stern | `\*` |
| Runde Klammer | `\(` oder `\)` |
| Zahl in eckigen Klammern | `\[[0-9]+\]` |
| Backslash | `\\` |

`.` bedeutet ein Zeichen im verfügbaren Zeilenfragment, keinen wörtlichen Punkt. Bei UTF-8 hängt die Verarbeitung von Scintilla-Adapter und Standardbibliothek ab; messen Sie Emoji-Längen nicht unter der Annahme „ein Punkt gleich ein sichtbares Zeichen“.

`\r` und `\n` bezeichnen CR/LF, erlauben im zeilenweisen Suchpfad aber keinen zeilenübergreifenden Treffer. Für Leerzeichen und Tabulatoren verwenden Sie `[ \t]`.

PCRE2s `\Q...\E` ist hier keine portable Literal-Maskierung. Maskieren Sie Sonderzeichen einzeln.

## 6. Klassen, Bereiche und Wortgrenzen

`[abc]` findet ein Zeichen der Liste, `[a-z]` eines Bereichs, `[^abc]` eines außerhalb. Danach kann eine Wiederholung stehen: `[0-9]+`.

| Klasse | Praktische Bedeutung |
| --- | --- |
| `[0-9]` | ASCII-Ziffer |
| `[A-Za-z]` | Lateinischer ASCII-Buchstabe |
| `[ \t]` | Gewöhnliches Leerzeichen oder Tabulator |
| `[^<]` | Verfügbares Zeichen außer öffnender spitzer Klammer |
| `[^"']` | Zeichen außer beiden geraden Anführungszeichenarten |
| `\d`, `\D` | Ziffernklasse der Implementierung und Negation |
| `\s`, `\S` | Leerraumklasse und Negation |
| `\w`, `\W` | Wortklasse und Negation |

Negative Klassen verstehen XML nicht. `[^<]*` eignet sich für einfachen Text zwischen Tags, ist aber kein vollständiger Parser für Entitäten, Kommentare oder CDATA.

`\b` ist eine Wortgrenze, `\B` keine. Für Attribute reicht das oft nicht: `id` kann nach einem Doppelpunkt zu einem anderen Namen gehören. Prüfen Sie erlaubte Attributtrenner ausdrücklich: Zeilenanfang, Leerzeichen oder Tabulator.

## 7. Anker und Wiederholung

`^` und `$` bezeichnen im aktuellen Pfad Anfang und Ende einer physischen Zeile. Für eine reine Leerraumzeile:

```regex
^[ \t]+$
```

Die Zeichen können gelöscht werden; die physische Zeile bleibt, da ihr Zeilenwechsel nicht zum Treffer gehört.

| Quantifizierer | Wiederholungen |
| --- | --- |
| `?` | Null oder eins |
| `*` | Null oder mehr |
| `+` | Eins oder mehr |
| `{3}` | Genau drei |
| `{3,}` | Mindestens drei |
| `{2,5}` | Zwei bis fünf |

C++11 unterstützt nichtgierige Formen wie `*?` und `+?`. Für XML-Attribute ist der Ausschluss des begrenzenden Anführungszeichens oft klarer:

```regex
"[^"\r\n]*"
```

Dies gilt für doppelte Anführungszeichen. Einfache benötigen einen eigenen entsprechenden Zweig.

Übernehmen Sie keine possessiven PCRE2-Quantifizierer, atomaren Gruppen oder PCRE2-Inline-Optionen.

## 8. Gruppen, Alternativen und Vorausschau

Eine erfassende Gruppe `( ... )` speichert ein Fragment für Rückverweis und Ersetzung. `(?: ... )` fasst Teile ohne Nummer zusammen.

Auswahl mehrerer Tags:

```regex
<(?:strong|emphasis)>
```

Ein numerischer Suchrückverweis kann Öffnungs- und Schließnamen eines einfachen einzeiligen Fragments verbinden:

```regex
<(strong|emphasis)>[ \t]*</\1>
```

Beide Namen müssen identisch sein. Beliebige XML-Verschachtelung wird damit nicht geprüft.

Positives Lookahead `(?=...)` und negatives `(?!...)` prüfen folgenden Kontext, ohne ihn einzuschließen. Hinter einem Tagnamen kann so eine erlaubte Grenze verlangt werden, damit `<a` nicht den Anfang von `<author>` trifft.

```regex
<a(?=[ \t>])
```

Linken Kontext können Sie erfassen und in der Ersetzung wiederherstellen; Lookbehind ist nicht verfügbar. Kopieren Sie keine `(?<=...)`-Ausdrücke aus der Design-Hilfe.

## 9. Ersetzung im Quelltextmodus

FBE verwendet `SCI_REPLACETARGETRE`: Ein Treffer liegt bereits vor, und Scintilla interpretiert die Ersetzung. Das ist weder `std::regex_replace` mit üblichem `$1` noch FBEs Design-Grammatik. [S1, S2]

### 9.1. Capture-Verweise

| Im Ersetzungsfeld | Bedeutung |
| --- | --- |
| `\0` | Gesamter Treffer |
| `\1` … `\9` | Entsprechende Gruppen |
| `$1` | Nicht als Gruppenverweis verwenden; falsche Syntax für diesen Pfad |

Suchen:

```regex
\(([0-9]+)\)
```

Ersetzen durch:

```text
[\1]
```

Aus `(12)` wird `[12]`. Das ist nur in ausgewähltem Kontext redaktionell sinnvoll: Eine eingeklammerte Zahl ist nicht automatisch ein Anmerkungszeichen.

Ein wörtliches Dollarzeichen muss im Quelltext-Ersetzungsfeld nicht nach fremden Regeln verdoppelt werden. Der Backslash ist dagegen ein Steuerzeichen.

### 9.2. Steuerzeichen in Ersetzungen

Im geprüften Scintilla-Code werden `\t`, `\n`, `\r` und `\\` als Tabulator, LF, CR und Backslash verarbeitet. Das widerspricht nicht dem fehlenden zeilenübergreifenden Suchen. [S2]

CRLF wird mit `\r\n`, LF mit `\n` eingefügt. Richten Sie sich nach dem Dokument und erzeugen Sie keine gemischten Zeilenenden.

PCRE2-Folgen wie `\x{00A0}` fügen hier kein Unicode-Zeichen ein; verwenden Sie echtes NBSP. Design-Befehle `\U`, `\L`, `\T`, `\S`, `\E` und `\Q` steuern hier weder Schreibweise noch Formatierung.

### 9.3. Löschen und XML-Erhaltung

Ein leeres Ersetzungsfeld löscht den Trefferbereich. Leere Tags, Attribute oder Links zu löschen kann trotz erfolgreicher Suche das Dokument beschädigen. Die folgenden Strukturrezepte sind überwiegend diagnostisch.

Ein `id` nur an einer Stelle umzuordnen oder umzubenennen kann alte Referenzen hinterlassen. Verwenden Sie für verbundene Objekte die ID-Umbenennung des Editors statt unabhängiger Massenersetzungen.

## 10. XML-Rezepte richtig lesen

XML unterscheidet Groß- und Kleinschreibung. `<p>` und `<P>` sind verschiedene Namen. Attributreihenfolge bestimmt nicht deren Bedeutung; Werte dürfen einfach oder doppelt zitiert sein, um das Gleichheitszeichen sind Leerzeichen erlaubt. Die Beispiele berücksichtigen übliche Formen, solange das Muster nicht übermäßig kompliziert wird. [S3]

`l:` und `xlink:` sind verbreitete XLink-Präfixe in FB2. Entscheidend ist die `xmlns`-Deklaration, nicht das Präfix selbst. Regeln mit diesen beiden Formen erfassen nicht automatisch alle anderen Präfixe. Passen Sie die Regel bei Bedarf an und prüfen Sie die Deklaration.

Viele Beispiele nähern den Inhalt eines öffnenden Tags mit `[^>]*` an. Für gewöhnliches FB2 ist das bequem, doch `>` darf in Attributwerten stehen. Kommentare und CDATA können Markup imitieren. Daher sind dies Kandidatensuchen, keine vollständige XML-Prüfung oder blinde Umstrukturierung.

Ein „leeres Element“ ist nicht stets ein Schemafehler, eine „numerische Entität“ kein beschädigtes Zeichen. Platzhalter, ungewöhnliche Namen und externe Links können legitim sein.

## 11. Praktische FB2/XML-Rezepte

Standardmäßig müssen alle beteiligten Teile auf derselben physischen Zeile liegen. Sucht das Muster einen ganzen öffnenden Tag, müssen auch sämtliche geprüften Attribute dort stehen. Bei reiner Attributsuche muss nicht der gesamte Tag einzeilig sein.

### K01. Leerzeichen am physischen Zeilenende

Nachgestellte gewöhnliche Leerzeichen und Tabulatoren finden.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[ \t]+$
```

Kontrolltext:

```text
<p>Текст</p>
```

Derselbe Kontrolltext mit sichtbaren Zeichen:

```text
<p>Текст</p>␠␠␠
```

Dabei steht ␠ für ein gewöhnliches Leerzeichen, [TAB] für einen Tabulator, [NBSP] für U+00A0, [NNBSP] für U+202F und [ZWSP] für U+200B. Dies ist eine erläuternde Darstellung; die Bezeichnungen werden nicht in das Buch eingefügt.

Ersetzen durch: das Feld vollständig leer lassen. Nicht das Wort „leer“ eingeben.

Ergebnis der Ersetzung:

```text
<p>Текст</p>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<p>Текст</p>
```


Einschränkungen und Hinweise: Löscht keinen Zeilenwechsel. In gemischtem XML-Inhalt, CDATA und leerraumsensitiven Bereichen können sie zum Text gehören. Nicht jede Zeilenbereinigung ist bedingungslos sicher.

### K02. Reine Leerraumzeile leeren

Leerraumzeichen einer sonst leeren Zeile entfernen.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
^[ \t]+$
```

Kontrolltext:

```text

<p>Text</p>
```

Derselbe Kontrolltext mit sichtbaren Zeichen:

```text
␠␠␠[TAB]
<p>Text</p>
```

Dabei steht ␠ für ein gewöhnliches Leerzeichen, [TAB] für einen Tabulator, [NBSP] für U+00A0, [NNBSP] für U+202F und [ZWSP] für U+200B. Dies ist eine erläuternde Darstellung; die Bezeichnungen werden nicht in das Buch eingefügt.

Ersetzen durch: das Feld vollständig leer lassen. Nicht das Wort „leer“ eingeben.

Ergebnis der Ersetzung:

```text

<p>Text</p>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<p>Text</p>
```


Einschränkungen und Hinweise: Die Zeile bleibt leer; LF/CRLF gehörte nicht zum Treffer. Dies löscht nicht alle leeren physischen Zeilen.

### K03. Leerzeichen vor dem Leerelement-Abschluss

Gewöhnlichen Zwischenraum direkt vor /> finden.

Aktion: nach Prüfung ersetzen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
[ \t]+/>
```

Kontrolltext:

```text
<empty-line />
```

Ersetzen durch:

```text
/>
```

Ergebnis der Ersetzung:

```text
<empty-line/>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<empty-line/>
```


Einschränkungen und Hinweise: XML erlaubt beide Schreibweisen. Die Änderung ist kosmetisch, keine zwingende Fehlerkorrektur. Die Folge kann auch in Text oder Kommentaren stehen; Sammelersetzungen prüfen.

### K04. Einfacher leerer Absatz

Ein p-Paar ohne Textinhalt prüfen.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
<p>[ \t]*</p>
```

Kontrolltext:

```text
<p>  </p><p>Text</p>
```

Erwarteter Treffer:

```text
<p>  </p>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<p>Text</p>
```


Einschränkungen und Hinweise: Attribute, Zeilenwechsel, NBSP-Entität und verschachtelte Tags sind nicht erfasst. Leerer Absatz und empty-line sind nicht automatisch austauschbar.

### K05. Zwei empty-line auf einer Zeile

Benachbarte FB2-Leerzeilenelemente auf einer physischen Quelltextzeile finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>
```

Kontrolltext:

```text
<empty-line/> <empty-line />
```

Erwarteter Treffer:

```text
<empty-line/> <empty-line />
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<empty-line/>
<empty-line/>
```


Einschränkungen und Hinweise: Dazwischen sind nur gewöhnliche Leerzeichen und Tabulatoren erlaubt. Zeilenwechsel sind absichtlich ausgeschlossen. Zwei Leerzeilen können Szenen trennen.

### K06. Leere wichtige Metadaten

Mehrere einfache Metadatenfelder ohne Inhalt prüfen.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
<(book-title|first-name|middle-name|last-name|genre|lang)>[ \t]*</\1>
```

Kontrolltext:

```text
<book-title> </book-title>
```

Erwarteter Treffer:

```text
<book-title> </book-title>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<book-title>Book</book-title>
```


Einschränkungen und Hinweise: Nicht jedes Feld ist überall Pflicht. Regex prüft weder FB2-Schema noch Elternstruktur oder die Richtigkeit gefüllter Werte.

### K07. Leere Inline-Formatierung

Leere paarige Formatierungselemente finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
<(strong|emphasis|strikethrough)>[ \t]*</\1>
```

Kontrolltext:

```text
<strong> </strong>
```

Erwarteter Treffer:

```text
<strong> </strong>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<strong>Text</strong>
```


Einschränkungen und Hinweise: NBSP, Attribute oder verschachtelte Elemente brauchen andere Regeln. Vor dem Löschen die mögliche strukturelle Bedeutung prüfen.

### K08. Gleiche Formatierung in sich verschachtelt

strong/strong von zulässigem strong/emphasis unterscheiden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
<(strong|emphasis)>[ \t]*<\1(?=[ \t>])
```

Kontrolltext:

```text
<strong><strong>Text</strong></strong>
```

Erwarteter Treffer:

```text
<strong><strong
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<strong><emphasis>Text</emphasis></strong>
```


Einschränkungen und Hinweise: Der zweite Name wird nicht unabhängig gewählt, sondern verweist auf den ersten. Der Treffer enthält nicht das ganze Element und ist nicht zum vollständigen Löschen gedacht.

### K09. Mögliche HTML-Tags nach Import

Verbreitete HTML-Namen finden, die in FB2 zu prüfen sind.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
</?(?:b|i|br|div|span|font)(?=[ \t/>])[^>]*>
```

Kontrolltext:

```text
<div>Text</div>
```

Erwartete Treffer nacheinander:

```text
<div>
</div>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<section><p>Text</p></section>
```


Einschränkungen und Hinweise: Kommentare und CDATA können tagähnliche Treffer liefern. b/strong und i/emphasis nicht pauschal ohne Inhalts- und Attributprüfung ersetzen.

### K10. Tag in Großbuchstaben

Übliche vollständig großgeschriebene Tagnamen finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
</?[A-Z][A-Z0-9_.:-]*(?=[ \t/>])[^>]*>
```

Kontrolltext:

```text
<P>Text</P>
```

Erwartete Treffer nacheinander:

```text
<P>
</P>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<p>Text</p>
```


Einschränkungen und Hinweise: Groß-/Kleinschreibung muss beachtet werden, sonst trifft die Regel auch p. Nicht sämtliche gültigen Unicode-XML-Namen oder zulässigen Namensräume werden geprüft.

### K11. HTML-NBSP-Entität

Wörtliches &nbsp; in der Datei finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
&nbsp;
```

Kontrolltext:

```text
<p>&nbsp;</p>
```

Erwarteter Treffer:

```text
&nbsp;
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<p>&#160;</p>
```


Einschränkungen und Hinweise: Ohne passende Deklaration gehört &nbsp; nicht zu den fünf vordefinierten XML-Entitäten. DTD und Kommentarkontext prüfen; keine globale Entitätsdekodierung.

### K12. Numerische Zeichenreferenzen

Dezimale oder hexadezimale Zeichennotation finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
&#(?:[0-9]+|x[0-9A-Fa-f]+);
```

Kontrolltext:

```text
<p>&#160; &#xA0;</p>
```

Erwartete Treffer nacheinander:

```text
&#160;
&#xA0;
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<p>&amp;</p>
```


Einschränkungen und Hinweise: Beide Formen können richtig sein. Geprüft wird die Form, nicht die Gültigkeit des Codepoints. &lt; als wörtliches < zu dekodieren kann XML beschädigen.

### K13. Leeres id

Leeres gewöhnliches id-Attribut mit beiden Anführungszeichenarten finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:""|'')
```

Kontrolltext:

```text
<section id="">
```

Erwarteter Treffer:

```text
 id=""
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<section id="s1">
```


Einschränkungen und Hinweise: Das führende Leerzeichen ist Teil des Treffers. Präfixnamen wie xml:id fehlen. Neue IDs erfordern Referenz- und Eindeutigkeitsprüfung, die Regex nicht leistet.

### K14. id-Werte auflisten

Gefüllte gewöhnliche IDs zur Namensprüfung finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:"[^"]+"|'[^']+')
```

Kontrolltext:

```text
<section id="s1">
```

Erwarteter Treffer:

```text
 id="s1"
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<section id="">
```


Einschränkungen und Hinweise: Trefferlisten beweisen weder Eindeutigkeit noch Gültigkeit. Verbundene Objekte mit FBEs Umbenennungsfunktion bearbeiten.

### K15. Externe HTTP(S)-Links

href mit verbreitetem XLink-Präfix und externer Adresse finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"https?://[^"]+"|'https?://[^']+')
```

Kontrolltext:

```text
<a l:href="https://example.test/book">Text</a>
```

Erwarteter Treffer:

```text
 l:href="https://example.test/book"
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<a l:href="#note1">1</a>
```


Einschränkungen und Hinweise: Sucht das Attribut, nicht jede URL im Text. l und xlink werden unterstützt, andere Präfixe separat berücksichtigen. Erreichbarkeit wird nicht geprüft.

### K16. Lokale file://-Links

Lokale Dateiverknüpfungen finden, die beim Leser ausfallen können.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"file://[^"]+"|'file://[^']+')
```

Kontrolltext:

```text
<a xlink:href="file:///C:/Book/image.png">Text</a>
```

Erwarteter Treffer:

```text
 xlink:href="file:///C:/Book/image.png"
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<a xlink:href="#image1">Text</a>
```


Einschränkungen und Hinweise: Unbekannte Adressen nicht automatisch öffnen. Der Treffer entscheidet nicht zwischen Einbetten, Ersetzen und Entfernen.

### K17. Leeres Linkziel

Gewöhnliche leere XLink-Ziele finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:""|'')
```

Kontrolltext:

```text
<a l:href="">Text</a>
```

Erwarteter Treffer:

```text
 l:href=""
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<a l:href="#note1">Text</a>
```


Einschränkungen und Hinweise: Leerer Linktext und fehlendes href sind andere Fälle und müssen getrennt geprüft werden.

### K18. Ziel #undefined

Wörtliches Platzhalterziel finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')
```

Kontrolltext:

```text
<a xlink:href="#undefined">Text</a>
```

Erwarteter Treffer:

```text
 xlink:href="#undefined"
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<a xlink:href="#note1">Text</a>
```


Einschränkungen und Hinweise: Die Existenz des Objekts prüfen. undefined ist als XML-Name nicht grundsätzlich verboten, weist praktisch aber oft auf eine unvollständige Zuordnung hin.

### K19. Links mit bookmark-Präfix

Typische Konverter-bookmark-Ziele statt aller internen Links finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#bookmark[^"]*"|'#bookmark[^']*')
```

Kontrolltext:

```text
<a l:href="#bookmark12">Text</a>
```

Erwarteter Treffer:

```text
l:href="#bookmark12"
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<a l:href="#note1">Text</a>
```


Einschränkungen und Hinweise: href="#..." trifft jeden internen Link und isoliert keine bookmark-Artefakte. Ein bookmark beweist keinen überflüssigen Link; vor Löschen prüfen.

### K20. Word/FBD-Rückverweise

Verbreitete _ftnref- und _ednref-Ziele finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#_(?:ftnref|ednref)[^"]*"|'#_(?:ftnref|ednref)[^']*')
```

Kontrolltext:

```text
<a l:href="#_ftnref1">Back</a>
```

Erwarteter Treffer:

```text
l:href="#_ftnref1"
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<a l:href="#note1">Text</a>
```


Einschränkungen und Hinweise: Rückverweise können für Navigation nötig sein. Nicht allein wegen der Namensherkunft löschen.

### K21. Anmerkungslinks unabhängig von Attributreihenfolge

Öffnenden a-Tag mit type=note und internem XLink-href in beliebiger Reihenfolge finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
<a(?=[ \t>])(?=[^>]*[ \t]type[ \t]*=[ \t]*(?:"note"|'note'))(?=[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+'))[^>]*>
```

Kontrolltext:

```text
<a l:href="#note1" type="note">1</a>
```

Erwarteter Treffer:

```text
<a l:href="#note1" type="note">
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<a type="link" l:href="#note1">1</a>
```


Einschränkungen und Hinweise: Auch type vor href, xlink und einfache Anführungszeichen sind erlaubt. Alle geprüften Attribute müssen auf einer Zeile liegen. Werte mit >, Kommentare und Sondermarkup benötigen XML-Parsing. Zielanmerkung wird nicht geprüft.

### K22. Mögliche numerische Anmerkungsmarker

Zahl in eckigen, geschweiften oder runden Klammern finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
\[[0-9]+\]|\{[0-9]+\}|\([0-9]+\)
```

Kontrolltext:

```text
<p>Text [12], {3}, (4).</p>
```

Erwartete Treffer nacheinander:

```text
[12]
{3}
(4)
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<p>[note]</p>
```


Einschränkungen und Hinweise: Es können Literaturverweise, Formelnummern, Erklärungen oder Text sein. Keine Anmerkungserstellung oder Eindeutigkeitsprüfung.

### K23. Vergessene Werte Your/Name

Typische englische Namensplatzhalter in Metadaten prüfen.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
<first-name>Your</first-name>|<last-name>Name</last-name>
```

Kontrolltext:

```text
<first-name>Your</first-name>
```

Erwarteter Treffer:

```text
<first-name>Your</first-name>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<first-name>John</first-name>
```


Einschränkungen und Hinweise: Autor-/Erstellerkontext und wirklichen Namen prüfen. Nicht automatisch Angaben einer anderen Ausgabe einsetzen.

### K24. Nicht zugeordnete Illustration #undefined

image mit typischem Platzhalterverweis finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
<image(?=[ \t/>])[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')[^>]*>
```

Kontrolltext:

```text
<image l:href="#undefined"/>
```

Erwarteter Treffer:

```text
<image l:href="#undefined"/>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<image l:href="#cover"/>
```


Einschränkungen und Hinweise: FB2 verwendet gewöhnlich XLink-href statt HTML-src. Passende binary-ID separat prüfen. Fehlendes Bild und falscher Verweis sind unterschiedliche Ursachen.

### K25. Windows-1251-Deklaration

Alte Kodierungsangabe in XML-Deklaration finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
<\?xml[ \t]+[^?]*encoding[ \t]*=[ \t]*(?:"windows-1251"|'windows-1251')[^?]*\?>
```

Kontrolltext:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Erwarteter Treffer:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<?xml version="1.0" encoding="utf-8"?>
```


Einschränkungen und Hinweise: Nicht grundsätzlich falsch. windows-1251 durch utf-8 als Text zu ersetzen kodiert keine Dateibytes um. Speichern/Konvertieren und Deklaration müssen zusammenpassen.

### K26. Ampersand ohne bekannte Standardentität

& vor einer Folge finden, die keiner üblichen Zeichenreferenz entspricht.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
&(?!(?:amp|lt|gt|apos|quot);|#[0-9]+;|#x[0-9A-Fa-f]+;)
```

Kontrolltext:

```text
<p>A & B</p>
```

Erwarteter Treffer:

```text
&
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<p>A &amp; B</p>
```


Einschränkungen und Hinweise: In CDATA und Kommentaren ist & erlaubt, DTDs können weitere Entitäten deklarieren. Diagnosefilter, kein Validator. Kontextloses & → &amp; kann doppelt maskieren.

### K27. Gewöhnliche interne Links

Lokales #id-Ziel getrennt von bookmark-Artefakten suchen.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+')
```

Kontrolltext:

```text
<a l:href="#note1">1</a>
```

Erwarteter Treffer:

```text
l:href="#note1"
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<a l:href="https://example.test">Text</a>
```


Einschränkungen und Hinweise: Regex zeigt die Referenz, beweist aber nicht genau ein existierendes Ziel. Verwaiste oder doppelte Verweise erfordern Dokumentanalyse.

### K28. Zwei empty-line nach einzeiliger Suche trennen

Unterschied zwischen Suchbegrenzung und Zeilenwechseleinfügung demonstrieren.

Aktion: eine einzelne kontrollierte Ersetzung.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
(<empty-line[ \t]*/>)[ \t]*(<empty-line[ \t]*/>)
```

Kontrolltext:

```text
<empty-line/> <empty-line/>
```

Ersetzen durch:

```text
\1\r\n\2
```

Ergebnis der Ersetzung:

```text
<empty-line/>
<empty-line/>
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<empty-line/>
<empty-line/>
```


Einschränkungen und Hinweise: Die Steuerfolgen werden von Scintilla in CRLF umgesetzt. Bei LF-Dokumenten \1\n\2 verwenden. Kein allgemeiner XML-Formatierer und keine Aktivierung mehrzeiliger Suche.

### K29. Doppelte Leerzeichen in einfachem XML-Text

Textzwischenraum zwischen Tags mit mehreren gewöhnlichen Leerzeichen finden.

Aktion: nur suchen und prüfen.

„Regulärer Ausdruck“ — ein; „Nur ganze Wörter“ — aus. „Groß-/Kleinschreibung beachten“ — ein.

Suchen:

```regex
>[^<]*[ \t]{2,}[^<]*<
```

Kontrolltext:

```text
<p>one  two</p>
```

Erwarteter Treffer:

```text
>one  two<
```

Gegenbeispiel, das nicht gefunden werden darf:

```text
<p>one two</p>
```


Einschränkungen und Hinweise: Treffer enthält spitze Begrenzungszeichen und das ganze einfache Fragment. Wörter besser in Design korrigieren. Nicht den gesamten Treffer durch ein Leerzeichen ersetzen.

## 12. Warum Regex keine XML-Prüfung ersetzt

Ein Ausdruck kann zeigen, dass Text einer Zeichenfolge ähnelt. Er beweist nicht die Richtigkeit des gesamten Dokuments: Elementverschachtelung, FB2-Schema, Namensraumdeklarationen, eindeutige IDs und existierende Linkziele sind gesondert zu prüfen.

Zwei identische `id` auf verschiedenen physischen Zeilen lassen sich im aktuellen zeilenweisen Pfad nicht verlässlich durch einen einzigen Vergleich finden. Gleiches gilt für ein fehlendes Linkziel an anderer Buchstelle. Hierfür braucht man FBE-Prüfung und Dokumentstruktur.

Dekodieren Sie nicht alle XML-Entitäten: `&lt;` und `&amp;` schützen oft Text davor, Markup zu werden. Entfernen Sie nicht alles HTML-Ähnliche aus CDATA, Kommentaren oder Codezitaten.

Tagpaarigkeit, Attribute und Inhalt müssen von der Ersetzungsoperation erhalten werden. Nur `<strong>` zu ändern lässt den Schließtag unverändert. Ein Rückverweis im Beispiel macht spätere Ersetzungen nicht automatisch strukturell sicher.

## 13. Häufige Fehler

### 13.1. Kleine statt großer Tags werden gefunden

Aktivieren Sie die Beachtung der Groß-/Kleinschreibung. Für XML ist das nicht kosmetisch. `[A-Z]` ohne Unterscheidung hebt den Sinn einer Großbuchstabenprüfung auf.

### 13.2. Wörtliches $1 erscheint in der Ersetzung

Verwenden Sie `\1`. FBE-Quelltextersetzungen laufen durch Scintilla, nicht JavaScript oder ein anderes API mit `$1`.

### 13.3. \p, \K oder Lookbehind funktionieren nicht

Diese Konstruktionen gehören zu einem anderen Profil. Für Textaufgaben zu Design wechseln oder das XML-Rezept mit expliziten Klassen, Kontexterfassung, nicht erfassenden Gruppen und Lookahead umschreiben.

### 13.4. Benachbarte Tags werden nicht gefunden

Prüfen Sie physischen Zeilenwechsel, Leerzeichen vor `/>`, einfache Anführungszeichen, weitere Attribute und Namensraumpräfixe. Wörtliche Nachbarschaft im Quelltext ist nicht dieselbe wie im XML-Baum.

### 13.5. Anmerkungslink fehlt nach Attributumordnung

„Zuerst type, dann href“ hängt von der Reihenfolge ab. K21 prüft beides mit unabhängigen Lookaheads. Mehrzeilige Tags bleiben für Ganz-Tag-Suche unerreichbar; suchen Sie das Attribut oder verwenden Sie Strukturwerkzeuge.

### 13.6. strong/emphasis wird als Verschachtelungsfehler gemeldet

Verschiedene gültige Stile dürfen ineinander liegen. Gleiche Namen zu prüfen verlangt wie K08 einen Rückverweis, nicht zwei unabhängige Alternativgruppen.

### 13.7. „Bookmark“ findet alle Anmerkungen

Ein interner Link mit `#` ist nicht automatisch ein bookmark-Artefakt. Trennen Sie allgemeinen Linkaudit K27 von speziellem bookmark-Präfix K19.

### 13.8. Ersetzung beschädigt sichtbaren Text

Im Quelltext können Treffer in Text, Attributen, Kommentaren und Binärdaten liegen. Änderung rückgängig machen und Bereich oder Muster eingrenzen. Die Bezeichnung „sichere Ersetzung“ hebt XML-Kontext nicht auf.

## 14. Grenzen des Quelltextprofils

Verwenden Sie hier weder UCP, PCRE2-Eigenschaftsklassen, Lookbehind, moderne JavaScript/PCRE2-Namensgruppen, atomare Gruppen, possessive Quantifizierer, Branch Reset, Unterprogramme, PCRE2-Verben, `\K`, `\G` noch Design-Formatbefehle.

PCRE2-Inline-Optionen ersetzen nicht die Dialogkästchen. Neue Browser-JavaScript-Funktionen erscheinen nicht automatisch in einer C++11-ECMAScript-Implementierung.

MatchOnLines bleibt zeilenweise, unabhängig von der im Muster angegebenen Zeilenwechselfolge. Kompilierung mit einer anderen Regex-Engine beweist keine FBE-Kompatibilität.

Für Unicode-Wörter und komplexe Buchkorrektur dient Design, für XML-Struktur XML/FB2-Funktionalität. Kompensieren Sie Grenzen nicht mit unbegrenztem `.*`.

## 15. Leistung und lange XML-Zeilen

Ein kurzer Ausdruck ist nicht zwangsläufig schnell. Mehrere unbegrenzte Wiederholungen und konkurrierende Alternativen können große Zeilen stark belasten. Besonders riskant ist eine einzige Zeile mit ganzem Dokument oder großem binary-Inhalt.

Beginnen Sie möglichst mit konkretem Tag-/Attributnamen und begrenzter Klasse statt „beliebige Zeichen“. Suchen Sie nicht alle denkbaren Fehler gleichzeitig; getrennte Regeln lassen sich besser prüfen und rückgängig machen.

Fassen Sie nicht alle Zeilen zur Umgehung von MatchOnLines zusammen. Das verändert das Dokument und garantiert keine akzeptable Suchzeit.

## 16. Kontrolle nach Ersetzung

Vergleichen Sie erwarteten und tatsächlichen Trefferbereich. Bei Gruppen müssen Anführungszeichen, Präfixe, spitze Klammern und Schließtags erhalten bleiben. Kommentare, CDATA und binary dürfen nicht unbeabsichtigt betroffen sein.

FB2 prüfen, speichern und nach wesentlichen Änderungen erneut öffnen. Quelltext ↔ Design, Anmerkungen, Bilder und sichtbaren Text kontrollieren. Bei ID-Umbenennung sowohl Objekt als auch sämtliche Referenzen prüfen.

## 17. Quellen und Geltungsbereich

Dies übersetzt das erweiterte Handbuch zum bereitgestellten regex-source.md. Die Struktur „Engine → Syntax → Ersetzung → XML-Beispiele → Grenzen“ bleibt erhalten; zu allgemeine Rezepte wurden eingegrenzt. Erklärungen und Kontrollszenarien sind neu verfasst, technische Unterschiede anhand von Primärquellen geprüft.

[S1] Offizielle Scintilla-Dokumentation, Searching: C++11-Modus, Suchflags und SCI_REPLACETARGETRE.

`https://www.scintilla.org/ScintillaDoc.html#Searching`

[S2] Scintilla-Quellcode im FBE-Stand d2257405d95b0328649acee64b38829b40a4314b: Document.cxx, Cxx11RegexFindText, MatchOnLines, BuiltinRegex::SubstituteByPosition. Er unterscheidet zeilenübergreifende Suche von CR/LF-Einfügung bei Ersetzungen.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/third_party/scintilla/src/Document.cxx`

[S3] W3C XML 1.0 und Namespaces in XML: Namen, Attribute, Entitäten und Namensräume.

`https://www.w3.org/TR/xml/`

`https://www.w3.org/TR/xml-names/`

[S4] SearchPresetCatalog.cpp, mainfrm.cpp, FBEview.cpp und search-preset-source-scintilla-smoke.cpp desselben FBE-Stands belegen Profil und vorhandene redaktionelle Aufgaben.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/tools/tests/search-preset-source-scintilla-smoke.cpp`

Zusätzliche Rezepte müssen im Zielbuild geprüft werden. Lokale C++11-ECMAScript-Kontrolltests sind keine Ausführung von Windows Scintilla. Details stehen in der Archiv-README.
