# Ayuda de expresiones regulares — Código

Nota sobre la traducción: las palabras y frases rusas de los ejemplos de control se conservan intencionadamente. Son datos de prueba: las expresiones regulares, las cadenas de sustitución, los espacios y los resultados esperados coinciden con la edición rusa de referencia. Se han traducido las explicaciones; los ejemplos no se adaptan automáticamente a la tipografía española.

Guía completa de búsqueda y reemplazo en el XML de un libro en FictionBook Editor Next.

Edición: 2 de octubre de 2026. Diseño se describe en regex-design.md. Estas recetas son para Código; no copie patrones PCRE2 sin comprobarlos.

Nota de traducción: expresiones, reemplazos y muestras permanecen idénticos a la edición rusa aprobada. Los textos rusos de los ejemplos son intencionados y mantienen comparables resultados y espacios.

## 1. Para qué sirve buscar en Código

Aquí se ven etiquetas XML, atributos, enlaces, entidades y texto. Es apropiado para auditar FB2: elementos vacíos, HTML importado, enlaces de sustitución, atributos extraños y restos de conversión.

Para corregir palabras suele convenir Diseño. En XML una coincidencia puede estar en atributos, comentarios, CDATA, nombres de archivo o datos binarios. Considérelo antes de reemplazar.

«Solo buscar y revisar» ofrece candidatos, no prueba errores. Hasta cambiar espacios puede alterar contenido significativo; ningún reemplazo sobre XML arbitrario es universalmente seguro.

## 2. Inicio rápido

Guarde copia, cambie a Código, abra Buscar y active regex. Los ejemplos XML distinguen normalmente la caja y desactivan Solo palabras completas. Los límites los define el patrón.

Párrafo simple vacío:

```regex
<p>[ \t]*</p>
```

Encuentra `<p></p>` y `<p>   </p>` en una línea física, no `<p>Текст</p>`. No elimine automáticamente ni cambie por `<empty-line/>`: sus funciones estructurales pueden ser diferentes.

Pegue solo el patrón, sin `/.../g`, comillas C++ ni duplicación de barras JSON.

## 3. Motor: Scintilla, no PCRE2

FBE usa Scintilla con `SCFIND_REGEXP` y `SCFIND_CXX11REGEX`. La versión considerada implementa en C++ la gramática ECMAScript, no PCRE2 ni todo el JavaScript moderno. [S1, S2]

También existe un modo básico antiguo con otras reglas. No mezcle sus grupos con paréntesis escapados con C++11: `(слово)` captura y `\(слово\)` busca paréntesis literales.

Unicode no está prohibido, pero no hay UCP ni propiedades PCRE2. Las clases dependen de la biblioteca estándar; `\w` no significa todas las letras de todos los idiomas. Use `[A-Za-z0-9_]` para ASCII previsible. `[А-Яа-яЁё]` cubre ruso, no todo cirílico.

### 3.1. Diferencias principales frente a Diseño

| Propiedad | Diseño | Código |
| --- | --- | --- |
| Sujeto | Representación textual del libro | XML |
| Motor | PCRE2-16 | Scintilla C++11 |
| UCP y propiedades Unicode | Disponibles en PCRE2 | No admitidas como sintaxis PCRE2 |
| Lookbehind | Con restricciones PCRE2 | No admitido |
| Primera captura en reemplazo | `$1` o `\1` | `\1` |
| Formato en reemplazo | Comandos FBE | Solo texto XML |
| Búsqueda entre líneas físicas | Depende del sujeto; reemplazo entre párrafos limitado | No en el camino actual |

## 4. Búsqueda línea por línea: limitación esencial

`MatchOnLines` aplica el patrón por separado al contenido de cada línea física. Una línea puede ser enorme; ajustarla al ancho de ventana no crea otra. [S2]

Por ejemplo:

```xml
<empty-line/> <empty-line/>
```

y:

```xml
<empty-line/>
<empty-line/>
```

Pueden representar la misma vecindad XML, pero la búsqueda solo puede tomar el primer par en una coincidencia; el segundo cruza una frontera.

Añadir `\r\n`, `\n`, `\x0D`, `\x0A`, `\s*` o una clase de todo carácter no evita la restricción. El motor no recibe ambas líneas juntas. Requiere cambiar la implementación, no solo el patrón.

