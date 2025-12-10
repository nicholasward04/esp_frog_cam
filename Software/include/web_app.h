static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>ESP32 Stream</title>
  <style>
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    html, body {
      width: 100%;
      height: 100%;
      font-family: Sans-serif;
    }

    body {
      display: flex;
      flex-direction: column;
      background: linear-gradient(135deg, #006994 0%, #87CEEB 100%);
    }

    #video-container {
      height: 80%;
      width: 100%;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
    }

    #video-stream {
      max-width: 90%;
      max-height: 90%;
      
      width: auto;
      height: auto;
      
      object-fit: contain;
      
      border: 16px solid #81C784; 
      border-radius: 20px;
      
      background-color: #000;
      display: block;
      box-shadow: 0 10px 20px rgba(0,0,0,0.3);
    }

    #controls-container {
      height: 20%;
      width: 100%;
      display: flex;
      justify-content: space-evenly;
      align-items: center;
      
      background: rgba(255, 255, 255, 0.15);
      backdrop-filter: blur(5px);
      border-top: 1px solid rgba(255,255,255,0.2);
    }

    .ctrl-btn {
      background-color: #81C784;
      border: 8px solid #2E7D32;
      border-radius: 15px;
      
      color: white;
      font-size: clamp(1rem, 4.5vw, 2rem);
      font-weight: bold;
      text-transform: uppercase;
      cursor: pointer;
      
      width: 40%;
      height: 60%;
      
      box-shadow: 0 4px 6px rgba(0,0,0,0.2);
    }

    .ctrl-btn:active {
      transform: scale(0.95);
      background-color: #66BB6A;
    }

  </style>
</head>
<body>

  <div id="video-container">
    <img id="video-stream" src="" alt="Connecting...">
  </div>

  <div id="controls-container">
    <button class="ctrl-btn" onclick="fetch('/led/toggle')">
      Toggle LED
    </button>
    <button class="ctrl-btn" onclick="fetch('/servo/wave')">
      Wave Hand
    </button>
  </div>

  <script>
    const view = document.getElementById("video-stream");
    let isRequesting = false;

    function refreshImage() {
        if (isRequesting) return; 
        isRequesting = true;
        const tempImage = new Image();
        tempImage.src = "/capture?t=" + new Date().getTime();
        
        tempImage.onload = function() {
            view.src = tempImage.src;
            isRequesting = false;
            setTimeout(refreshImage, 20); 
        };
    }
    refreshImage();
  </script>

</body>
</html>
)rawliteral";