# Instructions de contexte pour le projet Wind Waker Multiplayer HD

## Mission

Porter le mod multijoueur historique de The Legend of Zelda: The Wind Waker
vers WWHDRecomp sans casser le protocole réseau ni supprimer le travail
existant.

Lire `WWHD-ONLINE/docs/multiplayer/context.md` (chemin depuis la racine WWONLINE) avant toute
modification. Ce fichier contient
l'inventaire, les contraintes, les dépendances connues et l'ordre de migration.

## Contraintes impératives

1. Ne jamais inventer une fonction, une adresse, un hook, une structure ou une
   API réseau.
2. Vérifier toute fonction HD dans `ZeldaWWHDRecomp/runtime/guest/include`,
   `ZeldaWWHDRecomp/tools/recomp`, les sources de décompilation ou la
   documentation du dépôt.
3. Ne jamais porter directement une adresse `0x80xxxxxx` GameCube/Dolphin vers
   WWHDRecomp.
4. Ne pas supprimer les fichiers de `WindWakerClient` ou `WindWakerServer`.
5. Préserver le protocole historique jusqu'à preuve qu'une modification est
   nécessaire.
6. Séparer le codec et le modèle réseau des structures internes du jeu.
7. Ne pas déclarer une fonctionnalité terminée sans build et test.
8. Signaler explicitement les dépendances externes absentes, notamment
   `com.kirbymimi.mmb` et `com.kirbymimi.serverCommon`.
9. Ne pas ajouter de contenu provenant des fichiers de jeu ou de clés.
10. Traiter Android comme non supporté pour les guest mods tant que la
    documentation WWHDRecomp l'indique.

## Architecture à respecter

```text
protocole réseau historique
        |
modèle NetworkState indépendant du jeu
        |
adaptateur WWHD documenté
        |
SDK guest / runtime host WWHDRecomp
```q

Le code réseau ne doit pas connaître `fopAc_ac_c`, `daPy_lk_c`,
`g_dComIfG_gameInfo` ou les vtables. Ces dépendances appartiennent à
l'adaptateur moteur.

## SDK WWHDRecomp connu

Les primitives documentées sont :

- `WWHD_HOOK`
- `WWHD_HOOK_RETURN`
- `WWHD_REPLACE`
- `WWHD_GAME_FUNC`
- `WWHD_GAME_ORIGINAL`
- `WWHD_GAME_DATA`
- `wwhd_log`
- `wwhd_config_*`
- `wwhd_malloc` / `wwhd_free`
- `wwhd_file_*`
- `wwhd_logic_dt`
- `wwhd_logic_step`

Le SDK guest ne fournit pas actuellement de socket TCP/UDP ou d'IPC
personnalisé. Toute intégration réseau doit donc passer par un service host
réellement ajouté et enregistré dans le runtime, ou par une API officielle
documentée ultérieurement.

## Protocole historique à préserver

- serveur Java sur le port `10014` ;
- byte order big-endian ;
- flux VLQ ;
- paquets de création, mise à jour et suppression d'acteurs ;
- tables d'initialisation ;
- mises à jour de sauvegarde validées par le serveur ;
- ownership et tickets ;
- compatibilité avec le modèle de composants existant.

## Workflow de modification

Avant de coder :

1. lire `context.md` ;
2. rechercher le symbole et ses usages ;
3. lire les headers et appels environnants ;
4. vérifier la signature dans le SDK ou les sources ;
5. identifier les tests pertinents ;
6. proposer ou réaliser une modification limitée.

Après avoir codé :

1. vérifier les diagnostics ;
2. lancer le test ciblé ;
3. lancer le build ciblé ;
4. documenter tout test impossible et sa raison ;
5. ne pas masquer les erreurs par un fallback silencieux.

## Priorité d'implémentation

1. codec et tests de compatibilité ;
2. modèle de snapshot ;
3. lecture de l'état local de Link dans `daPy_Execute` ;
4. transport host vérifié ;
5. marqueur distant minimal ;
6. joueur distant Link ;
7. inventaire, progression, ennemis et apparences.

Ne pas commencer par l'injection de texture, la création d'un second Link ou
la synchronisation brute de la sauvegarde sans preuve des APIs et layouts HD.

## Documentation continue

Mettre à jour `context.md` lorsque l'un des éléments suivants change :

- une signature ou une adresse HD est vérifiée ;
- une dépendance est ajoutée ou retirée ;
- une décision d'architecture est prise ;
- un test est exécuté ;
- une limitation est levée ;
- un risque moteur est confirmé ou résolu.

Les notes doivent distinguer clairement :

- vérifié dans les sources ;
- déduit mais non encore testé ;
- bloqué par une dépendance ;
- implémenté et testé.

## Leçons vérifiées (mod 0.3.16 à 0.3.51)

Le détail, version par version, est dans `context.md`. Ce qui doit guider tout
travail futur :

- **Adresses** : le mod et le décompilé utilisent les adresses USA. Les journaux,
  les crashs et le code généré (`build/gen/code_*.c`, commentaires
  `/* ADDR: */`) utilisent les adresses EU. Convertir avec les `code_steps` de
  `tools/recomp/builds/eu.json`. Le nom `f_<USA>` du code généré est l'adresse
  USA, son `PPC_ENTER` l'adresse EU.
