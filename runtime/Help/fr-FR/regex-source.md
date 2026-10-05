# Aide sur les expressions régulières — Code

Note sur la traduction : les mots et phrases russes des exemples de contrôle sont conservés volontairement. Il s’agit de données de test : expressions régulières, chaînes de remplacement, caractères d’espacement et résultats attendus restent identiques à ceux de l’édition russe de référence. Les explications sont traduites ; les exemples ne sont pas automatiquement adaptés aux règles typographiques françaises.

Guide complet de recherche et de remplacement dans le XML d’un livre avec FictionBook Editor Next.

Édition du 2 octobre 2026. Le mode Design est décrit dans regex-design.md. Les recettes suivantes concernent le mode Code ; ne transposez pas les expressions PCRE2 sans vérification.

## 1. À quoi sert la recherche en mode Code

Le mode Code expose balises XML, attributs, liens, entités et texte. Il convient à l’audit FB2 : éléments vides, balises HTML importées, liens de substitution, attributs inattendus et restes techniques de conversion.

Pour relire des mots ordinaires, Design est souvent plus commode. En XML, un résultat peut aussi se trouver dans un attribut, un commentaire, une CDATA, un nom de fichier ou des données binaires. Tenez-en compte avant remplacement.

« Rechercher et vérifier uniquement » fournit des candidats, pas la preuve d’une erreur. Même remplacer des espaces peut modifier du contenu significatif ; aucun remplacement global sur du XML quelconque n’est universellement sûr.

## 2. Démarrage rapide

Enregistrez une copie, passez en Code, ouvrez Rechercher et activez les expressions régulières. Les exemples XML respectent généralement la casse et désactivent Mot entier. Le motif définit les limites.

Paragraphe simple vide :

```regex
<p>[ \t]*</p>
```

Trouve `<p></p>` et `<p>   </p>` sur une ligne physique, pas `<p>Текст</p>`. Ne supprimez pas automatiquement le résultat et ne le changez pas en `<empty-line/>` : leurs fonctions structurelles peuvent différer.

Collez seulement le motif, sans `/.../g`, guillemets de chaîne C++ ni doubles barres de JSON.

## 3. Moteur : Scintilla, pas PCRE2

FBE emploie Scintilla avec `SCFIND_REGEXP` et `SCFIND_CXX11REGEX`. Dans la version considérée, c’est une implémentation C++ de la grammaire ECMAScript, pas PCRE2 ni tout le JavaScript moderne. [S1, S2]

Scintilla possède un ancien moteur de base avec d’autres conventions. Ne mélangez pas ses groupes à parenthèses échappées avec le mode C++11 : `(слово)` capture, `\(слово\)` exige des parenthèses littérales.

Unicode n’est pas interdit, mais UCP et les propriétés PCRE2 sont absents. Les classes dépendent de la bibliothèque standard ; `\w` ne signifie pas forcément toutes les lettres de toutes les langues. Pour l’ASCII prévisible, utilisez `[A-Za-z0-9_]`. `[А-Яа-яЁё]` convient au russe sans couvrir tout le cyrillique.

### 3.1. Différences essentielles avec Design

| Propriété | Design | Code |
| --- | --- | --- |
| Sujet | Représentation textuelle du livre | Source XML |
| Moteur | PCRE2-16 | Scintilla C++11 |
| UCP et propriétés Unicode | Disponibles dans PCRE2 | Syntaxe PCRE2 non prise en charge |
| Assertion arrière | Disponible avec limites PCRE2 | Non prise en charge |
| Remplacement du premier groupe | `$1` ou `\1` | `\1` |
| Mise en forme au remplacement | Commandes FBE | Texte XML seulement |
| Recherche entre lignes physiques | Dépend du sujet ; remplacement entre paragraphes limité | Indisponible dans le chemin actuel |

## 4. Recherche ligne par ligne : limite essentielle

`MatchOnLines` confronte le motif au contenu de chaque ligne physique séparément. Une ligne peut être longue ; son retour visuel dans la fenêtre n’en crée pas une autre. [S2]

Exemple :

```xml
<empty-line/> <empty-line/>
```

et :

```xml
<empty-line/>
<empty-line/>
```

En XML, ces écritures peuvent représenter les mêmes voisins. Pour la recherche actuelle, seule la première tient dans une correspondance ; la seconde franchit une frontière de ligne.

Ajouter `\r\n`, `\n`, `\x0D`, `\x0A`, `\s*` ou une classe de tout caractère ne lève pas cette limite. Les deux lignes ne sont pas fournies ensemble au moteur. Il faudrait modifier le chemin de recherche, pas seulement le motif.

