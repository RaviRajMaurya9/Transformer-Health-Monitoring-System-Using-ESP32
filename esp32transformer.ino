#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ==================== WiFi Credentials ====================
const char* ssid = "ABCD";
const char* password = "123456789";

// ==================== Pin Definitions ====================
#define DHTPIN         4
#define DHTTYPE        DHT11
DHT dht(DHTPIN, DHTTYPE);

#define TRIG_PIN       15
#define ECHO_PIN       16

#define MQ2_PIN        34
#define MQ4_PIN        35
#define MQ5_PIN        32
#define MQ7_PIN        33
#define MQ8_PIN        25

#define FAN1_PIN       26
#define FAN2_PIN       27
#define FAN3_PIN       14
#define RELAY_PIN      12

// ==================== Thresholds ====================
const float TEMP_FAN1 = 30.0;
const float TEMP_FAN2 = 50.0;
const float TEMP_FAN3 = 90.0;
const float TEMP_TRIP = 100.0;

const int GAS_THRESHOLD_MQ2 = 2000;
const int GAS_THRESHOLD_MQ4 = 2000;
const int GAS_THRESHOLD_MQ5 = 2000;
const int GAS_THRESHOLD_MQ7 = 2000;
const int GAS_THRESHOLD_MQ8 = 2000;

const float OIL_TANK_HEIGHT_CM = 30.0;

// ==================== Global Variables ====================
float temperature = 0.0;
int oilLevel = 0;
int gasMQ2 = 0, gasMQ4 = 0, gasMQ5 = 0, gasMQ7 = 0, gasMQ8 = 0;
bool fan1 = false, fan2 = false, fan3 = false;
bool relayTripped = false;
bool gasAlert = false;

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 1000;

WebServer server(80);

// ==================== Sensor Reading ====================
int readOilLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 0;
  
  float distance = duration * 0.034 / 2.0;
  int percent = (int)((OIL_TANK_HEIGHT_CM - distance) / OIL_TANK_HEIGHT_CM * 100);
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  return percent;
}

void readSensors() {
  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println("DHT read error");
    t = temperature;
  }
  temperature = t;

  oilLevel = readOilLevel();

  gasMQ2 = analogRead(MQ2_PIN);
  gasMQ4 = analogRead(MQ4_PIN);
  gasMQ5 = analogRead(MQ5_PIN);
  gasMQ7 = analogRead(MQ7_PIN);
  gasMQ8 = analogRead(MQ8_PIN);

  gasAlert = (gasMQ2 > GAS_THRESHOLD_MQ2) ||
             (gasMQ4 > GAS_THRESHOLD_MQ4) ||
             (gasMQ5 > GAS_THRESHOLD_MQ5) ||
             (gasMQ7 > GAS_THRESHOLD_MQ7) ||
             (gasMQ8 > GAS_THRESHOLD_MQ8);

  fan1 = (temperature >= TEMP_FAN1);
  fan2 = (temperature >= TEMP_FAN2);
  fan3 = (temperature >= TEMP_FAN3);

  digitalWrite(FAN1_PIN, fan1 ? HIGH : LOW);
  digitalWrite(FAN2_PIN, fan2 ? HIGH : LOW);
  digitalWrite(FAN3_PIN, fan3 ? HIGH : LOW);

  if (temperature > TEMP_TRIP) {
    relayTripped = true;
  }
  digitalWrite(RELAY_PIN, relayTripped ? HIGH : LOW);
}