Un reemplazo sí puede insertar un salto. Es otra operación, ilustrada en K28. Distinga buscar entre líneas de insertar una frontera después de una coincidencia.

### 4.1. Trabajar con XML multilínea

Si un atributo está en una línea propia, búsquelo sin toda la etiqueta. Relaciones entre varias líneas requieren herramientas de estructura, validador, script o analizador XML.

No elimine todos los saltos para satisfacer una regex: puede cambiar texto, comentarios, CDATA y legibilidad. Decida primero si necesita realmente coincidencia multilínea.

## 5. Caracteres y escape

Un carácter normal coincide consigo mismo. Para un metacarácter literal, use barra inversa.

| Texto | Expresión |
| --- | --- |
| Punto | `\.` |
| Más | `\+` |
| Interrogación | `\?` |
| Asterisco | `\*` |
| Paréntesis | `\(` o `\)` |
| Número entre corchetes | `\[[0-9]+\]` |
| Barra inversa | `\\` |

`.` es un carácter disponible de la línea, no punto literal. En UTF-8 intervienen adaptador Scintilla y biblioteca; no mida emoji suponiendo un punto por símbolo visible.

`\r` y `\n` denotan CR/LF, pero no permiten atravesar líneas en esta búsqueda. Para espacios y tabulaciones use `[ \t]`.

`\Q...\E` de PCRE2 no es un escape literal portable aquí. Escape cada metacarácter por separado.

## 6. Clases, rangos y límites de palabra

`[abc]` encuentra un carácter del conjunto, `[a-z]` de un rango, `[^abc]` fuera. Puede seguir una repetición: `[0-9]+`.

| Clase | Sentido práctico |
| --- | --- |
| `[0-9]` | Cifra ASCII |
| `[A-Za-z]` | Letra latina ASCII |
| `[ \t]` | Espacio normal o tabulación |
| `[^<]` | Carácter disponible salvo < |
| `[^"']` | Carácter que no sea ninguna comilla recta |
| `\d`, `\D` | Clase de cifras de la implementación y negación |
| `\s`, `\S` | Espaciado y negación |
| `\w`, `\W` | Clase de palabra y negación |

Las clases negativas no entienden XML. `[^<]*` sirve para texto simple entre etiquetas, no análisis completo de entidades, comentarios y CDATA.

`\b` es frontera de palabra, `\B` su contrario. En atributos puede ser insuficiente: `id` puede seguir a dos puntos de otro nombre. Compruebe separadores válidos: inicio de línea, espacio o tabulación.

## 7. Anclas y repetición

`^` y `$` indican fronteras físicas de línea en este camino. Para una línea solo de blancos:

```regex
^[ \t]+$
```

Borrar el resultado deja la línea vacía: su terminación no formaba parte del rango.

| Cuantificador | Repeticiones |
| --- | --- |
| `?` | Cero o una |
| `*` | Cero o más |
| `+` | Una o más |
| `{3}` | Exactamente tres |
| `{3,}` | Al menos tres |
| `{2,5}` | Dos a cinco |

C++11 admite variantes no codiciosas `*?`, `+?`. Para atributos suele ser más claro excluir la comilla delimitadora:

```regex
"[^"\r\n]*"
```

Es para comillas dobles; las simples requieren otra rama.

No copie cuantificadores posesivos, grupos atómicos ni opciones integradas de PCRE2.

## 8. Grupos, alternativas y lookahead

`( ... )` captura un fragmento; `(?: ... )` agrupa sin número.

Varias etiquetas:

```regex
<(?:strong|emphasis)>
```

Una referencia numérica enlaza nombres de apertura y cierre en un fragmento sencillo de una línea:

```regex
<(strong|emphasis)>[ \t]*</\1>
```

Exige el mismo nombre, pero no valida anidación XML arbitraria.

Lookahead positivo `(?=...)` y negativo `(?!...)` comprueban la derecha sin consumirla. Un límite tras el nombre evita que `<a` coincida al inicio de `<author>`.

```regex
<a(?=[ \t>])
```

Para contexto izquierdo, capture y restaure en el reemplazo; no hay lookbehind. No copie `(?<=...)` desde Diseño.

