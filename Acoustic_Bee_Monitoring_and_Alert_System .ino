#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <WebServer.h>

// ===== Wi-Fi Credentials =====
const char* ssid = "JTL FAIBA";
const char* password = "Langii7@7";

// ===== SMTP (Gmail) Settings =====
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

const char* emailSenderAccount = "muthui@kabarak.ac.ke";  // Your Gmail
const char* emailSenderPassword = "xawiaruupcsffutj";     // App Password
const char* emailRecipient = "Danielmutua221@gmail.com";   // Recipient

// ===== Noise Monitoring =====
int noiseLevel = 0;
int threshold = 800;
bool alertSent = false;

// New structure to hold full alert details
struct AlertEntry {
    String recipient;
    String timestamp;
    String message;
};

// Variables for Alert Logging History (Circular Buffer)
const int MAX_ALERTS = 5;
AlertEntry alertHistory[MAX_ALERTS]; // Array of the new struct
int alertIndex = 0; // Pointer to the next slot to fill

// ===== Webserver =====
WebServer server(80);

// ===== HTML page with graph (20 readings only) =====
const char* webpage =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"<title>Acoustic Bee Monitoring and Alert System</title>\n"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
"<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n"
"<style>\n"
"body { font-family: Arial, sans-serif; background: linear-gradient(to bottom right, #ffecd2, #fcb69f); display: flex; flex-direction: column; align-items: center; margin:0; padding:20px; }\n"
".display { background: linear-gradient(to right, #ffe259, #ffa751); padding:25px; border-radius:20px; box-shadow:0 8px 20px rgba(0,0,0,0.3); text-align:center; color:#fff; width:300px; margin-bottom:30px; }\n"
"h1 { margin-bottom:15px; text-shadow:2px 2px 5px rgba(0,0,0,0.3); }\n"
".value { font-size:20px; margin:5px 0; font-weight:bold; }\n"
"input { padding:8px; width:100px; margin-right:6px; border-radius:6px; border:none; outline:none; }\n"
"button { padding:8px 12px; border:none; border-radius:6px; background:#ff6f61; color:white; cursor:pointer; font-weight:bold; margin: 5px; }\n"
"button:hover { background:#ff3b2e; }\n"
".alert { color:#ffd700; font-weight:bold; margin-top:10px; text-shadow:1px 1px 3px rgba(0,0,0,0.5); }\n"
".chart-container { background: rgba(255,255,255,0.2); padding:20px; border-radius:15px; width:90%; max-width:500px; }\n"
"#loggedAlertsDisplay { margin-top: 15px; background: rgba(255, 255, 255, 0.2); padding: 10px; border-radius: 10px; max-height: 150px; overflow-y: auto; text-align: left; }\n"
"#alertList li { border-bottom: 1px solid rgba(255,255,255,0.3); padding: 5px 0; font-size: 14px; white-space: pre-wrap; }\n" /* Added white-space for multi-line content */
"#alertList li:last-child { border-bottom: none; }\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div class=\"display\">\n"
"<h1>Acoustic Bee Monitoring and Alert System</h1>\n"
"<p class=\"value\">Noise Level: <span id=\"noiseValue\">--</span></p>\n"
"<p class=\"value\">Threshold: <span id=\"thresholdValue\">--</span></p>\n"
"<input id=\"thresholdInput\" type=\"number\" min=\"0\" max=\"1023\">\n"
"<button onclick=\"updateThreshold()\">Set Threshold</button>\n"
"<p id=\"alertMsg\" class=\"alert\"></p>\n"
"<button onclick=\"toggleAlerts()\">Logged Alerts</button>\n" 
"<div id=\"loggedAlertsDisplay\" style=\"display: none;\">\n"
"  <p style=\"margin: 0 0 5px 0; font-weight: bold; color: #fff;\">Logged Alerts (Newest First):</p>\n" /* Heading updated */
"  <ul id=\"alertList\" style=\"list-style: none; padding: 0;\"></ul>\n"
"</div>\n"
"</div>\n"
"<div class=\"chart-container\">\n"
"<canvas id=\"noiseChart\"></canvas>\n"
"</div>\n"
"<script>\n"
"const ctx = document.getElementById('noiseChart').getContext('2d');\n"
"const dataPoints = [];\n"
"const maxPoints = 20;\n"
"const noiseChart = new Chart(ctx, { type:'line', data:{ labels:[], datasets:[{ label:'Noise Level', data:dataPoints, borderColor:'#ff3b2e', backgroundColor:'rgba(255,59,46,0.2)', tension:0.3 }]}, options:{ animation:false, scales:{ y:{ min:0, max:1023 }}}});\n"
"async function refreshData() {\n"
"  const res = await fetch('/data');\n"
"  const data = await res.json();\n"
"  \n"
"  document.getElementById('noiseValue').innerText = data.noise;\n"
"  document.getElementById('thresholdValue').innerText = data.threshold;\n"
"  document.getElementById('alertMsg').innerText = data.alert ? 'Noise exceeded!' : '';\n"
"  \n"
"  // Update the alert list\n"
"  const alertList = document.getElementById('alertList');\n"
"  alertList.innerHTML = ''; // Clear previous entries\n"
"  \n"
"  // The C++ sends an array of objects. We reverse it to show the newest alert at the top.\n"
"  const validAlerts = data.alertHistory.filter(entry => entry !== null); // Filter out null placeholders\n"
"  \n"
"  if (validAlerts.length === 0) {\n"
"    alertList.innerHTML = '<li>No alerts logged yet.</li>';\n"
"  } else {\n"
"    validAlerts.slice().reverse().forEach(alert => {\n"
"      const listItem = document.createElement('li');\n"
"      // Display the detailed information\n"
"      listItem.innerHTML = \n"
"        `<strong>Time:</strong> ${alert.time}<br>` +\n"
"        `<strong>To:</strong> ${alert.recipient}<br>` +\n"
"        `<strong>Msg:</strong> ${alert.msg}`;\n"
"      alertList.appendChild(listItem);\n"
"    });\n"
"  }\n"
"\n"
"  dataPoints.push(data.noise);\n"
"  if (dataPoints.length > maxPoints) dataPoints.shift();\n"
"  noiseChart.data.labels = dataPoints.map((_, i)=>i+1);\n"
"  noiseChart.update();\n"
"}\n"
"async function updateThreshold() {\n"
"  const value = document.getElementById('thresholdInput').value;\n"
"  await fetch(`/data?threshold=${value}`);\n"
"  refreshData();\n"
"}\n"
"function toggleAlerts() {\n"
"  const display = document.getElementById('loggedAlertsDisplay');\n"
"  display.style.display = display.style.display === 'none' || display.style.display === '' ? 'block' : 'none';\n"
"}\n"
"setInterval(refreshData, 1000);\n"
"refreshData();\n"
"</script>\n"
"</body>\n"
"</html>\n";



