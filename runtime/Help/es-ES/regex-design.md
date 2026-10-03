# Ayuda de expresiones regulares — Diseño

Nota sobre la traducción: las palabras y frases rusas de los ejemplos de control se conservan intencionadamente. Son datos de prueba: las expresiones regulares, las cadenas de sustitución, los espacios y los resultados esperados coinciden con la edición rusa de referencia. Se han traducido las explicaciones; los ejemplos no se adaptan automáticamente a la tipografía española.

Guía completa para buscar, reemplazar y corregir libros en FictionBook Editor Next.

Edición: 2 de octubre de 2026. El modo Código se explica en regex-source.md. «Patrón» significa aquí una expresión regular; una «plantilla integrada» es una expresión guardada junto con sus opciones en el panel Plantillas.

Nota de traducción: las expresiones, los reemplazos y los ejemplos de prueba se conservan sin cambios respecto a la edición rusa comprobada. Las palabras rusas en bloques de código son intencionadas, especialmente para comprobar cirílico, mayúsculas y alfabetos mezclados. No traduzca esas muestras al comparar resultados.

## 1. Cómo utilizar esta guía

Una expresión regular describe una regla de búsqueda, no una sola cadena exacta. Por ejemplo, `[0-9]+` encuentra una secuencia de cifras de longitud variable, mientras que `[ \t]{2,}` encuentra dos o más espacios normales o tabulaciones. La coincidencia y el texto que la reemplaza son elementos distintos.

La sección 2 proporciona un primer resultado práctico. Las secciones 3–15 explican la sintaxis; la 16, la gramática especial de reemplazo de FBE. La 17 reúne recetas para libros con opciones, textos de prueba y advertencias. Al final figuran diagnóstico, rendimiento y fuentes.

Copie únicamente la expresión en Buscar. Las palabras «Buscar» y «Reemplazar», las etiquetas como U+0020 y los acentos graves de Markdown no forman parte del patrón. No añada delimitadores JavaScript `/.../g`, comillas de una cadena C++ ni barras inversas duplicadas de JSON.

Las casillas importan. Una coincidencia sin distinción de mayúsculas puede desaparecer al activarla. Los ajustes forman parte de la receta; no son una presentación opcional.

«Solo buscar y revisar» identifica un diagnóstico. El resultado puede ser correcto: una repetición deliberada, un nombre extranjero, una cita, un título o tipografía intencionada. «Reemplazar después de revisar» tampoco garantiza seguridad para cualquier libro.

## 2. Primera búsqueda y reemplazo prudente

Guarde una copia de trabajo. Cambie a Diseño, abra Buscar o Reemplazar y active Expresión regular. En los primeros ensayos desactive Solo palabras completas: es preferible definir los límites en el propio patrón. Compruebe ámbito y dirección.

Para espacios repetidos:

```regex
[ \t]{2,}
```

En `Он   пришёл` encuentra el intervalo de tres espacios. Introduzca un espacio normal como reemplazo. Cambie una sola coincidencia, compruebe `Он пришёл` y solo entonces valore Reemplazar todo.

Con muchos resultados, inspecciónelos primero y corrija uno o dos casos representativos. Después de una operación masiva, revise texto, cursiva, negrita, notas y límites de párrafos. Deshaga un resultado inesperado antes de iniciar otra serie de cambios.

Aplicar en Plantillas transfiere expresión y opciones al diálogo. No significa corregir automáticamente todos los resultados. La búsqueda o el reemplazo se ejecutan con sus comandos correspondientes.

## 3. Motor y límites del modo Diseño

Se utiliza PCRE2-16 con texto y patrones en UTF-16; UTF está siempre activado. FBE construye por separado el texto consultable y realiza los reemplazos teniendo en cuenta la estructura del libro. La documentación PCRE2 explica las coincidencias, pero no todas las operaciones de FBE. Véanse D1–D4.

Se busca en una representación textual del libro, no en el XML literal. `<strong>` no permite buscar negritas en Diseño. Para etiquetas use Código; para transformaciones estructurales, funciones del editor o scripts especializados.

El ajuste visual de una línea al ancho de la ventana no es un salto de línea. Distinga párrafos, saltos reales y ajuste visual. FBE emplea anclas multilínea en su representación para reflejar límites de párrafos. Encontrar texto entre párrafos y reemplazarlo son operaciones distintas: la implementación considerada rechaza reemplazos entre párrafos.

`\A` y `\z` se refieren al principio y final del sujeto entregado al motor. No equivalen automáticamente al principio y final de todo el FB2: influyen el ámbito y el fragmento creado por FBE.

## 4. Unicode: UTF, UCP y mayúsculas

### 4.1. Qué hace UTF

UTF permite procesar Unicode, incluido el cirílico. No translitera, no corrige OCR, no convierte `ё` en `е` ni unifica automáticamente representaciones canónicamente equivalentes.

Una letra acentuada puede almacenarse como un carácter o como letra seguida de marca combinante. Aunque se parezcan, la búsqueda carácter a carácter puede diferir. PCRE2 no normaliza NFC/NFD por usted. Para diacríticos puede necesitarse `\p{M}`.

### 4.2. Qué hace UCP

Unicode (UCP) cambia las clases abreviadas, especialmente `\w`, `\d`, `\s`, y sus límites dependientes `\b`, `\B`. No es el interruptor que habilita el cirílico en sí.

Aclaración importante respecto a versiones anteriores: las propiedades explícitas `\p{L}`, `\p{N}`, `\P{...}` funcionan en una compilación Unicode de PCRE2 incluso sin UCP. No obstante, un patrón que combine `\p{L}` y `\b` debería normalmente activar UCP para que letras y límites concuerden.

Compare la búsqueda de una palabra rusa:

