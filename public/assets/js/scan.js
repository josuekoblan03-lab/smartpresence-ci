// ─────────────────────────────────────────────────────────────
// Variables Globales
// ─────────────────────────────────────────────────────────────
let currentAppareilId = null; 
let currentSessionId = null;
let currentFingerprint = null;
let qrCanvas = null;
let qrInterval = null; 

// Initialisation temporelle pour empêcher l'usurpation de délai
let pageLoadTime = Date.now();

let faceModelsLoaded = false;

// ─────────────────────────────────────────────────────────────
// Bouclier 6: Empreinte Matérielle Avancée (WebGL + CPU + RAM)
// ─────────────────────────────────────────────────────────────
function getHardwareFingerprint() {
  const canvas = document.createElement('canvas');
  const gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
  let gpu = 'Unknown';
  if (gl) {
    const debugInfo = gl.getExtension('WEBGL_debug_renderer_info');
    if (debugInfo) {
      gpu = gl.getParameter(debugInfo.UNMASKED_RENDERER_WEBGL);
    }
  }

  const ecranLargeur   = screen.width;
  const ecranHauteur   = screen.height;
  const ecranProfondeur= screen.colorDepth;
  const nbCPU          = navigator.hardwareConcurrency || 0;
  const ram            = navigator.deviceMemory  || 0;
  const fuseau         = Intl.DateTimeFormat().resolvedOptions().timeZone;
  const platform       = navigator.platform;
  const ua             = navigator.userAgent;

  const rawFingerprint = `${gpu}-${ecranLargeur}x${ecranHauteur}-${ecranProfondeur}-${nbCPU}-${ram}-${fuseau}-${platform}-${ua}`;
  
  let hash = 0;
  for (let i = 0; i < rawFingerprint.length; i++) {
    const char = rawFingerprint.charCodeAt(i);
    hash = ((hash << 5) - hash) + char;
    hash = hash & hash;
  }
  return "HW_" + Math.abs(hash).toString(16) + "_" + (gpu.substring(0,6).replace(/[^a-zA-Z0-9]/g, ''));
}

async function loadFaceModels() {
  try {
    const MODEL_URL = 'https://justadudewhohacks.github.io/face-api.js/models/';
    await faceapi.nets.tinyFaceDetector.loadFromUri(MODEL_URL);
    await faceapi.nets.faceLandmark68Net.loadFromUri(MODEL_URL);
    await faceapi.nets.faceRecognitionNet.loadFromUri(MODEL_URL);
    faceModelsLoaded = true;
    console.log("Modèles IA Facial Chargés");
  } catch(e) {
    console.error("Erreur de chargement des modèles Face-API: ", e);
  }
}

// ─────────────────────────────────────────────────────────────
// Initialisation de la page
// ─────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', async () => {
  loadFaceModels();

  const params = new URLSearchParams(window.location.search);
  const tokenFromUrl = params.get('token');
  const sidFromUrl   = params.get('sid');

  if (tokenFromUrl) {
    document.getElementById('qr-token-input').value = tokenFromUrl;
  }

  if (sidFromUrl) {
    currentSessionId = parseInt(sidFromUrl);
    document.getElementById('manual-session-id').value = sidFromUrl;
    await loadSessionInfo(currentSessionId);
  }

  // Pre-remplir matricule si dispo
  const savedMatricule = localStorage.getItem('sp_matricule');
  if (savedMatricule) {
    document.getElementById('matricule').value = savedMatricule;
  }

  document.getElementById('presence-form').addEventListener('submit', submitPresence);

  document.getElementById('matricule').addEventListener('input', function() {
    this.value = this.value.toUpperCase();
  });

  document.getElementById('code-personnel').addEventListener('input', function() {
    this.value = this.value.replace(/[^0-9]/g, '').slice(0, 6);
  });
});

// ─────────────────────────────────────────────────────────────
// Charger info de session
// ─────────────────────────────────────────────────────────────
async function loadSessionInfo(sid) {
  sid = sid || parseInt(document.getElementById('manual-session-id').value);
  if (!sid || sid <= 0) {
    showToast('warning', 'Entrez un numéro de session valide');
    return;
  }
  
  try {
    const response = await fetch('/api/presence/session_info?sid=' + sid);
    const data = await response.json();

    if (data.success) {
      currentSessionId = sid;
      document.getElementById('session-banner').classList.remove('hidden');
      document.getElementById('session-name').innerText = data.session.titre;
      document.getElementById('session-info').innerText = `${data.session.date_creation} | Actif`;
      document.getElementById('session-selector').classList.add('hidden');
      
      const tokenZone = document.getElementById('token-zone');
      if (tokenZone) {
         tokenZone.classList.remove('hidden');
         await checkSessionToken();
         if(qrInterval) clearInterval(qrInterval);
         qrInterval = setInterval(checkSessionToken, 10000); 
      }
    } else {
      showToast('error', data.error || 'Session introuvable ou fermée');
    }
  } catch(err) {
    showToast('error', 'Erreur réseau');
  }
}

