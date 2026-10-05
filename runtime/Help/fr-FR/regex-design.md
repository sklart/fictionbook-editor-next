# Aide sur les expressions régulières — Design

Note sur la traduction : les mots et phrases russes des exemples de contrôle sont conservés volontairement. Il s’agit de données de test : expressions régulières, chaînes de remplacement, caractères d’espacement et résultats attendus restent identiques à ceux de l’édition russe de référence. Les explications sont traduites ; les exemples ne sont pas automatiquement adaptés aux règles typographiques françaises.

Guide complet de recherche, de remplacement et de relecture des livres dans FictionBook Editor Next.

Édition du 2 octobre 2026. Le mode Code est traité dans regex-source.md. Dans ce guide, « motif » désigne une expression régulière ; un « modèle intégré » est un ensemble enregistré comprenant une expression et ses paramètres dans le panneau Modèles.

## 1. Utiliser ce guide

Une expression régulière décrit une règle de recherche plutôt qu’une chaîne exacte. Par exemple, `[0-9]+` trouve une suite de chiffres de longueur quelconque, tandis que `[ \t]{2,}` trouve au moins deux espaces ordinaires ou tabulations. La correspondance trouvée et le texte qui la remplace sont deux choses différentes.

La section 2 permet un premier essai pratique. Les sections 3 à 15 expliquent la syntaxe ; la section 16 décrit la grammaire de remplacement propre à FBE. La section 17 réunit des recettes pour les livres, avec réglages, textes de contrôle et avertissements. Les dernières sections traitent des problèmes courants, des performances et des sources.

Ne copiez que l’expression dans le champ de recherche. Les mots « Rechercher » et « Remplacer », les indications comme U+0020 et les accents graves Markdown n’appartiennent pas au motif. Les délimiteurs JavaScript `/.../g`, les guillemets d’une chaîne C++ et les doubles barres obliques inverses de JSON sont inutiles.

Les cases à cocher sont importantes. Une correspondance obtenue sans respecter la casse n’est pas nécessairement retrouvée en la respectant. Les réglages font partie de la recette ; ils ne sont pas décoratifs.

« Rechercher et vérifier uniquement » signale une recette diagnostique. Le passage peut être correct : répétition volontaire, nom étranger, citation, titre ou choix typographique. « Remplacer après vérification » ne signifie pas non plus que la modification convient sans risque à tous les livres.

## 2. Première recherche et remplacement prudent

Enregistrez une copie de travail. Passez en Design, ouvrez Rechercher ou Remplacer et activez les expressions régulières. Pour commencer, désactivez Mot entier : mieux vaut exprimer les limites dans le motif. Vérifiez la portée et le sens de recherche.

Pour rechercher plusieurs espaces :

```regex
[ \t]{2,}
```

Dans `Он   пришёл`, la correspondance est l’intervalle de trois espaces. Saisissez une espace ordinaire dans le champ de remplacement. Remplacez une seule occurrence, vérifiez `Он пришёл`, puis envisagez seulement Remplacer tout.

En présence de nombreux résultats, inspectez-les d’abord et corrigez un ou deux cas représentatifs. Après une opération globale, vérifiez texte, italique, gras, notes et limites de paragraphes. Annulez un résultat inattendu avant une nouvelle série de modifications.

Appliquer dans le panneau Modèles transfère l’expression et les options vers le dialogue. Ce n’est pas une commande de correction inconditionnelle de tous les résultats. Utilisez ensuite les commandes de recherche ou de remplacement du dialogue.

## 3. Moteur et limites du mode Design

Ce mode utilise PCRE2-16, avec texte et motifs en UTF-16 ; UTF est toujours activé. FBE construit séparément le texte interrogeable puis effectue les remplacements en tenant compte de la structure du livre. La documentation PCRE2 explique donc les correspondances, mais pas toutes les opérations de FBE. Voir D1–D4.

La recherche porte sur une représentation textuelle du livre, pas sur le balisage XML littéral. `<strong>` ne permet pas de rechercher le texte en gras dans Design. Recherchez les balises en mode Code et utilisez les fonctions structurelles ou des scripts spécialisés pour les transformations.

Le retour visuel dû à la largeur de la fenêtre n’est pas un caractère de fin de ligne. Il faut distinguer paragraphes, sauts réels et retour visuel. FBE utilise des ancres multiligne dans sa représentation de recherche afin de refléter les limites des paragraphes. Trouver une correspondance entre paragraphes et la remplacer sont deux opérations différentes : l’implémentation étudiée refuse les remplacements interparagraphes.

`\A` et `\z` désignent le début et la fin du sujet transmis au moteur. Ce n’est pas automatiquement le début et la fin de tout le FB2 : la portée et le fragment construit par FBE interviennent.

## 4. Unicode : UTF, UCP et casse

### 4.1. Rôle d’UTF

UTF permet de traiter les caractères Unicode, y compris le cyrillique. Il ne translittère pas, ne corrige pas l’OCR, ne transforme pas `ё` en `е` et ne fusionne pas automatiquement les représentations canoniquement équivalentes.

Une lettre accentuée peut être stockée comme un caractère ou comme une lettre suivie d’un signe combinant. L’apparence est proche, mais la recherche caractère par caractère peut différer. PCRE2 n’effectue pas la normalisation NFC/NFD à votre place. Les livres avec diacritiques peuvent nécessiter `\p{M}`.

### 4.2. Rôle d’UCP

Unicode (UCP) modifie les classes abrégées, surtout `\w`, `\d` et `\s`, ainsi que les limites dépendantes `\b` et `\B`. Ce n’est pas l’option qui autorise le cyrillique lui-même.

Précision importante par rapport aux premières versions : les propriétés explicites `\p{L}`, `\p{N}` et `\P{...}` fonctionnent dans une compilation Unicode de PCRE2 même sans UCP. Mais un motif combinant `\p{L}` et `\b` devrait généralement activer UCP pour accorder la définition des lettres et des limites de mots.

Comparez la recherche d’un mot russe :

```regex
\bмир\b
```

Avec UCP, les limites suivent la classe de mot Unicode. Sans UCP, ne supposez pas que `\b` se comporte sur le cyrillique comme sur l’ASCII latin. L’absence de résultat ne prouve pas celle du mot.