```regex
\bмир\b
```

Con UCP, los límites siguen la clase Unicode de palabra. Sin UCP, no espere que `\b` se comporte en cirílico igual que en ASCII latino. No obtener un resultado no demuestra que la palabra falte.

Si necesita solo cifras ASCII, use `[0-9]` en vez de `\d`: UCP puede ampliar esta última a cifras decimales de otras escrituras.

### 4.3. Propiedades útiles

| Expresión | Qué encuentra |
| --- | --- |
| `\p{L}` | Letra Unicode |
| `\p{Lu}` | Letra mayúscula en búsqueda sensible a la caja |
| `\p{Ll}` | Letra minúscula en búsqueda sensible a la caja |
| `\p{M}` | Marca combinante |
| `\p{N}` | Carácter numérico, concepto más amplio que cifra decimal |
| `\p{Nd}` | Cifra decimal |
| `\p{Latin}` | Carácter de escritura latina |
| `\p{Cyrillic}` | Carácter de escritura cirílica |
| `\P{L}` | Carácter que no es una letra |

Para letras con acentos combinantes:

```regex
[\p{L}\p{M}]+
```

No es una definición lingüística universal de palabra: no incluye guiones ni apóstrofos. Añádalos deliberadamente.

### 4.4. La caja es una opción distinta

Distinga mayúsculas al buscar anomalías como `строчнаяПрописная`, iniciales o patrones con `\p{Lu}`/`\p{Ll}`. Ignorarlas puede anular el propósito de la regla. UCP no sustituye esta casilla.

Ignorar temporalmente la caja:

```regex
(?i)глава
```

Limitar el efecto a un grupo:

```regex
(?i:глава)[ \t]+[0-9]+
```

## 5. Caracteres literales y escape

Las letras y la mayoría de signos coinciden consigo mismos. Fuera de una clase, punto, paréntesis, corchetes, asterisco, más, interrogación, llaves, anclas y barra inversa pueden tener función especial.

Para un metacarácter literal, anteponga una barra inversa:

| Texto buscado | Expresión |
| --- | --- |
| Punto | `\.` |
| Interrogación | `\?` |
| Signo más | `\+` |
| Asterisco | `\*` |
| Paréntesis de apertura | `\(` |
| Paréntesis de cierre | `\)` |
| Número entre corchetes | `\[[0-9]+\]` |
| Barra inversa literal | `\\` |

`(123)` encuentra los dígitos `123` y los captura; no exige paréntesis en el texto. Para encontrar `(123)` escape los paréntesis.

Un fragmento literal largo puede encerrarse entre `\Q` y `\E` en PCRE2:

```regex
\QЦена (руб.) + доставка\E
```

Las mismas secuencias significan otra cosa en el reemplazo de FBE. No transfiera automáticamente reglas de escape entre campos.

## 6. Espacios, tabulaciones y caracteres invisibles

| Expresión | Significado en búsqueda PCRE2 |
| --- | --- |
| `[ ]` | Solo espacio normal U+0020 |
| `[ \t]` | Espacio normal o tabulación |
| `\t` | Tabulación U+0009 |
| `\x{00A0}` | Espacio de no separación |
| `\x{202F}` | Espacio estrecho de no separación |
| `\h` | Espaciado horizontal, incluidos varios espacios Unicode |
| `\s` | Espaciado que puede incluir saltos de línea |
| `\r` | Retorno de carro CR |
| `\n` | Salto de línea LF |
| `\R` | Secuencia de salto de línea Unicode |
| `\x{00AD}` | Guion discrecional |
| `\x{200B}` | Espacio de anchura cero |
| `\x{FEFF}` | FEFF dentro del texto |

Para limpiar huecos ordinarios entre palabras prefiera `[ \t]` a `\s`. La segunda clase puede incluir párrafos y espacios inseparables. `\h` también sirve para diagnosticar, pero resulta demasiado amplio para un reemplazo tipográfico sin especificar.

Un espacio inseparable une signo y número, iniciales y apellido, o número y unidad. Cambiarlos todos por espacios normales puede empeorar la composición. FBE permite configurar el carácter NBSP; compruebe las recetas U+00A0 con el libro y la versión usada.

U+200C y U+200D pueden ser necesarios en determinadas escrituras y emoji compuestos. No borre indiscriminadamente todos los caracteres «invisibles». Detectar no implica error.

## 7. Clases y rangos

Una clase entre corchetes consume un carácter del conjunto. `[abc]` significa una de tres letras, no la palabra `abc`. Para una o más, añada un cuantificador: `[abc]+`.

`[^abc]` consume un carácter ajeno al conjunto. No comprueba que «abc no preceda al texto». Para contexto utilice aserciones lookaround.

Dentro de una clase, punto y la mayoría de paréntesis pierden su función especial. El guion puede definir un rango; para uno literal, sitúelo al principio o final o escápelo. `\]` permite incluir el corchete de cierre.

```regex
[А-Яа-яЁё-]+
```

Admite letras rusas y guion, no todo el cirílico. Las letras ucranianas, bielorrusas y otras requieren una clase más amplia o una propiedad Unicode.

Dentro de corchetes, `\b` no es un límite de palabra. No ponga límites de palabra en conjuntos ordinarios de caracteres.

## 8. Anclas, límites y coincidencias vacías

Un ancla verifica una posición sin consumir letra ni espacio. `^`, `$` o `\b` por sí solos pueden producir una coincidencia de longitud cero, que no se muestra como selección textual normal.

