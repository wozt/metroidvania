# Metroid Vania — éditeur GTK4 de salles natives

## Ouverture d'une salle

L'onglet **Native rooms** liste les salles de Zero Mission identifiées dans la décompilation.
Cliquer une ligne ouvre sa **propre page centrale** (un double-clic et le bouton
Open ont le même résultat). Chaque salle possède un état de travail, un atlas
et une palette indépendants. Les pages sont réorganisables/détachables.

## Outils

- B : crayon (dessin continu sur les metatiles natifs)
- E : gomme (bloc 0)
- F : remplissage contigu
- I : pipette
- V : sélection rectangulaire ; cliquer dans la sélection puis glisser
  pour **déplacer les blocs** du calque actif (les cellules source sont vidées)
- H : main pour déplacer la vue sans modifier la salle
- G : afficher ou masquer la grille
- Ctrl+Z / Ctrl+Y (ou Ctrl+Maj+Z) : annuler et rétablir
- Ctrl+S : enregistrer la copie de travail

Les commandes de la barre d'outils utilisent des pictogrammes accessibles
et des infobulles affichées après **1 500 ms** de survol. Leurs groupes se
répartissent sur plusieurs lignes lorsque l'espace diminue.

BG1 et BG2 sont éditables séparément. La palette de metatiles apparaît dans
un onglet **Metatiles natifs** du panneau latéral ; cliquer un motif sélectionne
le bloc à appliquer.

## Protection des données originales

Le fichier enregistré reste privé :
`assets/extracted/overrides/metroid/<zone>_<index>.mvnative`.
Les fichiers de base `workrooms/`, les captures, la ROM et les sous-modules
de décompilation ne sont jamais modifiés. Fermer un onglet avec des changements
non enregistrés est refusé. La sauvegarde est propre à chaque salle.

## Limitations

La sélection déplace actuellement des blocs (pas des pixels individuels).
Les collisions, les entités, les musiques et BG0/BG3 ne sont pas éditables.
L'éditeur natif reste pour Zero Mission ; Aria nécessite un importeur distinct.
Les modifications de salle ne sont pas encore raccordées au jeu SDL3 principal.