Pour des chiffres strictement ASCII, utilisez `[0-9]` plutôt que `\d` : UCP peut étendre ce dernier aux chiffres décimaux d’autres écritures.

### 4.3. Propriétés utiles

| Expression | Élément recherché |
| --- | --- |
| `\p{L}` | Lettre Unicode |
| `\p{Lu}` | Lettre majuscule lorsque la casse est respectée |
| `\p{Ll}` | Lettre minuscule lorsque la casse est respectée |
| `\p{M}` | Signe combinant |
| `\p{N}` | Caractère numérique, notion plus large qu’un chiffre décimal |
| `\p{Nd}` | Chiffre décimal |
| `\p{Latin}` | Caractère de l’écriture latine |
| `\p{Cyrillic}` | Caractère de l’écriture cyrillique |
| `\P{L}` | Caractère qui n’est pas une lettre |

Pour une suite de lettres avec accents combinants :

```regex
[\p{L}\p{M}]+
```

Ce n’est toujours pas une définition linguistique universelle du mot : traits d’union et apostrophes en sont absents. Ajoutez-les de manière délibérée.

### 4.4. La casse est un réglage distinct

Respectez la casse pour détecter des anomalies comme `строчнаяПрописная`, des initiales ou des motifs avec `\p{Lu}` et `\p{Ll}`. Ignorer la casse peut annuler le sens même de la règle. UCP ne remplace pas cette option.

Ignorer temporairement la casse dans une expression :

```regex
(?i)глава
```

Limiter cet effet à un groupe :

```regex
(?i:глава)[ \t]+[0-9]+
```

## 5. Caractères littéraux et échappement

Les lettres et la plupart des signes se représentent eux-mêmes. Hors d’une classe, point, parenthèses, crochets, astérisque, plus, point d’interrogation, accolades, ancres et barre oblique inverse peuvent avoir un sens particulier.

Pour rechercher un métacaractère littéral, faites-le précéder d’une barre oblique inverse :

| Texte à trouver | Expression |
| --- | --- |
| Point | `\.` |
| Point d’interrogation | `\?` |
| Signe plus | `\+` |
| Astérisque | `\*` |
| Parenthèse ouvrante | `\(` |
| Parenthèse fermante | `\)` |
| Nombre entre crochets | `\[[0-9]+\]` |
| Barre oblique inverse elle-même | `\\` |

`(123)` recherche `123` et l’enregistre dans un groupe ; les parenthèses du texte ne sont pas exigées. Pour trouver `(123)`, échappez les parenthèses.

Un long passage littéral peut être encadré par `\Q` et `\E` dans un motif PCRE2 :

```regex
\QЦена (руб.) + доставка\E
```

Ces mêmes séquences ont un autre sens dans le champ de remplacement de FBE. Ne transférez pas automatiquement les règles d’échappement de la recherche au remplacement.

## 6. Espaces, tabulations et caractères invisibles

| Expression | Sens dans la recherche PCRE2 |
| --- | --- |
| `[ ]` | Espace ordinaire U+0020 seulement |
| `[ \t]` | Espace ordinaire ou tabulation |
| `\t` | Tabulation U+0009 |
| `\x{00A0}` | Espace insécable |
| `\x{202F}` | Espace fine insécable |
| `\h` | Espacement horizontal, dont plusieurs espaces Unicode |
| `\s` | Espacement pouvant inclure des fins de ligne |
| `\r` | Retour chariot CR |
| `\n` | Saut de ligne LF |
| `\R` | Séquence de rupture de ligne Unicode |
| `\x{00AD}` | Trait d’union conditionnel |
| `\x{200B}` | Espace sans chasse |
| `\x{FEFF}` | FEFF présent dans le texte |

Pour nettoyer les intervalles ordinaires entre mots, préférez `[ \t]` à `\s`. La seconde classe est plus large et peut toucher les limites de paragraphes et les espaces insécables. De même, `\h` est utile au diagnostic, mais trop large pour un remplacement typographique non précisé.

L’espace insécable lie les parties d’une notation : signe et numéro, initiales et nom, nombre et unité. Les remplacer toutes par des espaces ordinaires peut dégrader la composition. FBE possède aussi un réglage du caractère NBSP ; comparez les recettes U+00A0 avec les paramètres du livre et de la version utilisée.

U+200C et U+200D peuvent être nécessaires à certaines écritures et aux emoji composés. Ne supprimez pas systématiquement tous les caractères « invisibles ». Détecter n’équivaut pas à constater une erreur.

## 7. Classes et plages de caractères

Une classe entre crochets consomme un caractère de son ensemble. `[abc]` signifie l’une des trois lettres, pas le mot `abc`. Pour une ou plusieurs, ajoutez un quantificateur : `[abc]+`.

`[^abc]` consomme un caractère absent de l’ensemble. Il ne vérifie pas qu’« abc ne précède pas le texte ». Les assertions lookaround servent à ces vérifications contextuelles.

Dans une classe, le point et la plupart des parenthèses perdent leur rôle spécial. Le trait d’union peut définir une plage ; placez-le au début ou à la fin, ou échappez-le, pour l’obtenir littéralement. `\]` permet d’inclure un crochet fermant.

```regex
[А-Яа-яЁё-]+
```

Ce motif accepte les lettres russes et le trait d’union, mais pas tout le cyrillique. L’ukrainien, le biélorusse et d’autres langues nécessitent un ensemble plus large ou une propriété Unicode.

Dans des crochets, `\b` n’est pas une limite de mot. N’intégrez pas des limites de mots à un ensemble de caractères ordinaire.

## 8. Ancres, limites et correspondances vides

Une ancre teste une position sans consommer de lettre ni d’espace. `^`, `$` ou `\b` seuls peuvent donc produire une correspondance de longueur nulle, qui ne ressemble pas à une sélection textuelle normale.

| Ancre | Signification |
| --- | --- |
| `^` | Début de ligne en multiligne, sinon début du sujet |
| `$` | Fin de ligne en multiligne ; le traitement de la dernière fin de ligne dépend du mode |
| `\A` | Début du sujet uniquement |
| `\z` | Fin stricte du sujet |
| `\Z` | Fin du sujet ou position précédant sa dernière fin de ligne |
| `\b` | Limite entre caractère de mot et caractère hors mot |
| `\B` | Position qui n’est pas une limite de mot |
| `\G` | Position de départ de l’appel de correspondance courant |