| Ancla | Significado |
| --- | --- |
| `^` | Inicio de línea con multilínea; en otro caso, inicio del sujeto |
| `$` | Fin de línea con multilínea; la última terminación depende del modo |
| `\A` | Solo inicio del sujeto |
| `\z` | Fin estricto del sujeto |
| `\Z` | Fin del sujeto o posición anterior a su último salto |
| `\b` | Límite entre carácter de palabra y carácter ajeno |
| `\B` | Posición que no es límite de palabra |
| `\G` | Posición inicial de la llamada actual de coincidencia |

`\G` no recuerda por sí solo la coincidencia anterior. Depende del desplazamiento inicial suministrado por la aplicación. En llamadas sucesivas puede ser el final anterior, pero no es un método universal de recorrer un libro en FBE. Use límites explícitos para corrección corriente. [D1]

Para una palabra y no un fragmento de otra más larga, use límites o pruebas de letras vecinas. Con UCP:

```regex
\bтом\b
```

No coincide con el principio de `томик`. Guiones y apóstrofos pueden separar palabras de manera distinta para motor y corrector.

Sea especialmente prudente con coincidencias vacías y Reemplazar todo: se inserta texto en posiciones en lugar de sustituir caracteres visibles. Empiece por coincidencias no vacías.

## 9. Cuantificadores: repetición y retroceso

| Forma | Repeticiones del elemento previo |
| --- | --- |
| `?` | Cero o una |
| `*` | Cero o más |
| `+` | Una o más |
| `{3}` | Exactamente tres |
| `{3,}` | Al menos tres |
| `{2,5}` | Entre dos y cinco |

El cuantificador afecta al carácter, clase o grupo previo. `аб+` repite solo `б`; `(?:аб)+` repite las dos letras.

### 9.1. Búsqueda codiciosa

Entrada:

```text
«первый» и «второй»
```

Patrón:

```regex
«.*»
```

Punto-asterisco toma primero todo lo posible; el motor retrocede si es necesario para satisfacer el resto. Aquí el resultado abarca las dos parejas de comillas.

### 9.2. Búsqueda no codiciosa

```regex
«.*?»
```

Empieza con el mínimo, pero también amplía el resultado cuando hace falta. Las búsquedas sucesivas encuentran `«первый»` y `«второй»`.

Para una pareja sencilla suele ser más claro excluir expresamente el cierre:

```regex
«[^»\r\n]*»
```

No analiza comillas anidadas, que necesitan revisión editorial separada.

### 9.3. Cuantificadores posesivos

```regex
«.*+»
```

No significa «comillas aún más correctas». `.*+` absorbe el cierre y se niega a devolverlo; el `»` restante ya no puede coincidir. No hay resultado en el ejemplo.

Una variante que excluye el cierre puede servir:

```regex
«[^»\r\n]*+»
```

Cuantificadores posesivos y grupos atómicos controlan el retroceso, no aceleran universalmente cualquier patrón.

## 10. Grupos y referencias posteriores

Los paréntesis capturan fragmentos. Se numeran desde 1 por orden de paréntesis capturantes de apertura de izquierda a derecha; anidarlos no cambia ese orden.

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

En `02.10.2026`, los grupos contienen `02`, `10`, `2026`. Se valida la forma, no la fecha del calendario.

Un grupo no capturante `(?:...)` agrupa sin consumir número. Ayuda a mantener `$1` y `$2` previsibles en reemplazos complejos.

Los grupos con nombre mejoran la lectura:

```regex
(?<word>\p{L}+)[ \t]+\k<word>
```

Una referencia busca el mismo texto capturado. Una llamada a subrutina, explicada después, repite una regla y puede encontrar otro texto. Son mecanismos distintos.

Para palabras repetidas en libros, añada límites, opciones de caja y espacios adecuados; abajo hay una receta. Sin límites, la referencia puede coincidir con parte de una palabra mayor.

Los grupos con nombre en búsqueda no habilitan automáticamente reemplazos por nombre en FBE. Use las referencias numéricas confirmadas.

## 11. Alternativas y grupos atómicos

La barra vertical elige entre ramas:

```regex
(?:глава|часть)[ \t]+[0-9]+
```

La agrupación importa. Sin ella, el sufijo común podría afectar solo a la última rama. Las alternativas se prueban en el orden escrito; coloque conscientemente formas largas y cortas.

Un grupo atómico impide volver al interior de un grupo que ya coincidió:

```regex
(?>а|аб)в
```

Con `абв`, la primera rama elige `а`; después no puede volver para elegir `аб`, por lo que falla. La versión no atómica puede encontrarlo. Es un ejemplo didáctico, no una recomendación de añadir atomicidad sin pruebas.

## 12. Comprobar el contexto: lookaround

Las aserciones examinan contexto sin incluirlo en la coincidencia completa. Así se puede seleccionar solo un número y conservar su signo anterior.

```regex
(?<=№ )[0-9]+
```

En `№ 125` encuentra solo `125`. El espacio es exactamente uno normal.

Comprobación a la derecha:

```regex
[0-9]+(?=[ \t]+руб\.)
```

En `125 руб.` coincide el número, no `руб.`.

Aserción negativa hacia delante:

```regex
\bглава\b(?![ \t]+[0-9])
```

Con UCP encuentra «глава» si no le sigue numeración ordinaria tras un espacio. Ilustra selección contextual, no corrección automática.

Lookbehind es `(?<=...)`, su negativo `(?<!...)`. Lookahead es `(?=...)`, su negativo `(?!...)`.

Lookbehind limita la longitud. PCRE2 moderno admite algunas longitudes variables acotadas, no repetición ilimitada arbitraria. Para recetas portables, use contexto corto fijo o captura/`\K`; no dependa de `.*` dentro de lookbehind.

## 13. Opciones dentro del patrón

