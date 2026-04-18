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

  const payload = {
    matricule: matricule,
    code_personnel: codePersonnel,
    qr_token: qrToken,
    session_id: sessionId,
    device_id: getHardwareFingerprint()
  };

  document.getElementById('presence-form').classList.add('hidden');
  document.getElementById('biometric-scanner').classList.remove('hidden');
  document.querySelector('.scan-form-title').innerText = "Vérification de sécurité...";

  const overlay = document.getElementById('liveness-overlay');
  const overlayText = document.getElementById('bio-status');
  
  try {
    overlay.style.display = 'flex';
    overlayText.textContent = "Acquisition GPS de départ...";
    
    let gps1;
    try {
      gps1 = await getExactGPSPosition();
      document.getElementById('gps-status').classList.remove('hidden');
      document.getElementById('gps-status').innerText = '?? Position de départ capturée';
    } catch(err) {
      throw new Error("Localisation refusée. Activez le GPS.");
    }
    
    if (!faceModelsLoaded) {
      overlayText.textContent = "Chargement de l'intelligence artificielle...";
      while(!faceModelsLoaded) await new Promise(r => setTimeout(r, 500));
    }
    
    overlay.style.background = 'transparent'; 
    let faceDescriptorArray;
    try {
      faceDescriptorArray = await startBiometricScan();
    } catch(err) {
      throw new Error(err);
    }
    
    overlay.style.background = 'rgba(0,0,0,0.8)';
    overlayText.textContent = "Validation anti-déplacement (GPS final)...";
    
    let gps2;
    try {
      gps2 = await getExactGPSPosition();
      document.getElementById('gps-status').innerText = '?? Position finale vérifiée !';
    } catch(err) {
      throw new Error("Impossible de vérifier la postion finale.");
    }
    
    overlayText.textContent = "Envoi sécurisé au serveur...";

    payload.gps_start_lat = gps1.lat.toString();
    payload.gps_start_lng = gps1.lng.toString();
    payload.gps_end_lat = gps2.lat.toString();
    payload.gps_end_lng = gps2.lng.toString();
    payload.face_descriptor = JSON.stringify(faceDescriptorArray);

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
      if (data.error && (data.error.includes('HW_') || data.error.includes('HW_') || data.error.includes('Empreinte') || data.error.includes('Biométrique'))) {
         showFraudBlocked(data.error);
      } else {
         showError(data.error || 'Erreur inconnue');
         document.getElementById('presence-form').classList.remove('hidden');
      }
    }

  } catch(err) {
    document.getElementById('biometric-scanner').classList.add('hidden');
    document.getElementById('presence-form').classList.remove('hidden');
    showError(err.message || 'Erreur inconnue');
  }
}