// ─────────────────────────────────────────────────────────────
// Boucle de rafraîchissement du Token
// ─────────────────────────────────────────────────────────────
let lastAutoRefreshedToken = null;

async function checkSessionToken() {
  if (!currentSessionId) return;

  try {
    const response = await fetch('/api/presence/session_token?sid=' + currentSessionId);
    const data = await response.json();
    
    if (data.success) {
      if (data.token !== lastAutoRefreshedToken) {
          lastAutoRefreshedToken = data.token;
          
          if(!document.getElementById('qr-token-input').value) {
             document.getElementById('qr-token-input').value = data.token; 
          }
          
          document.getElementById('token-display').innerText = data.token;
          
          const fullUrl = `${window.location.origin}/scan.html?sid=${currentSessionId}&token=${data.token}`;
          if (qrCanvas) {
             qrCanvas.clear();
             qrCanvas.makeCode(fullUrl);
          } else {
             qrCanvas = new QRCode(document.getElementById("qr-code-canvas"), {
                text: fullUrl,
                width: 150,
                height: 150,
                colorDark : "#0f0e17",
                colorLight : "#ffffff",
                correctLevel : QRCode.CorrectLevel.H
             });
          }
          
          let duration = parseInt(data.duration) || 60;
          startCountdown(duration);
      }
    } else {
      clearInterval(qrInterval);
      document.getElementById('token-display').innerText = "Session Fermée";
      showToast('error', "La distribution de token est arrêtée.");
    }
  } catch(err) {
      console.log('Attente réseau pour token...');
  }
}

function startCountdown(seconds) {
  let left = seconds;
  const cBar = document.getElementById('countdown-bar');
  const cNum = document.getElementById('countdown');
  
  if (cBar) { cBar.style.width = '100%'; cBar.style.transition = 'none'; }
  if (cNum) { cNum.innerText = left + 's'; }
  
  let cdInterval = setInterval(() => {
    left--;
    if (cBar) {
       let pct = (left / seconds) * 100;
       cBar.style.width = pct + '%';
       if (pct < 20) cBar.style.background = '#e74c3c';
       else cBar.style.background = 'var(--color-primary)';
    }
    if (cNum) { cNum.innerText = left + 's'; }
    if (left <= 0) clearInterval(cdInterval);
  }, 1000);
}

// ─────────────────────────────────────────────────────────────
// Soumission Présence: Face Liveness & Double GPS
// ─────────────────────────────────────────────────────────────
async function submitPresence(e) {
  e.preventDefault();

  const matricule      = document.getElementById('matricule').value.trim();
  const codePersonnel  = document.getElementById('code-personnel').value.trim();
  const qrToken        = document.getElementById('qr-token-input').value.trim();
  const sessionId      = currentSessionId || parseInt(document.getElementById('manual-session-id').value) || 0;

  if (!matricule || !codePersonnel || !qrToken || !sessionId) {
    showError('Veuillez remplir correctement tous les champs.');
    return;
  }

  localStorage.setItem('sp_matricule', matricule);

  document.getElementById('presence-form').classList.add('hidden');
  document.getElementById('biometric-scanner').classList.remove('hidden');
  
  document.querySelector('.scan-form-title').innerText = "Vérification de sécurité en cours";

  const overlay = document.getElementById('liveness-overlay');
  const overlayText = document.getElementById('bio-status');
  
  try {
    overlay.style.display = 'flex';
    overlayText.textContent = "Acquisition GPS de départ...";
    
    // GPS_START
    let gps1;
    try {
      gps1 = await getExactGPSPosition();
      document.getElementById('gps-status').classList.remove('hidden');
      document.getElementById('gps-status').innerText = '📍 Position de départ capturée';
    } catch(err) {
      throw new Error(err.message || err);
    }
    
    if (!faceModelsLoaded) {
      overlayText.textContent = "Chargement de l'intelligence artificielle...";
      while(!faceModelsLoaded) await new Promise(r => setTimeout(r, 500));
    }
    
    // VISAGE
    overlay.style.background = 'transparent'; 
    let base64Image;
    try {
      base64Image = await startBiometricScan();
    } catch(err) {
      throw new Error(err);
    }
    
    overlay.style.background = 'rgba(0,0,0,0.8)';
    overlayText.textContent = "Validation anti-déplacement (GPS final)...";
    
    // GPS_END
    let gps2;
    try {
      gps2 = await getExactGPSPosition();
      document.getElementById('gps-status').innerText = '📍 Position finale vérifiée !';
    } catch(err) {
      throw new Error(err.message || err);
    }
    
    overlayText.textContent = "Envoi sécurisé au serveur...";

    const payload = {
      matricule: matricule,
      code_personnel: codePersonnel,
      qr_token: qrToken,
      session_id: sessionId,
      device_id: getHardwareFingerprint(),
      gps_start_lat: gps1.lat.toString(),
      gps_start_lng: gps1.lng.toString(),
      gps_end_lat: gps2.lat.toString(),
      gps_end_lng: gps2.lng.toString(),
      base64_image: base64Image
    };

    const response = await fetch('/api/presence/mark', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });

    const data = await response.json();
    document.getElementById('biometric-scanner').classList.add('hidden');

    if (data.success) {
      showSuccess(data.etudiant_nom, data.horodatage);
    } else {
      if (data.error && (data.error.includes('HW_') || data.error.includes('Empreinte') || data.error.includes('Biométrique'))) {
         showFraudBlocked(data.error);
      } else {
         showError(data.error || 'Erreur inconnue');
         document.getElementById('presence-form').classList.remove('hidden');
         document.querySelector('.scan-form-title').innerText = "Saisissez vos informations";
      }
    }

  } catch(err) {
    document.getElementById('biometric-scanner').classList.add('hidden');
    document.getElementById('presence-form').classList.remove('hidden');
    document.querySelector('.scan-form-title').innerText = "Saisissez vos informations";
    showError(err.message || err);
  }
}