| Opción | Efecto |
| --- | --- |
| `(?i)` | Ignorar mayúsculas y minúsculas |
| `(?-i)` | Distinguirlas |
| `(?m)` | Anclas de principio/final multilínea |
| `(?-m)` | Desactivar anclas multilínea |
| `(?s)` | Permitir que el punto incluya salto de línea |
| `(?-s)` | Restaurar el comportamiento normal del punto |
| `(?x)` | Ignorar espacios insignificantes y comentarios del patrón |

`(?m)` no hace que el punto cruce líneas. `(?s)` no permite reemplazos entre párrafos en FBE. Son dos opciones del motor y una limitación separada de la aplicación.

En modo extendido, los espacios del patrón pueden dejar de ser literales. Use `[ ]` para un espacio requerido; `#` fuera de una clase puede iniciar comentario. Los patrones multilínea son útiles en documentos, pero las recetas siguientes ocupan una línea para el campo del editor.

## 14. Funciones avanzadas de PCRE2

La mayoría de correcciones no requieren esta sección. Explica construcciones de patrones ajenos. Que el motor admita una función no garantiza que todo patrón correspondiente sea cómodo o seguro sobre el libro entero.

### 14.1. Reiniciar el comienzo de la coincidencia

```regex
№[ \t]*\K[0-9]+
```

En `№ 125` comprueba el prefijo, pero devuelve solo `125`. Sirve para cambiar el número y no el signo. No use `\K` dentro de aserciones sin comprobarlo: PCRE2 restringe esa combinación.

### 14.2. Condición según participación de un grupo

```regex
^(\()?([0-9]+)(?(1)\))$
```

Acepta `12` o `(12)`, no `(12` sin cierre. La condición consulta si participó el grupo 1. No convierta este ejemplo en reemplazo dependiente de grupos opcionales sin comprobar el adaptador de FBE.

### 14.3. Numeración compartida en alternativas

```regex
(?|глава ([0-9]+)|часть ([0-9]+))
```

El número ocupa el grupo 1 en ambas ramas: branch reset. En casos simples, un grupo no capturante con una captura común suele ser más claro.

### 14.4. Llamada a subrutina

```regex
(?<pair>[0-9]{2})-(?&pair)
```

En `12-34` ambas partes cumplen «dos dígitos», aunque sean distintas. `\k<pair>` exigiría repetir `12`.

Las subrutinas recursivas describen algunas estructuras anidadas, pero no reemplazan el analizador XML. Use herramientas estructurales para `<section>`, `<poem>`, notas y tablas.

### 14.5. Omitir fragmentos

```regex
«[^»\r\n]*»(*SKIP)(*FAIL)|\bслово\b
```

Con UCP busca la palabra fuera de pares sencillos de comillas rusas. La primera rama marca el texto citado para omitirlo; la segunda busca. No analiza comillas anidadas o sin cerrar, así que no es infalible en cambios globales.

## 15. Lo que una expresión no puede decidir

«Mayúscula después de minúscula», «sin puntuación final» y «palabra repetida» son señales formales, no decisiones editoriales. Regex ignora si `да да` es una errata, diálogo intencionado, verso o título.

No añada puntos automáticamente, no una todos los párrafos que empiezan en minúscula, no sustituya todas las letras latinas por cirílicas ni todos los guiones por rayas. Las operaciones complejas exigen contexto y, a veces, varios pasos DOM. Los scripts FBE de limpieza, palabras pegadas y notas se destinan a ello.

## 16. Gramática de reemplazo de FBE

PCRE2 busca, pero FBE analiza la cadena de reemplazo. Compruebe antes de reutilizar ejemplos de JavaScript, Python, .NET, PCRE2 substitute u otros editores. Las órdenes siguientes están sustentadas por el código y la guía original. [D3, D4]

### 16.1. Coincidencia y grupos

| En el reemplazo | Significado |
| --- | --- |
| `$0` o `\0` | Coincidencia completa |
| `$1` … `$9` | Grupo del número correspondiente |
| `\1` … `\9` | Otra forma de referencia numérica |
| `$+` o `\+` | Último grupo devuelto por el adaptador |

«FBE solo reemplaza grupos 1–9» necesita matiz: también se insertan texto normal y coincidencia completa. Está limitado el acceso numérico directo, no la cantidad de grupos de PCRE2.

No use `$10` para el grupo 10. `${name}` no pertenece a la gramática confirmada. Mantenga las capturas necesarias entre las nueve primeras y convierta las auxiliares en no capturantes.

Los grupos opcionales y vacíos requieren un ensayo específico: FBE tiene un adaptador SubMatches. En nuevos reemplazos masivos, evite depender de sutilezas de grupos ausentes o del «último grupo»; use capturas obligatorias explícitas.

### 16.2. Reordenar grupos

Buscar:

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Reemplazar por:

```text
$3-$2-$1
```

`02.10.2026` pasa a `2026-10-02`. Es una demostración, no una recomendación de reformatear todas las fechas.

### 16.3. Cambiar mayúsculas y minúsculas

| Orden | Efecto |
| --- | --- |
| `\U` | Convertir el fragmento insertado a mayúsculas |
| `\L` | Convertirlo a minúsculas |
| `\T` | Primera letra mayúscula y resto minúscula |
| `\Q` | Reiniciar órdenes activas de caja/formato para la parte siguiente |

Buscar:

```regex
(иван)
```

Reemplazar por:

```text
\T$1\Q
```

Resultado: `Иван`. `\T` no capitaliza lingüísticamente cada palabra: el fragmento `иван иванов` no tiene por qué convertirse en `Иван Иванов`.

No superponga `\U` y `\L` sin reiniciar. Separe segmentos con `\Q` y revise cirílico y diacríticos. La caja la transforma FBE, no un normalizador Unicode de PCRE2.

