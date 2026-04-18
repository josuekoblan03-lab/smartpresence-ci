const fs = require('fs');
const path = require('path');

function fixFile(fileRel) {
  const file = path.join(__dirname, fileRel);
  if (!fs.existsSync(file)) return;
  console.log("Fixing " + file);
  let str = fs.readFileSync(file, 'utf8');

  // Try decoding double-UTF8 if needed
  try {
    let testDecode = Buffer.from(str, 'latin1').toString('utf8');
    if (testDecode.includes('é') || testDecode.includes('è') || testDecode.includes('à')) {
      str = testDecode;
      console.log('Decoded Latin1 -> UTF8 in ' + fileRel);
    }
  } catch (e) {}

  // Manual fallback replacements
  str = str.replace(/Ã©/g, 'é')
           .replace(/Ã¨/g, 'è')
           .replace(/Ãª/g, 'ê')
           .replace(/Ã¢/g, 'â')
           .replace(/Ã´/g, 'ô')
           .replace(/Ã®/g, 'î')
           .replace(/Ã/g, 'à');

  // Mettre à jour les Emojis cassés du screenshot
  str = str.replace(/ðŸ"¢/g, '🔢')
           .replace(/ðŸŽ"/g, '🎓')
           .replace(/ðŸ""/g, '📋')
           .replace(/ðŸ"\|/g, '🔑')
           .replace(/ðŸ"±/g, '📱')
           .replace(/ðŸ”’/g, '🔒')
           .replace(/ðŸ“‹/g, '📋')
           .replace(/âœ"/g, '✓')
           .replace(/âœ¨/g, '✨')
           .replace(/ðŸš€/g, '🚀')
           .replace(/âš /g, '⚠️');

  fs.writeFileSync(file, str, 'utf-8');
}

['public/scan.html', 'public/assets/js/scan.js', 'public/assets/js/app.js'].forEach(fixFile);
