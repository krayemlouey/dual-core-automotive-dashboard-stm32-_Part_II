# Dual-Core Automotive Dashboard — STM32H747I-DISCO + Jetson Nano

> Tableau de bord automobile temps réel basé sur un SoC **dual-core STM32H747** (Cortex-M7 @ 480 MHz + Cortex-M4 @ 240 MHz), avec affichage TouchGFX, acquisition multi-capteurs via bus CAN, et supervision IA via Jetson Nano en Ethernet UDP.

[![MCU](https://img.shields.io/badge/MCU-STM32H747I--DISCO-blue?style=flat-square&logo=stmicroelectronics)](https://www.st.com/)
[![Framework](https://img.shields.io/badge/UI-TouchGFX%204.26-lightblue?style=flat-square)](https://www.touchgfx.com/)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green?style=flat-square)](https://www.freertos.org/)
[![CAN](https://img.shields.io/badge/Bus-FDCAN%20500%20kbps-orange?style=flat-square)]()
[![Ethernet](https://img.shields.io/badge/Ethernet-LwIP%20%2B%20LAN8742A-blueviolet?style=flat-square)]()
[![AI](https://img.shields.io/badge/Edge--AI-Jetson%20Nano%20UDP-red?style=flat-square)]()
[![Author](https://img.shields.io/badge/Author-Louey%20Krayem-black?style=flat-square)]()

---

## Vue d'ensemble

Ce projet implémente un **tableau de bord automobile complet** embarqué sur la carte de développement **STM32H747I-DISCO**. Il exploite le dual-core du SoC en mode **AMP (Asymmetric Multiprocessing)** : le cœur **CM4 gère l'acquisition des données capteurs** via le bus CAN, tandis que le cœur **CM7 pilote l'interface graphique TouchGFX** et la communication Ethernet avec un module IA (Jetson Nano).

### Fonctionnalités principales

| Fonctionnalité | Détail |
|---|---|
| 🎨 **Interface graphique** | TouchGFX @ 480×800 px 24bpp — jauges animées, alertes couleur |
| 🚗 **Vitesse** | Potentiomètre → ADC → CAN → Dashboard (0–250 km/h) |
| 🌡️ **Température** | Capteur DHT11 → CM4 → SRAM4 partagée → CM7 |
| ⛽ **Carburant** | Potentiomètre → ADC → CAN → Jauge circulaire |
| 📡 **LiDAR** | TFmini-S UART → distance obstacle en cm |
| 🌐 **Ethernet IA** | TX télémétrie JSON → Jetson Nano (UDP :5000) |
| 🔔 **Alertes IA** | RX alertes JSON ← Jetson Nano (UDP :5001) → LED clignotante |
| 🗺️ **GPS** | Position GPS fixe (démonstration) |

---

## Architecture système

```
  ┌───────────────────┐    CAN 500kbps     ┌──────────────────────────────────────┐
  │   NUCLEO-F446RE   │ ─────────────────► │       STM32H747I-DISCO               │
  │                   │   ID 0x100          │                                      │
  │  PA0 → Vitesse    │   8 octets payload  │  ┌─────────────┐  ┌──────────────┐  │
  │  PA1 → Carburant  │                     │  │  CM4 240MHz │  │  CM7 480MHz  │  │
  │  PA4 → DHT11 Temp │                     │  │  FDCAN1 RX  │  │  TouchGFX UI │  │
  │  PA10→ TFmini-S   │                     │  │  Process()  │  │  FreeRTOS    │  │
  │  PB8/9 → CAN TX   │                     │  └──────┬──────┘  └──────┬───────┘  │
  └───────────────────┘                     │         │   SRAM4 partagé│           │
                                            │         └────0x38000000──┘           │
                                            │                  │                   │
                                            │         ┌────────▼────────┐          │
                                            │         │  EthernetTask   │          │
                                            │         │  LwIP + LAN8742A│          │
                                            └─────────┴────────┬────────┴──────────┘
                                                               │  RJ45
                                                    UDP:5000 ◄─┤
                                            ┌───────────────┐  ├─► UDP:5001
                                            │  Jetson Nano  │◄─┘
                                            │ 192.168.1.20  │
                                            │   (IA / ML)   │
                                            └───────────────┘
```

---

## Structure du projet

```
├── CM4/                        # Firmware cœur Cortex-M4 (acquisition CAN)
│   └── Core/Src/main.c         # Réception FDCAN1, écriture SRAM4
├── CM7/                        # Firmware cœur Cortex-M7 (UI + Ethernet)
│   ├── Core/Src/
│   │   ├── main.c              # Init système, création threads FreeRTOS
│   │   ├── freertos.c          # EthernetTask — LwIP polling + UDP TX/RX
│   │   └── udp_sender.c        # TX JSON capteurs + RX alertes Jetson
│   └── TouchGFX/gui/           # Interface graphique dashboard
│       ├── model/Model.cpp     # Lecture SRAM4, getters alertes
│       └── dashboardmedium_screen/
│           └── DashboardMediumView.cpp  # Rendu, LEDs alerte, zone Jetson
├── NUCLEO_F446RE_CAN_TX/       # Firmware NUCLEO émetteur CAN
├── STM32CubeIDE/               # Projets STM32CubeIDE (CM4 + CM7)
├── EWARM/                      # Projet IAR EWARM
├── MDK-ARM/                    # Projet Keil MDK-ARM
├── Drivers/                    # HAL STM32H7 + BSP
├── Middlewares/                # FreeRTOS + LwIP + TouchGFX
├── jetson_autonomous.py        # Script Python Jetson Nano (supervision IA)
├── flash_both_h747.ps1         # Script flash CM4 + CM7 en séquence
├── rebuild_cm4.ps1             # Recompile uniquement CM4
├── STM32H747I-DISCO.ioc        # Configuration STM32CubeMX
├── DOCUMENTATION.md            # Documentation technique complète
└── changelog.txt               # Historique des versions
```

---

## Matériel requis

| Composant | Référence | Rôle |
|---|---|---|
| **STM32H747I-DISCO** | ST Discovery | Carte principale — dual-core + écran 480×800 |
| **NUCLEO-F446RE** | ST Nucleo | Émetteur CAN — acquisition capteurs |
| **TFmini-S** | Benewake | Capteur LiDAR UART — distance obstacle |
| **DHT11** | — | Capteur température/humidité 1-Wire |
| **2× Potentiomètres** | — | Simulation vitesse et niveau carburant |
| **Transceiver CAN** | MCP2551 | Adaptation bus CAN entre NUCLEO et H747 |
| **Jetson Nano** | NVIDIA | Module IA — supervision et alertes via UDP |
| **Câble RJ45** | droit (Auto-MDIX) | Liaison Ethernet H747 ↔ Jetson |

---

## Câblage

### NUCLEO-F446RE → Capteurs

| Broche NUCLEO | Signal | Capteur |
|---|---|---|
| PA0 (A0) | ADC1_IN0 | Potentiomètre Vitesse |
| PA1 (A1) | ADC1_IN1 | Potentiomètre Carburant |
| PA4 (A2) | GPIO 1-Wire | DHT11 |
| PA10 (D2) | USART1_RX | TFmini-S fil VERT |
| 5V (CN6) | Alimentation | TFmini-S fil ROUGE |
| GND (CN6) | Masse | TFmini-S fil NOIR |
| PB8 | CAN1_RX | Transceiver CAN-H |
| PB9 | CAN1_TX | Transceiver CAN-L |

### STM32H747I-DISCO → Bus CAN

| Broche DISCO | Signal | Connexion |
|---|---|---|
| PA11 (PMOD P3) | FDCAN1_RX | Transceiver CAN |
| PA12 (PMOD P3) | FDCAN1_TX | Transceiver CAN |
| GND | GND commun | NUCLEO + H747 |

### Ethernet

| Équipement | IP | Port |
|---|---|---|
| STM32H747I-DISCO | `192.168.1.10` | TX:5000 / RX:5001 |
| Jetson Nano | `192.168.1.20` | RX:5000 / TX:5001 |

> Câble **droit** (pas croisé) — le PHY LAN8742A gère l'Auto-MDIX.

---

## Protocoles de communication

### Trame CAN (NUCLEO → H747) — ID `0x100`, 8 octets

| Octets | Champ | Encodage |
|---|---|---|
| [0–1] | `speed_x10` | uint16 LE — Vitesse × 10 (ex: 1234 = 123.4 km/h) |
| [2–3] | `temp_x10` | uint16 LE — Température × 10 (ex: 235 = 23.5°C) |
| [4–5] | `fuel_x10` | uint16 LE — Carburant × 10 |
| [6–7] | `lidar_cm` | uint16 LE — Distance cm (`0xFFFF` = invalide) |

### UDP TX : H747 → Jetson (port 5000) — toutes les 200 ms

```json
{"speed": 120.5, "temp": 23.1, "fuel": 75.0, "lidar": 45, "gear": 3}
```

### UDP RX : Jetson → H747 (port 5001) — alertes

```json
{"status": "critical", "level": 2, "target": "temp", "msg": "SURCHAUFFE MOTEUR 110C"}
{"status": "ok", "level": 0, "msg": ""}
```

| `status` | Effet |
|---|---|
| `critical` | LED clignotante ROUGE + cadre rouge |
| `warning` | Cadre orange |
| `ok` | Cadre gris — retour normal |

---

## Mémoire partagée SRAM4 — `0x38000000`

Région MPU configurée **Non-Cacheable + Shareable** pour la cohérence entre CM4 et CM7.

| Offset | Taille | Champ | Description |
|---|---|---|---|
| `0x00` | 4 | `speed` | float — km/h |
| `0x04` | 4 | `temperature` | float — °C |
| `0x08` | 4 | `fuel` | float — % |
| `0x18` | 4 | `lidar_cm` | float — cm (−1 = invalide) |
| `0x1C` | 4 | `can_rx_count` | Compteur trames CAN reçues |
| `0x20` | 64 | `alert_text` | Message alerte Jetson (ASCII) |
| `0x60` | 1 | `alert_level` | 0=OK · 1=WARN · 2=DANGER |
| `0x61` | 1 | `ethernet_ok` | 1 = Ethernet actif |

---

## Compilation & Flash

### Prérequis

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
- [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)
- [TouchGFX Designer](https://www.touchgfx.com/studio) *(optionnel, pour éditer l'UI)*

### Flash CM4 + CM7 en séquence (PowerShell)

```powershell
.\flash_both_h747.ps1
```

> ⚠️ Toujours flasher **CM4 avant CM7**. Le CM7 réveille le CM4 via le registre HSEM.

### Recompiler CM4 uniquement

```powershell
.\rebuild_cm4.ps1
```

### Configurer le Jetson Nano (IP statique)

```bash
sudo ip addr add 192.168.1.20/24 dev eth0
sudo ip link set eth0 up
ping 192.168.1.10   # Vérifier la réponse H747
python3 jetson_autonomous.py
```

---

## Diagnostic rapide

| Symptôme | Action |
|---|---|
| `can_rx_count` reste à 0 | Vérifier câble CAN-H/CAN-L + alimentation transceiver |
| Pas de ping `192.168.1.10` | LED ETH_LINK doit être verte — vérifier câble RJ45 + IP Jetson |
| Alertes non affichées | Lancer `jetson_autonomous.py` — approcher la main < 50 cm du LiDAR |
| Écran blanc au boot | CM7 flashé sans CM4 — reflasher dans l'ordre CM4 → CM7 |

---

## Auteur

**Louey Krayem** — Ingénieur Systèmes Embarqués  
📧 loueykrayem@gmail.com