### 16.4. Negrita y cursiva

`\S` activa negrita para el fragmento insertado; `\E`, cursiva. `\Q` termina los comandos activos para el texto insertado a continuación.

```text
\S$1\Q
```

Formatea el grupo 1. No busca negrita existente ni elimina toda la maquetación del libro. Pruebe un fragmento y Deshacer.

### 16.5. Significados distintos en búsqueda y reemplazo

En búsqueda, `\S` es un carácter no blanco; en reemplazo FBE, negrita. En búsqueda, `\Q...\E` protege un literal; al reemplazar, `\Q` reinicia órdenes y `\E` activa cursiva.

No introduzca `\n`, `\t`, `\x{00A0}`, `\$` o `$$` en reemplazo Diseño esperando reglas ajenas. No forman parte de la gramática literal descrita; una secuencia desconocida puede descartarse. Para NBSP, use el carácter real; para conservar un signo especial, capture y restitúyalo o use reemplazo ordinario comprobado.

Un campo vacío elimina el resultado; un campo con un espacio reemplaza por un espacio. No escriba literalmente `<пусто>`, `[NBSP]` ni `U+00A0`.

## 17. Recetas prácticas de edición y corrección

Son escenarios independientes; los nombres no tienen por qué coincidir exactamente con el catálogo integrado. Pruebe los reemplazos sobre una ocurrencia. Los espacios y tabulaciones reales permanecen en los bloques de prueba. Los contraejemplos ilustran al menos un límite, no sustituyen la revisión del libro completo.

### D01. Varios espacios normales en uno

Normalizar huecos ordinarios sin tocar una NBSP aislada.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[ \t]{2,}
```

Texto de prueba original:

```text
Он   пришёл.
```

Reemplazar por: un espacio normal U+0020. Es un carácter, no la palabra «espacio».

Resultado del reemplazo:

```text
Он пришёл.
```

Contraejemplo que no debe coincidir:

```text
Он пришёл.
```


Limitaciones y observaciones: Varios espacios pueden ser deliberados en versos, tablas o sangrías simuladas. Revise el ámbito; no es una reestructuración de sangrías de párrafo.

### D02. Espacios al principio del párrafo

Eliminar sangría manual antes de texto corriente.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
^[ \t]+
```

Texto de prueba original:

```text
   Начало абзаца.
```

El mismo texto de prueba con los espacios visibles:

```text
␠␠␠Начало␠абзаца.
```

Aquí ␠ representa un espacio normal; [TAB], una tabulación; [NBSP], U+00A0; [NNBSP], U+202F; y [ZWSP], U+200B. Es una representación explicativa: no introduzca estas etiquetas en el libro.

Reemplazar por: dejar el campo completamente vacío. No escribir la palabra «vacío».

Resultado del reemplazo:

```text
Начало абзаца.
```

Contraejemplo que no debe coincidir:

```text
Начало абзаца.
```


Limitaciones y observaciones: La sangría manual puede ser significativa en poesía o composición artística. El ancla indica una línea textual, no el ajuste visual de ventana.

### D03. Espacios al final del párrafo

Eliminar la cola de espacios normales o tabulaciones.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[ \t]+$
```

Texto de prueba original:

```text
Конец абзаца.
```

El mismo texto de prueba con los espacios visibles:

```text
Конец␠абзаца.␠␠␠
```

Aquí ␠ representa un espacio normal; [TAB], una tabulación; [NBSP], U+00A0; [NNBSP], U+202F; y [ZWSP], U+200B. Es una representación explicativa: no introduzca estas etiquetas en el libro.

Reemplazar por: dejar el campo completamente vacío. No escribir la palabra «vacío».

Resultado del reemplazo:

```text
Конец абзаца.
```

Contraejemplo que no debe coincidir:

```text
Конец абзаца.
```


Limitaciones y observaciones: NBSP queda excluida deliberadamente. Diagnostique los espacios especiales aparte.

### D04. Espacio antes de coma u otra puntuación

Conservar el signo y quitar el hueco precedente.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[ \t]+([,;:!?])
```

Texto de prueba original:

```text
Слово , другое !
```

Reemplazar por:

```text
$1
```

Resultado del reemplazo:

```text
Слово, другое!
```

Contraejemplo que no debe coincidir:

```text
Слово, другое!
```


Limitaciones y observaciones: Pensado para texto ruso ordinario. La tipografía francesa permite espacios especiales antes de ciertos signos; adapte antes de aplicarlo a esos libros.

### D05. Espacio tras un signo de apertura