// ==================== Web Handlers ====================
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes">
    <title>Transformer Health Monitor | Secure IoT Dashboard</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Inter', sans-serif; }
        body {
            min-height: 100vh;
            background: radial-gradient(circle at 20% 30%, #0a2f44, #031a2b);
            background-attachment: fixed;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
            position: relative;
            overflow-x: hidden;
        }
        .particle {
            position: fixed;
            background: rgba(255,255,255,0.1);
            border-radius: 50%;
            pointer-events: none;
            animation: floatParticle linear infinite;
            z-index: 0;
        }
        @keyframes floatParticle {
            0% { transform: translateY(100vh) rotate(0deg); opacity: 0; }
            10% { opacity: 0.5; }
            90% { opacity: 0.5; }
            100% { transform: translateY(-20vh) rotate(360deg); opacity: 0; }
        }
        .login-overlay {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0,0,0,0.85);
            backdrop-filter: blur(12px);
            display: flex;
            justify-content: center;
            align-items: center;
            z-index: 1000;
            transition: 0.3s;
        }
        .login-card {
            background: rgba(255,255,255,0.1);
            border-radius: 32px;
            padding: 40px 30px;
            width: 90%;
            max-width: 400px;
            text-align: center;
            border: 1px solid rgba(255,255,255,0.2);
            backdrop-filter: blur(20px);
            box-shadow: 0 20px 40px rgba(0,0,0,0.4);
            animation: fadeInUp 0.5s ease;
        }
        .login-card i { font-size: 4rem; color: #ffb347; margin-bottom: 20px; }
        .login-card h2 { color: white; margin-bottom: 10px; font-weight: 700; }
        .login-card p { color: #ccc; margin-bottom: 25px; font-size: 0.9rem; }
        .login-card input {
            width: 100%;
            padding: 14px 18px;
            margin-bottom: 20px;
            background: rgba(255,255,255,0.2);
            border: 1px solid rgba(255,255,255,0.3);
            border-radius: 50px;
            color: white;
            font-size: 1rem;
            outline: none;
        }
        .login-card input:focus { border-color: #ffb347; background: rgba(255,255,255,0.3); }
        .login-card button {
            background: linear-gradient(135deg, #ffb347, #ff6b35);
            border: none;
            width: 100%;
            padding: 12px;
            border-radius: 50px;
            font-weight: bold;
            font-size: 1rem;
            color: white;
            cursor: pointer;
            transition: 0.2s;
        }
        .login-card button:hover { transform: scale(1.02); box-shadow: 0 5px 15px rgba(255,107,53,0.4); }
        .error-msg { color: #ff6b6b; margin-top: 15px; font-size: 0.85rem; display: none; }
        .dashboard { max-width: 1300px; width: 100%; margin: 0 auto; position: relative; z-index: 2; display: none; }
        .header {
            text-align: center;
            margin-bottom: 35px;
            background: rgba(0,0,0,0.4);
            padding: 20px;
            border-radius: 60px;
            backdrop-filter: blur(8px);
            border: 1px solid rgba(255,255,255,0.15);
        }
        .header h1 {
            font-size: 2rem;
            font-weight: 800;
            background: linear-gradient(135deg, #fff, #ffb347);
            -webkit-background-clip: text;
            background-clip: text;
            color: transparent;
        }
        .header h1 i { background: none; color: #ffb347; margin-right: 12px; }
        .header p { color: #ddd; margin-top: 8px; font-weight: 500; }
        .alert-banner {
            background: linear-gradient(95deg, #dc3545, #b02a37);
            color: white;
            padding: 14px 20px;
            border-radius: 60px;
            text-align: center;
            margin-bottom: 20px;
            display: none;
            font-weight: 600;
            box-shadow: 0 4px 12px rgba(220,53,69,0.3);
        }
        .alert-banner i { margin-right: 10px; font-size: 1.2rem; }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 24px;
            margin-bottom: 30px;
        }
        .card {
            background: rgba(15, 25, 45, 0.7);
            backdrop-filter: blur(12px);
            border-radius: 28px;
            padding: 22px;
            border: 1px solid rgba(255,255,255,0.2);
            transition: all 0.25s ease;
            box-shadow: 0 10px 20px rgba(0,0,0,0.2);
            color: #fff;   /* <-- THIS MAKES ALL DEFAULT TEXT WHITE */
        }
        .card:hover {
            transform: translateY(-6px);
            border-color: rgba(255,180,70,0.6);
            box-shadow: 0 20px 30px rgba(0,0,0,0.3);
        }
        .card-title {
            font-size: 1.2rem;
            font-weight: 600;
            margin-bottom: 18px;
            color: #ffb347;
            display: flex;
            align-items: center;
            gap: 12px;
            border-left: 3px solid #ffb347;
            padding-left: 14px;
        }
        .card-title i { font-size: 1.4rem; }
        .value {
            font-size: 2.5rem;
            font-weight: 800;
            margin: 12px 0;
            color: white;
        }
        .unit { font-size: 1rem; font-weight: 500; color: #aaa; }
        .progress-bar {
            background: rgba(255,255,255,0.2);
            border-radius: 30px;
            height: 12px;
            margin: 15px 0;
            overflow: hidden;
        }
        .progress-fill {
            height: 100%;
            width: 0%;
            border-radius: 30px;
            transition: width 0.4s ease;
        }
        .fan-status {
            font-size: 2rem;
            display: flex;
            gap: 20px;
            margin: 15px 0;
        }
        .fa-fan.on {
            color: #ffb347;
            animation: spin 1.2s linear infinite;
            text-shadow: 0 0 8px #ffb347;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .relay-status {
            display: flex;
            align-items: center;
            gap: 14px;
            font-size: 1.5rem;
            font-weight: bold;
            margin-top: 10px;
        }
        .normal { color: #2ecc71; }
        .tripped { color: #e74c3c; }
        .gas-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        .gas-name i { width: 30px; color: #ffb347; }
        .gas-value { font-weight: 600; }
        .gas-value.normal { color: #2ecc71; }
        .gas-value.alert { color: #e74c3c; font-weight: bold; text-shadow: 0 0 4px #e74c3c; }
        .reset-btn { text-align: center; margin-top: 25px; }
        button {
            background: linear-gradient(135deg, #ffb347, #ff6b35);
            border: none;
            padding: 12px 32px;
            font-size: 1rem;
            font-weight: 600;
            border-radius: 50px;
            color: white;
            cursor: pointer;
            transition: 0.2s;
            box-shadow: 0 4px 10px rgba(0,0,0,0.2);
        }
        button:disabled { background: #555; cursor: not-allowed; opacity: 0.6; }
        button:hover:not(:disabled) { transform: scale(1.02); box-shadow: 0 8px 20px rgba(255,107,53,0.4); }
        .footer {
            text-align: center;
            color: rgba(255,255,255,0.6);
            margin-top: 40px;
            font-size: 0.75rem;
            padding: 15px;
        }
        @media (max-width: 700px) {
            .grid { gap: 16px; }
            .value { font-size: 2rem; }
            .card { padding: 18px; }
        }
        @keyframes fadeInUp {
            from { opacity: 0; transform: translateY(30px); }
            to { opacity: 1; transform: translateY(0); }
        }
    </style>
</head>
<body>
    <div id="loginOverlay" class="login-overlay">
        <div class="login-card">
            <i class="fas fa-shield-alt"></i>
            <h2>Secure Access</h2>
            <p>Enter password to view transformer dashboard</p>
            <input type="password" id="loginPass" placeholder="Password" autocomplete="off">
            <button onclick="checkLogin()">Unlock Dashboard <i class="fas fa-arrow-right"></i></button>
            <div id="loginError" class="error-msg">❌ Incorrect password. Try again.</div>
        </div>
    </div>
    <div id="dashboard" class="dashboard">
        <div class="header">
            <h1><i class="fas fa-bolt"></i> TRANSFORMER HEALTH MONITOR</h1>
            <p><i class="fas fa-chart-line"></i> Real‑time protection & diagnostics | ESP32 IoT</p>
        </div>
        <div id="tempAlert" class="alert-banner"><i class="fas fa-temperature-high"></i> 🔥 HIGH TEMPERATURE! >50°C</div>
        <div id="oilAlert" class="alert-banner"><i class="fas fa-oil-can"></i> 🚨 LOW OIL LEVEL! Below 80%</div>
        <div id="gasAlert" class="alert-banner"><i class="fas fa-skull-crosswalk"></i> 💨 GAS ALERT! Dangerous levels detected</div>
        <div class="grid" id="dataGrid"></div>
        <div class="reset-btn">
            <button id="resetRelayBtn" onclick="resetRelay()"><i class="fas fa-power-off"></i> Reset Protection Relay</button>
        </div>
        <div class="footer">
            <i class="fas fa-microchip"></i> ESP32 Secure Gateway | <i class="far fa-clock"></i> <span id="liveTime"></span>
        </div>
    </div>
    <script>
        const PASSWORD = 'admin123';
        function checkLogin() {
            const pass = document.getElementById('loginPass').value;
            if (pass === PASSWORD) {
                document.getElementById('loginOverlay').style.display = 'none';
                document.getElementById('dashboard').style.display = 'block';
                fetchData();
                setInterval(fetchData, 2000);
                updateClock();
                setInterval(updateClock, 1000);
            } else {
                document.getElementById('loginError').style.display = 'block';
                setTimeout(() => { document.getElementById('loginError').style.display = 'none'; }, 2000);
            }
        }
        document.getElementById('loginPass').addEventListener('keypress', function(e) {
            if (e.key === 'Enter') checkLogin();
        });
        function updateClock() {
            const now = new Date();
            document.getElementById('liveTime').innerText = now.toLocaleTimeString();
        }
        function fetchData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    let html = `
                        <div class="card">
                            <div class="card-title"><i class="fas fa-thermometer-three-quarters"></i> Core Temperature</div>
                            <div class="value" style="color: ${getTempColor(data.temp)};">${data.temp.toFixed(1)}<span class="unit"> °C</span></div>
                            <div class="progress-bar"><div class="progress-fill" style="width: ${Math.min(100, data.temp/1.2)}%; background-color: ${getTempColor(data.temp)};"></div></div>
                            <div><i class="fas fa-tachometer-alt"></i> Fan stage: ${data.fan1 ? '1' : '0'} | ${data.fan2 ? '2' : '0'} | ${data.fan3 ? '3' : '0'}</div>
                        </div>
                        <div class="card">
                            <div class="card-title"><i class="fas fa-oil-can"></i> Dielectric Oil Level</div>
                            <div class="value">${data.oil}<span class="unit"> %</span></div>
                            <div class="progress-bar"><div class="progress-fill" style="width: ${data.oil}%; background-color: ${getOilColor(data.oil)};"></div></div>
                            <i class="fas fa-tint"></i> ${data.oil < 80 ? '⚠️ Refill recommended' : '✓ Optimal'}
                        </div>
                        <div class="card">
                            <div class="card-title"><i class="fas fa-fan"></i> Active Cooling</div>
                            <div class="fan-status">
                                <i class="fas fa-fan ${data.fan1 ? 'on' : ''}"></i> Fan 1
                                <i class="fas fa-fan ${data.fan2 ? 'on' : ''}"></i> Fan 2
                                <i class="fas fa-fan ${data.fan3 ? 'on' : ''}"></i> Fan 3
                            </div>
                            <div>Status: ${data.fan1 ? 'ON' : 'OFF'} | ${data.fan2 ? 'ON' : 'OFF'} | ${data.fan3 ? 'ON' : 'OFF'}</div>
                        </div>
                        <div class="card">
                            <div class="card-title"><i class="fas fa-shield-virus"></i> Overload Relay</div>
                            <div class="relay-status">
                                ${data.relay ? '<i class="fas fa-ban tripped"></i><span class="tripped">TRIPPED (Supply Cut)</span>' : '<i class="fas fa-check-circle normal"></i><span class="normal">NORMAL (Energized)</span>'}
                            </div>
                            ${data.relay ? '<small><i class="fas fa-exclamation-circle"></i> Manual reset required after cooldown</small>' : '<small><i class="fas fa-check"></i> No fault</small>'}
                        </div>
                    `;
                    const gasSensors = [
                        { name: 'MQ-2', icon: 'fire', value: data.mq2, threshold: 2000, label: 'Combustible Gas' },
                        { name: 'MQ-4', icon: 'seedling', value: data.mq4, threshold: 2000, label: 'Methane (CH₄)' },
                        { name: 'MQ-5', icon: 'gas-pump', value: data.mq5, threshold: 2000, label: 'LPG / Natural Gas' },
                        { name: 'MQ-7', icon: 'smog', value: data.mq7, threshold: 2000, label: 'Carbon Monoxide (CO)' },
                        { name: 'MQ-8', icon: 'flask', value: data.mq8, threshold: 2000, label: 'Hydrogen (H₂)' }
                    ];
                    html += `<div class="card" style="grid-column: span 2;">
                                <div class="card-title"><i class="fas fa-microscope"></i> Dissolved Gas Analysis (DGA)</div>`;
                    gasSensors.forEach(gas => {
                        const isAlert = gas.value > gas.threshold;
                        html += `<div class="gas-item">
                                    <span class="gas-name"><i class="fas fa-${gas.icon}"></i> ${gas.name} (${gas.label})</span>
                                    <span class="gas-value ${isAlert ? 'alert' : 'normal'}">${gas.value} <span class="unit">ppm*</span></span>
                                </div>`;
                    });
                    html += `<div class="gas-item"><i class="fas fa-chart-simple"></i> <small>*Raw ADC values – calibration recommended</small></div>
                            </div>`;
                    document.getElementById('dataGrid').innerHTML = html;
                    document.getElementById('tempAlert').style.display = data.temp > 50 ? 'flex' : 'none';
                    document.getElementById('oilAlert').style.display = data.oil < 80 ? 'flex' : 'none';
                    document.getElementById('gasAlert').style.display = data.gasAlert ? 'flex' : 'none';
                    const resetBtn = document.getElementById('resetRelayBtn');
                    resetBtn.disabled = !(data.relay && data.temp <= 100);
                })
                .catch(err => console.error('Fetch error:', err));
        }
        function getTempColor(t) {
            if (t < 30) return '#2ecc71';
            if (t < 50) return '#f39c12';
            return '#e74c3c';
        }
        function getOilColor(level) {
            if (level < 30) return '#e74c3c';
            if (level < 60) return '#f39c12';
            return '#2ecc71';
        }
        function resetRelay() {
            fetch('/resetRelay')
                .then(r => r.text())
                .then(msg => { alert(msg); fetchData(); })
                .catch(err => alert('Reset failed: ' + err));
        }
        function createParticles() {
            for(let i=0;i<40;i++) {
                let p = document.createElement('div');
                p.className = 'particle';
                let size = Math.random()*6+2;
                p.style.width = size+'px';
                p.style.height = size+'px';
                p.style.left = Math.random()*100+'%';
                p.style.animationDuration = Math.random()*10+5+'s';
                p.style.animationDelay = Math.random()*5+'s';
                p.style.background = `rgba(255,180,70,${Math.random()*0.3+0.1})`;
                document.body.appendChild(p);
            }
        }
        createParticles();
    </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleData() {
  StaticJsonDocument<512> doc;
  doc["temp"] = temperature;
  doc["oil"] = oilLevel;
  doc["fan1"] = fan1;
  doc["fan2"] = fan2;
  doc["fan3"] = fan3;
  doc["relay"] = relayTripped;
  doc["mq2"] = gasMQ2;
  doc["mq4"] = gasMQ4;
  doc["mq5"] = gasMQ5;
  doc["mq7"] = gasMQ7;
  doc["mq8"] = gasMQ8;
  doc["gasAlert"] = gasAlert;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleResetRelay() {
  if (temperature <= TEMP_TRIP) {
    relayTripped = false;
    digitalWrite(RELAY_PIN, LOW);
    server.send(200, "text/plain", "Relay reset. System normal.");
  } else {
    server.send(200, "text/plain", "Cannot reset: temperature still above 100°C!");
  }
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  dht.begin();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(FAN1_PIN, OUTPUT);
  pinMode(FAN2_PIN, OUTPUT);
  pinMode(FAN3_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(FAN1_PIN, LOW);
  digitalWrite(FAN2_PIN, LOW);
  digitalWrite(FAN3_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP address: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/resetRelay", handleResetRelay);

  server.begin();
  Serial.println("HTTP server started");
}

// ==================== Main Loop ====================
void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastSensorRead >= sensorInterval) {
    readSensors();
    lastSensorRead = now;

    Serial.printf("Temp: %.2f°C, Oil: %d%%, MQ2: %d, MQ4: %d, MQ5: %d, MQ7: %d, MQ8: %d, GasAlert: %d\n",
                  temperature, oilLevel, gasMQ2, gasMQ4, gasMQ5, gasMQ7, gasMQ8, gasAlert);
  }
}