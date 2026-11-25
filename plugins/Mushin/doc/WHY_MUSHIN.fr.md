# Pourquoi Mushin ? — L'instrument de jeu live sur MPC

**Mushin** est un processeur créatif à 8 étages pensé pour transformer une piste ou un mix entier en matière vivante, jouable en temps réel sur MPC autonome (ARM32 et ARM64, MPC OS 3.10.1+).

Dans la tradition japonaise, le *mushin* (« l'esprit sans pensée ») désigne cet état de fluidité où l'on joue d'instinct, sans hésiter ni réfléchir à l'outil. C'est l'idée derrière Mushin : **sculpter le son, la tension, le pompage et le groove d'un seul geste.**

---

## 1. Jouer les transitions plutôt que les programmer

Mushin réunit dans un seul slot d'insert toute une chaîne musicale cohérente où **chaque étage nourrit le suivant** :
- **Saturation S-Curve & Exhaustion** (chaleur analogique jusqu'à l'écrêtage brutal, activable à la volée)
- **Double filtrage expressif (`CLN` / `VOWL` / `ACID` / `DIGI`)** avec mode **Voyelles formantiques** (*Ah / Eh / Ee / Oh / Oo*), routage Série/Parallèle, pavés `XY` en direct et matrice LFO 2×6
- **Dégradation Info Decay** (2–16 bits, sous-échantillonnage, lien `Exh Link` et fond violet pixelisé proportionnel à l'écrasement)
- **Oscillateur Noise / FM** (6 formes d'ondes routables Pré-Dist, Pré-Filtre ou Post, avec modulation FM du filtre)
- **Trance Gate 16 pas synchronisé** : **128 motifs rythmiques**, **16 pas cliquables directement au doigt sur l'écran**, enveloppe AHDS et réglage **Offset** pour laisser passer les kicks même en plein découpage
- **Pompe Sidechain dédiée (`PUMP`)** : Ratio jusqu'à 20:1, filtres clé `HPF`/`LPF`, source interne ou impulsion test 4×4, cibles combinables (`Drive`, `Cut`, `Gain`, `A Reso`) et vumètres `IN` / `DUCK`
- **Délai Stéréo / Peigne & Limiteur Lookahead (`CLN`/`LMC`)** avec vumètre horizontal **LIMITER GR**

---

## 2. Les 8 Macros : jouer avec les deux mains (`MAIN`)

À `0%`, le son reste intact. Dès qu'on tourne les grands orbes lumineux aux Q-Links, chaque macro réveille plusieurs réglages synchronisés sur toutes les pages de détail :
- **Banque 1 (Cyan)** : **EXHAUSTION** (saturation & drive), **CHOKE** (fermeture spectrale pré-drop Série/Parallèle), **INFO DECAY** (bit-crushing rétro + affichage pixelisé), **CORRUPT MIX** (mix + injection bruit dans les filtres).
- **Banque 2 (Magenta)** : **KINETIC MOD** (animation LFO1/2 sur les filtres), **RESONATOR** (peigne métallique dub/industriel), **GATE CARVE** (hachage rythmique au tempo), **SMASH** (compression/limiteur `LMC` musclé).

---

## 3. 96 Presets d'usine prêts à jouer

Classés par préfixes (`SAT:`, `FLT:`, `VOWL:`, `DECY:`, `LFO:`, `GATE:`, `PUMP:`, `COMBO:`, `MUSH:`), les **96 programmes intégrés** mettent instantanément à jour tous les boutons, pas du gate, filtres et macros dès leur sélection.