## 9. Reemplazo en Código

Con `SCI_REPLACETARGETRE`, Scintilla expande la cadena después de encontrar la coincidencia. No es `std::regex_replace` con `$1` ni la gramática Diseño. [S1, S2]

### 9.1. Referencias a capturas

| Reemplazo | Significado |
| --- | --- |
| `\0` | Coincidencia completa |
| `\1` … `\9` | Grupos correspondientes |
| `$1` | No usar como referencia: no es la sintaxis de este camino |

Buscar:

```regex
\(([0-9]+)\)
```

Reemplazar por:

```text
[\1]
```

`(12)` se convierte en `[12]`. Solo tiene sentido editorial en contexto seleccionado: no todo número entre paréntesis es una nota.

Un dólar literal no debe duplicarse por convenciones de otro sistema. La barra inversa sí es un carácter de control.

### 9.2. Caracteres de control en reemplazo

El código Scintilla revisado procesa `\t`, `\n`, `\r`, `\\` como tabulación, LF, CR y barra inversa. No contradice la búsqueda monolínea. [S2]

Use `\r\n` para CRLF y `\n` para LF según el documento. Evite terminaciones mezcladas involuntariamente.

`\x{00A0}` no inserta aquí Unicode: use NBSP real. Las órdenes Diseño `\U`, `\L`, `\T`, `\S`, `\E`, `\Q` no controlan caja ni formato.

### 9.3. Borrar y conservar XML

Un reemplazo vacío elimina el rango. Borrar etiqueta, atributo o enlace vacío puede dañar el documento pese a encontrarlo correctamente. Las recetas estructurales son sobre todo diagnósticas.

Renombrar un `id` solo en un sitio puede dejar referencias antiguas. Use el comando de renombrado FBE para objetos vinculados, no sustituciones textuales independientes.

## 10. Cómo interpretar las recetas XML

XML distingue caja: `<p>` y `<P>` no son lo mismo. El orden de atributos no define el significado; valores pueden usar comillas simples o dobles y espacios alrededor de igual. Las recetas cubren variantes habituales sin complicación excesiva. [S3]

`l:` y `xlink:` son prefijos habituales de XLink en FB2. El significado lo establece `xmlns`, no el prefijo por sí solo. Otros prefijos requieren adaptar la regla y revisar la declaración.

Muchos ejemplos aproximan la etiqueta con `[^>]*`. Es práctico, pero `>` puede estar legalmente dentro de atributos. Comentarios y CDATA pueden imitar marcado. Por tanto, son candidatos de revisión, no validación completa ni reestructuración ciega.

Un elemento vacío no siempre viola el esquema; una entidad numérica no es un carácter corrupto. Nombres extraños, placeholders y enlaces externos pueden tener una finalidad legítima.

## 11. Recetas prácticas FB2/XML

Salvo indicación contraria, los componentes deben estar en una línea física. Si se busca la etiqueta completa, todos los atributos examinados deben compartir esa línea. Buscar solo un atributo no exige que toda la etiqueta sea monolínea.

### K01. Espacios al final de una línea física

Encontrar espacios normales y tabulaciones finales.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[ \t]+$
```

Texto de prueba original:

```text
<p>Текст</p>
```

El mismo texto de prueba con los espacios visibles:

```text
<p>Текст</p>␠␠␠
```

Aquí ␠ representa un espacio normal; [TAB], una tabulación; [NBSP], U+00A0; [NNBSP], U+202F; y [ZWSP], U+200B. Es una representación explicativa: no introduzca estas etiquetas en el libro.

Reemplazar por: dejar el campo completamente vacío. No escribir la palabra «vacío».

Resultado del reemplazo:

```text
<p>Текст</p>
```

Contraejemplo que no debe coincidir:

```text
<p>Текст</p>
```


Limitaciones y observaciones: No elimina terminación. En contenido mixto, CDATA y zonas sensibles a blancos, pueden ser texto. Limpiar todas las líneas no es incondicionalmente seguro.

### K02. Vaciar una línea de blancos

Quitar blancos de una línea sin más contenido.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
^[ \t]+$
```

Texto de prueba original:

```text

<p>Text</p>
```

El mismo texto de prueba con los espacios visibles:

```text
␠␠␠[TAB]
<p>Text</p>
```

Aquí ␠ representa un espacio normal; [TAB], una tabulación; [NBSP], U+00A0; [NNBSP], U+202F; y [ZWSP], U+200B. Es una representación explicativa: no introduzca estas etiquetas en el libro.

Reemplazar por: dejar el campo completamente vacío. No escribir la palabra «vacío».

Resultado del reemplazo:

```text

<p>Text</p>
```

Contraejemplo que no debe coincidir:

```text
<p>Text</p>
```


Limitaciones y observaciones: Permanece vacía: LF/CRLF no pertenecían a la coincidencia. No elimina todas las líneas físicas vacías.

### K03. Espacio antes del cierre vacío

Encontrar hueco normal inmediatamente antes de />.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[ \t]+/>
```

Texto de prueba original:

```text
<empty-line />
```

Reemplazar por:

```text
/>
```

Resultado del reemplazo:

```text
<empty-line/>
```

Contraejemplo que no debe coincidir:

```text
<empty-line/>
```


Limitaciones y observaciones: XML admite ambas formas. Es cosmético, no corrección obligatoria. La secuencia también puede aparecer en comentarios o texto; revisar reemplazo global.

### K04. Párrafo simple vacío

Examinar pareja p sin contenido textual.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
<p>[ \t]*</p>
```

Texto de prueba original:

```text
<p>  </p><p>Text</p>
```

Coincidencia esperada:

```text
<p>  </p>
```

Contraejemplo que no debe coincidir:

```text
<p>Text</p>
```


Limitaciones y observaciones: No cubre atributos, saltos, entidad NBSP o etiqueta anidada. Un p vacío y empty-line no son automáticamente equivalentes.

### K05. Dos empty-line en una línea

Encontrar elementos FB2 de línea vacía contiguos en una línea fuente física.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>
```

Texto de prueba original:

```text
<empty-line/> <empty-line />
```

Coincidencia esperada:

```text
<empty-line/> <empty-line />
```

Contraejemplo que no debe coincidir:

```text
<empty-line/>
<empty-line/>
```


Limitaciones y observaciones: Solo espacios normales y tabulaciones entre ellos. Excluye deliberadamente saltos. Dos líneas vacías pueden separar escenas.

### K06. Metadatos importantes vacíos

Examinar varios campos sencillos sin contenido.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
<(book-title|first-name|middle-name|last-name|genre|lang)>[ \t]*</\1>
```

Texto de prueba original:

```text
<book-title> </book-title>
```

Coincidencia esperada:

```text
<book-title> </book-title>
```

Contraejemplo que no debe coincidir:

```text
<book-title>Book</book-title>
```


Limitaciones y observaciones: No todos son obligatorios siempre. Regex no valida esquema, padre ni exactitud de valores llenos.

### K07. Formato inline vacío

Encontrar elementos de formato emparejados y vacíos.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
<(strong|emphasis|strikethrough)>[ \t]*</\1>
```

Texto de prueba original:

```text
<strong> </strong>
```

Coincidencia esperada:

```text
<strong> </strong>
```

Contraejemplo que no debe coincidir:

```text
<strong>Text</strong>
```


Limitaciones y observaciones: NBSP, atributos y anidación necesitan otra condición. Verifique el papel estructural antes de borrar.

### K08. Formato idéntico anidado en sí mismo

Distinguir strong/strong de strong/emphasis válido.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
<(strong|emphasis)>[ \t]*<\1(?=[ \t>])
```

Texto de prueba original:

```text
<strong><strong>Text</strong></strong>
```

Coincidencia esperada:

```text
<strong><strong
```

Contraejemplo que no debe coincidir:

```text
<strong><emphasis>Text</emphasis></strong>
```


Limitaciones y observaciones: El segundo nombre referencia al primero, no se elige independientemente. El resultado no incluye el elemento completo ni se destina a borrarlo entero.

### K09. Posibles etiquetas HTML de importación