`\G` ne mémorise pas seul la correspondance précédente. Il dépend du décalage initial fourni par l’application. Lors d’appels successifs, celui-ci peut être la fin du résultat précédent, mais ce n’est pas une méthode universelle pour parcourir un livre dans FBE. Préférez des limites explicites en relecture courante. [D1]

Pour trouver un mot plutôt qu’une partie de mots plus longs, utilisez des limites ou des tests de lettres voisines. Avec UCP :

```regex
\bтом\b
```

Le début de `томик` ne correspond pas. Les traits d’union et apostrophes peuvent néanmoins séparer les mots autrement pour le moteur que pour le relecteur.

Employez les résultats vides avec prudence dans Remplacer tout : du texte est inséré à des positions, au lieu de remplacer des caractères visibles. Commencez par des motifs à résultat non vide.

## 9. Quantificateurs : répétition et retour arrière

| Notation | Répétitions de l’élément précédent |
| --- | --- |
| `?` | Zéro ou une |
| `*` | Zéro ou davantage |
| `+` | Une ou davantage |
| `{3}` | Exactement trois |
| `{3,}` | Au moins trois |
| `{2,5}` | De deux à cinq |

Le quantificateur concerne le caractère, la classe ou le groupe précédent. `аб+` répète seulement `б` ; `(?:аб)+` répète les deux lettres.

### 9.1. Recherche gourmande

Texte initial :

```text
«первый» и «второй»
```

Motif :

```regex
«.*»
```

Le point suivi d’un astérisque prend d’abord autant de texte que possible, puis le moteur revient en arrière au besoin pour satisfaire la suite. Ici, le résultat englobe les deux paires de guillemets.

### 9.2. Recherche non gourmande

```regex
«.*?»
```

Elle commence par le minimum de caractères, mais élargit aussi le résultat si nécessaire. Les recherches successives trouvent `«первый»`, puis `«второй»`.

Pour une paire simple, limiter explicitement le contenu est souvent plus clair :

```regex
«[^»\r\n]*»
```

Ce n’est pas une analyse de guillemets imbriqués. Ceux-ci demandent une vérification éditoriale séparée.

### 9.3. Quantificateur possessif

```regex
«.*+»
```

Ce n’est pas un motif de guillemets « encore meilleur ». `.*+` absorbe aussi le guillemet fermant et refuse de le rendre ; le `»` restant du motif ne peut plus correspondre. Aucun résultat sur ce texte.

Une variante excluant le signe fermant peut convenir :

```regex
«[^»\r\n]*+»
```

Utilisez quantificateurs possessifs et groupes atomiques pour maîtriser les retours arrière, pas comme accélération universelle.

## 10. Groupes et références arrière

Les parenthèses mémorisent un fragment. La numérotation commence à 1, selon les parenthèses capturantes ouvrantes de gauche à droite ; l’imbrication ne change pas cet ordre.

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Pour `02.10.2026`, les groupes contiennent `02`, `10` et `2026`. Cela vérifie la forme, non la validité calendaire de la date.

Le groupe non capturant `(?:...)` regroupe sans prendre de numéro. Il aide à conserver des `$1` et `$2` prévisibles dans les remplacements complexes.

Un groupe nommé facilite la lecture :

```regex
(?<word>\p{L}+)[ \t]+\k<word>
```

Une référence arrière recherche le même texte capturé. Un appel de sous-programme, décrit plus loin, répète une règle et peut trouver un autre texte. Ce sont des mécanismes distincts.

Pour une répétition de mot dans un livre, ajoutez limites, réglage de casse et espaces adaptés ; une recette figure plus bas. Sans limites, une référence peut trouver une portion d’un mot plus long.

Les groupes nommés dans la recherche n’ajoutent pas automatiquement des substitutions nommées au remplacement FBE. Utilisez les références numériques confirmées.

## 11. Alternative et groupes atomiques

La barre verticale choisit une branche :

```regex
(?:глава|часть)[ \t]+[0-9]+
```

Le regroupement est important. Sans lui, une partie droite commune peut ne concerner que la dernière branche. Les alternatives sont essayées dans l’ordre écrit ; disposez consciemment les formes longues et courtes.

Un groupe atomique interdit le retour dans un groupe qui a déjà réussi :

```regex
(?>а|аб)в
```

Sur `абв`, la première branche choisit `а`, puis revenir à `аб` est interdit : aucun résultat. Sans atomicité, le motif peut réussir. C’est un exemple pédagogique ; n’ajoutez pas l’atomicité sans vérifier le résultat.

## 12. Tester le contexte : lookaround

Une assertion lookaround vérifie un contexte qui n’entre pas dans la correspondance complète. On peut ainsi sélectionner un numéro tout en conservant son signe précédent.

```regex
(?<=№ )[0-9]+
```

Sur `№ 125`, seul `125` est trouvé. L’espace est ici exactement une espace ordinaire.

Test à droite :

```regex
[0-9]+(?=[ \t]+руб\.)
```

Sur `125 руб.`, le nombre correspond, pas `руб.`.

Assertion négative vers l’avant :

```regex
\bглава\b(?![ \t]+[0-9])
```

Avec UCP, le mot « глава » est recherché lorsqu’il n’est pas suivi d’une numérotation ordinaire séparée par une espace. C’est une sélection contextuelle, non une règle de correction.

L’assertion arrière est `(?<=...)`, sa négation `(?<!...)`. L’assertion avant est `(?=...)`, sa négation `(?!...)`.

Le lookbehind impose des limites de longueur. PCRE2 moderne autorise certaines longueurs variables bornées, pas des répétitions arbitrairement illimitées. Pour des recettes portables, choisissez un contexte court de longueur fixe ou une capture/`\K` ; ne comptez pas sur `.*` dans une assertion arrière.

## 13. Options intégrées au motif

