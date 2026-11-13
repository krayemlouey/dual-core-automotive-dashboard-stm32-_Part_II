import os
import time
import urllib.request
import math

# Région : Sousse, Tunisie
LAT_MIN = 35.78
LAT_MAX = 35.87
LON_MIN = 10.58
LON_MAX = 10.70
ZOOMS = [13, 14, 15]

USER_AGENT = "STM32-GPS-Map/1.0"

def deg2num(lat_deg, lon_deg, zoom):
    lat_rad = math.radians(lat_deg)
    n = 2.0 ** zoom
    xtile = int((lon_deg + 180.0) / 360.0 * n)
    ytile = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    return (xtile, ytile)

def download_tiles():
    for z in ZOOMS:
        x_min, y_max = deg2num(LAT_MIN, LON_MIN, z)
        x_max, y_min = deg2num(LAT_MAX, LON_MAX, z)
        
        for x in range(x_min, x_max + 1):
            for y in range(y_min, y_max + 1):
                url = f"https://tile.openstreetmap.org/{z}/{x}/{y}.png"
                filepath = f"tiles/{z}/{x}/{y}.png"
                
                os.makedirs(os.path.dirname(filepath), exist_ok=True)
                
                if not os.path.exists(filepath):
                    print(f"Téléchargement : {url}")
                    req = urllib.request.Request(url, headers={'User-Agent': USER_AGENT})
                    try:
                        with urllib.request.urlopen(req) as response, open(filepath, 'wb') as out_file:
                            out_file.write(response.read())
                        time.sleep(0.5) # Respect des règles OSM
                    except Exception as e:
                        print(f"Erreur : {e}")
                else:
                    print(f"Déjà téléchargé : {filepath}")

if __name__ == "__main__":
    download_tiles()
    print("Téléchargement terminé !")