Eliminar espacios tras un paréntesis, corchete o comilla rusa de apertura.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
([(\[«„])[ \t]+
```

Texto de prueba original:

```text
« слово» ( пример)
```

Reemplazar por:

```text
$1
```

Resultado del reemplazo:

```text
«слово» (пример)
```

Contraejemplo que no debe coincidir:

```text
«слово» (пример)
```


Limitaciones y observaciones: Solo afecta a espacios en fórmulas o ejemplos si siguen inmediatamente a un signo listado. Revise siempre el contexto.

### D06. Espacio ante un signo de cierre

Eliminar espacios normales antes de paréntesis, corchete o comilla de cierre.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[ \t]+([)\]»”])
```

Texto de prueba original:

```text
«слово » (пример )
```

Reemplazar por:

```text
$1
```

Resultado del reemplazo:

```text
«слово» (пример)
```

Contraejemplo que no debe coincidir:

```text
«слово» (пример)
```


Limitaciones y observaciones: No normaliza todas las convenciones de comillas ni de paréntesis matemáticos.

### D07. Tres puntos exactos en puntos suspensivos

Convertir tres puntos consecutivos sin tocar una secuencia mayor.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?<!\.)\.{3}(?!\.)
```

Texto de prueba original:

```text
Он подумал... и ответил.
```

Reemplazar por:

```text
…
```

Resultado del reemplazo:

```text
Он подумал… и ответил.
```

Contraejemplo que no debe coincidir:

```text
Содержание.....12
```


Limitaciones y observaciones: Revise el criterio editorial. Cuatro puntos y puntos de guía del índice se dejan intactos.

### D08. Puntos suspensivos separados

Unir tres puntos separados por espacios normales.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?<!\.)\.[ \t]*\.[ \t]*\.(?!\.)
```

Texto de prueba original:

```text
Он подумал. . . и ответил.
```

Reemplazar por:

```text
…
```

Resultado del reemplazo:

```text
Он подумал… и ответил.
```

Contraejemplo que no debe coincidir:

```text
Слово. Другое.
```


Limitaciones y observaciones: También encuentra tres puntos contiguos. No usar en líneas de guía ni omisiones de citas con convenciones especiales.

### D09. Espacio inseparable después de №

Mantener unido el signo al número siguiente.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
№[ \t]+([0-9]+)
```

Texto de prueba original:

```text
№ 125
```

Reemplazar por:

```text
№ $1
```

Resultado del reemplazo:

```text
№ 125
```

Contraejemplo que no debe coincidir:

```text
№125
```


Limitaciones y observaciones: El reemplazo lleva un U+00A0 real entre № y $1. No sustituirlo por la notación \x{00A0}. No inserta un espacio ausente.

### D10. Espacio inseparable después de §

Unir el signo de sección con el número.

Acción: reemplazar después de revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
§[ \t]+([0-9]+)
```

Texto de prueba original:

```text
§ 12
```

Reemplazar por:

```text
§ $1
```

Resultado del reemplazo:

```text
§ 12
```

Contraejemplo que no debe coincidir:

```text
§12
```


Limitaciones y observaciones: Entre § y $1 hay NBSP real. Con numeración compleja, verifique el fragmento encontrado.

### D11. Posible palabra adyacente repetida

Encontrar repetición entre espacios horizontales sin cruzar párrafos.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: desactivado.

Buscar:

```regex
\b(\p{L}+)[ \t\x{00A0}]+\1\b
```

Texto de prueba original:

```text
Это это уже было.
```

Coincidencia esperada:

```text
Это это
```

Contraejemplo que no debe coincidir:

```text
Это уже было.
```


Limitaciones y observaciones: «Да да» y otras repeticiones pueden ser voluntarias. Antes de borrar la segunda, decida entre supresión, coma o ninguna modificación. No cubre totalmente apóstrofos y guiones.

### D12. Latín y cirílico dentro de una palabra

Detectar mezcla OCR en una palabra continua, no cualquier línea bilingüe.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?<![\p{L}\p{M}])(?=[\p{L}\p{M}]*\p{Latin})(?=[\p{L}\p{M}]*\p{Cyrillic})[\p{L}\p{M}]+(?![\p{L}\p{M}])
```

Texto de prueba original:

```text
В слове Тeст латинская e.
```

Coincidencia esperada:

```text
Тeст
```

Contraejemplo que no debe coincidir:

```text
Он прочитал Latin.
```


Limitaciones y observaciones: En Тeст, e es latina. Una palabra totalmente latina junto a una rusa no se marca como mixta. Fórmulas, marcas y efectos tipográficos pueden ser legítimos. No translitera.

### D13. Minúscula seguida inmediatamente de mayúscula

Detectar posibles palabras pegadas o error de caja.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
\p{Ll}\p{Lu}
```

Texto de prueba original:

```text
ОнвышелИздома.
```

Coincidencia esperada:

```text
лИ
```

Contraejemplo que no debe coincidir:

```text
Он вышел из дома.
```


Limitaciones y observaciones: Debe distinguir la caja. Nombres como McDonald pueden ser correctos. Indica una transición, no reconstruye automáticamente la separación.

### D14. Dígito entre letras

Encontrar una sustitución OCR habitual de letra por dígito.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
\p{L}+[0-9]+\p{L}+
```

Texto de prueba original:

```text
Это сл0во.
```

Coincidencia esperada:

```text
сл0во
```

Contraejemplo que no debe coincidir:

```text
В главе 10 текст.
```


Limitaciones y observaciones: H2O y otras fórmulas son válidas. No cambie todos los 0 por о ni los 3 por з.

### D15. Puntuación dentro de letras

Examinar un signo con letras inmediatamente a ambos lados.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
\p{L}+[.,;:!?]\p{L}+
```

Texto de prueba original:

```text
Он,сказал слово.
```

Coincidencia esperada:

```text
Он,сказал
```

Contraejemplo que no debe coincidir:

```text
Он, сказав слово, ушёл.
```


Limitaciones y observaciones: También coinciden abreviaturas, direcciones y dominios. Para solo comas, use [,]. No inserte espacios en todos los resultados de golpe.

### D16. Párrafo iniciado en minúscula

Detectar un posible salto de párrafo superfluo.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
^[ \t]*\p{Ll}
```

Texto de prueba original:

```text
продолжение предложения.
```

Coincidencia esperada:

```text
п
```

Contraejemplo que no debe coincidir:

```text
Начало предложения.
```


Limitaciones y observaciones: Versos, leyendas, listas y citas pueden empezar en minúscula. No une párrafos ni comprende sus vecinos.

### D17. Sin puntuación final antes de cierres

Detectar final de letra o cifra seguido solo de signos de cierre y espacios.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[\p{L}\p{N}][»”")\]}]*[ \t\x{00A0}]*$
```

Texto de prueba original:

```text
«Он пришёл»
```

Coincidencia esperada:

```text
л»
```