Encontrar nombres HTML comunes que revisar en FB2.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
</?(?:b|i|br|div|span|font)(?=[ \t/>])[^>]*>
```

Texto de prueba original:

```text
<div>Text</div>
```

Coincidencias esperadas, en orden:

```text
<div>
</div>
```

Contraejemplo que no debe coincidir:

```text
<section><p>Text</p></section>
```


Limitaciones y observaciones: Comentarios y CDATA pueden coincidir. No convierta b en strong e i en emphasis globalmente sin revisar contenido y atributos.

### K10. Etiqueta en mayúsculas

Encontrar nombres corrientes totalmente en mayúsculas.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
</?[A-Z][A-Z0-9_.:-]*(?=[ \t/>])[^>]*>
```

Texto de prueba original:

```text
<P>Text</P>
```

Coincidencias esperadas, en orden:

```text
<P>
</P>
```

Contraejemplo que no debe coincidir:

```text
<p>Text</p>
```


Limitaciones y observaciones: Debe distinguir la caja, o coincidirá p normal. No enumera todos los nombres XML Unicode válidos ni comprueba espacios de nombres.

### K11. Entidad HTML NBSP

Encontrar &nbsp; literal en la fuente.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
&nbsp;
```

Texto de prueba original:

```text
<p>&nbsp;</p>
```

Coincidencia esperada:

```text
&nbsp;
```

Contraejemplo que no debe coincidir:

```text
<p>&#160;</p>
```


Limitaciones y observaciones: Sin declaración no es una de las cinco entidades XML predefinidas. Revise DTD y comentarios. No decodifique todas las entidades globalmente.

### K12. Referencias numéricas de caracteres

Encontrar notación decimal o hexadecimal.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
&#(?:[0-9]+|x[0-9A-Fa-f]+);
```

Texto de prueba original:

```text
<p>&#160; &#xA0;</p>
```

Coincidencias esperadas, en orden:

```text
&#160;
&#xA0;
```

Contraejemplo que no debe coincidir:

```text
<p>&amp;</p>
```


Limitaciones y observaciones: Ambas pueden ser correctas. Solo se comprueba forma, no código válido. Decodificar &lt; como < literal puede dañar XML.

### K13. id vacío

Encontrar un id ordinario vacío con ambos tipos de comillas.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:""|'')
```

Texto de prueba original:

```text
<section id="">
```

Coincidencia esperada:

```text
 id=""
```

Contraejemplo que no debe coincidir:

```text
<section id="s1">
```


Limitaciones y observaciones: El espacio anterior forma parte del resultado. No incluye xml:id. Crear IDs exige unicidad y referencias, no comprobadas por regex.

### K14. Enumerar valores id

Encontrar ids ordinarios llenos para revisar nombres.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:"[^"]+"|'[^']+')
```

Texto de prueba original:

```text
<section id="s1">
```

Coincidencia esperada:

```text
 id="s1"
```

Contraejemplo que no debe coincidir:

```text
<section id="">
```


Limitaciones y observaciones: Una lista no demuestra unicidad o validez. Use renombrado FBE en objetos enlazados.

### K15. Enlaces externos HTTP(S)

Encontrar href con prefijo XLink habitual y dirección externa.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"https?://[^"]+"|'https?://[^']+')
```

Texto de prueba original:

```text
<a l:href="https://example.test/book">Text</a>
```

Coincidencia esperada:

```text
 l:href="https://example.test/book"
```

Contraejemplo que no debe coincidir:

```text
<a l:href="#note1">1</a>
```


Limitaciones y observaciones: Busca el atributo, no toda URL. Cubre l y xlink; otros prefijos por separado. No verifica disponibilidad de la dirección.

### K16. Enlaces locales file://

Encontrar un archivo local que quizá no funcione para el lector.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"file://[^"]+"|'file://[^']+')
```

Texto de prueba original:

```text
<a xlink:href="file:///C:/Book/image.png">Text</a>
```

Coincidencia esperada:

```text
 xlink:href="file:///C:/Book/image.png"
```

Contraejemplo que no debe coincidir:

```text
<a xlink:href="#image1">Text</a>
```


Limitaciones y observaciones: No abra direcciones desconocidas automáticamente. No decide si integrar archivo, cambiar enlace o borrarlo.

### K17. Destino vacío

Encontrar una referencia XLink vacía.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:""|'')
```

Texto de prueba original:

```text
<a l:href="">Text</a>
```

Coincidencia esperada:

```text
 l:href=""