En revanche, un remplacement peut insérer une fin de ligne. C’est une opération distincte, montrée en K28. Ne confondez pas rechercher entre lignes et insérer une ligne après une correspondance.

### 4.1. Travailler avec du XML multiligne

Pour un attribut placé seul sur une ligne, cherchez souvent l’attribut sans toute la balise ouvrante. Les relations entre plusieurs lignes exigent outils structurels, validateur, script ou outil XML.

Ne supprimez pas toutes les fins de ligne pour satisfaire une regex : texte, commentaires, CDATA et lisibilité pourraient changer. Déterminez d’abord si la tâche nécessite vraiment une correspondance multiligne.

## 5. Caractères et échappement

Un caractère ordinaire se recherche lui-même ; un métacaractère littéral s’échappe avec une barre oblique inverse.

| Texte recherché | Expression |
| --- | --- |
| Point | `\.` |
| Plus | `\+` |
| Interrogation | `\?` |
| Astérisque | `\*` |
| Parenthèse | `\(` ou `\)` |
| Nombre entre crochets | `\[[0-9]+\]` |
| Barre oblique inverse | `\\` |

`.` représente un caractère disponible de la ligne, pas un point littéral. En UTF-8, l’adaptateur Scintilla et la bibliothèque standard déterminent le traitement ; ne mesurez pas les emoji en supposant qu’un point égale toujours un signe visible.

`\r` et `\n` désignent CR/LF, mais ne font pas traverser les lignes au chemin actuel. Pour espaces et tabulations, utilisez `[ \t]`.

`\Q...\E` de PCRE2 n’est pas une citation littérale portable ici. Échappez les caractères spéciaux individuellement.

## 6. Classes, plages et limites de mots

`[abc]` trouve un caractère de la liste, `[a-z]` d’une plage et `[^abc]` hors de la liste. Une répétition peut suivre : `[0-9]+`.

| Classe | Sens pratique |
| --- | --- |
| `[0-9]` | Chiffre ASCII |
| `[A-Za-z]` | Lettre ASCII latine |
| `[ \t]` | Espace ordinaire ou tabulation |
| `[^<]` | Caractère disponible autre que < |
| `[^"']` | Caractère autre que les deux guillemets droits |
| `\d`, `\D` | Classe de chiffres de l’implémentation et négation |
| `\s`, `\S` | Espacement et négation |
| `\w`, `\W` | Classe de mot et négation |

Une classe négative ne comprend pas XML. `[^<]*` est utile pour un contenu textuel simple, pas pour analyser entièrement entités, commentaires ou CDATA.

`\b` est une limite de mot, `\B` son contraire. Pour les attributs, cela peut être insuffisant : `id` peut suivre un deux-points dans un autre nom. Vérifiez plutôt un séparateur autorisé : début de ligne, espace ou tabulation.

## 7. Ancres et répétitions

`^` et `$` désignent les limites physiques de ligne dans le chemin actuel. Pour une ligne de blancs horizontaux :

```regex
^[ \t]+$
```

Supprimer les caractères trouvés laisse la ligne physique vide : sa fin de ligne ne faisait pas partie du résultat.

| Quantificateur | Répétitions |
| --- | --- |
| `?` | Zéro ou une |
| `*` | Zéro ou davantage |
| `+` | Une ou davantage |
| `{3}` | Exactement trois |
| `{3,}` | Au moins trois |
| `{2,5}` | Deux à cinq |

La grammaire C++11 accepte les variantes non gourmandes `*?` et `+?`. Pour un attribut, exclure le guillemet délimitant est souvent plus clair :

```regex
"[^"\r\n]*"
```

L’exemple concerne les guillemets doubles. Les simples nécessitent une branche adaptée.

Ne transposez pas les quantificateurs possessifs, groupes atomiques et options intégrées de PCRE2.

## 8. Groupes, alternative et assertions avant

Un groupe capturant `( ... )` mémorise un fragment ; `(?: ... )` regroupe sans numéro.

Choisir plusieurs balises :

```regex
<(?:strong|emphasis)>
```

Une référence numérique peut relier noms ouvrant et fermant dans un fragment simple sur une ligne :

```regex
<(strong|emphasis)>[ \t]*</\1>
```

Le même nom est exigé. Cela ne valide pas une imbrication XML quelconque.

Les assertions avant positive `(?=...)` et négative `(?!...)` vérifient la suite sans la consommer. Une limite valide après un nom évite que `<a` corresponde au début de `<author>`.

```regex
<a(?=[ \t>])
```

Pour vérifier à gauche, capturez le contexte puis restituez-le dans le remplacement. Le lookbehind est indisponible ; ne copiez pas `(?<=...)` depuis l’aide Design.