Contraejemplo que no debe coincidir:

```text
«Он пришёл!»
```


Limitaciones y observaciones: Títulos y leyendas no suelen necesitar punto. Una referencia de nota tras puntuación correcta puede dar falso positivo. No marca «Он пришёл!», a diferencia de comprobar solo el último ».

### D18. Minúscula después de final de oración

Detectar un probable error de caja después de punto, interrogación o exclamación.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[.!?…][ \t]+[«„“"(\[]?\p{Ll}
```

Texto de prueba original:

```text
Он пришёл. потом ушёл.
```

Coincidencia esperada:

```text
. п
```

Contraejemplo que no debe coincidir:

```text
Он пришёл. Потом ушёл.
```


Limitaciones y observaciones: Los puntos abreviativos y suspensiones del autor no siempre terminan la oración. Son candidatos a revisión.

### D19. Posible punto omitido

Encontrar paso de terminación minúscula a palabra capitalizada tras un espacio.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
\p{Ll}[»”]?[ \t]+[«„“]?\p{Lu}\p{Ll}+
```

Texto de prueba original:

```text
Он пришёл Потом ушёл.
```

Coincidencia esperada:

```text
л Потом
```

Contraejemplo que no debe coincidir:

```text
Он пришёл потом ушёл.
```


Limitaciones y observaciones: Nombres y títulos dentro de oración producirán muchos resultados válidos. No decide si hace falta punto, coma o nada.

### D20. Parejas de comillas rectas

Encontrar un fragmento sencillo entre comillas dobles rectas en una línea.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
"([^"\r\n]+)"
```

Texto de prueba original:

```text
Он сказал "да".
```

Coincidencia esperada:

```text
"да"
```

Contraejemplo que no debe coincidir:

```text
Он сказал «да».
```


Limitaciones y observaciones: Revise anidación, pulgadas, código y estilo elegido. «$1» solo conviene en contexto seleccionado; no es tipografía universal.

### D21. Guion o raya entre números

Encontrar un posible intervalo para decisión editorial.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?<![0-9])[0-9]+[ \t]*[-—][ \t]*[0-9]+(?![0-9])
```

Texto de prueba original:

```text
Страницы 12 - 15.
```

Coincidencia esperada:

```text
12 - 15
```

Contraejemplo que no debe coincidir:

```text
Страницы 12–15.
```


Limitaciones y observaciones: También pueden coincidir 2026-10-02, números negativos y restas. No convierta automáticamente en intervalos. – es semirraya, — raya, - guion.

### D22. Dos iniciales antes del apellido

Encontrar una forma sencilla de dos iniciales para revisar espacios.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
\b(\p{Lu})\.[ \t]*(\p{Lu})\.[ \t]+(\p{Lu}\p{Ll}+)\b
```

Texto de prueba original:

```text
И.О. Иванов
```

Coincidencia esperada:

```text
И.О. Иванов
```

Contraejemplo que no debe coincidir:

```text
Иванов Иван
```


Limitaciones y observaciones: No cubre todos los apellidos compuestos ni diacríticos. Tras revisar, puede usar $1., NBSP, $2., NBSP, $3; introduzca espacios inseparables reales.

### D23. Apellido antes de dos iniciales

Encontrar el orden inverso del nombre.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
\b(\p{Lu}\p{Ll}+)[ \t]+(\p{Lu})\.[ \t]*(\p{Lu})\.
```

Texto de prueba original:

```text
Иванов И.О.
```

Coincidencia esperada:

```text
Иванов И.О.
```

Contraejemplo que no debe coincidir:

```text
Иванов Иван
```


Limitaciones y observaciones: Comprueba forma, no identidad. Apellidos compuestos, partículas y tres iniciales requieren otra regla.

### D24. Caracteres invisibles para revisar

Encontrar guion discrecional, espacio de anchura cero o FEFF.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[\x{00AD}\x{200B}\x{FEFF}]
```

Texto de prueba original:

```text
сло​во
```

El mismo texto de prueba con los espacios visibles:

```text
сло[ZWSP]во
```

Aquí ␠ representa un espacio normal; [TAB], una tabulación; [NBSP], U+00A0; [NNBSP], U+202F; y [ZWSP], U+200B. Es una representación explicativa: no introduzca estas etiquetas en el libro.

Coincidencia esperada:

```text
​
```

Contraejemplo que no debe coincidir:

```text
слово
```


Limitaciones y observaciones: El resultado no se ve: U+200B está entre о y в. Inserte o borre solo tras averiguar su función. BOM del archivo y FEFF textual son casos diferentes.

### D25. Espacios Unicode inusuales

Encontrar espacios estrechos, anchos y especiales.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]
```

Texto de prueba original:

```text
10 000
```

El mismo texto de prueba con los espacios visibles:

```text
10[NNBSP]000
```

Aquí ␠ representa un espacio normal; [TAB], una tabulación; [NBSP], U+00A0; [NNBSP], U+202F; y [ZWSP], U+200B. Es una representación explicativa: no introduzca estas etiquetas en el libro.

Coincidencia esperada:

```text
 
```

Contraejemplo que no debe coincidir:

```text
10 000
```


Limitaciones y observaciones: Un espacio estrecho inseparable entre cifras puede ser correcto. Detecta incoherencia, no declara erróneo todo carácter especial.

### D26. Interrogaciones y exclamaciones repetidas

Localizar puntuación expresiva o duplicación accidental.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
[!?]{2,}
```

Texto de prueba original:

```text
Что?! Правда!!!
```

Coincidencias esperadas, en orden:

```text
?!
!!!
```

Contraejemplo que no debe coincidir:

```text
Что? Правда!
```


Limitaciones y observaciones: ?! y repeticiones del autor pueden ser intencionadas. La acción es revisar, no reducir toda secuencia a un signo.

### D27. Х cirílica junto a numerales romanos

Encontrar Х rusa en una notación con caracteres romanos latinos.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: no necesario para esta expresión. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
(?<=[IVXLCDM])Х|Х(?=[IVXLCDM])
```