- **Arité** : avant un `WWHD_REPLACE`, lire le début de la fonction dans le
  code généré (registres lus, arguments transmis), même si le décompilé donne
  une signature.
- **Mémoire guest** : le runtime donne la MEM2 jusqu'à `0x60000000`
  (`mem::kMem2End`, 0x50000000 sur console). Toute borne de pointeur doit
  utiliser cette fin. En 0.3.50, une garde du mod bornée à 0x50000000
  (`puppet.h draw_ptr`) rendait invisible tout modèle au-delà, ce qui a été
  pris à tort pendant 15 versions pour une limite du jeu ou du GPU. Chercher
  d'abord dans le mod une constante qui contredit la nouvelle disposition.
- **Tas du jeu** : ne pas agrandir les tas du jeu (essais 0.3.25 à 0.3.37 :
  crashs de démarrage, organisation fragile). Les skins ont leur propre tas
  (`heap_grow.h`, mode 1 par défaut) : arène sead + 256 Mio, tas « MpSkinHeap »
  créé juste après le 12e tas racine, avant `FrameworkCafe` qui prend tout le
  reste. Les archives « MpSk_ » y sont lues (`selectObjectHeap` 02607F80) **et**
  montées (`selectStageHeap` 02604094, qui sinon rend le command heap de
  14 Mio). Une archive se charge dans tout le plus grand bloc libre de son
  parent, puis le bloc est réduit : toujours raisonner en plus grand bloc
  libre, pas en place totale.
- **Shaders des skins** : les `.sharcfb` identiques à ceux du `Link.szs` du jeu
  sont retirés des skins (launcher `SkinImporter.shrinkSkin`,
  `make_skin.py --game`). Le mod les lit dans l'archive « Link »
  (`skin.h skin_get_file`, `ResMgr_getFile2` 026124F4). Six skins différents
  ont été affichés en même temps (testé en 0.3.51).
- **Tests en jeu** : chaque nouvelle version du mod doit être reconfirmée dans
  le gestionnaire de mods (« native code you have not confirmed »), sinon elle
  n'est pas chargée. Ne pas charger de savestate pour tester des changements
  de mémoire ou de modèles : des modèles absents venaient d'un savestate
  chargé, pas du changement testé.
- **Journal** : `instances/<n>/wwhd.log` (racine de l'instance) est le journal
  complet. `captures/wwhd.log` n'est qu'un tampon des dernières lignes, écrit à
  l'ouverture de l'overlay ou à la sortie.
- **Cinématiques** : on ne force pas un event chez les autres, il est joué par
  ses acteurs locaux. On partage la cause (la position du joueur qui le
  lance), on exclut les actions personnelles (portes, coffres, dialogues,
  objets, warp, chutes) et on revérifie la zone au moment de placer.
- **Copies d'acteurs portés** : le jeu place un objet porté avec
  `dComIfGp_getPlayer(0)`. Une copie portée par un autre joueur doit tourner
  avec son puppet comme joueur 0 (`actor_sync.h as_swap_in`).

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Mise en place (vérifiée le 2026-10-23, graphify 0.9.94) : le graphe couvre toute la racine WWONLINE
(WWHD-ONLINE, WindWakerServer, WindWakerClient, mmb, MMBServerCommon), extrait depuis la racine avec
`graphify extract . --code-only` (analyse AST locale, rien n'est envoyé, environ 5 min). Il est
rangé dans `WWHD-ONLINE/graphify-out/` ; `WWONLINE/graphify-out` est une jonction Windows vers ce
dossier, si bien que toutes les commandes graphify se lancent depuis la racine. `graph.json` (environ
207 Mo, plus que la limite de 100 Mo de GitHub) et le cache ne sont pas poussés : seul
`GRAPH_REPORT.md` l'est (`graphify-out/.gitignore`). Chaque machine reconstruit le graphe. Les
exclusions sont dans `WWONLINE/.graphifyignore` (copie suivie : `docs/multiplayer/graphifyignore`).
Sur une autre machine, depuis WWONLINE : `python -m pip install --user uv`, puis
`python -m uv tool install --python 3.12 graphifyy` et `python -m uv tool update-shell` ; copier
`WWHD-ONLINE/docs/multiplayer/graphifyignore` en `.graphifyignore` ; créer la jonction
(`cmd /c mklink /J graphify-out WWHD-ONLINE\graphify-out`) ; `graphify install --project
--platform claude` ; `graphify extract . --code-only` ; `graphify cluster-only . --no-viz --no-label`
pour le rapport. Après une modification de code : `graphify update .`.

Rules:
- When graphify-out/graph.json exists, run `graphify query "<question>"` before your first grep, search or source-file read for a codebase question, and put this rule in every subagent prompt that explores code. Read raw files only for lines the graph or an earlier read already located. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
- A committed graphify-out/graph.json is rewritten whole on every build, so a large diff to it in a pull request is expected. Do not flag it as churn or suggest skipping the graph rebuild or its hooks.