## 9. Remplacement en mode Code

Avec `SCI_REPLACETARGETRE`, la correspondance est déjà trouvée et Scintilla interprète le remplacement. Ce n’est ni `std::regex_replace` avec `$1`, ni la grammaire Design. [S1, S2]

### 9.1. Références aux captures

| Dans le remplacement | Signification |
| --- | --- |
| `\0` | Correspondance entière |
| `\1` … `\9` | Groupes correspondants |
| `$1` | Ne pas utiliser comme référence : syntaxe incorrecte ici |

Rechercher :

```regex
\(([0-9]+)\)
```

Remplacer par :

```text
[\1]
```

`(12)` devient `[12]`. Cela n’a de sens éditorial que dans le bon contexte : un nombre entre parenthèses n’est pas forcément un appel de note.

Le dollar littéral n’a pas à être doublé selon les règles d’un autre système. La barre oblique inverse est, elle, un caractère de contrôle.

### 9.2. Caractères de contrôle au remplacement

Le code Scintilla vérifié interprète `\t`, `\n`, `\r` et `\\` comme tabulation, LF, CR et barre oblique inverse. Cela ne contredit pas l’absence de recherche entre lignes. [S2]

Pour CRLF, utilisez `\r\n` ; pour LF, `\n`, selon le document. Évitez de mélanger involontairement les terminaisons.

`\x{00A0}` n’insère pas un Unicode dans ce champ. Utilisez une véritable NBSP. Les commandes Design `\U`, `\L`, `\T`, `\S`, `\E` et `\Q` ne règlent ici ni casse ni mise en forme.

### 9.3. Suppression et conservation du XML

Un remplacement vide supprime la plage trouvée. Effacer une balise, un attribut ou un lien vide peut endommager le document malgré une recherche réussie. Les recettes structurelles sont surtout diagnostiques.

Renommer un `id` à une seule occurrence peut laisser les références anciennes. Pour les objets liés, utilisez le renommage d’identifiant FBE, pas des remplacements textuels indépendants.

## 10. Lire les recettes XML

XML respecte la casse : `<p>` et `<P>` diffèrent. L’ordre des attributs ne change pas leur sens, les valeurs peuvent être entre guillemets simples ou doubles et des espaces entourer l’égalité. Les exemples couvrent les cas usuels tant que la règle reste raisonnable. [S3]

`l:` et `xlink:` sont des préfixes XLink fréquents en FB2. Le sens dépend de `xmlns`, pas du préfixe en soi. Les recettes limitées à ces formes ne couvrent pas tous les préfixes : adaptez-les après vérification de la déclaration.

`[^>]*` approxime souvent le contenu d’une balise ouvrante. C’est pratique, mais `>` peut apparaître légalement dans une valeur d’attribut. Commentaires et CDATA peuvent imiter des balises. Ces recettes sélectionnent donc des candidats, sans validation complète ni restructuration aveugle.

Un élément vide n’est pas toujours une erreur de schéma, une entité numérique n’est pas un caractère corrompu. Un nom inhabituel, un lien externe ou un placeholder peuvent avoir un rôle légitime.

## 11. Recettes pratiques FB2/XML

Par défaut, les parties impliquées doivent figurer sur une ligne physique. Si le motif vise toute la balise ouvrante, tous les attributs examinés doivent aussi y être. Une recherche d’attribut seul n’exige pas que toute la balise soit sur la même ligne.

### K01. Espaces en fin de ligne physique

Trouver les espaces ordinaires et tabulations finales.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
[ \t]+$
```

Texte de contrôle initial :

```text
<p>Текст</p>
```

Le même texte de contrôle avec les espaces rendus visibles :

```text
<p>Текст</p>␠␠␠
```

Ici, ␠ représente une espace ordinaire, [TAB] une tabulation, [NBSP] U+00A0, [NNBSP] U+202F et [ZWSP] U+200B. Cette notation est explicative ; ne pas insérer ces libellés dans le livre.

Remplacer par : laisser le champ entièrement vide. Ne pas saisir le mot « vide ».

Résultat du remplacement :

```text
<p>Текст</p>
```

Contre-exemple qui ne doit pas correspondre :

```text
<p>Текст</p>
```


Limites et remarques : Ne supprime pas la fin de ligne. Dans du contenu XML mixte, CDATA ou des zones où les blancs sont significatifs, ils peuvent appartenir au texte. Un nettoyage de toutes les lignes n’est pas inconditionnellement sûr.

### K02. Vider une ligne de blancs

Retirer les blancs d’une ligne sans autre contenu.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
^[ \t]+$
```

Texte de contrôle initial :