```

Contraejemplo que no debe coincidir:

```text
<a l:href="#note1">Text</a>
```


Limitaciones y observaciones: Texto de enlace vacío y href ausente son otros casos separados.

### K18. Destino #undefined

Encontrar destino placeholder literal.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')
```

Texto de prueba original:

```text
<a xlink:href="#undefined">Text</a>
```

Coincidencia esperada:

```text
 xlink:href="#undefined"
```

Contraejemplo que no debe coincidir:

```text
<a xlink:href="#note1">Text</a>
```


Limitaciones y observaciones: Compruebe el objeto real. undefined no es ilegal como nombre XML; suele indicar vinculación incompleta.

### K19. Enlaces con prefijo bookmark

Encontrar destinos bookmark de convertidores, no cualquier enlace interno.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#bookmark[^"]*"|'#bookmark[^']*')
```

Texto de prueba original:

```text
<a l:href="#bookmark12">Text</a>
```

Coincidencia esperada:

```text
l:href="#bookmark12"
```

Contraejemplo que no debe coincidir:

```text
<a l:href="#note1">Text</a>
```


Limitaciones y observaciones: href="#..." encuentra todos los internos y no aísla bookmark. Un bookmark no prueba inutilidad; revisar antes de borrar.

### K20. Retornos Word/FBD

Encontrar _ftnref y _ednref en destinos.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#_(?:ftnref|ednref)[^"]*"|'#_(?:ftnref|ednref)[^']*')
```

Texto de prueba original:

```text
<a l:href="#_ftnref1">Back</a>
```

Coincidencia esperada:

```text
l:href="#_ftnref1"
```

Contraejemplo que no debe coincidir:

```text
<a l:href="#note1">Text</a>
```


Limitaciones y observaciones: Un retorno puede ser necesario para navegación. No borrarlo solo por origen del nombre.

### K21. Notas sin depender del orden de atributos

