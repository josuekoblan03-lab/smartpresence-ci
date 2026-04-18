import os
import base64
import cv2
import numpy as np
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import insightface
from insightface.app import FaceAnalysis

# Désactiver les avertissements onnxruntime
os.environ["ONNXRUNTIME_SUPPRESS_WARN"] = "1"

app = FastAPI()

# Initialisation LÉGÈRE de l'IA InsightFace (modèle s = small/mobile)
print("Loading InsightFace Model (buffalo_s)...")
face_app = FaceAnalysis(name='buffalo_s', allowed_modules=['detection', 'recognition'], providers=['CPUExecutionProvider'])
# ctx_id=0 (CPU), det_size (très basse résolution pour aller plus vite)
face_app.prepare(ctx_id=0, det_size=(320, 320))
print("Model loaded successfully!")

class ImageRequest(BaseModel):
    base64_image: str

@app.post("/analyze")
def analyze_face(req: ImageRequest):
    try:
        # Décoder le Base64
        img_data = req.base64_image
        if ',' in img_data:
            img_data = img_data.split(',')[1]
            
        img_bytes = base64.b64decode(img_data)
        np_arr = np.frombuffer(img_bytes, np.uint8)
        img = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
        
        if img is None:
            raise HTTPException(status_code=400, detail="Image invalide")
            
        # Analyser l'image
        faces = face_app.get(img)
        
        if len(faces) == 0:
            return {"success": False, "error": "Aucun visage détecté"}
            
        if len(faces) > 1:
            return {"success": False, "error": "Plusieurs visages détectés. Un seul autorisé."}
            
        face = faces[0]
        embedding = face.embedding.tolist()  # Convertir nb_array(512) en Liste Python
        
        return {
            "success": True,
            "embedding": embedding
        }

    except Exception as e:
        print("Erreur IA:", str(e))
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/health")
def health_check():
    return {"status": "ok"}