```text

<p>Text</p>
```

Le même texte de contrôle avec les espaces rendus visibles :

```text
␠␠␠[TAB]
<p>Text</p>
```

Ici, ␠ représente une espace ordinaire, [TAB] une tabulation, [NBSP] U+00A0, [NNBSP] U+202F et [ZWSP] U+200B. Cette notation est explicative ; ne pas insérer ces libellés dans le livre.

Remplacer par : laisser le champ entièrement vide. Ne pas saisir le mot « vide ».

Résultat du remplacement :

```text

<p>Text</p>
```

Contre-exemple qui ne doit pas correspondre :

```text
<p>Text</p>
```


Limites et remarques : La ligne demeure vide : LF/CRLF ne faisait pas partie du résultat. Ce n’est pas la suppression de toutes les lignes physiques vides.

### K03. Espace avant fermeture d’un élément vide

Trouver un intervalle ordinaire juste avant />.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
[ \t]+/>
```

Texte de contrôle initial :

```text
<empty-line />
```

Remplacer par :

```text
/>
```

Résultat du remplacement :

```text
<empty-line/>
```

Contre-exemple qui ne doit pas correspondre :

```text
<empty-line/>
```


Limites et remarques : XML accepte les deux formes. La modification est cosmétique, pas obligatoire. La séquence peut aussi apparaître dans un texte ou commentaire ; vérifier avant remplacement global.

### K04. Paragraphe simple vide

Examiner une paire p sans contenu textuel.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
<p>[ \t]*</p>
```

Texte de contrôle initial :

```text
<p>  </p><p>Text</p>
```

Correspondance attendue :

```text
<p>  </p>
```

Contre-exemple qui ne doit pas correspondre :

```text
<p>Text</p>
```


Limites et remarques : Ne couvre pas attributs, sauts de ligne, NBSP sous forme d’entité ou balises imbriquées. Un p vide et empty-line ne sont pas automatiquement interchangeables.

### K05. Deux empty-line sur une ligne

Trouver deux éléments de ligne vide FB2 voisins sur une ligne source physique.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
<empty-line[ \t]*/>[ \t]*<empty-line[ \t]*/>
```

Texte de contrôle initial :

```text
<empty-line/> <empty-line />
```

Correspondance attendue :

```text
<empty-line/> <empty-line />
```

Contre-exemple qui ne doit pas correspondre :

```text
<empty-line/>
<empty-line/>
```


Limites et remarques : Seules espaces ordinaires et tabulations les séparent. Le saut de ligne est volontairement exclu. Deux lignes vides peuvent séparer des scènes.

### K06. Métadonnées importantes vides

Vérifier plusieurs champs simples sans contenu.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
<(book-title|first-name|middle-name|last-name|genre|lang)>[ \t]*</\1>
```

Texte de contrôle initial :

```text
<book-title> </book-title>
```

Correspondance attendue :

```text
<book-title> </book-title>
```

Contre-exemple qui ne doit pas correspondre :

```text
<book-title>Book</book-title>
```


Limites et remarques : Tous ces champs ne sont pas obligatoires partout. Regex ne valide ni schéma FB2, ni parent, ni justesse des valeurs remplies.

### K07. Mise en forme inline vide

Trouver des éléments de mise en forme appariés et vides.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
<(strong|emphasis|strikethrough)>[ \t]*</\1>
```

Texte de contrôle initial :

```text
<strong> </strong>
```

Correspondance attendue :

```text
<strong> </strong>
```

Contre-exemple qui ne doit pas correspondre :

```text
<strong>Text</strong>
```


Limites et remarques : NBSP, attributs ou éléments imbriqués exigent un autre motif. Vérifier le rôle structurel avant suppression.

### K08. Mise en forme identique imbriquée en elle-même

Distinguer strong/strong du mélange valide strong/emphasis.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
<(strong|emphasis)>[ \t]*<\1(?=[ \t>])
```

Texte de contrôle initial :

```text
<strong><strong>Text</strong></strong>
```

Correspondance attendue :

```text
<strong><strong
```

Contre-exemple qui ne doit pas correspondre :

```text
<strong><emphasis>Text</emphasis></strong>
```


Limites et remarques : Le second nom n’est pas choisi indépendamment : il renvoie au premier. Le résultat ne comprend pas l’élément entier et n’est pas destiné à être supprimé comme tel.

### K09. Balises HTML possibles après import

