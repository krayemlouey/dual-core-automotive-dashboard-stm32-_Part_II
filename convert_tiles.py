import os
import glob
from PIL import Image

def convert_to_rgb565_bmp():
    png_files = glob.glob('tiles/**/*.png', recursive=True)
    
    if not png_files:
        print("Aucun fichier PNG trouvé. Lancez d'abord download_tiles.py")
        return
        
    for png_path in png_files:
        bmp_path = png_path.replace('.png', '.bmp')
        
        if os.path.exists(bmp_path):
            continue
            
        try:
            with Image.open(png_path) as img:
                # Vérifier la taille exacte
                if img.size != (256, 256):
                    img = img.resize((256, 256))
                
                # Convertir en RGB (si RGBA) puis en BGR;16 (équivalent PIL du RGB565 pour BMP)
                img = img.convert('RGB')
                
                # Sauvegarder en BMP 16 bits (RGB565)
                # PIL gère le format BGR;16 pour les BMP 16 bits
                img.save(bmp_path, format='BMP')
                
                print(f"Converti : {bmp_path}")
        except Exception as e:
            print(f"Erreur avec {png_path}: {e}")

if __name__ == "__main__":
    print("Installation de Pillow requise : pip install Pillow")
    convert_to_rgb565_bmp()
    print("Conversion terminée !")
