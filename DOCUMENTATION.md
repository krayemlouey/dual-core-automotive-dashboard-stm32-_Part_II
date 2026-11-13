# Documentation Projet — Dashboard Automotive STM32H747 + Jetson Nano

> Mis à jour : Phase 2 — Intégration Ethernet Jetson Nano  
> Statut : ✅ Phase 1 (CAN + LiDAR) FONCTIONNEL · ✅ Phase 2 (Ethernet Jetson) IMPLÉMENTÉ

---

## 1. Architecture générale

```
╔══════════════════════════════════════════════════════════════════════════╗
║  NUCLEO-F446RE (STM32F446)           STM32H747I-DISCO                   ║
║  ┌──────────────┐                    ┌──────────────────────────────┐   ║
║  │ PA0 : Pot.Vitesse → ADC1         │ CM4 (240 MHz)                │   ║
║  │ PA1 : Pot.Fuel    → ADC1         │  ├── FDCAN1 (PA11/PA12)      │   ║
║  │ PA4 : DHT11       → Temp.        │  │   RX trame CAN 0x100      │   ║
║  │ PA10: TFmini-S    → USART1 RX    │  │   → SRAM4 @ 0x38000000    │   ║
║  │ PB8/PB9 → CAN1 Transceiver ──── │──┘                           │   ║
║  └──────────────┘                   │ CM7 (480 MHz) TouchGFX       │   ║
║                                     │  ├── LiDAR, Vitesse, Temp    │   ║
║                                     │  ├── Carburant, Boîte vit.   │   ║
║         Ethernet RJ45 ─────────────│──┤  Zone d'alerte Jetson      │   ║
║  ┌──────────────┐                  │  └──────────────────────────── │   ║
║  │ Jetson Nano  │                  │  EthernetTask (FreeRTOS)       │   ║
║  │ 192.168.1.20 │◄─── UDP 5000 ───│    LwIP + LAN8742A RMII        │   ║
║  │  (IA / ML)   │────►UDP 5001 ───│    TX trame JSON capteurs      │   ║
║  └──────────────┘    alertes       │    RX alertes JSON Jetson      │   ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 2. Câblage matériel

### 2.1 NUCLEO-F446RE — Capteurs

| Broche NUCLEO | Signal          | Composant           |
|---------------|-----------------|---------------------|
| PA0 (A0)      | ADC1_IN0        | Potentiomètre Vitesse |
| PA1 (A1)      | ADC1_IN1        | Potentiomètre Carburant |
| PA4 (A2)      | GPIO + 1-Wire   | DHT11 Température   |
| **PA10 (D2)** | USART1_RX       | TFmini-S fil **VERT** |
| PA2 (D1 / non utilisé) | —     | TFmini-S fil **BLANC** (TX LiDAR, non lu) |
| **5V (CN6)**  | Alimentation    | TFmini-S fil **ROUGE** |
| **GND (CN6)** | Masse           | TFmini-S fil **NOIR** |
| PB8           | CAN1_RX (AF9)   | Transceiver CAN — fil CAN-H |
| PB9           | CAN1_TX (AF9)   | Transceiver CAN — fil CAN-L |

### 2.2 STM32H747I-DISCO — Bus CAN

| Broche DISCO  | Signal          | Connexion           |
|---------------|-----------------|---------------------|
| PA11 (PMOD P3)| FDCAN1_RX (AF9) | Transceiver CAN — sortie |
| PA12 (PMOD P3)| FDCAN1_TX (AF9) | Transceiver CAN — entrée |
| GND           | GND commun      | NUCLEO + H747       |

### 2.3 STM32H747I-DISCO — Ethernet (Nouveau — Phase 2)

| Connecteur DISCO | Signal    | Jetson Nano          |
|------------------|-----------|----------------------|
| CN9 (RJ45)       | ETH RMII  | RJ45 câble droit     |
| IP STM32         | 192.168.1.10 | Adresse fixe       |
| IP Jetson        | 192.168.1.20 | Configurer sur Jetson |

> **⚠️ Câble droit** (pas croisé) car le PHY LAN8742A gère Auto-MDIX.

---

## 3. Protocoles de communication

### 3.1 Bus CAN — NUCLEO → H747

- **Débit :** 500 kbps  
- **ID :** `0x100`  
- **Payload : 8 octets**

| Octets | Champ        | Encodage                         |
|--------|--------------|----------------------------------|
| [0-1]  | speed_x10    | uint16 LE — Vitesse × 10 (ex: 1234 = 123.4 km/h) |
| [2-3]  | temp_x10     | uint16 LE — Temp. × 10 (ex: 235 = 23.5°C) |
| [4-5]  | fuel_x10     | uint16 LE — Carburant × 10      |
| [6-7]  | lidar_cm     | uint16 LE — Distance cm (0xFFFF = invalide) |

### 3.2 UDP TX — H747 → Jetson Nano (port 5000)

Envoyé toutes les **200 ms** par `EthernetTask` :

```json
{"speed":120.5,"temp":23.1,"fuel":75.0,"lidar":45,"gear":3}
```

| Champ   | Type    | Unité   | Description                              |
|---------|---------|---------|------------------------------------------|
| speed   | float   | km/h    | Vitesse mesurée (lue depuis CAN NUCLEO)  |
| temp    | float   | °C      | Température DHT11                        |
| fuel    | float   | %       | Niveau de carburant                      |
| lidar   | float   | cm      | Distance TFmini-S (ex: `45` cm, `null` si hors portée/invalide) |
| gear    | uint    | 0-5     | Rapport de boîte (0=N, 1 à 5 calculé)    |

---

### 3.3 UDP RX — Jetson Nano → H747 (port 5001)

Reçu par `udp_rx_callback()` dans LwIP :

```json
{"status":"critical","level":2,"target":"temp","msg":"SURCHAUFFE MOTEUR 110C"}
```

ou pour le carburant :
```json
{"status":"critical","level":2,"target":"fuel","msg":"NIVEAU BAS RESERVE"}
```

ou pour la vitesse / obstacle :
```json
{"status":"critical","level":2,"target":"speed","msg":"OBSTACLE PROCHE 30cm"}
```

ou retour à la normale :
```json
{"status":"ok","level":0,"msg":""}
```

| Champ    | Valeurs acceptées                | Effet sur le Dashboard                                  |
|----------|----------------------------------|---------------------------------------------------------|
| `status` | `"critical"` / `"critic"`        | **Clignotement ROUGE de la LED correspondante** + Cadre Rouge |
|          | `"warning"`                      | LEDs noires éteintes + Cadre d'alerte Orange            |
|          | `"ok"`                           | LEDs noires éteintes + Cadre gris discret                |
| `target` | `"temp"` / `"temperature"`       | Déclenche la LED **sous la jauge de température**       |
| (option) | `"fuel"` / `"carburant"`         | Déclenche la LED **sous la jauge de carburant**         |
|          | `"speed"` / `"obstacle"` (défaut)| Déclenche la LED **vitesse au-dessus (entre 0 et 250)** |
| `level`  | `2` (danger), `1` (warn), `0`    | Gravité associée                                        |
| `msg`    | string (max 63 car. ASCII)       | Message affiché. *Note : si `target` est omis, les mots-clés dans `msg` (ex: "TEMP", "FUEL", "OBSTACLE") sélectionnent automatiquement la bonne LED !* |

---

### 3.4 Interface Graphique TouchGFX — Widgets d'Alerte

1. **Cadre d'alerte Jetson (Agrandie en hauteur) :**
   - **Position :** `(234, 348)` à gauche de l'écriture LiDAR.
   - **Dimensions :** `114 × 46 px` (hauteur augmentée de 36 px à **46 px**).
   - **Contraintes respectées :** Ne touche ni les jauges rondes, ni le LiDAR (`X=383`), ni la boîte de vitesse, ni la barre de séparation (`Y=346`).
   - **Couleur :** Rouge (danger critique), Orange (warning), Gris sombre (repos).

2. **Les 3 LEDs Triangulaires (forme triangle creux, double contour) :**
   - **LED 1 — Température :** `(77, 452)` — **sous la jauge de température** (`gaugetemp`).
   - **LED 2 — Carburant (Fuel) :** `(700, 452)` — **sous la jauge de carburant** (`gaugefuel`).
   - **LED 3 — Vitesse / Obstacle :** `(386, 56)` — **au-dessus, entre 0 et 250** (en haut entre les cadrans, dans le vide).
   - **Forme :** Chaque LED est un triangle vectoriel creux avec **périmètre extérieur** (28×24 px) et **périmètre intérieur** (centre vide).
   - **Comportement :**
     - **Repos / normal :** Éteinte en noir (`RGB(0, 0, 0)`).
     - **Alerte critique Jetson :** Clignote en **rouge vif (`RGB(255, 0, 0)`)** sur la LED correspondant au problème identifié.


---

## 4. Mémoire partagée SRAM4 — Layout complet

Adresse base : `0x38000000`  
Région MPU : Non-Cacheable + Shareable (MPU_REGION_NUMBER6 dans CM7/main.c)

| Offset | Taille | Champ           | Description                              |
|--------|--------|-----------------|------------------------------------------|
| 0x00   | 4      | `speed`         | Vitesse km/h (float)                     |
| 0x04   | 4      | `temperature`   | Température °C (float)                   |
| 0x08   | 4      | `fuel`          | Carburant % (float)                      |
| 0x0C   | 4      | `gps_lat`       | Latitude fixe 35.8256 (float)            |
| 0x10   | 4      | `gps_lon`       | Longitude fixe 10.6369 (float)           |
| 0x14   | 1      | `gps_updated`   | Flag GPS                                 |
| 0x15   | 1      | `demo_mode`     | 1 = pas de CAN réel                      |
| 0x16   | 2      | `padding`       | Alignement (padding[1]=0xAA = CM4 OK)    |
| 0x18   | 4      | `lidar_cm`      | Distance LiDAR cm (-1 = invalide)        |
| 0x1C   | 4      | `can_rx_count`  | Compteur trames CAN reçues               |
| 0x20   | 64     | `alert_text`    | ⭐ **NOUVEAU** — Message alerte Jetson   |
| 0x60   | 1      | `alert_level`   | ⭐ **NOUVEAU** — 0=OK, 1=WARN, 2=DANGER |
| 0x61   | 1      | `ethernet_ok`   | ⭐ **NOUVEAU** — 1 = Ethernet actif      |
| 0x62   | 2      | `jetson_pad`    | Réservé futur                            |

---

## 5. Flux de données complet

```
[DHT11/ADC/TFmini] → F446RE CAN TX → [FDCAN1 H747 CM4]
                                             │
                                       Process_CAN_Frame()
                                             │
                                        SRAM4 0x38000000
                                          ┌──┴──┐
                               CM7 Model::tick()   EthernetTask
                                    │                   │
                              TouchGFX UI         UDP_SendTelemetry()
                           (Vitesse,Carburant,         │
                            Temp,LiDAR,Alerte)    → Jetson Nano
                                    ▲                   │
                              alertText ←── UDP_Poll() ←┘
                          (écrit par                (alertes Jetson)
                           udp_rx_callback)