| Option | Effet |
| --- | --- |
| `(?i)` | Ignorer la casse |
| `(?-i)` | Respecter la casse |
| `(?m)` | Ancres de début/fin multiligne |
| `(?-m)` | Désactiver ces ancres multiligne |
| `(?s)` | Autoriser le point à englober une fin de ligne |
| `(?-s)` | Rétablir le comportement habituel du point |
| `(?x)` | Ignorer les espaces non significatifs et les commentaires du motif |

`(?m)` ne fait pas traverser les lignes au point. `(?s)` n’autorise pas les remplacements interparagraphes dans FBE. Il s’agit de deux options de recherche et d’une restriction applicative distincte.

En mode étendu, les espaces du motif peuvent ne plus être littérales. Utilisez `[ ]` pour une espace requise ; `#` hors classe peut commencer un commentaire. Les motifs joliment disposés sur plusieurs lignes conviennent à la documentation, mais les recettes suivantes sont sur une ligne pour le champ de recherche.

## 14. Fonctions avancées de PCRE2

La plupart des corrections n’exigent pas cette section. Elle explique des constructions présentes dans des motifs d’autrui. Le support syntaxique du moteur ne rend pas tout motif pratique ou sûr à l’échelle d’un livre.

### 14.1. Réinitialiser le début de la correspondance

```regex
№[ \t]*\K[0-9]+
```

Sur `№ 125`, le préfixe est vérifié, mais seul `125` est signalé comme résultat. Cela sert à modifier le nombre, pas le signe. N’employez pas `\K` dans un lookaround sans vérification : PCRE2 restreint cette combinaison.

### 14.2. Condition sur la participation d’un groupe

```regex
^(\()?([0-9]+)(?(1)\))$
```

Le motif accepte `12` ou `(12)`, pas `(12` non fermé. La condition vérifie la participation du groupe 1. Ne transformez pas cet exemple en remplacement dépendant de groupes optionnels sans tester l’adaptateur FBE.

### 14.3. Numérotation partagée entre alternatives

```regex
(?|глава ([0-9]+)|часть ([0-9]+))
```

Le numéro est dans le groupe 1 dans les deux branches : c’est le branch reset. Pour les cas simples, un groupe non capturant avec une capture commune reste généralement plus clair.

### 14.4. Appel de sous-programme

```regex
(?<pair>[0-9]{2})-(?&pair)
```

Dans `12-34`, les deux parties respectent la règle « deux chiffres », bien que différentes. Une référence `\k<pair>` exigerait à nouveau `12`.

Les sous-programmes récursifs décrivent certaines structures imbriquées, mais ne remplacent pas le parseur XML de FBE. Utilisez les outils structurels pour `<section>`, `<poem>`, les notes et les tableaux.

### 14.5. Ignorer des fragments

```regex
«[^»\r\n]*»(*SKIP)(*FAIL)|\bслово\b
```

Avec UCP, le mot est recherché hors de paires simples de guillemets russes. La première branche marque la citation à ignorer, la seconde cherche le mot. Les guillemets imbriqués ou non fermés ne sont pas analysés ; ne vous y fiez pas sans réserve pour des corrections globales.

## 15. Ce qu’une expression régulière ne décide pas

« Majuscule après minuscule », « aucun signe final » et « mot répété » sont des indices formels, non des décisions éditoriales. Regex ignore si `да да` est une faute, une réplique volontaire, un vers ou un titre.

N’ajoutez pas automatiquement des points, ne fusionnez pas tous les paragraphes débutant par une minuscule, ne remplacez pas toute lettre latine par du cyrillique et tout trait d’union par un tiret. Une opération complexe exige du contexte, parfois plusieurs étapes DOM. Les scripts FBE de nettoyage, de mots collés et de notes traitent ces tâches plus élaborées.

## 16. Grammaire de remplacement de FBE

PCRE2 recherche, mais FBE interprète le remplacement. Les exemples JavaScript, Python, .NET, PCRE2 substitute ou d’autres éditeurs doivent donc être vérifiés. Les commandes suivantes sont fondées sur le code FBE et le guide initial. [D3, D4]

### 16.1. Correspondance et groupes

| Dans le remplacement | Signification |
| --- | --- |
| `$0` ou `\0` | Correspondance complète |
| `$1` … `$9` | Groupe de numéro correspondant |
| `\1` … `\9` | Autre notation d’un groupe numérique |
| `$+` ou `\+` | Dernier groupe renvoyé par l’adaptateur de correspondance |

« FBE ne remplace que les groupes 1–9 » est imprécis : le texte ordinaire et la correspondance complète peuvent également être insérés. C’est l’accès numérique direct aux groupes qui est limité, pas leur nombre dans PCRE2.

N’utilisez pas `$10` pour le dixième groupe. `${name}` ne fait pas partie de la grammaire confirmée. Placez les captures utiles parmi les neuf premières et rendez les groupes auxiliaires non capturants.

Les groupes optionnels ou vides exigent un essai séparé : FBE possède son propre adaptateur SubMatches. Pour les nouveaux remplacements globaux, évitez de dépendre des subtilités des groupes absents ou de la notion de dernier groupe ; préférez des captures obligatoires explicites.

### 16.2. Réordonner des groupes

Rechercher :

```regex
([0-9]{2})\.([0-9]{2})\.([0-9]{4})
```

Remplacer par :

```text
$3-$2-$1
```

`02.10.2026` devient `2026-10-02`. C’est une démonstration, pas une recommandation de changer toutes les dates du livre.

### 16.3. Modifier la casse

| Commande | Effet |
| --- | --- |
| `\U` | Convertir le fragment inséré en majuscules |
| `\L` | Le convertir en minuscules |
| `\T` | Première lettre en majuscule, les autres en minuscules |
| `\Q` | Réinitialiser les commandes de casse/formatage pour la suite |

Rechercher :

```regex
(иван)
```

Remplacer par :

```text
\T$1\Q
```

Résultat : `Иван`. `\T` n’est pas une capitalisation linguistique de chaque mot : le fragment entier `иван иванов` ne devient pas nécessairement `Иван Иванов`.

N’empilez pas `\U` et `\L` sans remise à zéro. Séparez les portions par `\Q` et vérifiez cyrillique et diacritiques. FBE change la casse, pas un normaliseur Unicode de PCRE2.

### 16.4. Gras et italique