Texto de prueba original:

```text
Глава IХ
```

Coincidencia esperada:

```text
Х
```

Contraejemplo que no debe coincidir:

```text
Глава IX
```


Limitaciones y observaciones: Х es cirílica; X, latina. Revise caja y contexto. No valida todo el número romano.

### D28. Varias mayúsculas antes de minúsculas

Detectar posible anomalía OCR de caja al inicio de palabra.

Acción: solo buscar y revisar.

«Expresión regular»: activada; «Solo palabras completas»: desactivada. «Unicode (UCP)»: activado. «Distinguir mayúsculas y minúsculas»: activado.

Buscar:

```regex
\p{Lu}{2,}\p{Ll}+
```

Texto de prueba original:

```text
Он сказал ПРИвет.
```

Coincidencia esperada:

```text
ПРИвет
```

Contraejemplo que no debe coincidir:

```text
Он сказал Привет.
```


Limitaciones y observaciones: Nombres, siglas con sufijos y abreviaturas latinas pueden ser válidos. No cambie la caja globalmente sin revisar.

## 18. Problemas frecuentes y diagnóstico

### 18.1. El texto se ve, pero no se encuentra

Compruebe Diseño, regex, caja, ámbito, dirección y caracteres reales. `a` latina y `а` cirílica se parecen, pero difieren. NBSP no es espacio normal y las comillas tipográficas no son rectas.

Para límites rusos revise UCP; para patrones de mayúsculas, la caja. No combine Solo palabras completas con límites propios complejos sin necesidad.

### 18.2. Se encuentra demasiado texto

Sospeche del punto-asterisco codicioso o de una clase negativa amplia. Sustituya «cualquier texto» por un conjunto permitido, limite longitud y fronteras y compruebe dotall.

### 18.3. Los espacios cruzan párrafos

No use `\s+` como sinónimo de espacio normal. Empiece por `[ \t]+` entre palabras y añada NBSP por separado si hace falta.

### 18.4. Se insertan cifras, se pierden barras o falla un grupo nombrado

Revise la sección 16. `${name}`, `$10`, `\n`, `\x{...}` no se interpretan según reglas de búsqueda u otro editor. La captura requerida debe existir y no ser solo opcional en otra rama.

### 18.5. «Alfabetos mezclados» encuentra frases bilingües normales

Un ejemplo anterior probaba cirílico y latín en toda la línea, incluida `Он прочитал Latin.`. D12 limita ambas comprobaciones a una palabra. Es la diferencia crucial entre error OCR y línea bilingüe.

### 18.6. «Falta un punto» marca una cita correcta

El último carácter no basta: `»` puede seguir a `!`. D17 considera cierres, aunque títulos y notas siguen necesitando revisión.

### 18.7. No se encuentra una estructura

Un regex textual no ve el DOM como un script. Etiquetas, anidación, IDs ausentes y movimiento de notas son tareas distintas. Quizá corresponda usar Código, validador o script, no un patrón aún más complejo.

## 19. Rendimiento y libros grandes

Empiece por una condición concreta: signo, clase, palabra o inicio de párrafo. Evite repeticiones ilimitadas anidadas y varios fragmentos competidores de «texto cualquiera». Un fallo puede explorar muchísimas alternativas.

No codicioso no equivale a rápido: también explora posibilidades. Excluir delimitadores, acotar longitud y reducir ámbito suelen ayudar más.

Si la búsqueda tarda, no superponga otros reemplazos. Simplifique y pruebe en texto corto. Un error de límite de recursos PCRE2 no significa «sin resultados».

Para procesos con párrafos vecinos y etiquetas, un script puede ser más claro y seguro. No mezcle todas las reglas en una alternativa enorme; las reglas separadas explican cada hallazgo.

## 20. Verificaciones antes de guardar

Inspeccione principio, centro y final del fragmento editado. Revise varios resultados de cada tipo, especialmente citas, intervalos, iniciales, notas y formato. Asegúrese de conservar NBSP significativas y límites de párrafos.

Tras cambios estructurales delicados, valide con FBE. Guarde, vuelva a abrir si procede y compare. Una búsqueda exitosa no sustituye validación FB2 ni corrección editorial.

## 21. Fuentes y alcance

Traducción del manual ruso ampliado a partir de regex-design.md. Se redactaron explicaciones y pruebas nuevas y se aclararon UCP, límites del sujeto, alfabetos mezclados y reemplazo con fuentes primarias. Las recetas son escenarios editoriales, no citas del manual PCRE2.

[D1] Sintaxis oficial PCRE2: anclas, grupos, propiedades Unicode, retroceso y opciones.

`https://www.pcre.org/current/doc/html/pcre2pattern.html`

[D2] Documentación oficial Unicode de PCRE2.

`https://www.pcre.org/current/doc/html/pcre2unicode.html`

[D3] Compatibilidad PCRE2 y adaptador FBE Next, revisión d2257405d95b0328649acee64b38829b40a4314b.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/docs/pcre2-compatibility.md`

[D4] GetReplStr, PrepareRegexReplacementText y operaciones de FBEview.cpp; SearchPresetCatalog.cpp y search-preset-design-fixtures.cpp de la misma revisión.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/src/fbe/FBEview.cpp`

Sintaxis, capacidades de interfaz y acierto editorial son tres niveles. Las pruebas locales no garantizan cualquier compilación con cualquier libro. El README del archivo explica la validación.