```

---

## 6. Fichiers modifiés — Phase 2

### CM4 (STM32H747 — Réception CAN)

| Fichier | Modification |
|---------|-------------|
| `CM4/Core/Src/main.c` | Struct `SharedData_t` étendue avec `alert_text[64]`, `alert_level`, `ethernet_ok` |

### CM7 — Ethernet & LwIP

| Fichier | Modification |
|---------|-------------|
| `CM7/Core/Inc/udp_sender.h` | Refait pour Jetson Nano — TX port 5000, RX port 5001 |
| `CM7/Core/Src/udp_sender.c` | Implémentation TX JSON + RX callback + écriture SRAM4 |
| `CM7/Core/Inc/lwipopts.h` | IP STM32=192.168.1.10, MAC=02:00:00:00:00:0A |
| `CM7/Core/Src/freertos.c` | `EthernetTask` ajoutée — init LwIP + polling + TX télémétrie |
| `CM7/Core/Src/main.c` | Déclaration + création thread `EthernetTask` dans FreeRTOS |

### CM7 — TouchGFX Dashboard

| Fichier | Modification |
|---------|-------------|
| `CM7/TouchGFX/gui/include/gui/model/Model.hpp` | Ajout `getAlertText()`, `getAlertLevel()`, `getEthernetOk()` + membres privés |
| `CM7/TouchGFX/gui/src/model/Model.cpp` | Lecture SRAM4 alertes dans `tick()` + implémentation getters |
| `CM7/TouchGFX/gui/include/gui/dashboardmedium_screen/DashboardMediumPresenter.hpp` | Ajout 3 getters alerte |
| `CM7/TouchGFX/gui/src/dashboardmedium_screen/DashboardMediumPresenter.cpp` | Délégation vers model |
| `CM7/TouchGFX/gui/include/gui/dashboardmedium_screen/DashboardMediumView.hpp` | Ajout `alertBox`, `alertText`, buffers alerte |
| `CM7/TouchGFX/gui/src/dashboardmedium_screen/DashboardMediumView.cpp` | Masque image3/image4/GPS, affiche zone alerte Jetson avec couleur dynamique |

---

## 7. Configuration réseau Jetson Nano

Sur la Jetson Nano, configurer l'interface Ethernet :

```bash
# Configurer IP statique
sudo ip addr add 192.168.1.20/24 dev eth0
sudo ip link set eth0 up