Encontrar apertura a con type=note y href XLink interno en cualquier orden.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
<a(?=[ \t>])(?=[^>]*[ \t]type[ \t]*=[ \t]*(?:"note"|'note'))(?=[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+'))[^>]*>
```

Texto de prueba original:

```text
<a l:href="#note1" type="note">1</a>
```

Coincidencia esperada:

```text
<a l:href="#note1" type="note">
```

Contraejemplo que no debe coincidir:

```text
<a type="link" l:href="#note1">1</a>
```


Limitaciones y observaciones: También permite xlink y comillas simples. Los atributos examinados deben estar en una línea. Valores con >, comentarios y XML especial requieren parser. No verifica existencia de nota destino.

### K22. Posibles marcadores numéricos de nota

Encontrar cifras entre corchetes, llaves o paréntesis.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
\[[0-9]+\]|\{[0-9]+\}|\([0-9]+\)
```

Texto de prueba original:

```text
<p>Text [12], {3}, (4).</p>
```

Coincidencias esperadas, en orden:

```text
[12]
{3}
(4)
```

Contraejemplo que no debe coincidir:

```text
<p>[note]</p>
```


Limitaciones y observaciones: Pueden ser bibliografía, fórmulas, aclaraciones o texto. No crea notas ni comprueba unicidad.

### K23. Your/Name olvidados

Comprobar placeholders ingleses habituales de nombres.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
<first-name>Your</first-name>|<last-name>Name</last-name>
```

Texto de prueba original:

```text
<first-name>Your</first-name>
```

Coincidencia esperada:

```text
<first-name>Your</first-name>
```

Contraejemplo que no debe coincidir:

```text
<first-name>John</first-name>
```


Limitaciones y observaciones: Revise autor/creador y nombre real. No sustituya automáticamente con datos de otra edición.

### K24. Ilustración no vinculada #undefined

Encontrar image dirigido al placeholder común.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
<image(?=[ \t/>])[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')[^>]*>
```

Texto de prueba original:

```text
<image l:href="#undefined"/>
```

Coincidencia esperada:

```text
<image l:href="#undefined"/>
```

Contraejemplo que no debe coincidir:

```text
<image l:href="#cover"/>
```


Limitaciones y observaciones: FB2 suele usar XLink-href, no HTML-src. Verifique binary por separado. Imagen ausente y referencia errónea son causas distintas.

### K25. Declaración windows-1251

Encontrar codificación antigua en declaración XML.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
<\?xml[ \t]+[^?]*encoding[ \t]*=[ \t]*(?:"windows-1251"|'windows-1251')[^?]*\?>
```

Texto de prueba original:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Coincidencia esperada:

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Contraejemplo que no debe coincidir:

```text
<?xml version="1.0" encoding="utf-8"?>
```


Limitaciones y observaciones: No es error en sí. Cambiar windows-1251 por utf-8 no recodifica bytes. Use guardado/conversión consistente con declaración.

### K26. Ampersand sin entidad estándar reconocida

Encontrar & ante una secuencia no parecida a una referencia estándar.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
&(?!(?:amp|lt|gt|apos|quot);|#[0-9]+;|#x[0-9A-Fa-f]+;)
```

Texto de prueba original:

```text
<p>A & B</p>
```

Coincidencia esperada:

```text
&
```

Contraejemplo que no debe coincidir:

```text
<p>A &amp; B</p>
```


Limitaciones y observaciones: & se permite en CDATA/comentarios y DTD puede definir más entidades. Es filtro, no validador. & → &amp; sin contexto puede duplicar escape.

### K27. Enlaces internos ordinarios

Encontrar destino #id local aparte de bookmark.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+')
```

Texto de prueba original:

```text
<a l:href="#note1">1</a>
```

Coincidencia esperada:

```text
l:href="#note1"
```

Contraejemplo que no debe coincidir:

```text
<a l:href="https://example.test">Text</a>
```


Limitaciones y observaciones: Muestra la notación, no prueba un único destino existente. Referencias rotas o duplicadas necesitan análisis global.

### K28. Separar dos empty-line tras búsqueda monolínea

Demostrar diferencia entre límite de búsqueda e inserción de salto al reemplazar.

Acción: realizar un único reemplazo controlado.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(<empty-line[ \t]*/>)[ \t]*(<empty-line[ \t]*/>)
```

Texto de prueba original:

```text
<empty-line/> <empty-line/>
```

Reemplazar por:

```text
\1\r\n\2
```

Resultado del reemplazo:

```text
<empty-line/>
<empty-line/>
```

Contraejemplo que no debe coincidir:

```text
<empty-line/>
<empty-line/>
```


Limitaciones y observaciones: Scintilla convierte las secuencias en CRLF. Para documento LF use \1\n\2. No es formateador XML universal ni activa búsqueda multilínea.

### K29. Espacios dobles en texto XML sencillo

Encontrar fragmento entre etiquetas con varios espacios normales.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
>[^<]*[ \t]{2,}[^<]*<
```

Texto de prueba original:

```text
<p>one  two</p>
```

Coincidencia esperada:

```text
>one  two<
```

Contraejemplo que no debe coincidir:

```text
<p>one two</p>
```


Limitaciones y observaciones: Incluye delimitadores angulares y todo el fragmento. Las palabras se corrigen más cómodamente en Diseño. No reemplace el resultado completo por un espacio.

## 12. Por qué regex no sustituye la validación XML

Una expresión prueba semejanza local de caracteres, no corrección de todo el documento. Anidación, esquema FB2, declaraciones de espacios de nombres, IDs únicos y destinos de enlaces exigen controles separados.

Dos `id` iguales en líneas distintas no se detectan fiablemente mediante una comparación del camino monolínea. Igual ocurre con un destino ausente en otra parte del libro. Use validación y estructura FBE.

No decodifique todas las entidades: `&lt;`, `&amp;` suelen evitar que el texto se interprete como marcado. No borre todo lo parecido a HTML dentro de CDATA, comentarios o citas de código.

La operación debe preservar parejas, atributos y contenido. Cambiar solo `<strong>` mantiene el cierre viejo. Una referencia en un ejemplo no vuelve segura cualquier modificación posterior.

## 13. Errores comunes

### 13.1. Se encuentran minúsculas en vez de mayúsculas

Active la distinción de caja. En XML no es cosmética. `[A-Z]` insensible anula la comprobación buscada.

### 13.2. Aparece $1 literalmente

Use `\1`. El reemplazo Fuente pasa por Scintilla, no JavaScript ni un API con `$1`.

### 13.3. Fallan \p, \K o lookbehind

Pertenecen a otro perfil. Para texto use Diseño; para XML, reescriba con clases explícitas, contexto capturado, grupos no capturantes y lookahead.

### 13.4. No aparecen etiquetas vecinas

Revise salto físico, espacio ante `/>`, comillas simples, atributos y prefijos. Vecindad textual y del árbol XML son distintas.

### 13.5. La nota no aparece cuando se reordenan atributos

«type luego href» depende del orden. K21 usa lookaheads independientes. Una etiqueta multilínea aún no puede encontrarse entera; busque el atributo o la estructura.

### 13.6. strong/emphasis se marca como anidación errónea

Estilos válidos distintos pueden anidarse. Para el mismo nombre se necesita referencia como K08, no alternativas independientes.

### 13.7. «Bookmark» encuentra todas las notas

Un interno `#` no es automáticamente bookmark residual. Separe auditoría general K27 y prefijo específico K19.

### 13.8. Se daña texto visible

En Código se puede coincidir en texto, atributos, comentarios o binary. Deshaga y acote patrón o ámbito. El nombre «reemplazo seguro» no elimina el contexto XML.

## 14. Limitaciones del perfil Source

No use UCP, propiedades PCRE2, lookbehind, grupos nombrados de JavaScript moderno/PCRE2, grupos atómicos, posesivos, branch reset, subrutinas, verbos PCRE2, `\K`, `\G` ni comandos de formato Diseño.

Las opciones PCRE2 internas no sustituyen las casillas. Funciones recientes de navegadores no aparecen automáticamente en C++11 ECMAScript.

MatchOnLines mantiene la búsqueda por líneas, sea cual sea la terminación escrita en el patrón. Compilar en otro motor no demuestra compatibilidad FBE.

Para palabras Unicode y corrección compleja use Diseño; para estructura, XML/FB2, no un `.*` ilimitado como compensación.

## 15. Rendimiento y líneas XML largas

Una expresión corta no siempre es rápida. Repeticiones ilimitadas y alternativas pueden ralentizar líneas grandes, especialmente todo el documento o binary en una sola línea.

Empiece con nombre concreto de etiqueta/atributo y clase limitada. Diagnósticos separados son más fáciles de revisar y deshacer que buscar todos los fallos a la vez.

No aplane XML para evitar MatchOnLines: cambia el documento sin garantizar velocidad.

## 16. Comprobaciones posteriores

Compare rango previsto y real. Con capturas, revise comillas, prefijos, ángulos y cierres. Evite afectar comentarios, CDATA y binary.

Valide FB2, guarde y reabra después de cambios importantes. Compruebe Código ↔ Diseño, notas, imágenes y texto. Un ID renombrado exige revisar objeto y referencias.

## 17. Fuentes y alcance

Traducción del manual ampliado basado en regex-source.md. Conserva motor, sintaxis, reemplazo, ejemplos XML y límites. Se precisaron recetas demasiado amplias y se redactaron explicaciones y pruebas apoyadas en fuentes primarias.

[S1] Documentación Scintilla, Searching: C++11, opciones y SCI_REPLACETARGETRE.

`https://www.scintilla.org/ScintillaDoc.html#Searching`

[S2] Scintilla en FBE d2257405d95b0328649acee64b38829b40a4314b: Document.cxx, Cxx11RegexFindText, MatchOnLines y BuiltinRegex::SubstituteByPosition. Distingue búsqueda entre líneas de inserción CR/LF.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/third_party/scintilla/src/Document.cxx`

[S3] W3C XML 1.0 y Namespaces in XML: nombres, atributos, entidades y espacios de nombres.

`https://www.w3.org/TR/xml/`

`https://www.w3.org/TR/xml-names/`

[S4] SearchPresetCatalog.cpp, mainfrm.cpp, FBEview.cpp y search-preset-source-scintilla-smoke.cpp de la misma revisión definen el perfil y sus tareas editoriales.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/tools/tests/search-preset-source-scintilla-smoke.cpp`

Las recetas adicionales deben comprobarse en la compilación FBE objetivo. Las pruebas locales C++11 ECMAScript no equivalen a ejecutar Windows Scintilla. El README detalla su alcance.