// ─────────────────────────────────────────────────────────────
// Affichage des états
// ─────────────────────────────────────────────────────────────
function showSuccess(nom, horodatage) {
  document.getElementById('presence-form-container').classList.add('hidden');
  const resultPanel = document.getElementById('scan-result');
  resultPanel.classList.remove('hidden');
  resultPanel.classList.add('success');
  
  document.getElementById('result-icon').innerText = '✅';
  document.getElementById('result-title').innerText = 'Présence Validée';
  document.getElementById('result-desc').innerHTML = `Merci <b>${nom}</b> !<br>Enregistré à ${horodatage}`;
  document.getElementById('status-text').innerText = 'Profil Scellé & Authentifié';

  confetti({ particleCount: 150, spread: 80, origin: { y: 0.6 } });
}

function showFraudBlocked(reason) {
  document.getElementById('presence-form-container').classList.add('hidden');
  document.body.style.background = '#3e0606'; 
  
  const resultPanel = document.getElementById('scan-result');
  resultPanel.classList.remove('hidden');
  resultPanel.style.background = '#fff0f0';
  resultPanel.style.border = '2px solid #ff3333';
  
  document.getElementById('result-icon').innerText = '🚨';
  document.getElementById('result-title').innerText = 'TENTATIVE DE FRAUDE INTERCEPTÉE';
  document.getElementById('result-title').style.color = '#aa0000';
  document.getElementById('result-desc').innerHTML = `
    <div style="text-align:left;font-size:0.85rem;color:#555;">
       <b>Code Raison:</b><br><span style="color:#aa0000;font-weight:bold;">${reason}</span><br><br>
       Le système a analysé le comportement et a intercepté une anomalie biométrique, géographique ou matérielle. 
       Votre action a été tracée dans le journal disciplinaire.
    </div>
  `;
  document.getElementById('status-text').innerText = 'Accès Verrouillé · Traces Enregistrées';
  document.getElementById('status-dot').style.background = '#ff0000';
}

function showError(msg) {
  const errDiv = document.getElementById('form-error');
  errDiv.classList.remove('hidden');
  document.getElementById('error-text').innerText = msg;
}

function hideError() {
  document.getElementById('form-error').classList.add('hidden');
}

function showToast(type, msg) {
  const container = document.getElementById('toast-container');
  const toast = document.createElement('div');
  toast.className = `toast toast-${type}`;
  toast.innerText = msg;
  container.appendChild(toast);
  setTimeout(() => toast.classList.add('show'), 100);
  setTimeout(() => {
    toast.classList.remove('show');
    setTimeout(() => toast.remove(), 300);
  }, 4000);
}

function resetForm() {
  window.location.reload();
}