# Vérifier que la H747 répond (ping)
ping 192.168.1.10
```

### Serveur Python minimal de test (Jetson Nano)

```python
#!/usr/bin/env python3
"""
test_jetson.py — Test communication UDP avec STM32H747
Reçoit les trames capteurs et renvoie des alertes.
"""
import socket
import json
import time

STM32_IP = "192.168.1.10"
RX_PORT  = 5000   # Reçoit depuis H747
TX_PORT  = 5001   # Envoie vers H747

rx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
rx_sock.bind(("0.0.0.0", RX_PORT))
rx_sock.settimeout(1.0)

tx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print(f"Écoute sur UDP:{RX_PORT} | Envoi vers {STM32_IP}:{TX_PORT}")

while True:
    try:
        data, addr = rx_sock.recvfrom(256)
        payload = json.loads(data.decode())
        print(f"[H747→Jetson] {payload}")

        # Exemple : alerte si obstacle proche
        level = 0
        msg   = "OK"
        if payload.get("lidar") is not None and payload["lidar"] < 50:
            level = 2
            msg   = f"OBSTACLE {int(payload['lidar'])}cm"
        elif payload.get("speed", 0) > 120:
            level = 1
            msg   = f"VITESSE {int(payload['speed'])}km/h"

        alert = json.dumps({"level": level, "msg": msg}).encode()
        tx_sock.sendto(alert, (STM32_IP, TX_PORT))
        print(f"[Jetson→H747] level={level} msg={msg}")

    except socket.timeout:
        pass