`\S` active le gras du fragment inséré, `\E` l’italique. `\Q` met fin aux commandes actives pour le texte inséré ensuite.

```text
\S$1\Q
```

Ce remplacement formate le groupe 1. Il ne recherche pas du texte déjà gras et n’efface pas toute mise en forme existante. Testez un fragment et l’annulation.

### 16.5. Sens différents en recherche et remplacement

En recherche, `\S` désigne un caractère non blanc ; en remplacement FBE, le gras. En recherche, `\Q...\E` protège un fragment littéral ; en remplacement, `\Q` réinitialise et `\E` active l’italique.

Ne saisissez pas `\n`, `\t`, `\x{00A0}`, `\$` ou `$$` en remplacement Design en attendant le comportement d’un autre éditeur. Ces notations n’appartiennent pas à la grammaire littérale décrite ; une séquence inconnue peut être supprimée. Pour NBSP, utilisez le caractère réel ; pour conserver un signe spécial, utilisez un groupe capturé ou un remplacement ordinaire vérifié.

Un champ vide supprime la correspondance. Un champ contenant une espace remplace par une espace. Ne saisissez pas littéralement les indications `<пусто>`, `[NBSP]` ou `U+00A0`.

## 17. Recettes pratiques de correction et de relecture

Ces scénarios sont autonomes ; leurs noms ne correspondent pas obligatoirement exactement au catalogue intégré. Testez d’abord tout remplacement sur une occurrence. Les espaces et tabulations réelles sont conservées dans les blocs de contrôle. Les contre-exemples illustrent au moins une limite, mais ne remplacent pas la vérification du livre entier.

### D01. Réduire plusieurs espaces ordinaires à une seule

Uniformiser les intervalles ordinaires sans toucher une NBSP isolée.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
[ \t]{2,}
```

Texte de contrôle initial :

```text
Он   пришёл.
```

Remplacer par : une espace ordinaire U+0020. Il s’agit d’un caractère, pas du mot « espace ».

Résultat du remplacement :

```text
Он пришёл.
```

Contre-exemple qui ne doit pas correspondre :

```text
Он пришёл.
```


Limites et remarques : Des espaces multiples peuvent être volontaires dans les vers, tableaux ou faux retraits. Vérifiez la portée ; ce n’est pas une modification des retraits de paragraphes.

### D02. Espaces au début du paragraphe

Supprimer un retrait manuel avant du texte ordinaire.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
^[ \t]+
```

Texte de contrôle initial :

```text
   Начало абзаца.
```

Le même texte de contrôle avec les espaces rendus visibles :

```text
␠␠␠Начало␠абзаца.
```

Ici, ␠ représente une espace ordinaire, [TAB] une tabulation, [NBSP] U+00A0, [NNBSP] U+202F et [ZWSP] U+200B. Cette notation est explicative ; ne pas insérer ces libellés dans le livre.

Remplacer par : laisser le champ entièrement vide. Ne pas saisir le mot « vide ».

Résultat du remplacement :

```text
Начало абзаца.
```

Contre-exemple qui ne doit pas correspondre :

```text
Начало абзаца.
```


Limites et remarques : Les retraits manuels peuvent avoir un sens en poésie et composition artistique. L’ancre désigne une ligne textuelle, pas le retour visuel de la fenêtre.

### D03. Espaces en fin de paragraphe

Supprimer une queue d’espaces ordinaires ou de tabulations.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
[ \t]+$
```

Texte de contrôle initial :

```text
Конец абзаца.
```

Le même texte de contrôle avec les espaces rendus visibles :

```text
Конец␠абзаца.␠␠␠
```

Ici, ␠ représente une espace ordinaire, [TAB] une tabulation, [NBSP] U+00A0, [NNBSP] U+202F et [ZWSP] U+200B. Cette notation est explicative ; ne pas insérer ces libellés dans le livre.

Remplacer par : laisser le champ entièrement vide. Ne pas saisir le mot « vide ».

Résultat du remplacement :

```text
Конец абзаца.
```

Contre-exemple qui ne doit pas correspondre :

```text
Конец абзаца.
```


Limites et remarques : NBSP est volontairement exclue. Utilisez un diagnostic séparé pour les espaces inhabituelles.

### D04. Espace avant virgule ou autre ponctuation

Conserver le signe et supprimer l’intervalle qui le précède.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
[ \t]+([,;:!?])
```

Texte de contrôle initial :

```text
Слово , другое !
```

Remplacer par :

```text
$1
```

Résultat du remplacement :

```text
Слово, другое!
```

Contre-exemple qui ne doit pas correspondre :

```text
Слово, другое!
```


Limites et remarques : Règle prévue pour le texte russe ordinaire. La typographie française prévoit une espace spéciale avant certains signes ; ne l’appliquez pas sans adaptation à un livre français.

### D05. Espace après un signe ouvrant