Trouver des noms HTML fréquents à examiner dans FB2.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
</?(?:b|i|br|div|span|font)(?=[ \t/>])[^>]*>
```

Texte de contrôle initial :

```text
<div>Text</div>
```

Correspondances attendues, successivement :

```text
<div>
</div>
```

Contre-exemple qui ne doit pas correspondre :

```text
<section><p>Text</p></section>
```


Limites et remarques : Commentaires et CDATA peuvent produire des correspondances. Ne convertissez pas globalement b en strong et i en emphasis sans vérifier contenu et attributs.

### K10. Balise en majuscules

Trouver des noms usuels entièrement en majuscules.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
</?[A-Z][A-Z0-9_.:-]*(?=[ \t/>])[^>]*>
```

Texte de contrôle initial :

```text
<P>Text</P>
```

Correspondances attendues, successivement :

```text
<P>
</P>
```

Contre-exemple qui ne doit pas correspondre :

```text
<p>Text</p>
```


Limites et remarques : La casse doit être respectée, sinon p ordinaire correspond aussi. Tous les noms XML Unicode et espaces de noms valides ne sont pas vérifiés.

### K11. Entité HTML NBSP

Trouver &nbsp; littéral dans la source.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
&nbsp;
```

Texte de contrôle initial :

```text
<p>&nbsp;</p>
```

Correspondance attendue :

```text
&nbsp;
```

Contre-exemple qui ne doit pas correspondre :

```text
<p>&#160;</p>
```


Limites et remarques : Sans déclaration adéquate, &nbsp; ne fait pas partie des cinq entités XML prédéfinies. Vérifier DTD et commentaires. Ne pas décoder globalement toutes les entités.

### K12. Références numériques de caractères

Trouver les écritures décimales ou hexadécimales.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
&#(?:[0-9]+|x[0-9A-Fa-f]+);
```

Texte de contrôle initial :

```text
<p>&#160; &#xA0;</p>
```

Correspondances attendues, successivement :

```text
&#160;
&#xA0;
```

Contre-exemple qui ne doit pas correspondre :

```text
<p>&amp;</p>
```


Limites et remarques : Les deux peuvent être correctes. Le motif vérifie la forme, pas le point de code. Transformer &lt; en < littéral peut endommager le XML.

### K13. id vide

Trouver un attribut id ordinaire vide, avec les deux types de guillemets.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:""|'')
```

Texte de contrôle initial :

```text
<section id="">
```

Correspondance attendue :

```text
 id=""
```

Contre-exemple qui ne doit pas correspondre :

```text
<section id="s1">
```


Limites et remarques : L’espace précédente appartient au résultat. xml:id n’est pas couvert. Créer un nouvel ID implique références et unicité, non contrôlées par regex.

### K14. Lister les valeurs id

Trouver des id ordinaires remplis pour audit.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(?:^|[ \t])id[ \t]*=[ \t]*(?:"[^"]+"|'[^']+')
```

Texte de contrôle initial :

```text
<section id="s1">
```

Correspondance attendue :

```text
 id="s1"
```

Contre-exemple qui ne doit pas correspondre :

```text
<section id="">
```


Limites et remarques : Les résultats ne prouvent ni unicité ni validité. Utiliser le renommage FBE pour les objets liés.

### K15. Liens HTTP(S) externes

Trouver href avec un préfixe XLink courant et une adresse externe.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"https?://[^"]+"|'https?://[^']+')
```

Texte de contrôle initial :

```text
<a l:href="https://example.test/book">Text</a>
```

Correspondance attendue :

```text
 l:href="https://example.test/book"
```

Contre-exemple qui ne doit pas correspondre :

```text
<a l:href="#note1">1</a>
```


Limites et remarques : Vise l’attribut, pas chaque URL du texte. l et xlink sont couverts ; vérifier les autres préfixes séparément. L’accessibilité de l’adresse n’est pas testée.

### K16. Liens locaux file://

Trouver une référence locale qui pourrait être inutilisable chez le lecteur.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"file://[^"]+"|'file://[^']+')
```

Texte de contrôle initial :

```text
<a xlink:href="file:///C:/Book/image.png">Text</a>
```

Correspondance attendue :

```text
 xlink:href="file:///C:/Book/image.png"
```

Contre-exemple qui ne doit pas correspondre :

```text
<a xlink:href="#image1">Text</a>
```


Limites et remarques : Ne pas ouvrir automatiquement une adresse inconnue. Le résultat ne décide pas s’il faut intégrer le fichier, changer le lien ou le supprimer.

### K17. Cible de lien vide

Trouver un href XLink ordinaire vide.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:""|'')
```

Texte de contrôle initial :

```text
<a l:href="">Text</a>
```

Correspondance attendue :

```text
 l:href=""