```

---

## 8. Procédure de compilation et flash

### Compiler et flasher NUCLEO-F446RE

```powershell
cd NUCLEO_F446RE_CAN_TX
.\build_and_flash.ps1
```

### Compiler et flasher STM32H747I-DISCO (CM4 + CM7)

```powershell
.\flash_both_h747.ps1
```

> ⚠️ Toujours flasher CM4 **avant** CM7. Le CM7 réveille le CM4 via HSEM.

---

## 9. Diagnostic et debug

### Vérifier la communication CAN

- Ouvrir un terminal série sur **COM5** (H747 ST-LINK)
- `can_rx_count > 0` dans SRAM4+0x1C → CAN OK

### Vérifier Ethernet

- La LED **ETH_LINK** sur la DISCO doit être verte
- Depuis la Jetson : `ping 192.168.1.10`
- Si pas de ping → vérifier câble + IP Jetson

### Vérifier alertes sur dashboard

- Lancer `test_jetson.py` sur la Jetson
- Approcher la main à moins de 50 cm du LiDAR → la zone rouge "OBSTACLE Xcm" doit apparaître sur l'écran du dashboard

---

## 10. Prochaines étapes possibles

| Tâche | Description |
|-------|-------------|
| Modèles IA Jetson | Intégrer YOLO/TensorRT pour détection obstacles caméra |
| Boîte de vitesses physique | Ajouter capteur Hall sur levier de vitesses → champ `gear` CAN |
| DHCP Jetson | Remplacer IP fixe par DHCP si réseau partagé |
| Chiffrement | Ajouter HMAC sur trames UDP pour sécurité |
| Multi-alertes | Gérer une file de messages Jetson (queue FreeRTOS) |