// ─────────────────────────────────────────────────────────────
// UTILS: Biométrie Faciale et GPS
// ─────────────────────────────────────────────────────────────
function getExactGPSPosition() {
  return new Promise((resolve, reject) => {
    if (!navigator.geolocation) {
      reject('GPS non supporté par ce navigateur.');
      return;
    }
    
    const successCb = (pos) => resolve({lat: pos.coords.latitude, lng: pos.coords.longitude});
    
    const errorCb = (err) => {
      let msg = 'Erreur GPS inconnue';
      if (err.code === 1) msg = 'Localisation refusée. Sur iPhone: Réglages > Safari > Position et mettez ce site sur Autoriser. Rechargez la page.';
      if (err.code === 2) msg = 'Position indisponible (signal GPS trop faible pour valider).';
      if (err.code === 3) msg = 'Temps d`attente dépassé.';
      
      // Essai Basse precision si Timeout ou Impossible
      if (err.code !== 1) {
        navigator.geolocation.getCurrentPosition(successCb, () => reject(msg), { enableHighAccuracy: false, timeout: 8000, maximumAge: 0 });
      } else {
        reject(msg);
      }
    };

    navigator.geolocation.getCurrentPosition(
      successCb,
      errorCb,
      { enableHighAccuracy: true, timeout: 5000, maximumAge: 0 }
    );
  });
}

function computeYaw(landmarks) {
  const nose = landmarks.getNose()[3];
  const jawline = landmarks.getJawOutline();
  const leftEdge = jawline[0];
  const rightEdge = jawline[16];
  const distLeft = nose.x - leftEdge.x;
  const distRight = rightEdge.x - nose.x;
  return distLeft / distRight;
}

function computePitch(landmarks) {
  const positions = landmarks.positions;
  const bridge = positions[27]; // Entre les yeux
  const tip = positions[30];    // Bout du nez
  const chin = positions[8];    // Menton
  
  const distTop = Math.abs(tip.y - bridge.y);
  const distBot = Math.abs(chin.y - tip.y);
  return distBot / (distTop || 1);
}

async function startBiometricScan() {
  return new Promise(async (resolve, reject) => {
    const video = document.getElementById('videoElement');
    const canvas = document.getElementById('faceCanvas');
    const bioStatus = document.getElementById('bio-status');
    const bioSpinner = document.getElementById('bio-spinner');
    
    let stream;
    try {
      stream = await navigator.mediaDevices.getUserMedia({ video: { facingMode: 'user' } });
      video.srcObject = stream;
    } catch(err) {
      return reject("Caméra inaccessible. Vous devez autoriser la caméra.");
    }

    video.addEventListener('play', () => {
      canvas.width = video.videoWidth;
      canvas.height = video.videoHeight;
      const displaySize = { width: video.videoWidth, height: video.videoHeight };
      faceapi.matchDimensions(canvas, displaySize);
      
      const actions = [
        { id: 'gauche', msg: "Tournez la tête à GAUCHE ⬅️" },
        { id: 'droite', msg: "Tournez la tête à DROITE ➡️" }
      ];
      const targetAction = actions[Math.floor(Math.random() * actions.length)];
      let actionReussie = false;
      
      bioStatus.textContent = targetAction.msg;
      bioSpinner.style.display = 'none';

      const scanInterval = setInterval(async () => {
        if (!stream.active) return; 

        const options = new faceapi.TinyFaceDetectorOptions({ inputSize: 224, scoreThreshold: 0.3 });
        const detections = await faceapi.detectAllFaces(video, options)
                                        .withFaceLandmarks()
                                        .withFaceDescriptors();
        
        const resized = faceapi.resizeResults(detections, displaySize);
        canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height);

        if (detections.length === 0) {
           bioStatus.textContent = "Aucun visage détecté";
           return;
        }
        if (detections.length > 1) {
           bioStatus.textContent = "Un seul visage autorisé !";
           return;
        }

        const face = detections[0];
        const landmarks = face.landmarks;
        
        let targetReached = false;
        
        if (targetAction.id === 'gauche') {
           const yaw = computeYaw(landmarks);
           if (yaw > 1.6) targetReached = true; 
        } else if (targetAction.id === 'droite') {
           const yaw = computeYaw(landmarks);
           if (yaw < 0.6) targetReached = true;
        }

        if (targetReached && !actionReussie) { 
            actionReussie = true;
            bioStatus.textContent = "C'est bon ! Ne bougez plus un instant...";
            bioStatus.style.color = "var(--color-success)";
            
            clearInterval(scanInterval);
            
            setTimeout(async () => {
              const finalDet = await faceapi.detectSingleFace(video, options).withFaceLandmarks();
              
              // Capture photo (high res snapshot)
              const captureCanvas = document.createElement('canvas');
              captureCanvas.width = video.videoWidth;
              captureCanvas.height = video.videoHeight;
              captureCanvas.getContext('2d').drawImage(video, 0, 0);
              const b64 = captureCanvas.toDataURL('image/jpeg', 0.90);

              stream.getTracks().forEach(t => t.stop());
              if (!finalDet) reject("Echec de la capture du visage. Re-essayez.");
              else resolve(b64);
            }, 300);
        } else {
             if (!actionReussie) {
                 bioStatus.textContent = targetAction.msg;
                 bioStatus.style.color = "white";
             }
        }
      }, 100);
    });
  });
}