```

Contre-exemple qui ne doit pas correspondre :

```text
<a l:href="#note1">Text</a>
```


Limites et remarques : Texte de lien vide et attribut href absent sont d’autres cas à vérifier séparément.

### K18. Cible #undefined

Trouver une cible de substitution littérale.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(?:^|[ \t])(?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')
```

Texte de contrôle initial :

```text
<a xlink:href="#undefined">Text</a>
```

Correspondance attendue :

```text
 xlink:href="#undefined"
```

Contre-exemple qui ne doit pas correspondre :

```text
<a xlink:href="#note1">Text</a>
```


Limites et remarques : Vérifier l’objet réel. undefined n’est pas interdit comme nom XML ; il signale souvent une association inachevée.

### K19. Liens préfixés bookmark

Trouver les cibles bookmark typiques de convertisseurs, pas tous les liens internes.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#bookmark[^"]*"|'#bookmark[^']*')
```

Texte de contrôle initial :

```text
<a l:href="#bookmark12">Text</a>
```

Correspondance attendue :

```text
l:href="#bookmark12"
```

Contre-exemple qui ne doit pas correspondre :

```text
<a l:href="#note1">Text</a>
```


Limites et remarques : href="#..." trouve tout lien interne sans isoler bookmark. Une telle cible ne prouve pas un lien inutile ; examiner avant suppression.

### K20. Liens de retour Word/FBD

Trouver _ftnref et _ednref dans les cibles.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#_(?:ftnref|ednref)[^"]*"|'#_(?:ftnref|ednref)[^']*')
```

Texte de contrôle initial :

```text
<a l:href="#_ftnref1">Back</a>
```

Correspondance attendue :

```text
l:href="#_ftnref1"
```

Contre-exemple qui ne doit pas correspondre :

```text
<a l:href="#note1">Text</a>
```


Limites et remarques : Un retour peut servir à la navigation. Ne pas supprimer seulement à cause de l’origine du nom.

### K21. Liens de notes indépendants de l’ordre des attributs

