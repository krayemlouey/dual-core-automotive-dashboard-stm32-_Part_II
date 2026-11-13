import os
import requests
from PIL import Image
import math
import time

# --- CONFIGURATION ---
ZOOM = 15
# Coordonnées pour Sousse et Monastir
LAT_MIN, LAT_MAX = 35.65, 36.00
LON_MIN, LON_MAX = 10.50, 11.00

OUTPUT_DIR = "tiles"
TILE_URL = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
HEADERS = {'User-Agent': 'STM32MapDownloader/1.0'}

def latlon_to_tile(lat, lon, z):
    lat_rad = math.radians(lat)
    n = 2.0 ** z
    x = int((lon + 180.0) / 360.0 * n)
    y = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    return x, y

def download_tile(z, x, y):
    path = os.path.join(OUTPUT_DIR, str(z), str(x))
    os.makedirs(path, exist_ok=True)
    filename = os.path.join(path, f"{y}.bmp")
    
    if os.path.exists(filename):
        return

    url = TILE_URL.format(z=z, x=x, y=y)
    try:
        response = requests.get(url, headers=HEADERS, timeout=10)
        if response.status_code == 200:
            temp_png = "temp.png"
            with open(temp_png, "wb") as f:
                f.write(response.content)
            img = Image.open(temp_png).convert("RGB")
            img = img.resize((256, 256))
            img.save(filename, "BMP")
            print(f"OK: {z}/{x}/{y}")
            time.sleep(0.1)
        else:
            print(f"Err {response.status_code}")
    except Exception as e:
        print(f"Error: {e}")

# --- MAIN ---
print("Téléchargement Sousse & Monastir (Zoom 14, 15, 16)...")

for z in [14, 15, 16]:
    x_min, y_max = latlon_to_tile(LAT_MIN, LON_MIN, z)
    x_max, y_min = latlon_to_tile(LAT_MAX, LON_MAX, z)

    total = (x_max - x_min + 1) * (y_max - y_min + 1)
    print(f"Total tuiles à télécharger pour le zoom {z}: {total}")

    count = 0
    for x in range(x_min, x_max + 1):
        for y in range(y_min, y_max + 1):
            download_tile(z, x, y)
            count += 1
            if count % 10 == 0:
                print(f"Progression Z{z}: {count}/{total}")

print("Terminé ! Copiez le dossier 'tiles' sur votre carte SD.")