// ===== Functions =====
int readNoise() {
  return random(200, 900); // simulate noise
}

String getTimestamp() {
  unsigned long totalSeconds = millis() / 1000;
  int h = (totalSeconds / 3600) % 24;
  int m = (totalSeconds / 60) % 60;
  int s = totalSeconds % 60;
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  return String(buf);
}

void sendEmailAlert(String msg) {
  SMTPSession smtp;
  SMTP_Message email;

  email.sender.name = "ESP32 Mailer";
  email.sender.email = emailSenderAccount;
  email.subject = "Noise Alert";
  email.addRecipient("Recipient", emailRecipient);
  email.text.content = msg.c_str();

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = emailSenderAccount;
  session.login.password = emailSenderPassword;
  session.login.user_domain = "";

  if (!smtp.connect(&session)) {
    Serial.println("SMTP Connect failed!");
    return;
  }

  if (!MailClient.sendMail(&smtp, &email)) {
    Serial.print("Error sending Email: "); Serial.println(smtp.errorReason());
  } else {
    Serial.println("Email sent successfully!");
  }

  smtp.closeSession();
}

// ===== Webserver Routes =====
void handleRoot() {
  server.send(200, "text/html", webpage);
}

void handleData() {
  if (server.hasArg("threshold"))
    threshold = server.arg("threshold").toInt();

  noiseLevel = readNoise();
  bool alert = noiseLevel > threshold;

  if (alert && !alertSent) {
    alertSent = true;
    String currentTimestamp = getTimestamp();
    String alertMessage = "Bee Hive Alert! Noise level Exceeded!! Noise level: " + String(noiseLevel) + " at " + currentTimestamp;
    
    sendEmailAlert(alertMessage);
    
    // Log the complete alert entry
    alertHistory[alertIndex].recipient = emailRecipient;
    alertHistory[alertIndex].timestamp = currentTimestamp;
    alertHistory[alertIndex].message = alertMessage;
    alertIndex = (alertIndex + 1) % MAX_ALERTS;
  }
  if (!alert) alertSent = false;

  // Format the entire history array into a single JSON array string
  String historyJson = "[";
  for (int i = 0; i < MAX_ALERTS; i++) {
    if (i > 0) historyJson += ",";

    if (alertHistory[i].timestamp.length() > 0) {
        // Simple string replacement for JSON safety (double quotes)
        String escapedMessage = alertHistory[i].message;
        escapedMessage.replace("\"", "\\\""); 

        historyJson += "{";
        historyJson += "\"recipient\":\"" + alertHistory[i].recipient + "\",";
        historyJson += "\"time\":\"" + alertHistory[i].timestamp + "\",";
        historyJson += "\"msg\":\"" + escapedMessage + "\"";
        historyJson += "}";
    } else {
        // Use 'null' to clearly mark empty slots in the JSON array
        historyJson += "null"; 
    }
  }
  historyJson += "]";

  // Send the history array in the JSON response
  String json = "{\"noise\":" + String(noiseLevel) +
                ",\"threshold\":" + String(threshold) +
                ",\"alert\":" + String(alert ? "true" : "false") +
                ",\"alertHistory\":" + historyJson + "}"; // New field for the list

  server.send(200, "application/json", json);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);

  // Initialize the history array
  for (int i = 0; i < MAX_ALERTS; i++) {
    alertHistory[i].recipient = "";
    alertHistory[i].timestamp = "";
    alertHistory[i].message = "";
  }
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected!");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Webserver started");
}

void loop() {
  server.handleClient();
}
