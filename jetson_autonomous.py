#!/usr/bin/env python3
"""
stream_autonomous.py  —  Jetson Nano → STM32H747 via /dev/ttyACM1
Caméra Logitech Brio 100 + YOLOv8 + Alerte série STM32
"""

import cv2
import time
import threading
import os
import sys

# ── Configuration ──────────────────────────────────────────────────────────────
SERIAL_PORT  = "/dev/ttyACM1"
BAUD_RATE    = 115200
MODEL_PATH   = "/home/nvidia/models/best.pt"
VIDEO_DEVICE = 0          # /dev/video0 = Logitech Brio 100

# ── État partagé entre les threads ────────────────────────────────────────────
shared = {
    "frame":           None,
    "lock":            threading.Lock(),
    "running":         True,
    "alert_active":    False,
    "last_alert_time": 0.0,
    "fps_cam":         0.0,
    "fps_ai":          0.0,
}

# =============================================================================
# THREAD 1 : Capture Caméra (30 FPS fluide, non-bloquant pour l'IA)
# =============================================================================
def video_capture_worker():
    print("[CAM] Ouverture /dev/video0 (Logitech Brio 100)...")
    cap = None

    # Essaie V4L2 MJPEG d'abord
    cap = cv2.VideoCapture(VIDEO_DEVICE, cv2.CAP_V4L2)
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc('M','J','P','G'))
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_FPS, 30)

    if not cap.isOpened():
        # Fallback générique
        cap = cv2.VideoCapture(VIDEO_DEVICE)

    if not cap.isOpened():
        print("[CAM] ERREUR: Impossible d'ouvrir la caméra!")
        shared["running"] = False
        return

    print(f"[CAM] Caméra prête — {int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))}x"
          f"{int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))} FPS={cap.get(cv2.CAP_PROP_FPS):.0f}")

    cnt, t0 = 0, time.time()
    while shared["running"]:
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.02)
            continue
        with shared["lock"]:
            shared["frame"] = frame
        cnt += 1
        if time.time() - t0 >= 2.0:
            shared["fps_cam"] = cnt / (time.time() - t0)
            cnt, t0 = 0, time.time()

    cap.release()
    print("[CAM] Arrêté.")

# =============================================================================
# THREAD 2 : Inférence YOLOv8 (asynchrone, non bloquant)
# =============================================================================
def ai_worker():
    print(f"[AI] Chargement modèle : {MODEL_PATH} ...")
    try:
        from ultralytics import YOLO
        model = YOLO(MODEL_PATH)
        names = list(model.names.values())
        print(f"[AI] Modèle OK — Classes : {names}")
    except Exception as e:
        print(f"[AI] ERREUR chargement : {e}")
        return

    ALERT_CLASSES = {"cigarette", "cig", "smoke", "cigaret"}
    cnt, t0 = 0, time.time()

    while shared["running"]:
        img = None
        with shared["lock"]:
            if shared["frame"] is not None:
                img = shared["frame"].copy()

        if img is None:
            time.sleep(0.05)
            continue

        results = model(img, imgsz=320, conf=0.30, verbose=False)
        alert = False

        for r in results:
            for box in r.boxes:
                cls_id  = int(box.cls[0])
                cls_name = model.names.get(cls_id, "").lower()
                conf    = float(box.conf[0])
                if any(kw in cls_name for kw in ALERT_CLASSES):
                    alert = True
                    print(f"\n[AI >>>] {cls_name.upper()} détecté {conf*100:.0f}%")
                    break

        now = time.time()
        if alert:
            shared["alert_active"] = True
            shared["last_alert_time"] = now
        else:
            # Maintien alerte 5 secondes après la dernière détection
            if now - shared["last_alert_time"] > 5.0:
                shared["alert_active"] = False

        cnt += 1
        if time.time() - t0 >= 3.0:
            shared["fps_ai"] = cnt / (time.time() - t0)
            cnt, t0 = 0, time.time()

        time.sleep(0.02)

# =============================================================================
# THREAD 3 : Communication Série STM32 (/dev/ttyACM1)
# =============================================================================
def serial_worker():
    print(f"[SERIAL] Configuration {SERIAL_PORT} @ {BAUD_RATE} bauds...")
    os.system(f"stty -F {SERIAL_PORT} {BAUD_RATE} raw -echo 2>/dev/null")
    os.system(f"chmod 666 {SERIAL_PORT} 2>/dev/null")

    fd = None
    try:
        fd = os.open(SERIAL_PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        print(f"[SERIAL] Port {SERIAL_PORT} ouvert !")
    except Exception as e:
        print(f"[SERIAL] ERREUR ouverture {SERIAL_PORT} : {e}")
        return

    last_state   = None    # None, True (alerte), False (surveillance)
    last_tx_time = 0.0

    while shared["running"]:
        now       = time.time()
        is_alert  = shared["alert_active"]

        # Lecture des logs STM32 (non-bloquant)
        try:
            raw = os.read(fd, 512)
            if raw:
                for line in raw.decode(errors="ignore").splitlines():
                    line = line.strip()
                    if line:
                        print(f"[STM32] {line}")
        except BlockingIOError:
            pass
        except Exception:
            pass

        # Envoi de commandes vers la STM32
        # On envoie '1\n' pour déclencher l'alerte (1 octet + LF = fiable à 115200 bauds)
        # On envoie '0\n' pour réinitialiser en SURVEILLANCE
        if is_alert:
            if last_state != True or (now - last_tx_time) >= 0.5:
                try:
                    os.write(fd, b"1\n")
                    last_state   = True
                    last_tx_time = now
                    print("[SERIAL TX] >>> ALERTE 1 envoyée >>>")
                except Exception as e:
                    print(f"[SERIAL TX] Erreur : {e}")
        else:
            if last_state == True and (now - shared["last_alert_time"]) > 5.5:
                try:
                    os.write(fd, b"0\n")
                    last_state   = False
                    last_tx_time = now
                    print("[SERIAL TX] --- CLEAR 0 envoyé ---")
                except Exception as e:
                    pass

        time.sleep(0.05)

    if fd is not None:
        try:
            os.close(fd)
        except Exception:
            pass

# =============================================================================
# MAIN
# =============================================================================
def main():
    print("=" * 60)
    print("  SYSTEME AUTONOME : Jetson Nano + STM32H747 Dashboard")
    print("=" * 60)

    threads = [
        threading.Thread(target=video_capture_worker, daemon=True, name="CAM"),
        threading.Thread(target=ai_worker,            daemon=True, name="AI"),
        threading.Thread(target=serial_worker,        daemon=True, name="SERIAL"),
    ]
    for t in threads:
        t.start()

    try:
        while shared["running"]:
            time.sleep(2.0)
            etat = "🔴 ALERTE CIGARETTE" if shared["alert_active"] else "🟢 SURVEILLANCE"
            print(f"[STATUS] Cam={shared['fps_cam']:.1f}fps  "
                  f"IA={shared['fps_ai']:.2f}it/s  {etat}")
    except KeyboardInterrupt:
        print("\n[MAIN] Arrêt...")
    finally:
        shared["running"] = False
        time.sleep(1.0)
        print("[MAIN] Terminé.")

if __name__ == "__main__":
    main()
