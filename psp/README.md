# Operazione Ponte Spezzato — Port PSP

Port nativo per PlayStation Portable del gioco web "Operazione Ponte Spezzato"
(strategia tattica WWII in tempo reale). Compilato con **PSPSDK** e impacchettato
in `EBOOT.PBP` tramite **GitHub Actions**.

## Come si ottiene l'EBOOT
Il file `EBOOT.PBP` viene prodotto automaticamente a ogni push dalla workflow
`.github/workflows/build.yml` (container Docker `pspdev/pspdev:latest`).
Scarica l'artefatto "EBOOT.PBP" dalla pagina **Actions** del repository.

Per compilare in locale serve il toolchain PSPSDK:
```
make -C psp/src
```
L'output `psp/src/EBOOT.PBP` va copiato in `/PSP/GAME/OPSPEZZATO/` della memory stick.

## Comandi (PSP)
- **Croce (X)** — Ordine MUOVI: invia il plotone alla posizione del mirino
- **Cerchio (O)** — Ordine ATTACCA: il plotone punta e distrugge il nemico/obiettivo
- **Triangolo** — Ordine DIFENDI: il plotone tiene la posizione e spara
- **Quadrato** — Schiera un nuovo plotone (RINFORZI) se disponibili
- **L / R** — Zoom out / in
- **D-Pad / Analogo** — muove il mirino (la telecamera segue)
- **Start** — Pausa (e dal game over: torna al menu fazioni)

Obiettivo: distruggere tutti gli obiettivi sulla mappa (ponti, bunker, artiglierie, QG).
Se il plotone viene annientato e non restano rinforzi, la missione fallisce.

## Differenze rispetto all'originale
L'originale è un gioco web HTML5/Canvas con rete **P2P (WebRTC) via Trystero**.
La PSP non dispone di motore JavaScript né di WebRTC, quindi questo port è una
**riscrittura nativa in C** con:
- Grafica 2D software disegnata nel framebuffer della GU (niente immagini `.png`:
  sprite e font sono generati a codice);
- Audio PCM sintetizzato via `sceAudio` (spari, esplosioni, colpi);
- Modalità **single player**: comandi il tuo plotone e affronti le altre fazioni
  e le difese gestite dalla **CPU (NPC)**. Il multiplayer P2P non è presente.

## Struttura
```
psp/src/game.h     Tipi, costanti, dichiarazioni
psp/src/game.c     Generazione procedurale della mappa, unità, IA, simulazione
psp/src/render.c   Framebuffer GU, font bitmap, disegno mondo/HUD
psp/src/audio.c    Catena audio PSP e SFX
psp/src/main.c     Init GU/audio, loop, input
```