Supprimer les espaces ordinaires après une parenthèse, un crochet ou un guillemet russe ouvrant.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
([(\[«„])[ \t]+
```

Texte de contrôle initial :

```text
« слово» ( пример)
```

Remplacer par :

```text
$1
```

Résultat du remplacement :

```text
«слово» (пример)
```

Contre-exemple qui ne doit pas correspondre :

```text
«слово» (пример)
```


Limites et remarques : Ne touche les espaces de formules ou d’exemples que si elles suivent immédiatement un signe listé. Le contexte reste à vérifier.

### D06. Espace avant un signe fermant

Supprimer les espaces ordinaires avant une parenthèse, un crochet ou un guillemet fermant.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
[ \t]+([)\]»”])
```

Texte de contrôle initial :

```text
«слово » (пример )
```

Remplacer par :

```text
$1
```

Résultat du remplacement :

```text
«слово» (пример)
```

Contre-exemple qui ne doit pas correspondre :

```text
«слово» (пример)
```


Limites et remarques : Ce n’est pas un normaliseur universel de guillemets ni de conventions mathématiques.

### D07. Exactement trois points en points de suspension

Convertir trois points consécutifs sans toucher une chaîne plus longue.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
(?<!\.)\.{3}(?!\.)
```

Texte de contrôle initial :

```text
Он подумал... и ответил.
```

Remplacer par :

```text
…
```

Résultat du remplacement :

```text
Он подумал… и ответил.
```

Contre-exemple qui ne doit pas correspondre :

```text
Содержание.....12
```


Limites et remarques : Vérifiez la politique éditoriale. Quatre points et les pointillés d’une table des matières ne sont volontairement pas modifiés.

### D08. Points de suspension espacés

Rassembler trois points séparés par des espaces ordinaires.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
(?<!\.)\.[ \t]*\.[ \t]*\.(?!\.)
```

Texte de contrôle initial :

```text
Он подумал. . . и ответил.
```

Remplacer par :

```text
…
```

Résultat du remplacement :

```text
Он подумал… и ответил.
```

Contre-exemple qui ne doit pas correspondre :

```text
Слово. Другое.
```


Limites et remarques : Accepte aussi trois points adjacents. Ne pas utiliser sur des lignes de conduite ou des omissions de citation soumises à des règles particulières.

### D09. Espace insécable après №

Lier le signe de numéro au nombre suivant.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
№[ \t]+([0-9]+)
```

Texte de contrôle initial :

```text
№ 125
```

Remplacer par :

```text
№ $1
```

Résultat du remplacement :

```text
№ 125
```

Contre-exemple qui ne doit pas correspondre :

```text
№125
```


Limites et remarques : Le remplacement contient un vrai U+00A0 entre № et $1. Ne pas le remplacer par la suite imprimée \x{00A0}. Le motif n’insère pas une espace absente.

### D10. Espace insécable après §

Lier le signe de paragraphe à son numéro.

Action : remplacer après vérification.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
§[ \t]+([0-9]+)
```

Texte de contrôle initial :

```text
§ 12
```

Remplacer par :

```text
§ $1
```

Résultat du remplacement :

```text
§ 12
```

Contre-exemple qui ne doit pas correspondre :

```text
§12
```


Limites et remarques : Une véritable NBSP se trouve entre § et $1. Avec une numérotation complexe, vérifiez le fragment effectivement trouvé.

### D11. Répétition possible d’un mot adjacent

Trouver une répétition séparée par des espaces horizontales sans franchir les paragraphes.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — désactivé.

Rechercher :

```regex
\b(\p{L}+)[ \t\x{00A0}]+\1\b
```

Texte de contrôle initial :

```text
Это это уже было.
```

Correspondance attendue :

```text
Это это
```

Contre-exemple qui ne doit pas correspondre :

```text
Это уже было.
```


Limites et remarques : « Да да » et d’autres répétitions peuvent être voulues. Ne supprimez pas automatiquement la seconde occurrence : suppression, virgule ou absence de correction dépendent du contexte. Apostrophes et traits d’union ne sont pas entièrement traités.

### D12. Latin et cyrillique dans un même mot

Détecter un mélange OCR à l’intérieur d’un mot continu, pas toute ligne bilingue.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
(?<![\p{L}\p{M}])(?=[\p{L}\p{M}]*\p{Latin})(?=[\p{L}\p{M}]*\p{Cyrillic})[\p{L}\p{M}]+(?![\p{L}\p{M}])
```

Texte de contrôle initial :

```text
В слове Тeст латинская e.
```

Correspondance attendue :

```text
Тeст
```

Contre-exemple qui ne doit pas correspondre :

```text
Он прочитал Latin.
```


Limites et remarques : Dans Тeст, e est latin. Un mot entièrement latin voisin d’un mot russe n’est pas considéré mélangé. Formules, noms de produits et jeux typographiques peuvent être valides. Aucune translittération.

### D13. Minuscule immédiatement suivie d’une majuscule

Trouver une possible fusion de mots ou erreur de casse.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
\p{Ll}\p{Lu}
```

Texte de contrôle initial :

```text
ОнвышелИздома.
```

Correspondance attendue :

```text
лИ
```

Contre-exemple qui ne doit pas correspondre :

```text
Он вышел из дома.
```


Limites et remarques : Respecter la casse est indispensable. Des marques et noms comme McDonald peuvent être corrects. Le résultat indique une transition, pas une séparation de mots reconstruite.

### D14. Chiffre entre lettres

Trouver une substitution OCR courante d’une lettre par un chiffre.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
\p{L}+[0-9]+\p{L}+
```

Texte de contrôle initial :

```text
Это сл0во.
```

Correspondance attendue :

```text
сл0во
```

Contre-exemple qui ne doit pas correspondre :

```text
В главе 10 текст.
```


Limites et remarques : H2O et d’autres formules peuvent être correctes. Ne remplacez pas tous les 0 par о ni tous les 3 par з.

### D15. Ponctuation dans une séquence de lettres

Vérifier un signe immédiatement entouré de lettres.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
\p{L}+[.,;:!?]\p{L}+
```

Texte de contrôle initial :

```text
Он,сказал слово.
```

Correspondance attendue :

```text
Он,сказал
```

Contre-exemple qui ne doit pas correspondre :

```text
Он, сказав слово, ушёл.
```


Limites et remarques : Abréviations, adresses et domaines peuvent aussi correspondre. Pour les virgules seulement, réduisez la classe à [,]. N’insérez pas une espace dans tous les résultats d’un seul coup.

### D16. Paragraphe commençant par une minuscule

Repérer une éventuelle rupture de paragraphe superflue.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
^[ \t]*\p{Ll}
```

Texte de contrôle initial :

```text
продолжение предложения.
```

Correspondance attendue :

```text
п
```

Contre-exemple qui ne doit pas correspondre :

```text
Начало предложения.
```


Limites et remarques : Vers, légendes, listes et citations commencent souvent légitimement par une minuscule. Le motif ne fusionne pas les paragraphes et ne comprend pas le contexte voisin.

### D17. Absence de ponctuation finale avant les guillemets fermants

Trouver une fin alphabétique ou numérique suivie seulement de signes fermants et d’espaces.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
[\p{L}\p{N}][»”")\]}]*[ \t\x{00A0}]*$
```

Texte de contrôle initial :

```text
«Он пришёл»
```

Correspondance attendue :

```text
л»
```

Contre-exemple qui ne doit pas correspondre :

```text
«Он пришёл!»
```


Limites et remarques : Titres et légendes se passent souvent de point. Un appel de note après une ponctuation correcte peut produire un faux positif. Contrairement au seul dernier », ce motif ne signale pas «Он пришёл!».

### D18. Minuscule après une fin de phrase

Trouver une probable erreur de casse après point, interrogation ou exclamation.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
[.!?…][ \t]+[«„“"(\[]?\p{Ll}
```

Texte de contrôle initial :

```text
Он пришёл. потом ушёл.
```

Correspondance attendue :

```text
. п
```

Contre-exemple qui ne doit pas correspondre :

```text
Он пришёл. Потом ушёл.
```


Limites et remarques : Les points d’abréviation et les suspensions d’auteur ne terminent pas toujours une phrase. Il s’agit seulement de candidats à examiner.

### D19. Point éventuellement manquant

Trouver le passage d’une terminaison minuscule à un mot capitalisé séparé par une espace.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
\p{Ll}[»”]?[ \t]+[«„“]?\p{Lu}\p{Ll}+
```

Texte de contrôle initial :

```text
Он пришёл Потом ушёл.
```

Correspondance attendue :

```text
л Потом
```

Contre-exemple qui ne doit pas correspondre :

```text
Он пришёл потом ушёл.
```


Limites et remarques : Les noms propres et titres internes à une phrase donnent de nombreux résultats valides. La recette ne décide pas entre point, virgule ou absence de signe.

### D20. Paires de guillemets droits

Trouver un fragment simple entre guillemets doubles droits sur une ligne.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
"([^"\r\n]+)"
```

Texte de contrôle initial :

```text
Он сказал "да".
```

Correspondance attendue :

```text
"да"
```

Contre-exemple qui ne doit pas correspondre :

```text
Он сказал «да».
```


Limites et remarques : Vérifiez imbrication, pouces, code et système de guillemets choisi. «$1» n’est adapté qu’à un contexte sélectionné, pas à une conversion typographique universelle.

### D21. Trait d’union ou tiret cadratin entre nombres

Repérer un possible intervalle à soumettre à une décision éditoriale.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
(?<![0-9])[0-9]+[ \t]*[-—][ \t]*[0-9]+(?![0-9])
```

Texte de contrôle initial :

```text
Страницы 12 - 15.
```

Correspondance attendue :

```text
12 - 15
```

Contre-exemple qui ne doit pas correspondre :

```text
Страницы 12–15.
```


Limites et remarques : Une date 2026-10-02, un nombre négatif ou une soustraction peuvent aussi correspondre. Ne les convertissez pas automatiquement en intervalles. – est demi-cadratin, — cadratin, - trait d’union.

### D22. Deux initiales avant le nom

Trouver une notation simple à deux initiales pour en vérifier les espaces.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
\b(\p{Lu})\.[ \t]*(\p{Lu})\.[ \t]+(\p{Lu}\p{Ll}+)\b
```

Texte de contrôle initial :

```text
И.О. Иванов
```

Correspondance attendue :

```text
И.О. Иванов
```

Contre-exemple qui ne doit pas correspondre :

```text
Иванов Иван
```


Limites et remarques : Tous les noms composés et diacritiques ne sont pas couverts. Après sélection, on peut employer $1., NBSP, $2., NBSP, $3 ; insérer de vraies espaces insécables, pas leurs noms.

### D23. Nom avant deux initiales

Trouver l’ordre inverse du nom.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
\b(\p{Lu}\p{Ll}+)[ \t]+(\p{Lu})\.[ \t]*(\p{Lu})\.
```

Texte de contrôle initial :

```text
Иванов И.О.
```

Correspondance attendue :

```text
Иванов И.О.
```

Contre-exemple qui ne doit pas correspondre :

```text
Иванов Иван
```


Limites et remarques : Contrôle de forme, pas identification d’une personne. Noms composés, particules et trois initiales exigent une autre règle.

### D24. Caractères invisibles à examiner

Trouver trait d’union conditionnel, espace sans chasse ou FEFF.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
[\x{00AD}\x{200B}\x{FEFF}]
```

Texte de contrôle initial :

```text
сло​во
```

Le même texte de contrôle avec les espaces rendus visibles :

```text
сло[ZWSP]во
```

Ici, ␠ représente une espace ordinaire, [TAB] une tabulation, [NBSP] U+00A0, [NNBSP] U+202F et [ZWSP] U+200B. Cette notation est explicative ; ne pas insérer ces libellés dans le livre.

Correspondance attendue :

```text
​
```

Contre-exemple qui ne doit pas correspondre :

```text
слово
```


Limites et remarques : Le résultat est invisible : U+200B se situe entre о et в. Ne l’insérez ou ne le supprimez qu’après avoir déterminé son rôle. BOM physique et FEFF dans le texte sont distincts.

### D25. Espaces Unicode inhabituelles

Trouver espaces fines, larges et autres espaces spéciales.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
[\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}]
```

Texte de contrôle initial :

```text
10 000
```

Le même texte de contrôle avec les espaces rendus visibles :

```text
10[NNBSP]000
```

Ici, ␠ représente une espace ordinaire, [TAB] une tabulation, [NBSP] U+00A0, [NNBSP] U+202F et [ZWSP] U+200B. Cette notation est explicative ; ne pas insérer ces libellés dans le livre.

Correspondance attendue :

```text
 
```

Contre-exemple qui ne doit pas correspondre :

```text
10 000
```


Limites et remarques : Une espace fine insécable entre groupes de chiffres peut être parfaitement correcte. La recette révèle une hétérogénéité ; elle ne déclare pas chaque caractère erroné.

### D26. Interrogations et exclamations répétées

Repérer une ponctuation expressive ou accidentellement doublée.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
[!?]{2,}
```

Texte de contrôle initial :

```text
Что?! Правда!!!
```

Correspondances attendues, successivement :

```text
?!
!!!
```

Contre-exemple qui ne doit pas correspondre :

```text
Что? Правда!
```


Limites et remarques : ?! et les répétitions d’auteur peuvent être intentionnels. L’action normale est l’examen, pas la réduction de toutes les chaînes à un signe.

### D27. Х cyrillique près de chiffres romains

Trouver un Х russe dans une notation autrement composée de signes romains latins.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — inutile pour cette expression. « Respecter la casse » — activé.

Rechercher :

```regex
(?<=[IVXLCDM])Х|Х(?=[IVXLCDM])
```

Texte de contrôle initial :

```text
Глава IХ
```

Correspondance attendue :

```text
Х
```

Contre-exemple qui ne doit pas correspondre :

```text
Глава IX
```


Limites et remarques : Х est cyrillique et X latin. Vérifiez casse et contexte ; le nombre romain entier n’est pas validé.

### D28. Plusieurs majuscules avant une minuscule

Repérer une possible erreur OCR de casse au début d’un mot.

Action : rechercher et vérifier uniquement.

« Expression régulière » — activée ; « Mot entier » — désactivé. « Unicode (UCP) » — activé. « Respecter la casse » — activé.

Rechercher :

```regex
\p{Lu}{2,}\p{Ll}+
```

Texte de contrôle initial :

```text
Он сказал ПРИвет.
```

Correspondance attendue :

```text
ПРИвет
```

Contre-exemple qui ne doit pas correspondre :

```text
Он сказал Привет.
```


Limites et remarques : Noms, abréviations avec suffixes et sigles latins peuvent être corrects. Ne changez pas globalement la casse sans examen.

## 18. Problèmes courants et diagnostic

### 18.1. Le texte est visible, mais ne correspond pas

Vérifiez mode Design, option regex, casse, portée, direction et caractères réels. `a` latin et `а` cyrillique se ressemblent, mais diffèrent. NBSP n’est pas une espace ordinaire ; guillemets typographiques et droits diffèrent.

Vérifiez UCP pour les limites russes et la casse pour les majuscules/minuscules. N’ajoutez pas inutilement Mot entier à des limites déjà complexes dans le motif.

### 18.2. Un fragment trop grand est trouvé

Soupçonnez d’abord le point-étoile gourmand ou une classe négative trop large. Remplacez « n’importe quel texte » par un ensemble explicite et limitez longueur et bornes. Vérifiez dotall.

### 18.3. Les espaces englobent des paragraphes

`\s+` n’est pas un synonyme d’espace ordinaire. Commencez par `[ \t]+` entre mots et ajoutez NBSP séparément seulement si nécessaire.

### 18.4. Chiffres inattendus, barres perdues ou nom de groupe ignoré

Consultez la grammaire FBE de la section 16. `${name}`, `$10`, `\n` et `\x{...}` ne suivent pas automatiquement les règles de recherche ou d’un autre éditeur. La capture requise doit exister et ne pas être seulement optionnelle dans une autre branche.

### 18.5. Alphabets mélangés dans une phrase bilingue normale

Un ancien exemple vérifiait latin et cyrillique dans toute la ligne et trouvait aussi `Он прочитал Latin.`. D12 limite les deux tests à un mot. C’est la différence essentielle entre une anomalie OCR et une ligne bilingue.

### 18.6. « Pas de point » signale une citation correcte

Le dernier caractère seul ne suffit pas : `»` peut suivre `!`. D17 tient compte des fermetures, mais titres et appels de notes demandent toujours vérification.

### 18.7. La structure recherchée reste introuvable

Une regex textuelle ne voit pas le DOM comme un script structurel. Balises, imbrication, liens vers des IDs absents et déplacement de notes sont d’autres tâches. L’outil approprié peut être Code, le validateur FB2 ou un script, pas une expression encore plus complexe.

## 19. Performances et grands livres

Commencez par une condition précise : signe, classe, mot ou début de paragraphe. Évitez les répétitions illimitées imbriquées et plusieurs fragments concurrents de « texte quelconque ». Un échec peut provoquer un très grand nombre d’essais.

La non-gourmandise n’est pas un remède universel : elle explore aussi des possibilités. Exclure explicitement un séparateur, borner la longueur et réduire la portée sont souvent plus efficaces.

Si la recherche devient longue, ne lancez pas d’autres remplacements par-dessus. Simplifiez puis testez sur un court texte. Une erreur de limite de ressources PCRE2 ne signifie pas « aucune correspondance ».

Pour des opérations en plusieurs étapes impliquant paragraphes voisins et balises, un script est souvent plus clair et sûr. N’unissez pas toutes les règles dans une immense alternative : des règles distinctes expliquent chaque résultat.

## 20. Contrôles avant enregistrement

Inspectez début, milieu et fin de la zone traitée. Vérifiez plusieurs résultats de chaque type, surtout guillemets, intervalles, initiales, notes et mise en forme conservée. Assurez-vous que les NBSP utiles et les paragraphes n’ont pas changé inopinément.

Après des opérations sensibles à la structure, lancez la validation FBE. Enregistrez, rouvrez au besoin et comparez. Une recherche regex réussie ne remplace ni validation FB2 ni relecture éditoriale.

## 21. Sources et champ d’application

Cette traduction reprend le guide russe enrichi issu du fichier fourni regex-design.md. Les explications et contrôles ont été rédigés à nouveau ; les imprécisions sur UCP, les limites du sujet, les alphabets et le remplacement ont été corrigées à partir de sources primaires. Les recettes sont des scénarios éditoriaux, pas des citations du manuel PCRE2.

[D1] Syntaxe officielle PCRE2 : ancres, groupes, propriétés Unicode, retours arrière et options.

`https://www.pcre.org/current/doc/html/pcre2pattern.html`

[D2] Documentation officielle Unicode de PCRE2.

`https://www.pcre.org/current/doc/html/pcre2unicode.html`

[D3] Compatibilité PCRE2 et adaptateur FBE Next, révision d2257405d95b0328649acee64b38829b40a4314b.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/docs/pcre2-compatibility.md`

[D4] GetReplStr, PrepareRegexReplacementText et recherche/remplacement dans FBEview.cpp ; SearchPresetCatalog.cpp et search-preset-design-fixtures.cpp de la même révision.

`https://github.com/sklart/fictionbook-editor-next/blob/d2257405d95b0328649acee64b38829b40a4314b/src/fbe/FBEview.cpp`

Syntaxe du moteur, possibilités de l’interface et justesse éditoriale sont trois niveaux. Les tests locaux ne garantissent pas toute version de FBE sur tout document. Le README de l’archive indique l’état des vérifications.