Trouver une balise ouvrante a avec type=note et href XLink interne dans les deux ordres.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
<a(?=[ \t>])(?=[^>]*[ \t]type[ \t]*=[ \t]*(?:"note"|'note'))(?=[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+'))[^>]*>
```

Texte de contrôle initial :

```text
<a l:href="#note1" type="note">1</a>
```

Correspondance attendue :

```text
<a l:href="#note1" type="note">
```

Contre-exemple qui ne doit pas correspondre :

```text
<a type="link" l:href="#note1">1</a>
```


Limites et remarques : Accepte aussi xlink et guillemets simples. Tous les attributs examinés doivent tenir sur une ligne. Valeurs avec >, commentaires et balisage atypique nécessitent XML. La note cible n’est pas vérifiée.

### K22. Marqueurs numériques possibles de notes

Trouver un nombre entre crochets, accolades ou parenthèses.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
\[[0-9]+\]|\{[0-9]+\}|\([0-9]+\)
```

Texte de contrôle initial :

```text
<p>Text [12], {3}, (4).</p>
```

Correspondances attendues, successivement :

```text
[12]
{3}
(4)
```

Contre-exemple qui ne doit pas correspondre :

```text
<p>[note]</p>
```


Limites et remarques : Il peut s’agir de bibliographie, formules, explications ou texte ordinaire. La recette ne crée pas de notes et ne vérifie pas l’unicité.

### K23. Valeurs Your/Name oubliées

Vérifier les placeholders anglais habituels du nom dans les métadonnées.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
<first-name>Your</first-name>|<last-name>Name</last-name>
```

Texte de contrôle initial :

```text
<first-name>Your</first-name>
```

Correspondance attendue :

```text
<first-name>Your</first-name>
```

Contre-exemple qui ne doit pas correspondre :

```text
<first-name>John</first-name>
```


Limites et remarques : Examiner auteur/créateur et nom réel. Ne pas insérer automatiquement les données d’une autre édition.

### K24. Illustration non liée #undefined

Trouver image avec une cible de substitution habituelle.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
<image(?=[ \t/>])[^>]*[ \t](?:l|xlink):href[ \t]*=[ \t]*(?:"#undefined"|'#undefined')[^>]*>
```

Texte de contrôle initial :

```text
<image l:href="#undefined"/>
```

Correspondance attendue :

```text
<image l:href="#undefined"/>
```

Contre-exemple qui ne doit pas correspondre :

```text
<image l:href="#cover"/>
```


Limites et remarques : FB2 utilise normalement XLink-href, pas HTML-src. Vérifier séparément le binary correspondant. Image absente et lien erroné sont des causes distinctes.

### K25. Déclaration windows-1251

Trouver l’ancienne indication d’encodage XML.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
<\?xml[ \t]+[^?]*encoding[ \t]*=[ \t]*(?:"windows-1251"|'windows-1251')[^?]*\?>
```

Texte de contrôle initial :

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Correspondance attendue :

```text
<?xml version="1.0" encoding="windows-1251"?>
```

Contre-exemple qui ne doit pas correspondre :

```text
<?xml version="1.0" encoding="utf-8"?>
```


Limites et remarques : Pas une erreur en soi. Remplacer windows-1251 par utf-8 ne réencode pas les octets. Utiliser une sauvegarde/conversion cohérente avec la déclaration.

### K26. Esperluette sans entité standard reconnue

Trouver & devant une séquence qui ne ressemble pas à une référence standard.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
&(?!(?:amp|lt|gt|apos|quot);|#[0-9]+;|#x[0-9A-Fa-f]+;)
```

Texte de contrôle initial :

```text
<p>A & B</p>
```

Correspondance attendue :

```text
&
```

Contre-exemple qui ne doit pas correspondre :

```text
<p>A &amp; B</p>
```


Limites et remarques : & est valide dans CDATA/commentaires et une DTD peut déclarer d’autres entités. Filtre diagnostique, pas validateur. & → &amp; sans contexte peut doubler l’échappement.

### K27. Liens internes ordinaires

Trouver #id local séparément des bookmark.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(?:l|xlink):href[ \t]*=[ \t]*(?:"#[^"]+"|'#[^']+')
```

Texte de contrôle initial :

```text
<a l:href="#note1">1</a>
```

Correspondance attendue :

```text
l:href="#note1"
```

Contre-exemple qui ne doit pas correspondre :

```text
<a l:href="https://example.test">Text</a>
```


Limites et remarques : Le motif montre l’écriture du lien, pas l’existence d’une cible unique. Liens orphelins ou doublons exigent une analyse documentaire.

### K28. Séparer deux empty-line après une recherche monoligne

Illustrer la différence entre recherche limitée et insertion d’un saut par remplacement.

Action : effectuer un remplacement unique et contrôlé.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
(<empty-line[ \t]*/>)[ \t]*(<empty-line[ \t]*/>)
```

Texte de contrôle initial :

```text
<empty-line/> <empty-line/>
```

Remplacer par :

```text
\1\r\n\2
```

Résultat du remplacement :

```text
<empty-line/>
<empty-line/>
```

Contre-exemple qui ne doit pas correspondre :

```text
<empty-line/>
<empty-line/>
```


Limites et remarques : Scintilla développe les séquences en CRLF. Pour un document LF, utiliser \1\n\2. Ce n’est ni un formateur XML universel ni l’activation de recherche multiligne.

### K29. Espaces doubles dans du texte XML simple

Trouver un fragment entre balises avec plusieurs espaces ordinaires.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Respecter la casse » — activé.

Rechercher :

```regex
>[^<]*[ \t]{2,}[^<]*<
```

Texte de contrôle initial :

```text
<p>one  two</p>
```

Correspondance attendue :

```text
>one  two<
```

Contre-exemple qui ne doit pas correspondre :

```text
<p>one two</p>
```


Limites et remarques : Le résultat comprend les délimiteurs angulaires et tout le fragment. Design convient généralement mieux aux mots. Ne pas remplacer tout le résultat par une espace.

## 12. Pourquoi regex ne remplace pas la validation XML

Une expression établit une ressemblance locale avec une suite de caractères, pas la validité du document entier : imbrication, schéma FB2, déclarations d’espaces de noms, IDs uniques et cibles existantes nécessitent des contrôles distincts.

Deux `id` identiques sur des lignes différentes ne peuvent pas être détectés fiablement par une seule comparaison dans le chemin actuel. De même pour une cible manquante ailleurs dans le livre. Utilisez validation FBE et analyse structurelle.

Ne décodez pas toutes les entités : `&lt;` et `&amp;` empêchent souvent un texte de devenir balisage. Ne supprimez pas tout ce qui ressemble à HTML dans CDATA, commentaires ou citations de code.

L’opération doit préserver appariement, attributs et contenu. Changer seulement `<strong>` laisse la fermeture inchangée. Une référence arrière dans un exemple ne sécurise pas toute modification ultérieure.

## 13. Erreurs courantes

### 13.1. Balises minuscules au lieu de majuscules

Activez Respecter la casse. Ce n’est pas cosmétique en XML. `[A-Z]` sans sensibilité annule le but du contrôle.

### 13.2. $1 littéral dans le remplacement

Utilisez `\1`. FBE passe par Scintilla, pas par JavaScript ni un autre API à `$1`.

### 13.3. \p, \K ou lookbehind ne fonctionnent pas

Ces constructions appartiennent à un autre profil. Pour du texte, utilisez Design ; pour XML, réécrivez avec classes explicites, capture du contexte, groupes non capturants et assertions avant.

### 13.4. Deux balises voisines restent introuvables

Vérifiez fin de ligne physique, espace avant `/>`, guillemets simples, attributs supplémentaires et préfixes. Voisinage textuel et voisinage de l’arbre XML diffèrent.

### 13.5. Une note disparaît après changement d’ordre des attributs

« type puis href » dépend de l’ordre. K21 vérifie chaque propriété par un lookahead séparé. Si la balise est multiligne, la recherche complète reste impossible ; cherchez un attribut ou utilisez la structure.

### 13.6. strong/emphasis est signalé comme mauvaise imbrication

Des styles valides différents peuvent être imbriqués. Répéter le même nom se teste par référence arrière comme K08, pas par deux alternatives indépendantes.

### 13.7. « Bookmark » trouve toutes les notes

Un lien interne `#` n’est pas automatiquement un artefact bookmark. Distinguez audit général K27 et préfixe spécifique K19.

### 13.8. Le remplacement endommage le texte visible

En Code, le résultat peut être dans texte, attribut, commentaire ou binary. Annulez et resserrez motif ou portée. L’étiquette « remplacement sûr » ne supprime pas le contexte XML.

## 14. Limites du profil Source

N’utilisez pas UCP, propriétés PCRE2, lookbehind, groupes nommés modernes JavaScript/PCRE2, groupes atomiques, quantificateurs possessifs, branch reset, sous-programmes, verbes PCRE2, `\K`, `\G` ni commandes de formatage Design.

Les options intégrées PCRE2 ne remplacent pas les cases du dialogue. Les fonctions récentes du JavaScript des navigateurs n’apparaissent pas automatiquement dans C++11 ECMAScript.

MatchOnLines reste ligne par ligne, quelle que soit la séquence de fin de ligne du motif. Compiler localement avec un autre moteur ne prouve pas la compatibilité FBE.

Pour les mots Unicode et la relecture complexe, utilisez Design. Pour la structure, utilisez XML/FB2, pas un `.*` illimité pour contourner les limites.

## 15. Performances et longues lignes XML

Une courte expression n’est pas nécessairement rapide. Répétitions illimitées et alternatives concurrentes peuvent ralentir les grandes lignes, surtout une ligne unique avec tout le document ou un grand binary.

Commencez par un nom de balise ou attribut précis et une classe bornée plutôt que « tous caractères ». Des diagnostics séparés sont plus faciles à contrôler et annuler qu’une recherche de toutes les erreurs possibles.

N’aplatissez pas toutes les lignes pour contourner MatchOnLines : cela change le document sans garantir les performances.

## 16. Vérification après remplacement

Comparez la plage attendue et trouvée. Avec des groupes, contrôlez guillemets, préfixes, angles et fermetures. Vérifiez l’absence d’atteinte aux commentaires, CDATA et binary.

Validez FB2, enregistrez et rouvrez après les modifications importantes. Contrôlez Code ↔ Design, notes, images et texte. Un ID renommé implique son objet et tous les liens associés.

## 17. Sources et champ d’application

Cette traduction reprend le guide étendu issu de regex-source.md. La structure moteur, syntaxe, remplacement, exemples XML et limites est conservée ; des motifs trop larges ont été précisés. Explications et contrôles ont été rédigés à nouveau et comparés aux sources primaires.

[S1] Documentation officielle Scintilla, Searching : mode C++11, options et SCI_REPLACETARGETRE.

`https://www.scintilla.org/ScintillaDoc.html#Searching`

[S2] Code Scintilla du dépôt FBE à d2257405d95b0328649acee64b38829b40a4314b : Document.cxx, Cxx11RegexFindText, MatchOnLines, BuiltinRegex::SubstituteByPosition. Il distingue recherche interligne et insertion CR/LF au remplacement.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/third_party/scintilla/src/Document.cxx`

[S3] W3C XML 1.0 et Namespaces in XML : noms, attributs, entités et espaces de noms.

`https://www.w3.org/TR/xml/`

`https://www.w3.org/TR/xml-names/`

[S4] SearchPresetCatalog.cpp, mainfrm.cpp, FBEview.cpp et search-preset-source-scintilla-smoke.cpp de la même révision définissent le profil et les tâches éditoriales existantes.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/tools/tests/search-preset-source-scintilla-smoke.cpp`

Les recettes supplémentaires doivent être vérifiées dans la version FBE cible. Un test local C++11 ECMAScript n’est pas une exécution de Windows Scintilla. Le README précise les vérifications.
