# Contexte du projet Wind Waker Multiplayer HD

## Objectif

Porter le mod multijoueur existant de The Legend of Zelda: The Wind Waker
GameCube vers la version Wii U recompilée par WWHDRecomp, en conservant autant
que possible le protocole réseau, le serveur Java et le modèle de données
multijoueur.

Le port doit remplacer les mécanismes spécifiques à l'ancien client injecté
dans Dolphin par une intégration fondée sur le SDK guest et le runtime host de
WWHDRecomp.

## Dépôts présents dans le workspace

- `WindWakerClient/` : ancien client C++ injecté dans la version GameCube.
- `WindWakerServer/` : serveur Java et interfaces JavaFX.
- `ZeldaWWHDRecomp/` : source du port recompilé WWHD.
- `WindWakerHD-v0.3.0-windows-x86_64/` : distribution/runtime précompilée.
- `DATA/` : données locales du jeu. Ne jamais ajouter ni redistribuer de
  fichiers de jeu.

Les trois projets principaux sont des dépôts Git imbriqués. Le dépôt parent
`WindWakerRecomp` n'est pas lui-même un dépôt Git.

## Règles de conservation

- Ne pas réécrire le protocole réseau sans justification et tests de
  compatibilité.
- Ne pas supprimer les fichiers de l'ancien projet.
- Ne pas réutiliser les adresses GameCube/Dolphin dans WWHDRecomp.
- Ne pas inventer de fonctions, hooks ou structures HD.
- Toute adresse ou signature HD doit provenir des headers générés, des sources
  publiques de décompilation ou de la documentation du dépôt.
- Ne pas considérer une fonctionnalité comme fonctionnelle avant compilation et
  test.
- Ne pas inclure de ROM, RPX, DOL, texture, modèle, sauvegarde ou clé dans un
  commit.

## Ancien client : éléments importants

### Réseau et sérialisation

- `WindWakerClient/src/mmb++/net/`
- `WindWakerClient/src/mmb++/stream/`
- `WindWakerClient/src/Socket/`
- `WindWakerClient/src/WWClient/`
- `WindWakerClient/src/resourceClient/`

Le format utilise notamment des flux big-endian, des entiers VLQ et des
identifiants de paquets déclarés par le client et le serveur.

### Modèle multijoueur

- `WindWakerClient/src/ActorSync/ActorSync.cpp`
- `WindWakerClient/src/ActorSync/SyncedActor.cpp`
- `WindWakerClient/src/ActorSync/SyncedActor.h`
- `WindWakerClient/src/ActorSync/ActorSyncConfig.cpp`
- `WindWakerClient/src/ActorSync/Components/`

Le modèle contient les acteurs synchronisés, l'ownership, les tickets, les
UID de jeu, les composants, les mises à jour partielles et les factories.

### Dépendances moteur à isoler

- `fopAc_ac_c`
- `daPy_lk_c`
- `dComIfGp_getPlayer`
- `g_dComIfG_gameInfo`
- `fopAcM_fastCreate`
- `fopAcM_delete`
- itérateurs de processus
- vtables et layouts JSystem
- pointeurs mémoire fixes `0x80xxxxxx`
- trampolines et `asm volatile`

Les fichiers concernés sont principalement :

- `WindWakerClient/src/Hooks.cpp`
- `WindWakerClient/src/ActorSync/ActorSyncConfig.cpp`
- `WindWakerClient/src/ActorSync/ActorSpecifics.cpp`
- `WindWakerClient/src/ActorSync/Components/ActorBaseSync.cpp`
- `WindWakerClient/src/ActorSync/Components/ActorHealthSync.cpp`
- `WindWakerClient/src/ActorSync/Components/ActorPlayerSync.cpp`
- `WindWakerClient/src/SaveServer/SaveServer.cpp`

### Ancienne injection

`WindWakerClient/project.py`, `data/symbols.ld`, les patchs DOL, les hooks
`hook_branch*`, les trampolines et les adresses Dolphin sont historiques. Ils
servent de référence comportementale uniquement et ne doivent pas être
portés tels quels.

## Ancien serveur Java

### Entrées

- `WindWakerServer/src/main/java/com/kirbymimi/windwakerserver/WWServer.java`
- `WindWakerServer/src/main/java/com/kirbymimi/windwakerserver/WWClient.java`
- `WindWakerServer/pom.xml`

Le serveur écoute actuellement sur le port `10014` et utilise le byte order
big-endian.

### Synchronisation serveur

- `ActorFilter.java`
- `components/ActorBaseSync.java`
- `components/ActorHealthSync.java`
- `components/ActorPlayerSync.java`
- `components/BokoblinSync.java`
- `components/ChuChuSync.java`
- `components/PropsSync.java`

### Sauvegarde

- `save/SaveServer.java`
- `save/WWSave.java`
- `save/SaveMapGenerator.java`

Le serveur valide les offsets reçus avant d'appliquer une modification et
diffuse les changements acceptés aux autres clients. Cette propriété doit être
conservée.

### Dépendances manquantes du workspace

Le serveur importe des classes de :

- `com.kirbymimi.mmb`
- `com.kirbymimi.serverCommon`

Leur source ou leurs artefacts doivent être fournis avant de pouvoir certifier
une compilation complète du serveur.

## Launcher Java

`LauncherUI.java` et `DolphinMemoryManager.java` appartiennent à l'ancien
workflow Dolphin. Ils peuvent fournir des idées d'interface pour le pseudo,
l'adresse serveur et les couleurs, mais leurs fonctions de scan MEM1,
`ReadProcessMemory`, `WriteProcessMemory` et d'injection ne sont pas valides
pour WWHDRecomp.

## WWHDRecomp : SDK vérifié

Fichiers principaux :

- `ZeldaWWHDRecomp/runtime/guest/include/wwhd_guest.h`
- `ZeldaWWHDRecomp/runtime/guest/include/wwhd/functions.h`
- `ZeldaWWHDRecomp/runtime/guest/include/wwhd/bindings.h`
- `ZeldaWWHDRecomp/runtime/guest/include/wwhd/link.h`
- `ZeldaWWHDRecomp/runtime/guest/include/wwhd/actor.h`
- `ZeldaWWHDRecomp/runtime/guest/include/wwhd/objects.h`
- `ZeldaWWHDRecomp/runtime/guest/include/wwhd/data.h`
- `ZeldaWWHDRecomp/runtime/guest/include/wwhd/save.h`
- `ZeldaWWHDRecomp/docs/mod-sdk-v2.md`

Le SDK permet notamment :

- `WWHD_HOOK`
- `WWHD_HOOK_RETURN`
- `WWHD_REPLACE`
- `WWHD_GAME_FUNC`
- `WWHD_GAME_ORIGINAL`
- `WWHD_GAME_DATA`
- accès aux services host documentés : logs, configuration, fichiers, heap,
  entrée et compteur de logique

L'exemple `examples/guest-mods/heart-ticker` confirme qu'un hook de
`WWHD_ADDR_daPy_Execute` peut lire et modifier des données de sauvegarde
exposées par le SDK.

## Limitation réseau actuelle

Le SDK guest public ne fournit pas de socket TCP/UDP, de service IPC arbitraire
ou de callback réseau. Le transport ne doit donc pas être ajouté dans un
`mod.c` en inventant une API.

La stratégie cible est :

```text
serveur Java existant
        |
transport host ajouté au runtime WWHDRecomp
        |
file de snapshots réseau
        |
adaptateur guest/HD appelé depuis les hooks de logique
        |
état NetworkState indépendant du moteur
```

Une modification du runtime host sera nécessaire pour exposer le transport au
mod, sauf si une API officielle est ajoutée ultérieurement au SDK.

Le premier transport host est maintenant implémenté dans
`runtime/src/network_transport.{h,cpp}`. Il utilise TCP via Winsock sous
Windows et les sockets POSIX sous Linux/macOS, avec une connexion dans un
thread dédié, des files entrante/sortante bornées par trame et un parsing du
framing historique. Les imports `wwhd_net_*` ont été ajoutés au SDK guest :
le guest fournit une trame complète au host et ne manipule jamais un descripteur
de socket. Ce transport ne certifie pas encore la compatibilité du handshake
Java ni les IDs dynamiques : il fournit seulement le canal fiable nécessaire.
Le premier élément reçu est traité comme le paquet d'initialisation historique
`VLQ(length) + payload`; les éléments suivants sont des trames normales
`VLQ(packet_id) + VLQ(payload_length) + payload`. Il n'est pas exposé sur
Android tant que le support guest Android reste explicitement hors périmètre.

Le package guest expérimental
`examples/guest-mods/multiplayer-connection/` utilise ce transport pour
établir la connexion au serveur historique, lire dynamiquement l'ID de
`CLIENT_INIT_PACKET` et envoyer l'identité brute de huit octets attendue par
`WWClient.receiveInit`. Il ne synchronise encore ni Link, ni les acteurs, ni la
sauvegarde.

## Architecture cible

### Couche protocole

Responsabilités :

- codec big-endian/VLQ ;
- IDs de paquets ;
- framing ;
- connexion/déconnexion ;
- files entrantes et sortantes ;
- compatibilité serveur ;
- aucune connaissance de `fopAc_ac_c` ou `daPy_lk_c`.

### Couche modèle

Responsabilités :

- `NetworkState` ;
- `SyncedActorState` ;
- ownership ;
- interpolation ;
- dirty state ;
- validation des tailles et limites.

### Couche adaptateur WWHD

Responsabilités :

- lire Link local ;
- appliquer un snapshot distant ;
- mapper les acteurs HD ;
- créer/détruire une représentation distante ;
- lire l'inventaire et la progression via des offsets HD documentés ;
- détecter les transitions de scène ;
- gérer les couleurs, animations et pseudo.

## Ordre d'implémentation

### Étape 1 : contexte et contrat

- maintenir `context.md` et `claude.md` ;
- documenter les invariants ;
- identifier les signatures réellement utilisables ;
- ne pas toucher au protocole.

### Étape 2 : probe HD mono-client

Le SDK généré confirme que `WWHD_ADDR_daPy_Execute` reçoit le pointeur de Link
et que sa base `fopAc_ac_c` expose `current` à l'offset vérifié `0x314`.
`actor_place` expose la position `cXyz`, l'angle `csXyz` et la room. Le probe
compilable `ZeldaWWHDRecomp/examples/guest-mods/link-transform-probe/` lit ces
champs en lecture seule et les écrit dans les logs à une fréquence configurable.
Il ne crée aucun acteur, ne modifie pas le gameplay et n'ouvre aucune connexion
réseau.

Cette étape ne prouve pas encore que la build complète du jeu ou l'installation
du package fonctionne : elle doit être compilée avec le Makefile des exemples,
puis installée dans une build WWHD traduite avec `--mod-hooks`. Après validation
en jeu, ces valeurs pourront alimenter le `TransformSnapshot` sans inventer de
layout. Le stage ID reste à identifier séparément.

### Correctif installation code-mod Windows

Le gestionnaire installe les mods pendant que WWHDRecomp est lancé. Le runtime
lit périodiquement `data/code-mods-status.json` pendant que le processus Python
de préparation publie ce fichier par `os.replace`. Windows peut alors renvoyer
`WinError 5` si le lecteur tient encore brièvement le fichier ouvert. Le helper
`tools/installer/code_mods.py::atomic_json` réessaie maintenant ce remplacement
pendant une courte fenêtre sur Windows uniquement. Le comportement atomique et
la conservation de l'ancien fichier en cas d'échec sont conservés. Le test
`AtomicJson.test_retries_windows_replace_when_status_reader_holds_target` passe.

L'installation des packages utilise également un renommage de dossier staging
vers `Mods/<id>`. `runtime/src/mods/packages.cpp` réessaie désormais ce
renommage lorsque Windows signale un refus d'accès transitoire. Ce correctif
nécessite une nouvelle build du runtime distribuée ; contrairement au correctif
Python précédent, il ne suffit pas de copier un fichier dans la distribution.

### Étape 2 : codec et test de compatibilité

- extraire le codec indépendant ;
- tester les paquets historiques ;
- vérifier le byte order et les VLQ ;
- ne pas dépendre du moteur.

### Étape 3 : lecture locale de Link

- hook vérifié de `daPy_Execute` ;
- position, rotation, stage et room ;
- aucun joueur distant au début ;
- test de stabilité mono-client.

### Étape 4 : transport host

- ajouter le transport au runtime seulement après avoir identifié le point
  d'extension exact ;
- préserver le serveur Java et le port `10014` ;
- gérer les erreurs explicitement.

### Étape 3b : modèle NetworkState

`ZeldaWWHDRecomp/runtime/src/network_state.{h,cpp}` fournit maintenant un
modèle indépendant du moteur. Il conserve le snapshot local, les snapshots
distants indexés par `client_id`, limite le nombre de joueurs distants, rejette
les snapshots obsolètes et permet de retirer ou vider les joueurs distants.
Le modèle n'inclut aucun header HD et ne connaît ni `fopAc_ac_c`, ni les
adresses de hooks, ni le transport.

Le test autonome `runtime/tools/network_state_test.cpp` vérifie l'acceptation,
le rejet des doublons obsolètes, la limite de capacité et la déconnexion d'un
joueur. Il a été compilé et exécuté avec `g++ -std=c++20 -Wall -Wextra
-Werror`. Cette étape ne constitue pas encore une synchronisation réseau :
aucun transport host n'est branché et aucun snapshot n'est envoyé au serveur.

### Étape 5 : marqueur distant

- recevoir un snapshot ;
- interpoler ;
- afficher une représentation minimale qui ne dépend pas encore d'un second
  Link ;
- valider deux clients et les transitions.

### Étape 6 : joueur distant Link

- création native HD vérifiée ;
- mise à jour position/rotation ;
- animation minimale ;
- rendu ;
- pseudo et couleurs.

### Étape 7 : gameplay

- ennemis ;
- santé et états ;
- inventaire ;
- progression ;
- transitions complexes ;
- ressources et apparences.

## Tests obligatoires

- tests de codec byte-for-byte ;
- tests de paquets invalides et tailles excessives ;
- test connexion/déconnexion ;
- deux clients dans la même zone ;
- changement de room ;
- changement de stage ;
- suppression d'un client ;
- retard et perte de paquets selon le transport retenu ;
- sauvegarde locale intacte après déconnexion ;
- compatibilité avec le serveur Java historique ;
- build WWHDRecomp avec le mod désactivé et activé.

## État au 15 octobre 2026

- Audit initial terminé.
- Un codec indépendant du moteur est implémenté dans
  `ZeldaWWHDRecomp/runtime/src/multiplayer_protocol.{h,cpp}`.
- Le codec reproduit le framing historique `VLQ(packet_id)`,
  `VLQ(payload_length)`, `payload`, ainsi que la terminaison VLQ sur le bit
  haut et les scalaires big-endian.
- Un `TransformSnapshot` minimal (client, étape logique, stage, room, position,
  rotation) est sérialisable sans dépendance au moteur.
- Les tests autonomes sont dans
  `ZeldaWWHDRecomp/runtime/tools/multiplayer_protocol_test.cpp` et passent avec
  `g++ -std=c++20 -Wall -Wextra -Werror`.
- La cible CMake `multiplayer_protocol_test` et le test CTest
  `multiplayer_protocol` sont déclarés, mais CMake Tools n'a pas de
  configuration active dans le workspace au moment du test.
- Le modèle indépendant `runtime/src/network_state.{h,cpp}` et sa cible
  `network_state_test` sont ajoutés ; le test passe avec
  `g++ -std=c++20 -Wall -Wextra -Werror`.
- Aucun hook HD ajouté par le projet de migration.
- Le serveur Java historique n'est pas certifiable sans `mmb` et
  `serverCommon`.
- Le joueur distant complet reste le principal risque moteur.
- La prochaine implémentation doit vérifier le point d'extension d'un service
  host réseau et cartographier les packet IDs historiques avant tout envoi réel.
- Un transport TCP host minimal est implémenté et sa syntaxe C++ est validée,
  mais aucun échange avec le serveur Java n'est encore certifié. Il faut
  maintenant construire un harness de boucle locale, puis brancher le handshake
  historique après vérification de ses payloads.
- Le package guest de handshake est écrit mais n'a pas pu être compilé ici :
  l'exécutable `clang` avec backend PowerPC n'est pas disponible dans
  l'environnement courant.

### Session du 15 octobre 2026 (reprise)

Vérifié dans les sources :

- Le format d'init historique (`WindWakerClient/src/mmb++/net/Client.cpp`,
  `readInitPacket`) est : `VLQ(count)` + noms des paquets reçus, `VLQ(count)`
  + noms des paquets émis (l'index dans cette liste est l'ID), `VLQ(count)` +
  initialiseurs nommés (`ActorSync`, `ActorFilter`, `SaveServer`,
  `ResourceClient`). Le parsing de `multiplayer-connection/mod.c` correspond.
- L'ancien client envoie `CLIENT_INIT_PACKET` avec `writeLong(mClientUID)`
  (8 octets big-endian) ; le serveur lit 8 octets bruts et les encode en
  Base64. L'identité de 8 octets du mod est donc compatible sur le fil.
- `OutputPacket::send` émet `VLQ(id) + VLQ(longueur) + payload`, identique à
  `write_frame`.

Corrigé :

- `mod.c` : appel `read_vlq` avec un argument manquant (erreur de compilation)
  ; tampon d'init agrandi pour contenir 1 MiB + préfixe VLQ.
- `network_transport.cpp` : `disconnect()` fermait le socket encore utilisé
  par le worker, qui le refermait ensuite (double fermeture). L'appelant fait
  maintenant `shutdown()` et seul le worker ferme. Une trame annonçant plus de
  1 MiB est rejetée dès l'en-tête au lieu d'être bufferisée.
- `guest_mods.cpp` : `svc_net_send/receive` rejettent un pointeur guest nul ou
  une plage qui déborde, comme `svc_file_read`.

Implémenté et testé :

- `runtime/tools/network_transport_test.cpp` (cible CMake/CTest
  `network_transport`) : faux serveur loopback qui envoie l'init fragmentée
  octet par octet puis des trames coalescées, vérifie l'émission client octet
  pour octet, l'absence de troncature avec un tampon trop petit, la
  déconnexion propre côté serveur, le rejet d'une trame surdimensionnée, la
  déconnexion locale et l'erreur explicite sur port refusé. Passe avec
  `g++ -std=c++20 -Wall -Wextra -Werror` (MSYS2 mingw32, Windows).
- Les tests `multiplayer_protocol` et `network_state` passent toujours.
- `mod.c` passe `gcc -fsyntax-only` côté hôte ; ce n'est pas une compilation
  PowerPC et ne remplace pas `make` avec clang/ld.lld.

Non vérifié / limitations :

- `guest_mods.cpp` n'a pas été recompilé (build complète du runtime non
  configurée ici).
- `disconnect()` joint le worker : si celui-ci est bloqué dans
  `getaddrinfo`/`connect` vers un hôte injoignable, le thread de jeu attend la
  fin du timeout système.
- Le diff de `CMakeLists.txt` déplace `${GEN_DIR}/imports.c` de `gamecode`
  vers `wwhd`. Origine et nécessité à confirmer avant commit.
- Aucun échange avec le vrai serveur Java : `mmb` et `serverCommon` manquent
  toujours.

### Synchronisation du joueur local (PlayerPuppet)

Vérifié dans les sources historiques (client C++ et composants Java) :

- Entrée dans une zone (`WWClient::onGameRun`) : `CLIENT_PLAYER_CHANGE_MANAGER`
  avec le seul nom de stage (le `sprintf` ignore la room), puis
  `CLIENT_ACTOR_SYNC_CREATE` (`ActorSync::getOrCreate`) : octet `hasGameUID`,
  `VLQ solver`, `"PlayerPuppet"` deux fois, puis `VLQ count` + `(VLQ index,
  données)` pour chaque composant de la factory reçue dans l'initialiseur
  `ActorSync`. Sortie de zone : `CLIENT_PLAYER_LEAVE_MANAGER` vide.
- Mise à jour client (`ActorSync::calcWriters`) : `VLQ uid`, octet ticket,
  composants invalidés. Réception serveur (`SyncedActor::readUpdate`) : octet
  ticket, `VLQ owner`, composants. Le `CREATE` serveur ajoute `VLQ uid`,
  `hasGameUID`, `hasSolver [VLQ solver]`, nom, propriétés.
- `ActorBaseSync` : `current.pos` (3 floats BE) + `shape_angle` (3 shorts BE).
  `ActorPlayerSync` : `u16 item`, `VLQ proc`, 5 × `u32` couleurs RGB565,
  4 octets `GXColor`, 8 listes d'événements préfixées par un `VLQ count`.
- Les initialiseurs n'ont pas de préfixe de longueur : `ActorSync`,
  `ActorFilter`, `SaveServer` (`VLQ n` + `n` × 2 octets) et `ResourceClient`
  (arbre récursif) doivent tous être parsés pour rester alignés ; un nom inconnu
  est fatal dans l'ancien client.
- Un composant inconnu dans une factory est fatal (`ACTORSYNC_CREATE_COMPONENT`).
  On en déduit, sans pouvoir le vérifier (`serverCommon` absent), que le serveur
  n'envoie pas `SyncedActorComponent` dans la factory `PlayerPuppet`.

Vérifié dans le SDK HD : `fopAc_ac_c::current` (0x314), `shape_angle` (0x328),
`wwhd_play_get()` (0x025200D4) et `WWHD_PLAY_START_STAGE_NAME_OFFSET` (0x5134,
assertion dans `tools/guestmod/ci_guestmods.py`). Déduit mais non testé en jeu :
que ce nom de stage (`mStartStage`) équivaut au `mCurStage.getName()` du client
GameCube, et que l'offset 0x5134 est identique dans la build EU.

Implémenté et testé :

- `examples/guest-mods/multiplayer-connection/mp_protocol.h` : codec C
  freestanding (sans libc, sans header de jeu), compilable pour le guest
  PowerPC et nativement.
- `runtime/tools/mp_protocol_test.c` (CTest `mp_protocol`) : vecteurs dorés
  écrits à la main d'après le code historique (VLQ, `CHANGE_MANAGER`, `CREATE`
  PlayerPuppet complet, `UPDATE`), parsing de l'init avec les quatre
  initialiseurs, rejet de toute troncature, d'un initialiseur inconnu et d'un
  composant non supporté, décodage d'un `ActorPlayerSync` avec listes non vides
  et `s16` négatif encodé en VLQ 5 octets.
- `mod.c` réécrit : handshake, entrée de zone, uid local reçu via le solver,
  ticket suivi, `UPDATE` toutes les 0,1 s si Link a bougé, joueurs distants
  décodés et loggés toutes les 2 s. Aucun acteur n'est créé dans le jeu.
- Compilation PowerPC réelle réussie avec `D:\msys64\mingw64\bin\clang.exe`
  (cible ppc32) et `C:\devkitPro\msys2\mingw64\bin\ld.lld.exe`. Le traducteur
  `tools/guestmod/guestmod.py` accepte l'ELF : 10 fonctions, un hook d'entrée
  `0240EBB0`, 13 services tous enregistrés, un appel jeu `025200D4`.
- `tools/multiplayer/fake_server.py` : serveur de test stdlib qui relaie les
  PlayerPuppet entre clients d'une même zone ; `--bot` ajoute un joueur scripté
  qui tourne autour du client. Ce n'est pas le serveur de référence : uid,
  ticket = 0 et owner = uid sont des hypothèses. Testé avec deux clients natifs
  utilisant le codec du mod : création, visibilité mutuelle, relais d'update,
  `REMOVE` à la déconnexion, et mode bot.
- Runtime `build/windows-msys2-d/wwhd.exe` relinké avec les services réseau.
  Les CTest `multiplayer_protocol`, `network_state`, `network_transport` et
  `mp_protocol` passent ; `--renderer-smoke` passe.
- `tools/multiplayer/run_dev.ps1` lance ce runtime avec le jeu et le dossier
  utilisateur de la distribution portable.

Problème de build résolu localement :

- Avec le clang MINGW64, GNU ld ne résout pas les stubs `weak` de
  `build/gen/imports.c` (112 symboles `imp_nn_olv_*`, `imp_nn_boss_*`,
  `imp_padscore_WENC*`). La build a été reconfigurée avec
  `CMAKE_EXE_LINKER_FLAGS=-fuse-ld=C:/devkitPro/msys2/mingw64/bin/ld.lld.exe`.
  La solution propre est l'environnement CLANG64 documenté (lld par défaut) ou
  `pacman -S mingw-w64-x86_64-lld`.

Testé en jeu le 15 octobre 2026 (jeu EUR BCZP01, `run_dev.ps1`,
`fake_server.py --bot`) :

- Le module guest est traduit, chargé et compilé par le runtime local
  (10 fonctions, 1 hook) ; tous les services `wwhd_net_*` sont résolus.
- Handshake complet ; identité `wwhduser` reçue par le serveur.
- Nom de stage lu correctement en EU via `wwhd_play_get()` + 0x5134 :
  `sea_T` (écran titre) puis `sea`. L'offset est donc confirmé pour la build EU.
- Ordre `CHANGE_MANAGER` → `CREATE` → uid local reçu via le solver correct à
  chaque changement de zone.
- Sens montant : positions et `shape_angle` de Link reçues par le serveur et
  cohérentes avec les déplacements.
- Sens descendant : le bot (uid distant) est décodé par le mod ; les positions
  loggées sont sur le cercle de 150 unités attendu.
- Observation : à l'entrée de `sea_T`, la position du `CREATE` diffère de la
  première mise à jour, car Link n'est pas encore placé au moment où le nom de
  stage change. C'est sans effet pour l'instant, mais à surveiller pour le
  futur marqueur distant.

Toujours non testé : deux instances du jeu, serveur Java réel, transitions
complexes (portes, chargements intérieurs), reconnexion.

### Marqueur distant HUD (étape 5)

Vérifié dans le SDK : bindings `mDoLib_pos2camera` (025F1108) et
`mDoLib_project` (025F0C48), API HUD v2 (`wwhd_hud_register/emit`, repère de
base 1280×720, ancre `CENTER` d'après `anchored()` dans le runtime).
Vérifié dans `decomp/wwhd_src` : `wwhd_play_get()` est `dComIfGp_get()`
(générateur `tools/guestmod/public_sdk_data.py`), `mDoLibGameView::mpView` est
à +0x5FA4, et `mDoLib_pos2camera` ne teste pas cette vue (le mod le fait).
La projection sort dans le rectangle `screenWidth`/`screenHeight` (données
10058FA4/10058FAC de `m_Do_lib.cpp`), que le mod ramène à 1280×720.

Implémenté, compilé PowerPC et accepté par le traducteur, **non testé en jeu** :

- position distante lissée vers la dernière reçue (fenêtre de 0,1 s) ;
- point à 170 unités au-dessus des pieds, passé en espace caméra : caché si
  derrière (z ≥ −20), projeté, caché hors écran ; rayon selon la distance ;
- cercle teinté de la couleur de tunique reçue, contour blanc et label
  `J<uid>`.
- Entrée de zone retardée de 10 pas logiques après le changement de nom de
  stage, ce qui corrige le `CREATE` envoyé avec une position périmée observée
  au test précédent.
- Déduit : la vue de jeu à +0x5FA4 est identique en EU (même hypothèse que
  0x5134, confirmée pour celui-ci).

Testé en jeu le 15 octobre 2026 (EUR, faux serveur `--bot`) : validé par
l'utilisateur. Le marqueur suit le bot avec la bonne projection. Les
hypothèses +0x5FA4, `screenWidth`/`screenHeight` et l'axe −Z sont donc
confirmées en EU.

### Link distant (étape 6, expérimental, option `puppet`)

Analyse du client GameCube : le Link distant était un second `daPy_lk_c` créé
par `fopAcM_fastCreate("Link", 0xFFFFB000, …)`. Il fallait environ vingt
injections pour qu'il ne prenne pas la place du joueur (`skipPlayerSet`,
`skipPuppetUpdate`, neuf `skipSetDemo`, `playerEyeFix`,
`DisablePlayerAttention`, `Makebgwaitfix`…).

Vérifié dans `decomp/wwhd_src` pour la HD (détails dans `puppet.h`) :

- `fpcNm_PLAYER_e` vaut 0xA8 en HD (0xA9 sur GC).
- `phase_1` (0240EE2C) écrit le Link créé dans le registre
  `play + 0x5B2C` (`mpPlayer[2]`, `mpLinkPlayer` à +8).
- `playerInit` appelle `setStartDemo` (02543534), qui modifie l'état global
  des événements même pour l'index 255. Avec les paramètres GC `0xFFFFB000`,
  le mode de départ vaut 0xB et lance la démo 0xD3 : c'est une cause probable
  des crashs GC.
- `playerInit` réécrit la table de textures de la donnée de modèle
  **partagée** (`dComIfG_getObjectRes("Link")` dans `initModel`,
  `linktexS3TC`) ; `playerDelete` peut la ré-échanger.
- `makeBgWait` (phase_3) écrit le point de réapparition dans la sauvegarde,
  peut attacher Link au bateau et lancer une musique de fond.
- `execute` écrit des états globaux même pour un Link qui n'est pas le
  joueur n°0 (statuts A/R/Do, liste d'attention).

Protections implémentées (remplacements actifs seulement pour un acteur
`PUPPET_PARAMS` = 0xFFAA0040 et de procname 0xA8 ; tout autre Link passe par
le code d'origine) :

- phase_1 : sauvegarde et restauration des 12 octets du registre,
  attention désactivée ;
- phase_2 : `setStartDemo` neutralisé (renvoie 255 sans effet), table de
  textures partagée sauvegardée puis restaurée ;
- phase_3 : seulement `procWait_init` ;
- execute : seulement la partie visuelle de l'`execute` HD ;
- delete : table de textures protégée ;
- un Link distant n'est jamais mis en erreur ni supprimé avant sa
  construction (attente `cPhs_INIT`, comme le jeu quand la caméra manque) ; un
  Link distant sans emplacement se supprime lui-même une fois créé ;
- création seulement hors événement, dans la room du joueur local, 4 au
  maximum ; création asynchrone (`fopAcM_create`), suivie par
  `fopAcM_SearchByID` ; recréation si la room est déchargée, 3 essais ;
- coupure automatique si un id résout vers un autre acteur.

Test en jeu n°1 (15 octobre 2026, `puppet=1`) : **crash**
`J3DDrawBuffer.cpp:243 p_pkt->getEntryPtr() == 0` depuis `daPy_lk_c::draw`.
La pile affiche des adresses EU (`027F16C4` = `entryImm` 027F0E04 + 0x8C0 ;
`023D9824` = `draw` + 4). Cause : le `draw` HD insère quatre paquets J3D
statiques globaux (`l_onCupOffAupPacket1/2`, `l_offCupOnAupPacket1/2`, en
0x1046CE20/CEB8/CF50/CFE8), qui changent le test de profondeur pour dessiner
les yeux par-dessus les cheveux. Le second Link les réinsérait. C'est le bug
que `playerEyeFix` corrigeait sur GameCube.

Correctif : chaque Link distant a ses quatre paquets, construits par
`J3DPacket::J3DPacket` (027F1278) avec la vtable des statiques à +0xC
(10058D40 / 10058D10) ; `J3DDrawBuffer::entryImm` (027F0E04) les substitue
pendant le `daPy_Draw` d'un Link distant. Les adresses passent par
`WWHD_GAME_DATA` (traduction EU). Un Link distant pas encore suivi n'est pas
dessiné. Compilé et accepté par le traducteur, à retester en jeu.

Effets résiduels connus (documentés, pas des crashs) : `procWait_init`
efface une fois un bit de statut d'affichage du joueur n°0 ; `playerDelete`
efface le statut boomerang et le métronome du joueur n°0 ; les contrôleurs
d'animation des yeux ou de la bouche de la donnée de modèle partagée suivent
le dernier Link dessiné.

Compilé PowerPC sans avertissement (`-Wall -Wextra -Wshadow -Wconversion`),
accepté par le traducteur (16 fonctions, 6 remplacements). **Non testé en
jeu.** Points à confirmer en jeu : la couche courante lors de
`fopAcM_create` depuis l'`execute` du joueur, et l'absence d'effet de
`setAnimeRatioHD`, `checkOriginalHatAnimation` et `jointCB1` sur le joueur
n°0.

### Launcher et réglages en direct

- `launcher_cfg.h` : parseur freestanding de `launcher.cfg` (dossier `Data`
  du mod : `uid`, `host`, `port`, `color`, `color2`, `cursor`, `puppet`).
  Testé (valeurs invalides ignorées une par une, toutes les troncatures,
  taille maximale). Le mod le relit chaque seconde et envoie un `UPDATE`
  `ActorPlayerSync` si les couleurs changent (`mp_write_look_update`, vecteur
  doré testé).
- `WindWakerServer/.../HdModBridge.java` : écriture validée et atomique de ce
  fichier (fichier temporaire puis `ATOMIC_MOVE`). `LauncherUI` : dossier
  utilisateur WWHD, Player ID, COLORIZE vers la HD (voie Dolphin conservée),
  case « Link distant », CONNECT et SAVE PROFILE. Compilé avec `javac
  -Xlint:all` (JavaFX 21, JNA 5.13) sans avertissement. Test croisé : fichier
  écrit par Java, relu par le parseur C du mod, rejet des valeurs invalides
  sans modification du fichier. L'interface JavaFX n'a pas été lancée.
- `tools/multiplayer/run_dev.ps1` : instances isolées
  (`build/windows-msys2-d/instances/<n>/{user,save}`) copiées une fois depuis
  la distribution, qui n'est jamais écrite (empreintes vérifiées). Chaque
  instance a son `launcher.cfg` avec une identité et une couleur propres ;
  options `-CopyModsFrom` et `-Reset`. Deux bugs trouvés et corrigés par le
  test : chemins trop longs (`robocopy`) et identité dupliquée lors d'une
  copie de mods.

### Couleur du joueur

Le client GameCube recolorait la texture de tunique (J3D, ResTIMG RGB565,
DCFlushRange) à partir de « mailboxes » mémoire écrites par le launcher
Dolphin. Rien de cela n'est portable : le runtime HD ne remplace les textures
qu'au niveau des fichiers d'archive (docs/mod-manager.md, « content file
replacement only »). Ce qui a été porté :

- `mp_darken_rgb565`, `mp_player_look_set_colors` (dérivation des 5 couleurs
  d'`ActorPlayerSync::calc`) et conversions RGB888/565 dans `mp_protocol.h`,
  avec des tests aux valeurs calculées à la main ;
- options `color`/`color2` (`RRGGBB`) envoyées dans `ActorPlayerSync` ; un
  joueur distant est dessiné avec sa couleur primaire ;
- le bot du faux serveur porte du rouge (0xF800/0xFC00) pour le vérifier.

Bloqué : le rendu réel de la tunique colorée, qui nécessite un Link distant
(étape 6) et une API texture HD vérifiée.

### Multiplicateur d'ennemis

Porté depuis `WindWakerClient/src/Hooks.cpp::enemyMultiplier` dans un package
séparé `examples/guest-mods/enemy-multiplier` :

- `WWHD_REPLACE` de `dStage_actorCreate` (025C1154). La source décompilée
  montre qu'elle prend possession du bloc de paramètres (libéré ou passé à
  `fpcSCtRq_Request`). Chaque copie reçoit donc son propre bloc
  `fopAcM_CreateAppend` (025D5600, 0x24 octets) copié de l'original.
- Liste d'ennemis : les 50 noms de `WindWakerClient/data/enemies.h`.
- Options : `multiplier` (1–9, défaut 4 comme le GC), `spread` (distance de
  l'anneau, défaut 60 ; 0 reproduit le GC), `randomize` (randomiseur de
  paramètres GC, désactivé par défaut car il choisit des variantes
  arbitraires).
- Compilé PowerPC et accepté par le traducteur (1 hook replace), **non testé
  en jeu**. Non synchronisé en réseau : les copies partagent le `setID` de
  l'original.

### Couleurs de tunique et animations de Link (session du 15 octobre 2026, soir)

Workspace : le projet a été copié de `ProjetModding/WindWakerRecomp` vers
`DossierWindWakerOnline/Client`. `build/windows-msys2-d` a été configuré à
l'ancien emplacement : `cmake --build` y répond « no work to do » et le CTest
`mp_protocol` a tourné sur l'ancien binaire. Il faut reconfigurer ce dossier
de build au nouvel emplacement. `WindWakerServer` contient maintenant `mmb`,
`MMBServerCommon` et les jars JavaFX : les dépendances sont présentes, mais
le serveur n'a pas été recompilé ici. `HdModBridge.java` avait disparu avec ce
nouveau `WindWakerServer` (le `LauncherUI` est revenu à sa version Dolphin) ;
il a été réécrit.

Vérifié dans les sources :

- Les 7 fonctions d'animation ont une adresse et une signature dans le SDK :
  `setMoveAnime` 023E0E50, `setSingleMoveAnime` 023E0A04, `setActAnimeUpper`
  023DE7E8, `resetActAnimeUpper` 023DC6A4, `setAnimeEquipSword` 023E9F60,
  `setAnimeEquipItem` 023EF750, `setAnimeUnequip` 023ECEF0. Le code HD des
  quatre premières (decomp `d_a_player_main_01.cpp`, `_02.cpp`) ne modifie que
  le Link appelé. `setAnimeEquipItem` lit l'objet prêt du joueur local
  (`getReadyItem`) et efface un bit de statut du joueur 0 : il n'est pas
  rejoué.
- Le serveur Java (`ActorPlayerSync.java`) relaie les 8 listes d'événements.
- Texture HD : les en-têtes de la table de textures du modèle Link ont un
  `GX2Texture*` en +0x20 (`TextureHeader::mHDTexture`). La tunique héros est
  la texture « linktexS3TC » ; `playerInit` garde les originaux (liste en
  +0x458, copies en +0x48C, drapeau `daPyFlg1_CASUAL_CLOTHES` 0x8 de
  `mNoResetFlg1` +0x3BC). Le layout `GX2Texture`/`GX2Surface` vient de
  `runtime/third_party/cemu`. Le renderer ré-uploade une texture dont la
  mémoire a été écrite par le CPU (`runtime/src/write_watch.h`). Les blocs BC
  sont envoyés tels quels à Vulkan (`formats.cpp`), donc leurs extrémités
  RGB565 sont little-endian en mémoire guest.
- Le J3D HD (chargement des matériaux et des textures) n'est ni dans la
  décompilation ni dans le SDK.

Implémenté (package `multiplayer-connection` 0.2.0) :

- `anim_sync.h` : les 7 fonctions sont remplacées ; un appel sur le Link local
  (joueur 0, jamais un puppet) est enregistré, puis le code d'origine
  s'exécute. Les listes partent dans `ActorPlayerSync` au plus 30 fois par
  seconde, avec `mEquipItem` et `mCurProc` (champs du SDK). Elles sont
  bornées à 16 par liste, et l'appel le plus récent est toujours gardé. Sur un
  puppet, les 4 animations sont rejouées avant `animeUpdate`. Les indices
  `daPy_ANM` ≥ 0xEA (fin de l'enum GameCube) et les slots haut du corps ≥ 3
  sont ignorés. Les événements reçus avant que le puppet existe sont
  abandonnés. La synchro de l'équipement et des coups n'est pas faite.
- `tunic_color.h` (freestanding) : même règle HSB que l'aperçu de
  `LauncherUI` (teinte 65–165, saturation ≥ 0.22, luminosité ≥ 0.08, teinte
  < 100 = couleur secondaire). Recoloriage des extrémités BC1/BC2/BC3 (ordre
  BC1 et index préservés, jamais d'index transparent créé) et des pixels
  8_8_8_8.
- `tunic.h` : la texture héros du Link local est recolorée sur place à partir
  d'une copie intacte dans le tas du mod (`heap_size` 8 Mio). C'est aussi
  appliqué hors connexion, et relu chaque seconde depuis `launcher.cfg`. Un
  rechargement de la texture par le jeu est détecté par un hash. Pour chaque
  puppet aux couleurs différentes, une copie (`GX2Texture` et image à la même
  adresse modulo 64 Kio) est substituée dans la table partagée uniquement
  pendant son `daPy_lk_c::draw`, avec le masque dirty mis à jour comme
  `playerInit`. L'option `remote_tunics` permet de couper ce mécanisme.
- `HdModBridge.java` : écriture validée (règles du parseur du mod) et
  atomique de `launcher.cfg`, avec réessai si Windows refuse l'accès ; les
  autres lignes sont conservées et les doublons retirés. Dans `LauncherUI`,
  le champ « WWHD SETTINGS » (bouton BROWSE, chemin mémorisé dans les
  Preferences) relie le launcher au fichier. COLORIZE écrit
  `color`/`color2`/`cursor` (la voie Dolphin est conservée), SAVE PROFILE
  ajoute `puppet`, et CONNECT écrit `host`/`port` (pris en compte au prochain
  démarrage du jeu).
- `fake_server.py` garde la dernière mise à jour par composant, pour qu'un
  arrivant reçoive la position et les couleurs.

Testé :

- `mp_protocol_test.c` (gcc `-std=c11 -Wall -Wextra -Werror`) : ordre des
  champs `ActorPlayerSync::write` comparé à une écriture champ par champ,
  aller-retour, toutes les troncatures, bornes et débordement, message plein
  < 2 Kio. HSB des références calculé à la main, recoloriage, blocs BC1 3 et
  4 couleurs avec inversion, extrémités égales, BC3, 8_8_8_8.
- Bout en bout natif : deux clients (paquets produits par le codec du mod)
  via `fake_server.py`. L'autre client reçoit couleurs, objet et
  `setMoveAnime`.
- Compilation PowerPC sans avertissement (`-Wall -Wextra -Wshadow
  -Wconversion -Werror`) ; traducteur : 37 fonctions, 15 remplacements, 21
  services.
- `javac -Xlint:all` sans avertissement (JavaFX 21, JNA 5.13). Test croisé :
  fichier écrit par `HdModBridge`, relu par `launcher_cfg.h`, valeurs
  invalides refusées sans modifier le fichier. L'UI JavaFX n'a pas été lancée.

Non testé en jeu / à confirmer :

- Les 7 nouvelles adresses dans la build EU (le traducteur refuse à
  l'installation celles qui n'existent pas en EU).
- Format réel de la texture héros : le mod logue au premier passage
  `multiplayer: tunic texture found` (format, taille, tile mode). S'il n'est
  pas BC1/BC2/BC3/8_8_8_8, les couleurs se désactivent avec un message.
- Offsets decomp +0x3BC, +0x458, +0x48C supposés identiques en EU (même
  hypothèse que +0x5134 et +0x5FA4, confirmés).
- Couleur propre de chaque puppet : dépend du moment où le J3D HD lit
  l'en-tête de texture. Si c'est après le `draw`, les puppets portent la
  couleur locale.
- Rendu des animations rejouées sur le puppet.

Test en jeu n°1 de la tunique (0.2.0) : le mod s'est désactivé (« not recolourable »). Le
pointeur en ResTIMG + 0x20 n'est pas un GX2Texture mais un objet texture de 0x90 octets :
u32 drapeaux, puis le GX2Surface en +4 (decomp boomerangTextureSurface_c, rempli par 02773798
depuis le même pointeur de l'image 0x72 de l'archive Link). Confirmé par le log : dimension 1
en +4, largeur 640 en +8, pointeurs image et mip en +0x28 / +0x30. Corrigé en 0.2.1 (lecture
du surface à +4, copie des puppets sur 0x90 octets). Format réel encore à lire dans le log.

Test en jeu n°2 (0.2.1, puppet=1, faux serveur --bot) : tunique locale OK (BC1 sRGB 0x431,
640x384, tile mode 4). Crash au premier dessin du puppet : « Unsupported GX2 surface format 512 »
dans le thread de rendu. Cause : la copie de l'objet texture pour la couleur du puppet ne faisait
que 0x90 octets alors que le jeu alloue 192 octets pour ces objets (d_a_tbox_env.cpp,
Tbox_allocateTexture(192) puis constructeur). Corrigé en 0.2.2 : copie de 0xC0 octets, et option
remote_tunics désactivée par défaut (le moteur lit peut-être dans l'objet des champs mis en cache).

### Yeux, pseudos et course du puppet (0.2.3, 15 octobre 2026, nuit)

Retours du test en jeu : les yeux du Link local se voient à travers le puppet placé devant lui ;
la course du puppet paraît au ralenti ; le point de couleur doit devenir un pseudo.

Vérifié dans les sources :

- Les listes de dessin sont à `play + 0x5D30` (`dDlst_list_c`, `mpView` en +0x274 = 0x5FA4
  déjà confirmé). `daPy_lk_c::draw` met le visage et les cheveux, plus deux passes yeux/sourcils
  **sans test de profondeur**, dans `mpOpaListP0` (+0x24, « PLAYER_1 », play + 0x5D54), entre
  les paquets `onCupOffAup`/`offCupOnAup`, qui en HD règlent les masques de canaux couleur/alpha
  (modes 7 et 8 de `m_Do_ext_more_anm.cpp`). La passe des yeux avec profondeur est dans
  `mpXluListP1` (+0x2C). Le reste du corps, les mains et les PNJ sont dans `mpOpaList`
  (+0x48, « OBJECT_OPA », play + 0x5D78).
- `J3DJoint::entryIn` HD = 027F583C(model, joint) (`decomp/wwhd_src/include/JSystem/J3DGraph.h`).
- `daPy_lk_c::execute` appelle `decOldFrameMorfCounter(m_old_fdata)` après `jointCB1`. Champ
  `m_old_fdata` (+0x65CC) et binding `wwhd_ext_decOldCounter` (025E3EC8) présents dans le SDK.
  `setMoveAnime` lance un morph vers la pose précédente (`initOldFrameMorf`) ; sans
  décrément, `mOldFrameRate` ne retombe jamais à 0.
- `ActorPlayerSync.java` (serveur) ne lisait ni n'écrivait le nom que le mod envoie déjà en fin
  de composant depuis la session précédente : avec le vrai serveur, chaque mise à jour du
  joueur était désalignée. La factory `PlayerPuppet` (`actorCreators.ktml`) est
  `ActorBaseSync`, `ActorPlayerSync`, puis `SyncedActorComponent` (non synchronisable) :
  `ActorPlayerSync` est donc le dernier composant d'un paquet.

Implémenté :

- Yeux : pendant le `draw` d'un puppet, `mpOpaListP0` pointe vers `mpOpaList`, ce qui fait de
  sa tête un occultant ordinaire comme un PNJ. Ses paquets 0 et 1 et ses deux passes d'yeux
  sans profondeur (les deux `entryIn` qui les suivent) ne sont pas dessinés. Ses yeux gardent
  la passe avec profondeur de `mpXluListP1`. Déduit, non testé : `mpOpaList` est dessinée après
  P0, ou ne produit pas le marqueur alpha des yeux, comme pour les PNJ en jeu. Effet attendu :
  les yeux du puppet peuvent être un peu masqués par sa frange.
- Course : le puppet appelle `decOldFrameMorfCounter` comme `execute`.
- Pseudo (`name_tag.h`) : contour sombre, remplissage en dégradé vertical du blanc au gris en
  4 bandes, puis une couche de la couleur primaire de tunique à 45 %. Opacité selon la distance
  entre les deux Links : opaque jusqu'à 10 m, fondu, caché à partir de 20 m. On suppose
  1 unité = 1 cm (déduit, non vérifié). Au plus 104 éléments HUD par nom, soit 832 pour 8 noms
  (limite de 1024 par liste). Racine carrée de Newton : le 750 n'a pas `fsqrt` et le guest n'a
  pas de libm.
- Le manifeste déclare l'option `name`, déjà lue par `mod.c` ; version 0.2.3.
- Serveur : `ActorPlayerSync` lit le nom s'il reste des octets dans le paquet (client GameCube :
  pas de nom), le nettoie (ASCII imprimable, 16 caractères au plus) et l'écrit toujours.
- Launcher : `HdModBridge` valide `name` (1 à 16 caractères ASCII imprimables). SAVE PROFILE
  écrit le champ « PLAYER NAME », qui est prérempli depuis `launcher.cfg`.

Testé :

- `mp_protocol_test.c` passe avec gcc mingw32 `-std=c11 -Wall -Wextra -Werror`. Les vecteurs
  dorés n'avaient pas été mis à jour pour le nom. Ajouts : nom écrit après les hits, nom distant
  nettoyé et coupé à 16 caractères, clé `name` de `launcher.cfg` (espaces retirés, 17
  caractères refusés). Une comparaison `== 2.4f` échouait avec l'excès de précision x87 ; elle
  est corrigée par un cast `(float)`.
- Mod compilé PowerPC sans avertissement (`-Wall -Wextra -Wshadow -Wconversion -Werror`) ;
  traducteur : 44 fonctions, 18 hooks (dont le nouveau remplacement 027F583C), 24 services.
- `mmb`, `MMBServerCommon` et `windwakerserver` compilent avec `javac -Xlint:all` (JDK 24,
  JavaFX 21, jars du cache Maven `~/.m2` pour JNA, LWJGL et GraalVM), sans avertissement dans
  les fichiers modifiés. Test ponctuel (hors dépôt) de `ActorPlayerSync` : un composant HD
  avec nom est relu puis réécrit à l'identique ; un composant GameCube sans nom est relu sans
  octet restant puis réécrit avec un nom vide ; le nettoyage du nom correspond au codec C.

Non testé : rendu en jeu des trois corrections ; existence de 027F583C en EU (le traducteur
la vérifie à l'installation) ; serveur Java réel lancé avec le nom ; l'interface JavaFX.
Remarque : le bouton « Reload area » du launcher écrit `reload_area`, que `HdModBridge`
refuse (clé inconnue) et que le mod ne connaît pas. Ce point n'a pas été modifié.

Test en jeu de 0.2.3 (16 octobre 2026, faux serveur `--bot`) : connexion et handshake OK,
puis « malformed actor create » et déconnexion. Le bot de `fake_server.py` envoyait un
`ActorPlayerSync` sans nom, alors que le lecteur du mod exigeait le nom. Corrigé : le bot envoie
« Bot », et le mod (`mp_read_player_events`) prend un nom vide quand le paquet se termine
après la liste des hits, comme le serveur Java. Un client GameCube ou un mod plus ancien ne
coupe donc plus la connexion. `mp_protocol_test` couvre ce cas ; mod recompilé (44 fonctions,
18 hooks). À retester en jeu.

### Yeux du puppet, hauteur du pseudo, option « cacher les noms » (0.2.4, 16 octobre 2026)

Test en jeu de 0.2.3 : le puppet n'a plus d'yeux du tout. Les yeux de Link ne se dessinent donc
qu'avec les passes sans profondeur ; la passe avec profondeur seule ne suffit pas. Le correctif
de 0.2.3 (tête dans `OBJECT_OPA`, passes sans profondeur supprimées) est abandonné, ainsi que
le remplacement de 027F583C.

Vérifié dans les sources : Tetra/Zelda (`d_a_npc_zl1_draw.cpp`) utilise la même technique des
yeux, avec ses propres paquets et sa propre paire de listes : `setListP0` = play + 0x5D64
(`mpOpaListZelda0`, 1 seau, « Zelda_1 ») et `setListP1` = play + 0x5D6C (`mpXluListZelda1`,
« Zelda_3 »). C'est le pendant exact de la paire de Link, 0x5D54 / 0x5D5C (`d_drawlist.cpp`
`init`).

Implémenté : pendant le `draw` d'un puppet, les deux pointeurs de liste de Link
(`play + 0x5D54`, `+0x5D5C`) pointent vers ceux de Zelda, puis sont restaurés. Le puppet
dessine ses yeux exactement comme le jeu, avec ses propres paquets, dans des listes que le Link
local n'utilise pas. Déduit, non testé : ces listes sont dessinées à chaque image même sans
Tetra. Limites : les puppets partagent cette paire entre eux, et avec Tetra quand elle est
présente. Les yeux d'un puppet peuvent alors se voir à travers un autre puppet.

Pseudo : la ligne de base est à 140 unités au-dessus des pieds au lieu de 170.

Option « cacher les noms » : clé `hide_names` (0/1) dans `launcher.cfg` et option de manifeste
`hide_names`, relues chaque seconde. Quand elle vaut 1, aucun pseudo n'est dessiné. Dans le
launcher, la case « Hide player names » (qui n'était reliée à rien) écrit la clé dès qu'on la
coche ou la décoche, et SAVE PROFILE l'enregistre aussi. `HdModBridge` valide 0/1.

Testé : `mp_protocol_test` (clé `hide_names` valide et invalide) ; mod PowerPC sans
avertissement, traducteur 43 fonctions, 17 hooks ; `javac -Xlint:all` OK. Non testé en jeu ;
l'interface JavaFX n'a pas été lancée.

### Course du puppet : cause dans le runtime « True 60 » ; peinture du bateau (0.2.5, 16 octobre 2026)

Test en jeu de 0.2.4 : la course du puppet est toujours lente, et maintenant saccadée.

Vérifié dans les sources (`runtime/src/true60.cpp`) : le mode True 60 fait tourner Link à 60 Hz.
Les demi-passes sont des « previews » reprises à la passe suivante, et `J3DFrameCtrl::update` /
`decOldFrameMorfCounter` sont mis à l'échelle par `dt`. `classify()` prenait pour Link **tout**
processus dont l'execute est `daPy_Execute` et faisait `g_link = proc`. Avec un puppet :
- `g_link` basculait entre le Link local et le puppet à chaque pas ;
- le puppet (proc `Wait`, convertie) s'exécutait aussi sur les demi-passes ;
- `link_preview_restore` (qui exige `g_snap.proc == g_link`) ne reprenait plus la preview du
  vrai Link.

Le correctif de 0.2.3 (décrément du morph) n'en masquait qu'une partie. Cause aggravante : le
client n'active pas `TCP_NODELAY` (Nagle). Ce point n'est pas modifié ici.

Implémenté (runtime) : `is_controlled_link(proc)`. Seul le joueur n° 0 du registre
(`GD(0x1046F0B0) + 0x5B2C`, la même adresse que `savestate.cpp` et `aspect.cpp`) est Link pour
True 60, pour la trace et pour `g_link_steps`. Si aucun joueur n'est encore enregistré, le
comportement est inchangé. Un autre `daPy_lk_c` est un acteur 30 Hz ordinaire : exécuté
seulement sur les passes complètes et dessiné interpolé. Le runtime est recompilé
(`build/windows-msys2-d/wwhd.exe`, `--renderer-smoke` PASS).

Build : `build/windows-msys2-d` pointe vers l'ancien chemin
`ProjetModding/WindWakerRecomp/ZeldaWWHDRecomp`, qui n'existe plus. Une **jonction NTFS** de ce
chemin vers `DossierWindWakerOnline/Client/ZeldaWWHDRecomp` a été créée. Ninja garde ainsi ses
commandes, et seuls les fichiers modifiés sont recompilés (ici 4 étapes). Supprimer la jonction
casse ce dossier de build ; la solution durable est de reconfigurer un dossier de build au
nouvel emplacement (recompilation complète).

Peinture du bateau (`boat_paint.h`) : même principe que la tunique. Les textures des modèles
corps et tête (`daShip_c` +0x3B4 / +0x3B8 → `McaMorf::mpModel` +0x90 → `J3DModel::mModelData`
+0xAC → table de textures +0x30, offsets de la décompilation) qui contiennent de la peinture
rouge/orangé gardent une copie intacte et sont recolorées sur place.
- Règle de pixel (`tc_palette_make_boat`) : teinte dans [335, 360) ou [0, 35], saturation
  ≥ 0,40, luminosité ≥ 0,08, référence #E63946. `LauncherUI.recolorRedPixel` utilise la même
  règle, au lieu de teinte ≤ 25 et saturation ≥ 0,22.
- Rescan toutes les 0,5 s depuis l'execute du bateau. On n'écrit que dans les textures que le
  bateau référence encore ; les autres restent en réserve et sont libérées si la place manque.
  Budget de 4 Mio de copies. Rechargement détecté par hash.
- La teinte des couleurs de matériau rouges (version précédente) est conservée pour la peinture
  non texturée.
- Déduit, non testé : les parties texturées et les matériaux teintés sont distincts. Sinon, la
  couleur est appliquée deux fois et paraît plus sombre. Bois brun très saturé de teinte < 35 :
  il serait recoloré aussi. Le mod logue chaque texture peinte (« boat painted texture »).

Testé : `mp_protocol_test` (règle du bateau calculée à la main : référence, orange, bouclage de
la teinte, jaune, brun pâle, noir, gris, vert ; la règle de la tunique inchangée) ; mod PowerPC
0.2.5 sans avertissement (44 fonctions, 17 hooks) ; `javac -Xlint:all` OK. Non testé en jeu.

### Test de 0.2.5 et suite (0.2.6, 16 octobre 2026)

Test en jeu de 0.2.5 : course du puppet toujours lente et saccadée (les autres animations sont
correctes) ; couleur du bateau inchangée.

Constats dans `instances/1/wwhd.log` :
- `[true60] game logic at 60 steps/s off` : True 60 était **désactivé**. Le correctif de
  `classify()` (0.2.5) est juste quand True 60 est actif, mais ce n'était pas la cause observée.
  Il est conservé.
- `boat paint applied` sans aucune ligne `boat painted texture` : aucune texture des modèles
  corps/tête ne contient de rouge selon la règle. La peinture rouge vient de la couleur de
  matériau (log `boat red material index 0`).

Vérifié dans les sources : le bloc uniforme d'un matériau J3D HD (`material + 0xA0`) commence par
des bits « modifié », un par slot. Le slot 5 (027F9F0C) est le registre TEV C1
(`setTevColor(1)`). Le jeu met `| 0x20` avant d'écrire le slot 5 (`d_a_ship.cpp` pour l'ombre,
`d_a_obj_ikada.cpp` pour la couleur de mer). L'ancien code du mod écrivait la couleur sans ce
bit : elle n'était jamais envoyée au GPU. C'est cohérent avec la remarque de l'utilisateur sur le
GameCube (couleur prise au chargement seulement).

Implémenté :
- Bateau, matériaux : modèles corps **et** tête. La couleur C1 d'origine est lue une fois par
  adresse de matériaux (données de modèle partagées entre instances). Les matériaux retenus
  suivent la règle du bateau (`tc_palette_make_boat`). La couleur écrite est calculée depuis
  l'original avec la règle HSB du launcher, puis on met `| 0x20`. Retour à l'original quand la
  couleur est vidée. Toujours appliqué après l'execute du bateau et après
  `setLightTevColorType`. Les textures (`boat_paint.h`) restent gérées si un jour l'une contient
  du rouge.
- Réseau : `TCP_NODELAY` côté client (`network_transport.cpp`) et côté serveur Java
  (`mmb/net/Server.java`, sockets acceptées). Avant, Nagle et l'ACK retardé groupaient les
  trames par ~200 ms : positions et animations arrivaient par rafales.
- Course : aucune cause démontrable à la lecture (rejeu de `setMoveAnime` idempotent, pas de
  facteur d'échelle dans `J3DFrameCtrl::update`, interpolation indexée par bloc de matrices,
  donc pas de mélange entre les Links). Diagnostic temporaire `anim_diag.h` : toutes les 2 s, le
  log donne, pour le Link local et chaque puppet, executes/s, avance réelle de
  `mFrameCtrlUnder[1]` (frames/s), rate, proc, `m34C3`, rejeux `setMoveAnime`/s et sauts de
  phase causés par le rejeu. À lire au prochain test pendant une course.

Testé : `mp_protocol_test`, `network_transport_test` (g++ `-Wall -Wextra -Werror`), runtime
recompilé (4 étapes Ninja), mod 0.2.6 traduit (45 fonctions, 17 hooks), `javac -Xlint:all`.
Non testé en jeu.

### Diagnostic de la course, mesuré en jeu (0.2.7, 16 octobre 2026)

Mesures `anim_diag.h` de 0.2.6 (`instances/1/wwhd.log`) :
- Link local en course (proc 6) : rate 2,3, avance d'environ 68 frames/s (30 × 2,3) : normal.
- Puppet d'un joueur qui court : rate 2,29, avance d'environ 67 frames/s par `animeUpdate`
  (la bonne vitesse), **mais** environ 10 sauts de phase/s d'environ 8 frames, faits par le
  rejeu de `setMoveAnime` : un par lot reçu (30 rejeux/s). Ce sont ces sauts qui donnent l'aspect
  lent et saccadé.

Cause (vérifiée dans les sources, déduite pour l'effet) : `setMoveAnime` repart de la phase
`frame / getFrameMax` lue dans le `J3DAnmTransform` de `mAnmRatioUnder[0]`. En HD, les tas
d'animation renvoient la ressource de l'archive elle-même (`Player_getArchiveResource`) : cet
objet et son `mFrame` sont partagés entre tous les Links et emplacements qui jouent la même
animation.

Correctif (`anim_sync.h`, `anim_phase_save` / `anim_phase_keep`) : autour de chaque
`setMoveAnime` rejoué, si les animations des deux emplacements du sous-corps restent les mêmes,
le puppet garde ses propres frames (sous-corps, et haut du corps s'il ne change pas). Seuls
vitesses et poids sont mis à jour. Un changement d'animation recalcule la phase comme le jeu.
Disposition `anmRatioPack_c` : 0x10 octets, `mpAnmTransform` en +4 (decomp
`d_a_player_main_02.h`). Le diagnostic reste actif pour vérifier : « jumps/s » doit tomber à
environ 0 en course.

Bateau : la règle HSB n'a retenu aucun matériau en 0.2.6 (pas de log « painted »). On revient à
la détection « rouge dominant » (r ≥ 1,15 × g et b), qui trouvait le matériau 0 du corps. On
garde le bit `0x20`, les originaux, le corps et la tête. La couleur écrite est la couleur
choisie au pic de l'original. La couleur C1 de chaque matériau est loguée une fois
(« boat material », « C1 r/g/b », « painted ») pour vérifier ce qui porte le rouge.

Testé : mod PowerPC 0.2.7 sans avertissement (45 fonctions, 17 hooks), `mp_protocol_test`.
Non testé en jeu.

### Course réglée, couleur du bateau par les registres GX (0.2.8, 16 octobre 2026)

Test de 0.2.7 : **animation de course du puppet confirmée correcte par l'utilisateur** (la phase
est conservée au rejeu). Le diagnostic `anim_diag.h` est retiré.

Bateau (le Lion Rouge, `daShip_c`) toujours inchangé. Log de 0.2.7 : le corps a 1 matériau, la
tête 2, et le slot uniforme C1 vaut 0 partout en dehors du dessin. Le slot n'est rempli qu'au
dessin ; la vraie couleur est la couleur GX du bloc TEV.

Vérifié dans les sources : le bloc TEV d'un matériau HD (`material + 0x18`, vtable en +4 :
`setTevColor` +0x24, `getTevColor` +0x34 → `GXColorS10*`, `setTevKColor` +0x3C, `getTevKColor`
+0x4C → `GXColor*`) et la correspondance avec les slots uniformes : TEV Cn → slot 4+n,
konst Kn → slot 7+n, bit « modifié » = 1 << slot (`d_a_bgn_draw.cpp` C0 → 4 / 0x10,
`d_a_ghostship.cpp` K3 → 10 / 0x400, ombre du bateau et `d_a_obj_ikada.cpp` C1 → 5 / 0x20).
Conversion de couleur avant le slot : 0274D458(dst, src, 1.0f).

Implémenté (`ship_material.h`, remplace le code de `mod.c`) : pour le corps et la tête, les
registres C0–C2 et K0–K3 de chaque matériau sont lus une fois par adresse de matériaux. Un
registre rouge dominant (r ≥ 40, r ≥ 1,15 × g et b) est de la peinture. Chaque registre est
logué (« boat material C/K », r/g/b, « -> paint »). Écriture comme le jeu : couleur GX, setter
du bloc TEV, conversion, slot uniforme et bit « modifié ». Appliqué après l'execute du bateau et
après `setLightTevColorType`. Retour aux originaux quand la couleur est vidée. Les méthodes
virtuelles sont appelées par pointeur (dispatch du runtime).

Testé : mod PowerPC 0.2.8 (44 fonctions, 17 hooks, appels 0274D458 et 027F9F0C présents),
`mp_protocol_test`. Non testé en jeu. Si aucun registre n'est marqué « paint » dans le log, le
rouge vient d'ailleurs (couleur de matériau du canal de lumière, ou couleurs de sommets) : ce
sera la prochaine piste.

### Bateau : les registres GX sont blancs, l'orange est une texture (0.2.9, 16 octobre 2026)

Log de 0.2.8 : les 3 matériaux du Lion Rouge (corps 1, tête 2) ont C0–C2 et K0–K3 tous à
(255, 255, 255). Le bloc en `material + 0x18` sert aussi de bloc de couleur
(`d_a_obj_pirateship.cpp` appelle `setMatColor(1)` sur ce même pointeur et écrit le slot 5).
L'orange vient donc d'une texture. L'utilisateur précise que la peinture est plutôt orange que
rouge. Le scan de 0.2.5–0.2.8 ne retenait aucune texture : soit la teinte était hors de la
bande (≤ 35°, saturation ≥ 0,40), soit le format n'était pas pris en charge, et la texture était
alors écartée sans message.

Implémenté :
- Règle du bateau élargie à l'orange : teinte [335, 360) ∪ [0, 42], saturation ≥ 0,30. Le jaune
  des garnitures (~50°) reste hors bande. `LauncherUI.recolorRedPixel` est aligné.
- Une texture est « peinte » si au moins 1 % de ses blocs sont dans la bande.
- Log une fois par texture : adresse, format GX2, largeur, hauteur, octets, format recolorable,
  part de peinture (‰). Log du nombre de textures de chaque modèle.
- `ship_material.h` reste en place : registres blancs, donc aucun effet, mais il logue.

Testé : `mp_protocol_test` (orange foncé #C87828 recoloré, brun pâle à saturation 0,27
ignoré) ; mod 0.2.9 traduit (45 fonctions, 17 hooks) ; `javac` OK. Non testé en jeu.

### Bateau : erreur de lecture de la table de textures (0.2.10, 16 octobre 2026)

Log de 0.2.9 : « boat model textures 9050 » pour les deux modèles, et aucune texture loguée.
Cause vérifiée dans les sources : `modelData + 0x30` contient un **pointeur** vers la
`TextureTable` (`ModelData::mTexture`, `d_a_player_main_04b.h`, lu par `playerInit` comme
`getTexture()`), pas la table elle-même. `boat_paint.h` lisait le pointeur comme si c'était la
table : aucune texture du bateau n'a jamais été examinée en 0.2.5–0.2.9. `puppet.h` et `tunic.h`
déréférencent correctement. Corrigé (`tunic_ld32(data + 0x30)`).

Approche : recoloriage sur place depuis une copie intacte, comme la tunique, qui fonctionne en
jeu. L'échange d'en-tête vers une copie allouée (l'idée de l'utilisateur, et ce que faisait la
couleur des puppets) avait fait planter le rendu (« Unsupported GX2 surface format »). La texture
du bateau n'appartient qu'au bateau, donc le résultat visuel est le même.

Testé : mod 0.2.10 traduit, `mp_protocol_test`. Non testé en jeu.

### Joueurs sur la carte marine, icône de Link, ombre du bateau (0.2.11, 16 octobre 2026)

Log de 0.2.10 : la peinture du bateau fonctionne. Textures retenues : 25EA673C (512×256 BC3,
812 ‰), 25EA667C (512×256 BC3, 407 ‰), 25EA697C (1024² BC1, 272 ‰) et 24FBC164 (1024² BC1,
270 ‰). La dernière est hors de la zone de l'archive du bateau (25EA…). Signalé par
l'utilisateur : la couleur du bateau assombrit Link, comme une ombre sur lui.

**Bateau** (déduit, non testé en jeu) : une texture que le modèle du Link local liste aussi
(`daPy_lk_c::mpCLModelData` +0x444, table de textures +0x30, même objet HD ou même image) n'est
plus peinte. Vérifié dans les sources : `modelData + 0x34` est la `JUTNameTab`
(`ShipCreateModelTextures::mTextureName`, `d_a_ship_create.h`) et 027ED1F0 en donne le nom
(`daShip_replaceSeaETextures` et `playerInit`). Chaque texture est maintenant loguée avec son
nom. Si l'ombre reste, le log nommera la texture à exclure.

**Carte marine** — vérifié dans le runtime (`story_chart.cpp`, `story_notebook_game.cpp`,
`story_notebook.cpp`, `story_notebook_bag.h`, `controller_reapply.cpp`) :
- `hdSeaChartMenu_c::updateProgressIcons` 02678B74 à chaque mise à jour du menu ;
  `hideMenuPanes` 02678B00 quand l'écran est occupé ; `destroy` 02677B90 ;
- zone de carte : pane principal du menu (+0x70), 7×7 cases de 100000, centre de D4 à
  l'origine du pane, une case = taille / 7, x vers l'est, z vers le sud (y du layout vers le haut) ;
- panes : parent +0x0C, enfants +0x14, translation +0x1C, échelle +0x34, taille +0x3C, flags +0x44
  (bit 0 visible), alpha global +0x46, nom +0x80 ;
- layout du menu +0x44 → +4 → +0xC = racine `L_Map_00` ; icône de Link `P_MapLinkFace_00`,
  matériau +0xF4, texture GX2 +0x28 (64×64) ;
- les layouts du GamePad sont conçus en 1280×720 et affichés en 854×480 ; `hdInputManager`
  (101F5088) +0x1D0 = 0 quand les menus passent sur la TV (manette Pro).

Runtime : le HUD n'acceptait qu'un callback par mod, sur un seul écran. Or les pseudos sont sur
la TV et la carte sur le GamePad. Ajout du service hôte `wwhd_hud_register_drc(callback)` : une
seconde liste, GamePad seulement, publiée à côté de la première (clé mod + écran + slot dans
`hud::Store`). Documenté dans `wwhd_guest.h` et `docs/mod-sdk-v2.md`.

Mod (`sea_chart.h`, `mod.c`) — déduit, non testé en jeu :
- Sur la mer (`sea`), chaque joueur distant est une flèche (`assets/map_player.png`, notre
  dessin, `tools/multiplayer/make_map_marker.py`) teintée de sa couleur `cursor`. Comme le client
  GameCube, qui dessinait `mPlayerCursor` sur la carte.
- La flèche est orientée selon son cap (rotation = π − cap) et suit le fondu du menu. Son pseudo
  est affiché en dessous, sauf si les noms sont cachés.
- Position : composition des panes de la zone de carte jusqu'à la racine. Rien n'est dessiné si
  un pane est caché ou si le menu est occupé.
- Liste GamePad, ou liste TV avec la manette Pro (les pseudos 3D sont alors omis).
- Le vert de `P_MapLinkFace_00` est recoloré aux couleurs de tunique locales (règle de la
  tunique), sur place depuis une copie intacte, avec restauration sans couleur. Le format est
  logué (« sea chart Link icon texture »).
- Limites : carte marine seulement (pas les cartes d'îles ou de donjons). Hors de la mer, les
  positions reçues ne sont pas des coordonnées de mer, donc rien n'est affiché.

Corrigé : `guest_hooks_test` ne se liait plus depuis les services `wwhd_net_*` (transport et
protocole absents de la cible CMake).

Testé : `guest_hooks_test` (nouveau test des deux listes TV/GamePad et du désenregistrement
séparé), `wwhd.exe --renderer-smoke`, `mp_protocol_test`, mod 0.2.11 traduit (55 fonctions, 20
hooks, `wwhd_hud_register_drc` importé). Package copié dans `instances/1`. Non testé en jeu.

### Icône de Link pour les joueurs distants, carte seulement menu ouvert (0.2.12, 16 octobre 2026)

Demandé : utiliser l'icône de Link du jeu pour les autres joueurs, et ne plus les dessiner
quand le menu Start est fermé. Le GamePad garde sa carte affichée pendant le jeu, donc
« menu à l'écran » ne suffit pas.

Vérifié (`docs/gamepad-display.md`, `runtime/src/gfx/gamepad_ui_state.cpp`) : le menu Start est
ouvert quand l'état de `SCENE-ingame_ui` (101F8344) est 1049EC84 / 1049ECB0 / 1049ECDC (entrée,
menu, sous-fenêtre). Le descripteur est lu en +0x18 → +8, sinon en +0x14. Les joueurs ne sont
dessinés que dans ces états.

Runtime, deux services hôtes (documentés dans `wwhd_guest.h` et `docs/mod-sdk-v2.md`) :
- `wwhd_hud_game_pixels(texture, rgba, capacity)` : décode une GX2Texture du jeu en RGBA8 dans la
  mémoire du mod, avec le décodeur du port (`game_glyph::read_texture`, branché par
  `mods/guest_game_texture.cpp`). Rien du jeu n'est stocké ;
- `wwhd_hud_image(rgba, w, h)` : crée une image HUD depuis des pixels du mod.

Mod (déduit, non testé en jeu) :
- l'icône `P_MapLinkFace_00` est décodée avant notre recoloration locale ;
- pour chaque joueur distant, elle est recolorée à ses couleurs de tunique (règle de la tunique)
  et mise en cache par couleur, version décodée et époque HUD ;
- notre flèche, à la couleur `cursor`, est placée à côté dans la direction du joueur (sin/cos par
  série de Taylor, sans libm) ;
- les noms des panes voisins de l'icône sont logués (« sea chart Link icon group ») pour retrouver
  la flèche du jeu.

Testé : `guest_hooks_test` (décodeur absent, tampon trop petit, tampon hors du mod, copie, image,
libération), `wwhd.exe --renderer-smoke`, `mp_protocol_test`, mod 0.2.12 traduit (56 fonctions,
20 hooks). Copié dans `instances/1`. Non testé en jeu.

### Puppet du bateau (0.2.13, 16 octobre 2026)

Demandé : chaque joueur a son Lion Rouge en puppet, qui se déplace quand le joueur est dessus.

**Protocole.** Le protocole historique n'a pas de synchro du bateau du joueur (seulement
`WarShipSync`). Extension WWHD après le nom dans `ActorPlayerSync` : `u8 flags` (1 présent,
2 monté, 4 voile levée), puis, si présent, `f32 x, y, z` et `u16 rx, ry, rz`. Côté C, le bloc
n'est lu que si l'octet suivant a le bit 7 à 0 : dans une création du serveur, `ActorPlayerSync`
peut être suivi de l'index du composant suivant (un octet VLQ, bit 7 levé, au plus 8 composants),
cas révélé par `mp_protocol_test`. Le serveur Java lit le bloc s'il reste des octets (comme le
nom) et le réécrit toujours. Le client envoie le bateau à au plus 10 Hz quand il bouge, et
immédiatement quand ses flags changent.

Vérifié dans le décomp (`d_a_ship_create.cpp`, `d_a_ship.cpp`, `d_a_ship_exec.cpp`, `d_a_grid.cpp`)
et le SDK :
- `fpcNm_SHIP` 0xA5 ; `create` 0247C358 écrit `play + 0x5B3C = this` ;
- `create` crée la voile (`fopAcM_create` 0xAB, `daGrid_c`), la flèche de vent (025DADA4, 0x16)
  et, en mer, `Coming2` (`fopAcM_createChild` 025D5A20, 0x10E) ;
- la voile garde son bateau dans une globale unique (1046438C), fixée au premier bateau trouvé ;
- `shipDelete` vide le bit `SAIL` de `play + 0x5CDC` ;
- statut 0 du joueur `play + 0x5CD8`, bit monté 0x10000 ; `mStateFlag` +0x644 (`SAIL_ON`
  0x200) ; `m0392` +0x67A (mât levé = 0xB) ; `setPartOnAnime` 0247BB24 / `setPartOffAnime`
  0247BA38 ;
- fin d'`execute` : matrice ZXY (`mDoMtx_ZXYrotS` 025F1AA4), `McaMorf::play` 025E535C et
  `calc` 025E55A0, tête sur l'articulation 4 du corps.

**Puppet** (`ship_puppet.h`, avec l'option `puppet`) — déduit, non testé en jeu :
- un vrai `daShip_c` aux paramètres 0xFFAB0000, créé (salle -1) seulement si l'étape a le bateau
  local ;
- `create` d'origine conservé, avec trois neutralisations :
  - `mShipActor` sauvegardé puis restauré ;
  - voile remplacée par un faux id ;
  - flèche de vent et `Coming2` non créés ;
- `daShip_Execute` remplacé par la seule fin matricielle, à la position lissée reçue, avec le mât
  levé ou baissé selon la voile ;
- `draw` d'origine ; suppression qui préserve le statut joueur 1.

Limites :
- pas de voile en tissu, de canon, de grue ni de sillage ;
- couleur de bateau commune (textures partagées) ;
- le reflet du matériau de coque suit le dernier bateau dessiné.

Faux serveur : `--bot --ship` fait naviguer le bot sur son bateau (cercle de 1200 à 2500 à l'est).

Testé :
- `mp_protocol_test`, avec les vecteurs mis à jour et les cas suivants : bloc complet,
  troncatures, bloc absent, bloc avant `ActorBaseSync` ;
- mod 0.2.13 traduit (66 fonctions, 26 hooks) ;
- `ActorPlayerSync.java` compilé avec `-Xlint:all` ;
- aller-retour Java (hors dépôt) d'un composant encodé en C : bloc bateau identique, bloc absent
  réécrit avec flags 0.

Risque préexistant relevé : Java réécrit le VLQ du proc -1 sur 10 octets, que le lecteur C
(5 octets au plus) refuserait. C'est sans effet tant que le proc local est un vrai numéro de
procédure.

Non testé en jeu.

### Crash au chargement du niveau (0.2.14, 16 octobre 2026)

Log de 0.2.13 (`sea_T`) : « FATAL: indirect branch to unknown address 47C5DBB0 (lr=025E13DC) »
pendant l'étape `perform` d'une phase de création d'acteur (`DrawIterater` → 02019F74 →
025E13B0). Aucune création de puppet de bateau n'était encore demandée.

Cause vérifiée dans le décomp : `fopAcM_createChild` (025D5A20) prend **9** arguments
(`d_a_arrow.cpp` : `..., s8 subtype, u32 createFunc`, le 9ᵉ sur la pile). La liaison du SDK
(`bindings.h`) en déclare 8. Le remplacement à 8 arguments de 0.2.13 transmettait au jeu un
callback de création pris sur sa propre pile, pour tous les enfants créés par le jeu.

Corrigé :
- 025D5A20 n'est plus remplacé ;
- un `Coming2` dont le parent (`parentActorID` +0x2E8, suivi par `daComing2::_execute`
  021138E0) est un bateau puppet se supprime à son premier execute ;
- arités des autres remplacements recoupées : `fopAcM_create` 8 (`d_a_coming2.cpp`),
  `fopKyM_create` 5 (`d_a_fan.cpp`), méthodes du bateau 1.

Règle retenue : vérifier l'arité d'une fonction dans le décomp avant de la remplacer. Une liaison
du SDK peut omettre des arguments passés sur la pile.

Testé : mod 0.2.14 traduit (66 fonctions, 26 hooks), `mp_protocol_test`. Non testé en jeu.

### Main du puppet, couleur par bateau, voile, canon et grue (0.2.15, 16 octobre 2026)

Demandé :
1. la main du Link puppet allait sur le bateau local quand le joueur local montait sur le sien ;
2. une couleur propre à chaque bateau ;
3. la voile du puppet (seul le mât se voyait) ;
4. le canon et la grue déployés.

**Main** (vérifié dans `d_a_player_main_01.cpp`) : `jointAfterCB` 023D7564 et
`setShipRideArmAngle` 023D7198 lisent le bateau du joueur 0 (`play + 0x5B3C`) et son statut
`SHIP_RIDE` (`play + 0x5CD8`, 0x10000), puis placent la main sur le haut du gouvernail
(`ship + 0x720`). Pendant la fin visuelle de l'execute d'un Link puppet, ces deux valeurs sont
maintenant celles du puppet : son bateau puppet et son état « monté ». Le bateau puppet calcule
son `mTillerTopPos` comme l'execute du jeu (articulation 0xA × `tiller_top_offset` 101D03C0).

**Voile** (vérifié dans `d_a_grid.cpp` et `d_a_ship_exec.cpp`) :
- `daGrid_c` est créé par `create` et son id est rangé dans `mGridID` (+0x6F8) ;
- il lit son bateau dans la globale 1046438C, fixée au premier bateau trouvé par nom à sa
  création ;
- l'execute du bateau le place : articulation 7, angles du bateau, `scale.y` = longueur de l'axe
  z de l'articulation 7, +0x2B78 = 1 − longueur de l'axe x de l'articulation 8.

Implémenté :
- la voile du puppet est maintenant créée (le faux id de 0.2.13 et le remplacement de
  `fopAcM_create` sont retirés) et placée par l'execute du puppet ;
- pendant ses `create` 021655E0, `execute` 021644BC et `draw` 02162F68 (1 argument chacun), la
  globale contient le bateau puppet, puis reprend sa valeur ;
- après toute création de voile, la globale est remise sur le bateau local si elle désigne un
  puppet ;
- la voile est supprimée avec son bateau.

**Canon et grue** :
- protocole : bits 4-5 des flags = `mPart` (+0x637) du bateau ;
- le puppet appelle à chaque pas `setPartAnimeInit` 0247BC44 (abaisse le mât, puis le relève avec
  la nouvelle partie) ;
- les modèles canon (+0x418) et grue (+0x41C), placés par le callback du mât, sont calculés tant
  qu'ils sont déployés ;
- le crochet (+0x420, placé par `setRopePos` avec la physique de la corde et des effets d'eau) est
  réduit à un point ;
- la visée du canon n'est pas synchronisée.

**Couleur par bateau** :
- protocole : flag 0x08 suivi de r, g, b (couleur du bateau du launcher) ;
- `boat_paint.h` : pour chaque bateau puppet dont la peinture diffère de la peinture locale, une
  copie de chaque texture peinte est faite depuis la copie intacte : objet de 0xC0 octets, image
  et mips à la même adresse modulo 64 Kio, recolorés ;
- pendant le `daShip_Draw` 02472AAC du puppet, ces copies remplacent les objets dans les tables
  partagées du corps et de la tête, avec le masque dirty comme `setResTIMG` ; les originaux sont
  restaurés ensuite ;
- non vérifiable dans les sources : le code J3D HD de liaison des textures n'est pas décompilé. Si
  le moteur lie la texture après l'entrée du modèle, tous les bateaux garderont la peinture locale.
  C'est la même incertitude que pour `remote_tunics` ;
- budget : 8 Mio de tas au maximum (`packages.cpp`), soit environ 1,9 Mio par bateau et trois
  bateaux. Au-delà, le message « no mod heap left for one more ship paint » est logué.

Tests :
- `mp_protocol_test` : bloc avec couleur et partie, couleur seule, comparaison de couleur ;
- mod 0.2.15 traduit (73 fonctions, 29 hooks) ;
- `ActorPlayerSync.java` compilé avec `-Xlint:all` ;
- aller-retour Java d'un bloc avec couleur et partie : identique ;
- faux serveur : le bot a la voile déployée et un bateau bleu.

Non testé en jeu.

### Link puppet attaché au bateau, canon, grue, gouvernail et voile synchronisés (0.2.16, 16 octobre 2026)

Demandé : le Link puppet doit être fixé à son bateau (le lissage de position était mauvais en
navigation), et le canon et la grue doivent prendre leurs vrais angles et la hauteur.

**Link monté** (vérifié dans `d_a_player_main_02.cpp`) : `setShipRidePos` 023E287C place Link à
matrice de base du corps × `l_ship_offset` 10034F84 (0, 15, −35), ou `l_ship_offset3`
10034F9C (0, 15, −96) dans le proc 0xD7, avec le cap du bateau. Elle modifie aussi le statut du
bouton R du joueur 0 et lit le vent, donc elle n'est pas appelée.

Quand le joueur distant est monté, le Link puppet prend cette position depuis la matrice de son
bateau puppet. Cette matrice est calculée depuis la même position lissée que celle de l'execute du
bateau, si bien que les deux bougent ensemble. Le proc vient de `ActorPlayerSync` (`curProc`) et
`puppet_set_look` le reçoit.

**Détail du bateau** : nouveau flag 0x40 du bloc bateau, suivi de six `s16` et d'une position :
- gouvernail `m0366` (+0x64E) ;
- voile `mSailAngle` (+0x64C, lu aussi par le tissu) ;
- canon : tangage `m0394` (+0x67C) et lacet `m0396` (+0x67E) ;
- rotation de la grue `m0398 + m039C` (+0x680, +0x684) ;
- corde `mRopeCnt` (+0x686, au plus 125) ;
- puis `f32` x, y, z du crochet (base de `mpLinkModel`).

Les champs sont vérifiés dans `d_a_ship.h` et lus par les callbacks d'articulation
(`bodyJointCallBack`, `cannonJointCallBack`, `craneJointCallBack`). Le puppet les écrit avant de
calculer ses modèles, avec un lissage par l'angle le plus court.

**Corde** : `mRopeLine` (+0x424) contient ses lignes en +0x184, et les points de la première ligne
sont à l'adresse lue en +0 de cette ligne (`m_Do_ext_line_mat1.h`). `setRopePos` simule la corde,
avec le point 0 en bas. Le puppet trace une corde droite :
- de la pointe du bras (articulation 1 × `ship_ropeOffset` 101D03CC, 160 unités) jusqu'au crochet ;
- crochet à la position reçue, orienté selon le bateau ;
- tracé par le draw d'origine (`mRopeCnt` ≥ 2).

Sans le détail (ancien émetteur), le crochet est replié et aucune corde n'est tracée.

Testé :
- `mp_protocol_test` : octets du bloc détail, aller-retour, troncature ;
- mod 0.2.16 traduit (73 fonctions, 29 hooks) ;
- `ActorPlayerSync.java` compilé avec `-Xlint:all` ;
- aller-retour Java du bloc complet : identique.

Non testé en jeu.

### Link exactement sur le bateau, crochet orienté, icônes de carte à jour (0.2.17, 16 octobre 2026)

**Décalage restant sur le bateau** (déduit) : le lissage de la position du bateau puppet se
faisait pendant l'execute du Link local, entre les execute des acteurs. Selon leur ordre, le bateau
et son passager pouvaient donc lire deux états lissés différents dans la même image. Désormais, le
lissage et la matrice de base sont calculés une seule fois par image (`ship_slot_advance`), par le
premier des deux à s'exécuter (bateau ou Link puppet monté, drapeaux `ship_ahead` / `link_ahead`).
L'autre reprend exactement la même matrice. Le facteur de lissage vient de `wwhd_logic_dt`.

**Crochet de la grue** (vérifié dans `d_a_ship.cpp`, `setRopePos`) : avec plus de 2 points, le
jeu oriente le crochet selon le dernier segment de la corde (c = point 0 − point 1), ramené dans le
repère du cap du bateau :
- rotation `ZXYrot(atan2s(cz′, cy), cap, atan2s(−cx′, √(cy² + cz′²)))` ;
- puis `XrotM(−0x4000)` (la rotation de 90° qui manquait).

Le puppet applique ce calcul à sa corde droite (crochet − pointe), avec `cM_atan2s` 020195B0 et
`mDoMtx_YrotS` 025F1884 pour le sinus et le cosinus du cap. Le Newton de `ship_length` passe à 24
itérations, ce qui suffit pour une longueur de corde.

**Icônes de carte** : le menu Start met le jeu en pause, l'execute du Link local s'arrête, et le
réseau n'était donc plus lu. Le hook `updateProgressIcons` de la carte appelle maintenant
`multiplayer_pump`, qui reçoit les trames et les traite avec `handle_frame`, uniquement quand le
menu est ouvert. Les flèches utilisent la dernière position et le dernier cap reçus, et non la
position lissée (figée pendant la pause).

Testé : mod 0.2.17 traduit (66 fonctions, 29 hooks, appels 020195B0 et 025F1884 présents),
`mp_protocol_test`. Non testé en jeu.

### Cou du bateau, objets tenus et bottes du Link puppet (0.2.18, 16 octobre 2026)

**Cou du Lion Rouge** (vérifié dans `d_a_ship.cpp`, `headJointCallBack1` 02471C88) : les
articulations 2 à 7 de la tête tournent selon `m03A0` (+0x688, tangage) et `m03A2` (+0x68A, lacet
réparti par articulation). Le bloc détail gagne deux `s16` après `rope`, appliqués et lissés sur le
puppet. Ce bloc change de longueur : 0.2.17 et 0.2.18 sont incompatibles entre eux (client et
serveur Java à mettre à jour ensemble).

**Objets tenus** (vérifié dans `d_a_player_main_01.cpp`, `d_a_player_main_05.cpp`) :
`daPy_lk_c::makeItemType` 023DF600 crée le modèle de l'objet à partir de `mEquipItem`, dans le tas
d'objets du Link concerné. Le puppet applique l'objet du joueur distant (`ActorPlayerSync item`,
qui existait déjà) au changement :
- `deleteEquipItem(puppet, 0)` 023DC7AC, sans son, pour retirer un modèle créé ; jamais pour
  l'épée, qui coupe la musique d'épée ;
- puis `mEquipItem` = objet, et `makeItemType` pour longue-vue 0x20, Tingle Tuner 0x21,
  baguette 0x22, boîtes à images 0x23/0x26, arcs 0x27/0x35/0x36, marteau 0x33, bouteille 0x50 ;
- épée en main : `mEquipItem` 0x103, placée par `setItemModel` ;
- feuille Mojo en vol : 0x102 et `setParachuteFanModel` 0241A204, comme `procFanGlide_init`. Le
  proc lui-même n'est pas lancé (il consomme de la magie et fixe un statut du joueur 0, ce que le
  client GameCube compensait en remettant `mItemMagicCount` à 0) ;
- non montrés : grappin 0x25, boomerang 0x2D, hookshot 0x2F, bombes 0x31, qui sont des acteurs
  autonomes agissant pour le joueur 0 ; et le contenu des bouteilles.

**Bottes de plomb** : `mNoResetFlg0` (+0x3B8) 0x2000000 (vérifié dans le draw et `setItemModel`).
Ajout d'un bloc WWHD « extras » après le bloc bateau (un octet, bit 7 à 0, détecté comme le bateau ;
bit 0 = bottes). Le Java le lit s'il reste des octets et le réécrit. Le puppet lève ou baisse son
flag.

**Sons** : `setItemModel` règle le type de son des bottes, de l'épée et du bouclier, qui est un état
audio global (025E1E04, 025E1E14, 025E1E2C ; `u32` dans le SDK, 1 et 2 arguments dans le décomp).
Les puppets écrasaient celui du Link local à chaque pas. Ces trois fonctions sont maintenant sans
effet pendant la fin visuelle de l'execute d'un puppet.

Testé :
- `mp_protocol_test` : vecteurs avec extras, troncatures, bottes, détail avec cou ;
- mod 0.2.18 traduit (69 fonctions, 32 hooks) ;
- `ActorPlayerSync.java` compilé avec `-Xlint:all` ;
- aller-retour Java : fin de composant identique.

Non testé en jeu.

### Lame de l'épée, crash des objets, acteurs qui voient les puppets (0.2.19, 16 octobre 2026)

**Crash après plusieurs changements d'objet** (rapports `captures/crash-20261016-1053*.log`) :
« J3DPacket.cpp mEntryPtr == (0) », dans `makeItemType` → `set*Model` → `initModel` (création
J3D), appelé par le mod. Cause vérifiée dans le décomp :
- `setItemHeap` 023DDF64 choisit l'un des deux tas d'objets de Link, en n'en changeant qu'une fois
  par image (`mResetFlg0` +0x3C0, bit 0x4000) ;
- `setAnimeHeap` 023DDEA8 vide ce tas (`freeAll`) avant d'y créer le modèle ;
- l'execute du jeu remet `mResetFlg0` à 0 à chaque image (`d_a_player_main_04.cpp`), celui du
  puppet ne le faisait pas : chaque modèle était recréé dans le même tas, par-dessus les paquets
  du précédent encore inscrits dans les listes de dessin.

Corrigé : `mResetFlg0 = 0` à chaque execute du puppet.

**Lame de l'épée** : `setSwordModel(1)` 023DE0B4 crée la lame, l'éclat et la pointe dans le tas
d'objets, comme le jeu quand Link dégaine. Le puppet l'appelle pour 0x103. Avant chaque
`deleteEquipItem`, `mEquipItem` est remis à « aucun », pour ne pas passer par son cas épée (qui
coupe la musique d'épée).

**Acteurs** (`puppet_sense.h`, option `actors_see_puppets`, activée par défaut) : les acteurs
trouvent le joueur via le joueur 0 du registre (`play + 0x5B2C`), le plus souvent par un accès
inline. Pendant l'execute (`fopAc_Execute` 025D475C, 1 argument) d'un ennemi (`group` +0x2DA = 2)
ou d'un PNJ (4), le joueur 0 est le Link le plus proche, local ou puppet ; il reprend sa valeur
après l'appel. Rien n'est envoyé au serveur.

Effets connus, déduits et documentés :
- les attaques sur un puppet ne blessent personne (ses colliders ne sont pas enregistrés) ;
- un acteur qui garde le pointeur du joueur garde celui du puppet ;
- pendant cet execute, l'acteur compare les coups reçus avec le puppet ;
- un événement lancé pour « le joueur » se lance dans la partie locale ;
- les groupes joueur, objet et environnement ne sont pas touchés (objets de Link, bateau, portes,
  coffres, caméra).

Testé : mod 0.2.19 traduit (70 fonctions, 33 hooks, `fopAc_Execute` remplacé, appel 023DE0B4
présent), `mp_protocol_test`, manifest valide. Non testé en jeu.

### Builds sur deux PC : `build_mod.ps1` et `run_dev.ps1` portables (16 octobre 2026)

Les deux PC n'ont pas la même chaîne de compilation :
- PC 1 : runtime compilé avec MSYS2 MINGW64 (`D:\msys64\mingw64`, libstdc++) et `ld.lld` de
  devkitPro pour le mod ;
- PC 2 : MSYS2 CLANG64 (`C:\msys64\clang64`, libc++).

Le commit aacab04 avait figé `-MsysBin` sur CLANG64, si bien que `run_dev.ps1` ne trouvait plus
les DLL du runtime sur le PC 1.

Implémenté :
- `run_dev.ps1` : `-MsysBin` vide par défaut. Il prend alors le dossier du compilateur indiqué par
  `CMAKE_CXX_COMPILER` dans `<BuildDir>\CMakeCache.txt`, ce qui donne les DLL de ce build. L'option
  `-BuildMod` rebuilde et installe le mod d'abord ;
- `tools/multiplayer/build_mod.ps1` :
  - cherche un `clang` qui liste `ppc32` dans `--print-targets` : celui du build, puis CLANG64,
    puis MINGW64. Le clang de l'install (llvm-mingw) n'a pas la cible PowerPC (vérifié) ;
  - lie avec le `ld.lld` de l'install (vérifié : il lie l'ELF PowerPC) ;
  - vérifie la traduction (`guestmod.py`, Python de l'install) ;
  - copie le package dans `instances\<n>\user\ModManager\Mods\multiplayer-connection`, sans
    `mod.o` ;
  - le jeu traduit et compile `mod.elf` lui-même au démarrage (`GuestBuild`).

`mod.elf` n'est pas versionné : il faut le rebuilder après chaque pull.

Testé sur le PC 1 :
- `build_mod.ps1` : mod 0.2.20 du pull compilé, traduit (72 fonctions, 33 hooks) et installé dans
  l'instance 1 ;
- détection `-MsysBin` = `D:\msys64\mingw64\bin` (`libstdc++-6.dll` présente) ;
- les deux scripts s'analysent sans erreur ;
- `ninja wwhd` : seule l'empreinte de version est recompilée.

Non testé sur le PC 2.

### Acteurs placés synchronisés et multiplicateur décidé par le serveur (0.2.21, 16 octobre 2026)

Choix d'architecture : le modèle historique est conservé, car le serveur n'a pas de logique de
jeu. Le serveur (`SyncedActorManager` / `SyncedActorComponent`) donne chaque acteur au joueur le
plus proche, réévalue ce choix toutes les 2 s, gère les tickets et l'exclusivité ; le jeu du
propriétaire fait tourner l'acteur. Le client HD (`actor_sync.h`) corrige trois limites du client
GameCube, vérifiées dans ses sources (`ActorBaseSync.cpp`, `ActorHealthSync.cpp`, `ActorSync.cpp`) :
- il recopiait la vie du propriétaire à chaque image : le coup d'un non-propriétaire était
  effacé ;
- il n'interpolait la position qu'après l'execute de l'acteur ;
- `requestRemove` n'était jamais appelé : un ennemi tué restait chez les autres.

Vérifié dans les sources :
- serveur : `SyncedActorClient` (`CLIENT_ACTOR_EXCLUSIVE_ADD` change le propriétaire
  immédiatement ; une mise à jour à l'ancien ticket est ignorée) ; `SyncedActorComponent` (relais à
  tous sauf au propriétaire, ticket +1 et mise à jour complète au changement de propriétaire,
  `REMOVE` envoyé à tous) ; `SyncedActorServer.writeClientInit` (toutes les factories, y compris
  `DefaultCreators`) ;
- décomp HD :
  - `fopAcM_prm_class` fait 0x24 octets, avec deux octets de padding en 0x22/0x23 ;
    `fopAcM_CreateAppend` 025D5600 met le bloc à zéro ;
  - `dStage_actorCreate` 025C1154 prend 2 arguments ;
  - `fopAc_Create` 025D4C70 lit `mpUserData` (+0xAC) quand `mInitState` (+0xC) vaut 0 ;
  - `fopAc_Delete` 025D4B4C et `fopAc_Execute` 025D475C prennent 1 argument ;
  - `dStage_getName2` 025C1150, `fopAcM_delete_actor` 025D57E0 ;
  - `fopAc_ac_c::health` est un s8 en 0x3A1.

Implémenté :
- **Codec** (`mp_protocol.h`) :
  - lecture de toutes les factories (noms d'au plus 8 caractères ; une factory est utilisable si
    tous ses composants ont une taille fixe) ;
  - tailles : `ActorHealthSync` 6, `ChuChuSync` 3, `BokoblinSync` 6, `PropsSync` 5 ;
  - liste `ActorFilter` conservée (512 noms au plus) ;
  - écriture et lecture des créations et mises à jour d'acteurs placés ; `EXCLUSIVE_ADD/REMOVE`.
  Les composants GameCube spécifiques sont écrits à zéro et ignorés en lecture : leurs offsets
  GameCube ne sont pas portés.
- **Extension de protocole** (rétrocompatible) : le multiplicateur est une entrée
  `#enemy_multiplier=N` (1 à 15) de la liste `ActorFilter`. Le client GameCube ne fait que
  l'ajouter à son ensemble de noms, où aucun acteur ne correspond.
- **Identité d'un acteur** : le bloc de paramètres entier (0x24 octets) sert de game UID.
  `dStage_actorCreate` (remplacé) marque les acteurs placés : padding[0] = 0xA5, padding[1] =
  numéro de copie. Le client GameCube utilisait les 0x21 premiers octets ; ici `room_no` et la
  copie en font partie. Les acteurs créés à l'exécution (objets, effets, puppets) n'ont pas de
  marque et ne sont pas synchronisés, comme sur GameCube. Le nom vient de
  `dStage_getName2(procName, argument)` (comme `ACTORSYNC_GAME_ACTOR_TO_NAME`). Il est comparé à
  la liste et sert de nom de factory.
- **Multiplicateur intégré** : le mod `enemy-multiplier` est remplacé. Il ne doit plus être activé
  avec celui-ci : les deux remplacent `dStage_actorCreate`. Il était désactivé dans le profil de
  l'instance 1. Liste des 50 ennemis GameCube. Copies sur des anneaux de 60 unités (8 par anneau),
  une constante pour que tous les clients aient les mêmes positions, donc les mêmes UID. La
  valeur s'applique aux stages chargés après l'init du serveur.
- **Propriétaire** : il envoie la position (toutes les 0,1 s si elle change) et la vie / le statut
  (dès qu'ils changent).
- **Non-propriétaire** : position, angle et vie du réseau sont posés avant l'execute (où l'acteur
  calcule ses matrices et ses colliders) et la position de nouveau après. L'IA locale tourne
  quand même : animations, attaques sur le Link local. Sans mise à jour depuis 2 s, l'acteur
  tourne seul.
- **Coups** : une baisse de vie pendant l'execute d'un non-propriétaire est un coup de ce joueur.
  La vie locale est gardée 1,5 s, `EXCLUSIVE_ADD` est envoyé, et le joueur devient propriétaire
  et envoie la nouvelle vie. L'exclusivité est rendue (`EXCLUSIVE_REMOVE`) 3 s après le dernier
  coup. Même règle pour un objet non ennemi déplacé de plus d'une unité dans l'execute alors que le
  Link local est à moins de 250 unités (porté, poussé).
- **Mort** : un ennemi (`group` 2) supprimé avec une vie ≤ 0, alors qu'il en avait eu, envoie
  `CLIENT_ACTOR_SYNC_REMOVE`, s'il appartient au joueur ou si celui-ci l'a frappé. Sur
  `SERVER_ACTOR_SYNC_REMOVE`, les autres suppriment leur copie (`fopAcM_delete_actor`). Une
  suppression avec de la vie restante (déchargement d'une salle ou d'un stage) ne supprime rien
  chez les autres.
- **Option** `sync_actors` (activée par défaut).
- **`puppet_sense.h`** : son remplacement de `fopAc_Execute` devient une fonction appelée par
  celui d'`actor_sync.h`.
- **Serveur Java** : `ActorFilter` ajoute l'entrée du multiplicateur, lu dans le nouveau
  `resources/WWServer/settings.ktml` (`"enemyMultiplier": 1`) ou dans la propriété
  `-Dww.enemyMultiplier=N`, limité à 1–15.
- **`fake_server.py`** : factories de `actorCreators.ktml`, liste de `actorList.ktml`,
  `--multiplier`, et logique Java des acteurs placés (un acteur par game UID et par zone,
  propriétaire le plus proche toutes les 2 s, exclusivité, tickets, relais sauf au propriétaire,
  `REMOVE` à tous, perte des acteurs quand on quitte la zone).

Limites connues (non traitées dans cette étape) :
- pas de rejeu des coups du hit list GameCube (`SetAtTgCommonHitInf` : structures de collision HD
  non vérifiées) ; l'exclusivité le remplace ;
- un coup donné pendant que l'autre joueur détient l'exclusivité est perdu après 1,5 s ;
- les états spécifiques (Bokoblin, ChuChu, pots portés ou cassés) ne sont pas appliqués : il faut
  les retrouver dans les classes HD. Un pot cassé chez le propriétaire reste chez les autres ;
- pas d'animation de mort chez les non-propriétaires : leur copie disparaît au `REMOVE` ;
- les acteurs créés avant la réception de l'init serveur ne sont pas suivis ;
- `actor_status` est envoyé mais pas appliqué (il porte des bits d'état local : porté, accroché) ;
- le multiplicateur s'applique aussi aux boss de la liste GameCube (`Gnd`, `Bgn`, `Daiocta`…).

Testé :
- `mp_protocol_test` (gcc mingw32 `-Wall -Wextra -Werror`) : nouveau test `test_placed_actors`
  avec octets écrits à la main (création, mise à jour, exclusivité, création serveur, Default,
  factory inutilisable, troncatures, entrée multiplicateur) ;
- scénario Python à deux clients contre `fake_server.py` : création, doublon renvoyé sans solver,
  relais sauf au propriétaire, `EXCLUSIVE_ADD` (propriétaire B, ticket 2, mise à jour complète aux
  deux), ancien ticket ignoré, exclusivité tenue plus de 2 s, rendue puis reprise par le plus
  proche (ticket 3), `REMOVE` à tous ;
- le paquet d'init réel du faux serveur est lu par le codec C : 217 noms, 19 factories,
  multiplicateur 4 ;
- mod 0.2.21 compilé sans avertissement, traduit (82 fonctions, 36 hooks), installé dans les
  instances 1 et 3 ;
- `ActorFilter.java` compile avec `javac -Xlint:all`.

Non testé : le jeu (aucune partie lancée) ; le serveur Java en exécution. Un test ponctuel
d'`ActorFilter` hors serveur échoue avant le code modifié : le chargeur de ressources MMB ne
trouve pas `actorList` dans ce lanceur.

### Coups d'épée rejoués sur les acteurs, mort normale partout, hit stop retiré (0.2.22, 16 octobre 2026)

Retour du test de 0.2.21 : la synchronisation fonctionne, mais chez les autres joueurs les ennemis
ne mouraient pas normalement et ne lâchaient pas d'objets (ils disparaissaient au `REMOVE`), et
chaque coup d'épée gelait le jeu une image.

**Coups rejoués, comme le hit list GameCube** (`ActorPlayerSync.cpp` : `sendPlayerHit` puis
`SetAtTgCommonHitInf` chez les receveurs). Chaque client applique les mêmes coups : l'ennemi meurt
normalement partout (animation, objets lâchés localement chez chacun).

Vérifié dans le décomp HD :
- `cCcSState::SetAtTgCommonHitInf` 0200CFA8 (4 arguments, binding `wwhd_set_at_tg`) et
  `cCcSState::ChkAtTg` 0200D19C (1 argument ; il efface d'abord les coups de l'image avec
  `ClrAtHitInf/ClrTgHitInf`) ;
- `cCcD_Obj` : `mObjAt` à 0 (SPrm, RPrm, HitObj, vtable, Type 0x10, Atp 0x14), `mObjTg` à 0x18
  (HitObj 0x20), `mObjCo` 0x2C, `mStts` 0x44 ; `dCcD_GObjInf::mGObjAt` 0x50 (SPrm, EffCounter
  +0x14, Se/Mtrl/HitMark/Spl +0x1C, Vec +0x2C) ; `cCcD_Stts::mp_actor` 0xC ;
- `dComIfG_play_c::mCcS` = `wwhd_play_get()` + 0x26A4 ;
- les dégâts viennent du collider d'attaque (`at_power_check` : `mAtp`, `mAtType`), avec le
  traitement « épée » quand l'acteur du statut est un PLAYER (le puppet en est un) ;
- le filtre des coups continus (`dCcS::ChkAtTgHitAfterCross`) porte sur le statut de la cible, mis
  à jour chez le receveur comme pour un vrai coup.

Implémenté :
- **Capture** (`actor_sync.h`, `WWHD_HOOK_RETURN` 0200CFA8) : un collider d'épée du Link local
  (`mAtCps[0..2]` 0x7B1C, `mAtCyl` 0x78BC) touche un collider d'un acteur synchronisé (son statut
  pointe l'acteur, décalage < 0x10000) et le coup est pris (`mObjTg.mHitObj` est l'épée). Le coup
  part dans la prochaine mise à jour `ActorPlayerSync`.
- **Protocole** : bloc d'extension WWHD « strikes » après les coups d'épée. Il contient un octet de
  nombre, puis pour chaque coup :
  - uid de la cible (VLQ) et décalage du collider (VLQ) ;
  - index du collider, atp, se, mtrl, hit mark, spl (un octet chacun) ;
  - SPrm d'attaque, type, SPrm GObjAt (u32 chacun) ;
  - vecteur et position du coup (3 f32 chacun).

  Le hit list historique reste vide : ses receveurs rejouaient avec leur copie du collider du
  joueur, dont le puppet HD n'a pas les valeurs d'attaque.
- **Rejeu** (`WWHD_HOOK_RETURN` 0200D19C, quand `self` est le `mCcS` du play) : un `dCcD_Cps`
  à nous (même construction que `sword_swing.h`) prend les valeurs reçues, avec le statut du
  puppet du frappeur (à défaut, celui du Link local). Puis `SetAtTgCommonHitInf(mCcS, ce
  collider, collider de la cible, position)`. La cible réagit à son execute suivant comme à un coup
  local.
- **Vie** : celle du propriétaire n'est appliquée que si elle est plus basse que la copie (coup
  manqué), jamais pour soigner, jamais sur une copie à 0. Une baisse de vie juste après un coup
  rejoué (0,5 s) ne déclenche pas de demande d'exclusivité.
- **`REMOVE`** : une copie déjà mourante finit sa mort ; une copie encore vivante est supprimée
  après 3 s.
- **Hit stop** : il était retiré sur GameCube (`project.py`, « timer pause » à 0x80239608).
  `cc_at_check` écrit `nextPauseTimer` (0x101EACB7) ; `dScnPly_Execute` saute la scène tant que
  `dScnPly_ply_c::calcPauseTimer` (025B02E8) rend non zéro. Ce dernier est remplacé : les deux
  compteurs (`l_timers` 0x101EACB6, deux s8) sont remis à zéro et il rend 0. Option `hit_stop`
  (désactivée par défaut) pour garder celui du jeu.
- **Serveur Java** (`ActorPlayerSync.java`) : il relaie désormais les coups d'épée et les
  strikes, cumulés entre deux écritures comme les autres listes d'événements, puis vidés. Avant,
  le serveur Java perdait le bloc des coups d'épée (les herbes coupées ne passaient que par le
  faux serveur). Le faux serveur relaie les octets bruts : rien à changer.

Limites connues :
- seuls les coups de l'épée (et du marteau, qui passe par les mêmes colliders) sont envoyés ; pas
  les bombes, flèches, boomerang ni grappin ;
- les dégâts bonus des attaques tournoyantes lisent le Link local chez le receveur. La vie du
  propriétaire corrige l'écart vers le bas ; une copie restée vivante est supprimée au `REMOVE` ;
- les objets lâchés ne sont pas synchronisés : chaque joueur a les siens ;
- tous les joueurs doivent avoir 0.2.22 : un mod plus ancien refuse le nouvel octet.

Testé :
- `mp_protocol_test` : vecteurs dorés mis à jour, plus le nouveau `test_strikes` (octets écrits à
  la main, aller-retour, troncatures, bornes, ajout) ;
- une mise à jour pleine (listes, coups d'épée et strikes au maximum) tient dans 4 Kio ;
- aller-retour réel C → Java → C : un `ActorPlayerSync` avec un coup d'épée et deux strikes, écrit
  par le codec C, est relu puis réécrit à l'identique par `ActorPlayerSync.java` (229 octets) ;
  l'écriture suivante n'en contient plus ;
- mod 0.2.22 compilé sans avertissement, traduit (87 fonctions, 39 hooks), installé dans les
  instances 1 et 3 ; `javac -Xlint:all` sans avertissement.

Remarque : le serveur Java réécrit un `proc` à -1 en VLQ de 10 octets, que le codec C refuse.
C'est un comportement antérieur, sans effet en jeu (`mCurProc` n'est jamais -1). Il n'a pas été
modifié.

Non testé en jeu.

### Acteurs créés en jeu : objets lâchés, bombes, flèches, boomerang (0.2.23, 16 octobre 2026)

But : synchroniser les acteurs que le jeu crée pendant la partie avec `fopAcM_fastCreate` /
`fopAcM_create` (le « FastCreate Item Sync » du TODO GameCube). Les objets lâchés par les ennemis
étaient tirés au hasard dans chaque jeu (`fopAcM_createItemFromTable` : `cM_rndF`). Les armes des
autres joueurs n'existaient pas.

Vérifié dans le décomp HD :
- `fopAcM_CreateAppend` 025D5600 fait le bloc de paramètres de toutes les fonctions de création
  (`createAppend`) ;
- `fopAcM_create` 025D5834 (8 arguments en registres) est une requête : l'id rendu par
  `fpcSCtRq_Request` est celui que portera le process (`fpcBs_Create(name, id, data)` dans
  `f_pc_stdcreate_req.cpp`) ;
- un ennemi mort crée `daDisappear` (`fopAcM_createDisappear`), qui crée l'objet plus tard dans son
  propre execute (`d_a_disappear.cpp`) ;
- Link crée bombe, boomerang et grappin comme acteurs tenus (`LK_fastCreate`,
  `d_a_player_main_01.cpp`) et la flèche à l'armement (`d_a_player_main_02.cpp`) ;
- bombe, flèche et boomerang lisent le joueur : `play + 0x5B2C` (`mPlayerInfo[0].mpPlayer`) et
  `+ 0x5B34` (`mpPlayerPtr[0]`) ;
- numéros de process : item 0xFF, bombe 0x126, Bomb2 0x127, flèche 0x1D8, boomerang 0x1B0.

Implémenté :
- **Protocole** :
  - nouveau composant `ActorSpawnSync` (extension WWHD, 37 octets : process, paramètres,
    argument, angle, échelle, salle, speedF, speed, gravity) ;
  - factory `Spawned` (`ActorBaseSync`, `ActorSpawnSync`) dans `actorCreators.ktml` et le faux
    serveur ;
  - création sans game UID (`mp_write_actor_create_named`) ;
  - les strikes portent `attacker` (uid de l'arme, 0 pour l'épée) et le décalage du collider
    d'attaque.
- **Serveur Java** : `ActorSpawnSync.java` (octets gardés tels quels) ; `ActorPlayerSync.java` lit
  les deux nouveaux VLQ des strikes.
- **Faux serveur** : acteurs `Spawned` gardés par zone et envoyés aux clients qui entrent ensuite
  (comme `SyncedActorManager.addClient`).
- **Contexte de création** (`actor_sync.h`) : pendant l'execute ou la suppression d'un acteur, le
  contexte vaut :
  - `LOCAL` pour le Link local ;
  - `OWNED` pour un ennemi synchronisé dont on est propriétaire, ou un acteur spawné à soi ;
  - `FOREIGN` pour un ennemi d'un autre joueur, ou une copie ;
  - celui hérité d'un acteur non synchronisé créé en `OWNED/FOREIGN` (la fumée de mort).

  `fopAcM_CreateAppend` est remplacé pour écrire le contexte dans le padding du bloc (0xA6,
  contexte).
- **Objets lâchés** :
  - en contexte `FOREIGN`, l'objet est gelé puis supprimé à son premier execute ;
  - en `LOCAL/OWNED`, il est annoncé ; les autres créent une copie (`fopAcM_create` avec les
    paramètres reçus, retrouvée par son id dans `fopAc_Create`), lui donnent ses vitesses et
    suivent les positions du propriétaire ;
  - un objet supprimé à moins de 300 unités du Link local (ramassé) est retiré pour tous.
- **Armes du Link local** (bombes, flèches, boomerang) :
  - annoncées et tenues en exclusivité par le créateur ; retirées à leur suppression chez lui ;
  - chez les autres, la copie se crée et tourne avec le puppet du créateur comme joueur 0 et Link
    (sans puppet, pas de copie) ;
  - ses coups ne touchent rien (`SetAtTgCommonHitInf` est maintenant remplacé et les saute) ;
  - les coups de l'arme du créateur sur un acteur synchronisé partent en strikes, rejoués avec le
    collider de la copie, qui reçoit les valeurs d'attaque le temps de l'appel puis les retrouve.
- Les bombes des ennemis restent locales : elles blessent le Link de chaque jeu.

Limites connues :
- le grappin n'est pas partagé ;
- une copie d'arme peut se supprimer elle-même si son code exige d'être tenue par le joueur. Ce
  n'est pas vérifié pour chaque état : elle disparaît alors chez l'autre, sans autre effet ;
- l'explosion d'une bombe distante ne casse pas les objets non synchronisés et ne blesse personne
  localement ;
- si le propriétaire de l'ennemi ne le voit pas mourir, personne n'a l'objet ;
- un objet qui expire près du Link local est aussi retiré pour les autres (sans gêne : ils
  expirent ensemble).

Testé :
- `mp_protocol_test` : création spawnée (octets écrits à la main), lecture serveur, troncatures,
  mise à jour de position seule, strike de bombe ;
- scénario Python contre `fake_server.py` : création sans game UID, relais aux autres avec le
  composant entier, envoi au client qui arrive ensuite, ramassage retiré pour tous, bombe en
  exclusivité (gardée plus de 2 s malgré un joueur plus proche) et mises à jour relayées ;
- aller-retour C → Java → C d'un `ActorPlayerSync` avec un strike de bombe : identique
  (235 octets) ;
- mod 0.2.23 compilé sans avertissement, traduit (92 fonctions, 40 hooks), installé dans les
  instances 1 et 3 ; `javac -Xlint:all` sans avertissement.

Non testé en jeu.

### Flèches tirées et élémentaires, herbe, fleurs et boules d'objets (0.2.24, 16 octobre 2026)

Retour du test de 0.2.23 :
- les bombes fonctionnent ;
- les flèches restaient collées au modèle du puppet ;
- les objets lâchés ne sont pas toujours les mêmes ; l'herbe coupée et les boules d'objets
  (`Iball`) n'étaient pas synchronisées.

Vérifié dans le décomp HD :
- **Flèches** (`d_a_arrow.cpp`) :
  - `procWait` place la flèche à la main gauche du joueur 0 (`setKeepMatrix` : matrice du modèle
    et `current.pos`) jusqu'à ce que ses paramètres valent 1 (posés par Link au tir), puis
    `procMove` ;
  - l'élément `mArrowType` (+0x71D) est fixé à la création par `setTypeByPlayer` 02052420, selon le
    choix local (`l_keepType` 101D5F43) et la magie locale ;
  - un changement d'élément crée une nouvelle flèche (`changeArrowType`) ;
  - `arrowUseMp` 02053AD0 retire la magie du joueur local au tir.
- **Herbe et fleurs** : `dGrass_data_c::WorkAt` 0254A7C8 et `dFlower_data_c::WorkAt` 025453B8
  (5 arguments) reçoivent l'acteur qui coupe (`dCcMassS_Mng::Chk`) et lâchent un objet par
  `fopAcM_createItemFromTable`, qui dépend de la vie du joueur. Chaque jeu tirait donc son
  propre objet.
- **Boules d'objets** : `daIball` (process 0x18F, `d_a_ib.cpp`) est créée par
  `fopAcM_createIball` (fumée de mort) et crée ses objets quand on la frappe (`mCyl.ChkTgHit`).

Implémenté :
- **Protocole** : `ActorSpawnSync` passe à 38 octets avec `variant` (l'élément de la flèche).
  Le composant peut aussi partir dans une mise à jour : le propriétaire le renvoie quand les
  paramètres de sa flèche changent, c'est-à-dire au tir. Java (`BYTES = 38`), faux serveur et
  tests mis à jour.
- **Flèches** :
  - la copie prend l'élément du créateur (`setTypeByPlayer` remplacé pour les copies) et ne
    consomme pas la magie locale (`arrowUseMp` remplacé) ;
  - quand les paramètres reçus valent 1, ils sont écrits dans la copie, qui est tirée ;
  - les flèches élémentaires sont donc partagées ; chaque changement d'élément annonce une
    nouvelle flèche et retire l'ancienne.
- **Herbe et fleurs** : la coupe se fait dans le contexte `LOCAL` quand le Link local coupe,
  `FOREIGN` quand c'est un puppet (coup d'épée rejoué). L'objet de celui qui coupe est partagé ;
  les autres suppriment le leur.
- **Boules d'objets** : elles suivent la règle des objets. Seule celle du propriétaire de
  l'ennemi existe partout. Les coups dessus partent en strikes (c'est un acteur synchronisé), donc
  elle casse dans tous les jeux. Les objets d'une copie (contexte `FOREIGN`) sont supprimés et
  remplacés par ceux du propriétaire.
- **Garde** : sans factory `Spawned` sur le serveur, aucun objet n'est plus supprimé localement.
  Avant, les objets des ennemis des autres disparaissaient sans être remplacés.

Limites connues :
- sans l'option puppet, les coups d'épée rejoués sur l'herbe ont le Link local pour acteur : l'herbe
  donne alors un objet chez chacun ;
- buissons et pots (acteurs placés non ennemis) : leurs objets restent propres à chaque jeu ;
- après le tir, la copie de flèche vole seule et suit aussi les positions du propriétaire
  (10 par seconde, interpolées).

Testé :
- `mp_protocol_test` : composant à 38 octets, mise à jour position + spawn d'une flèche tirée ;
- scénarios Python (acteurs placés et spawnés) contre le faux serveur ;
- mod 0.2.24 compilé sans avertissement, traduit (96 fonctions, 44 hooks), installé dans les
  instances 1 et 3 ; `javac -Xlint:all` sans avertissement.

Non testé en jeu. Le serveur Java doit être relancé avec les nouvelles ressources (factory
`Spawned` de `actorCreators.ktml`) et les classes recompilées.

### Menu Start (déconnexion), flèches vers le sol, bombes du canon (0.2.25, 16 octobre 2026)

**Menu Start** : un joueur qui ouvrait le menu, surtout sur le bateau, disparaissait pour les
autres. Cause vérifiée :
- pendant la pause, l'execute du Link local (où le mod lit le réseau) ne tourne pas ;
- le transport du runtime ferme la connexion quand 256 paquets reçus attendent
  (`network_transport.cpp`, `kMaxQueuedFrames`) ;
- sur le bateau, le trafic (bateau, acteurs de la mer) remplit cette file en quelques secondes.

Correction dans le mod : `WWHD_HOOK` à l'entrée de `dScnPly_Execute` (025B0314, 1 argument),
exécuté à chaque image, menu ouvert compris (`d_s_play.cpp` : ses tests `dMenu_flag` /
`isPause` sont à l'intérieur), qui appelle `multiplayer_pump`. Le runtime n'est pas modifié.

**Flèches tirées vers le sol** : la direction du tir est `current.angle`, que `procWait` prend de
la main gauche du joueur 0 (`setKeepMatrix`). Chez Link, la visée tourne le haut du corps par code,
pas par animation : la main du puppet ne vise donc pas. Corrections :
- avec le tir, la copie reçoit la position et les angles de la flèche du propriétaire (envoyés
  avec les paramètres à 1) ;
- `daArrow_c::arrowShooting` (02053814) est remplacé pour les copies : il pose ces valeurs
  (`current.angle.x = -shape_angle.x`, comme `setKeepMatrix`) avant le calcul de la vitesse ;
- la copie vole ensuite seule, sans être recalée sur les positions du propriétaire.

**Bombes du canon** : le canon crée sa bombe dans l'execute du bateau (`d_a_ship_exec.cpp`,
`fopAcM_fastCreate` de bombe, vitesses et gravité données après). Corrections :
- le bateau du joueur local (`play + 0x5B3C`, `mpPlayerPtr[2]`) s'exécute dans le contexte
  `LOCAL`, comme Link : ses bombes sont annoncées comme ses armes ;
- les copies (sauf flèches et boules) reçoivent les vitesses et la gravité transmises ;
- le contexte est calculé avant l'execute, donc avant que `ship_puppet.h` échange le pointeur du
  bateau pour un bateau puppet : un bateau puppet n'est pas `LOCAL`.

Testé : mod 0.2.25 compilé sans avertissement, traduit (100 fonctions, 46 hooks), installé dans
les instances 1 et 3. Non testé en jeu.

### Objets portés et randomizer de variantes d'ennemis (0.2.26, 16 octobre 2026)

**Objets portés** (le `PropsSync` GameCube : pots, cailloux, bombes). Chez les autres, la copie
d'un objet tenu par un joueur subissait la gravité. Vérifié dans le décomp :
- Link qui tient un acteur met le bit `fopAcStts_CARRY_e` (0x2000) de `actor_status` ; le grappin
  met `fopAcStts_HOOK_CARRY_e` (0x100000) (`include/gc_api/f_op/f_op_actor.h`) ;
- l'acteur ne tombe plus et laisse son porteur le placer : `d_a_tsubo_mode.cpp` teste
  `actor_status & 0x102000`, `d_a_bomb.cpp` teste `& 0x2000`.

Implémenté :
- une copie qui suit son propriétaire reçoit ses bits de portage, avant et après son execute ;
- pour un acteur placé, ils viennent du statut de son `ActorHealthSync` (reçu mais non appliqué
  jusqu'ici) ; pour un acteur spawné, de `mp_spawn::variant` (bit 0 portage, bit 1 grappin), que le
  propriétaire renvoie quand il change ;
- les champs propres au `PropsSync` GameCube (état, lancé, au sol, angle du modèle) étaient des
  offsets GameCube : ils restent écrits à zéro et non appliqués.

**Randomizer de variantes**. Il remplace le randomizer GameCube, qui mettait `0xFFFF0000 |
aléatoire` dans les paramètres. Réglage du serveur : `settings.ktml` `"enemyRandomizer"` ou
`-Dww.enemyRandomizer=1`, transmis comme entrée `#enemy_randomizer=1` d'`ActorFilter` (codec, Java,
faux serveur `--randomizer`). Le fonctionnement :
- les copies du multiplicateur prennent, champ par champ, une valeur de la liste `as_variants`
  (`actor_sync.h`) ;
- un champ n'est changé que si la valeur de l'original est dans sa liste ;
- le tirage est un hash FNV-1a du bloc d'origine et du numéro de copie : identique sur tous les
  clients, donc les game UID restent les mêmes.

Listes, vérifiées dans la création de chaque acteur :

| Ennemi | Source | Champ | Valeurs |
|---|---|---|---|
| Bokoblin | `d_a_bk.cpp` | bits 5-7 (bit 5 : 7 PV au lieu de 5 ; bits 6-7 : arme 1 ou 7, sinon 0) | 0x00 à 0xA0 |
| Bokoblin | `d_a_bk.cpp` | bits 0-3 (comportement : porteur, chemin, événements) | gardé |
| ChuChu (5 noms) | `d_a_cc.cpp` | couleur, bits 8-15 | 0 à 4 |
| Darknut | `d_a_tn.cpp` | comportement, bits 0-3 | 0 (rôde) ou 4 (garde) seulement |
| Darknut | `d_a_tn.cpp` | couleur d'armure, bits 4-7 | 0 à 5 |
| Keese | `d_a_ki.cpp` | dégâts de feu, bit 15 | 0, 1 |
| Octorok | `d_a_oq.cpp` | projectile, bits 8-15 | 0, 1, 0xFF |

Les copies des autres ennemis gardent les paramètres de l'original. Moblin, Peahat, Magtail,
Miniblin et Wizzrobe ont été examinés : leurs octets de type sont liés au placement (lanterne,
mer ou terre, trajet, invocation). Ils ne sont pas listés tant que des valeurs sûres ne sont pas
vérifiées.

Testé :
- `mp_protocol_test` (entrée randomizer) ;
- faux serveur compilé ;
- `javac -Xlint:all` (`ActorFilter`) ;
- mod 0.2.26 compilé sans avertissement, traduit (113 fonctions, 46 hooks), installé dans les
  instances 1 et 3.

Non testé en jeu.

### Morts naturelles forcées, feuille Deku, particules, marteau (0.2.27, 16 octobre 2026)

**Ennemis qui ne mouraient pas.** Cause probable trouvée :
- la vie du propriétaire, appliquée quand elle est plus basse, pouvait mettre une copie à 0 PV
  sans passer par le code de dégâts ;
- or seul ce code (`cc_at_check` puis la réaction de l'ennemi) lance la mort : la copie restait
  vivante à 0.

Corrections :
- la vie reçue n'est plus écrite sous 1 ;
- une copie morte chez son propriétaire (vie reçue à 0 ou `REMOVE`) mais vivante ici reçoit un
  coup mortel rejoué : le dernier strike vu sur elle, avec 127 de dégâts, par le chemin des vrais
  coups (`as_replay`), toutes les 0,3 s tant que son collider refuse les coups. Elle meurt donc à
  la manière du jeu (animation, objets) ;
- elle est supprimée 4 s après le `REMOVE` si elle vit encore.

**Feuille Deku.**
- Modèle : pendant le coup de feuille (`daPyProc_FAN_SWING_e` 0x92), Link crée le grand éventail
  avec `setFanModel` (023E7660, `d_a_player_main_02.cpp`) ; `makeItemType` ne crée rien pour la
  feuille en HD. Le puppet prend un pseudo-objet `PUPPET_ITEM_FAN_SWING` quand le proc distant
  vaut 0x92 : `mEquipItem` = 0x34, puis `setFanModel`.
- Vent : `daPy_lk_c::mFanWindCps` (+0x7EC4, `d_a_player_main.h`). Il est capturé comme un coup
  d'épée de plus (`sword_swing.h`, son type et son vecteur), donc l'herbe et les fleurs se
  couchent chez les autres. Il part aussi en strike (index 4) sur les acteurs synchronisés. Les
  objets non synchronisés (torches, etc.) ne sont pas soufflés chez les autres.

**Particules** (`particle_sync.h`).
- Toute particule passe par `dPa_control_c::set` (025A847C, 12 arguments dont 4 sur la pile,
  `include/bindings.h` du décomp), qui est remplacé.
- Une émission faite dans le contexte `LOCAL` (Link local ou son bateau), sans callback et à vie
  finie (`mMaxFrame` +0x5C > 0 juste après la création), part dans l'`ActorPlayerSync` :
  nouveau bloc WWHD après les strikes (groupe, id, drapeaux, position, angle et échelle
  optionnels, alpha). Les autres la recréent au même endroit.
- Les émissions sans fin, que le code qui les crée arrête lui-même, ne sont pas copiées. Les
  matrices posées après la création (anneaux du marteau, `setHammerQuake`) gardent l'angle de
  création.
- Les étincelles d'un coup rejoué viennent déjà du code de coup du jeu
  (`dCcS::ProcAtTgHitmark`).
- Serveur Java : relaie le bloc de particules ; faux serveur : octets bruts.

**Marteau au sol** : couvert par les particules (`setHammerQuake` 0243BFC8 : `HM_SENKO00`,
`HM_SENPU00`, `HM_SYOGEKI00`, poussière du sol). L'étourdissement des ennemis proches n'est pas
envoyé.

Testé :
- `mp_protocol_test` : vecteurs dorés mis à jour, bloc de particules écrit à la main ;
- aller-retour C → Java → C d'un `ActorPlayerSync` avec coup d'épée, strikes et deux particules :
  identique (288 octets) ;
- mod 0.2.27 compilé sans avertissement, traduit (116 fonctions, 47 hooks), installé dans les
  instances 1 et 3 ; `javac -Xlint:all`.

Non testé en jeu. Remplacer `dPa_control_c::set` (arguments sur la pile) est un point à
surveiller au premier lancement.

### Particules à leur vraie place, Bokoblin rose (0.2.28, 16 octobre 2026) ; étude des skins

**Particules** : retour de 0.2.27, seule la particule de l'épée dégainée apparaissait, sous le
puppet. Cause : le code de Link recale ses émetteurs sur une matrice (main, épée, sol) juste après
`dPa_control_c::set` (`JPASetRMtxTVecfromMtx`, `setHammerQuake`). La position de création (ses
pieds) n'était donc pas la bonne. Corrections :
- un émetteur créé pendant l'execute du Link local ou de son bateau est noté ;
- juste après cet execute (`ps_flush`, appelé depuis `as_execute`), on relit sa rotation et sa
  position globales (`JPABaseEmitter` : `mGlobalRotation` +0x1F0, matrice 3×4 ;
  `mGlobalTranslation` +0x22C ; vérifié dans `include/JSystem/JParticle/JPA_emitter_calc.h`) ;
- l'enregistrement porte la matrice (drapeau `MP_PARTICLE_MATRIX`, 9 floats ; Java mis à jour) ;
- le rejeu crée l'émetteur puis lui pose cette rotation ;
- un émetteur sans fin (`mMaxFrame` 0, `doTerminationProcess`) devient une salve de 30 images ;
- le filtre sur la durée et l'exclusion des émetteurs à callback sont retirés : la position
  finale suffit.

**Bokoblin rose** (`d_a_bk.cpp`) :
- le type 0xB devient type 4 avec `m02DC = 1` : modèle `BK_PINK` dans `useHeapInit`, pas d'arme,
  pas de bit « renforcé » ;
- le randomizer tire maintenant aussi le type du Bokoblin parmi 0, 4 et 0xB, seulement quand
  l'original en a un de ces trois.

Testé :
- `mp_protocol_test` (matrice, identité sans drapeau, drapeau inconnu refusé) ;
- aller-retour C → Java → C avec une particule à matrice : identique (324 octets) ;
- mod 0.2.28 compilé sans avertissement, traduit (116 fonctions, 47 hooks), installé dans les
  instances 1 et 3.

Non testé en jeu.

**Skins / modèles personnalisés (étude, rien d'implémenté)**. Vérifié :
- le gestionnaire de mods accepte déjà des paquets de contenu (`docs/mod-manager.md`,
  « Existing model, texture and UI mods ») qui remplacent des fichiers du jeu, archives de modèles
  comprises. Mais ce remplacement est local et vaut pour tous les Link : le joueur local et ses
  puppets partagent les données de l'archive « Link » (`puppet.h`) ;
- le mod clone déjà, par puppet, la texture de tunique (`tunic.h`) : la voie technique d'une
  texture propre à chaque joueur existe.

Deux niveaux possibles :
1. **Skins de texture par joueur** (faisable avec l'existant) : remplacer la texture du corps d'un
   puppet par une image fournie par le joueur, au lieu de la recoloration. Il faut un format de
   skin (PNG converti au format de la texture du jeu, mêmes dimensions), un identifiant de skin
   dans `ActorPlayerSync`, et un moyen de transmettre l'image (dossier partagé ou transfert par
   le serveur, une texture faisant quelques centaines de Kio).
2. **Modèles 3D par joueur** (plus lourd) : charger une archive supplémentaire et construire les
   modèles d'un puppet depuis elle, au lieu de l'archive « Link ». Le modèle doit garder le
   squelette de Link (mêmes joints, que suivent les animations et le code des objets tenus). Cela
   demande d'identifier dans le décomp le chargement d'archive (`dRes_control`) et la construction
   des modèles (`initModel`, `playerInit`), et de vérifier que le système de contenu peut ajouter
   une archive nouvelle.

Rien ne doit venir des fichiers du jeu dans le dépôt : les skins sont fournis par les joueurs.

### Tenue au choix : tunique ou « pyjama » (0.2.29, 16 octobre 2026)

Vérifié dans le décomp (`d_a_player_main_04b.cpp` `playerInit` 02410BE8) :
- toutes les textures `linktexS3TC` du modèle Link **partagé** reçoivent l'en-tête de la tenue
  de l'île (ressource 0x71 de l'archive « Link ») ;
- `mpCurrLinktex` (+0x454) liste ces entrées ;
- sauf tenue de l'île (événement 0x2A80 non obtenu, ou octet de sauvegarde +0x1C0 non nul :
  `mNoResetFlg1` +0x3BC |= `daPyFlg1_CASUAL_CLOTHES` 0x8), les entrées sont échangées avec les
  copies d'en-têtes de 0x24 octets à +0x48C ;
- règle qui en découle : les entrées portent la tenue portée, les copies l'autre ;
- `playerDelete` rééchange si le drapeau est mis (`d_a_player_main_04.cpp`) ;
- `daPy_lk_c::draw` (023D9820) masque le chapeau et la boucle selon ce drapeau
  (`hideHatAndBackle`) ;
- `LinkUpdateApi::swapTexture` (0272B8B8) n'a ni source ni binding : il n'est pas utilisé.

Implémenté (`outfit.h`) :
- `outfit_swap` refait l'échange de `playerInit` : en-têtes entiers, puis masque dirty par
  `tunic_mark_dirty` (comme `setResTIMG`). La règle entrées = tenue portée est conservée ;
- **Link local** : à chaque pas, si la tenue choisie diffère de la tenue portée, échange et bascule
  du drapeau (hors connexion aussi). `playerDelete` restaure ensuite normalement ;
- **puppet** :
  - son drapeau suit la tenue du joueur distant (son chapeau, au dessin) ;
  - s'il porte l'autre tenue que le Link local, l'échange du Link local est fait seulement pendant
    son dessin (`puppet_draw`, avant les copies de couleur de tunique), puis défait ;
- **protocole** : bit `extras` 0x02 `MP_EXTRA_CASUAL` (la tenue réellement portée). Le serveur Java
  garde déjà l'octet `extras` (& 0x7F) ;
- **réglages** : option `outfit` (`auto` = la sauvegarde, `hero`, `casual`) et clé `outfit` de
  `launcher.cfg` (`launcher_cfg.h`, validation `HdModBridge`, liste déroulante « Clothes » dans
  `LauncherUI`, appliquée aussitôt et avec SAVE PROFILE).

Incertitude, la même que pour les couleurs de tunique : l'échange pendant le dessin du puppet
suppose que le J3D HD lit les en-têtes de texture et la visibilité du chapeau au moment du dessin.
L'utilisateur a constaté que les couleurs par joueur fonctionnent, ce qui va dans ce sens.

Testé :
- `mp_protocol_test` (`outfit` dans `launcher.cfg`, valeurs refusées) ;
- `javac -Xlint:all` (`LauncherUI`, `HdModBridge`) ;
- mod 0.2.29 compilé sans avertissement, traduit (117 fonctions, 47 hooks), installé dans les
  instances 1 et 3.

Non testé en jeu.

### Launcher JavaFX refait : serveur intégré, console, mutators, pyjama (16 octobre 2026)

`LauncherUI.java` est réécrit. Les éléments factices sont retirés ou rendus fonctionnels :
- boutons serveur qui n'affichaient qu'un message ;
- métriques fixes ;
- liste « Appearance » sans effet ;
- bouton « Reload area » (clé refusée par `HdModBridge`) ;
- pied de page « Connected / Ready » sans connexion ;
- bouton de sauvegarde des mutators sans effet.

**Pages** :
- **Play Online** : adresse, port, fichier `launcher.cfg` (BROWSE) et SAVE CONNECTION, plus
  « USE MY OWN SERVER » et trois cartes d'étapes.
- **Prepare Your Voyage** :
  - nom ; couleurs de tunique avec aperçu, APPLY, RANDOMIZE, ORIGINAL ; couleur du bateau avec
    aperçu ;
  - interrupteurs **pyjama** (`outfit=casual`, sinon `hero`), « afficher les autres en Link »
    (`puppet`) et « cacher les noms », enregistrés aussitôt dans `launcher.cfg` ;
  - les couleurs enregistrées sont relues au chargement de la page.
- **Server** :
  - cartes d'état : pastille, joueurs, durée, ennemis, port ;
  - onglet CONSOLE : START / STOP / RESTART, console en direct (suivi automatique, CLEAR, COPY,
    limite de 200 000 caractères), conservée quand on change d'onglet ou de page ;
  - onglet MUTATORS : multiplicateur 1 à 15 et randomizer, mémorisés dans les `Preferences`,
    SAVE MUTATORS et SAVE & RESTART SERVER ; PVP, environnement, météo et heure affichés
    « SOON ».
- Barre latérale avec l'état du serveur ; pied de page avec le fichier lié et l'état du serveur ;
  messages qui disparaissent après 3,5 s. Styles ajoutés à `launcher.css` : ombres, pastilles,
  console sombre, interrupteurs, badge SOON.

**`ServerProcess.java`** (nouveau) :
- lance le serveur comme processus séparé : même `java`, même classpath, classe `WWServer`,
  avec `-Dww.enemyMultiplier` et `-Dww.enemyRandomizer` ;
- envoie la sortie ligne à ligne à la console ;
- compte les joueurs grâce à « joined » (nouvelle ligne de `WWClient.receiveInit`) et « left »
  (`WWServer.onRemoveChild`) ;
- un démarrage raté (exception du thread `main`, par exemple le port déjà pris) arrête le
  processus, que les threads de MMB garderaient sinon vivant, avec un message clair ;
- le serveur est arrêté à la fermeture du launcher.

Vérifié :
- `MMBSystem.openFile` résout les chemins depuis la racine du **lecteur courant**. Le serveur doit
  donc être lancé depuis le lecteur où se trouvent ses ressources : c'est le cas sur `D:`, et c'est
  ce qui faisait échouer mes tests précédents depuis `C:\…\Temp` ;
- MMB charge sa configuration (`MMBSystem/resourceActions.ktml`) depuis les ressources de `mmb`,
  copiées par Maven dans ses classes.

Testé :
- `javac -Xlint:all` sans avertissement (`LauncherUI`, `ServerProcess`, `WWClient`) ;
- dans une copie de build sur `D:` (`ZeldaWWHDRecomp/build/wwserver-test`, tous les modules
  compilés, ressources copiées comme Maven ; dossier jetable) : le vrai serveur démarre via
  `ServerProcess` et affiche « Enemy multiplier : 2/3, randomizer : … », preuve que les propriétés
  arrivent jusqu'à `ActorFilter` ;
- la sortie est capturée ; le port 10014 déjà occupé (un `fake_server.py` en cours) est détecté,
  le processus arrêté et l'état revient à STOPPED.

Non testé : l'interface JavaFX à l'écran, un serveur démarré avec le port libre et des joueurs
réels.

### Déconnexions avec le serveur Java, statut au redémarrage (0.2.30, 16 octobre 2026)

**Déconnexions**. Le journal de l'instance 1 montrait `multiplayer: malformed actor update` juste
après l'arrivée d'un joueur, puis la coupure. Cause vérifiée :
- `ActorPlayerSync.java` relit les VLQ 32 bits des clients par `readVLQInt` (int signé) et les
  réécrivait par `writeVLQ(long)` ;
- une valeur négative (fin d'animation -1 de `setSingleMoveAnime`, `proc` -1) devenait un VLQ de
  10 octets, que `mp_read_vlq` refuse (5 au plus) ;
- le faux serveur relaie les octets bruts : le problème n'apparaissait qu'avec le serveur Java,
  maintenant lancé par le launcher.

Corrections :
- les 18 écritures VLQ de champs `int` d'`ActorPlayerSync` passent par `writeVLQ32`
  (`value & 0xFFFFFFFFL`) ;
- côté mod, un paquet d'acteur illisible (création ou mise à jour) est journalisé (uid, taille,
  nombre ; les 5 premiers puis un sur 100) et ignoré, au lieu de couper la connexion. Les trames
  sont délimitées, donc la suivante est lue normalement. Les erreurs d'init et de trame restent
  fatales.

**Statut OFFLINE après RESTART**. Le gestionnaire de fin du processus terminé et le démarrage du
suivant s'exécutaient dans un ordre quelconque : la fin de l'ancien serveur remettait l'état à
STOPPED après le démarrage du nouveau. Maintenant, la fin d'un processus ne change l'état que s'il
est encore le serveur courant, et le redémarrage détruit l'ancien puis démarre le suivant à sa
sortie.

Testé :
- aller-retour C → Java → C d'un `ActorPlayerSync` avec `proc` 0xFFFFFFFF, une animation de fin
  -1 et une valeur 0xFFFFFFF0 : identique (370 octets) ;
- `javac -Xlint:all` (`ActorPlayerSync`, `ServerProcess`, `LauncherUI`) ;
- mod 0.2.30 compilé sans avertissement, traduit (118 fonctions, 47 hooks), installé dans les
  instances 1 et 3.

Non testé en jeu.

### Mutators appliqués en direct, sans redémarrer le serveur (0.2.31, 16 octobre 2026)

Demande : un changement de mutator dans le launcher s'applique tout de suite, sans redémarrer le
serveur.

Chaîne :

1. **Launcher → serveur lancé.** Le bouton « APPLY MUTATORS » enregistre les valeurs puis appelle
   `ServerProcess.sendMutators(N, r)`. Celui-ci écrit la ligne `mutators N 0|1` sur l'entrée
   standard du processus serveur. Si aucun serveur ne tourne, les valeurs restent enregistrées
   pour le prochain démarrage. « SAVE & RESTART SERVER » reste disponible.
2. **Serveur.**
   - `ActorFilter` lance un thread daemon `server-console` qui lit `System.in`.
   - Une ligne `mutators N 0|1` est passée au thread de l'acteur par `mail(Runnable)` (API MMB
     `Actor.mail`, vérifiée), puis `applyMutators` s'exécute :
     - il met à jour les valeurs, que les clients qui se connectent ensuite reçoivent dans leur
       init ;
     - il envoie à tous les clients connectés le nouveau paquet serveur `SERVER_MUTATORS`.
   - Format du paquet : VLQ du multiplicateur, puis un u8 pour le randomizer.
   - Le paquet est déclaré par l'enum `@ClientPacket ActorFilter.PacketTypesList`.
     `Server.collectPackets` parcourt les classes des acteurs enfants (vérifié dans
     `mmb/net/Server.java`), et `ActorFilter` est un enfant du serveur (`serverCreator.ktml`).
3. **Mod.**
   - `mp_parse_init` associe `SERVER_MUTATORS` par son nom (`session.server_mutators`) et
     `mp_read_mutators` le décode.
   - `handle_frame` met la session à jour et appelle `as_mutators_changed()`. `handle_init` aussi :
     une zone chargée avant la connexion prend les valeurs du serveur.
   - Au pas suivant, `as_apply_mutators` fait suivre les nouvelles valeurs à la zone chargée :
     - Les ennemis placés sont mémorisés dès leur chargement, même sans copies ni connexion :
       chaque original dans `as_groups` (nom de stage et bloc de paramètres), chaque copie dans
       `as_enemy_copies` (numéro de copie). Le nom vient de `as_stage_create`, par `as_notes`, une
       table indexée par l'adresse du bloc, celui que lit `fopAc_Create`.
     - Les copies manquantes sont créées comme au chargement, par `as_make_copy`, commun aux deux
       chemins. Le bloc est ensuite passé à `dStage_actorCreate` avec un faux
       `stage_actor_data_class` qui ne contient que le nom. C'est vérifié dans
       `decomp/wwhd_src/d/d_stage.cpp:86` : la fonction ne lit que `name`, écrit `argument` et
       `gbaName`, puis appelle `fpcSCtRq_Request(fpcLy_CurrentLayer(), …)`.
     - Sont supprimées (`fopAcM_delete_actor`) les copies dont le numéro est supérieur ou égal au
       nouveau multiplicateur, ou toutes les copies si le randomizer change. Leur propriétaire
       envoie `CLIENT_ACTOR_SYNC_REMOVE`.
     - Une copie tuée avant n'est pas recréée, sauf si le randomizer change.
   - **Changement de compatibilité :** le hash des variantes (`as_randomize`) ignore désormais
     `gbaName` (0x1B) et `argument` (0x20), écrits par `dStage_actorCreate`. Une copie créée en
     direct et une copie créée au chargement ont ainsi le même UID. Les variantes tirées diffèrent
     donc de celles de la 0.2.30 : tous les clients doivent être en 0.2.31.
4. **Fake server.** La ligne `mutators N 0|1` sur son entrée standard fait la même chose.
   `SERVER_MUTATORS` est ajouté à sa liste de paquets serveur.

État :

- **Implémenté et testé :**
  - `mp_protocol_test` est OK (index du paquet dans l'init, décodage, valeur bornée à 15, paquet
    tronqué refusé).
  - Le test du fake server sur le port 10099 est OK : le nom est présent dans l'init ; la trame
    reçue est `84 82 86 01` (VLQ MMB, dernier octet marqué par 0x80) ; un client connecté ensuite
    reçoit `#enemy_multiplier=6` et `#enemy_randomizer=1`.
  - Mod 0.2.31 compilé et installé sur les instances 1 et 3 (119 fonctions, 47 hooks).
  - `javac` OK pour `ActorFilter`, `ServerProcess` et `LauncherUI`.
- **Non testé à l'exécution :**
  - Le serveur Java réel : le port 10014 était occupé par le serveur lancé depuis le launcher de
    l'utilisateur (PID 3272), qui n'a pas été arrêté.
  - En jeu : création et suppression des copies en direct.
- **Risques déduits, non testés :**
  - Les copies créées en direct passent par `fpcLy_CurrentLayer()` au moment du pas du mod, le
    même contexte que les répliques `fopAcM_create` qui fonctionnent déjà. À confirmer en jeu
    avec un ennemi d'une salle.
  - Le client GameCube reçoit un ID serveur inconnu si les mutators changent pendant qu'il est
    connecté. Sa réaction n'est pas vérifiée.
  - Une copie dont le `fopAc_Create` n'a pas encore eu lieu au moment de l'application n'est pas
    supprimée.

### Skins : un autre modèle de Link par joueur (0.2.32, 17 octobre 2026)

Demande : chaque joueur choisit son modèle (exemple fourni : `ZeldaWWHDRecomp/tools/skin/Miku`,
pack Cemu).

**Jeu de base (vérifié).** Le pack Miku remplace `Common/Pack/permanent_3d.pack`. Comparé au
pack du jeu (copie EUR du joueur), une seule entrée diffère : `Link.szs`, et dans `Link.szs`
seul `Link.bfres` change (les 54 `.sharcfb` sont identiques). Installé comme mod de contenu
(gestionnaire de mods, « Install from a file or folder », accepte les packs Cemu à titleIds
incluant 0005000010143500), il remplace Link pour tous les Link de la machine, puppets compris.
`Demo01.szs` et `LinkRM_Room0.szs` (cinématiques, chambre) ne servent pas aux puppets.

**Vérifié dans le décomp :**
- un modèle HD se charge par le nom de l'archive : `"%s.bfres"` puis un `<modèle>.sharcfb` par
  modèle (`hd_res_model.cpp` `hdResModel_loadFiles`) ;
- `ResourceArchiveInfoHD::loadResourceTable` (02604550) trouve les index d'une archive par le
  hash de son nom dans la table du jeu (`d_resorce.cpp`) : une archive d'un nom inconnu n'a pas
  d'index ;
- tous les `dComIfG_getObjectRes("Link", idx)` de `daPy_lk_c` passent par
  `dRes_control_c::getRes` 026066C4 (et `getIDRes` 026067F4) ;
- un objet se charge depuis `Object/<nom>.szs` par le thread de `ResMgr` (`hd_res_mgr.cpp`) ;
  `dComIfG_setObjectRes/syncObjectRes/deleteObjectRes` (025203xx) prennent un `char*` ;
  sync : 0 chargé, > 0 en cours, < 0 échec ;
- un objet hors « sea »/« gy » va dans `mpObjectParentHeap` (`needsSeparateHeap`) ;
- `base_process_class::mBsPcId` est à +0x04 (`f_pc_base.h`), l'id rendu par `fopAcM_create` ;
- vtable SafeString du contrôle de ressources : 100E1994 (`d_resorce.h`), `assureTermination`
  vide (0260A824).

**Implémenté :**
1. **Format de skin** : `skin_<id>.szs` (id : 1 à 12 de a-z 0-9 _) dans le dossier Data du mod
   (celui de `launcher.cfg`). C'est le `Link.szs` du mod avec `Link.bfres` renommé
   `MpSk_<id>.bfres`, en SARC Yaz0 non compressé (comme `notebook_model.py`). Outils :
   `tools/multiplayer/make_skin.py` (dossier de mod, `permanent_3d.pack`, `Link.szs`, ou
   `Link.bfres` + `--game`) et `SkinImporter.java` (même sortie, octet pour octet).
2. **Runtime** : `runtime/src/mods/mp_skins.{h,cpp}`. `hle/fs.cpp` `read_path` sert
   `.../Object/MpSk_<id>.szs` depuis `skin_<id>.szs` du dossier Data de `multiplayer-connection`
   (`set_directory` au chargement du mod, `guest_mods.cpp`). Rien d'autre n'est redirigé.
3. **Mod (`skin.h`)** :
   - chargement par `setObjectRes`, attente par `syncObjectRes`, déchargement
     (`deleteObjectRes`) quand plus aucun Link ne l'utilise depuis 5 s ; fichier absent (lu par
     `wwhd_file_read`), refus, échec ou plus de 30 s : Link normal, une ligne de log ;
   - `LoadTableResources` remplacé : pour une archive `MpSk_*`, il reçoit le nom « Link », donc
     les index de Link ;
   - `getRes` / `getIDRes` remplacés : pendant le code d'un Link lié à un skin (phases 1 à 3,
     execute, draw, delete), « Link » est remplacé par l'archive du skin ;
   - liaison à la phase 1 : Link local → `skin` de `launcher.cfg` (attend le chargement), puppet →
     skin du joueur (créneau dont `proc` == `mBsPcId`). `puppet_step` attend le skin avant de créer
     le puppet. Un joueur qui change de skin : son puppet est supprimé puis recréé ;
   - un Link avec skin garde ses textures : pas de recoloration de tunique ni d'échange de tenue
     pour lui ;
   - le skin envoyé est celui que porte le Link local (lié à sa création) : un nouveau choix
     s'applique à la zone suivante, chez tout le monde en même temps.
4. **Protocole** : extension WWHD après les particules, chaîne terminée par NUL (`mp_set_skin`,
   id invalide = pas de skin). `ActorPlayerSync.java` la garde comme état (comme le nom) et la
   renvoie. Clé `skin` dans `launcher.cfg` (`launcher_cfg.h`, `HdModBridge`), option `skin` du
   manifeste.
5. **Launcher** : page Prepare Your Voyage, section SKIN : liste (Link + skins installés) et
   IMPORT A SKIN (dossier du mod, nom demandé, import en tâche de fond).

**État :**
- **Implémenté et testé :**
  - `make_skin.py` sur Miku : 55 fichiers, identiques au `Link.szs` du mod sauf le nom du
    bfres ; `SkinImporter` donne le même fichier (cmp) ;
  - `mp_skins_test` (nouveau CTest `mp_skins`), `mod_content_fs_*`, `language_sources` OK ;
  - `mp_protocol_test` OK (bloc skin, troncatures, ids refusés, index du composant suivant,
    clé `skin` de `launcher.cfg`) ; vecteurs dorés mis à jour (+1 octet `00`) ;
  - aller-retour C → Java → C : relais identique, le skin reste au second envoi ;
  - mod 0.2.32 compilé et traduit (125 fonctions, 50 hooks), installé sur les instances 1 et 3 ;
    `skin_miku.szs` copié dans leur dossier Data ;
  - `wwhd.exe` relinké, `--renderer-smoke` PASS ; `javac` OK.
- **Non testé en jeu.** Points à vérifier :
  - le tas parent des objets accepte une archive de ~23 Mo (et plusieurs skins) ;
  - aucun autre code ne cherche « Link » par nom pour le modèle d'un Link (flèches, grappin,
    boomerang : acteurs à part, ils restent ceux de Link) ;
  - le Link local attend son skin en phase 1 à chaque chargement de zone (le skin reste chargé
    entre zones tant qu'il est porté).
- **Limites connues :** un joueur ne voit un skin que s'il a le même fichier sous le même nom
  (pas de transfert par le serveur). Quand le Link local porte un skin, les couleurs de tunique
  des puppets sans skin ne sont plus clonées depuis la bonne texture.
- **Attention dépôt :** le commit d45eef1 contient `tools/skin/Miku` (le `permanent_3d.pack`
  du mod, donc les fichiers du jeu). Contraire à la règle 9 : ne pas pousser en l'état.

### Carte du monde du launcher et téléportation (0.2.33, 17 octobre 2026)

Demande : afficher les joueurs sur la carte de l'onglet 4 du launcher (`images/map.jpg`), avec
`images/playericon.png` dont le vert devient la couleur du joueur ; cliquer un autre joueur
propose de s'y téléporter pour 35 rubis.

**Vérifié :**
- mer : 7 × 7 carrés de 100000 unités, centre du carré D4 en 0, x vers l'est, z vers le sud
  (`sea_chart.h`) ;
- `map.jpg` (2048²) : grille mesurée sur ses traits, départ (142, 166), carrés de 271,7 × 270,7 px
  (le bas de la dernière ligne est hors de l'image) ;
- rubis : `dSv_player_status_a_c::mRupee` à `*(101F84DC) + 0x20 + 4` (`d_save.h`) ; paiement par
  `dComIfG_play_c::mItemRupeeCount` (play + 0x48A8, play à +0x12A0 de `dComIfGp_get()`), que le
  compteur applique avec son décompte puis remet à 0 (`d_meter_02.cpp`), comme les boutiques ;
- `SHIP_RIDE` : play + 0x5CD8 & 0x10000 (`puppet.h`).

**Implémenté :**
- **mod (`mod.c` `map_step`)** : toutes les 0,5 s, `players.txt` dans le dossier Data (`stage=`,
  `rupees=`, `step=`, `me=` puis `player=` par joueur distant connu,
  `uid|x|z|cap|RRGGBB|nom`, couleur = tunique principale ; `teleport=id|résultat`). Il lit
  `teleport.cfg` (`id=`, `target=`) : un nouvel id = une demande, celui trouvé à la première
  lecture est ignoré. Refus : `offline`, `not_sea`, `busy` (événement ou téléport en cours),
  `riding` (sur le bateau), `no_target`, `rupees`. Sinon 35 rubis payés, Link placé 100 unités
  au-dessus de la dernière position reçue de la cible, maintenu 45 pas sans vitesse ;
- **launcher (`LauncherUI` onglet World Map)** : rafraîchi toutes les 0,5 s tant que la page est
  affichée ; icône recolorée (pixels verts du bonnet, yeux exclus, ombrage conservé) par couleur,
  la sienne plus grande avec un halo blanc ; clic sur un autre joueur → confirmation (coût, rubis
  possédés) → `teleport.cfg` ; la réponse du mod s'affiche en message. Fichier de plus de 3 s :
  « le jeu ne tourne pas ».

**Limite :** le client ne connaît que les joueurs de sa zone (gestionnaire du serveur) : la carte
montre les joueurs en mer, le joueur local doit y être aussi.

**État :**
- Testé : mod 0.2.33 compilé et traduit (126 fonctions, 50 hooks), installé instances 1 et 3 ;
  `javac` OK ; recoloration vérifiée sur une image de contrôle (rouge, bleu, marron) ; lecture
  des lignes `player=` vérifiée.
- Non testé en jeu : affichage avec de vrais joueurs, paiement, téléportation. Risque déduit :
  le chargement des salles de mer autour du point d'arrivée (maintien de 45 pas supposé
  suffisant) ; un joueur sur une île peut arriver avant la collision du sol.

### Correctif : mod 0.2.33 refusé au chargement (0.2.34, 17 octobre 2026)

Constaté (log de l'instance 1) : « module ABI does not match this game version: rebuild the mod ».
Cause vérifiée : le module traduit importait `__fixsfdi` (aide du compilateur pour une conversion
float → entier 64 bits, `map_put_int(o, (long long)x)` dans `map_step`). Ce n'est pas un service
hôte (`guest_mods.cpp`) : l'init du module renvoie null et le runtime affiche ce message
générique (le message ne dit rien de l'import manquant).

Corrigé :
- `map_step` n'utilise plus que des entiers 32 bits (`map_put_int` s32, `map_put_uint` u32) ;
- `build_mod.ps1` compare désormais les imports du module traduit (`k_services`) aux services de
  `guest_mods.cpp` et refuse le build sinon (vérifié sur une liste contenant `__fixsfdi`).

Testé : mod 0.2.34 compilé, traduit (126 fonctions, 50 hooks), « every import is a host
service », installé instances 1 et 3. Non relancé en jeu.

### Skins chargés seulement au changement de zone ; liste des joueurs sur la carte (0.2.35, 17 octobre 2026)

**Crashs constatés** (instances 1 et 3, `captures/crash-20261017-*.log`, adresses EU converties
avec `tools/recomp/builds/eu.json`) :
- instance 3 : `skin miku loading` (skin d'un autre joueur, demandé en pleine partie) puis
  crash dans le dessin d'un Link : `daPy_lk_c::draw` 023D9820 → `entryDLSetLight` 023D8DF0 →
  `mDoExt_modelEntryDL` → `mDoExt_drawModel` 025E2BF4 → 027F55FC, lecture de null + 0x3C ;
- instance 1 : `skin miku loading` pour le Link local qui attendait son skin en phase_1, puis
  un acteur lit null + 0x314 (`current.pos`) : un Link qui attend en phase_1 n'est pas encore
  enregistré comme joueur 0.
Cause exacte du premier crash non identifiée. Les deux ont en commun le chargement d'une
archive de ~23 Mo en pleine partie : c'est ce point qui est supprimé.

**Corrigé (`skin.h`, `puppet.h`, `mod.c`) :**
- chargement et déchargement des skins **uniquement** pendant le chargement de la scène :
  remplacement de `dScnPly phase_5` (025B2F90, `d_s_play_2.cpp`), après les ressources
  préchargées du jeu et avant la création du stage et de ses acteurs ; la scène attend nos
  archives (échec ou 30 s : Link) ;
- skins voulus : celui du joueur local (`launcher.cfg` relu au chargement de scène) et ceux des
  joueurs rencontrés dans les 10 dernières minutes (`skin_note_remote`) ; les autres sont
  déchargés à ce moment-là ;
- le Link local n'attend plus en phase_1 ; un puppet n'attend plus son skin. Un skin pas encore
  chargé montre Link jusqu'à la zone suivante ; un joueur qui change de skin n'est recréé que si
  le nouveau est déjà chargé (ou revient à Link).

**Launcher, World Map :** liste PLAYERS à gauche (icône, nom en couleur primaire, case de la carte
« F2 », « you » pour soi, clic = téléportation) ; nom sous chaque icône en couleur primaire avec
un contour sombre.

**État :**
- Testé : mod 0.2.35 compilé, traduit (129 fonctions, 51 hooks), imports vérifiés, installé
  instances 1 et 3 ; `javac` OK ; rendu hors écran de la page (JavaFX snapshot, `players.txt`
  factice) : positions F2 / C5 / D7 correctes, liste et noms lisibles.
- Non testé en jeu : chargement en `phase_5` (mémoire, durée), disparition du crash.

### Rubis sur la carte du launcher ; joueurs sur la carte du GamePad pendant le jeu (0.2.36, 17 octobre 2026)

**Launcher (World Map)** : panneau « YOUR RUPEES » (`images/rupee.png` + nombre lu dans
`players.txt`) au-dessus de la liste des joueurs ; la confirmation de téléportation montre
l'icône recolorée et le pseudo (couleur primaire) du joueur ciblé, le prix avec l'icône rubis et
les rubis possédés. `javac` OK, rendu de la page vérifié hors écran (snapshot JavaFX) ; la boîte
de dialogue n'a pas été rendue hors écran (modale).

**Carte du GamePad pendant le jeu** : c'est le même écran que la carte du menu Start
(`hdSeaChartMenu_c`, `story_chart.cpp`). `chart_on_screen(v, in_menu)` : la liste du GamePad
(`draw_gamepad`) dessine dès que la carte s'est mise à jour ce pas et que ses panes sont
visibles, plus seulement menu ouvert ; positions lissées (`display`) hors menu, dernières reçues
dans le menu (jeu en pause). Déduit, non vérifié en jeu : la carte ne se met pas à jour (ou ses
panes sont cachés) quand le GamePad montre un autre écran (objets).

**Carte de donjon et carte du Chant des tornades : non faites, recherche en cours.** Vérifié dans
`cking.rpx` (EU, références lis/addi converties en adresses USA avec `builds/eu.json`) :
- layouts : `DungeonMap_00`, `DungeonMapLink_00`, `N_LinkIcon_00`, `G_LinkIcon_00`, état
  `LoopLinkIcon` (donjon) ; `WarpMap_00`, `WarpArea_00`, `L_WarpArea_00..08`, `L_LinkPos_01`
  (Chant des tornades) ;
- code qui les nomme : `DungeonMap_00` vers US 0265E648, `G_LinkIcon_00` vers 0265E3F4,
  `DungeonMapLink_00` vers 02665438 ; `WarpArea_00` vers 026EEFE4, `L_WarpArea_00` vers
  026F05FC, `WarpMap_00` vers 026F068C et 026F0758 (fonctions non nommées dans `functions.h`) ;
- `hdPadDungeonUpdatePlayer` (02657914) ne place pas l'icône de Link (désassemblé : un appel de
  focus d'entrée), donc le calcul monde → carte du donjon reste à trouver.
Reste à faire : trouver pour chaque écran l'objet, sa fonction de mise à jour (hook) et la
transformation monde → pane (donjon : échelle par étage ; carte des tornades : grille de la mer,
comme la carte marine).

### Annuaire des joueurs, téléportation vers une autre zone, carte de donjon et carte des tornades (0.2.37–0.2.38, 17 octobre 2026)

**Annuaire (extension WWHD du protocole, `mp_protocol.h` « player directory »)** :
- `CLIENT_PLAYER_DIRECTORY` (chaque seconde, si le serveur le connaît) : nom, couleur (RGB565),
  stage, point / salle / calque d'entrée dans ce stage (`dStage_startStage_c` à play + 0x5134 :
  point s16 +8, salle s8 +0xA, calque s8 +0xB), salle courante, x y z, cap, drapeaux, dernière
  place en mer si connue ;
- `SERVER_PLAYER_DIRECTORY` (réponse à ce client) : id du destinataire, puis id + entrée de
  chaque joueur (24 au plus). Serveur : `WWClient.java` (entrée validée octet par octet, gardée,
  liste renvoyée), enum dans `ActorFilter.PacketTypesList`. Un client sans l'extension n'envoie
  rien ; un serveur sans elle ne la liste pas dans son init : le mod reprend l'ancien comportement.

**Bug serveur corrigé (vérifié)** : les méthodes `@ServerPacket` de `WWClient` s'exécutent sur
chaque client connecté (`PacketActorMail.test` compare le parent de l'acteur, ici le serveur).
`CLIENT_INIT_PACKET` était donc traité par tous les clients : chacun prenait l'identifiant du
dernier venu et réécrivait « joined » (compteur du launcher faussé). Filtre `pack.getSource() !=
this` ajouté aux deux méthodes.

**Mod** :
- `players.txt` : `directory=`, lignes `id|x|z|cap|RRGGBB|nom|stage|mer x|mer z|mer connue`, tous
  les joueurs du serveur ;
- téléportation : même stage → saut ; autre stage → 35 rubis, `dComIfGp_setNextStage` (0252012C)
  avec le stage, le point, la salle et le calque d'entrée de la cible, puis saut sur sa dernière
  position 20 pas après l'arrivée (réponses `loading`, puis `ok` ou `arrived`) ;
- cartes du jeu : joueurs de tout le serveur (estompés hors de la mer, à leur dernière place en
  mer) ; nouveau hook retour `hdSeaChartGrid_c::refreshChartMarks` (026834A4) : toute grille de
  carte marine affichée sert de carte (zone à +0xC8), ce qui doit couvrir la carte du Chant des
  tornades (`WarpMap_00` contient `Map_00`, `MapAreaNow_00`, `BtnMapBalloon_00`) — déduit, la
  première grille de chaque écran est journalisée ; les hooks invités tournent autour du
  remplacement de cette fonction par `story_chart.cpp` (même chaîne, `guest_mods.cpp`) ;
- carte de donjon (`dungeon_map.h`, 0.2.38, expérimental) : hook retour `hdDungeonUpdateMarkers`
  (0265799C) ; pane d'icône de Link cherché par nom (`N_LinkIcon_00`…) sous la racine du layout
  (objet + 0x44 → + 4 → + 0xC, comme la carte marine) ; échelle mesurée quand Link marche
  (déplacement de l'icône / déplacement monde, axes cohérents à 30 %) ; joueurs du même stage à
  moins de 600 unités de hauteur dessinés autour de l'icône. Journalisé : écran, pane trouvé ou
  arbre des panes, échelle, « l'icône ne bouge pas ».

**Launcher** : tous les joueurs (liste : « Great Sea · C5 » ou « in M_Dra09 ») ; carte : place en
mer ou dernière place en mer (estompée) ; téléportation vers n'importe qui avec l'annuaire.

**État :**
- Testé : `mp_protocol_test` (annuaire : octets exacts, aller-retour, troncatures, drapeau
  inconnu, plus de 24 joueurs, noms coupés au NUL) ; vrai serveur Java (copie sur le port 10098,
  le 10014 étant celui de l'utilisateur) avec deux clients Python : annuaire correct, un seul
  « joined » par client ; `javac` OK ; rendu hors écran du launcher avec un annuaire factice ;
  mod 0.2.38 compilé, traduit (137 fonctions, 53 hooks), imports vérifiés, installé instances 1
  et 3.
- Non testé en jeu : envoi réel de l'annuaire, téléportation inter-zones (risque : cible dans une
  salle non chargée), carte du Chant des tornades, carte de donjon (calibrage).

### Correctifs des cartes : positions figées, icônes hors menu, carte de donjon exacte (0.2.39, 17 octobre 2026)

Retour du test de 0.2.38 : sur la carte marine les autres joueurs ne bougeaient plus ; avec la
manette Pro, les joueurs apparaissaient sur la TV pendant le jeu (carte de donjon et carte marine)
menu fermé ; sur la carte de donjon la position était fausse.

- Cause 1 (vérifiée dans le code) : depuis 0.2.37 la carte marine ne lisait que l'annuaire, envoyé
  une fois par seconde et seulement pendant le jeu (pas pendant la pause du menu). Corrigé : les
  joueurs de sa propre zone sont repris des joueurs distants en direct (position lissée en jeu,
  dernière position reçue en pause, comme avant 0.2.37) ; l'annuaire ne sert plus qu'aux autres
  zones (une entrée de sa zone qui porte le nom d'un joueur distant affiché n'est pas doublée).
- Cause 2 (vérifiée dans le code) : le chemin TV (`draw_markers`) dessinait la carte de donjon et
  toute grille de carte marine sans exiger le menu Start ouvert ; les écrans GamePad continuent
  d'être mis à jour pendant le jeu. Corrigé : TV = carte de donjon seulement menu ouvert ; grille de
  l'écran MAP (sa zone de carte est le panneau principal `+0x70` du menu) seulement menu ouvert ;
  une autre grille (carte des tornades, déduit) quand elle se met à jour. Chaque nouvelle grille
  journalise si c'est celle de l'écran MAP, si le menu est ouvert et le nom de son panneau.
- Cause 3 (vérifiée dans le binaire EU, adresses converties en USA par `eu.json`) : le panneau
  `N_LinkIcon_00` ne bouge pas ; l'icône de Link est un objet à part (écran `+0xBC`). Chaîne du jeu :
  02655870 place sur la carte = (x, z) monde − origine (écran `+0x2FC`, `+0x300`) ; 026568E8 la copie
  dans l'icône `+0x58/+0x5C` ; 0268531C (slot 0x114) la transforme par les slots 0x124…0x13C
  (02685430 ×s avec y inversé, s = `*(f32*)(carte+0x4B4)` ; 02685478 rotation de l'angle carte
  `+0x4AC` en degrés ; 026854D8 × `cellule+0x2C` ; 02685534 + place de la cellule) en `+0x68/+0x6C`.
  Étage affiché : écran `+0x3D0`, comparé par le jeu à `map_GetFloorNo(y)` (0258DC48, binding SDK).
  Implémenté : un autre joueur = place de Link (`+0x68`) + partie linéaire × (sa place − celle de
  Link), puis la chaîne des panneaux parents ; seulement s'il est sur l'étage affiché. Calculé dans
  le hook 0265799C (appels jeu), dessiné ensuite. Le calibrage par la marche est supprimé.
- Déduit, non vérifié : que l'icône de Link du donjon utilise bien ces classes (deux autres classes
  remplacent l'échelle) et que la translation du panneau vaut `+0x68`. Journal par écran : échelles,
  angle, places, translation du panneau, étage ; puis trois « dungeon map check » comparant le
  déplacement réel de l'icône de Link à la prédiction.
- Tests : mod 0.2.39 compilé sans avertissement, traduit (136 fonctions, 53 hooks), imports vérifiés,
  installé instances 1 et 3. Non testé en jeu.

### Carte de donjon : l'icône de Link est une racine de layout (0.2.40, 17 octobre 2026)

- Constaté dans le journal de 0.2.39 : aucune ligne « dungeon map screen », donc la lecture
  échouait à chaque mise à jour et rien n'était dessiné. Cause déduite du binaire (02685D30, EU
  02685D30 → USA 0268562C par `eu.json`) : quand l'icône ne nomme aucun panneau, elle prend la
  racine (`lyt+0xC`) de son propre layout. Cette racine n'a pas de parent, et la lecture exigeait
  un parent.
- Implémenté : sans parent, la translation de la racine est prise directement sur l'écran (origine
  au milieu, y vers le haut). Hypothèse non vérifiée. Si la racine est masquée (Link sur un autre
  étage que celui affiché), rien n'est dessiné.
- Diagnostic ajouté : un échec de lecture est journalisé une fois par écran et par raison
  (« dungeon map not read » ; 1 icône, 2 panneau, 3 carte, 4 cellule, 5 échelle, 6 valeurs,
  7 masquée, 8 parents, 9 places). L'écran lu journalise aussi « icon is a layout root ».
- Tests : compilé, traduit (136 fonctions, 53 hooks), imports vérifiés, installé sur les
  instances 1 et 3. Non testé en jeu.
- Sans rapport avec le mod : le journal contient « [runtime error] Expected three-part version »
  (`runtime/src/mods/catalogue_schema.h`, version de catalogue mal formée). Non étudié.

### Pseudos sous le menu, tête du Lion Rouge, particules suivies, armes des autres joueurs, boomerang (0.2.41, 17 octobre 2026)

Retour du test de 0.2.40 : la carte de donjon fonctionne. Demandes : cacher les pseudos quand le
menu Start est ouvert ; la tête du Lion Rouge plus sombre que la coque pour certaines couleurs ; les
particules partagées ne marchent pas (seulement en tenant B, au mauvais endroit, au mauvais moment) ;
les bombes des autres joueurs (canon compris) ne blessent ni joueurs ni ennemis ; le boomerang mal
lancé, sans effet sur l'environnement.

**Pseudos** : `draw_markers` ne dessine plus les noms quand `chart_menu_open()` (menus items,
collection, carte, sacoche, TV ou GamePad). Non testé en jeu.

**Tête du Lion Rouge** (vérifié hors ligne sur `Ship.szs` de `szs_permanent0.pack`, rien copié
dans le dépôt) : le modèle de tête liste `toonEX` (1024×1024 BC1), une rampe d'éclairage toon dont
la peinture est rouge pur (saturation et luminosité médianes 1,00). Recolorée, elle multipliait la
texture de la tête (`fn_head1`, déjà recolorée) par la couleur choisie une seconde fois, d'où une
tête plus sombre que la coque (dont la rampe est neutre), surtout pour les couleurs peu saturées ou
sombres. Corrigé : une texture dont plus de la moitié des extrémités « peinture » sont à saturation
et luminosité ≥ 0,95 est une rampe ; sous une peinture personnalisée elle devient grise à la même
luminosité (`tc_palette_make_boat_neutral`, `tc_ramp_endpoint`), pour le bateau local et les
copies des autres bateaux. Vérifié hors ligne : parmi les textures de `Ship.bfres`, seules `toonEX`
et `new_ho2` (hors des modèles corps/tête) répondent à la règle. `mp_protocol_test` étendu. Non
testé en jeu.

**Particules suivies** (vérifié dans le décomp) : la charge de l'épée (`CHARGEPOWER00/01`,
`daPy_mtxFollowEcallBack_c::makeEmitter` 023D457C) et l'attaque tournoyante (`ROUNDATTACK00..02`,
callback de Link, couleurs prm/env, salle) sont des émetteurs à callback : le callback les recale à
chaque image (023D4528 copie une matrice de Link dans `mGlobalRotation`/`mGlobalTranslation`),
`end()` (023D4538) les arrête (`mMaxFrame` −1, `mFlags | 1`, `& ~0x40`, callback nul). 0.2.40 les
envoyait une fois, à leur place de création, et les rejouait en salve de 30 images.
- Protocole (extension WWHD, bloc des particules) : drapeaux 8 suivi, 16 arrêt, 32 couleurs ;
  après l'alpha `u8` clé, puis prm et env (`u8` ×4 chacun). Un arrêt sans clé est refusé.
  `MP_MAX_PARTICLES` passe à 16. Java (`ActorPlayerSync`) lit les nouveaux champs.
- Émetteur : un émetteur à callback du Link local reçoit une clé ; un pas après sa création son
  enregistrement part (place, couleurs, échelle), puis sa place à chaque pas où elle change (au
  moins tous les 10 pas), puis un arrêt quand son callback a changé, que `mFlags` a 0x01 ou 0x08
  (`doTerminationProcess`, `JPA_emitter_calc.cpp`) ou que `mMaxFrame` < 0.
- Receveur : l'émetteur est créé (couleurs comprises), marqué `Immortal` (0x40), replacé à chaque
  enregistrement, arrêté comme `end()` à l'arrêt ou après 45 pas sans nouvelles. Au changement de
  zone, seules les copies encore `Immortal` et sans callback sont arrêtées.
- Le callback de l'attaque tournoyante n'est pas rejoué : ses particules suivent l'émetteur, pas la
  traînée de l'épée. Les effets créés hors de l'execute de Link (pendant son draw) ne sont toujours
  pas capturés.
- Tests : `mp_protocol_test` (octets écrits à la main, troncatures, arrêt sans clé refusé) ;
  aller-retour C → Java → C d'un `ActorPlayerSync` avec particules suivies : identique
  (451 octets). Non testé en jeu.

**Armes des autres joueurs** (bombes, canon, flèches, boomerang) : depuis 0.2.23 une copie d'arme
ne touchait rien (`SetAtTgCommonHitInf` remplacé), seuls les strikes du créateur sur les acteurs
synchronisés passaient. Maintenant une copie touche le Link local et les acteurs non synchronisés
(pots, herbe, interrupteurs, ennemis hors de la liste du serveur) ; pas un acteur synchronisé (le
strike du créateur, une seule fois) ni un puppet de Link. Journaux ajoutés : coups d'une copie
(Link local / acteur), coups laissés au strike, strikes d'armes envoyés, rejoués avec ou sans la
copie, refusés par le collider de la cible. Cause de « pas de dégâts aux ennemis » non établie
(aucun journal de test) : ces lignes la diront.

**Boomerang** (vérifié dans `d_a_boomerang.cpp`) : `procWait` (020CF4CC) lance le vol quand
`mParameters` vaut 1 (posé par Link au lancer) et, sinon, recolle le modèle dans la main du joueur 0
à chaque image (`setKeepMatrix`) ; `procMove` le remet à 0 au rattrapage. Seules les flèches
recevaient les paramètres du propriétaire : la copie restait dans la main du puppet, sans collider
d'attaque. Maintenant le propriétaire envoie les paramètres du boomerang à chaque changement et la
copie les reçoit avant son execute (`as_boomerang_params`) ; sa place suit celle du propriétaire et
son collider touche l'environnement (voir ci-dessus). Non testé en jeu ; risque : au retour, la copie
appelle `returnBoomerang` sur le puppet, ou se supprime si le puppet ne peut pas le rattraper.

Tests : mod 0.2.41 compilé sans avertissement, traduit (137 fonctions, 53 hooks), imports vérifiés,
installé instances 1 et 3 ; `mp_protocol_test` OK. Le serveur Java et le launcher doivent être
recompilés (lecture des particules).

### Boomerang rattrapé, hookshot et grappin des autres joueurs (visuel seulement) (0.2.42, 17 octobre 2026)

Retour du test de 0.2.41 : le boomerang des autres part et touche, mais revenu au puppet il continue
de tourner dans sa main. Demande : les deux grappins (grappin et hookshot), visuellement seulement.

**Boomerang** (vérifié dans le décomp) :
- `daPy_lk_c::execute` calcule après `jointCB1` `mBoomerangCatchPos` (joint 0 × `l_execute_boomerang_catch`
  10035B18, `d_a_player_main_04.cpp`) et `mHookshotRootPos` (joint 8 × `l_execute_hookshot_root`
  10035B24). L'execute du puppet ne le faisait pas : la copie volait vers un point périmé. Le puppet les
  calcule maintenant (`puppet_item_points`, écrits par décalage +0x72FC / +0x72F0, SDK `link.h`).
- Au rattrapage du propriétaire (ses paramètres repassent à 0), la copie est remise dans la main comme
  `procMove` le fait (020CEE10) : `mIsReturning`, `mThirdPerson`, `mProcAdjust` à 0, `mProcSelector`
  −1, `mProcTarget` = sa valeur de `procWait` relevée sur la copie elle-même après `create()` (adresse
  propre à la région, jamais écrite en dur), `speedF` 0. `mCatchAndDelete` est effacé à chaque pas
  (la copie ne se supprime plus en arrivant à la main avant le rattrapage du propriétaire).
  Décalages `daBoomerang_c` : `include/d/actor/d_a_boomerang.h`.

**Hookshot** (`daHookshot_c`, process 0xA9, `d_a_hookshot.cpp`) et **grappin** (`himo2_class`,
process 0x1BE `HIMO2`, `d_a_himo2.cpp`, `d_a_himo2_move.cpp`) : acteurs tenus créés par
`makeItemType` (023DF600, `LK_fastCreate`). Ils deviennent des armes partagées (classe `WEAPON`,
noms « Hookshot », « Himo2 ») ; le propriétaire envoie ses paramètres et, pour le grappin, son état
`m02DC` (+0x3F8, dans `mp_spawn.variant`) à chaque changement. Les copies :
- ne touchent rien (`as_hit`) et se dessinent avec le puppet comme joueur (`fopAc_Draw` 025D4654
  remplacé : boomerang, hookshot, grappin ; le hookshot pose sa chaîne depuis `mHookshotRootPos` du
  « link player », le grappin sa corde et son crochet dans les mains du joueur 0) ;
- hookshot : en mode attente (0) il exécute `procWait` (dans la main du puppet) ; dans les autres
  modes il est placé à la pointe du propriétaire et son execute tourne sans proc (l'index du ptmf
  `mCurrProcFunc` +0xD7AA mis à 0 le temps de l'appel : execute n'appelle le proc que s'il est non
  nul), ce qui ne calcule que la chaîne et la matrice de la pointe ;
- grappin : son execute ne tourne jamais (il lit la caméra et la manette locales, son accrochage
  lance un événement). `as_rope_visual` remplit ce que le draw (0216CA5C) lit : mains `m02EC`
  (+0x408, matrices des joints 8 et 12 × (4, 0, 0) comme l'execute), `m02B4` ; en attente (états 0,
  1) état 0, `m02CC` 97 (crochet dans la main gauche, rouleau dans la droite) ; sorti (≥ 2) état 2
  (vol libre), segments `m0310[m02CC+1 .. 98]` en ligne droite de la main au crochet du propriétaire,
  crochet orienté (`m1E`, `m1C` par `cM_atan2s` 020195B0), `m2188` la longueur ; `m24D8`, `m24D9`,
  `m029E` à 0. Décalages : `include/d/actor/d_a_himo2.h`.
- Le puppet montre le pistolet du hookshot (`setHookshotModel` 023DE654, qui prend son propre tas
  d'objet ; pas `makeItemType`, qui créerait un hookshot du joueur 0).
- Limites : corde droite (pas de balancement ni d'enroulement autour d'un poteau) ; une copie n'est
  créée que si le puppet de son propriétaire existe déjà (un hookshot sorti avant l'arrivée de
  l'autre joueur n'apparaît qu'au prochain équipement). Racine carrée par Newton (`as_sqrt`, le 750
  n'a pas de `fsqrt` et le module n'importe aucune libm).

Tests : mod 0.2.42 compilé sans avertissement (`-Werror`), traduit (139 fonctions, 54 hooks), imports
vérifiés, installé instances 1 et 3. Protocole et serveur inchangés. Non testé en jeu.

### Shop du launcher : invoquer des créatures pour soi ou pour un autre joueur (0.2.43, 17 octobre 2026)

Demande : un 5e onglet « Shop » qui montre les rubis et les butins, et vend des créatures envoyées
à un joueur choisi (ou à soi) : Chuchu rouge = 1 gelée rouge + 5 rubis, Bokoblin 10, Moblin 15,
Darknut 20 rubis ; icônes des butins dans `images` (bokoblin = Pendentif de joie, moblin = Collier
de crânes, darknut = Emblème de chevalier, baba = Graine de Boko Baba, kargarok = Plume dorée, les
trois gelées).

Choix (à confirmer par l'utilisateur) : chaque créature coûte 1 butin de son espèce en plus des
rubis ; prix des créatures non citées : Chuchu vert 5, bleu 10, Boko Baba 10, Kargarok 15. Les deux
tables (`mod.c` `shop_creatures`, `LauncherUI` `SHOP_ITEMS`) doivent rester identiques.

Vérifié :
- butins : `dSv_player_c` à `*(0x101F84DC) + 0x20` (`d_save.h`), `mBagItemRecord.mBeastNum[i]`
  (+0x9C + i), index fixes (`d_a_npc_bs1.cpp`, `d_a_npc_bm1_b.h`, `d_a_npc_bms1_talk.cpp`) :
  0 Collier, 1 Graine, 2 Plume, 3 Emblème, 4/5/6 gelées rouge/verte/bleue, 7 Pendentif. Paiement
  comme les boutiques du jeu : compteurs différés `play + 0x5B48` (rubis, s32) et
  `play + 0x5B70 + 2i` (`mItemBeastNumCounts`, s16) ;
- noms de la table des acteurs (lue dans le binaire EU) : `Bk` 0xBD, `mo2` 0xBC, `Tn` 0xBF,
  `c_red`/`c_green`/`c_blue` 0xCE, `bbaba` 0xD6 (Boko Baba, `d_a_bo.cpp`), `Bb` 0xB5 (Kargarok,
  `d_a_bb.cpp`) ; tous dans `actorList.ktml` (synchronisés) ;
- paramètres : relevés dans les entrées `ACTR` des stages du jeu (analyse locale, rien copié) :
  Bk `FFFFFF00`, mo2 `FFFFFF00`, Tn `FFFFFF00`, c_red `FFFF01FF`, c_green `FFFF00FF`, c_blue
  `FFFF02FF` (couleur = octet 1, `d_a_cc.cpp`), bbaba `FFFFFFFF`, Bb `FFFF0F04` ; angles comme dans
  les stages (x 0x1F, z 0xFF : pas d'interrupteur de mort) ;
- `fopAcM_CreateAppend` (025D5600) remplit les valeurs par défaut du bloc ; `dStage_actorCreate`
  (025C1154, l'original) écrit argument et gbaName puis demande le process ;
- le serveur diffuse la création d'un acteur placé à tous les clients de la zone
  (`SyncedActorManager.broadcastActor`), mais pas aux joueurs qui arrivent ensuite (`addClient`
  ne renvoie que les acteurs sans game UID).

Implémenté :
- **Launcher** : onglet 05 « Shop » : rubis et 8 butins avec quantités, choix du destinataire
  (soi ou un joueur de l'annuaire), 8 cartes (icône, prix butin + rubis, bouton BUY grisé si l'on
  n'a pas de quoi payer), réponse du jeu en notification. Échange par fichiers comme la
  téléportation : `shop.cfg` (`id`, `item`, `target`, 0 = soi) ; `players.txt` gagne
  `spoils=` (8 quantités) et `shop=id|résultat` (ok, sent, price, busy, offline, server,
  no_target, failed, unknown).
- **Mod** (`mod.c` « the launcher's shop ») : vérifie et paie, puis pour soi crée la créature à
  250 unités devant Link, face à lui (`actor_sync.h as_summon`), pour un autre joueur envoie
  `CLIENT_PLAYER_SUMMON`. À la réception de `SERVER_PLAYER_SUMMON`, la créature apparaît devant le
  Link local (hors événement) et « X sent you a Moblin! » s'affiche 4 s en haut de l'écran.
- **Synchronisation** : le bloc est marqué `AS_TAG` avec le numéro de copie `AS_SUMMON` (0xFE) ;
  l'acteur est synchronisé comme les ennemis du stage, son game UID étant le bloc. Un jeu qui
  reçoit la création d'un bloc invoqué qu'il n'a pas le recrée depuis ce bloc
  (`as_summon_remote`, une fois par 150 pas), puis l'associe comme un acteur placé.
- **Protocole** (extension WWHD) : `CLIENT_PLAYER_SUMMON` (VLQ id de la cible dans l'annuaire,
  u8 créature, chaîne nom de l'acheteur) ; `SERVER_PLAYER_SUMMON` (u8 créature, chaîne nom),
  envoyé à la seule cible (`WWClient.receiveSummon`, créature < 32, nom ≤ 32, rien après).

Tests :
- `mp_protocol_test` (octets, troncatures, octet en trop, créature inconnue, session sans les
  paquets) ;
- vrai serveur Java (copie, port 10098) avec deux clients Python : la cible reçoit
  `(4, "Tetra")`, l'acheteur rien, une demande malformée est refusée et journalisée ;
- rendu hors écran de l'onglet avec un `players.txt` factice (boutons actifs seulement quand le
  prix est payable) ;
- mod 0.2.43 compilé (`-Werror`), traduit (143 fonctions, 54 hooks), imports vérifiés, installé
  instances 1 et 3 ; `javac` du launcher et du serveur.

Non testé en jeu. Risques : chargement de l'archive de l'ennemi en cours de partie (les acteurs
attendent leur ressource à la création) ; place devant Link dans un mur ou au-dessus du vide ;
Kargarok créé avec des paramètres de la mer ; un joueur qui entre dans la zone après l'invocation
ne voit pas la créature. Le serveur Java et le launcher doivent être recompilés.

### Randomizer élargi, équipement des Darknuts, créatures du shop variées, Kargarok à 5 (0.2.44, 17 octobre 2026)

Demande : un randomizer qui touche plus d'ennemis (Moblin bleu ou noir, Peahat, Wizzrobe…) ; le
casque et la cape des Darknuts selon leurs paramètres ; les créatures du shop avec les variantes du
randomizer, sauf Chuchus, Boko Baba et Kargarok ; Kargarok à 5 rubis.

Vérifié dans le décomp et les stages du jeu (relevé local des entrées `ACTR`) :
- **Darknut** (`d_a_tn.cpp` create) : l'équipement est `angle.x` bits 5-7 du bloc, pas les
  paramètres : 0 rien, 1 casque, 2 bouclier, 3 casque + bouclier, 4 bouclier + cape, 5 les trois
  (borné à 5) ; les stages emploient 0x1F, 0x5F, 0x7F…
- **Moblin** (`d_a_mo2.cpp`) : mode = octet 0. Le mode 1 est le Moblin à lanterne de la Forteresse :
  son propre modèle (l'original GC, plus sombre ; les autres modes prennent le modèle 0x7F qui
  remplace la table de matériaux verte GC) et sa lanterne (`kantera_get_init`, acteur local).
  Mode 0xF : les statues de Hyrule (gardées). Risque connu : en « mode prison » (`rouya_mode`,
  aucun drapeau `isCollect(0, 0..3)`, donc sans épée), un Moblin à lanterne qui attrape Link
  l'envoie dans la prison de la Forteresse (`setNextStage("majroom")`).
- **Wizzrobe** (`d_a_wz.cpp`) : type = octet 0. La couleur est la frame du `brk`
  (type + mini-boss, 3 quand le type est 3, comportement 0). Les types 1 et 2 invoquent depuis un
  chemin de la salle, 12 et 13 sont des scènes de porte : gardés.
- **Peahat** (`d_a_ph.cpp`) : octet 0 = 0xFF ou 0 Peahat, sinon Seahat ; octets 1 et 2 = distances
  (×100, 0xFF par défaut). Pas de variante visuelle sur terre.

Implémenté (`actor_sync.h as_variants`, `as_randomize`) :
- un champ peut être dans les paramètres ou dans `angle.x` du bloc ; une entrée peut exiger une
  valeur des paramètres (`require_mask`/`require_value`) ;
- nouvelles entrées : Darknut équipement 0-5 ; Moblin mode {0, 1, 0xFF} ; Wizzrobe type
  {0, 3, 0xFF} ; Peahat terrestre (octet 0 = 0xFF) distances {0xFF, 5, 15, 30} ×100.
  Les tirages des champs déjà présents ne changent pas (même hash, même index de champ) ;
- toujours appliqué aux seules copies du multiplicateur (les ennemis d'origine gardent leurs
  paramètres, comme avant) ;
- shop : Bokoblin, Moblin et Darknut reçoivent les variantes (`as_summon`, tirage sur le bloc
  lui-même, graine 0x5E ; les autres jeux reçoivent le bloc résultant comme game UID). Chuchus,
  Boko Baba et Kargarok non. Kargarok : 1 Plume dorée + 5 rubis (mod et launcher).

Tests :
- `rand_test` natif (fonction extraite du header) : équipements 0..5 répartis, autres bits de
  `angle.x` et chemins/interrupteurs gardés, Seahat, Moblins statues, Wizzrobe invocateur et
  ennemi non listé inchangés, tirage en place identique au tirage sur une copie ;
- mod 0.2.44 compilé (`-Werror`), traduit (144 fonctions, 54 hooks), imports vérifiés, installé
  instances 1 et 3 ; `javac` du launcher.

Non testé en jeu.

### Progression partagée (SaveServer), notifications d'objets, heure et météo du serveur (0.2.45, 17 octobre 2026)

Demande : synchroniser la progression entre joueurs par la sauvegarde ; une petite notification en
bas à gauche, avec l'icône de l'objet, quand un autre joueur débloque un objet ou ouvre un coffre ;
vie, rubis, magie, flèches, bombes… restent propres à chaque client ; dans le launcher, régler
l'heure et la météo ; au passage jour → nuit décidé par le serveur, garder les acteurs des deux
périodes (un OR).

Vérifié dans les sources :
- protocole historique `save/SaveServer.java` : l'init « SaveServer » porte la carte des champs
  (VLQ nombre, puis {u8 type, u8 longueur en octets}, types 0 octet … 7 bits booléens) ;
  `CLIENT_SAVE_UPDATE` = VLQ nombre + (VLQ offset, valeur brute) ; `SERVER_SAVE_UPDATE` = VLQ
  nombre + (VLQ offset, VLQ longueur, octets). Le SaveServer était absent de `serverCreator.ktml`.
- `dSv_save_c` = 0x778 octets à `*(0x101F84DC) + 0x20` ; mémoire du stage courant à
  `dSv_info_c + 0x778`, `putSave` 025B9D24 / `getSave` 025B9C9C ; la carte Java `WWSave` totalisait
  1886 octets (`fMapBits` boolean[188] au lieu de 0x31 octets) : corrigé à 1912 = 0x778.
- `itemForSlot` 0262F880 (n'utilise pas `this`) : objet → frame d'icône (0x37 = aucune) ; les icônes
  sont `BtnItemIcon_00.szs` / `timg/Icon128_nn^l.bflim` du pack `permanent_2d` (55 frames, RGBA8
  tile mode 4, 128×128). Lues à l'exécution dans les fichiers du joueur, rien n'est copié.
- heure : `mTime` f32 degrés à `dSv_save_c + 0x24`, `mDate` u16 à + 0x28, 0.02°/frame, jour 90°-270°,
  `frameTimePass` 101EBFD8 (0 = l'heure ne passe pas dans ce stage), `setDaytime` relit la
  sauvegarde chaque frame ; `dKy_daynight_check` 02556D14 décide à la création de nombreux PNJ.
- météo : seulement en mer, par les dés de `g_env_light` 10475A68 (`mDiceWeatherMode` +0x1093,
  `mDiceWeatherState` +0x1094, `mDiceWeatherTime` +0xFC4).

Implémenté :
- serveur Java : `@User` sur les champs propres au joueur (statut, lieu de retour, compteurs,
  sac…), `@Bits` sur les tableaux de drapeaux ; nouveau paquet `CLIENT_SAVE_MERGE` (même corps que
  `CLIENT_SAVE_UPDATE`) : union OR des bits, case d'inventaire vide (0xFF) remplie sinon la plus
  grande valeur, chaîne gardée sauf vide, nombre le plus grand ; le client reçoit ce qui diffère de
  son envoi, les autres ce qui a changé ; nouveau `SERVER_SAVE_FROM` (pseudo de l'auteur) avant
  chaque mise à jour diffusée ; correctif des mises à jour de tableaux booléens (le serveur tuait
  le client). Serveur neuf : `User/SaveServer/users` créés, la première fusion fournit la
  progression commune (`AbstractSaveServer` arrêtait le serveur sans sauvegarde fournie).
- serveur Java, monde : `SERVER_WORLD` (u8 drapeaux 1 heure, 2 météo, 4 changement explicite ;
  f32 heure en degrés ; u16 jour ; u8 météo 0-5), envoyé à l'arrivée d'un joueur, à chaque commande
  et toutes les 15 s ; commandes console `world time H|off`, `world weather N`. L'horloge du
  monde compte depuis le démarrage du serveur.
- launcher : cartes « WEATHER » (liste + SET WEATHER) et « TIME OF DAY » (curseur 0-24 h, SET TIME,
  PLAYERS' OWN CLOCKS) ; `ServerProcess.sendCommand` écrit la commande sur l'entrée du serveur.
- runtime : `lyt_build read_texture` (bflim RGBA8/BC1/BC3) et service host `wwhd_hud_game_icon`
  (icône du jeu par frame).
- mod (`save_sync.h`) : fusion à la connexion (par blocs ≤ 3000 octets), puis différences ;
  plages locales jamais envoyées ni appliquées (vie, rubis, magie, munitions, heure, lieu…) ;
  mise à jour reçue écrite dans la sauvegarde, avec `putSave`/`getSave` si le stage courant est
  touché ; notifications (4 au plus, ~5 s) pour objets, cœurs, bourse, magie, carquois, sac à
  bombes, chants, Triforce, perles et coffres, avec l'icône du jeu.
- mod (`world_sync.h`) : heure explicite appliquée tout de suite, rappels seulement là où l'heure
  passe et au-delà d'une heure d'écart ; `mDate` n'est pas écrit (phase de lune propre au joueur) ;
  météo imposée en mer en tenant les dés en exécution ; « jeu » remet les dés à zéro.
- mod (`actor_sync.h`) : registre des blocs d'acteurs du stage et de leur résultat de création ;
  au basculement jour/nuit, les acteurs refusés de la salle courante sont redemandés, ceux déjà là
  restent (les deux périodes jusqu'au prochain chargement).

Tests :
- `mp_protocol_test` (carte capturée du serveur, codecs sauvegarde et monde) : ok ;
  `lyt_build_test` (aller-retour `read_texture`) et `item_icons_test` : ok ;
- intégration avec la copie du vrai serveur (`TestServer` comme `WWServer.main`, port 10098, deux
  clients Python) : serveur neuf amorcé par le premier, union renvoyée au second, `SERVER_SAVE_FROM`
  puis mise à jour chez l'autre, rien renvoyé à l'émetteur, `world time 20` → 300° jour 0
  explicite chez les deux, `world weather 5` : ok ; la sauvegarde est rechargée au redémarrage
  (constaté sur un passage avec `User/SaveServer` existant) ;
- wwhd.exe reconstruit ; mod 0.2.45 compilé (`-Werror`), traduit (152 fonctions, 54 hooks), imports
  vérifiés, installé instances 1 et 3 ; `javac` du launcher, de `SaveServer` et `ActorFilter`.

Non testé en jeu : application des mises à jour dans la sauvegarde vivante, notifications et
icônes à l'écran, météo forcée, recréation des PNJ jour/nuit.

Risques :
- une valeur de sauvegarde écrite pendant une scène peut ne prendre effet qu'au prochain chargement ;
- les acteurs redemandés au basculement peuvent se chevaucher avec ceux de l'autre période (voulu)
  ou refuser encore (interrupteurs, événements) ;
- la règle « le plus grand » pour les nombres peut ne pas convenir à tous les compteurs partagés.

### Crash jour/nuit, météo sur les îles, pyjama, notifications d'améliorations (0.2.46, 17 octobre 2026)

Retours en jeu sur 0.2.45 : crash en repassant de la nuit au jour sur certaines îles ; la météo
imposée ne change rien sur l'île où l'on est ; demande : pyjama tant que la tenue n'est pas
débloquée ; afficher en bas à droite les autres déblocages (quarts et réceptacles de cœur, épées et
leurs améliorations, boucliers, tenue…).

Constaté dans les logs (instances 1 et 3, `captures/crash-20261017-182546.log`) : chaque bascule
redemandait ~170 acteurs refusés (« stage actors asked again 176 ») ; crash dans 027E6664 (lecture
de 0x14) sous une création lancée par le mod (pile : multiplayer-connection.dll → 025D4614 →
02466D34 → … ). Les acteurs refusés pour une autre raison (interrupteurs, événements, objets
ramassés) étaient recréés hors contexte.

Vérifié dans les sources :
- `dKy_daynight_check` 02556D14 (bindings, sans argument) est appelé par les `init_*` des PNJ pendant
  leur création (`d_a_npc_ko1.cpp`, `d_a_npc_ob1.cpp`, `d_a_npc_kk1_a.cpp`, `d_a_npc_people.cpp`…).
- `dKy_Execute` 0255D128 : les dés de météo ne tournent que si `frameTimePass` (101EBFD8) ≠ 0 **et**
  `isItem(save + 0x71, 2, 0)` (Baguette du Vent ; `FrameSave::mItems` 0x71 = `dSv_player_get_item_c`,
  isItem = octet[slot] & (1 << bit), désassemblé) ; sinon la pluie est effacée chaque frame. C'est
  pourquoi la météo forcée était sans effet sur une île où l'heure est figée. Même condition
  (Baguette du Vent) pour l'avance de l'heure dans `setDaytime`.
- tenue : `playerInit` met le pyjama si l'event 0x2A80 n'est pas posé
  (`dSv_event_c::isEventBit` 025B8B94 sur `*(0x101F84DC) + 0x644`) ou si `save + 0x1C0` ≠ 0.
- `dSv_player_collect_c` (save + 0xB4) : `mCollect[0]` bits 0-3 épée du héros, Excalibur,
  demi-puissance, pleine puissance ; `[1]` bits 0-1 bouclier du héros, bouclier miroir ; `[2]` bit 0
  bracelets de force ; `[3]` / `[4]` bit 0 amulettes (`d_item_2.cpp`, `d_item.cpp`, `d_save_2.cpp`).

Implémenté :
- `actor_sync.h` : pendant `fopAc_Create`, le bloc en création est retenu ; un hook avant
  `dKy_daynight_check` marque son entrée du registre comme « dépend de l'heure ». À la bascule, seuls
  les refusés marqués de la salle courante sont redemandés, une fois par acteur placé (doublons
  ignorés).
- `world_sync.h` : remplacement de `dKy_Execute` : pendant une météo imposée, `frameTimePass` et le
  bit de la Baguette du Vent sont mis à 1 le temps de l'appel puis remis ; la météo du jeu (pluie,
  orage, couleurs) se fait alors aussi sur les îles à heure figée.
- `outfit.h` : « hero » choisi dans le launcher mais event 0x2A80 absent → pyjama.
- `save_sync.h` : notifications sur deux côtés ; à gauche objets, chants, perles, Triforce, coffres ;
  à droite cœurs, bourse, magie, carquois, sac de bombes, épées, puissance d'Excalibur, boucliers,
  bracelets, amulettes, tenue du héros (event 0x2A80 passé à vrai par une mise à jour reçue).
- Le temps n'avance toujours pas en continu sur les îles qui le figent (comportement du jeu gardé).

Tests : mod 0.2.46 compilé (`-Werror`), traduit (155 fonctions, 56 hooks), imports vérifiés,
installé instances 1 et 3. Non testé en jeu : absence du crash, météo sur île, pyjama, notifications
de droite.

Risques : un PNJ qui consulte l'heure ailleurs que dans sa création n'est pas marqué ; la météo
forcée écrit transitoirement un bit de la sauvegarde pendant `dKy_Execute` (remis avant toute
lecture de `save_sync`).

### Dynamic scaling, ennemis élites, HUD masquable (0.2.47, 17 octobre 2026)

Demande : vie et dégâts des ennemis selon le nombre de joueurs présents dans la zone ; ennemis
élites (petite probabilité) avec couronne, attaques plus rapides, couleur primaire du modèle
randomisée, bombes lâchées pendant le combat, grosse explosion à la mort, beaucoup plus de vie ;
dans l'onglet 02 du launcher, un bouton pour cacher le HUD.

Vérifié dans les sources :
- `fopAc_ac_c::health` s8 0x3A1, `max_health` s8 0x3A0, `group` 0x2DA, `eyePos` 0x37C (SDK actor.h).
- `dCcS::CalcTgPlusDmg` 02518670 (d_cc_s.cpp, 5 arguments) : `tgStts->m_dmg` (+0x16) prend `mAtp`
  de l'attaquant ; `cCcD_Stts::mp_actor` +0xC (c_cc_d.h). Les dégâts de Link viennent de
  `mStts.m_dmg` (`d_a_player_main_03.cpp`, `setDamagePoint(-damage)`).
- `J3DFrameCtrl::update` 027F2FC4, `checkPass` 027F2BF8 (fenêtre `[frame, frame + rate)`), `mRate` +0
  (J3DAnimation.cpp, m_Do_ext.h).
- `mDoExt_drawModel` 025E2BF4 (m_Do_ext_heap_model.cpp) : tous les `mDoExt_model*` y finissent et
  il dessine immédiatement (`mDoExtHeap_draw`) ; `J3DModel` → `J3DModelData` en +0xAC (m_Do_ext.h).
- bombe : `daBomb_prm_make` = état | cheap << 16 | zéro << 17 | 0x80000000 (d_a_bomb_static.cpp) ;
  `l_attrState` {0xA1, 0x1C, 0x9C, 0x9C, 0x20C, 0x300, 0x200, 2, 0x21} : état 0 et 8 explosent à la
  création (ATTR 1 → `procExplode_init`), état 1 mèche allumée ; état 8 `mAtp = 2`. `mRestTime` 150.
- `dMeter_Draw` 02593630 (d_meter_01.cpp, 1 argument) met les écrans du HUD dans les listes 2D.
- Le SDK documente entrée + retour sur la même fonction (exemple `heart-ticker`).

Implémenté :
- protocole : entrées `#dynamic_scaling=0|1` et `#elite_chance=N` (0-100) de la liste ActorFilter ;
  `SERVER_MUTATORS` étendu (VLQ multiplicateur, u8 randomizer, u8 scaling, u8 élite ;
  `mp_read_mutators_ex` accepte aussi l'ancienne forme). Serveur : `ActorFilter` (settings.ktml
  `dynamicScaling` / `eliteChance`, propriétés `ww.dynamicScaling` / `ww.eliteChance`, commande
  `mutators N 0|1 [0|1 P]`), `ServerProcess.Mutators`, cartes « DYNAMIC SCALING » (case) et
  « ELITE ENEMIES » (curseur 0-30 %, 5 % par défaut) du launcher.
- `elite.h` + `elite_color.h` :
  - ennemis concernés : les ennemis placés (bloc marqué) de `el_regular` (pas de boss, cochon,
    navire) ; élite décidé par FNV-1a du bloc (game UID) : mêmes élites et même couleur partout ;
  - scaling, avec n = joueurs de la zone (annuaire : même stage ; en mer même salle ou 5000
    unités ; à défaut les joueurs distants de l'aire) : vie × (1 + 0,5 (n − 1)) appliquée au
    premier execute avec vie > 0 puis suivie, plafond 127 ; dégâts reçus par le Link local × (1 +
    0,25 (n − 1)) via `CalcTgPlusDmg` quand l'attaquant est du groupe ennemi ;
  - élite : vie × 3, dégâts × 1,5 ; animations × 1,5 pendant son execute (update et checkPass) ;
    bombe allumée près du Link local toutes les 4 à 8 s s'il est à moins de 1400 unités (hors
    événement), créée non marquée (reste dans ce jeu) ; à la mort (supprimé à vie ≤ 0, ou 3 s
    après), 7 explosions immédiates (centre état 8, anneau de 6 état 0, rayon 140) ; couronne
    (`assets/crown.png`, dessinée par le script du mod, aucune donnée du jeu) projetée au-dessus
    de `eyePos` (ou de la position si `eyePos` n'est pas tenu) ;
  - couleur : histogramme des teintes de toutes les textures du modèle, fenêtre de 3 × 15° ;
    bande de ±32° tournée vers une des 12 teintes (≥ 90° de la primaire) ; modèle gris (moins de
    12 % de texels saturés, au moins 20 % de gris clairs) : gris clairs teintés. Copies dans une
    arène (3, 2 ou 1 Mio) où chaque image garde les 16 bits bas de l'original ; échange des en-têtes
    pendant `mDoExt_drawModel` d'un élite, restauré au retour, masque dirty comme `tunic.h` ;
    12 jeux de copies au plus, le plus ancien libéré.
- HUD : `hide_hud` (launcher.cfg, option manifest, case « Hide the HUD » de l'onglet 02,
  `HdModBridge`) ; `dMeter_Draw` remplacé ne fait rien tant que le HUD est caché.

Tests :
- `mp_protocol_test` (entrées, `mp_read_mutators_ex` ancienne et nouvelle forme, bornes,
  `hide_hud`) : ok ;
- `elite_color_test` (nouveau : teinte primaire, modèle gris, bande seule recolorée, ordre BC1,
  RGBA8, formats refusés) : ok ;
- serveur réel (`mutators_test.py`) : entrées d'init avec les propriétés de démarrage, commande à
  5 mots → `82 01 00 19`, forme à 3 mots qui garde scaling/élite, élite > 100 borné : ok ;
  `save_world_test.py` toujours ok ;
- mod 0.2.47 compilé (`-Werror`), traduit (170 fonctions, 62 hooks), imports vérifiés, installé
  instances 1 et 3 ; `javac` serveur et launcher.

Non testé en jeu : tout le comportement (vie, dégâts, vitesse, bombes, explosion, couronne,
couleur, HUD caché).

Risques : `dMeter_Draw` peut ne pas couvrir tous les écrans HD (mini-carte, boutons du GamePad) ;
une animation pilotée hors `J3DFrameCtrl` n'accélère pas ; un ennemi qui revit après une vie à 0
peut exploser ; l'arène des couleurs dépend du tas libre du mod (8 Mio partagés avec les skins) ;
les bombes de l'élite blessent aussi les ennemis de ce jeu (comme toute bombe).

### Élites : couleur des matériaux, taille, couronne à 10 m ; HUD caché par les écrans HD (0.2.48, 17 octobre 2026)

Retour en jeu sur 0.2.47 : ça marche plutôt bien, mais la couleur principale des ennemis ne change
pas vraiment ; demande : élites un peu plus gros, couronne cachée au-delà d'environ 10 m (comme les
pseudos) ; le bouton « Hide the HUD » ne fait rien.

Constaté dans le log (instance 1) : les élites étaient des ChuChus (`c_green`) et des Miniblins
(`Puti`) ; « HUD hidden » est bien reçu, mais le HUD reste.

Vérifié dans les sources :
- la couleur des ChuChus vient d'animations de registres TEV (`brk`, d_a_cc.cpp), pas des textures ;
- matériau HD : 0x39C octets, `J3DModelData` +0xC nombre, +0x10 tableau (d_kankyo_6.h) ; mot de
  drapeaux à +0xA0, slot k par 027F9F0C(drapeaux, k), bit 1 << k pour le renvoyer au shader ;
  couleurs TEV 0-2 = slots 4-6, couleurs konst 0-3 = slots 7-10 (d_a_bgn_draw.cpp, d_a_swhit0.cpp,
  d_a_ship.cpp, d_a_obj_ajav.cpp) ;
- `hdIngameUi_c::drawHudScreens` 02715CA0 (this, info) dessine les écrans HUD HD sur la TV (nommé
  par runtime/src/mods/story_lyt_screen.cpp, qui l'accroche aussi : les hooks du port restent hors
  de la couche des mods, docs/mod-sdk-v2.md). `dMeter_Draw` (GameCube) ne dessinait rien de visible.

Implémenté :
- couleurs de matériau : pendant `mDoExt_drawModel` d'un élite, la teinte dominante des slots 4-10
  saturés de cette instance est calculée à chaque dessin, puis ces couleurs (bande ±40°) prennent la
  teinte de l'élite (celle des textures quand c'est la même couleur), drapeaux posés, valeurs remises
  au retour ; la teinte principale des textures compte aussi les couleurs de matériau saturées ;
  bande portée de ±32° à ±40° ;
- taille × 1,25 (`scale` après l'execute, de nouveau quand l'acteur l'a réécrite) ;
- couronne cachée au-delà de 1000 unités du Link local, fondu dès 700 ;
- HUD : `drawHudScreens` remplacé ne dessine rien tant que `hide_hud` est actif, sauf menu Start
  ouvert ou événement en cours (fenêtres de message) ; le remplacement de `dMeter_Draw` est retiré.

Tests : `elite_color_test` et `mp_protocol_test` ok ; mod 0.2.48 compilé (`-Werror`), traduit
(171 fonctions, 62 hooks), imports vérifiés, installé instances 1 et 3. Non testé en jeu.

Risques : un acteur qui calcule l'échelle de son modèle avant la fin de son execute n'est pas
agrandi ; une couleur de matériau non animée et non réécrite par le jeu revient grâce à la
restauration au retour du dessin, mais si le shader lisait le slot plus tard la couleur d'origine
s'afficherait ; le HUD caché masque aussi la mini-carte TV.

### Élites tirés à chaque chargement, couleur des matériaux tournée en entier, bombes pour certains, Mothulas du randomizer (0.2.49, 17 octobre 2026)

Retour en jeu sur 0.2.48 : les élites sont toujours les mêmes à chaque retour dans la salle ; les
ChuChus ressemblent plus à une teinte qu'à un changement de couleur ; les Mothulas ne changent plus
de couleur (0.2.47 marchait mieux pour eux) ; demande : seuls certains élites posent des bombes ; le
randomizer peut faire des Mothulas volants ou non.

Vérifié dans les sources :
- `ActorHealthSync` : un short pour la vie (client GameCube : `gameActor->health = mHealth`, l'octet
  bas ; serveur Java : relais pur `readShort`/`writeShort`) ; la vie du jeu est un s8, l'octet haut
  ne fait que répéter le signe.
- Mothula `gmos` (d_a_gm.cpp create) : octet de paramètre 2 = type (1 au sol sans ailes,
  « gmos_g », ailes 15, vie 2 ; 2 volant, « gmos_f » ; 0/0xFF mini-boss), octet 1 non nul = larve
  murale (0xFF compte comme 0), octet 0 = ailes absentes.

Implémenté :
- tirage par chargement : à la création, chaque jeu tire élite (chance du serveur), poseur de bombes
  (40 % des élites) et teinte (0-11) au hasard ; le propriétaire l'envoie dans l'octet haut de la vie
  (`mp_health_word` : 0x40 | élite | bombes << 1 | teinte << 2, jamais 0x00/0xFF) ; une copie qui suit
  son propriétaire l'adopte avant son execute (vie remise au nouveau facteur, taille rendue) ;
  `sent_tag` fait renvoyer la vie quand le tirage change ;
- matériaux : toutes les couleurs de registre qui ont une teinte (saturation ≥ 0,12) tournent du même
  angle (primaire → teinte de l'élite), au lieu de la seule bande : vrai changement de teinte des
  ChuChus (corps, ombre, reflet sur plusieurs registres) ;
- la teinte principale des textures ne compte de nouveau que les textures (comme 0.2.47 : les
  couleurs de matériau de 0.2.48 la détournaient chez les Mothulas) ;
- bombes : seulement les élites poseurs ;
- randomizer : `gmos` de type 1 ou 2 (pas mini-boss, pas larve) tire 1 ou 2 ; une entrée de
  variantes dont la condition échoue passe à la suivante du même nom (`continue`).

Tests : `mp_protocol_test` (mot de vie avec tag, -1 sans tag, lectures anciennes) et
`elite_color_test` ok ; mod 0.2.49 compilé (`-Werror`), traduit (171 fonctions, 62 hooks), installé
instances 1 et 3. Non testé en jeu.

Risques : tant que le propriétaire n'a rien envoyé, chaque jeu montre son propre tirage ; tous les
joueurs doivent avoir 0.2.49 (un 0.2.48 lirait le tag comme une vie énorme).

### Couleur des ChuChus dans le tampon d'instance, palette de 20 couleurs, facultés et butin des élites (0.2.50, 17 octobre 2026)

Retour en jeu sur 0.2.49 : la couleur des ChuChus ne marche toujours pas (seuls les yeux semblent
colorés) ; les élites ont presque toujours bleu, violet ou jade ; demande : une vingtaine de couleurs
choisies (blanc et gris foncé compris) ; facultés : invoquer de temps en temps des chauves-souris ou
des Miniblins, attaques électriques avec des particules ; 3 butins de monstre et une petite chance
de rubis violet.

Vérifié (analyse locale des fichiers du joueur, rien copié ; code généré du port `build/gen`) :
- `Cc.bfres` (permanent_3d.pack, Cc.szs) : 11 paramètres de couleur par matériau (`amb_color0/1`,
  `mat_color0/1`, `tev_color0-2`, `konst0-3`) ; l'animation `cc` (FSHU) anime `tev_color1` des
  matériaux `SC_dbady` (corps) et `SC_ha` ; d'autres animent `konst3`.
- Les noms `f_XXXXXXXX` du code généré sont les adresses USA (décomp), les commentaires celles du
  jeu EUR du joueur (le désassembleur du scratchpad lit l'EUR : décalé de 4 à 0x38 octets).
- Dessin HD d'un modèle (`f_027F55FC`) : pour chaque matériau, `f_027F9FB8(matériau + 0xA0,
  J3DModel +0x34 + i × 0x3C)` copie les paramètres marqués dans l'entrée du modèle (ressource +0,
  drapeaux +4, ensemble de bits +0xC, données +0x28 ; paramètre trouvé par nom avec 027DF9B0 dans
  le dictionnaire ressource +0x38 relatif ; descripteurs de 0x14 octets à ressource +0x34 relatif,
  offset en +2, index lié en +0xC) ; puis les animations de matériau (modelData +0x38..+0x48,
  virtuelle +0x3C) ; puis `f_027F53CC(modèle, …)`. `d_a_obj_ftree.cpp` lit une `tev_color` ainsi.
  Cause du bug : 0.2.48-0.2.49 peignaient les slots partagés, recopiés puis écrasés par l'animation.
- Link : `setDamagePoint` 023F51D0, `setDamageElecEmitter` 023F9340, `procElecDamage_init` 023F95F4
  (d_a_player_main_03.cpp, le chemin d'un coup électrique) ; particule `ID_AK_JN_CCTHUNDER00` 0x3ED,
  groupe 0 (d_a_cc_exec.cpp, bindings `dComIfGp_particle_set`).
- Keese `keeth` 0xFFFF0200 (angle z 0xFF), Miniblin `Puti` (d_a_pt.cpp : bits 5-7 délai de
  réapparition, 7 = aucune ; bit 4 attente hors écran ; octets 2-3 interrupteurs) : 0xFFFF0FEF.
- `fopAcM_createItem` 025D8870 (8 arguments, action 4 = projeté, d_a_disappear.cpp) ; objets :
  trophées 0x1F, 0x45-0x4B, rubis violet 0x05.

Implémenté :
- `elite_color.h` : palette de 20 couleurs (`ec_palette`, blanc, argent, gris foncé, obsidienne
  compris) ; la couleur primaire garde sa saturation et sa luminosité moyennes (référence) ; un
  texel ou une couleur peinte prend la teinte cible + son écart à la primaire, la saturation et la
  luminosité cibles × son rapport à la référence (règle de la tunique : le blanc et le gris foncé
  marchent, l'ombrage reste) ; une entrée trop proche de la couleur du modèle passe à la suivante ;
  `ec_paint_rgb` peint toutes les couleurs de matériau qui ont une teinte.
- `elite.h` : peinture du tampon d'instance au hook de 027F53CC pendant le dessin d'un élite (les
  11 couleurs trouvées par nom, marquées à renvoyer comme 027F9FB8 ; jamais repeintes deux fois :
  une valeur égale à la dernière écrite garde son original) ; l'ancienne peinture des slots
  partagés est retirée ; textures inchangées (bande ±40°) avec la palette.
- tirage : élite, faculté (aucune 20 %, bombes 30 %, invocation 25 %, électricité 25 %), couleur
  0-19 ; tag `0x01` non élite, `0x80 | faculté << 5 | couleur` ; une créature invoquée n'est jamais
  élite.
- invocation : par le jeu qui fait tourner l'élite (`owned`), Link à moins de 1200 unités, toutes
  les 12 à 22 s, 3 au plus, Keese ou Miniblin via `as_summon` (synchronisés comme le shop).
- électricité : salves d'étincelles 0x3ED (émetteur arrêté après 10 frames, jamais gardé), et Link à
  moins de 300 unités un choc toutes les 5 à 9 s : demi-cœur × facteur de dégâts, appliqué dans
  l'execute de Link (`el_link_step` dans `multiplayer_daPy_Execute`), hors événement.
- butin : à l'explosion, par le jeu qui fait tourner l'élite, contexte OWNED (annoncé et
  synchronisé comme les objets lâchés) : 3 trophées au hasard, 15 % un rubis violet.
- launcher : texte de la carte « ELITE ENEMIES ».

Tests : `elite_color_test` (palette, blanc, gris foncé, entrée suivante, modèle gris, peinture des
registres d'un ChuChu, BC1, RGBA8) et `mp_protocol_test` ok ; mod 0.2.50 compilé (`-Werror`),
traduit (179 fonctions, 63 hooks), installé instances 1 et 3 ; `javac` du launcher. Non testé en jeu.

Risques : l'offset des données peut désigner un paramètre non utilisé par le shader (sans effet) ;
un élite électrique peut électrocuter Link dans un état où le jeu refuse (`procElecDamage_init`
rend FALSE, seul le demi-cœur est retiré) ; les trophées lâchés par `fopAcM_createItem` restent à
confirmer en jeu.

### Correctif : crash en ramassant le butin d'un élite (0.2.51, 17 octobre 2026)

Constaté (instance 1, `crash-20261017-231945.log`) : arrêt du jeu (`PROGRAM HALT`, d_save.cpp ligne
2381, « 0 <= i_no && i_no < 128 ») depuis l'execute d'un objet ramassé (daItem).

Vérifié (d_a_item.cpp) : `mItemBitNo` vient de l'octet 1 des paramètres ; pour tout objet sauf la
gelée bleue il est masqué à 0x7F (-1 → 127) ; la gelée bleue (0x4B) le garde entier et le pose
comme interrupteur au ramassage (`itemOnSwitch`). Le butin des élites, créé avec le bit -1, donnait
une gelée bleue d'interrupteur 255.

Corrigé : la gelée bleue est retirée des trophées possibles (7 restent). Mod 0.2.51 compilé,
traduit, installé instances 1 et 3. Non testé en jeu.

### Correctif : soft lock des élites électriques, étincelles à 10 m (0.2.52, 18 octobre 2026)

Retour : « élites électricité soft lock le joueur » ; étincelles visibles seulement à 10 m (lu comme
la couronne : cachées au-delà).

Cause probable, déduite (aucun log du blocage, le log avait été remis à zéro) : 0.2.50 appelait
`procElecDamage_init` 023F95F4, qui ordonne un événement compulsoire (`dEvt_control_c::compulsory`
02540310) et sort dans l'état 0x6C ; `procElecDamage` (0241ECB0) le termine après 20 frames par
`dComIfGp_event_reset` (play + 0x52B8 |= 8), mais dans d'autres modes (nage, bateau 0x2000, suspendu,
corde, portage, en l'air…) le jeu n'utilise pas ce chemin (d_a_player_main_03.cpp : sur le bateau
`setDamagePointWait`), et Link restait bloqué.

Corrigé : le choc n'est donné qu'à un Link au sol hors des modes 0x01342A32 (0x20 suspendu, 0x10
caché au mur, 2 en l'air, 0x200 grappin, 0x800 corde, 0x2000 bateau, 0x40000 nage, 0x100000
portage, 0x200000 poussée, 0x1000000 rampe) et hors événement ; l'init est appelée d'abord et le
demi-cœur et les étincelles de Link ne suivent que si elle réussit ; garde-fou : si `mCurProc` reste
0x6C plus de 60 pas, l'événement est remis à zéro (journal « Link freed »). Étincelles de l'élite
seulement à moins de 1000 unités de Link. Mod 0.2.52 compilé, traduit, installé instances 1 et 3.
Non testé en jeu.

### Correctif : crash des bombes avec un skin (0.2.53, 18 octobre 2026)

Retour : « le jeu crash quand je joue avec un skin et je m'approche des ennemis ».

Constaté (instance 1, skin `linkle` chargé, 8 crashs identiques `crash-20261018-*.log`, de 00:23 à
14:10) : lecture de null + 0x3C dans 027F5EBC EU = **027F55FC USA** (`J3DModel::viewCalc`) ←
`mDoExt_drawModel` 025E2BF4 ← `mDoExt_modelUpdateDL` 025E2DE0 ← **`daBomb_c::draw_norm` 020C4398** ←
`daBomb_c::draw` 020C4660 ← `fopAc_Draw` (remplacé par le mod). C'est la même pile que le premier
crash non identifié de 0.2.35 (« 027F55FC, lecture de null + 0x3C »). La bombe est celle des élites
(faculté bombes, une bombe allumée près de Link toutes les 4 à 8 s dans un rayon de 1400 unités).

Vérifié dans le décomp :
- la bombe normale construit son modèle depuis « Link » index 0x3C (`dRes_INDEX_LINK_BDL_BOMB_e`,
  d_a_bomb.h ; `daBomb_c::createHeap`, hors du code de tout Link, donc jamais redirigé) ;
- `draw_norm` joue sur ce modèle la BRK du Link local (`player + 0x44D4`) ;
- Link lie cette BRK à **son** modèle 0x3C avec la BRK 0x51 (d_a_player_main_01.cpp,
  `initBrkAnm(lk_getLinkRes_l(0x3C), self + 0x44D4, 0x51)` → `entry(modelData)`), pendant sa
  création, donc dans la fenêtre du skin (skin.h).

Déduit (cohérent avec les 8 piles, non vérifié instruction par instruction) : avec un skin, la BRK
est liée au modèle de bombe de l'archive du skin. Le modèle de bombe du jeu, que dessine la bombe,
n'est jamais lié, d'où le pointeur nul dans `viewCalc`. Les bombes du joueur sont concernées aussi.

Corrigé (`skin.h`, `skin_get_res`) : les index 0x3C et 0x51 de « Link » ne sont jamais redirigés vers
un skin. Avec un skin, les bombes gardent leur apparence d'origine.

Tests : mod 0.2.53 compilé (`-Werror`), traduit (179 fonctions, 63 hooks), imports vérifiés,
installé instances 1 et 3. **Non testé en jeu.** À vérifier : bombe posée par le joueur et bombes
d'élite avec le skin `linkle`, sans crash. Risque restant : les autres modèles que Link anime pendant
sa création (index 0x42, 0x37, 0x38, 0x40, 0x41, même boucle) pourraient avoir le même problème s'ils
sont dessinés par un autre acteur. Non vérifié.

### Taille des élites, lag des élites (0.2.54, 18 octobre 2026)

Retours : la taille de beaucoup d'élites ne change pas ; certaines grandissent sans fin (crânes,
Bokoblins sortis des vases), et des modèles géants apparaissent parfois dans le ciel en mer ; lag
jusqu'à 3 fps avec certaines élites, surtout plusieurs côte à côte (Keese, Magtails, Boko Babas).

**Taille — cause (lue dans le code du mod)** : `el_execute_end` multipliait `fopAc_ac_c::scale` par
1,25 chaque fois que la valeur différait de celle écrite par le mod. Un acteur qui fait évoluer son
scale par pas relatifs (apparition progressive) repartait de la valeur agrandie, puis était remultiplié :
croissance sans fin. Un acteur dont le modèle ne lit pas `scale` ne grandissait pas. Les modèles
géants dans le ciel en sont probablement une conséquence (déduit, non vérifié).

**Taille — corrigé** : `scale` n'est plus touché. Hook `WWHD_HOOK_RETURN` sur `J3DModel::calc`
(027F4D5C, vérifié : decomp-notes.md, m_Do_ext.h) ; pendant l'execute ou le draw d'une élite
(`el_growing`), les matrices du monde du modèle (modèle +0x2C → bloc, +0x10 matrices 3x4, +0x2C
nombre u16 : layout du runtime `interp.cpp`) sont multipliées par 1,25 autour de la translation de la
matrice 0. `calc` les recalcule de zéro à chaque appel : rien ne s'accumule. La collision et la
logique de l'acteur ne changent pas ; les objets attachés aux os (armes) suivent les os agrandis.
Déduit, non testé : (1) les sommets pondérés (skinning) suivent les matrices de joints, comme le
suppose l'interpolation du runtime ; (2) un modèle calculé hors execute/draw de l'acteur ne grandit
pas.

**Lag — causes (lues dans le code, non mesurées)** :
- `el_set_for` : 12 jeux de textures recolorées (modèle, couleur). Au-delà (Boko Babas : plusieurs
  modèles par élite ; plusieurs élites de couleurs différentes), le premier dessin de chaque pas
  évinçait un jeu du pas précédent et le reconstruisait (copie + recoloration de textures entières),
  en boucle à chaque pas ; l'arène pleine relançait aussi la construction à chaque dessin ;
- `el_paint_instance` : à chaque dessin (2 à 4 par pas à 120 fps), 11 recherches de nom dans le
  dictionnaire de chaque matériau, avec un cache de 8 modèles seulement (un groupe de Magtails en a
  bien plus).

**Lag — corrigé** : 24 jeux ; au plus une construction par pas ; un jeu n'est remplacé que s'il n'a
pas servi depuis 90 pas (3 s) ; un jeu qui ne tient pas n'est retenté qu'après 600 pas (20 s). Les
élites en trop gardent leurs couleurs d'origine au lieu de faire ramer. Couleurs des matériaux :
32 modèles en cache, emplacements des paramètres (entrée, table, index, offset) cherchés une fois par
modèle et revalidés par la base, le nombre et la ressource du matériau 0 ; l'adresse est recalculée
à chaque dessin depuis l'entrée +0x28.

Tests : mod 0.2.54 compilé (`-Werror`), traduit (180 fonctions, 64 hooks), imports vérifiés, installé
instances 1 et 3. **Non testé en jeu.**

### Butin des élites débloqué, choc électrique, shop en deux catégories, cœurs et magie jaunes (0.2.55, 18 octobre 2026)

Demandes : les élites ne lâchent que les butins déjà obtenus de façon vanilla, sinon des rubis
bleus ; le choc électrique fait tomber Link en arrière ; shop en deux catégories « Troll » (les
créatures) et « Upgrades & Utilities » : cœur jaune (75 rubis), magie jaune (75 rubis), Grappin long
(5 gelées bleues, 5 emblèmes de chevalier, 1000 rubis) et Cochon doré (5 plumes dorées, 100 rubis)
affichés mais pas encore disponibles. Choix de l'utilisateur : bonus temporaires façon Breath of the
Wild, personnels, 5 cœurs et une petite jauge (16) au plus, gardés jusqu'à épuisement.

**Butin — vérifié** : `dSv_player_get_bag_item_c::mBeastFlags` (save `+0x20 + 0x90 + 4`) reçoit un
bit à la première prise de chaque butin (`item_func_*` → `dComIfGs_onGetItemBeast`, d_item.cpp,
d_item_2.cpp ; lu par `item_getcheck_func_*`, d_item_4.cpp). Index `dBeastIdx_*` (d_item_4.h) :
0 collier, 1 graine, 2 plume, 3 emblème, 4/5/6 gelées rouge/verte/bleue, 7 pendentif. Rubis bleu
`dItemNo_BLUE_RUPEE_e` = 0x02 (d_item_5.h). **Implémenté** : `el_loot` tire parmi les butins dont
`isBeast` (025B7690, appelée directement) est vrai, sinon rubis bleu ; rubis violet inchangé. En
multijoueur, c'est la sauvegarde du jeu qui possède l'élite qui décide.

**Choc électrique — vérifié** : `procElecDamage` (0241ECB0, d_a_player_main_05.cpp) finit par
`procLargeDamage_init` (chute en arrière) quand `mProcVar3 != 0`, c'est-à-dire quand
`procElecDamage_init` a reçu une position source ; sans position et au sol : `lk05CheckNextMode`.
Le mod passait la position de l'élite. **Corrigé** : `procElecDamage_init(link, nullptr)`, comme le
jeu pour un sol électrifié (d_a_player_main_03.cpp). Déduit : plus de chute ni de blocage après.

**Cœurs et magie jaunes — vérifié** : toute variation passe par les compteurs différés
`mItemLifeCount` (play `+0x5B44`, f32, quarts de cœur ; `setDamagePoint` 023F51D0 y ajoute les
dégâts, doublés en mode héroïque) et `mItemMagicCount` (play `+0x5B60`, s16 ; coût des flèches dans
d_a_arrow.cpp), que `dMeter_Execute` (02593B10, d_meter_01.cpp / d_meter_02.cpp) applique à la
sauvegarde puis remet à zéro. 02593B10 diffère entre USA et EU, mais l'entrée d'une fonction est
toujours traduite (`guest_addr.h`). La vie max et la magie max sont partagées par `save_sync.h` : les
bonus n'y sont donc pas.
**Implémenté (`bonus.h`)** : `WWHD_HOOK` avant `dMeter_Execute` ; un compteur négatif est d'abord
pris sur le bonus. Fichier `bonus.cfg` du dossier Data (`hearts=` quarts, `magic=`), écrit à l'achat
et au plus une fois par seconde après une dépense. Achat : +1 cœur (4 quarts, max 20), +4 magie
(max 16, refusé sans jauge de magie : `no_magic`). Dessin HUD v2 (TV 1280×720) : cœurs jaunes générés
par le code (équation du cœur, aucun fichier du jeu), dans les emplacements après le dernier
réceptacle ; magie jaune en barre fine sous la jauge. Positions mesurées sur une capture 4K du HUD HD
(`slot1.png`) : 1er cœur centré (56 ; 36,5), pas 28,8, rangées à 25,6, 10 par rangée ; jauge de
x 44 à 331, y 79 à 98. Masqué pendant un événement, un menu ou si le HUD est caché.
Limites : un objet refusé faute de magie (le jeu teste la jauge avant de dépenser) le reste tant que
la jauge normale est vide, même avec de la magie jaune ; la position exacte de la barre jaune ne suit
pas la longueur réelle de la jauge (16 ou 32).

**Shop** : `mod.c` articles 8 à 11 (`SHOP_UPGRADES`, pour soi seulement, cible ignorée) ; 10 et 11
répondent `soon` sans rien payer ; réponses `full`, `no_magic` ; `players.txt` gagne
`bonus=quarts,magie,cœur plein,magie pleine`. Les créatures (0 à 7) et `CLIENT_PLAYER_SUMMON` ne
changent pas. Launcher : catégories TROLL et UPGRADES & UTILITIES, cartes avec description, prix
multi-butins, quantité possédée, boutons BUY / MAX / SOON (badge `soon-badge`) ; icônes `heart.jpg`,
`magic.png`, `pig.png` (déjà dans `images`) recolorées en jaune / or à l'exécution (`tintedIcon`).

Tests : mod 0.2.55 compilé (`-Werror`), traduit (184 fonctions, 65 hooks), imports vérifiés, installé
instances 1 et 3 ; `LauncherUI.java` compilé avec `javac -Xlint:all` (sortie hors dépôt) sans
avertissement. **Non testé en jeu, onglet Shop non rendu.**

**Erreur au chargement (constatée, `wwhd.log`)** : « Mod cannot use updateHudStatus_02593B10 on EU:
function differs between game builds » → mod non chargé. Le runtime refuse tout mod qui accroche ou
appelle une fonction de `differing_functions` (la note « l'entrée est toujours traduite » de
`guest_addr.h` ne suffit pas). **Corrigé** : cœurs jaunes pris dans `daPy_lk_c::setDamagePoint`
(`WWHD_REPLACE` 023F51D0, `(void* self, f32)` vérifié dans `wwhd/bindings.h` : la différence du
compteur avant/après l'appel, Link local seulement) ; magie jaune dans un `WWHD_HOOK` de
`dMeter_magicGaugeMove` (0259B698, appelée par `dMeter_Execute` avant d'appliquer la magie,
d_meter_01.cpp). Conséquence : les pertes de vie hors `setDamagePoint` (chute dans le vide…) ne sont
pas absorbées. `build_mod.ps1` vérifie désormais les hooks et appels du rapport contre les
`differing_functions` de `tools/recomp/builds/*.json` (contrôlé : 02593B10 détectée, 0259B698 et
023F51D0 acceptées). Mod recompilé (185 fonctions, 66 hooks), réinstallé instances 1 et 3. Non
relancé en jeu.

**Cœurs et magie jaunes invisibles (retour en jeu)** — deux causes vérifiées dans les sources :
- le runtime refuse tout élément HUD dont `thickness` n'est pas > 0 (`guest_hud.h` `geometry`), et
  un seul élément refusé fait abandonner **toute** la liste du pas (`list.invalid`) : avec un bonus,
  les noms, couronnes et notifications du mod disparaissaient aussi. `bonus.h` ne mettait pas
  `thickness` ;
- `bonus.h` attendait `as_link`, que seule la synchronisation des acteurs renseigne (connecté, sync
  active) : hors de ce cas, ni dessin ni absorption.
**Corrigé** : `thickness = 1` sur les trois éléments ; Link local suivi par `bonus_note_link` dans
`multiplayer_daPy_Execute` (hors puppets), considéré vivant 4 pas. Mod reconstruit (185 fonctions,
66 hooks), réinstallé instances 1 et 3. Non relancé en jeu.

### Choc électrique façon barrière ; couronnes disparues (0.2.56, 18 octobre 2026)

Retours : le choc d'un élite électrique bloque toujours Link ; les couronnes n'apparaissent plus.

**Couronnes — constaté** : la session de 15 h 11 à 15 h 23 (`captures/wwhd-previous.log`) tournait
avec le build 0.2.55 qui avait le bug `thickness = 0` de `bonus.h` : la liste de dessin entière du
mod était refusée, couronnes comprises. Corrigé au tour précédent. Ce n'est pas un problème de lag :
l'affichage n'est pas limité à une couronne. Le journal suivant (15 h 24) montre le mod non chargé
(« native code you have not confirmed ») : à réactiver dans le gestionnaire de mods.

**Choc — constaté** : aucune ligne « shocked Link » ni « Link freed » dans la session de 15 h 23 ;
la cause exacte du blocage n'est pas dans les journaux. **Changé, à la demande de l'utilisateur** :
plus de paralysie. Le choc fait ce que le jeu fait pour un sol électrifié hors corde ou eau
(d_a_player_main_03.cpp, attribut 0x16) : `setDamagePointWait` (023F9D1C, `(this, f32)` vérifié
dans `wwhd/bindings.h` : dégâts si Link n'est pas déjà en attente de dégâts, attente de 30 frames,
voix et son de dégât) puis `setDamageElecEmitter` (023F9340, étincelles sur Link). Aucun changement
de proc, aucun événement ; plus de garde-fous de mode. `el_link_step` ne dépend plus de `as_link`
(appelé seulement pour le Link local). Les cœurs jaunes absorbent ce choc (`setDamagePointWait`
appelle `setDamagePoint`, remplacé par `bonus.h`).

Tests : mod 0.2.56 compilé (`-Werror`), traduit (185 fonctions, 66 hooks), vérifications du build
(imports, fonctions qui diffèrent) OK, installé instances 1 et 3. Non testé en jeu.

### Cœurs et magie jaunes avec les dessins du HUD du jeu (0.2.57, 18 octobre 2026)

Demande : ne plus dessiner nos propres cœurs, reprendre ceux du jeu en jaune, de même pour la magie.

**Vérifié — layouts** (lecture locale de `permanent_2d_EuFrench.pack`, script d'analyse hors dépôt,
rien copié) : `Main_00` → `N_Default_00` → `L_HeartAll_00` (layout `HeartAll_00` : `L_Heart_00` à
`L_Heart_19`, 10 par rangée, pas de 40 et rangées à ±18) et `N_HeartPos_00` → `L_MagicBar_00`.
`Heart_00` : `N_Heart_00` (échelle 0,8) → `P_Base_00` (`HeartBase_00`, blanc `ffffffc8`) →
`P_Heart_00` (`HeartFull_00`), origine au centre. `MagicBar_00` : `N_Magic_00` (jauge de 16 :
`P_MagicBase_00` 196×18, `P_Magic_00` 190×12) et `N_Magic_01` (32 : 386 et 380), origine à gauche ;
le vert vient du matériau (noir `00ff00`, blanc `00a600`).
**Vérifié — mémoire** (`docs/decomp-notes.md`, runtime `story_notebook_game.cpp`,
`story_lyt_screen.cpp`) : `nw::lyt::Pane` +0x0C parent, +0x14 sentinelle des enfants, +0x3C taille,
+0x44 drapeaux (bit 0 visible), +0x45 alpha, +0x48 matrice globale 3×4 (espace du layout, origine
au centre, y vers le haut, 1 unité = 1 pixel TV), +0x80 nom ; matériau d'une image +0xF4, première
texture à +0x28 ; `hdParts_c` : layout +0x44, `nw::lyt::Layout` à +4, racine à +0xC.

**Implémenté (`bonus.h`)** : `WWHD_HOOK` sur `hdParts_c::draw` (026F8B54, `(this, viewport)`, ne
diffère pas entre builds) : l'écran dont l'arbre contient `L_HeartAll_00` est retenu avec les 20
`P_Heart_00` / `P_Base_00` et les panes de la jauge ; revalidé à chaque pas (racine et nom du pane).
Textures `HeartFull_00`, `HeartBase_00`, `Magic_00` (fond et remplissage) lues par
`wwhd_hud_game_pixels`, rouges du cœur passés en jaune, gris de la jauge en rampe jaune (comme le
matériau le fait en vert), images HUD v2 refaites après un changement d'époque. Position de chaque
case lue dans sa matrice globale : cœurs jaunes dans les cases après le dernier réceptacle (au-delà
de la 20e, une rangée plus bas), cœur partiel coupé horizontalement ; jauge jaune comme une seconde
jauge juste après le cadre du jeu (jauge de 16 ou 32 selon `N_Magic_0x` visible), un point =
11,875 unités du layout × l'échelle. Alpha = produit des alphas des parents (0 si l'un est caché).
Tout élément qui serait refusé par le runtime n'est pas envoyé (taille ≤ 0, NaN…), pour ne jamais
faire tomber la liste entière. Journal : « HUD's hearts and magic meter found », ou panes / texture
introuvables. Les anciens dessins mesurés sur capture sont retirés.

Déduit, non vérifié : que `drawHudScreens` passe par `hdParts_c::draw` pour `Main_00` ; que
`HeartFull_00` est un cœur seul (pas un atlas) ; que l'alpha des parents suit l'apparition et la
disparition du HUD. Tests : mod 0.2.57 compilé (`-Werror`), traduit (191 fonctions, 67 hooks),
vérifications du build OK, installé instances 1 et 3. Non testé en jeu.

### Cœurs jaunes au-delà de 20 ; téléportation par la reprise du jeu (0.2.58, 18 octobre 2026)

Retours : les cœurs jaunes ne doivent pas descendre d'une rangée après la 20e case mais continuer à
droite ; la téléportation du launcher « téléporte dans les airs puis ramène à la position d'origine ».

**Cœurs** : au-delà de la case 20, les cœurs jaunes continuent à droite de la case 20 (deuxième
rangée), au pas mesuré entre les cases 0 et 1.

**Téléportation — cause (déduite du code, aucun journal de tentative disponible)** : le mod posait
Link 100 unités au-dessus de la cible et l'y tenait 45 pas en écrivant `current/old.pos` après son
execute, sans charger la salle de la cible ; sans sol sous lui il tombait et le jeu le remettait à
son point de reprise (le symptôme décrit).
**Vérifié dans le décomp** : avec le point de départ -1, le joueur apparaît à la reprise de la
sauvegarde (`d_stage_2.cpp`, position et angle de `dSv_restart_c`, paramètres = `mRestartParam`) ;
`dSv_restart_c` à `*(101F84DC) + 0x1148` (`d_a_npc_os_b.h`), `setRoom` 025B9810 (`d_save_4.cpp`),
`mRestartParam` à +0x116C. Le jeu s'en sert après une chute : `dStage_restartRoom` 025C3D68
(`setNextStage(scène, -1, salle de reprise, couche -1, 0, mode, 0, 0)` puis le paramètre) ;
`startRestartRoom` : mode 5, info d'événement 0xC9, paramètre `setParamData(salle, 0, 0xC9, 0)` =
`salle | 0xC9 << 24` (`d_a_player_main.cpp`).
**Implémenté (`mod.c` `teleport_go`)** : reprise = position, angle et salle de la cible (annuaire :
salle actuelle `room`), puis `setNextStage(scène de la cible, -1, salle, couche -1 si même scène
sinon la couche de la cible, 0, 5, 0, 0)` et paramètre `salle | 0xC9 << 24`. Le jeu charge la salle
et pose Link au sol, même scène ou non. Réponse `loading`, puis `ok` quand Link est de nouveau dans
la scène hors événement (au moins 45 + 20 pas) ; `failed` (rubis rendus) si le jeu refuse. Sans
annuaire : joueur de la zone par uid, salle de Link. Retirés : saut au-dessus de la cible, maintien
en l'air, second saut après changement de scène. Conséquence : le point de reprise devient la place
de la cible. Launcher : messages `loading` et `failed`.

Non vérifié en jeu : l'animation d'arrivée (celle d'après une chute, événement 0xC9), une cible dans
l'eau ou sur un bateau. Tests : mod 0.2.58 compilé (`-Werror`), traduit (193 fonctions, 67 hooks),
vérifications du build OK, installé instances 1 et 3 ; `LauncherUI.java` compilé (`javac -Xlint:all`).

### Crash des colonnes d'eau avec skin ; bombes d'élite visibles de tous ; puppets en vue rapprochée (0.2.59, 18 octobre 2026)

**Crash — constaté** (instance 1 skin `linkle` : `crash-20261018-154938.log`, `-155303.log` ;
instance 3 skin `miku` : `-154940.log`, « indirect branch to unknown address » dans le même dessin),
chaque fois juste après « an elite enemy exploded » : `J3DModel::viewCalc` ← `mDoExt_modelUpdateDL`
← **`dWpillar_c::draw`** (025D24B0 USA, colonne d'eau) ← `fopKy_Draw` 025DA968.
**Vérifié** : la colonne d'eau d'une bombe (`mParam != 0`) prend son modèle et sa bck dans « Link »
0x42 / 0x0E (`dRes_INDEX_LINK_BDL_GWP00_e`, `d_wpillar.cpp`) ; Link lie à son 0x42 la brk 0x55 et la
btk 0x63 pendant sa création (`d_a_player_main_01.cpp`, boucle `anmLines`), donc à la copie du skin.
Même mécanisme que la bombe (0.2.53), le risque signalé alors. **Corrigé (`skin.h`)** : jamais
redirigés vers un skin, tous les modèles de cette boucle et leurs animations : 0x3C/0x51, 0x42/0x55/
0x63, 0x37/0x68, 0x38/0x65, 0x40/0x61, 0x41/0x62.

**Bombes d'élite** : la bombe allumée qui tombe sur le Link local est créée dans le contexte LOCAL
(`el_make_bomb(..., shared = 1)`), comme ses propres bombes : annoncée, les autres jeux en font une
copie avec le puppet de ce joueur comme joueur 0 (`actor_sync.h`), qui tombe sur ce puppet et dont
l'explosion blesse les joueurs proches. Les 7 bombes de l'explosion à la mort restent locales.

**Puppets à moitié dessinés — vérifié** : `daPy_lk_c::draw` lit l'état d'attention de la caméra du
Link dessiné, `play + 0x5B00 + idx × 0x34` avec `idx = +0x69BC` (`lk_camAttnStatus`) ; bit 0x20
(caméra collée : arc, grappin, vue subjective) → seules la manche et l'oreille restent visibles
(`d_a_player_main_01.cpp` 3902-3935) ; bit 2 → `checkPlayerNoDraw` (02444E58, `d_a_player_main_08.cpp`)
vrai, chemin « mains seulement ». Un puppet a l'index du Link local. **Corrigé (`puppet.h`)** :
bits 0x22 enlevés de ce mot pendant le dessin d'un puppet, remis juste après.

Tests : mod 0.2.59 compilé (`-Werror`), traduit (193 fonctions, 67 hooks), vérifications du build OK,
installé instances 1 et 3. Non testé en jeu.

### Crash des bombes pendant un changement de scène ; connexion depuis le launcher ; launcher refait (0.2.60-0.2.61, 18 octobre 2026)

**Crash — constaté** (`crash-20261018-155932.log`, mod 0.2.58) : lecture de null + 0x378 dans
`dCamera_c::ForceLockOff` (025052BC USA) ← `daBomb_c::procExplode_init` (020C5EF8) ← `create_init` ←
`daBomb_c::create`, juste après « teleport » puis des explosions d'élite. **Vérifié** : l'explosion
commence par `dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0))` (`d_a_bomb.cpp`) ; pendant le
rechargement de la scène les élites morts supprimés explosaient (`el_delete_end`) sans caméra.
**Corrigé (0.2.60, `elite.h` `el_camera_ready`)** : pas de bombe d'élite tant que la caméra du joueur
n'existe pas (id : s8 à play + 0x5B2C + 4, `WWHD_PLAY_PLAYER_OFFSET` ; caméra : play + 0x12A0 +
0x4858 + id × 0x34, `d_com_inf_game.h`, la même base que l'état de caméra des puppets).

**Connexion à la demande (0.2.61)** : jusqu'ici le mod ne se connectait qu'au démarrage et restait
`FAILED` après un échec. `connect.cfg` (`id=`, lu avec les fichiers de la carte) : la connexion en cours
est fermée, tout ce que la session avait créé est effacé (`stop_remote_views`, `clear_area_state`,
zone, annuaire, dernier aspect envoyé), puis `start()` relit `launcher.cfg` et se connecte ; le HUD
n'est enregistré qu'une fois. `players.txt` : `connection=` connecting / connected / offline.
Launcher : bouton CONNECT TO THE SERVER (onglet 1 ; enregistre l'adresse puis écrit `connect.cfg`),
état du jeu sous les boutons et dans la colonne, notification « Connected » ou échec.

**Launcher** : colonne de gauche refaite (dégradé, pastilles numérotées, barre dorée de la page
active, rubrique NAVIGATION, carte STATUS avec points de couleur Server / Game, vagues décoratives) ;
shop en deux onglets TROLL (destinataire et créatures) et UPGRADES & UTILITIES, onglet gardé tant que
le launcher tourne. Styles en fin de `launcher.css`.

Tests : mod 0.2.61 compilé (`-Werror`), traduit (194 fonctions, 67 hooks), vérifications du build OK,
installé instances 1 et 3 ; `LauncherUI.java` compilé (`javac -Xlint:all`) ; rendu hors écran de
l'onglet 1 et des deux onglets du shop (JavaFX snapshot, hors dépôt) vérifié à l'œil. Non testé en
jeu : la reconnexion (fermeture et réouverture du transport hôte), le chargement de zone après.

### Couleurs des puppets avec un Link local en skin ; notification de connexion (0.2.62, 18 octobre 2026)

Retour : en instance 3 (skin `miku`), le puppet porte les couleurs du joueur local.

**Cause (lue dans le code)** : la couleur locale est écrite en place dans la texture de tunique
partagée du Link (`tunic.h`) ; dès que le Link local porte un skin, `tunic_refresh` n'était plus
appelé et la texture partagée gardait les couleurs locales écrites avant le skin : tous les puppets
sans skin les portaient. De plus `remote_tunics` était désactivé (valeur par défaut, aucune valeur
enregistrée dans les instances) : aucun puppet n'avait ses propres couleurs.
**Corrigé** : Link local en skin → source = un puppet sans skin (`puppet_unskinned_actor`), couleurs
locales = originales (la texture partagée est rendue à ses couleurs), chaque puppet a sa copie ;
`remote_tunics` activé par défaut (manifeste et `wwhd_config_bool(..., 1)`) : l'échange de texture
pendant le dessin est le même que pour les couleurs des élites, qui fonctionnent en jeu, et les
couleurs par joueur avaient été constatées en 0.2.29.

**Notification** : un identifiant nouveau dans l'annuaire des joueurs (pas à la première réception,
ni après une reconnexion) → notification en bas à gauche (`save_sync.h` `sv_notice_joined`) :
« <nom> joined the game », avec l'icône de Link de la carte aux couleurs du joueur (`chart_face_for`,
emplacements après ceux de la carte) ; tant que l'icône du jeu n'est pas décodée (lue dans le layout
de la carte, `sea_chart.h`), la flèche de la carte (`assets/map_player.png`) teintée à sa couleur.

Tests : mod 0.2.62 compilé (`-Werror`), traduit (195 fonctions, 67 hooks), vérifications du build OK,
installé instances 1 et 3. Non testé en jeu.

### Crashs, Link figé sur le bateau, taille des cœurs jaunes, carte du launcher (0.2.63, 18 octobre 2026)

**Crash 16:17 — constaté** (`crash-20261018-161706.log`, 0.2.61) : écriture de null dans
`daMo2` `createHeap` → `mDoExt_3DlineMat0_c::init` (025E9B80) sous `fopAcM_entrySolidHeap_`
(025D6174), après « enemy copies created 257 ». **Vérifié** (`f_op_actor_mng.cpp`,
`f_op_actor_mng_2.cpp`) : un ennemi prend sa mémoire dans `mDoExt_getHeap(1)` sur la mer ou si
`stageHeapFlags` (1047E6C0) bit 0 est à 0, sinon `getHeap(0)` (025E2F64) ; une estimation qui ne
tient pas est retentée avec tout le plus grand bloc, et ce code du Moblin ne vérifie pas son
allocation. **Corrigé (`actor_sync.h` `as_solid_heap`)** : pour une copie du multiplicateur, moins de
2 Mio libres (`JKRHeap::getTotalFreeSize` 027EC1FC) → `fopAcM_entrySolidHeap` (025D63E8) renvoie
FALSE : l'échec prévu par le jeu (cPhs_ERROR_e, acteur supprimé), jamais redemandé ; journal.

**Crash 16:19 / 16:22 — constaté** (instances 3 et 1, 0.2.61) : null + 0x3C dans `viewCalc` ←
`mDoExt_modelEntryDL` (025E2E5C) ← `entryDLSetLight` ← `daPy_lk_c::draw`, appelé par le mod ; juste
après le départ d'un joueur (instance 3) et au démarrage (instance 1). Même pile qu'en 0.2.35.
**Cause non trouvée.** Les index sortis du skin en 0.2.59 (0x37, 0x38, 0x40, 0x41, 0x42) sont des
modèles dessinés par d'autres acteurs (flèche, scintillement, effet de glace, colonne d'eau ;
`d_a_arrow.h`, `d_a_arrow_iceeff.h`, `d_wpillar.h`) : Link n'en crée pas, il y lie des animations.
**Garde ajoutée (`puppet.h`)** : `mDoExt_modelEntryDL` / `mDoExt_modelUpdateDL` remplacés ; un modèle
sans données (+0xAC nul) n'est pas dessiné et le journal note le modèle, le Link qui dessine,
puppet ou non, son skin. Déduit : ne couvre pas une animation mal liée (cas de la bombe).

**Link figé sur le bateau — vérifié** : la création d'un puppet ne gardait que 12 octets de
`mPlayerInfo`, celle d'un bateau puppet que le pointeur du bateau ; or `procWait_init` (appelé pour
chaque puppet) efface le bit 0x100000 du statut 1 du joueur 0 (`d_a_player_main_02.cpp`) et la
création/suppression d'un Link ou d'un bateau écrit les statuts du joueur 0. **Corrigé** : les deux
mots de statut du joueur 0 (play + 0x5CD8 / 0x5CDC : SHIP_RIDE, SAIL, 0x100000) sont gardés et
remis autour des phases 1, 2, 3 et de la suppression d'un puppet, et de la création et de la
suppression d'un bateau puppet. Déduit : c'est ce bit qui figeait Link (une arme le remettait).

**Cœurs jaunes** : le jeu dessine ses cœurs petits sauf le dernier ; échelles lues dans les matrices
des cases de tes vrais cœurs (min et max) : cœurs jaunes à la petite, le dernier à la grande.

**Carte du launcher — vérifié** (`d_save_3.cpp`, `d_a_npc_so_mode.cpp`) : une case est débloquée
quand le bit 0 de `mFmapBits[case]` (save + 0x20 + 0xC4 + 0x40, case = ligne × 7 + colonne) est
mis : c'est ce que le poisson teste (`isSaveArriveGrid(salle - 1)`) ; trois cases au départ.
**Implémenté** : `players.txt` `chart=` (49 × 0/1) ; téléportation vers une case verrouillée refusée
(`locked`, avant de payer) ; launcher : cases verrouillées sous un brouillard opaque marqué « ? »,
joueur sur une telle case signalé (« unknown square ») et refusé ; mis à jour toutes les 500 ms.
Rendu hors écran vérifié avec un `players.txt` factice (remis ensuite).

Tests : mod 0.2.63 compilé (`-Werror`), traduit (199 fonctions, 70 hooks), vérifications du build
OK, installé instances 1 et 3 ; `LauncherUI.java` compilé (`javac -Xlint:all`). Non testé en jeu.


## Déploiement aux amis : release custom v0.3.0-online

**Vérifié** : la recomp officielle v0.3.0 (et la devel 2741b680) n'a pas les services host
`wwhd_net_*` (ni `wwhd_hud_emit`) : `data\bin\wwhd.exe` ne contient pas ces noms. Le mod importe
ces services ; `init` du module renvoie null et le runtime affiche « module ABI does not match this
game version: rebuild the mod » (`runtime/src/mods/guest_mods.cpp`, `load_one`). Recompiler le mod
n'y change rien : il faut un runtime qui fournit ces services.

**Implémenté et testé (installation)** : release construite comme `.github/workflows/release.yml`
(job windows) depuis ce fork :
- toolchain `llvm-mingw-20260922-ucrt-x86_64` (celle de l'install), cmake/ninja MSYS2 mingw64,
  Python MSYS2 (le Python embarqué ignore le dossier du script : `write_map_fixture.py` échoue) ;
- `stubgen.py build/gen-stub` (aucun code du jeu), `build/rel`, cibles `wwhd wwhd-extract
  wwhd_yaz0 wwhd-setup` seulement : `guest_hooks_test` ne se lie plus (`item_icons::game_icon`,
  `mp_skins::set_directory` absents de sa liste de sources) ;
- `tools/release/package.py --version v0.3.0-online`, Python embarqué 3.14.8 et Vulkan-Headers
  1.4.363 épinglés (SHA-256 vérifiés), `tools/shadercache/template.bin` repris de la release
  officielle ;
- `tools/release/guard.py` : OK (292 fichiers, aucun code/fichier du jeu, aucune clé).
Sortie : `Client\dist\WindWakerHD-v0.3.0-online-windows-x86_64.zip` (31 Mo),
`multiplayer-connection-0.2.63.wwhdmod` (manifest, mod.elf, assets), `LISEZMOI.txt`.
Test : zip extrait dans un dossier neuf, `Setup in a console window.bat --yes --no-launch
--game-dir <dump EUR>` : 6/6 étapes OK, `data\bin\wwhd.exe` contient `wwhd_net_connect` et
`wwhd_hud_emit`. Dossier de test supprimé (il contenait le jeu compilé depuis le dump).
Non testé : chargement du mod et connexion dans cette install (lancement graphique).

## Skins GameCube (format Wind Waker Randomizer) : `make_gc_skin.py`

Demande : importer les skins GameCube (exemple `ZeldaWWHDRecomp/tools/skin/AmongUs` : `Link.arc`,
`metadata.txt`, `color_masks/`, `preview/`).

**Vérifié hors ligne** (Link.szs EUR du joueur, rien copié dans le dépôt) :
- `Link.bfres` HD (BFRES 3.4.0.1) contient les 54 mêmes modèles que le `Link.arc` GC, mêmes noms ;
- `cl` HD : 42 os aux noms et poses GC, 24 matériaux aux noms GC (`ear(2)` → `ear_2_`), un FSHP
  par matériau nommé `<os>__<matériau>` ; maillages riggés (os 0, `vtx_skin_count` 2 à 4) en espace
  modèle, `_i0` = index de matrice lisse (table FSKL), poids `_w0` ; maillages rigides (mains, yeux,
  sourcils...) en espace local de leur os ; `_c0` constant par maillage (noir pour les yeux) ;
- `cl.sharcfb` : un modèle de shader par matériau (mêmes noms), option `vtx_skin_count` ;
  `hd_res_model.cpp` charge `<modèle>.sharcfb` et lie par `hdResModel_bindShader` (027735DC) ;
- arc d'origine du `Link.arc` comparé à l'arc HD : chaque vertex GC (ramené en espace modèle par
  la pose de repos J3D, T·Rz·Ry·Rx·S) coïncide à 0,000 près avec un vertex HD, UV identiques
  (pas de retournement), les 324 triangles HD dans l'ordre inverse de la lecture GC des strips ;
- textures HD : BC1/BC3 sRGB (couleur), BC5 avec sélecteurs R,R,R,G (yeux, sourcils), 4× la
  résolution GC ;
- le `cl.bdl` Among Us garde la structure (42 os, 24 matériaux, 24 formes) ; parmi les modèles,
  seul `cl` diffère des positions HD (les autres « différents » sont des refontes HD : grappin,
  effets).

**Implémenté** : `tools/multiplayer/j3d.py` (RARC, Yaz0, J3D : JNT1/EVP1/DRW1/VTX1/SHP1/MAT3/TEX1/INF1,
textures I4/I8/IA4/IA8/RGB565/RGB5A3/RGBA8/C4/C8/C14X2/CMPR) et `tools/multiplayer/make_gc_skin.py` :
gabarit = `Link.szs` du jeu du joueur ; pour chaque modèle converti (par défaut ceux de `cl`,
`hands`, `katsura` dont les vertex diffèrent du HD), chaque FSHP reçoit la forme GC du même
matériau (buffers ajoutés, même disposition de vertex et même nombre de poids, poids réduits aux
plus forts si besoin, os sans matrice lisse → parent, liste des os du maillage réécrite, boîte
large pour les riggés) ; textures nommées par les matériaux et leurs images d'animation
remplacées en RGBA8 linéaire (sRGB gardé, sélecteurs gardés), sauf la rampe `ZBtoonEX` du shader
HD. Sortie : `skin_<id>.szs` de `make_skin.py` (`MpSk_<id>.bfres`). Test `mp_gc_skin`
(`test_make_gc_skin.py`, sans fichiers du jeu).

**Testé** : `test_make_gc_skin.py` OK ; Among Us converti en 6 s (cl, 35 textures, aucun poids
réduit, aucun vertex déplacé), 24,4 Mo ; rendu hors jeu de `cl` relu depuis le BFRES produit
conforme à `preview/preview_hero.png` ; `skin_amongus.szs` copié dans le dossier Data des instances
1 et 3. **Non testé en jeu.**

**Limites** : couleurs personnalisées (`hero_custom_colors`, masques) non gérées ; un modèle GC à
structure différente (autres matériaux ou os) est refusé avec un message ; import par le launcher
(`SkinImporter.java`) non branché : outil Python seulement.

## Crash en entrant dans une zone, cœurs rouges/jaunes, heure et météo partagées par un navigateur (0.2.64)

**Crash (vérifié dans les journaux)** : `crash-20261018-171223.log` et `-202101.log`, même pile :
`as_solid_heap` → `fopAcM_entrySolidHeap_` (025D6174) → `daBoko_c::createHeap` (020C1DA8) →
création du modèle J3D (025E38A0 → 027F4CC4 ... 027BF644), écriture à l'adresse 0. Juste avant :
« enemy copy not made ... 1945356 » : la garde de 2 Mo refusait les copies, mais l'arme d'une copie
(daBoko, créée par la copie, non marquée) ne trouvait plus de bloc pour son estimation ; le jeu
retente alors avec le plus grand bloc et le modèle échoue sans vérifier.
**Corrigé (`actor_sync.h` `as_solid_heap`)** : copies refusées sous 6 Mo libres ou quand le plus
grand bloc libre est sous 2 Mo (JKRHeap `getFreeSize`, vtable +0x74, vtable du tas en +0xC :
vérifié dans `story_lyt_screen.cpp` et `f_op_actor_mng.h`) ; tout acteur dont l'estimation (+ 4 Ko)
ne tient plus dans le plus grand bloc est refusé (échec du jeu, acteur supprimé) au lieu de la
reprise qui plante ; journal avec le procédé. Déduit, non testé en jeu.

**Cœurs (vérifié dans `Heart_00.bflyt` / `.bflan` du jeu, rien copié)** : un cœur =
`N_Heart_00` (échelle 0,8 ; `Heart_00_Max` la change : le cœur courant grossit) > `P_Light_00`
(halo, `Heart_00_Loop` anime son alpha et son échelle), `P_Base_00` > `P_Heart_00`, `P_Gloss_00`.
**Implémenté (`bonus.h`)** : avant chaque dessin de l'écran (hook `hdParts_c::draw`, avant
l'original), quand des cœurs jaunes suivent : le gros cœur du jeu reçoit la partie linéaire des
matrices d'un autre emplacement (même taille que les autres, idempotent), son `P_Light_00` est
caché (bit visible, remis ensuite) ; le dernier cœur jaune est grand et reçoit le halo (texture du
halo recolorée en jaune, alpha animé du jeu). Non testé en jeu.

**Heure et météo** : nouveau paquet d'extension `CLIENT_WORLD` (corps de `SERVER_WORLD`,
`mp_write_world`, test `mp_protocol_test` aller-retour). Le mod l'envoie toutes les 2 s quand le
joueur est sur `sea`, que le temps passe (`frameTimePass`) et hors événement : heure, jour, et la
météo de ses dés quand elle n'est pas imposée (DICE_MODE 0-5 de `d_kankyo_4.h` → 1,2,3,4,5,5).
Serveur (`ActorFilter.reportWorld`, `WWClient.receiveWorld`) : suit un seul navigateur à la fois
(le premier, jusqu'à 6 s sans rapport), en fait l'horloge du monde (sauf heure fixée par le
launcher) et sa météo celle des autres (sauf météo fixée par le launcher) ; le navigateur suivi ne
reçoit pas sa propre météo (ses dés continuent) et reçoit à sa prise de rôle une météo levée ; sans
navigateur, les autres retrouvent leur météo.
Tests : mod 0.2.64 compilé (`-Werror`), traduit (200 fonctions, 70 hooks), vérifications OK, installé
instances 1 et 3 ; `mp_protocol_test` OK ; sources du serveur compilées (javac, sortie temporaire).
Non testé en jeu ni avec le serveur lancé : le serveur doit être recompilé (IDE) pour `CLIENT_WORLD`.

## Cœurs jaunes coupés en quarts, import des skins GameCube par le launcher (0.2.65)

**Quarts de cœur (vérifié hors ligne)** : `Heart_00.szs` `HeartDamage_02/01/00^l.bflim` (60 × 50,
décodés par `tools/story_mods/lyt/bflim.py`) : 1/4 = quart haut gauche, 2/4 = moitié gauche, 3/4 =
tout sauf le quart haut droit, coupe au texel (31, 24). **Implémenté (`bonus.h`)** :
`bonus_emit_quarters` dessine 1 à 3 quadrants de l'image du cœur jaune (sous-rectangles UV du HUD v2)
au lieu d'une coupe horizontale. Non testé en jeu.

**Skins GameCube** : `tools/skin/Garfield/` contient le skin Among Us (Link.arc, aperçus et
metadata identiques octet pour octet, auteur sup3rgh0st) : ce n'est pas Garfield. `Sora` se convertit
(make_gc_skin.py : 2 vertices réduits à 3 poids, 253 vertices de `podA`/`ear(8)` rattachés à leur os),
rendu hors jeu conforme à `preview_hero.png` (iris bleu dans `hitomi`, seconde texture des yeux).
Cause de « ne marche pas » : le bouton IMPORT A SKIN du launcher (`SkinImporter`) ne connaissait que
les packs HD (`permanent_3d.pack`/`Link.szs`) ; seul Among Us avait été converti à la main.
**Implémenté** : `GcSkinConverter.java`, portage de make_gc_skin.py (RARC, J3D, textures, BFRES) ;
sortie identique octet pour octet au Python sur Among Us et Sora. Le launcher : un dossier avec un
`Link.arc` passe par GcSkinConverter ; dossier du jeu = `game_dir` du `data\install.json` au-dessus du
dossier Data du mod (installation release), sinon celui retenu (préférence `gameDir`), sinon demandé
une fois. Notes de conversion dans la console du launcher.
Tests : mod 0.2.65 compilé, traduit (200 fonctions, 70 hooks), installé instances 1 et 3 ; serveur et
launcher compilés (javac) ; `skin_sora.szs` copié dans le dossier Data des instances 1 et 3. Non testé en jeu.

## Copies d'ennemis sans plafond fixe, tous les skins GameCube (0.2.66)

**Tas des acteurs (vérifié dans `f_op_actor_mng_2.cpp` / `f_op_actor_mng.cpp`)** : en HD,
`fopAcM_entrySolidHeap` (025D63E8) ignore l'estimation et appelle `fopAcM_entrySolidHeap_`
(025D6174 : acteur, callback, taille 0, tas parent) : le tas de l'acteur prend tout le plus grand bloc
libre puis est réduit. Parent : `mDoExt_getHeap(1)` (025E2F64) en mer ou si `stageHeapFlags` bit 0
est éteint, sinon `getHeap(0)` ; `getGameHeap` (025E2FBC) = `getHeap(0)`. Les autres tas (archives,
commandes, animation : `m_Do_ext_tail.cpp`, `m_Do_dvd_thread.cpp`) servent aux chargements : non utilisés.
**Implémenté (`actor_sync.h` `as_solid_heap`)** : la taille réellement utilisée par chaque type
d'acteur est retenue (tas à `fopAc_ac_c` + 0xF4, `getHeapSize` vtable +0x6C) ; une copie va dans
celui des deux tas d'acteurs qui a le plus grand bloc libre (appel direct de 025D6174), refusée
seulement si aucun n'a sa taille + 25 % + 64 Ko + 1 Mo de réserve ; un autre acteur (arme d'une copie
comprise) dont le tas choisi par le jeu n'a plus de bloc pour sa taille connue est construit dans
l'autre tas, refusé seulement si aucun n'en a (taille inconnue : le jeu essaie comme avant). Les
seuils fixes de 6 Mo / 2 Mo sont retirés. Déduit, non testé en jeu.

**Skins GameCube** : `tools/skin/Garfield/Link.arc` est maintenant un autre fichier (PiPPiN, 1 144 160
octets) : `cl`, `hands` et `katsura` modifiés, os nommés `skeleton_root_<nom>`, matrices de liaison
(EVP1) différentes de la pose de repos des os (écart 1,0 sur `chest_jnt`). Autre modèle testé :
« Nouveau dossier (2) » (Link remodelé, formes sans index de matrice, yeux sans normales) et le
`Link.arc` GameCube d'origine.
**Corrigé (`j3d.py`, `make_gc_skin.py`, `GcSkinConverter.java`)** :
- vertex pondérés ramenés à la pose de repos comme le jeu GameCube les dessine :
  Σ poids × (os au repos × inverse de liaison EVP1) × position (corrige Garfield et les jambes
  d'Among Us) ;
- os : même nom, ou nom finissant par « _ » + nom HD (le plus long), sinon l'os du parent (note) ;
- matériau GC sans équivalent HD : dessiné dans la forme HD qui a la même première texture (le plus
  de poids), sinon laissé de côté (note) ; forme HD sans forme GC : un triangle dégénéré (note) ;
- texture renommée dans un matériau GC : remplace la texture HD de même place.
Tests : `test_make_gc_skin.py` (noms d'os, pose de repos) OK ; Python et Java identiques octet pour
octet sur Among Us, Sora, Garfield, l'autre modèle et le Link d'origine ; rendus hors jeu de Garfield,
Among Us, Sora, l'autre modèle conformes ; skins copiés dans les instances 1 et 3 ; mod 0.2.66
compilé, traduit (201 fonctions, 70 hooks), vérifications OK (025D6174 identique EU/USA) ; serveur et
launcher compilés. Non testé en jeu.

## Mort partagée : fantôme, cadavre et réanimation (0.2.67)

Demande : pas de game over tant qu'un joueur du même stage est vivant ; le mort laisse son corps et
devient un fantôme sans interaction ; un vivant le réanime en maintenant A 6 s devant le corps
(texte + jauge). Choix du joueur : réanimé sur son corps avec 3 cœurs, fantôme presque invisible pour
les autres, chute dans le vide ou la lave = retour au corps.

**Vérifié (`d_a_player_main_03/04.cpp`)** : `daPy_lk_c::changeDeadProc` (023F7820) : sans événement,
démo 0, vie 0, hors `ModeFlg_DAMAGE` (8) : fée en bouteille (`checkBottle` 025B5C80, objet 0x57,
`mPlayerItem` = save + 0x20 + 0x3C) sinon `dProcDead_init` (au sol) ou chute ; `setStickData`
(023F32E4) remet puis remplit `mItemTrigger` (+0x690D) et `mItemButton` (+0x690E) ; `setCollision`
(024076CC) enregistre le cylindre de Link (co/tg) et ses attaques ; `checkFallCode` (023FEA6C) : vide
→ `startRestartRoom` (023FD4E4, dégâts + retour), sorties → `changeSceneExitId` (025C38B8) (et HD :
sorties coupées quand la vie est à 0) ; lave → `dProcLavaDamage_init` (023FE6B8) ; `procWait_init`
(023E2FF4).

**Implémenté (`death.h`, branché dans `mod.c`, `bonus.h`, `puppet.h`, `puppet_sense.h`)** :
- `changeDeadProc` remplacé : Link local à 0 vie, au sol, sans fée, avec un joueur vivant dans la zone
  (les `remotes` de l'ActorSync) → fantôme (corps = sa position et son angle), Link reste debout à 0 vie ;
- fantôme : boutons effacés après `setStickData` (le stick reste), aucun collider (`setCollision` non
  appelé), aucun dégât (`setDamagePoint`, bonus.h), aucune sortie (`changeSceneExitId` refusé), vide et
  lave → retour au corps (`startRestartRoom`, `dProcLavaDamage_init` remplacés) ; les ennemis visent un
  vivant plutôt que le fantôme, jamais un corps ou un fantôme (puppet_sense.h) ;
- protocole : bloc « mort » après le skin (MP_DEATH_GHOST : VLQ id annuaire, corps x y z, angle y),
  gardé et renvoyé par `ActorPlayerSync.java` ; `CLIENT_PLAYER_REVIVE` (VLQ id, nom) →
  `SERVER_PLAYER_REVIVE` (nom) au seul joueur visé (`WWClient.receiveRevive`) ;
- chez les autres : le corps = un puppet (emplacement `PUPPET_BODY_UID | uid`, couleurs/skin du joueur,
  `ANM_DIELONG` 0x62 une fois), le fantôme dessiné un pas logique sur deux, son nom à 35 % ; marqueur
  « nom (down) » sur chaque corps, « Your body » pour le sien ; `PUPPET_MAX` 8 ;
- réanimation : vivant à moins de 1,5 m d'un corps → « Hold A to revive X » + jauge, A maintenu 6 s
  (relâché : la jauge redescend), A ne fait rien d'autre près d'un corps ; le mort reçoit 3 cœurs
  (vie en attente remise à 0), est replacé sur son corps (`procWait_init`) ;
- game over : plus aucun vivant dans la zone (ou connexion perdue) → le fantôme laisse
  `changeDeadProc` d'origine le tuer (écran de game over normal).
Tests : `mp_protocol_test` OK (nouveau `test_death` : bloc mort, ancien expéditeur, troncature,
paquets revive ; les vecteurs existants testent l'octet « vivant » puis l'ancienne forme) ; mod 0.2.67
compilé (`-Werror`), traduit (211 fonctions, 76 hooks), aucune fonction différente EU/USA, installé
instances 1 et 3 ; serveur et launcher compilés. **Non testé en jeu.**
Limites connues : le fantôme peut encore déclencher des zones d'événement ou des interrupteurs au sol ;
« presque invisible » = clignotement (pas de vraie transparence de modèle) ; au-delà de 4 puppets,
couleur de tunique par défaut (`TUNIC_MAX_SLOTS` 4) ; corps montré seulement avec les puppets activés
(sinon le marqueur) ; réanimation vers un corps dans une salle déchargée non traitée.

## Heure et météo du jeu en direct dans les mutateurs (launcher)

**Implémenté** : `ActorFilter.publishWorld` écrit sur la sortie du serveur `@world T W SOURCE` (T l'heure
en degrés ou « - » = horloge de chacun, W la météo partagée 0-5, SOURCE `launcher`, `sea NOM` ou
`none`) quand la source ou la météo change, quand l'horloge s'écarte d'un degré de ce que le launcher
calcule, et toutes les 15 s. `ServerProcess` lit ces lignes (hors console : `readWorld`) et garde
`World` ; `World.hoursNow()` avance l'heure au rythme du jeu (0,6° par seconde, comme
`WORLD_DEGREES_PER_SECOND`). `LauncherUI` met à jour toutes les 500 ms « In game now: HH:MM
(day/night), from X at sea / set here » dans TIME OF DAY et « At sea now: … » dans WEATHER.
Tests : serveur et launcher compilés ; harnais JavaFX hors jeu (scratchpad `SnapWorld.java`) : la ligne
d'état est prise et cachée, une autre passe ; 195° → « 13:07 (day), from Tetra at sea » après 3 s ;
« - 0 none » → horloge et météo de chacun ; capture de l'onglet MUTATORS conforme. Serveur non lancé
en vrai hors IDE (ressources). Non testé avec des joueurs.
## Bot de test « pousser une caisse » dans la page Debug (0.2.80)

Demande : un bouton du menu Debug qui fait apparaître devant le joueur un bot qui pousse une caisse,
pour tester la mécanique des blocs lourds (`heavy.h`) seul.

**Vérifié (`d_a_obj_movebox.cpp` / `.h` du décomp)** : daObjMovebox, nom de processus 0x2B ;
paramètres : type bits 24-27, interrupteur bits 8-15 (0xFF : `chk_appear` vrai, `path_init` sort),
objet bits 0-5 et son bit 16-22 (`make_item`), bit 30 (`mPrmZ` pris de `angle.z`), bit 31 = pas de
bouée (sinon enfant 0x1D1) ; taille de case : attribut +0x0C de `M_attr` (101CB424, déjà utilisé
par `actor_sync.h`). `ANM_WALKPUSH` 0x7F joué par `procPushMove` avec
`setSingleMoveAnime(0x7F, 1, 0, -1, 5)` (`d_a_player_main_07.cpp`).
**Déduit, non vérifié en HD** : la création charge elle-même l'archive de la caisse (type 0,
« Kkiba ») comme le `Mthd_Create` GameCube (`dComIfG_resLoad`) ; le create HD n'est pas dans le
décomp. Si la caisse n'est pas créée en 600 pas, le journal le dit et le bot est retiré.

**Implémenté** :
- `push_bot.h` : `debug.cfg` `bot=1` (bot qui pousse), `bot=2` (bot qui ne pousse pas), `bot=0`
  (retrait). Caisse type 0 (paramètres 0x807FFF3F, `angle.z` 0xFFFF) créée par `fopAcM_create` à
  1,5 m + demi-case devant Link, tournée comme lui ; le bot est un puppet (emplacement
  `PUPPET_BOT_UID` 0x20000000) sur le côté proche de la caisse, qui la suit quand elle bouge et
  rejoue `ANM_WALKPUSH` ; libellé « Bot (pushing) » / « Bot (not pushing) » au-dessus. Caisse
  disparue (zone quittée, cassée) → bot retiré. Sans l'option `puppet`, seul le libellé est dessiné.
- `heavy.h` : le bot compte comme joueur de la salle (`pb_room_player`) et, en mode 1, comme aide
  à la poussée à moins de 4 m (`pb_pushing_near`, jamais pour soulever) ; avec le bot, la mécanique
  s'applique aussi hors connexion. Attendu : bot qui pousse → 2/2, la caisse avance ; bot qui ne
  pousse pas → 1/2, la caisse reste.
- `puppet.h` : `puppet_clear_all` garde l'emplacement du bot (sans serveur, l'échec de connexion
  vide les puppets à chaque pas) ; `push_bot.h` le retire avec la caisse.
- `mod.c` : clé `bot=` dans `debug_step`, `pb_step` après `hv_step`, `pb_draw` après `hv_draw`.
- Launcher (`LauncherUI` page Debug) : panneau « PUSH BOT (HEAVY BLOCKS) », boutons SUMMON BOT
  PUSHING A CRATE, SUMMON BOT NOT PUSHING, REMOVE BOT.

Tests : mod 0.2.80 compilé, traduit (222 fonctions, 82 hooks), imports et fonctions différentes
EU/USA vérifiés, installé instances 1 et 3 ; `LauncherUI.java` compilé (javac, sortie temporaire).
**Non testé en jeu** (création de la caisse hors de sa salle d'origine, animation du bot, comptage).

## Compteur des blocs lourds agrandi, créature du shop en double chez la cible (0.2.81)

**Compteur** (`heavy.h` `hv_draw`) : le « 1/2 » au-dessus de ce qu'on porte ou pousse est dessiné à
l'échelle 1,2 au lieu de 0,6 (deux fois plus grand).

**Créature du shop en double** : retour : la cible d'un « troll » voit deux ennemis, les autres un seul.
Constaté (journal de l'instance 3) : la même créature d'un autre joueur (`c_green`) fabriquée deux
fois à environ 12 s d'intervalle (« another player's summoned creature made here »).
Lu dans le code : `as_summon_remote` ne retenait que les créatures faites à partir d'un autre jeu, et
seulement 150 pas (environ 5 s) ; `as_summon` (créature invoquée ici, donc chez la cible) ne retenait
rien. Une création renvoyée plus tard par le serveur (`as_match_create` ne trouve plus d'emplacement
sans uid) était donc refaite. Serveur relu (`SyncedActorClient.receiveClientActorSyncCreate`,
`SyncedActorManager.broadcastActor`) : il déduplique par game UID ; l'origine exacte du renvoi n'est
pas prouvée (déduit : un bloc renvoyé un peu différent, ou un acteur serveur retiré puis recréé).
**Corrigé (`actor_sync.h`)** : les créatures invoquées ici et celles faites depuis un autre jeu sont
retenues (16 dernières, sans délai d'oubli) ; une création reçue pour la même créature (nom,
paramètres, position et salle) n'est plus refaite. Le journal écrit alors « a summoned creature this
game already has, not made again » et les octets du bloc qui diffèrent (pour trouver l'origine).
Tests : mod 0.2.81 compilé, traduit (223 fonctions, 82 hooks), vérifications OK, installé instances 1
et 3. **Non testé en jeu.**

## Compteur des blocs lourds visible chez les autres joueurs (0.2.91)

Demande : quand un joueur pousse, tire ou essaie de soulever une chose lourde, le compteur « 1/2 »
doit aussi s'afficher chez les autres, pas seulement chez lui.

**Implémenté (`heavy.h` `hv_remote_step`, `hv_draw`; `push_bot.h` `pb_pushing_near_pos`)**, sans
changement de protocole : chaque jeu calcule le compteur à partir de ce qu'il reçoit déjà
(ActorPlayerSync : proc et position de chaque joueur). Un compteur s'affiche pour un joueur distant
vivant, à moins de 30 m, dont le proc est PUSH_PULL_WAIT, PUSH_MOVE ou PULL_MOVE (pousser ou tirer)
ou GRAB_MISS (forcer sur un objet lourd ; GRAB_READY est écarté car c'est aussi le début de tout
soulèvement léger). Aides comptées : ce joueur, les autres joueurs distants qui font la même action à
moins de 4 m de lui, le Link local s'il la fait aussi, et le bot de test. Nombre requis : celui de la
salle du Link local, calculé comme chez le pousseur. Position : le bloc synchronisé (emplacements
d'`actor_sync.h`, processus 0x2B) le plus proche à moins de 2,5 m du joueur, sinon au-dessus du
joueur. Un seul compteur par objet : pas en double avec celui du Link local, ni pour deux joueurs au
même bloc. Seulement en ligne (READY).
Déduit, non testé : le nombre peut différer d'un instant de celui du pousseur (positions lissées, proc
reçu avec retard) ; un objet soulevable n'étant pas connu à distance, le compteur de soulèvement est
au-dessus du joueur.
Tests : mod 0.2.91 (sur le commit 0.2.90 tiré entre-temps) compilé, traduit (237 fonctions,
90 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## Crash des flèches des autres joueurs, lueur des paquets de flèches élémentaires au bout des flèches (0.2.92)

**Crash (instance 1, `crash-20261019-181108.log`, mod 0.2.90)** : saut indirect vers 0 (lr 027F6068)
dans `J3DModel::viewCalc` ← `mDoExt_drawModel` ← `draw_update_dl` du mod (puppet.h) ←
`mDoExt_modelUpdateDL` ← **`daArrow_c::_draw` (02053188)**.
Vérifié (`d_a_arrow.cpp`, `d_a_arrow.h`) : `createInit` (02052738) garde l'animation de la pointe du
Link joueur : `mpBtk` (+0x3B8, entre `mpModel` +0x3B0 et `mLinChk` +0x3BC vérifié) = link + 0x46AC
(glace/feu) ou + 0x4720 (lumière) ; `_draw` lui donne sa frame à chaque dessin. Lu dans le mod : une
copie de flèche d'un autre joueur est créée avec le puppet du tireur comme Link joueur
(`as_swap_in`), donc elle gardait un pointeur dans le puppet.
Déduit (cohérent avec la pile, non reproduit) : le puppet supprimé (joueur parti, puppet refait), la
flèche dessinait avec une animation libérée. Les index du modèle de flèche (0x37, 0x38) étaient déjà
protégés des skins (0.2.53) : ce n'est pas la cause.
**Corrigé (`actor_sync.h` `as_arrow_btk_home`)** : après la création d'une copie de flèche, un `mpBtk`
qui pointe dans le puppet est déplacé au même champ du Link local (ce qu'utilise une flèche normale).

**Lueur du paquet élémentaire** : retour avec capture : la lueur est au pied du paquet, debout.
Vérifié (`d_a_item.cpp` `set_mtx` / `set_mtx_base`, `d_a_item.h`) : les modèles du paquet tournent de
`current.angle.y` + `mHDRotY` (+0x76C). Vérifié (`d_a_arrow_lighteff.cpp` `set_mtx` 02056EB8) :
`transS(pos)`, `ZXYrotM(angle.x, angle.y, 0)`, puis 62 unités selon +Z, donc direction
(cos p · sin y, sin p, cos p · cos y) pour `angle.x = -p`.
**Implémenté (`bonus.h` `bonus_glow_execute`)** : la lueur est orientée selon les flèches et sa pointe
(particules, lumière) placée au bout : direction = rotation du paquet + réglage, inclinaison 40°,
longueur 55 depuis le pied, pied à 5 unités au-dessus du sol. La position du bout dans le modèle
n'est pas dans le décomp : **estimée d'après la capture**, réglable en direct (clés `debug.cfg`
`glow_yaw`, `glow_pitch`, `glow_len`, `glow_base` ; launcher, page Debug, panneau « ELEMENTAL ARROW
GLOW » ; perdu au redémarrage du jeu, à reporter dans les valeurs par défaut une fois trouvé).
Tests : mod 0.2.92 compilé, traduit (237 fonctions, 90 hooks), vérifications OK, installé instances 1
et 3 ; `LauncherUI.java` compilé (javac, sortie temporaire). **Non testé en jeu.**

## Chance des cœurs jaunes réglable depuis la page Debug (0.2.93)

**Implémenté** : `bonus.h` : la chance d'un cœur jaune par butin (`BONUS_DROP_HEART`, 4 pour mille)
devient la variable `bonus_drop_heart`, utilisée par `bonus_yellow_roll` (la magie jaune garde ses
4 pour mille, juste après). `mod.c` `debug_step` : clé `debug.cfg` `yellow_heart=` (pour mille,
0-1000). Launcher, page Debug : panneau « YELLOW HEART DROP CHANCE », slider 0-100 % (au dixième,
envoyé en pour mille), APPLY et DEFAULT (0,4 %). Valeur perdue au redémarrage du jeu. À 100 %, la
magie jaune ne tombe plus (son intervalle de tirage est après celui du cœur).
Tests : mod 0.2.93 compilé, traduit (237 fonctions, 90 hooks), vérifications OK, installé instances 1
et 3 ; `LauncherUI.java` compilé (javac, sortie temporaire). **Non testé en jeu.**

## Lueur des flèches élémentaires : modèle décalé de la pointe (0.2.94)

Retour avec capture (paquet de flèches de lumière) : la grosse lueur est à côté de la flèche et les
petites particules ailleurs ; demande : les deux sur la pointe.
Vérifié (`m_Do_mtx.cpp` `mDoMtx_ZXYrotM` 025F1B48 : Y puis X puis Z ; `d_a_arrow_lighteff.cpp`
`setTopPos`, `set_mtx`, `CreateInit`) : les particules (`dPa_followEcallBack` sur `field_0x29C`,
+0x3B8) et la lumière suivent le « top », qui est aussi l'origine du modèle de la lueur (même
matrice + 62 selon Z). Le calcul de 0.2.92 met donc l'origine du modèle exactement sur le top.
Déduit (de la capture) : le maillage de la lueur est dessiné à l'écart de son origine. Le décalage
n'est pas mesurable dans le décomp.
**Implémenté (`bonus.h`, `mod.c`, launcher)** : deux réglages qui ne déplacent que le modèle de la
lueur depuis la pointe : en avant/arrière le long des flèches et à droite/gauche (horizontal),
de -100 à 100 unités (`debug.cfg` `glow_fwd=` / `glow_side=`, valeur + 100) ; particules et lumière
restent sur la pointe (réglée par les quatre premiers). Valeurs par défaut 0, à fixer après réglage
en jeu.
Tests : mod 0.2.94 compilé, traduit (237 fonctions, 90 hooks), vérifications OK, installé instances 1
et 3 ; `LauncherUI.java` compilé (javac, sortie temporaire). **Non testé en jeu.**

Réglage en jeu de la lueur des paquets élémentaires (0.2.95) : Turn 270, Tilt 40, Length to the tip 40, Height of the foot 15 (avant/côté 0), mis par défaut dans `bonus.h` et dans les champs du launcher. Mod 0.2.95 compilé, vérifications OK, installé instances 1 et 3 ; launcher compilé (javac).

## Chance des magies jaunes réglable aussi (0.2.96)

`bonus.h` : `bonus_drop_magic` (défaut `BONUS_DROP_MAGIC`, 4 pour mille), clé `debug.cfg`
`yellow_magic=` (`mod.c`). Les deux chances sont maintenant tirées séparément (cœur d'abord, puis
magie s'il n'y a pas de cœur) au lieu d'un seul tirage où la magie prenait les valeurs après celles du
cœur : à 0,4 % chacune l'écart est négligeable ; un cœur à 100 % n'empêche plus la magie quand le
cœur jaune est plein. Launcher : panneau « YELLOW LOOT DROP CHANCE », deux sliders (cœur, magie),
APPLY et DEFAULT.
Tests : mod 0.2.96 compilé, vérifications OK, installé instances 1 et 3 ; launcher compilé (javac).
**Non testé en jeu.**

## Flèches élémentaires : jamais de glace, nouveau générateur aléatoire (0.2.97)

Retour : les Darknuts ne donnent que du feu et de la lumière. Constaté (journal instance 1) : 15 paquets
élémentaires, 11 feu (type 1), 4 lumière (type 3), 0 glace (type 2). Le code tirait déjà le type sur 3
avec les flèches de lumière (`1 + bonus_random() % 3`). Simulation hors jeu du générateur xorshift32
(état xoré avec le pas à chaque appel) : environ 1/3 chacun, absence de glace non reproduite (0 sur 15
a environ 0,2 % de chances). Cause non prouvée ; le générateur était le point faible (tirages successifs
non indépendants).
**Corrigé (`bonus.h`)** : `bonus_random` passe en splitmix32 (compteur + nombre d'or + pas, mélange
complet) ; le type est tiré sur les bits de poids fort (`bonus_random_below`) : 1/3 chacun avec les
flèches de lumière, sinon 1/2 feu / glace. Simulation du nouveau tirage : 39 962 / 39 669 / 39 784.
Le journal compte maintenant feu, glace et lumière tombés depuis le lancement.
Tests : mod 0.2.97 compilé, vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## Crash à la création d'un Miniblin (0.2.98)

Constaté (`crash-20261019-185804.log`, instance 1, mod 0.2.96, adresses EU traduites en USA par
`tools/recomp/builds/eu.json`) : écriture à null + 0x340 dans le constructeur du modèle animé
(`mDoExt_McaMorf` 025E4F64) ← `useHeapInit` (02446A4C) ← `fopAcM_entrySolidHeap_` ← original de
`fopAcM_entrySolidHeap` appelé par `as_solid_heap` du mod ← **`daPt_Create` (02446E54, Miniblin,
`d_a_pt.cpp`, estimation 0x4B000)** ← `fopAc_Create`. Aucun refus de mémoire dans les dernières lignes.
Lu dans le code : pour un type d'acteur pas encore construit, `as_solid_heap` ne demandait que
256 Ko (`AS_HEAP_UNKNOWN`), moins que l'estimation du jeu (300 Ko), et laissait sinon le jeu essayer.
Déduit (cohérent avec la pile) : tas trop petit, une allocation du modèle échoue et le jeu écrit dedans.
**Corrigé (`actor_sync.h` `as_solid_heap`)** : type inconnu → au moins l'estimation du jeu + 25 % ;
sans cette place, le jeu n'essaie que si son estimation tient dans un des deux tas d'acteurs (le plus
grand) ; sinon l'acteur est refusé (journal « actor not made, no room ») au lieu du crash.
Tests : mod 0.2.98 compilé, vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## Launcher : bouton CONVERT A GAMECUBE SKIN

Demande : un bouton qui lance le convertisseur de skins GameCube, pour importer ensuite le résultat.
Diagnostic préalable des nouveaux skins `tools/skin/Minecraft` et `Peppino` : `make_gc_skin.py` et
`GcSkinConverter` les convertissent sans note ni erreur (sorties identiques), rendu hors jeu du `cl`
produit (scratchpad, Python pur) conforme ; seuls `cl`, `hands`, `katsura` sont convertis : les objets
que Minecraft remplace (une trentaine : épée, bouclier, arc, grappin...) restent ceux du jeu HD.
**Implémenté** :
- `GcSkinConverter` : la conversion (`convert`) est séparée de l'écriture ; `importGcSkin` (inchangé
  pour l'appelant) et nouveau `convertGcSkin(source, outDir, gameDir)` qui écrit `outDir/Link.szs`
  avec les noms du jeu (`Link.bfres`), qu'IMPORT A SKIN prend comme un mod de modèle HD
  (`SkinImporter`) et qu'on peut donner aux autres joueurs.
- `LauncherUI` : bouton « CONVERT A GAMECUBE SKIN » à côté d'IMPORT A SKIN : dossier GameCube (avec
  `Link.arc`), dossier du jeu (comme l'import), sortie `<dossier>_HD\Link.szs` à côté ; notes dans la
  console. Les deux threads (import, conversion) rattrapent aussi `OutOfMemoryError` (avant, le
  bouton restait désactivé sans message).
Tests : launcher compilé (javac) ; harnais (scratchpad) : Peppino converti puis `Peppino_HD` importé
= octet pour octet le skin de l'import direct. Bouton non cliqué (pas d'interface lancée).

## Créature du troll toujours en double chez la cible, flèches élémentaires sans stock (0.2.99)

**Troll en double** : constaté (journal instance 1, cible) : chaque Darknut envoyé donne deux
« creature summoned: Tn » de suite, donc deux `as_summon` par la cible elle-même : elle reçoit la
demande deux fois (le correctif 0.2.81 portait sur un autre chemin, les créatures renvoyées par
le serveur). Relu : le launcher n'écrit `shop.cfg` qu'une fois et bloque un nouvel achat tant que
le précédent n'a pas de réponse ; le mod de l'acheteur ne lit chaque id qu'une fois ;
`WWClient.receiveSummon` n'envoie qu'un paquet (garde `pack.getSource() != this`) ; la distribution
des paquets de MMB (`ServerClient.run` → `mail`) relue sans double appel prouvé. Cause exacte non
trouvée (aucun journal du serveur sur disque).
**Corrigé (`mod.c` `shop_received`)** : la même demande (créature et acheteur) reçue à nouveau en
moins de `SHOP_TWICE_STEPS` (30 pas, environ 1 s) est ignorée, journal « the same summon received
twice, made once ».

**Flèches élémentaires à 0** : relu (`d_a_arrow.cpp`) : `changeArrowType` (0205584C, flèche encochée)
et `changeArrowTypeNotReady` (0205578C, sans argument) font tourner normal → feu → glace → lumière →
normal en ne vérifiant que la magie (`mMagic` +0x14, `d_save.h`) et la possession (`mItems[12]`).
**Corrigé (`bonus.h`)** : les deux sont remplacées (aucune ne diffère EU/USA) : les types dont le stock
est à 0 sont sautés (le stock du type affiché est le compteur de la sauvegarde) ; pour la flèche
encochée, le jeu reçoit le type qui précède celui voulu (`mArrowType` +0x71D) puis fait son propre
changement ; échec de création → type et `l_keepType` remis.
Tests : mod 0.2.99 compilé, traduit (239 fonctions, 92 hooks), vérifications OK, installé instances 1
et 3. **Non testé en jeu.**

## Bouton RESET THIS ROOM (0.3.0)

Demande : un bouton dans l'onglet 02 du launcher pour recharger la room où l'on est, pour soi.
Vérifié (`d_stage_4.cpp`) : `dStage_restartRoom` (025C3D68, binding SDK, identique EU/USA) =
`setNextStage(scène courante, point -1, dSv_restart_c::mRestartRoom, couche -1, 0, mode)` puis le
paramètre de reprise ; `daPy_lk_c::startRestartRoom` (`d_a_player_main_03.cpp`) l'utilise après une
chute dans le vide avec le mode 5 et l'événement 0xC9. `dSv_restart_c` (`d_save.h`) : `mRestartRoom`
en +0 (save + 0x1148).
**Implémenté** : `mod.c` `room_reset_try` (clé `debug.cfg` `reset_room=1`, lue par `debug_step`) :
`dStage_restartRoom((salle de reprise & 0x3F) | 0xC9 << 24, 5)` ; Link revient à son point de reprise
(là où il est entré), les acteurs de la room sont refaits. Refusé pendant un événement, une
téléportation, sur le bateau ou en fantôme (journal « room not reset »). Launcher, onglet 02 :
panneau « ROOM », bouton RESET THIS ROOM.
Tests : mod 0.3.0 compilé, vérifications OK, installé instances 1 et 3 ; launcher compilé (javac).
**Non testé en jeu.**

## Élites maudits et jokers (0.3.1)

Demande : deux variantes d'élite, chacune deux fois moins fréquente qu'un élite, avec tout ce qu'a
un élite (vie ×3, dégâts ×1,5, vitesse, faculté, butin, explosion, taille).
- **Maudit** (crâne `assets/dead.png` au-dessus) : aura passive à 50 m (5000 unités) : l'invincibilité
  de Link après un coup est divisée par 3 ; ses coups maudissent Link comme un Bubble bleu ; tant
  qu'un maudit vit dans la room, pas de réanimation (message).
- **Joker** (œil `assets/eye.png`, dessiné par `tools/multiplayer/make_eye_icon.py`, art original) :
  vie ×6 (deux fois un élite) ; cible un joueur vivant de sa room au hasard toutes les 10 s (choix de
  l'utilisateur : la room, pas le stage) ; prend la couleur de palette la plus proche de la tunique
  de la cible ; seule la cible peut le blesser.

**Vérifié dans le décomp** :
- `daPy_py_c` (`d_a_player.h`) : `mDamageWaitTimer` s16 +0x3B0 (entre `mCutType` 0x3AC et
  `mQuakeTimer` 0x3B2 vérifiés), mis à 0x1E/0x14 quand Link est touché, décompté à chaque pas
  (`changeDamageProc` 023FA578) ; `mResetFlg0` +0x3C0 (vérifié), `daPyRFlg0_POISON_CURSE` 0x100000 ;
  l'execute de Link le transforme en `setDamageCurseEmitter` (023DCB54 : nuage `BUBBLECURSE00`,
  `damageEffectType` 1, 200 frames), comme un coup `spl` 3 (`setDamageEmitter`).
- `cc_at_check` (025192A8, `d_cc_uty.cpp`, binding SDK 2 arguments, identique EU/USA) : retire les PV
  des ennemis courants ; `CcAtInfo` : `mpObj` +0, `mpActor` +4, `mDamage` +8, `mbDead` +9.

**Implémenté** :
- `elite.h` : `kind` (élite, maudit, joker) ; tirage sur 200 (élite < 2c, maudit < 3c, joker < 4c, c =
  chance d'élite en %) ; octet synchronisé : maudit `0x20 | faculté << 3 | couleur 0-7`, joker
  `0x40 | faculté << 4 | cible 0-14` (15 : aucune) ; un ancien mod les ignore (ni 0x01 ni 0x80). La
  cible est sa place dans la liste des joueurs de la zone triée par uid de PlayerPuppet (`local_uid` et
  `remotes[]`, la même dans chaque jeu) ; `el_joker_step` (propriétaire : nouvelle cible toutes les
  10 s, ou tout de suite si elle meurt ou quitte la room) ; `el_at_check` (remplace `cc_at_check`) :
  sur un joker, un coup qui ne vient pas de sa cible rend les PV et annule `mDamage`/`mbDead`.
  Joueur derrière un coup (`el_hitter`) : Link local, puppet d'un joueur, copie d'arme (`creator`),
  arme locale, ou coup rejoué par le collisionneur du mod (`as_ghost` : le joueur est noté sur le slot
  de la cible, `replay_player`, dans `as_replay`) ; autre chose (ennemi, environnement) : refusé.
  `el_link_after` (après l'execute de Link) : une invincibilité nouvellement donnée est divisée par 3
  près d'un maudit vivant ; `el_plus_dmg` : un coup d'un maudit lève `POISON_CURSE` ; `el_cursed_in_room`.
- `death.h` : avec un maudit vivant dans la room, l'invite devient « A cursed elite is in the room: no
  revive » et A ne remplit pas la jauge.
- `mod.c` : `el_remote_room` (salle d'un joueur distant par l'entrée d'annuaire de même nom),
  `el_link_after` après `daPy_Execute_original`.
Limites connues (déduites) : un coup porté par l'arme tenue d'un ennemi maudit (acteur séparé) ne
maudit pas ; un ennemi qui ne passe pas par `cc_at_check` n'est pas protégé en joker ; sans annuaire,
la room des joueurs distants est inconnue (tous comptés comme dans la room).
Tests : mod 0.3.1 compilé (`-Werror`), traduit (244 fonctions, 93 hooks), vérifications OK, installé
instances 1 et 3. **Non testé en jeu.**

## Portes à cadenas : toute la room à moins de 15 m (0.3.2)

Demande : dans un donjon, une porte avec un cadenas ne s'ouvre que quand toutes les personnes de la
room sont à moins de 15 m de la porte.
**Vérifié (décomp)** : portes à verrou `daDoor10_c` (`d_a_door10.cpp`), `daDoor12_c` (`d_a_door12.cpp`),
`daKddoor_c` (`d_a_kddoor.cpp`) ; leur `setEventPrm` (02127080, 0212919C, 0219807C, un argument,
bindings SDK, identiques EU/USA) propose la porte à Link (`dEvtCnd_CANDOOR_e`, événement et outil
de carte du côté) quand il est dans sa zone et, si elle est verrouillée, qu'il a une petite clé
(`mKeyNum`) ou la grande clé. Verrou : `mbEnabled` en tête de `mKeyLock` (+0x424 door10, +0x400
door12) et de `mKey` (+0x42C kddoor), `WWHD_OFFSET` des headers.
**Implémenté (`door_lock.h`, branché dans `mod.c`)** : sur une porte verrouillée, tant qu'un joueur
vivant de la room du Link local est à plus de 15 m (1500 unités), l'original n'est pas appelé (la
porte ne propose rien, comme sans clé) ; compteur « présents/total » sur la porte (même dessin que
celui des blocs lourds) quand le Link local est à moins de 5 m. Joueurs de la room : le Link local et
les joueurs distants vivants dont la room d'annuaire est la sienne (`el_remote_room` ; sans annuaire,
tous ceux de la zone). Seul ou hors ligne : le jeu tel quel. Une fois la porte déverrouillée (verrou
retiré par le jeu), elle n'attend plus personne.
Tests : mod 0.3.2 compilé (`-Werror`), traduit (249 fonctions, 96 hooks), vérifications OK, installé
instances 1 et 3. **Non testé en jeu.**

## Variantes à 15 % des élites, grappin long, cochon doré (0.3.3)

**Variantes** (`elite.h`) : un élite (chance d'élite en %) devient maudit ou joker 15 fois sur 100
(`EL_VARIANT_PERCENT`), moitié chacun (7,5 % / 7,5 %) ; avant : chacun deux fois moins probable qu'un
élite, en plus des élites.

**Grappin long** (shop, Upgrades, 5 gelées bleues, 5 écussons, 1000 rubis, gardé pour toujours) :
vérifié : la portée de `daHookshot_c` est la constante 1500.0f lue par `lfs` à 0x10011724 dans la
visée (021789B4) et le retour du tir (02178DDC) (`d_a_hookshot.cpp`) ; code de l'EU cking.rpx balayé
(lis/lfs) : aucune autre lecture ; adresse identique USA (sous le premier pas de données d'`eu.json`) ;
`tools/recomp/ppc2c.py` traduit `lfs` en lecture mémoire. **Implémenté** : `bonus.h`
(`bonus_long_hookshot`, `hookshot=` dans `bonus.cfg`, `bonus_hookshot_step` écrit 3000.0f, seulement
si la constante vaut 1500 ou 3000), achat dans `mod.c` `shop_try_upgrade`, 5e champ de `bonus=` ;
launcher : carte active, « OWNED ».

**Cochon doré** (shop, Utilities, 5 plumes dorées, 100 rubis, un à la fois, `pig=` dans `bonus.cfg`) :
flèche bas (VPAD 0x0100) mains libres (procs WAIT/FREE_WAIT/MOVE/ATN_MOVE, hors événement et bateau)
→ cochon (`kb_class`, processus 0xDC, paramètres 0xFFFFFF00) créé devant Link puis porté par
`procGrabReady_init` (`mpAttnActorA`, comme une bombe sortie) ; lancé ou posé → 30 s de charge puis
supprimé. Vérifié (`d_a_kb*.cpp`, `d_a_kb.h`) : modes `m41E` (+0x766) 0 libre, 1 porté, 3 attaque ;
`attack_move` (pas `m420` +0x768 : 0x23 puis 0x24) fonce sur `dComIfGp_getPlayer(0)` et rebondit en
le touchant ; sphère `mSph` (+0xBD4) source `co_sph_src` 101B7FFC lue dans le RPX EU : type 0x800,
dégâts 4, `AtSPrm` 0xD ; TG du Bokoblin 0xFF1DFEFF / `TgSPrm` 3 ; `VsEnemy` 2, `VsPlayer` 4. **Implémenté
(`golden_pig.h`)** : `daKb_Execute` (02190CE0) remplacé pour ce cochon : santé gardée à 10 (sinon
`ALL_ANGER`), `VsEnemy` allumé et `VsPlayer` éteint ; lâché, il passe en attaque et son execute tourne
avec l'ennemi vivant le plus proche (≤ 30 m, `as_groups` et `as_enemy_copies`) comme joueur 0 et Link
(play + 0x5B2C / 0x5B34, remis après) ; vitesse de charge 32 (jeu : 15) ; doré comme les cœurs jaunes
(`elite.h` `el_draw_begin`). Local : les autres joueurs ne voient pas le cochon. Launcher : carte active.
Déduit, non testé : comportement de `procGrabReady_init` sur un acteur créé à l'instant ; flèche bas
peut avoir une autre fonction du jeu (non vérifié).
Tests : mod 0.3.3 compilé (`-Werror`), traduit (250 fonctions, 97 hooks), vérifications OK, installé
instances 1 et 3 ; launcher compilé (javac). **Non testé en jeu.**

## Cochon doré : crash au lancer, pas doré (0.3.4)

**Crash** (`crash-20261019-210919.log`, 0.3.3, juste après « golden pig thrown ») : saut indirect vers
0x42900000 (lr 0219468C) dans `speed_pos_set` (02194654) ← `daKb_Execute` remplacé. Vérifié
(`d_a_kb.cpp`) : `speed_pos_set` appelle `daPy_getGrabMissActor(dComIfGp_getPlayer(0))`, une
fonction virtuelle de Link ; le mod mettait l'ennemi en joueur 0 pendant tout l'execute du cochon, donc
l'appel partait dans les données de l'ennemi. **Corrigé (`golden_pig.h`)** : la substitution n'entoure
plus que `attack_move` (021947AC, remplacé, un argument, identique EU/USA), qui ne lit le joueur 0 que
pour sa position et pour reconnaître l'acteur touché (relu : aucun appel virtuel au joueur).

**Pas doré** : constaté dans le journal : « elite colour arena full ». Lu dans `elite.h` : un jeu de
textures recolorées ne remplace qu'un jeu inutilisé depuis 3 s, et un échec attend 20 s : avec des
élites à l'écran, le cochon gardait son rose. **Corrigé** : `el_paint_priority` (le cochon doré) : un jeu
non dessiné pendant ce pas lui fait place, sans attente après un échec ; journal « golden pig: gold
textures in its model N » ou « no colour set now » (pour voir si sa couleur est dans ses textures).
Tests : mod 0.3.4 compilé, traduit (251 fonctions, 98 hooks), vérifications OK, installé instances 1 et
3. **Non testé en jeu.**

## Crash des modèles partagés de Link, joker intouchable, icône du cochon sur la croix (0.3.5)

**Crash** (`crash-20261019-211908.log`, instance 3, 0.3.4, juste après « revive sent to Minecraft ») :
saut vers 0 dans `J3DModel::viewCalc` (027F55FC) ← `mDoExt_drawModel` ← `mDoExt_modelEntryDL`
(025E2E5C, `draw_entry_dl` du mod) ← `daPy_lk_c::entryDLSetLight` (023D8DF0) ← `daPy_lk_c::draw`
(023D9820). Désassemblé (EU cking.rpx) : `viewCalc` lit les données de modèle (+0xAC) puis leur
animation +0x44 et appelle sa vtable (+0x34, entrée +0x3C) : nulle. Vérifié (`d_a_player_main_01.cpp`) :
à sa création chaque Link lie ses objets d'animation (this + 0x44D4 … 0x4794 et ceux de ses objets)
aux données de modèle « Link » partagées (bombe 0x3C, colonne d'eau 0x42, flèches 0x37/0x38, 0x40,
0x41…) par `initBrkAnm` / `initBtkAnm` (023D4C78 / 023D4D64, `entry` : +0x44 brk, +0x48 btk,
`ArrowLightModelData`). Déduit : le corps du joueur mort (un puppet) avait remplacé ces liens ; supprimé à
la réanimation, il laissait des pointeurs libérés. **Corrigé (`puppet.h`)** : pour un puppet, ces deux
fonctions (identiques EU/USA) remettent les mots +0x3C à +0x48 des données de modèle comme avant :
les modèles partagés gardent les animations du Link local.

**Joker** : demandé : qu'il ne prenne plus du tout les coups des autres (pas seulement sans perte de
vie). **Implémenté** : `actor_sync.h` `as_hit` (remplace `cCcSState::SetAtTgCommonHitInf` 0200CFA8)
appelle d'abord `el_joker_refuses` (`elite.h`) : sur un joker, un coup qui ne vient pas de sa cible
n'est inscrit sur aucun des deux colliders (ni réaction, ni dégâts, l'arme traverse). Joueur d'un coup
rejoué : `as_replay_player_now`, posé par `as_replay` autour de `wwhd_set_at_tg`. `el_at_check` reste
en filet.

**Icône du cochon** : lu hors ligne (`Shortcut_00.szs` du pack 2D) : la croix du HUD en haut à gauche
est l'image `CrossKeyBtn_00` (58 × 58) ; l'icône de la baguette `P_Baton_00` est au-dessus, en (8, 39),
0,6 × 118 ; rien sur la flèche bas. **Implémenté** : `bonus.h` `bonus_cross_draw` (depuis
`hdParts_c::draw` 026F8B54, seulement avec un cochon en réserve) trouve `CrossKeyBtn_00` et note le
pas de son dessin ; `golden_pig.h` `gp_draw` dessine `assets/pig.png` (art original de
`tools/multiplayer/make_pig_icon.py`, 96 × 96) en (8, −39) de son centre, même taille que la baguette,
avec l'échelle et l'alpha de la croix, tant qu'un cochon attend d'être appelé et que le HUD est dessiné.
Tests : mod 0.3.5 compilé (`-Werror`), traduit (254 fonctions, 100 hooks), vérifications OK, installé
instances 1 et 3. **Non testé en jeu.**

## Crash dans J3DModel::viewCalc depuis le dessin d'un Link, garde-fou complet (0.3.6)

Constaté (`crash-20261019-213306.log`, instance 3, 0.3.5) : même chemin que -211908 (`daPy_lk_c::draw`
→ `entryDLSetLight` → `mDoExt_modelEntryDL` → `drawModel` → `viewCalc`), lecture à l'adresse 0x2A3C
dans la boucle des matériaux, juste après « puppet requested for uid 1 » (le joueur Minecraft ; skin
non installé sur l'instance 3 : un Link normal). Désassemblé (EU cking.rpx) : `viewCalc` lit les données
de modèle (+0xAC), leurs matériaux (+0x10, 0x39C chacun, nombre +0xC), les entrées par matériau du
modèle (+0x34, 0x3C chacune), et cinq animations des données de modèle (+0x38, +0x3C, +0x40, +0x44,
+0x48) dont il appelle la vtable (+0x34, entrée +0x3C). Cause exacte non trouvée : un modèle ou une
animation déjà libérés, tenus par un Link.
**Implémenté (`puppet.h`)** : `draw_model_ok` (sur `mDoExt_modelEntryDL` / `modelUpdateDL`, tous les
modèles) vérifie ces pointeurs (mémoire du jeu, alignés) et que la fonction appelée est dans le code du
jeu ; sinon le modèle n'est pas dessiné et le journal donne le modèle, le mot fautif, sa valeur, le Link
et son skin (« a model not fit to draw was left out »). La sauvegarde des puppets de 0.3.5 couvre
maintenant les cinq mots (+0x38 à +0x48).
Tests : mod 0.3.6 compilé, traduit (255 fonctions, 100 hooks), vérifications OK, installé instances 1 et
3. **Non testé en jeu.**

## Menu Start bloqué pour le fantôme (0.3.7)

Retour : en fantôme, le menu Start ne s'ouvre plus. Le code du menu HD n'est pas dans le décomp ; le
code de l'EU cking.rpx a été balayé pour les lectures de `play.mItemNowLife` (play + 0x5BAC, la vie
affichée par le compteur) : 9 lectures, dont deux dans l'interface HD (fonctions USA 026A7170 et
0270D710, sans nom, identiques EU/USA). Désassemblé : 026A7170 est un aiguillage de la machine à états
de l'interface (code lu dans 101F5008) ; une branche n'agit que si le joueur 0 n'a pas `mNoResetFlg1`
& 0x200, sans événement (+0x5292), sans `+0x5CD8 & 0x10`, et avec la vie affichée non nulle. Déduit : le
fantôme (vie 0) ne peut donc pas ouvrir le menu.
**Implémenté (`death.h`)** : crochets d'entrée et de retour sur ces deux fonctions (arguments non
touchés) : pour le Link local fantôme, la vie affichée vaut 1 pendant leur appel, puis reprend sa
valeur ; le reste du jeu voit toujours 0 (pas de sortie, pas d'ordre d'événement). Risque : sauvegarder
depuis le menu en fantôme enregistre une vie à 0 (non vérifié ce que le jeu en fait au chargement).
Tests : mod 0.3.7 compilé, traduit (259 fonctions, 104 hooks), vérifications OK, installé instances 1 et
3. **Non testé en jeu.**

## Mod refusé par le jeu : crochet hors fonction (0.3.8)

Constaté (journaux des instances 1 et 3) : « [runtime error] hook target 026A7874 is not a game
function » (EU de USA 026A7170) : le mod n'est pas chargé ; « Expected three-part version » à
l'ouverture de l'overlay ensuite (manifestes corrects : conséquence probable du mod refusé). Cause : en
0.3.7 le début des fonctions du menu a été pris sur `stwu r1` au lieu du `mflr r0` qui le précède
(désassemblé : EU 026A7870 / 0270DFC8). Vérifié : l'exécutable du runtime nomme ses fonctions
`f_<ADRESSE>` : `f_026A716C` et `f_0270D70C` y sont, pas 026A7170 / 0270D710.
**Corrigé** : `death.h` : crochets du menu sur USA 026A716C et 0270D70C. `tools/multiplayer/build_mod.ps1` :
nouveau contrôle « every hook target is a game function » (chaque cible de crochet doit avoir son
`f_<ADRESSE>` dans `build\windows-msys2-d\wwhd.exe`, sinon celui de la distribution), le build échoue
sinon.
Tests : mod 0.3.8 compilé, traduit (259 fonctions, 104 hooks), toutes les vérifications OK dont la
nouvelle, installé instances 1 et 3. **Non testé en jeu.**

## Garde-fou du tas d'objets de Link (0.3.9)

Constaté (`crash-20261019-220620/-220621.log`, instance 1, skin garfield) : « J3DPacket.cpp
mEntryPtr == (0) » dans `setAnimeHeap` ← `setSmallFanModel` ← `commonProcInit` ← `procFall_init` (fin du
vol à la feuille Mojo) ← execute du Link local (fonction du mod repérée par ses appels : le
remplacement de `daPy_Execute`, qui n'appelle que l'execute du jeu). Même assertion qu'en 0.2.18
(puppet) : un tas d'objets vidé alors qu'un modèle qu'il contient est encore dans les listes de
dessin. Vérifié (`d_a_player_main.cpp`) : `setItemHeap` (023DDF64) change de tas une fois par pas
(`mResetFlg0` 0x4000, remis à 0 en tête d'execute, seule sortie anticipée : `setGetDemo`) ;
`mCurrItemHeapIdx` +0x68D7 (SDK). Cause non trouvée. **Implémenté (`puppet.h`)** : `setItemHeap`
remplacé pour le Link local : si le tas allait être revidé sans changement alors que son dernier
modèle date d'un pas précédent, le changement est fait ; journal (« item heap: ... »).
Aucun nouveau crash depuis (journaux relus le 20 octobre).

## Puppets qui clignotent ou brûlent sans fin, culling à 100 m (0.3.10)

**Clignotement** : vérifié (`d_a_player_main_01.cpp` draw) : le voile rouge d'un Link touché est
dessiné tant que son `mDamageWaitTimer` (+0x3B0) > 0 ; seul `changeDamageProc` (execute) le décompte,
et un puppet ne le lance pas. **Corrigé (`puppet.h`)** : le puppet le décompte à chaque execute.
**Flammes** : lu dans `particle_sync.h` : une particule suivie n'est arrêtée chez son créateur que si son
rappel change, si elle a StopEmit/EnableDelete ou `mMaxFrame` < 0 ; déduit : une flamme qui finit
d'elle-même peut garder ces valeurs dans sa mémoire libérée, sa position repart sans fin et la copie
brûle toujours. **Corrigé** : arrêt d'une particule suivie après `PS_FOLLOW_MAX_STEPS` (300 pas, 10 s)
chez son créateur (arrêt envoyé) et d'une copie après `PS_COPY_MAX_STEPS` (450).
**Culling** (`puppet.h` `cull_far`, `CULL_DISTANCE` 10000 unités) : les Links et bateaux des autres
joueurs (puppets) et les ennemis (`group` 2, `actor_sync.h` `as_draw`) à plus de 100 m du joueur 0 ne
sont pas dessinés (leur execute continue) ; pas pendant un événement.
**Lag (journaux, `[interp] … logic steps/s`)** : instance 3, 60 mesures : médiane 29,8 pas/s, minimum
24,9 (cible 30) ; une « in-between pass probe took 263.8 ms (a hitch?) » : des à-coups ponctuels plutôt
qu'une lenteur continue. Sources possibles dans le mod, non mesurées : lectures/écritures de fichiers
toutes les 0,5 s (`teleport.cfg`, `shop.cfg`, `debug.cfg`, `connect.cfg`, `players.txt`) et 1 s
(`launcher.cfg`), recoloration des textures d'élites (une texture copiée par pas), création des puppets
(un Link complet) et des copies d'ennemis (multiplicateur), parcours de tables (512 acteurs suivis par
pas).
Tests : mod 0.3.10 compilé, traduit (260 fonctions, 105 hooks), vérifications OK, installé instances 1
et 3. **Non testé en jeu.**

## Interrupteurs de sol, grappin sur les joueurs, porter et lancer les joueurs (0.3.11)

**Interrupteurs de sol (`heavy.h` `hv_switch_ride`)** : vérifié (`d_a_obj_swpush.cpp`, binding SDK
`wwhd_rideCB_0239705C` à trois arguments, identique EU/USA, présent dans l'exécutable) : le rappel de
pose `rideCB` (0239705C) met `mRidingMode` (+0x480) quand un acteur est sur le dessus, l'interrupteur
descend ensuite. **Implémenté** : remplacé pour le Link local seulement ; il ne compte que si assez de
joueurs de la salle sont dessus (même règle que pousser/soulever : tous à deux, deux tiers à partir de
trois ; le Link local et les joueurs distants vivants à moins de 140 unités du centre et 120 de
hauteur) ; le compteur s'affiche au-dessus. Les autres acteurs (pot, bloc) pressent comme dans le jeu.

**Grappin sur les joueurs (`carry.h` `cy_hook_shot`)** : vérifié (`d_a_hookshot.cpp`,
`d_a_hookshot.h` avec `WWHD_OFFSET`) : `procShot` (02178DDC, un argument) tire Link vers un acteur
touché portant le statut 0x200000 (id +0xD774, décalage +0xD768, m2A4 +0xD500, angle +0xD510, mode 3,
ptmf +0xD7A8 = `procPlayerPull` 02179888, qui suit l'acteur). Un puppet n'a pas de collider :
**implémenté** : avant `procShot`, le trajet de la pointe sur le pas (mMoveVec ×15, 8 points) est testé
contre chaque puppet de joueur vivant (cylindre 45 × 150) ; un contact prend la même voie que le jeu
(sans le son ni la vibration). Seulement le grappin du Link local (`as_find` exclut les copies).
Déduit, non testé : un mur entre la pointe et le puppet sur ce même pas (≤ 105 unités) est ignoré.

**Porter / lancer (`carry.h`)** : vérifié (`d_a_player_main_02/04/07.cpp`) : `procGrabReady_init`
(023ED0F0) prend `mpAttnActorA` (+0x65B8) dans `mActorKeepGrab` (+0x65A0) ; `setGrabItemPos`
(0240A41C) met l'acteur porté entre les mains ; `procGrabThrow` (0242E010) donne à l'objet lancé
speed.y 34 / speedF 19 puis `freeGrabItem` (023DCF8C) ; `procAutoJump_init` (023F7958) fait le saut
d'un rebord (vitesses tirées de speedF, plafonné à 17). Procs : WAIT 4, MOVE 6, ATN_MOVE 7, WAIT_TURN
0x17, GRAB_THROW 0x71. **Implémenté** :
- A devant un puppet de joueur (1 m, cône ±67°, debout/marche, mains libres, hors événement, pas
  fantôme) : soulève le joueur à la manière du jeu (statuts 0x8010000 du puppet retirés avant) ;
  porter, poser et lancer (A) sont les procs du jeu ;
- nouveau paquet d'extension `CLIENT_PLAYER_CARRY` / `SERVER_PLAYER_CARRY` (`mp_protocol.h`
  `mp_write_carry`/`mp_read_carry` : VLQ uid ActorPlayerSync visé, VLQ uid de l'expéditeur, u8 action
  0 soulever / 1 poser / 2 lancer / 3 sauter, u16 orientation) ; le serveur (`WWClient.receiveCarry`,
  `ActorFilter.PacketTypesList.SERVER_PLAYER_CARRY` ajouté en fin d'énumération) le renvoie tel quel à
  tous les autres clients ; chacun note qui porte qui (pas de double prise) ;
- le joueur porté : son Link est placé après son execute entre les mains du puppet de son porteur
  (matrices 8 et 12 du modèle, -20 unités), vitesses à 0, orienté comme lui ; lancé : saut en avant
  (`procAutoJump_init` dans l'orientation du lanceur, puis speed.y 34, vitesse 19) ; A pendant qu'il est
  porté : petit saut (28 / 6) et le jeu du porteur lâche (`freeGrabItem`) ; porteur parti, mort ou
  événement : libéré ;
- tour de Links : un joueur porté garde ce qu'il porte (déduit : `procFall` garde la prise ; non
  vérifié en jeu) ; il faut soulever d'abord celui du haut.
Déduit, non testé : le Link porté est en chute (`procFall`) pendant le portage (animation de chute) ;
une chute longue après relâchement peut donner l'atterrissage lourd du jeu.

Tests : mod 0.3.11 compilé (`-Werror`), traduit (267 fonctions, 107 hooks), vérifications OK (imports,
fonctions différentes EU, cibles de hooks), installé instances 1 et 3 ; `mp_protocol_test` OK (nouveau
`test_carry` : octets, troncature, action invalide, expéditeur = cible) ; `WWClient.java` et
`ActorFilter.java` compilés par javac sans erreur (les erreurs restantes du lot viennent des
bibliothèques tierces absentes du classpath local : JavaFX, LWJGL, JNA, GraalVM). **Le serveur doit
être recompilé (IDE) pour le portage. Non testé en jeu.**

## Culling 400 m des joueurs, butin jaune indépendant, reset de room, PvP (0.3.12)

**Culling** (`puppet.h` `cull_beyond`) : les Links et bateaux des autres joueurs ne sont plus dessinés
au-delà de 400 m (`CULL_PLAYER_DISTANCE` 40000, `cull_player_far`) ; les ennemis restent à 100 m
(`cull_far`). La voile d'un bateau distant (`daGrid_c`, `ship_grid_draw` 02162F68) suit son bateau ;
canon, grue et barre sont dessinés par le draw du bateau (déjà coupé). Le grappin sur joueur
(`carry.h`) suit la même distance.

**Butin jaune** (`bonus.h` `bonus_yellow_roll`) : deux tirages indépendants (cœur, magie) ; les deux
peuvent tomber du même butin (avant : un cœur tiré empêchait la magie).

**Launcher** : « RESET THIS ROOM » est dans le panneau GAME OPTIONS de l'onglet 02 ; le panneau ROOM
est supprimé. Nouvelle case « PvP » (launcher.cfg `pvp=0/1`, `launcher_cfg.h`, testé).

**PvP (`pvp.h`)** — PvP actif seulement entre deux joueurs qui l'ont tous deux coché (bit
`MP_EXTRA_PVP` 0x08 des extras d'ActorPlayerSync ; le serveur transmet déjà les extras avec le masque
0x7F : aucun changement serveur), vivants.
- Verrouillage : vérifié (`d_attention.cpp`) : l'attention parcourt tous les acteurs et garde ceux dont
  `attention_info.flags` (+0x39C) répond au masque du joueur, à l'entrée `distances[type]` (+0x388) de
  `dist_table` (101D3CC0), mesurée depuis `attention_info.position` (+0x390). Lu dans le RPX EU :
  entrée 3 = 375 unités, ±500 (profil d'ennemi). **Implémenté** : après l'execute d'un puppet d'un
  joueur PvP : flags 4 (`LOCKON_BATTLE`), distance de combat 3, position à +100 ; sinon flags 0.
- Coups : décidés chez la victime. Lu dans le RPX EU : `l_at_cps_src` (101CEDA8) type 2, dégâts 1,
  AtSPrm 0xB ; la TG du corps de Link `l_cyl_src` (101CECDC) type 0xFF1DFEFF (contient 2), TgSPrm 5.
  **Implémenté** (`sword_swing.h`) : les capsules d'épée déjà rejouées pour le décor sont copiées, avec
  AtSPrm Set|VsPlayer (5, ni ennemi ni objet), propriétaire le puppet de l'attaquant, et données au
  dCcS (`dCcS::Set` 0200E240, play+0x26A4) : le jeu applique lui-même dégâts, recul, invincibilité,
  bouclier. Déduit, non testé : dégâts d'1 (un quart de cœur) par coup, ceux de la source de l'épée ;
  le type d'attaque envoyé (épée, attaque tournoyante) doit être dans le masque TG de Link.

Tests : mod 0.3.12 compilé (`-Werror`), traduit (267 fonctions, 107 hooks), vérifications OK, installé
instances 1 et 3 ; `mp_protocol_test` OK (clé `pvp`) ; `LauncherUI.java` compilé (javac + JavaFX du
dépôt). **Non testé en jeu.**

## Retours de la 0.3.12 → 0.3.13

Journaux relus (instances 1 et 3, 20 octobre) : aucun « lifting a player » ni « hookshot caught » ;
le portage a été fait depuis l'autre PC ; aucun « another player lifts us » reçu : déduit, le
serveur n'avait pas `CLIENT_PLAYER_CARRY` (non recompilé).
- **Grappin** : `cy_hook_shot` excluait tout grappin suivi par `actor_sync` ; or celui du Link local
  l'est aussi (il est envoyé). **Corrigé** : seules les copies (`replica`) sont exclues.
- **Porté = pseudo seul** : chez le porteur, `setGrabItemPos` déplace `current.pos` du puppet (le
  pseudo suit) mais ses matrices sont calculées dans `puppet_execute` depuis la position réseau.
  **Corrigé** (`carry.h` `cy_place_puppet`, appelé dans `puppet_execute`) : le puppet porté est placé
  entre les mains de son porteur (le Link local, ou le puppet du porteur noté par `cy_pairs`), orienté
  comme lui, avant le calcul des matrices.
- **Interrupteurs pour tous** (`heavy.h`) : les interrupteurs sont repérés par leur execute
  (`daObjSwpush::Act_c::Mthd_Execute` 02398014, hook d'entrée, binding SDK un argument) ; un
  interrupteur avec au moins un joueur dessus (local ou distant vivant) montre son compteur dans
  chaque jeu (même compte que `hv_switch_ride`). Pousser/soulever : inchangé (0.2.91).
- **Cœurs jaunes décalés** : déduit : la matrice d'un emplacement de cœur au-delà des réceptacles
  n'est pas mise à jour par le jeu (périmée). **Corrigé** (`bonus.h` `bonus_slot_center`) : au-delà
  des réceptacles, position extrapolée des emplacements affichés (10 par ligne ; pas entre 0 et 1 ;
  entre lignes, 0→10 si affiché, sinon le même pas vers le bas).
- **Textures cassées (vent, particules, ciel)** : pas de trace dans les journaux. Déduit : une copie
  de particule suivie d'un autre joueur reçoit position/rotation à chaque mise à jour puis sa fin ;
  si le jeu a libéré l'émetteur et l'a donné à un autre effet, on déformait cet effet. **Corrigé**
  (`particle_sync.h` `ps_copy_alive`) : rien n'est écrit dans un émetteur qui n'est plus Immortal sans
  callback (nos marques) ; la copie est oubliée. Non confirmé comme cause.
- **PvP en mutateur serveur** : case retirée de GAME OPTIONS (et clé `pvp` du launcher.cfg retirée) ;
  carte PVP de l'onglet serveur active (préférence `pvp`, `-Dww.pvp`, commande `mutators N r s e p`) ;
  `ActorFilter` : entrée d'init `#pvp=1`, octet final de `SERVER_MUTATORS` ; mod : `session.pvp`
  (`MP_PVP_ENTRY`, `mp_read_mutators_ex` + `pvp`, testé dans `mp_protocol_test`). Bit `MP_EXTRA_PVP`
  retiré.
- **Invoquer un acteur** (Debug du launcher) : liste filtrable des 822 noms de
  `resources/WWServer/actorlist.txt`, paramètres hexadécimaux ; debug.cfg `spawn=`/`spawn_params=` ;
  mod : `fopAcM_create_str` (025D58B4, 7 arguments, vérifié dans `f_op_actor_mng.cpp` ; -1 si nom
  inconnu) à 2 m devant Link, dans sa salle, face à lui ; local à ce jeu.
Tests : mod 0.3.13 compilé (`-Werror`), traduit (256 fonctions, 108 hooks), vérifications OK, installé
instances 1 et 3 ; `mp_protocol_test` OK ; `LauncherUI`, `ServerProcess`, `ActorFilter`, `WWClient`
compilés sans erreur (javac + JavaFX ; seule erreur restante : `AssimpLoader`, LWJGL absent).
**Le serveur doit être recompilé (portage, PvP). Non testé en jeu.**

## 0.3.14

Journaux (instances 1 et 3, 20 octobre 11:57, mod 0.3.13) : « lifting a player » ×3, « the hookshot
caught a player » ×2 (instance 1) ; « another player lifts us » ×3, « thrown by another player » ×2
(instance 3) : le paquet de portage et le grappin fonctionnent.
- **Porté immobile sur son propre écran** : déduit : placé seulement après son execute, quand le jeu
  avait déjà calculé ses matrices, et son execute suivant le ramenait au sol. **Corrigé**
  (`carry.h` `cy_before`, `cy_place_local`) : placé entre les mains du puppet porteur avant son execute
  et encore après ; `cy_step` passe avant l'envoi de la position.
- **Cœur jaune une case trop loin** (capture : trois cœurs, le jaune une case et demie plus loin) :
  déduit : position lue sur le remplissage (`P_Heart_00`) d'un cœur vide ou partiel, caché donc
  périmé. **Corrigé** (`bonus.h` `bonus_slot_center`) : toute case est calculée depuis les fonds
  (`P_Base_00`) des cases 0, 1 (et 10) ; journal « yellow hearts: containers … » (12 au plus) des
  entrées du placement pour vérifier.
- **Invocation synchronisée** : un nom de la liste d'acteurs du serveur est créé comme les créatures
  de la boutique (`as_summon` : bloc marqué, envoyé par le serveur, créé une fois chez les autres) ;
  un autre nom reste local (`fopAcM_create_str`).
Tests : mod 0.3.14 compilé (`-Werror`), traduit (258 fonctions, 108 hooks), vérifications OK, installé
instances 1 et 3. **Non testé en jeu.**

## 0.3.15

Journaux (instance 1, 12:23, 0.3.14) : aucun portage sur les instances 1 et 3 (le porté était sur
l'autre PC, version de son mod inconnue). Placement du cœur jaune : « containers 20, life 13, first
yellow slot 4 », cases 0-3 à x 56/84/113/142 (fond et remplissage identiques), jaune à 171 : la
sauvegarde dit 20 réceptacles mais le HUD en montre trois ; la vie 13 dépasse ce que le HUD affiche.
- **Cœur jaune** (`bonus.h`) : réceptacles = fonds visibles dans le HUD quand il en montre moins que la
  sauvegarde ; vie bornée à ces réceptacles. Implémenté, non testé en jeu.
- **Attaque tourbillon / tornade** (`particle_sync.h`) : vérifié (`d_a_player_main_04.cpp`
  setCollision, `d_particle_callbacks.cpp` 025A9C3C, `d_particle_local.h`) : les émetteurs
  ROUNDATTACK00/01/02 (groupe 1, 0x25/0x26/0x28) ne font rien seuls ; leur callback
  `dPa_cutTurnEcallBack_c` crée une particule par position donnée chaque frame (traînée de l'épée).
  **Implémenté** : leur copie reçoit son propre objet callback (vtable lue sur `m331C` +0x6730 du Link
  local), nourri chaque pas de 5 positions de l'ancienne à la nouvelle pointe d'épée du puppet (main
  droite + 70 unités vers l'extérieur) ; fin par le drapeau +5 (comme `end`). Déduit : position de la
  pointe approchée. Non testé en jeu.
- **Porté** (`carry.h`) : après le placement post-execute, `setWorldMatrix` (023FF6A0) et le calcul du
  modèle sont refaits ; journal de diagnostic des 4 premières frames d'un portage (proc, y donné, y
  après execute, décalage x) et « its puppet is not here » si le porteur n'a pas de puppet.
Tests : mod 0.3.15 compilé (`-Werror`), traduit (258 fonctions, 108 hooks), vérifications OK, installé
instances 1 et 3. **Non testé en jeu.**

## 0.3.16

Journal (instance 1, 12:37, 0.3.15, le joueur porté) : à chaque frame « proc after execute 4 », y donné
174-194, y après execute 168 (le sol), x déplacé de 8-20 : l'execute le remettait au sol.
- **Portage** : vérifié (`d_a_player_main_04.cpp` execute) : après `posMove`, `mAcch.CrrPos` corrige au
  sol ; avec `mModeFlg` (+0x6A70) `ModeFlg_CAUGHT` (0x10000000, posé par aucun code de Link ; aussi
  testé pour corde, échelle, bateau, grappin), la position d'après `posMove` est gardée. **Implémenté**
  (`carry.h`) : placé avant son execute avec CAUGHT, retiré à la libération (`cy_free_local`) ; plus de
  replacement ni recalcul de matrices après l'execute (équipement, caméra et saut de lancer partent de
  la bonne place). Hauteur : au moins 110 au-dessus des pieds du porteur (ses mains de puppet étaient
  à ~40). Déduit, non testé.
- **Icône du cochon** : 50 au lieu de 70,8, centre décalé de 35,4 vers la gauche (vu : son bord gauche
  à la place de son centre).
- **Flèches élémentaires** (`bonus.h` `bonus_elemental_cap`) : 10 par type avec le petit carquois
  (maximum de la sauvegarde 30), 20 (60), 30 (99) : stock, ramassage et compte affiché en visée.
Tests : mod 0.3.16 compilé (`-Werror`), traduit (259 fonctions, 108 hooks), vérifications OK, installé
instances 1 et 3. **Non testé en jeu.**

## Publication 0.3.16 (20 octobre)

- Git : `ZeldaWWHDRecomp` commit `a07f562` poussé sur `origin/main` (fichiers du mod modifiés dans
  la session + `runtime/tools/mp_protocol_test.c`). `WindWakerClient` non touché (modifications locales
  qui ne viennent pas de cette session). `WindWakerServer` (serveur, launcher) n'est dans aucun dépôt.
- `dist/` : `multiplayer-connection-0.3.16.wwhdmod` (manifest, mod.elf, assets ; remplace 0.2.63),
  `WWHD-Online-Launcher/` : `WWHD-Online-Launcher.jar` (toutes les sources mmb, MMBServerCommon,
  windwakerserver compilées en --release 17 ; JavaFX 21.0.1 win, JNA 5.13 ; images/CSS du launcher ;
  `WWServer/actorlist.txt` ; main `com.kirbymimi.windwakerserver.Main`), `Resources/` (ressources du
  serveur + `MMBSystem/` de mmb : `FileSystemJar` de mmb ne sait pas lire ses ressources dans un jar,
  mmb lit aussi `Resources/` à côté du jar), `User/` vide, `Lancer-le-launcher.bat`. GraalVM/Python,
  LWJGL, JXInput non embarqués (non chargés par la configuration du serveur).
- Tests : serveur lancé depuis le jar : configuration chargée jusqu'à l'ouverture du port (refusée :
  10014 déjà pris par un serveur en cours) ; launcher ouvert depuis le jar (JavaFX démarre,
  avertissements seulement). Convertisseur de skin non testé depuis le jar (classes présentes).
- Launcher pour les PC avec seulement Java 8 (décision de l'utilisateur, 20 octobre) : pas de jar
  Java 8 (JavaFX 21 exige Java 17 ; le JRE Oracle 8u451 n'a plus JavaFX ; 435 erreurs de syntaxe Java
  17 en --release 8). Runtime Java 21 embarqué : `dist/WWHD-Online-Launcher/runtime` (jlink du JDK
  Oracle 21.0.6 : java.base, java.desktop, java.logging, java.prefs, java.scripting, jdk.jfr,
  jdk.unsupported (jdeps) + java.xml, jdk.charsets, jdk.crypto.ec, jdk.zipfs ; 47 Mo) ;
  `Lancer-le-launcher.bat` l'utilise (sinon le javaw du PC). Le serveur lancé par le launcher prend
  le même runtime (`java.home`). Testé : serveur jusqu'à l'écoute du port 10014, launcher ouvert.

## 0.3.17 – 0.3.18 (20 octobre après-midi)

**Progression partagée** : une sauvegarde vierge reçoit tout ce que la sauvegarde du serveur a
(`SaveServer` merge, `User/SaveServer/save.ktml`). Launcher : bouton **NEW GAME** (onglet serveur,
serveur arrêté) : `save.ktml` et `users/` déplacés dans `User/SaveServer/backup-<date>` ; le prochain
joueur qui se connecte fixe la nouvelle progression.

**Pyjama** (0.3.17) : `outfit.h` `outfit_worn_casual` : la tenue réellement dans la table de textures
(l'objet HD de `getRes("Link", 0x71)`, comme `playerInit`) ; le drapeau `CASUAL` du Link local est
remis d'accord avec les textures. 0.3.18 : `players.txt` `hero_clothes=0/1` ; le launcher verrouille
la case pyjama cochée tant que la tenue n'est pas reçue, la décoche une fois à la réception.

**Crashs dist (14:09, 14:19, 14:29, instance 3 14:54)** : même signature, en mer, juste après le
chargement d'un skin. Analyse (journaux + code généré `build/gen/code_052.c`, adresses EU) : fonction
USA 02607B68 `ResourceControlHD::selectObjectArchiveHeap` (décomp `d_resorce.cpp`, archives `LOD`
des îles lointaines) : lit la vtable des 22 `mObjectArchiveHeaps` (+0x2078) sans test de nullité ;
`createObjectHeaps` (026079B4) les crée dans un tas de 15 Mio pris sur `mpObjectParentHeap`, où sont
les archives de skin (~25 Mo chacune) : plus de place, pointeurs nuls, lecture de null+0xC.
**Corrigé** (`skin.h`) : en mer (stage « sea »), avant de charger les skins dans `phase_5`, les skins
inutilisés sont déchargés et `createObjectHeaps` appelé ; garde sur `02607B68` (remplacé) : un tas nul
n'est jamais lu (l'archive va à `mpObjectHeap`, sinon au tas parent). Déduit, non testé en jeu.

**Notifications** : toutes en bas à gauche (`save_sync.h` `SV_RIGHT` = `SV_LEFT`, 6 emplacements).

**Pseudos à 30 fps** : vérifié (`docs/mod-sdk-v2.md`, `runtime/src/mods/guest_hud.h`) : la liste HUD
d'un mod est faite une fois par pas de logique et affichée telle quelle jusqu'au suivant. Runtime
modifié (`guest_hud.h` `Store`) : chaque liste publiée garde la précédente et sa date ; à chaque image
(`snapshot`), x/y des éléments de même rang, même type, même texte/image/ancre, déplacés de moins de
200 px, sont interpolés selon le temps écoulé (période mesurée entre deux pas), comme les modèles
interpolés (un pas de retard). Nécessite un nouveau runtime : build `build/rel` + `package.py`.
Note : `Client/ZeldaWWHDRecomp` et `D:/.../WindWakerRecomp/ZeldaWWHDRecomp` sont le même dossier
(lien) ; `build/windows-msys2-d` compile depuis le second chemin.

**Dist** : le jar du launcher 0.3.17 avait perdu `com/sun/...` (JavaFX, JNA) en remplaçant
`stage/com` : reconstruit complet (22 Mo), testé (le launcher s'ouvre).
- Release runtime refaite : `build/rel` (llvm-mingw-20260922, `ninja -d keeprsp`, cibles wwhd
  wwhd-extract wwhd_yaz0 wwhd-setup), `tools/release/package.py --version v0.3.0-online` vers
  `dist/_rel` (chemin court : le scratchpad dépasse MAX_PATH), Python embarqué vérifié (SHA-256 du
  pin), `template.bin` copié temporairement de la release officielle puis retiré du dépôt ;
  `guard.py` OK (292 fichiers). `dist/WindWakerHD-v0.3.0-online-windows-x86_64.zip` (31 Mo) remplacé.
  Build de dev (`windows-msys2-d`) : compilé, édition de liens refusée tant que le jeu tourne.
  Non testé en jeu (installation du nouveau zip, pseudos interpolés).
- Consigne (20 octobre) : ne plus modifier `dist/` sans demande explicite.
- Build de dev `windows-msys2-d` relié (16:01) : interpolation du HUD incluse (objets dépendants de
  `mods/guest_hud.h` recompilés).
- Launcher, Play Online : cartes 1/2/3 et bouton « USE MY OWN SERVER » supprimés ; « CONNECT TO THE
  SERVER » : gros bouton vert pleine largeur sous le panneau (`.connect-big-button`, les deux
  `launcher.css`). Barre latérale : vagues sur toute la largeur (272) et 120 de haut, hors du padding.
  Compilé (javac), lancé 10 s sans erreur ; rendu non vérifié visuellement.

## 0.3.19 : skins partagés par le serveur, particules de pots

**Skins** : avant, un skin ne se voyait que si chaque joueur avait le fichier `skin_<id>.szs`.
- Serveur : `SkinStore.java` (nouveau), service TCP sur port de jeu + 1 (10015), démarré par
  `WWServer.main` ; une requête par connexion : `HASH id`, `PUT id taille sha256` + octets, `GET id` ;
  id `[a-z0-9_]{1,12}`, 64 Mio max, SHA-256 vérifié, en-tête Yaz0/SARC exigé ; fichiers dans
  `User/skins/` à côté du serveur.
- Mod : `players.txt` `skins_missing=` (skins vus chez les autres ces 10 dernières minutes, fichier
  absent ; vérifié toutes les 150 étapes, `skin.h` `skin_missing`). Un skin absent est retenté au
  chargement de zone suivant (`skin_scene_load` remet les FAILED à IDLE).
- Launcher : toutes les 10 s, sur un thread à part : envoie le skin choisi (`launcher.cfg` skin) si
  l'empreinte du serveur diffère (empreinte locale en cache par taille/date) ; télécharge les
  `skins_missing` dans le dossier Data (fichier temporaire puis renommage ; une tentative par minute
  et par skin). Le pare-feu de l'hôte doit aussi ouvrir 10015.
**Particules de pots** : la casse d'un pot par le code du Link local (`daTsuboDamage::damaged`
024CC004) avait lieu pendant la capture des particules du joueur : ses effets étaient envoyés et
doublés chez les autres (qui cassent déjà leur copie). Capture coupée pendant `damaged` (hooks
d'entrée et de retour) et pendant `fopAcM_createItemFromTable` (`actor_sync.h`).
Tests : mod 0.3.19 compilé, vérifications OK, installé instances 1 et 3 ; `SkinStoreTest` (scratchpad,
port 20015, faux skin de 3 Mo) : absent, envoi, empreinte, téléchargement identique, absent, archive
invalide refusée, id invalide refusé : OK. Corrigé au passage : `MMBSystem.warningS` lève une
exception sans système MMB (journal de repli), id validé côté client avant envoi.

## 0.3.20

**Crashs 15:57 et 16:16 (0.3.18, instance 1)** : écriture à l'adresse 0 dans la météo
(`setup_cloud_geometry` ← `move_weather_frame` ← `weatherExecute`), juste après « the sea's object
heaps could not be made », sans skin chargé. Déduit : la création anticipée des tas de la mer
(0.3.18, `phase_5`) échouait même sans skin et privait la météo de mémoire. **Retirée** ;
`selectObjectArchiveHeap` garde son test de nullité.
**Pseudos** : dessinés en couches (contour, bandes de dégradé chacune dans un clip, couleur du joueur) ;
l'interpolation du runtime ne déplaçait pas les rectangles de clip : les bandes restaient à 30 Hz
derrière le texte. `guest_hud.h` : les `ClipPush` sont interpolés aussi ; seuil de saut 600 px (200
coupait le lissage en tournant vite la caméra). Distance : opaque jusqu'à 6 m, caché à 7 m (`mod.c`).
Tests : mod 0.3.20 compilé, vérifications OK, installé instances 1 et 3 ; runtime de dev recompilé.
**Non testé en jeu.**

## 0.3.21

**Partage des skins (essai à deux instances sur un PC)** : vérifié : le serveur écoute 10015,
`User/skins/skin_peppino.szs` reçu (empreinte identique au fichier de l'instance 1) ; l'instance 3
liste `skins_missing=peppino`. Cause : un seul launcher, lié au dossier Data de l'instance 1 : rien
ne télécharge pour l'instance 3 (entre deux PC, chaque joueur a son launcher). Launcher : option
`--config <launcher.cfg>` (un second launcher sur un autre dossier Data, sans changer le choix
enregistré). Mod (`skin.h`) : un skin en échec faute de fichier était encore gardé en échec tant
que son joueur restait vu : retenté dès que le fichier est là.
**Pseudos** : opaques jusqu'à 7 m, fondu jusqu'à 10 m.
Puis (même jour) : retour « reçu par le serveur, pas vu » : l'instance 3 listait
`skins_missing=peppino,shaggy` et n'avait toujours aucun fichier (aucun launcher sur son dossier).
Launcher : `skinSync` traite tous les jeux connus du PC (`knownGameFolders` : dossier lié, dossiers
liés par BROWSE mémorisés `hdLauncherCfgHistory`, autres instances de dev `instances/N/user/ModManager/
Data/multiplayer-connection`) dont `players.txt` date de moins d'une minute ; un skin déjà présent
dans un autre de ces dossiers est copié, sinon téléchargé du serveur ; l'envoi reste celui du jeu lié.
Tests : mod 0.3.21 compilé, vérifications OK, installé instances 1 et 3 ; launcher compilé.
**Non testé en jeu.**

## 0.3.22 : le Link porté

Journal (instance 1) : « y given before 809 », après execute 697, 695, 692, 690 : le Link repartait
de sa propre place précédente et s'enfonçait (CAUGHT coupe le sol). Vérifié
(`d_a_player_main_04.cpp`) : `daPy_lk_c::execute` commence par `current.pos = l_debug_keep_pos`
(1046CD48) et les angles de `l_debug_shape_angle` (1046CD10) / `l_debug_current_angle` (1046CD08),
que sa fin enregistre : toute place écrite entre deux executes est perdue (le portage 0.3.11-0.3.21,
et toute écriture de la place du Link local hors de son execute). **Corrigé** (`carry.h`
`cy_place_local`) : place et angle Y écrits aussi dans ces trois globales. Mod 0.3.22 compilé,
vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.23 : cinématiques pour toute la room

Demande : une cinématique lancée par un joueur se lance pour tous les joueurs de la room (pas les
coffres ni les portes de boss pour l'instant). Vérifié (décomp) : les events d'un stage
(`dEvDtEvent_c`, `mName[32]` à +0) démarrent dans `dEvent_manager_c::startProc` (02543C6C, binding SDK) ;
les acteurs les demandent par nom avec `fopAcM_orderOtherEvent2` (025D77DC, 4 arguments : acteur,
nom, 1, 0xFFFF), répété chaque pas jusqu'au démarrage.
**Implémenté** : `event_sync.h` (hook d'entrée de `startProc`), paquet `CLIENT_PLAYER_EVENT` /
`SERVER_PLAYER_EVENT` (`mp_protocol.h` : VLQ uid, stage ≤ 8, u8 room, nom ≤ 31, ASCII imprimable ;
test `test_event`), relais serveur `WWClient.receiveEvent` (+ `ActorFilter.SERVER_PLAYER_EVENT`).
Pas envoyés : les events génériques `DEFAULT_...` (dialogues, portes, coffres, objets), ni un event
lancé pour un autre joueur (écho, 10 s). Reçu : même stage et même room, Link local vivant, pas
d'event en cours : demandé pendant 3 s au plus avec le Link local comme acteur.
Déduit, non testé : une cinématique dont un acteur manque chez un joueur peut s'y comporter
autrement. Mod 0.3.23 compilé (111 hooks), vérifications OK, installé instances 1 et 3 ;
`mp_protocol_test` OK ; serveur compilé. **Le serveur doit être recompilé. Non testé en jeu.**

## 0.3.24 : mémoire des skins

Crash instance 3 16:52 (0.3.21, Omasao puis mer, skin amongus chargé) : 4 × « an object archive heap
is missing », puis écriture à l'adresse 0 dans `ResMgr_loadArchive` (USA 02612E64, `hd_res_mgr.cpp`,
fil de chargement) : une archive chargée dans un tas plein. Même cause que les crashs dist : les
skins (~25 Mo) occupent `mpObjectParentHeap` (+0x2040), où le jeu met ses objets et, en mer, son tas
d'objets de 15 Mio. **Implémenté** (`skin.h` `skin_room`) : avant de demander un skin, place libre du
tas parent (méthode virtuelle `getFreeSize`, vtable +0xC entrée +0x74, comme
`selectObjectArchiveHeap`) ; le skin n'est demandé que s'il reste 32 Mio (estimation du skin) +
12 Mio de réserve (+ 16 Mio en mer tant que le tas des îles n'est pas fait), sinon Link est montré et
le skin retenté à la zone suivante. Journal « skin memory: object heap free / needed » pour régler les
marges. Mod 0.3.24 compilé, vérifications OK (l'appel indirect passe le traducteur), installé
instances 1 et 3. **Non testé en jeu.**

## 0.3.25 : tas du jeu agrandis

**Vérifié dans les sources** (`hd/hd_sys_0203E584.cpp`, `hd/hd_sys_0203E9EC.cpp`, code généré) :
- tas racine principal (index 6, `hdRootGetMainHeap` 0203E8F4) = `getRootHeapSizes()[5]` = 0x1F200000 (499 Mio),
  pris dans le tas racine de sead (`createRootHeap` 0203E67C) ;
- la tâche racine 0203EE2C y fait « command heap » (overhead(4) + 0xE00000) et « system heap »
  (0x16C00000 - command, ≈ 350 Mio) ;
- dans le system heap : « data heap » (overhead(0x2000) + 0x0D400000, ≈ 212 Mio), et le reste est
  `mpObjectParentHeap` (+0x2040, écrit en 0203F244), où vont les objets et les skins
  (≈ 104 Mio libres dans le journal de l'instance 1) ;
- tous ces tas passent par `hdRootCreateExpHeap` 02753004 (5 arguments en registres) ; taille 0 =
  `getMaxAllocatableSize` du parent (vtable +0x7C, alignement 4) ;
- le tas MEM2 par défaut du runtime va de 0x104DB000 à 0x50000000 (≈ 1019 Mio, journal), plus que
  les ≈ 825 Mio des tas racines.

**Implémenté** (`heap_grow.h`, WWHD_REPLACE de 02753004) : le tas principal et le system heap sont
agrandis du même montant, qui finit libre dans le tas parent des objets. Montant = place restante
du tas racine sead − tas principal − tas racines suivants (77 Mio) − marge 64 Mio, au plus 256 Mio.
Si la création échoue, la taille d'origine est reprise. Journal « heaps: … made bigger by (MiB) ».
Pas sur la liste EU `differing_functions` (vérification du build OK).
Mod 0.3.25 compilé (112 hooks), installé instances 1 et 3. **Non testé en jeu** : le gain réel
dépend de ce que le runtime a déjà pris dans le tas par défaut (journal au démarrage).
Piste suivante, non faite : passer `mem::kMem2End` à 0x60000000 (+256 Mio). Cela touche les
contrôles de pointeurs codés en dur à 0x50000000 dans `interp_fx.cpp`, `true60*.cpp`, `turbo.cpp`
et `model_probe.cpp`.

## 0.3.26 : MEM2 du runtime élargie, marge corrigée

**Testé, échec de la 0.3.25** (instance 1, 17:29) : tas racine sead 701 Mio libres, tas principal et
system heap agrandis de 62 Mio, puis `PROGRAM HALT m_Do_machine.cpp:196 heap != 0` :
`mDoExt_createGameHeap` (10 Mio) n'a plus de place. Le jeu prend dans le tas racine sead bien plus
que les tas racines 7 à 12 : sans le mod il restait 125 Mio, et 63 Mio ne suffisent pas.

**Implémenté** :
- runtime : `mem::kMem2End` passe de 0x50000000 à 0x60000000 (`runtime.h`), donc le tas par défaut
  va jusqu'à 0x60000000. Le range 0x50000000–0x60000000 n'était utilisé nulle part dans le runtime
  (le runtime commence à 0x60000000). Les contrôles de pointeurs codés en dur à 0x50000000 utilisent
  maintenant `mem::kMem2End` : `interp_fx.cpp`, `model_probe.cpp`, `mods/turbo.cpp`, `true60.cpp`,
  `true60_test.cpp`. Les états sauvegardés gardent leurs régions dans le snapshot ; les anciens se
  restaurent sur la partie basse ;
- mod (`heap_grow.h`) : `HG_MARGIN` = 128 Mio, laissés au tas racine sead (≥ les 125 Mio d'origine).
  Le surplus ne vient donc que de la mémoire ajoutée par le runtime : environ 250 Mio attendus.

`wwhd.exe` de dev recompilé. Mod 0.3.26 installé instances 1 et 3.
**Testé** : avec le nouveau runtime, l'instance 1 démarre (« default heap 104DB000-60000000 ») et
tourne environ 650 images sans erreur, jusqu'à la fermeture de la fenêtre. Le mod multijoueur était **désactivé** dans son profil
(`profiles.json`, Default : false) : la partie mod n'est pas encore testée.
Préexistant, sans lien : le test `guest_hooks_test.exe` ne se lie plus (`item_icons::game_icon`,
`mp_skins::set_directory` manquent dans sa liste de sources).

## 0.3.27 : tenues écrites au lieu d'échangées

Problème : texture du pyjama sur la tenue du héros. Le modèle de Link est partagé par tous les Link,
et chaque `playerInit` / `playerDelete` (le Link local et chaque puppet) échange les en-têtes de
texture avec ses propres copies (+0x48C). Un échange fait dans un mauvais état laisse des en-têtes
mélangés. Les échanges du mod (`outfit_swap`) dépendaient aussi de l'état précédent.
**Vérifié dans les sources** : `playerInit` (`d_a_player_main_04b.cpp`) fabrique l'en-tête pyjama à
partir de `getRes("Link", 0x71)`, en recalant paletteOffset (+0x0C) et imageOffset (+0x1C) sur
l'entrée et en reprenant mHDTexture (+0x20). Les copies +0x48C sont prises sur l'entrée de même
index : les offsets sont relatifs à l'entrée.
**Implémenté** (`outfit.h` `outfit_force`) : le mod garde, pour chaque entrée, un en-tête héros et un
en-tête pyjama. Il les reconnaît à leur objet HD (celui de 0x71 = pyjama) et les apprend sur les
entrées et les copies du Link local. Le pyjama est refabriqué comme le fait `playerInit` s'il n'a
pas été vu. Ensuite, aucun échange :
- à chaque pas, les entrées reçoivent la tenue du Link local, ses copies l'autre tenue, et son
  drapeau (chapeau) suit ;
- pendant le dessin d'un puppet, les entrées reçoivent la tenue du puppet, puis celle du Link local.
Un en-tête n'est écrit que s'il diffère (avec le masque dirty). Les échanges que le jeu fait à la
création ou à la suppression d'un puppet sont corrigés au pas suivant.
Limite : si l'en-tête héros n'a jamais été vu sur ce modèle (entrées et copies déjà toutes en
pyjama), rien n'est forcé.
Mod 0.3.27 compilé (112 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.28 : cinématique, les joueurs rejoignent celui qui la lance

**Implémenté** (`event_sync.h` `ev_step`) : quand une cinématique reçue est demandée pour la première
fois (même stage et même room), le Link local est placé à la dernière position reçue du joueur qui
l'a lancée (`remotes[].transform`, ActorPlayerSync). La mise en place réutilise `death.h dh_place`,
celle de la réanimation : position, `old.pos`, vitesses, copies `l_debug_keep_pos` et angles comme
`setPlayerPosAndAngle`, puis `procWait_init`. Pas de placement sur le bateau. Le protocole ne change
pas : le serveur n'est pas à recompiler pour ça.
Déduit, non testé : la position reçue peut avoir une ou deux mises à jour de retard sur le début de
la cinématique. Mod 0.3.28 compilé, vérifications OK, installé instances 1 et 3.
**Non testé en jeu.**

## 0.3.29 : fin des cinématiques reçues (blocage)

**Testé, problème** (instances 1 et 3, A_mori) : l'instance 3 lance `AMORI_DEMO_2`, cinématique du
Bokoblin. L'instance 1 la reçoit, est placée sur l'instance 3 et la cinématique démarre, mais
l'instance 1 reste bloquée.
**Déduit** : un event de stage est joué par ses acteurs. Le Bokoblin du joueur qui reçoit n'a rien
demandé : il ne joue jamais son rôle, donc l'event ne finit jamais. Demander l'event avec le Link
local suffit à le démarrer, pas à le terminer.
**Vérifié dans les sources** : `dEvt_control_c::reset()` = `mEventFlag` (+0xE8) |= 8
(`d_event.h`, ce que les acteurs appellent pour finir un event). `EventControl::check` (0253FF34,
`d_event.cpp`) appelle ensuite `endProc`. Dans le buffer play : event control = 0x5292 − 0xC2,
flag à +0xE8.
**Implémenté** (`event_sync.h`) :
- celui qui a envoyé la cinématique envoie le nom réservé `#END` quand l'event est fini chez lui
  (mode revenu à 0). Le relais serveur ne change pas (nom ASCII ≤ 31 caractères) ;
- chez les autres, une cinématique reçue de ce joueur et en cours se termine avec le reset du jeu.
  Elle se termine aussi si ce joueur n'est plus là, ou après 3 min ;
- une cinématique lancée ou reçue dans les 2 dernières minutes n'est ni renvoyée ni redemandée :
  les acteurs de celui qui l'a reçue peuvent la relancer eux-mêmes plus tard, et elle ne repart pas
  chez tout le monde.
Mod 0.3.29 compilé (112 hooks), vérifications OK, installé instances 1 et 3.
**Non testé en jeu.** Après le reset, les acteurs de la cinématique chez celui qui l'a reçue
(Bokoblin) restent dans leur état d'avant. Ils peuvent la relancer localement quand il s'approche
(non envoyée, voir ci-dessus).

## 0.3.30 : cinématiques, seule la position est partagée

**Testé, 0.3.29** (l'utilisateur) : chez celui qui reçoit, la cinématique demandée avec le Link local
a une mauvaise caméra. Ensuite, son propre Bokoblin relance la cinématique correctement, puisque
le joueur est maintenant près de lui après la téléportation.
**Décision** : on synchronise la cause, pas l'effet. Celui qui reçoit n'ordonne plus rien
(`fopAcM_orderOtherEvent2` et `#END` retirés). Il est seulement placé sur celui qui a lancé la
cinématique (`dh_place`, au pas suivant, pas sur le bateau, pas pendant un event local), et ses
propres acteurs la lancent comme pour leur joueur. Une cinématique déclenchée autrement que par la
position (interrupteur, objet) ne joue que chez celui qui l'a lancée.
Une cinématique lancée ou reçue dans les 2 dernières minutes n'est ni renvoyée ni suivie : le
lancement local par les acteurs de celui qui l'a reçue ne repart pas vers la room.
Mod 0.3.30 compilé (112 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.31 : petite marche après la téléportation

**Testé, 0.3.30** (l'utilisateur) : après la téléportation, la cinématique ne se lance pas.
**Déduit** : le déclencheur (zone où le joueur entre) ne réagit pas à un Link posé immobile.
**Vérifié dans les sources** : `daPy_lk_c::setStickData` (023F32E4, `d_a_player_main_03.cpp`) écrit
`mStickDistance` (+0x6A08) et l'angle de marche monde `m34E8` (+0x6930). La marche de démo
(DEMO_N_WALK) met mStickDistance = 1 et m34E8 = l'angle de la démo. Champs présents dans le SDK
(`wwhd/link.h`).
**Implémenté** (`event_sync.h` `ev_after_stick`, appelé depuis le hook de retour existant de
`death.h dh_after_stick`) : après la mise en place, le Link local marche 30 pas logiques (1 s,
stick 0,6) dans la direction du joueur qui a lancé la cinématique. La marche s'arrête dès qu'un
event tourne, ou si le joueur est mort ou fantôme.
Mod 0.3.31 compilé (112 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.32 : position exacte de départ de la cinématique

**Testé, 0.3.31** (l'utilisateur et les journaux des instances 1 et 3) :
- le joueur téléporté arrive un peu derrière : c'était la dernière ActorPlayerSync reçue, en retard
  jusqu'à `UPDATE_PERIOD` (0,1 s) plus le trajet réseau ;
- certaines cinématiques reçues ne téléportent pas : le filtre « déjà vue depuis 2 min » refusait
  les essais répétés (`AMORI_DEMO_2` relancé), ou un event local tournait (`AMORI_DEMO_1` reçu, sans
  placement, sans message).
**Implémenté** :
- protocole (extension WWHD, pas historique) : `CLIENT_PLAYER_EVENT` / `SERVER_PLAYER_EVENT`
  prennent en option, après le nom, la position du joueur au moment de `startProc` (f32 x, y, z,
  s16 orientation). Un corps sans position reste valide (`mp_protocol.h`, test `test_event` étendu :
  octets, relecture, position coupée refusée). Serveur : `WWClient.receiveEvent` relaie la position
  si elle est là (14 octets), sinon comme avant. **Le serveur doit être recompilé** : l'ancien
  refuse le corps avec position (`available != 0`) ;
- `event_sync.h` : placement à cette position exacte. Si elle manque (ancien client), dernière
  transform reçue. Le filtre de répétition passe à 20 s. Une cinématique reçue pendant un event
  local attend sa fin (5 s au plus). Chaque cinématique non suivie est journalisée avec sa raison
  (mort, déjà jouée, event local, bateau, position inconnue).
`mp_protocol_test` OK. `WWClient.java` compile (javac). Mod 0.3.32 compilé (112 hooks), vérifications
OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.33 : Bokoblins et Moblins portés par les Kargarocs

Problème : les Bokoblins lâchés par les Kargarocs ne sont pas synchronisés.
**Vérifié dans les sources** :
- `daBb_Create` (`d_a_bb.cpp`) : en mode porteur (BB_MODE_CARRY_BOKOBLIN / MOBLIN), le Kargaroc
  crée son passager pendant sa propre création. Il prend un bloc de `fopAcM_CreateAppend`
  (025D5600 : bloc vidé, setID 0xFFFF, room 0xFF, argument 0xFF), y met sa position courante, ses
  angles home, sa room et les paramètres (les siens & 0xFF000000) | 0xFFFF05, puis
  `fpcSCtRq_Request` de fpcNm_BK_e ou fpcNm_MO2_e ;
- ce bloc n'avait pas l'étiquette des acteurs posés (AS_TAG, écrite par `dStage_actorCreate`) :
  l'enfant n'était jamais synchronisé ;
- `dStage_getName2` (`d_stage.cpp`) avec l'argument -1 rend le premier nom du processus (« Bk »,
  « mo2 », tous deux dans `actorList.ktml` du serveur).
**Implémenté** (`actor_sync.h` `as_create_append`) : un bloc créé pendant le `fopAc_Create` d'un
acteur posé et étiqueté (`as_roster_creating`) prend AS_TAG et le numéro de copie du créateur
(AS_CHILD 0xFD si le créateur est invoqué, pour ne pas passer pour une invocation). Comme chaque jeu
crée l'enfant au même moment avec le même bloc, les UID de jeu concordent et l'enfant se synchronise
comme un acteur posé (propriétaire, coups rejoués, suppression à la mort).
Déduit, non testé : vaut pour tout enfant créé pendant la création d'un acteur posé, s'il est dans
la liste du serveur. Mod 0.3.33 compilé (112 hooks), vérifications OK, installé instances 1 et 3.
**Non testé en jeu.**

## 0.3.34 : couleurs de tunique sur la texture du pyjama

**Testé** (capture de l'utilisateur, instance 1, 0.3.32) : sur la tenue du héros, la texture du
pyjama recolorée aux couleurs de tunique.
Journal de l'instance 1 :
- `tunic texture found` sur l'objet 2E9A2E40 (640×384, format 0x431 : le héros, seul objet vu sur
  l'instance 3 en tenue de héros) ;
- `clothes set, casual 1`, puis `tunic texture found` sur 2E9A5D80 (format 0x433, 245760 octets :
  le pyjama), puis `casual 0`, puis à nouveau 2E9A2E40.
**Déduit** : `tunic_find_hero` choisissait entre les entrées et les copies (+0x48C) selon le
drapeau. Pendant le changement de tenue, drapeau et table ne concordaient pas, et le pyjama a été
pris pour le héros et recoloré. Une texture quittée n'était pas restaurée : sa copie d'origine
était libérée et ses couleurs restaient. Une texture reprise ensuite était recapturée déjà colorée.
**Implémenté** (`tunic.h`) :
- `tunic_find_hero` prend, pour chaque entrée, l'en-tête dont l'objet HD n'est pas celui du pyjama
  (`getRes("Link", 0x71)`, `outfit_casual_texture`), entrée ou copie. Le drapeau ne sert que si
  l'objet du pyjama est inconnu, et l'objet du pyjama n'est jamais pris ;
- une texture quittée retrouve son image et ses mips d'origine avant que sa copie soit libérée
  (journal `tunic texture left, its colours restored`).
Mod 0.3.34 compilé (112 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Une texture déjà abîmée dans une partie en cours se répare au prochain lancement du jeu.

## 0.3.35 : interrupteurs, il faut être dessus

**Testé** (l'utilisateur) : un deuxième joueur collé contre l'interrupteur, sans monter dessus,
compte comme présent. Le test était un cylindre (rayon 140, hauteur ±120) autour de la position de
l'interrupteur.
**Vérifié dans les sources** (`d_a_obj_swpush.cpp`) :
- `rideCB` (0239705C) teste les pieds du joueur contre le carré du dessus, formé des sommets
  6, 10, 11, 7 (`swpushOpVertices`) du fond de collision `Act_c::mpBgW` (+0x3BC) ;
- table des sommets `pm_vtx_tbl` +0x90 (coordonnées monde, 0xC chacun), nombre dans `pm_bgd` +0x94
  → `m_v_num` ;
- `Mthd_Execute` écrit le y de ces sommets quand le dessus bouge.
**Implémenté** (`heavy.h hv_on_switch`) : un joueur (le Link local pour l'affichage, et les joueurs
distants) compte s'il est au-dessus de ce carré (même côté des quatre arêtes) et à moins de
25 sous / 60 au-dessus de sa hauteur. L'ancien cylindre ne sert que si le fond est introuvable.
Mod 0.3.35 compilé, vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Couleurs étranges sur les puppets : **déduit** de même cause que 0.3.34. Les copies de tunique des
puppets sont faites depuis la copie d'origine de la texture locale, qui pouvait être le pyjama ou
une image déjà recolorée. Non vérifié en jeu.

## 0.3.36 : skins jamais demandés (diagnostic) et arène sead

**Testé** (session de l'utilisateur 20:41-20:46, 0.3.35, instance 1, `instances/1/wwhd.log` complet) :
- aucune ligne de skin. Pourtant `launcher.cfg` contient skin=garfield (le vrai parseur
  `mp_parse_launcher_cfg`, testé sur le fichier, rend has_skin=1 garfield), `skin_garfield.szs` est
  dans Data, et les hooks de skin sont installés (EU 025B2F50 phase_5, 02606DC8 getRes, 02604C54) ;
- `heaps: room in sead's root heap 701 MiB, made bigger by 0`, malgré MEM2 jusqu'à 0x60000000.
**Déduit, non vérifié** : `skin_add` ne journalise pas dans trois cas seulement (identifiant vide,
déjà présent, aucun emplacement libre). La table `skin_archives` aurait des états invalides
(écrasée ?) ou des emplacements tous pris.
**Implémenté** :
- `skin.h` : à chaque chargement de scène, journal du skin local et de l'état de chaque emplacement
  non libre. Un emplacement dans un état inconnu est libéré (journalisé). `skin_add` journalise
  « no free skin slot » ;
- `heap_grow.h` : l'arène de sead ne prend pas tout le tas par défaut. **Vérifié dans le code
  généré** (pas de source décompilée) : 02756D18 (EU 027575D4, appelée une fois par 02755F80) reçoit
  r3 l'enregistrement de l'arène et r4 la taille voulue. Elle garde min(voulue, place libre du tas
  par défaut), alloue, écrit la taille à +4 et la mémoire à +0. WWHD_REPLACE : la taille voulue
  + 256 Mio. Journal « heaps: sead's arena (MiB) ». Pas dans `differing_functions`.
Mod 0.3.36 compilé (113 hooks), vérifications OK, installé instances 1 et 3.
**Test de démarrage impossible** : le gestionnaire de mods refuse une version réinstallée tant que
son code natif n'est pas reconfirmé par l'utilisateur
(`multiplayer-connection not loaded: it contains native code you have not confirmed`). Non contourné.

## 0.3.37 : plus aucun modèle avec la 0.3.36, mémoire du jeu gardée sous 0x50000000

**Testé, 0.3.36** (l'utilisateur, instance 1, 20:57) :
- arène sead 1207 Mio, tas principal et tas système +254 Mio, tas des objets 366 Mio libres ;
- le skin garfield est chargé (l'absence de skin de la session précédente ne se reproduit pas) ;
- mais **aucun modèle dessiné** : interp « models per step: 0.0 » en jeu (MajyuE), contre ~50
  d'habitude. Le jeu ne met plus ses modèles à jour.
**Vérifié** : aucune limite à 0x50000000 dans le chemin de rendu du runtime (GX2 display lists
`gx2_core.cpp`, buffer cache, write watch sur 4 Gio, `ld32`/`st32` directs). Seul Metal mappait
0x10000000 + 0x40000000 : corrigé (`metal_main.mm`, `kMem2End`).
**Déduit, non vérifié** : le tas principal finissait sous 0x50000000. Les tas racine créés après
lui (7 à 12) passaient au-dessus, dont le tas 7, l'allocateur d'affichage de la scène
(0203E900, `d_s_play_3.cpp phase_00 prepareDisplayHeap / attachDisplayHeap`). Le dessin du jeu ne
prendrait pas de mémoire au-delà de 0x50000000.
**Implémenté** (`heap_grow.h`) :
- avant le tas principal agrandi, une réserve de 81 Mio est créée dans le tas racine sead, puis
  détruite après lui (vtable +0x24 `mDestroy`, `d_resorce.h ResourceHeapVirtualsHD`, utilisée ainsi
  dans `d_resorce.cpp`). Les tas 7 à 12 s'y placent, sous le tas principal ;
- le « data heap » (overhead(0x2000) + 0x0D400000, direction ~0 = fin du tas système) est créé au
  début du tas système (direction 1). Seule la fin du tas système dépasse 0x50000000 : de la
  mémoire libre pour les objets, prise en dernier.
Journaux : adresses du tas principal, du tas système et du data heap.
Mod 0.3.37 compilé (113 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Si les modèles manquent encore, ou si un objet ou skin placé au-dessus de 0x50000000 ne s'affiche
pas, la limite vaut pour tout ce que le jeu dessine. Il faudra alors revenir à l'arène d'origine et
réduire la taille des skins.

## 0.3.38 : retour aux tas du jeu par défaut, option de test `heap_mode`

**Testé, 0.3.37** (l'utilisateur, 21:08) : tas principal à 24FFD77C (fin au-dessus de 0x50000000),
tas 7 à 12 redescendus en dessous, data heap au début du tas système. **Toujours aucun modèle.**
La piste « tas d'affichage au-dessus de 0x50000000 » est donc fausse.
**Corrections de diagnostic** :
- « models per step: 0.0 » n'est pas une preuve : l'interp ne mélange rien en mode 60 fps, et la
  session 0.3.35 (modèles visibles) affichait aussi 0 ;
- pas d'épuisement du tas par défaut : le runtime journalise `[mem] exp heap ... out of memory`,
  absent ;
- 0273BF88 (constantes 0x10000000 / 0x30000000 / 0x50000000 / 0x70000000) classe un angle par
  octants (atan2), pas une adresse.
**Bloqué** : la cause reste inconnue. Points communs entre 0.3.36 et 0.3.37 : l'arène sead plus
grande (sa fin au-dessus de 0x50000000), et les tas principal et système plus grands.
**Implémenté** : option `heap_mode` du mod (manifest, lue au démarrage, sans réinstallation) :
- 0 = tailles du jeu (défaut, comme la 0.3.35 : modèles visibles) ;
- 1 = arène sead + 256 Mio seulement ;
- 2 = arène et tas agrandis (0.3.37).
Un essai en mode 1 dira si c'est la mémoire de l'arène au-dessus de 0x50000000, ou l'agrandissement
des tas, qui casse le dessin.
Également mesuré : un skin décompressé fait 22,8 Mo, dont `MpSk_<id>.bfres` 14 Mo et environ 8,8 Mo
de shaders (`*.sharcfb`) copiés de l'archive Link. Piste d'économie : réutiliser les shaders de Link.
Mod 0.3.38 compilé (113 hooks), vérifications OK, installé instances 1 et 3. Metal : mapping MEM2
corrigé (`kMem2End`), non compilé ici (Windows).

## 0.3.39 : un tas propre aux skins

**Testé** (l'utilisateur) : les modèles absents coïncidaient avec le chargement d'un savestate
(emplacement 3, « portable state applied », « guest mod set differs ») dans les sessions 0.3.36 et
0.3.37. **Déduit** : l'agrandissement mémoire n'en est probablement pas la cause. Non confirmé.
**Crash** instance 3, 21:17 (0.3.38, mode 0, île de l'Aurore : skin changé, salle chargée, sortie
puis retour) : fil de ressources, écriture à l'adresse 0 dans EU 02613568 (= USA 02612E64
`ResMgr_loadArchive`, comme en 0.3.24).
**Vérifié dans les sources** (`hd_res_load_task.cpp ResLoadTask::startLoad`) : une archive se charge
dans un tas de taille 0, c'est-à-dire tout le plus grand bloc libre de son parent
(parent = `ResourceControlHD::selectObjectHeap` 02607F80), puis réduit (adjust).
**Déduit** : un skin déchargé (environ 23 Mo) laisse un trou parmi les objets du jeu, l'archive
suivante trouve un bloc trop petit. `skin_room` mesurait la place totale, pas le plus grand bloc.
**Implémenté** (`heap_grow.h`, `skin.h`) :
- `heap_mode` 1 devient le défaut : arène sead + 256 Mio, tas du jeu inchangés ;
- au premier skin, un tas « MpSkinHeap » est créé en fin du tas racine sead (direction -1), avec
  toute sa place sauf 128 Mio, et au plus 256 Mio : le jeu garde au moins la place qu'il avait ;
- `selectObjectHeap` (WWHD_REPLACE 02607F80, 4 arguments ; code généré : r3 control, r4 nom
  SafeString, r5/r6 transmis tels quels à 02603740) donne ce tas aux archives « MpSk_ » ;
- `skin_room` vérifie le plus grand bloc libre de ce tas (26 Mio par skin, en comptant les skins
  déjà demandés pendant ce chargement). Sans ce tas (mode 0 ou 2, ou plus de place), on revient à
  l'ancien calcul.
Capacité attendue : environ 9 à 10 skins complets.
Mod 0.3.39 compilé (114 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Le mode 1 met la mémoire des skins au-dessus de 0x50000000. À vérifier qu'ils s'affichent, sans
charger de savestate.

## 0.3.40 : tas des skins créé au démarrage, plus grand bloc libre

**Testé, 0.3.39** (instance 3, 21:33) :
- démarrage OK, mode 1, arène 1207 Mio ;
- au premier skin, `room in sead's root heap 0 MiB` : un tas créé par le jeu entre-temps prend tout
  le reste du tas racine sead (taille 0 = toute la place). Le tas des skins n'a pas été créé ;
- les skins sont retournés parmi les objets : garfield (106 → 84 Mio libres), puis amongus, puis le
  même crash (ResMgr, écriture à l'adresse 0). Les modèles s'affichaient (sans savestate).
**Implémenté** :
- `heap_grow.h` : le tas des skins est créé juste après le 6e tas racine qui suit le tas principal
  (tas 12, `createRootHeaps`), avant ce tas qui prend tout. Toute la place sauf 128 Mio, au plus
  256 Mio, en fin d'arène ;
- `skin.h` : sans tas des skins, `skin_room` utilise le plus grand bloc libre (vtable +0x7C
  getMaxAllocatableSize) du tas des objets, et non plus la place totale.
Mod 0.3.40 compilé (114 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.41 : routage des archives de skin vers leur tas

**Testé, 0.3.40** (instances 1 et 3, 21:37-21:41) : tas des skins créé au démarrage (382 Mio de place
après les tas racine, tas de 254 Mio à 5BC73650). Mais son plus grand bloc libre reste à
260095 Kio après le chargement d'amongus puis de minecraft : les skins n'y allaient pas. Même crash
sur l'instance 3 (ResMgr, écriture à l'adresse 0) après le changement de skin.
**Vérifié dans les sources** (`hd_res_load_task.h/.cpp`) : `startLoad` appelle
`selectObjectHeap(control, &request->mResource, request)`. Le nom de l'archive est
`Request::mArchive` (FixedString64 à +0, pointeur de texte à +0), donc dans le 3e argument.
`mResource` (+0x4C) ne contient pas le nom de l'archive.
**Implémenté** (`heap_grow.h`) : « MpSk_ » est cherché n'importe où dans les deux chaînes (nom et
requête). Journal « a skin archive goes to the skins' heap ».
Mod 0.3.41 compilé (114 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.42 : protection des tas d'archives stage, diagnostic des tas

**Testé, 0.3.41** (instances 1 et 3, 21:52) :
- le routage marche : « a skin archive goes to the skins' heap », et le plus grand bloc du tas des
  skins baisse d'environ 22 Mio par skin (260 → 237 → 215 Mio) ;
- crash sur l'instance 3 après le 3e skin (shaggy), d'un autre type : lecture de l'adresse 0x0C dans
  EU 02604388 = USA 02603C84 `selectStageArchiveHeap`.
**Vérifié dans les sources** (`d_resorce.cpp`, code généré) :
- `selectStageArchiveHeap` (2 arguments : control, nom) appelle `getFreeSize` sur
  `mStageArchiveHeaps[22]` (+0x20D0) sans test de nullité ;
- `createStageHeaps` (02603ACC) les crée dans un tas de 1,5 Mio pris sur `mpStageParentHeap`
  (+0x2044). D'après le code généré de 0203EE2C, +0x2044 = le « command heap » de 14 Mio.
**Déduit, non vérifié** : le command heap se remplit au fil des chargements de skins. Chaque
fichier de skin fait environ 25 Mo sur disque. Cause inconnue.
**Implémenté** (`heap_grow.h`) :
- WWHD_REPLACE de 02603C84 : si le tas stage ou un des 22 tas manque, l'archive va dans le tas
  stage, sinon dans le tas parent stage. Journalisé ;
- `hg_log_heaps` : à chaque chargement de scène (`skin_scene_load`), place libre et plus grand bloc
  des tas ressource (+0x203C), parent objets (+0x2040), parent stage (+0x2044) et du tas des skins.
Mod 0.3.42 compilé (115 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.43 : montage des archives de skin dans le tas des skins

**Testé, 0.3.42** (instance 3, 22:03, Omasao/mer) : tas parent stage (command heap, 14 Mio)
10065 Kio libres avant shaggy, 8078 après, 6091 après amongus, 1634 après garfield (en mer). Puis
« stage archive heaps are missing », la protection 0.3.42 détourne les archives, et un autre crash
(lecture de 0x0C dans EU 02606940 = USA 0260623C `setRes`). **Vérifié** : environ 2 Mio de command
heap par skin chargé.
**Vérifié dans les sources** (`d_resorce.cpp`, `hd_res_model.cpp`) :
- `setArchive` (02604EB4, enregistrement d'une archive lue par la tâche de chargement) la monte (bfres,
  un sharcfb par modèle, objets shader) dans un tas solide de taille 0 ;
- ce tas est pris sur `resourceCallSelectHeap` = `selectStageHeap` (02604094), qui rend
  `mpStageParentHeap` (le command heap) pour toute archive hors LOD de la mer.
Les données du fichier allaient au tas des skins (0.3.41), pas leur montage.
**Implémenté** (`heap_grow.h`) : WWHD_REPLACE de `selectStageHeap` (code généré : r3 control,
r4 nom, r5/r6 transmis) : les archives « MpSk_ » sont montées dans le tas des skins. Journal
« a skin archive is mounted in the skins' heap ».
Mod 0.3.43 compilé (116 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
À vérifier : le command heap ne baisse plus d'un skin à l'autre (journal `hg_log_heaps`).

## 0.3.44 : skins noirs à partir du 3e (test d'adresse)

**Testé, 0.3.43** (l'utilisateur) : plus de crash. Mais le 3e skin chargé est dessiné tout noir, avec
une silhouette correcte, et laisse de la « bouillie » noire à l'écran. Avec
`WWHD_VK_BUFFER_CACHE=0`, même résultat : le cache de tampons Vulkan (budget 107 Mio) n'est pas en
cause.
**Déduit, non vérifié** : le tas des skins (environ 0x4C000000-0x5C000000) se remplit par le bas,
environ 24 Mio par skin. Le 3e est le premier au-dessus de 0x50000000. Même famille que les modèles
absents du mode 2 ?
**Vérifié** : aucune limite d'adresse dans le runtime. `set_texture` encode `image >> 8` et
`sampled_texture` décode `w[2] << 8` sans plage. Rien non plus dans les mémos de shaders et de
surfaces, ni dans `draw.cpp`.
**Implémenté** : option `skin_heap_high` (test, désactivée par défaut). Après la création du tas des
skins, son début est occupé jusqu'à 0x50100000 (vtable +0x34 alloc, comme dans 02753004), pour que
le 1er skin se charge au-dessus de 0x50000000.
Mod 0.3.44 compilé (116 hooks), vérifications OK, installé instances 1 et 3. **Test à faire** :
avec l'option, un seul skin. Noir = l'adresse est en cause, visible = c'est le nombre de skins.

## 0.3.45 : test d'adresse corrigé

**Testé, 0.3.44** (instance 3, option active) : le skin s'affiche, mais **le test n'a rien pris** :
la sonde (alignement 4) est sortie en haut du tas des skins (5BC73640, sous son en-tête), au-dessus
de 0x50100000. Or les skins se chargent en bas (tas de taille 0 = tout le plus grand bloc, puis
réduit). Le skin est resté sous 0x50000000 : résultat non concluant.
**Implémenté** : la sonde et le bloc pris utilisent l'alignement -4 (l'autre bout), avec contrôle de
l'adresse obtenue et journal du plus grand bloc restant.
Mod 0.3.45 compilé, vérifications OK, installé instances 1 et 3. **Test à refaire.**

## 0.3.46 : tas des skins sous 0x50000000

**Testé, 0.3.45** (option `skin_heap_high`, un seul skin chargé au-dessus de 0x50000000) : **skin
invisible**. Sous 0x50000000, il s'affiche. **Vérifié en jeu** : ce qui est au-delà de 0x50000000 ne
se dessine pas. Cause non trouvée. Rien dans le runtime : textures (`set_texture` /
`sampled_texture`), registres de `gx2_texture_regs.cpp`, buffer cache, invalidations
(`surfaces.cpp`), aucune conversion physique, aucune fonction de validation.
**Implémenté** (`heap_grow.h`) : sans l'option, le tas des skins est créé **au début** de la place
restant dans le tas racine sead (direction 1), et s'arrête à 0x50000000 moins 1 Mio. L'adresse de
départ est mesurée par un petit tas créé puis détruit. Le tas que le jeu crée ensuite avec tout le
reste reçoit la mémoire au-dessus. Les 4 tas suivants créés dans le tas racine sead sont journalisés
(nom, taille demandée, adresse).
Capacité attendue : environ 149 Mio, soit environ 6 skins de la taille actuelle. Avec les shaders
retirés des skins (environ 15 Mo chacun), environ 9 à 10.
Mod 0.3.46 compilé (116 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu** :
vérifier que le jeu dessine encore tout, puisque son tas « tout le reste » est maintenant au-dessus
de 0x50000000.

## 0.3.47 : état stable, skins limités à la mémoire sous 0x50000000

**Testé, 0.3.46** (instance 1) : plus aucun modèle du jeu. Journal : tas des skins 193 Mio à
43DFD7DC, puis le jeu crée dans le tas racine sead `FrameworkCafe` (taille 0 = tout le reste) à
4FEFD7EC, puis `GraphicsSystem`, `sead::MethodTreeMgr` et `sead::TaskMgr`. Le système graphique de
sead se retrouvait au-dessus de 0x50000000. **Confirmé** : la mémoire GPU du jeu au-delà de
0x50000000 ne se dessine pas, ni le système graphique de sead, ni un skin. Cause toujours non trouvée.
Les calculs en 0x40000000 / 0xC0000000 du code généré (0261AE4C, 02633978, 0273C980…) sont des
octants d'angle, pas des adresses.
**Implémenté** :
- le placement bas devient l'option `skin_heap_low`. Par défaut, le tas des skins est à nouveau en
  fin d'arène (comme en 0.3.45, le jeu dessine tout) ;
- `hg_skin_heap_room` ne compte que la partie du plus grand bloc sous 0x50000000 (début du bloc =
  haut du tas, mesuré par une sonde d'alignement 4, moins le plus grand bloc). Un skin qui
  dépasserait reste Link (`skin_log` « no room left under 0x50000000 ») : plus de skin invisible ni
  de bouillie noire. Capacité : environ 68 Mio, soit 2 skins de la taille actuelle.
Effet du multiplicateur d'ennemis : les copies d'ennemis utilisent les tas d'acteurs du jeu
(`as_solid_heap`), et leurs archives ne sont chargées qu'une fois par espèce. Pas de concurrence avec
le tas des skins.
Mod 0.3.47 compilé (116 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.48 : shaders des skins partagés avec Link

**Mesuré** (lecture seule, `instances/1/.../Data`, comparaison avec le `Link.szs` de
`permanent_3d.pack`) : les `.sharcfb` des 9 skins sont tous identiques octet pour octet à ceux du
jeu. Tailles décompressées : 20,6-27,4 Mo avant, 12,0-18,8 Mo sans ces shaders (environ 8,6 Mo de
moins par skin).
**Vérifié dans les sources** (`hd_res_mgr.cpp`, code généré) :
- `ResMgr_getFile2` (026124F4 : 1er argument ignoré = objet `sResourceLoader`, puis la ressource
  d'archive, le SafeString du fichier, la taille) ;
- `ResMgr_getArchive` (026123FC : gestionnaire, nom ; clé = nom de l'archive, ex. « Link ») ;
- `hdResModel_loadFiles` lit « <archive>.bfres », puis un « <modèle>.sharcfb » par modèle.
**Implémenté** :
- mod (`skin.h skin_get_file`, WWHD_REPLACE 026124F4) : un `.sharcfb` absent de l'archive dont le
  bfres « MpSk_ » vient d'être lu est lu dans l'archive « Link ». Journal « skin shader taken from
  Link ». Les skins non allégés fonctionnent comme avant ;
- mod (`skin_room`) : besoin d'un skin = sa taille décompressée (en-tête Yaz0 +4, ou SARC +8, lu
  par `skin_file_present`) + 3 Mio de montage, au lieu de 26 Mio fixes ;
- launcher : `SkinImporter.gameLinkFiles` / `stripGameShaders` / `shrinkSkin`. Import HD allégé si
  le dossier du jeu est connu (`knownGameFolder`, sans le demander), import GameCube toujours
  allégé (son modèle de base est le `Link.szs` du jeu). Au démarrage, `shrinkSkins` allège une fois
  les `skin_*.szs` du dossier Data. Le skin du joueur, dont le hash change, est renvoyé par
  `skinSync`. Les skins reçus ne sont pas re-téléchargés (seuls les absents le sont) ;
- `make_skin.py --game` : même retrait.
Mod 0.3.48 compilé (117 hooks), vérifications OK, installé instances 1 et 3. Java compilé (javac).
**Non testé en jeu.** Risque : un `.sharcfb` de Link lu par deux modèles (Link et le skin). Si le
chargement d'un shader modifiait ses données, il faudrait copier le shader dans le tas des skins.

## 0.3.49 : étape 2, mesure de la mémoire inutilisée sous 0x50000000

**Testé, 0.3.48** (l'utilisateur) : les skins allégés (shaders lus dans « Link ») s'affichent bien.
**Implémenté** (`heap_grow.h hg_measure`, appelé à chaque chargement de scène via `hg_log_heaps`) :
pour chaque tas sous 0x50000000, la place libre actuelle et la plus petite atteinte depuis le
lancement, avec son adresse :
- tas racine 0-9 et 12, via leurs accesseurs (`hd_sys_0203E584.cpp`, sans argument,
  0203E8AC-0203E924) ;
- data heap, capturé à sa création (taille overhead(0x2000) + 0x0D400000, direction ~0) ;
- `FrameworkCafe` de sead, le premier tas créé dans le tas racine sead après les tas racine et le
  tas des skins ;
- tas parent objets, tas parent stage (command heap), tas des skins.
Pour le tas principal (6), la place libre est celle hors de ses sous-tas.
Mod 0.3.49 compilé (117 hooks), vérifications OK, installé instances 1 et 3. **Mesure à faire** :
une session qui passe par la mer, des îles, un donjon et un boss. On reprendra la marge sûre
(minimum − réserve) pour d'autres tas de skins sous 0x50000000.

## 0.3.50 : la limite 0x50000000 venait du mod (garde de dessin)

**Testé, 0.3.49** (instance 3, 23:21) : crash en mer à l'arrivée d'un joueur (création de ses
puppets) : `abort()` (signal 6, `ind_sgnl.c`). Pile : `daPy_lk_c::draw` d'un puppet → `mDoExt_drawModel`
(USA 025E2BF4) → `viewCalc` / `mDoExtHeap_draw` (USA 027F55FC) → USA 028F0314 (EU 028F0BD4) →
… → `abort`. Juste avant, la garde de dessin avait écarté des modèles de puppets (animations
libérées). Mesures 0.3.49 : les tas racine, le data heap et `FrameworkCafe` sont entièrement
découpés en sous-tas (0 à 2 Mio libres hors sous-tas). Marges visibles : tas parent objets
97-106 Mio, tas racine 8 (10 Mio, intact).
**Cause trouvée** (`puppet.h`, garde de dessin depuis 0.3.6) :
`draw_ptr(p) = 0x10000000 <= p < 0x50000000`. Tout modèle dont les données, les entrées ou les
matériaux étaient au-dessus de 0x50000000 était déclaré inapte et sauté. Ça explique les skins
invisibles ou noirs au-delà de 0x50000000, et l'absence de tout modèle quand les tas du jeu y
passaient (0.3.36, 0.3.37, 0.3.46). Rien dans le runtime ni dans le jeu.
**Implémenté** :
- `draw_ptr` et `bonus_ptr` (`bonus.h`) bornés à 0x60000000 (`mem::kMem2End`) ;
- `draw_code` refuse une entrée `calc` d'animation dans la bibliothèque C++ en fin de code
  (≥ 0x028E0000) : une animation libérée garde une vtable de classe de base, dont l'entrée est le
  gestionnaire de fonction virtuelle pure, qui appelle `abort`. Le modèle est sauté et journalisé ;
- `hg_skin_heap_room` rend de nouveau tout le plus grand bloc du tas des skins (option de secours
  `skin_heap_low_only`).
Capacité attendue : tas des skins de 254 Mio, soit environ 10 skins non allégés (environ 24 Mio) ou
environ 15 skins allégés (environ 16-17 Mio avec le montage).
Mod 0.3.50 compilé (117 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Restant : les animations libérées encore liées aux modèles des puppets (cause des modèles écartés),
à corriger à la source.

## 0.3.51 : pot sur le joueur, téléportations, bots à skin

**Testé, 0.3.50** (l'utilisateur) : les skins fonctionnent. Restent un pot dessiné sur le joueur, et
des téléportations n'importe où sur l'instance 3 (peut-être en 0, 0).
**Téléportations** (journal de l'instance 3) : `KNOB_START` / `KNOB_START_B` (portes de l'autre
joueur, 10 fois) étaient suivis. Un événement reçu juste avant un changement de zone plaçait le Link
dans la nouvelle zone (mer) aux coordonnées de l'ancienne. **Implémenté** (`event_sync.h`) :
- `ev_generic` exclut, en plus de « DEFAULT_ », les noms contenant KNOB, SHUTTER, DOOR, TREASURE,
  TBOX, TALK, SPEAK, GET, OPEN, WARP, PITFALL ou FALL (noms du décompilé : KNOB00, SHUTTER_DOOR,
  TREASURE, TALK, SPEAK, GET_ITEM ; vus : AJ_SPEAK, FALL_START) ;
- au moment de placer, le stage et la room doivent être ceux de la réception, sinon abandon
  journalisé.
**Pot** : il n'y a pas de pot invisible pour porter un joueur (`carry.h` fait porter le puppet par le
mécanisme du jeu). **Vérifié dans les sources** : `daTsuboMode::Act_c::mode_carry` (024CF844,
`d_a_tsubo_mode.cpp`) place un pot porté (status & 0x102000) avec `dComIfGp_getPlayer(0)`. La copie
d'un pot porté par un autre joueur (bit de son propriétaire, `as_apply_carry`) se dessinait donc
dans les mains du Link local. `as_place` ne la remettait au puppet qu'autour de son execute.
**Implémenté** (`actor_sync.h`) : `as_swap_in` fait aussi tourner une copie portée par un autre
joueur (`as_carried_by_other`, même condition que `as_carried_place`) avec le puppet de ce joueur
comme joueur 0 et link player, comme les armes copiées.
**Bots à skin** (onglet Debug) :
- mod `skin_bot.h` : `debug.cfg` « skinbot=<id> » ajoute un puppet portant ce skin (6 au plus,
  `SB_UID(k)` = PUPPET_BOT_UID | 0x101…), en arc à 2,2 m devant le Link local et tourné vers lui,
  replacé à chaque changement de zone. « skinbots=0 » les retire. Skin noté comme rencontré : chargé
  au prochain chargement de scène s'il ne l'est pas. `puppet_clear_all` garde tout uid portant le
  bit PUPPET_BOT_UID ;
- launcher : panneau « BOTS WITH A SKIN » (liste des skins du dossier Data, ADD / REMOVE).
Mod 0.3.51 compilé (117 hooks), vérifications OK, installé instances 1 et 3. Java compilé.
**Non testé en jeu.**

## Bilan de la session du 2026-10-20 (mod 0.3.16 → 0.3.51)

**Implémenté et testé en jeu** (l'utilisateur) :
- skins partagés via le serveur (SkinStore) ;
- mémoire des skins : tas propre « MpSkinHeap » de 254 Mio, archives lues et montées dedans ;
- shaders partagés avec Link ;
- 6 skins différents affichés en même temps (bots à skin, 0.3.51) ;
- plus de crash au changement de skin (0.3.43) ni de skin invisible au-delà de 0x50000000 (0.3.50) ;
- cinématiques : placement à la position exacte de départ (0.3.32), puis seule la position est
  partagée (0.3.30) ;
- tenues écrites au lieu d'échangées (0.3.27).
**Implémenté, non testé en jeu** :
- 0.3.51 : pot porté par un autre joueur dessiné dans ses mains, filtre des events personnels,
  revérification de zone avant téléportation ;
- 0.3.33 : Bokoblins/Moblins des Kargarocs synchronisés ;
- 0.3.35 : interrupteurs comptés seulement sur le dessus ;
- 0.3.34 : couleurs de tunique jamais sur le pyjama ;
- carry 0.3.22, HUD interpolé (runtime `guest_hud.h`).
**Restant** :
- animations libérées encore liées aux modèles de puppets (modèles écartés par la garde de dessin,
  `draw_guard_log`) : à corriger à la source ;
- au-delà de 6 skins, la limite est `PUPPET_MAX` (8 emplacements partagés), pas la mémoire. Capacité
  mémoire estimée : environ 10 skins non allégés, environ 15 allégés ;
- le serveur doit être recompilé pour : position des cinématiques (0.3.32), SkinStore, carry,
  events, mutateur PvP ;
- le test `guest_hooks_test.exe` ne se lie plus (symboles `item_icons::game_icon`,
  `mp_skins::set_directory` absents de sa liste de sources), préexistant ;
- dist non mise à jour (consigne : ne pas y toucher sans demande).
Copies de `claude.md` et de ce fichier : `ZeldaWWHDRecomp/docs/multiplayer/` (dépôt WWHD-ONLINE),
pour les récupérer sur une autre machine.

## 0.3.53 : crash au 8e Link portant un skin

0.3.52, non documentée ici : `PUPPET_MAX` 16 (`puppet.h`), `SB_MAX` 12 (`skin_bot.h`).
**Testé** (instance 1, 2026-10-21 13:36) : 8 bots à skin (3, garfield, amongus, linkle, miku,
minecraft, peppino, shaggy), 7 skins chargés dans leur tas. Crash à la création du 8e, le premier
portant « shaggy » : écriture à l'adresse 0. Pile : `phase_2` → `playerInit` → `as_solid_heap` →
`fopAcM_entrySolidHeap_` → `useHeapInit` → création du modèle (EU 025E38A0, 027F4CC4…027BF644).
**Vérifié dans les sources** :
- un Link demande un tas de taille 0 (`d_a_player_main_04b.cpp`) ;
- `fopAcM_entrySolidHeap_` (`f_op_actor_mng.cpp`) prend alors tout le plus grand bloc du parent,
  construit, puis réduit. Le code du modèle ne vérifie pas ses allocations.
**Déduit** : le tas des acteurs n'avait plus de bloc assez grand pour un Link portant ce skin.
`as_solid_heap` estimait le besoin d'après le plus grand Link construit (+25 %), sans tenir compte
du modèle propre à chaque skin.
**Implémenté** (`actor_sync.h`) :
- la taille du tas d'un Link portant un skin est notée par archive de skin (`as_skin_link_size`) ;
- le besoin d'un Link à skin = cette taille + 25 %, ou 2 fois celle d'un Link si ce skin n'a pas
  encore été porté ;
- les tailles des Links à skin ne gonflent plus celle d'un Link normal (`as_kind_note`).
Sans place, la création est refusée et journalisée (« actor not made, no room in the actors'
heaps »), et le puppet est réessayé plus tard.
Mod 0.3.53 compilé (117 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
La limite réelle du nombre de Links à skin est donc le tas des acteurs du jeu, pas le tas des skins.

## 0.3.54 : tas des puppets (crash au 9e Link)

**Testé, 0.3.53** (instance 1, 13:41) : les Links à skin prennent 573 à 691 Kio
(`a Link wearing a skin took`). Crash au 8e bot (9e Link avec le local) : `memcpy` vers 0x38.
**Vérifié** (code généré, décompilé) :
- `J3DModel::entryModelData` (USA 027F4404) alloue le bloc des matériaux du modèle par
  `JKRHeap::alloc` (027EC0AC → 0273AE48, tas courant = tas d'acteur du Link, alignement 0x100) ;
- résultat nul, puis 027DE5BC → 027DD9B0 → `DCZeroRange` sur une base nulle ;
- `fopAcM_entrySolidHeapRegister` (025D63E8, `f_op_actor_mng_2.cpp`) prend pour les processus
  0x26, 0xA5, 0xA8 (Link), 0x191, 0x1B5-0x1B6 (0x1B7 si paramètres 0) le **tas de jeu 0**, pour
  les autres le tas 1 ;
- les deux tas de jeu font 0xA00000 (`m_Do_machine.cpp mDoExt_createGameHeap`) ;
- `as_solid_heap` vérifiait toujours le tas 1 (`as_actor_heaps` « first ») : il laissait passer un
  Link alors que le tas 0, qui contient le Link local et tous les puppets, était plein.
**Implémenté** :
- `heap_grow.h` : tas « MpPuppetHeap » de 32 Mio en fin d'arène sead, créé avec le tas des skins
  (qui passe à environ 222 Mio) ;
- `actor_sync.h as_solid_heap` : un puppet (`puppet_is`, processus 0xA8) construit son tas dans
  le tas des puppets (`fopAcM_entrySolidHeap_` 025D6174 avec ce parent). Besoin = taille notée pour
  son skin + 25 %, sinon 2 fois un Link, sinon 1 Mio. Sans place, refus journalisé (« puppet not
  made, the puppets' heap is full »). Fonctionne aussi avec la synchro d'acteurs désactivée ;
- `as_uses_game_heap0` : pour les acteurs du tas 0, le contrôle de place se fait sur le tas 0.
Capacité attendue : environ 30 puppets. Le tas 0 du jeu (10 Mio) ne sert plus qu'au Link local et
aux autres acteurs du tas 0.
Mod 0.3.54 compilé (117 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.55 : 16 joueurs

**Testé, 0.3.54** (l'utilisateur) : 12 skins chargés et portés en même temps (bots), sans crash.
**Mesuré** (lecture seule, `shader_gain.py`) : tous les skins des instances 1 et 3 sont déjà
allégés, sans aucun shader du jeu : 11,8-14,1 Mo, 18,8 Mo pour « 3 », saria_in_win et toad
compris.
**Limites relevées** :
- `MAX_REMOTES` 8 → 15 (16 joueurs). Tableaux dimensionnés dessus : `remotes`, `markers`,
  `dmap_points`, `heavy.h hv_remote_shown`, listes d'`elite.h` ;
- `PUPPET_MAX` 16 → 24 (joueurs, corps et bots) ;
- `SWING_PLAYERS` 8 → 16 ;
- `TUNIC_MAX_SLOTS` 4 → 8 (environ 0,4 Mio de tas du mod par emplacement ; au-delà, tunique d'origine) ;
- inchangés : `SHIP_PUPPET_MAX` et `BOAT_CLONE_SLOTS` 4 (4 bateaux d'autres joueurs affichés). Un
  bateau-puppet prend sa place dans le tas de jeu 1 (10 Mio, partagé avec les acteurs) : à mesurer
  avant de les relever ;
- pas de limite de joueurs trouvée dans les sources Java du serveur.
**HUD** : tout le HUD du mod est une seule liste. Le runtime jette une liste entière au-delà de sa
limite (`guest_hud.h`), et un pseudo de 16 lettres coûte 104 éléments (4 bandes de dégradé).
- `name_tag.h` : au-delà de `NAME_FULL_NAMES` (6) pseudos visibles, une seule passe de
  remplissage par pseudo (2 × lettres éléments). 15 pseudos tiennent même sous l'ancienne limite de
  1024 ;
- runtime `guest_hud.h` : `kMaxCommands` 1024 → 4096 et `kMaxVertices` 32768 → 131072 (32 par
  élément comme avant). `guest_hud_test` recompilé et OK.
Capacité mémoire : tas des skins 222 Mio, soit environ 13 skins allégés distincts (au-delà, Link) ;
tas des puppets 32 Mio, soit environ 30 Links.
Mod 0.3.55 compilé (117 hooks), vérifications OK, installé instances 1 et 3. `wwhd.exe` **non
relinké** : le jeu tournait (fichier verrouillé). **Non testé en jeu.**

## 0.3.56 : objets levés visibles, rubis partagés, colère des cochons, bandeau du launcher

Runtime de dev relinké (`wwhd.exe`, limites HUD 0.3.55 incluses).
**Launcher** : bandeau de la page 1 (`homeHero`) rétréci. Marges 16/14 au lieu de 34/30, titre 32 px
au lieu de 43 (`launcher.css`, deux copies), soleil et voile plus petits. Compilé (javac).
**Vérifié dans les sources** :
- `daPy_lk_c::dProcGetItem_init` (024216C8, `d_a_player_main_05.cpp`, un argument) : procédure
  DEMO_GET_ITEM (0xAE), objet dans `mProcVar2` (+0x691A, SDK ; -1 sans objet) ;
- numéros des rubis (`d_item_data.h`) et montants (`d_item.cpp item_func_*_rupee`) : 0x01 1,
  0x02 5, 0x03 10, 0x04 20, 0x05 50, 0x06 100, 0x0F 200 ;
- colère des cochons = un seul drapeau pour tous, `l_kbStatics.ALL_ANGER` (données 101B7FA0,
  `d_a_kb.cpp`). Mis à 1 quand un cochon tombe à 0 PV (`d_a_kb_exec.cpp`), tous les cochons non
  touchés attaquent alors, remis à 0 par `attack_move`.
**Implémenté** (`item_share.h`) : transport par le paquet de cinématique (déjà relayé à tous par le
serveur), noms réservés commençant par « # », sans changement de serveur ni de protocole.
`event_sync.h ev_received` les passe à `share_received` :
- « #GET_xx » : hook de retour sur `dProcGetItem_init` pour le Link local. Chez les autres :
  l'icône du jeu (`sv_icon`) au-dessus du pseudo de ce joueur pendant 4 s (`mod.c draw_markers`).
  Pour un rubis, le même montant à chaque autre joueur connecté, où qu'il soit
  (`PLAY_ITEM_RUPEE_COUNT`). Les rubis sont locaux dans la synchro de sauvegarde : pas de double ;
- « #PIG_1 » / « #PIG_0 » : chaque changement du drapeau local est envoyé. Reçu dans le même stage
  et la même room, il est appliqué et n'est pas renvoyé.
Mod 0.3.56 compilé (118 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Un client 0.3.50 ou plus ancien traiterait « #PIG_x » comme une cinématique : tout le monde doit
avoir la 0.3.56.

## 0.3.57 : objet levé envoyé une fois, montant des rubis affiché, pot sur le joueur

**Testé, 0.3.56** (instances 1 et 3) : `dProcGetItem_init` est rappelée pendant toute la levée (elle
rend TRUE aussitôt quand la procédure est déjà DEMO_GET_ITEM). Le hook envoyait à chaque appel :
pour un coffre de 100 rubis (objet 6), l'instance 3 a reçu « rupees shared 100 » au moins 12 fois.
Aucune icône vue : un rubis n'a pas d'icône d'inventaire (`itemForSlot`).
**Implémenté** :
- `item_share.h` : un hook d'entrée garde la procédure d'avant l'appel. L'envoi n'a lieu qu'au
  passage à DEMO_GET_ITEM ;
- `mod.c draw_markers` : pour un rubis, « +montant » en doré au-dessus du pseudo à la place de
  l'icône.
**Pot sur le joueur** (l'utilisateur : en chargeant une salle où se trouve un joueur, un pot est
dessiné sur lui). **Déduit** : le paquet de création du serveur porte les dernières valeurs
connues de l'acteur, dont le bit « porté » (0x2000) d'un portage terminé depuis (pot lancé, cassé,
recréé avec la salle sous la même identité). `as_carried_place` collait donc le pot aux mains du
puppet de son propriétaire. **Implémenté** (`actor_sync.h`) : à la liaison d'un acteur posé, le
bit « porté » des valeurs du serveur est retiré. Seules les mises à jour suivantes du propriétaire
le remettent.
Mod 0.3.57 compilé (119 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.58 : détection de l'objet levé à chaque pas

**Testé, 0.3.57** (instances 1 et 3, rubis orange) : aucun envoi, pas de ligne « item held up ». Le
filtre « procédure différente de DEMO_GET_ITEM avant l'appel » excluait tous les appels.
**Déduit** : le jeu fait entrer Link dans 0xAE avant d'appeler `dProcGetItem_init`.
**Implémenté** (`item_share.h share_held_up`) : plus de hooks sur 024216C8. À chaque pas du Link
local, après son execute (`share_step(i_this)` dans `mod.c`), l'objet (`mProcVar2`) est envoyé une
seule fois, au pas où `mCurProc` devient 0xAE.
Mod 0.3.58 compilé (117 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.59 : icône à 40 m, bombes et flèches partagées (coffre et achat)

**Testé, 0.3.58** : les rubis d'un coffre sont partagés une fois. L'objet n'apparaît pas au-dessus
du puppet.
**Déduit** : le marqueur d'un joueur, pseudo et icône, n'existe qu'à moins de `NAME_HIDE_DISTANCE`
(10 m) (`mod.c update_markers`). L'option « cacher les pseudos » cachait aussi l'icône.
**Implémenté** :
- `mod.c` : `marker.name_shown`. Le marqueur va jusqu'à `SHARE_ITEM_DISTANCE` (40 m) pendant
  qu'un objet levé est affiché, le pseudo reste à 10 m. L'icône ou « +montant » se dessine même
  avec les pseudos cachés ;
- partage des munitions. **Vérifié** : `d_item_data.h` (0x0B-0x0E bombes 5/10/20/30, 0x10-0x12
  flèches 10/20/30), `d_item.cpp` (`item_func_bomb_*` donne aussi le sac de bombes), achat chez
  Beedle par `execItemGet` (0254DA38, `d_a_npc_bs1_msg.cpp`), inventaire `mItems[21]` à
  info + 0x3C (`d_save.h`), emplacements arc 0xC et bombes 0xD (`d_item.h`) :
  - « #GET_xx » (objet levé, coffre) : icône, plus rubis ou munitions chez les autres ;
  - « #GIVE_xx » : depuis `bonus.h bonus_exec_item_get` (remplacement existant d'`execItemGet`),
    un rubis ou des munitions reçus hors levée (proc ≠ 0xAE) et hors ramassage au sol
    (`bonus_ground_pickup`, compteur posé autour de `daItem_c::itemGetExecute` 0217DE18) :
    achat ou cadeau ;
  - réception (`share_give`) : rubis par le compteur ; bombes ou flèches seulement si le joueur
    a déjà le sac ou l'arc, par `bonus_exec_item_get` lui-même (stocks et plafonds des flèches
    élémentaires) ; `share_receiving` empêche le renvoi.
Mod 0.3.59 compilé (117 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.60 : modèle 3D de l'objet levé au-dessus du puppet

Demande : remplacer l'icône par le modèle de l'objet qui tourne, comme dans le jeu.
**Vérifié** (décompilé `d_a_demo_item.cpp`, `f_op_actor_mng_3.cpp`, code HD généré) :
- l'objet levé du jeu est `daDitem_c` (process `fpcNm_Demo_Item_e` 0x101), créé par
  `fopAcM_createDemoItem` (025D7D40) avec `item | bitNo<<8 | argFlag<<16` ;
- `mArgFlag` +0x76D (lu par `set_pos` 021236A8). `FLAG_UNK01` : son `Delete` n'appelle pas
  `execItemGet` (aucun objet donné). `FLAG_UNK08` : position `home.pos + mOffsetPos` (+0x760)
  au lieu du joueur 0 ;
- `daItemBase_c::show` (021842C8) et `dead` (0218432C) posent les bits 1 et 2 de +0x74F.
**Implémenté** (`item_share.h`) :
- à la réception de « #GET_xx », si le puppet de l'expéditeur existe, création d'un `daDitem`
  (drapeaux 0x09, `as_context = AS_CTX_NONE` pour ne pas le synchroniser) ;
- à chaque pas : `home.pos` = position du puppet + (0, 130, 0), affiché ; fin (`dead`) après
  `SHARE_ITEM_STEPS` ou si le puppet disparaît ;
- l'icône n'est plus dessinée quand le modèle existe (« +N » des rubis reste). Sans puppet
  (joueur loin ou puppets désactivés), l'icône reste.
Mod 0.3.60 compilé (117 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Risque : un `daDitem` charge l'archive de l'objet ; un objet dont l'archive n'est pas en mémoire
peut ne pas s'afficher ou mettre du temps.

## 0.3.61 : objet levé à 0,5 s, retiré quand la main se baisse ; dist refaite (21 octobre)

**Implémenté** (`item_share.h`) :
- le modèle (et l'icône de repli) apparaît `SHARE_SHOW_DELAY` (15 pas, 0,5 s) après « #GET_xx » ;
- l'expéditeur envoie « #END » quand son proc quitte DEMO_GET_ITEM (0xAE) après un « #GET » envoyé :
  le modèle (`dead`) et l'icône disparaissent chez les autres. Sécurité : 30 s au plus
  (`SHARE_ITEM_STEPS` 900), puppet disparu. Un client 0.3.60 ou plus ancien ignore « #END ».
**Dist refaite à la demande** :
- runtime `build/rel` (ninja, puis relink `-d keeprsp`), `package.py` vers `dist/_rel` (le Python
  MSYS2 veut des chemins Windows pour `--license`), `guard.py` OK (292 fichiers), zip remplacé ;
- `multiplayer-connection-0.3.61.wwhdmod` (remplace 0.3.18), `LISEZMOI.txt` mis à jour ;
- `WWHD-Online-Launcher.jar` : toutes les sources Java recompilées (`--release 17`, sans erreur),
  jar complet (JavaFX, JNA, ressources, MMBSystem). Testé : serveur démarre (multiplicateurs, skins
  sur 10015), launcher ouvert (JavaFX). Le dossier `dist/WindWakerHD-v0.3.0-online-windows-x86_64`
  (installation de test, 1,3 Go) n'est pas touché.
Mod 0.3.61 compilé (117 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.62 : crashs de la session entre amis (21 octobre), cinématiques par ennemi

**Constat** : le dossier partagé aux amis (`WWHD ONLINE 2.rar`, copie de
`dist/WindWakerHD-v0.3.0-online-windows-x86_64`, l'installation de test du 18 octobre) a le
runtime `2db0c10-dirty` (`data/bin/wwhd.exe` du 18 octobre), pas celui du zip refait le 21. Sans :
MEM2 jusqu'à 0x60000000 (journal « [mem] default heap 104DB000-50000000 » ; tas des skins
19 Mio libres au lieu d'environ 220), interpolation du HUD (pseudos à 30 im/s), limites HUD 4096.
Les skins absents et les pseudos à 30 im/s viennent probablement de là (déduit, non vérifié en jeu).

**Crash 1 (5 fois, 16:55-17:06)** : « PROGRAM HALT d_path.cpp:31 0 <= pnt_index &&
pnt_index < path->m_num », juste après la téléportation du mod vers un joueur à la Forteresse
(MajyuE, majroom). **Vérifié** (pile EU convertie) : fopAc_Create 025D4C70 -> daObjMovebox
Mthd_Create 0237A468 -> Act_c::path_init 02377078 -> dPath_GetPnt 025AAEB8. path_init prend le
point du chemin d'un bloc poussable dans deux switchs sauvegardés (progression partagée) ; un
point absent du chemin de ce jeu. **Implémenté** : `path_guard.h`, `WWHD_REPLACE` de
dPath_GetPnt (2 arguments vérifiés dans le code généré, m_num u16 à +0) : un index hors du chemin
est ramené au point le plus proche (log « path point outside its path »). Chemin nul ou vide :
l'original, comme avant.

**Crash 2 (17:28, TyuTyu)** : lecture de 0x4 dans `multiplayer-connection.dll`. **Vérifié** :
adresse du mod 7F024988 (module.c traduit + symboles de mod.elf via guestmod.Translator, base
0x7F000000), `as_step` lit `actor + AS_PC_ID` d'un `as_slot` sans acteur. Cause : `as_handle_remove`
marquait « removed » une copie pas encore créée (`AS_SPAWNING`, actor 0). **Corrigé** : copie non
demandée oubliée, demandée marquée `dropped` et supprimée dès sa création (`as_bind_replica`) ;
`as_step` libère un slot sans acteur.

**« Guest mod memory region is full »** (journal d'un ami, mod non chargé) : **vérifié**
(`packages.cpp assign_guest_region`) : région 7F000000-80000000. Le mod (code + données + tas
8 Mio) était à 0xA20000, story-notebook réservé à 7FA00000 derrière une ancienne version du mod
de 10 Mio : plus de place. **Corrigé** : tampon `incoming` 1 Mio -> 256 Kio (trames du serveur de
quelques Kio ; une trame plus grande resterait bloquée dans la file, `network_transport.cpp`).
Région 0x960000 (0.3.62).

**Pot sur un puppet** : **implémenté** (`actor_sync.h as_carry_trusted`) : le bit de port 0x2000
n'est cru que si le propriétaire a envoyé une position depuis moins de 1,5 s ou si son Link est dans
un proc de port (0x6E-0x74, d_a_player_main_0x.h). Déduit, non testé.

**Cinématiques** (demande : seules celles lancées par un ennemi téléportent) : **vérifié**
(`d_com_inf_game.h` PLAY_EVTCTRL 0x51D0, `d_event.h` mPt1 +0xC4 / mPt2 +0xC8, `d_event.cpp`
setParam appelé juste après `order`, startProc plus tard dans runProc). `ev_start` n'envoie que si
l'un des deux acteurs de l'ordre est du groupe ennemi (2).

Non traité : tuniques pyjama/héros (besoin d'un journal ou d'une description précise).
Mod 0.3.62 compilé (118 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Dist non modifiée.

## 0.3.63 : vêtements du jeu seulement, rubis ramassés, RESET THIS ROOM en cinématique

**Pyjama / tunique** (décision de l'utilisateur après la session du 21 : textures mélangées au
hasard au passage pyjama -> tunique) : plus de choix. `outfit_wanted` toujours `OUTFIT_AUTO` (le
drapeau du jeu, posé par playerInit selon l'event 0x2A80, que la progression partagée donne à tous
dès qu'un joueur l'a), `outfit_puppet_begin/end` supprimés (les puppets ne réécrivent plus les
vêtements de leur joueur au dessin). Gardé : la réécriture de la table partagée selon le drapeau du
Link local à chaque pas (réparation de la note 0.3.27). Launcher : case pyjama toujours
verrouillée, cochée tant que `hero_clothes=0`, décochée dès `hero_clothes=1` ; le profil écrit
`outfit=auto`. Compilé (javac).

**Rubis en l'air au-dessus des puppets** : **vérifié dans le code** : un objet *placé* (rubis du
stage, proc 0xFF) ramassé n'envoyait aucune suppression (`as_on_deleted` : ennemis morts et pots
seulement), et un objet placé reçu « removed » n'était supprimé que si `health > 0` (règle des
ennemis) : la copie restait à la dernière place reçue, celle de la montée de l'objet ramassé
au-dessus du joueur. **Corrigé** : ramassé près du Link local -> `client_remove` ; reçu -> supprimé
tout de suite.

**RESET THIS ROOM** : refusé pendant un event (« busy »). Autorisé maintenant (journal « room reset
during an event »), pour sortir d'une cinématique bloquée ; toujours refusé pendant un changement
de stage, sur le bateau ou en fantôme. Déduit, non testé : le chargement du stage remet le
contrôle d'event à zéro.

Mod 0.3.63 compilé (118 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.64 : armes des ennemis et des pots synchronisées, pas de port en cinématique

**Port d'un joueur** : `carry.h cy_lift_target` ignore un puppet dont le proc reçu
(`remote_proc`) est un proc de cinématique (daPyProc_DEMO_TOOL 0xA9 à DEMO_ICE_SLIP 0xDA,
`d_a_player_main_0x.h`). Déjà en place : pas de port quand le Link local est en event
(`cy_can_act`), un joueur porté dont l'event commence est lâché (`cy_place_local`, MP_CARRY_JUMP).

**Armes** (`daBoko_c`, process 0x1CF, `d_a_boko.cpp`). **Vérifié** : `mNowMode` u8 à +0x430
(code généré 020C19E4), modes WAIT 0, MOVE 1, ENEMY_CARRY 2, PLAYER_CARRY 3, THROW 6
(`d_a_boko.h`). Pots : `d_a_tsubo.cpp spec_make_boko` crée les bâtons avec le pot (dans chaque
jeu), portés (`setCarryNow`), lâchés à sa suppression avec une vitesse aléatoire (`spec_remove`).
**Implémenté** (`actor_sync.h`, classe `AS_CLS_BOKO`, nom « Boko » dans la fabrique « Spawned »,
sans changement du serveur) :
- tenue par un ennemi ou un pot : propre à chaque jeu ;
- tombée : partagée par le jeu propriétaire de l'ennemi (créée en contexte OWNED) ou celui qui a
  cassé le pot (`as_pot_break_note`, 3 s, 2,5 m) ; les autres suppriment la leur (`as_kill`) ;
  arme sans origine connue (posée dans le stage, ennemi non synchronisé) : locale ;
- copie : suit la place du propriétaire, dans les mains de son puppet quand il la porte (variant
  bit 0 = PLAYER_CARRY, `as_carried_place`) ;
- copie ramassée par le Link local : supprimée pour tous, annoncée de nouveau depuis ce jeu ;
  copie prise par un ennemi local : devient la sienne ; arme du propriétaire reprise par son
  ennemi : supprimée pour tous ;
- coups d'une copie : comme les autres armes (`as_weapon_of`), laissés aux strikes du propriétaire.
Mod 0.3.64 compilé (118 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Risques : la rotation d'une copie portée vient de l'arme du propriétaire (pas de la main du
puppet) ; un ennemi d'un autre joueur reste sans arme dans les autres jeux après l'avoir perdue.

## 0.3.65 : tonneaux multipliés par joueur, puppet caché sous un tonneau

**Vérifié** : le tonneau où l'on se cache est `daObjBarrel` (process 0x1C8, fpcNm_Obj_Barrel_e de
`d_a_player_main_01.h`, archive Ktaru_01, `d_a_obj_barrel.cpp`) ; « Ktaru » est dans la liste du
serveur (`actorCreators.ktml`, comme kotubo : synchronisé avec le bit de port).
**Implémenté** :
- `as_stage_create` : un tonneau (nom du stage cherché dans la table du jeu, `dStage_searchName`
  025C109C, process s16 à +8) est créé `mp_player_count()` fois (joueur local + joueurs connectés,
  16 au plus), copies en anneau de 1,3 m (`as_copy_spread`), même mécanisme que les ennemis
  (copie numérotée, synchronisée par son numéro). Le nombre est fixé au chargement du stage dans
  chaque jeu : un joueur arrivé plus tard peut avoir plus de tonneaux (les siens en trop restent
  locaux) ;
- `as_step` liste à chaque pas les joueurs dont le tonneau synchronisé est porté
  (`as_carried_by_other`, donc `as_carry_trusted`) ; `puppet_sense.h sense_nearest_puppet` ignore
  leurs puppets : ennemis et habitants ne les voient plus (le Link local sous son tonneau est géré
  par le jeu lui-même).
Mod 0.3.65 compilé (118 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## 0.3.66 : interrupteurs de la room partagés, retour au dernier endroit

**Interrupteurs** (`switch_sync.h`). **Vérifié** (`d_save_5.cpp`, `d_save_readable.h`) : switchs
0-127 = mémoire du stage (sauvegardée, déjà partagée par la progression), 128-191 = donjon
(`dSv_danBit_c`, non sauvegardés), 192-239 = zone de la room (`dSv_zoneBit_c`). Ces deux derniers
ne quittaient jamais un jeu. **Implémenté** : hooks d'entrée sur onSwitch 025B9E38, offSwitch
025B9F7C, revSwitch 025BA20C (this, numéro, room ; absents des differing_functions) : un vrai
changement (isSwitch 025BA0C0 avant) d'un switch 128-239 de `dComIfGs_info` (sauvegarde + 0x20)
part en « #SW1_nn_rr » / « #SW0_nn_rr » (transport d'item_share.h, relayé tel quel par le
serveur). Reçu : même stage (donjon) et même room (zone) -> onSwitch / offSwitch du jeu ; pas
renvoyé pendant 1 s (rebond d'un interrupteur que l'autre jeu remettrait à sa valeur), 8 envois
par pas au plus.

**Retour au dernier endroit** (`last_place.h`) : toutes les 5 s en ligne (hors bateau, mort,
event, téléportation), stage / room / couche / position / direction écrits dans
`last_place.bin` (dossier Data du mod) avec le serveur (host:port). À la connexion suivante au même
serveur, 2 s après, hors event : `teleport_go` (la téléportation de la carte, sans prix), sauf si
déjà dans cette room à moins de 10 m. Le fichier n'est réécrit qu'après cette tentative (abandon
après 30 s).

**Couleur du bateau** (question) : vérifié dans le code : le bouton APPLY BOAT COLOUR écrit
`ship_color` tout de suite (`writeHdSettings`, seulement si le launcher est lié au jeu par BROWSE),
le mod relit launcher.cfg chaque seconde (`refresh_launcher_settings`) et repeint en direct ;
la couleur part aussi aux autres (`ship_puppet.h` MP_SHIP_COLOR). Cause non trouvée sans journal.

Mod 0.3.66 compilé (121 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**

## Launcher : téléportation bloquée « A teleport is already on its way » (21-22 octobre)

**Vérifié dans le code** : le launcher garde `teleportPending` jusqu'à une réponse finale du jeu
(players.txt « teleport=id|résultat »), sans limite de temps. Le mod ignore exprès une demande
écrite avant sa première lecture de teleport.cfg (`mod.c` : « older than this game », jeu qui
démarre ou qui charge) et ne répond rien quand il est fermé : le launcher restait bloqué jusqu'à
son redémarrage. **Corrigé** (`LauncherUI`) : `teleportSince` ; sans réponse en 10 s (60 s après
« loading »), la demande est oubliée avec un message, une nouvelle peut partir. Compilé (javac).
Non testé.

**Couleur du bateau** : le problème venait de l'ancien build de la dist (runtime du 18 octobre),
d'après l'utilisateur.

## 0.3.67 : launcher lié par le dossier de la recomp, launcher.cfg toujours présent, statut sur les vagues

- Launcher : BROWSE choisit le dossier de la recomp (celui de Wind Waker HD.exe), plus le fichier.
  `configFromGameFolder` : `<recomp>/data/user/ModManager/...` (release portable),
  `<instance>/user/ModManager/...` (instances de dev), ou un dossier déjà à l'intérieur ; une
  recomp jamais lancée (Wind Waker HD.exe ou data/bin présents) : le chemin data/user. Le
  `launcher.cfg` absent est créé (`HdModBridge.update` crée dossiers et fichier). Les messages
  « BROWSE → launcher.cfg » disent maintenant « the recomp folder ». Un lien déjà enregistré (chemin
  du fichier) reste valable.
- Mod : `read_launcher_cfg` écrit un launcher.cfg vide (en-tête) quand il manque.
- Barre latérale : la carte STATUS (Server / Game, Connected) est posée sur les vagues, le nom
  « Wind Waker Online » en dessous.
Mod 0.3.67 compilé (121 hooks), installé instances 1 et 3 ; launcher compilé (javac). Rendu de la
barre latérale non vérifié visuellement.

## 0.3.68 : une seule cinématique pour un ennemi et ses copies du multiplicateur

**Constat** : un ennemi qui lance sa propre cinématique (Bokoblin, `d_a_bk_exec.cpp demo_camera` :
`fopAcM_orderPotentialEvent` à chaque pas tant que `eventInfo.checkCommandDemoAccrpt()` est faux ;
intro m1234 = 1, caméra de mort m1234 = 50) la lançait une fois par copie, jouées à la suite.
Idée de l'utilisateur (Twilight Princess : mettre les copies en pause) discutée : une pause seule
ne suffit pas, la copie relâchée redemande sa cinématique.
**Vérifié** : tout ordre passe par `dEvt_control_c::order` (0253EC0C, `d_event.cpp` : this, type,
priorité, flag, hind, acteur 1, acteur 2, index d'event ; 9e argument sur la pile, donc hooks
d'entrée et de retour, pas de remplacement) ; `mOrderCount` s8 à +0xC0 (code généré), « plein » à
8 = refus ; `eventInfo.mCommand` u16 à +0xF8 de l'acteur, INDEMO = 2 (`f_op_actor.h`) ;
`dComIfGp_event_reset` est inline : play + 0x52B8 |= 8.
**Implémenté** (`actor_sync.h`, gd_*) pour un acteur d'un groupe multiplié (as_groups /
as_enemy_copies) avec au moins 2 membres vivants :
- rassemblement : ordres refusés 0,5 s ou jusqu'à ce que tous les membres vivants demandent ;
- un seul joue : le membre qui demande à la fin du rassemblement mène, son ordre passe ;
- les autres suivent : tant que le meneur est INDEMO, chaque suiveur reçoit INDEMO avant son
  execute (il joue sa partie avec le meneur) ; son event_reset est annulé après son execute ;
- après : commande des suiveurs remise à NONE, même ordre du groupe refusé 5 s (au-delà une copie
  qui redemande joue la sienne : jamais bloquée pour de bon).
Mod 0.3.68 compilé (123 hooks), vérifications OK, installé instances 1 et 3. **Non testé en jeu.**
Risque : un ennemi dont la cinématique attend des données d'event (staff de l'event manager) peut
rester figé comme suiveur jusqu'à la fin de la cinématique du meneur.

## Launcher : RESET BOAT COLOUR

Bouton à côté d'APPLY BOAT COLOUR : `HdModBridge.remove` retire `ship_color` de launcher.cfg ; le
mod (relecture chaque seconde) n'a plus de couleur, `boat_refresh` remet les textures d'origine
(« original boat paint restored », boat_paint.h). Le sélecteur revient au rouge d'origine.
SAVE PROFILE n'écrit plus `ship_color` quand il n'y en a pas et que la couleur est restée le rouge
d'origine (sinon le reset serait annulé par un rouge « personnalisé »). Compilé (javac), non testé.

## 0.3.69 : tenue du héros avec les textures du pyjama (analyse de fond), pot sur un puppet

**Vérifié** (`d_a_player_main_04b.cpp playerInit` 02410BE8, `d_a_player_main_04.cpp playerDelete`
0240EBF0) : la table de textures du modèle de Link est partagée par tous les Links. playerInit
copie les en-têtes ACTUELS de la table comme « originaux » (m48C), écrit le pyjama, puis rééchange
si la sauvegarde dit héros ; playerDelete rééchange si le Link est en pyjama. Pour un seul Link
la table revient toujours propre. Les puppets sont déjà isolés : `puppet_phase_2` (qui appelle
playerInit) et `puppet_delete` sauvent et remettent les en-têtes (`puppet_texture_table`).
Ce qui écrivait encore : `outfit.h outfit_local_step`, à chaque pas, des en-têtes appris une fois
par model data (avec le pointeur absolu de l'objet texture HD à +0x20), jamais réappris tant que
le pointeur de model data ne change pas : après un rechargement de l'archive Link à la même
adresse (changement de zone, tenue reçue), des en-têtes périmés réécrits en boucle. Déduit : c'est
la cause de « modèle du héros, textures du pyjama » (image de la session du 22 octobre, dist 0.3.61
qui avait encore aussi l'écriture par puppet).
**Corrigé** : `outfit.h` n'écrit plus rien (lecture du drapeau et de l'event 0x2A80 seulement) ;
les vêtements du Link local sont ceux du jeu.

**Pot sur un puppet** : la dist jouée (0.3.61) n'avait pas `as_carry_trusted` (0.3.62). Ajout
d'un journal « carry: a copy in a puppet's hands » (propriétaire, process, proc du propriétaire,
âge de sa dernière place, placé ou spawné), une fois par copie, pour trouver la cause si ça revient.
Mod 0.3.69 compilé (123 hooks), installé instances 1 et 3. **Non testé en jeu.**

## 0.3.70 : dernier endroit gardé par le serveur, chargement direct ; launcher (zoom, mode sombre)

**Dernier endroit** (demande : gardé par le serveur, seulement l'emplacement, pas la sauvegarde ;
au premier chargement, directement là-bas). Sans changer le format du protocole :
- client (`last_place.h`) : toutes les 5 s (hors bateau, mort, event, téléportation) un paquet de
  cinématique « #LP_ll » (couche en hex, place : stage, room, x y z, direction) ;
- serveur (`WWClient`) : « #LP » n'est relayé à personne, gardé par uid (8 octets de l'init) dans
  `User/LastPlaces.txt`, renvoyé à ce joueur seul à son CLIENT_INIT_PACKET (SERVER_PLAYER_EVENT,
  émetteur 0). Testé : compilé (javac) ;
- client : la place reçue est appliquée 2 s après la connexion (`teleport_go`, sans prix) si le
  Link n'est pas déjà dans cette room à moins de 10 m, et gardée dans `last_place.bin` (format LPL2,
  avec le serveur) ;
- premier chargement : **vérifié** (`d_s_name.cpp` changeGameScene 025ADC60 : `dComIfGs_gameStart`
  02522198 = setNextStage du return place de la sauvegarde, puis demande de la scène, puis
  `mRestartRoomParam = 0`) : hook d'entrée sur gameStart, hook de retour sur changeGameScene qui
  remet stage, place de reprise et paramètre comme teleport_go, d'après `last_place.bin` du même
  serveur, seulement pour un joueur en ligne (host dans launcher.cfg). Plus de second chargement
  quand la place est connue localement. Le mod ne se connecte qu'au premier pas de Link, d'où la
  copie locale.
**Launcher** : description ELITE ENEMIES raccourcie ; RESET THIS ROOM en rouge (`.danger-button`,
les deux launcher.css) ; carte : zoom 1x-4x à la molette autour du curseur, boutons − + 1:1,
déplacement à la souris (ScrollPane pannable), icônes des joueurs à taille fixe ; Play Online :
interrupteur « Dark mode » (préférence `darkMode`, `launcher-dark.css` ajouté par-dessus, couleurs
seulement). Compilé (javac). Rendu non vérifié visuellement.
Mod 0.3.70 compilé (125 hooks), installé instances 1 et 3. **Non testé en jeu.** Le serveur doit
être recompilé (dist) pour garder les places.

## 0.3.71 : pot de la femme d'Outset sur un puppet ; tenue du héros, textures du pyjama (suite)

**Pot** (cause trouvée par l'utilisateur : la femme d'Outset qui porte un pot sur la tête). Le pot
porté par un PNJ a le bit de port 0x2000 (setCarryNow du PNJ) ; le jeu propriétaire du pot
l'envoyait, les autres le plaçaient dans les mains du puppet du propriétaire. **Corrigé**
(`actor_sync.h as_carried_by_local`) : les bits de port (statut et variant d'un objet spawné) ne
sont envoyés que si le Link local porte cet objet (`mActorKeepGrab.mActor` +0x65A4, carry.h).
**Tenue / pyjama** (après 0.3.70, testé par l'utilisateur : Link local correct, puppets avec le
chapeau mais les textures du pyjama, des deux côtés). **Vérifié** : le changement de tenue chez
mémé se fait sur place par la cinématique (`d_a_player_main_03.cpp setDemoData`, ENABLE_SHAPE :
échange de la table avec les copies du Link et bascule du drapeau), pas par un rechargement ; les
puppets ne passent pas par là (`puppet_execute` : mise à jour visuelle seulement). Un Link local
avec skin change la table de son skin : la table partagée des puppets n'est changée par personne.
Déduit : c'est la cause quand les joueurs ont un skin. **Implémenté** :
- `outfit.h outfit_puppet_draw_begin/end` : autour du dessin d'un puppet sans skin, l'objet
  texture HD (+0x20) de chaque entrée de vêtements est celui de SES vêtements (drapeau du puppet) :
  l'objet du pyjama (getRes("Link", 0x71)) ou celui du héros, appris de l'entrée ou des copies du
  puppet (+0x48C) quand ce n'est pas celui du pyjama ; remis après (comme les copies de couleur
  de tunic.h, appliquées ensuite) ;
- `tunic.h tunic_reset_all` : au changement du drapeau pyjama du Link local, couleurs d'origine
  remises, copies et textures oubliées, retrouvées 2 rafraîchissements plus tard.
Mod 0.3.71 compilé (125 hooks), installé instances 1 et 3. **Non testé en jeu.**

## 0.3.72 : crash de la cinématique de groupe, porté collé au puppet, tonneaux, bâtons

**Crash 21:31 (instances 1 et 3, 0.3.70)** : lecture de 0 dans le hook de `dEvt_control_c::order`
(0.3.68), appelé par `fopAcM_orderPotentialEvent` (025D7B24) d'un Moblin (`daMo2_Execute`
021C9B1C). Vérifié avec le module.c de la build qui a planté (GuestBuild c707cc…, base 7F240000) :
la boucle qui retire le meneur des suiveurs, pointeur d'entrée invalide. **Réécrit** (indices,
bornes) et simplifié à la demande : seulement les copies d'un même ennemi placé (même point de
départ), le premier qui demande joue tout de suite (plus d'attente), les autres refusés et suiveurs.
**Porté par un joueur** : le puppet du porteur prend chaque place reçue sans lissage
(`cy_is_carrier`), et le Link local est remis dans ses mains juste après la mise à jour de ce
puppet (`cy_after_carrier` : décalage des mains depuis old.pos ajouté à la nouvelle place,
setWorldMatrix 023FF6A0).
**Tonneaux** : copie portée par un puppet posée à ses pieds (plus à hauteur des mains). Caché :
**vérifié** (`d_a_player_main_01.cpp checkGrabBarrelSearch` 023DCF08 : mActorKeepGrab.mID +0x65A0
cherché, Obj_Barrel 0x1C8) ; le puppet dont le joueur porte son tonneau reçoit la copie dans son
mActorKeepGrab (`as_grab_barrels`), les acteurs jugent avec leur propre code ; l'exclusion du
puppet de 0.3.65 est retirée.
**Bâtons** : copie portée par son propriétaire toujours crue (variant envoyé à chaque changement),
dans la main droite. Synchronisation des bâtons des pots non expliquée (instance 1 : « weapon
fell, shared », instance 3 : aucune copie vue) : journaux ajoutés (décision à la chute, copie
demandée, copie créée).
**Lerp (question)** : envoi sans lissage (positions brutes, joueurs 30 fois/s au plus, acteurs
toutes les 0,1 s) ; à la réception : joueurs lissés d'un tiers de l'écart par pas (mod.c
update_markers, rotation directe), acteurs de même (`as_step`, blend dt/0,1), puis
l'interpolation d'images du runtime entre deux pas.
Mod 0.3.72 compilé (125 hooks), installé instances 1 et 3. **Non testé en jeu.**

## 0.3.73 : caché sous un tonneau d'après l'état du joueur ; tenue des puppets d'après leur joueur

- Tonneau porté par un puppet : remonté d'un cinquième vers les mains (0.3.72 : aux pieds).
- Caché (0.3.72 testé : les monstres prenaient encore l'aggro) : nouveau bit d'extras
  `MP_EXTRA_BARREL` 0x08 (ActorPlayerSync, relayé tel quel par le serveur, `extras & 0x7F`), mis
  par le jeu du joueur quand son Link tient un tonneau (le test de checkGrabBarrel :
  mActorKeepGrab.mActor +0x65A4, process 0x1C8). Tant qu'il est là, `puppet_sense.h` n'offre ce
  puppet à aucun acteur.
- Tenue (testé 0.3.71 avec skins : joueur en tunique, puppet en pyjama des deux côtés). Cause
  déduite : le drapeau d'un puppet est fixé une fois à sa création (playerInit) ; le cadeau de
  mémé change la tenue sur place, sans recréer le puppet. **Corrigé** : `outfit_puppet_draw_begin`
  prend la tenue envoyée par son joueur (`MP_EXTRA_CASUAL`, à chaque mise à jour) pour les textures
  et met le drapeau du puppet à l'unisson (chapeau et boucle, daPy_lk_c::draw). Indépendant des
  savestates.
Mod 0.3.73 compilé (125 hooks), installé instances 1 et 3. **Non testé en jeu.**

## 0.3.74 : la tenue des puppets AVEC skin (cause des échecs 0.3.69-0.3.73)

Toutes les corrections de tenue depuis 0.3.69 ne s'appliquaient qu'aux puppets SANS skin
(`puppet.h` : `if (!skinned) outfit_puppet_draw_begin`). Les tests de l'utilisateur se font avec
des skins : un puppet avec skin dessine avec la table de textures de SON skin, que son playerInit
a réglée une fois (pyjama à l'époque), jamais changée ensuite. **Vérifié** (`skin.h` skin_get_res
026066C4 / skin_key_for) : pendant le code d'un Link avec skin, getRes("Link", i) lit l'archive du
skin ; son objet pyjama est donc le 0x71 de l'archive du skin. **Implémenté** :
`skin_casual_object` (getRes de l'archive « MpSk_<id> », 0x71, objet HD +0x20) ;
`outfit_puppet_draw_begin(puppet, casual, objet pyjama)` appelé pour TOUS les puppets, avec l'objet
de Link ou celui du skin, objets du héros appris par model data (16 au plus). Journal « clothes:
puppet model data / casual wanted / casual object / clothes entries listed » à chaque changement.
Plan B proposé par l'utilisateur si ça ne suffit pas : recharger les textures du skin au
chargement de la salle suivante.
Mod 0.3.74 compilé (125 hooks), installé instances 1 et 3. **Non testé en jeu.**

## Launcher : barre latérale avec icônes, Serveur à part

Les numéros 01-06 des boutons de navigation sont remplacés par des icônes vectorielles (SVGPath
24x24 à 0,75 dans le rond doré `.nav-badge` ; `.nav-icon` dorée, bleu foncé sur la page active,
les deux launcher.css) : lecture (Play Online), bateau (Prepare Your Voyage), repère (World Map),
sac (Shop), insecte (Debug), serveur (Server). Le bouton Server est à part, sous un titre
« HOSTING », tout en bas juste au-dessus du bloc STATUS. Compilé (javac) ; rendu non vérifié
(capture d'écran impossible depuis la session de l'outil).

## Launcher : logo

`images/logo.png` (1254x1254, fourni par l'utilisateur) remplace la Triforce en haut à gauche
(`createLogo`, 48 px, chargé au double pour les écrans haute densité) et sert d'icône à la fenêtre
et à la barre des tâches (`stage.getIcons()`, 16 à 256 px). `images/logo.ico` (16-256) généré pour
un raccourci Windows (le launcher démarre par un .bat, qui ne porte pas d'icône). Compilé (javac).

## Launcher : police Sherwood

`SherwoodRegular.ttf` (famille « Sherwood », 45 Ko, fournie par l'utilisateur) chargée une fois au
démarrage (`Font.loadFont`) et appliquée par le CSS (les deux launcher.css) aux titres : barre
latérale (titre, pages, sections, signature), bannière, titres de section et de cartes. Textes
courants inchangés. Coût : négligeable (45 Ko lus une fois). Remarque : launcher.css commence par
un `@import` de Google Fonts (Barlow Condensed, Nunito) ; JavaFX va chercher ce fichier sur
Internet au démarrage et ne sait pas lire les polices woff2 qu'il désigne : sans effet probable,
et une attente possible hors ligne. Non modifié (à décider). Compilé (javac), rendu non vérifié.
Ajusté à la demande : Sherwood seulement pour le titre en haut à gauche (« WIND WAKER » /
« ONLINE RECOMP », `.brand-title`, `.brand-subtitle`), le reste revient à la police d'avant.

## Launcher : bouton SAVE en bas, signature retirée

- Barre latérale : signature « WIND WAKER ONLINE / HD recomp multiplayer » retirée, bloc HOSTING +
  STATUS descendu (marge du bas 10 au lieu de 22).
- Bas de page : le chemin du jeu (« Game linked: … ») remplacé par un bouton SAVE
  (`saveAllProfiles`) : réglages du serveur (préférences des mutateurs, `launcherSavers`), puis,
  si le jeu est lié, connexion (Play Online) et profil (Prepare Your Voyage) écrits ensemble dans
  launcher.cfg (`gameSavers`). Les pages étant refaites à chaque visite, une seule source par page
  (la dernière). Les boutons de chaque page restent. Compilé (javac), rendu non vérifié.

## Launcher : notifications par-dessus

`showToast` mettait la notification dans le bas du BorderPane racine (`root.setBottom`) : elle
prenait de la hauteur à toute la mise en page et remontait le bas de la barre latérale. Maintenant
la scène a pour racine un StackPane (mise en page + notification), la notification est par-dessus,
en bas à droite (60 px au-dessus du bas, au-dessus du bouton SAVE), transparente à la souris, et ne
déplace plus rien. Compilé (javac), rendu non vérifié.

## Launcher : link.png et red.png transparents

Les aperçus du Link et du bateau (images/link.png, sans canal alpha, et images/red.png) avaient un
fond blanc opaque, visible en mode sombre. Fond rendu transparent (remplissage depuis les bords
sur le blanc, tolérance 40, puis passage du blanc à la transparence sur une bande de 2 px pour
garder des contours lisses sans halo ; les blancs internes, yeux, dents, écume, restent). Les deux
copies de resources remplacées ; originaux gardés dans le scratchpad (img_backup). Le recoloriage
du launcher garde l'opacité (`pixel.getOpacity()`). Aperçu sur fond sombre vérifié.

## Launcher : le jeu trouvé quel que soit l'ordre de lancement

Avant, le launcher ne regardait que le dossier lié (BROWSE) : un jeu lancé depuis une autre
installation (autre instance, la release) restait « Not running ». `findRunningGame` (toutes les
2 s, si le players.txt du jeu lié n'est pas frais) : processus `wwhd.exe` (ProcessHandle, chemin
de l'exécutable remonté jusqu'à un dossier de recomp, `configFromGameFolder`), puis les dossiers
connus (`knownGameFolders` : liés avant, instances voisines) ; celui dont players.txt a moins de
3 s devient le jeu lié (mémorisé, message « Game found »). Limite : le mod écrit players.txt depuis
le pas de Link (`map_step`) : à l'écran titre et au menu des sauvegardes le jeu n'est pas encore
vu. Compilé (javac), non testé.

## 0.3.75 : suivre le Chant de la Tornade d'un autre joueur

**Vérifié** (`d_a_player_main_07.cpp`, `d_a_ship.cpp`, code généré) : le Chant de la Tornade
(chant 1) en mer crée un cyclone `fpcNm_TORNADO_e` 0x1BB (paramètre 1, place de Link), son id dans
`daShip_c::mTactWarpID` (+0x714) ; la carte donne la destination `mTactWarpPosNum` (+0x6AC) ; puis
Link pagaie (`procShipPaddle_init` 023F1E60) et le bateau passe en MODE_TACT_WARP 14
(`mNextMode` +0x636 ; `procTactWarp_init` 0247DBF8 écrit `mCurMode` +0x635 = 14, vu dans le code
généré) ; `procTactWarp` (02482438) soulève le bateau puis `dStage_changeScene(destination + 0xC5)`.
**Implémenté** (`gale_follow.h`) : le bateau local qui passe en tact warp avec une destination
envoie « #GALE_xx » (transport d'item_share.h) ; les autres voient 20 s « <joueur> played the
Ballad of Gales: hold A to follow » avec une barre ; sur leur bateau en mer (stage « sea »,
embarqués, hors event), A tenu 1 s (`mItemButton` & 1) fait comme le chant : cyclone sur leur
Link, id et destination dans leur bateau, Link qui pagaie, bateau en MODE_TACT_WARP. Hors du
bateau : « be on your boat at sea to follow ». Un suiveur ne renvoie pas « #GALE ».
Mod 0.3.75 compilé (125 hooks), installé instances 1 et 3. **Non testé en jeu.** Risques : le
cyclone créé hors de la cinématique du chant (la caméra d'event du bateau, `shipFirstStartEventCamera`,
démarre sans event en cours) ; un appui sur A sur le bateau fait aussi ce qu'il fait d'habitude.

## Launcher : le curseur TIME OF DAY suit l'heure du jeu

Le curseur des mutateurs restait à 12 h ; seule la ligne « In game now » suivait le serveur.
`refreshWorldNow` (toutes les 0,5 s) le place maintenant sur l'heure du jeu quand elle est connue,
sauf pendant que l'utilisateur le déplace et 4 s après (souris, clavier : `timeSliderTouched`),
pour pouvoir choisir une heure et faire SET TIME. Compilé (javac).

## 0.3.76 : positions des joueurs interpolées (tampon)

Avant : la place affichée faisait un tiers du chemin vers la dernière reçue à chaque pas (retard
qui grandit avec la vitesse, dépassement à chaque arrêt, rotations appliquées d'un coup).
Maintenant (`mod.c` rs_*) : les 6 dernières places reçues gardées avec leur heure d'arrivée
(`rs_clock`, avancé de dt à chaque pas) ; le puppet est montré 75 ms dans le passé (`RS_DELAY`,
environ deux envois d'ActorPlayerSync), sur la droite entre les deux places qui encadrent ce moment,
rotations par le plus court chemin ; après la plus récente, il y reste (pas d'extrapolation : un
joueur arrêté n'envoie plus rien). Saut de plus de 15 m (téléportation, changement de zone) : pris
tout de suite. Le porteur du Link local : sa place la plus récente (0.3.72). Protocole et serveur
inchangés. Ennemis et objets synchronisés : lissage inchangé (as_step).
Mod 0.3.76 compilé (125 hooks), installé instances 1 et 3. **Non testé en jeu.**
