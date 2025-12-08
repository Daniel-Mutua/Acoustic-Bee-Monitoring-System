#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <WebServer.h>

// ===== Wi-Fi Credentials =====
const char* ssid = "WI-FI name";
const char* password = "WI-FI password";

// ===== SMTP (Gmail) Settings =====
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

const char* emailSenderAccount = "your gmail account";  
const char* emailSenderPassword = "****************";     
const char* emailRecipient = "recipient's email";   

// ===== KY-037 Sound Sensor =====
#define SOUND_SENSOR_PIN 34   // Analog pin for KY-037 AO

int noiseLevel = 0;
int threshold = 800;
bool alertSent = false;

// ===== Alert Structure =====
struct AlertEntry {
    String recipient;
    String timestamp;
    String message;
};

// Alert Log History
const int MAX_ALERTS = 5;
AlertEntry alertHistory[MAX_ALERTS];
int alertIndex = 0;

// ===== Webserver =====
WebServer server(80);

// ===== HTML PAGE =====


// ===================================================
// REAL NOISE READING FROM KY-037
// ===================================================
int readNoise() {
    int raw = analogRead(SOUND_SENSOR_PIN);

    // Optionally convert raw 0–4095 to 0–1023 to match your UI scale
    int scaled = map(raw, 0, 4095, 0, 1023);

    return scaled;
}

// ===================================================
// TIMESTAMP
// ===================================================
String getTimestamp() {
  unsigned long totalSeconds = millis() / 1000;
  int h = (totalSeconds / 3600) % 24;
  int m = (totalSeconds / 60) % 60;
  int s = totalSeconds % 60;
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  return String(buf);
}

// ===================================================
// SEND EMAIL ALERT
// ===================================================
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

  if (!smtp.connect(&session)) return;

  MailClient.sendMail(&smtp, &email);
  smtp.closeSession();
}

// ===================================================
// SERVE HOME PAGE
// ===================================================
void handleRoot() {
  server.send(200, "text/html", webpage);
}

// ===================================================
// SEND SENSOR + ALERT JSON DATA
// ===================================================
void handleData() {
  if (server.hasArg("threshold"))
    threshold = server.arg("threshold").toInt();

  noiseLevel = readNoise();
  bool alert = noiseLevel > threshold;

  if (alert && !alertSent) {
    alertSent = true;
    String currentTimestamp = getTimestamp();
    String alertMessage = "Bee Hive Alert! Noise level exceeded! Noise level: " 
                          + String(noiseLevel) + " at " + currentTimestamp;

    sendEmailAlert(alertMessage);

    alertHistory[alertIndex].recipient = emailRecipient;
    alertHistory[alertIndex].timestamp = currentTimestamp;
    alertHistory[alertIndex].message = alertMessage;

    alertIndex = (alertIndex + 1) % MAX_ALERTS;
  }

  if (!alert) alertSent = false;

  // Format alert history JSON
  String historyJson = "[";
  for (int i = 0; i < MAX_ALERTS; i++) {
      if (i > 0) historyJson += ",";

      if (alertHistory[i].timestamp.length() > 0) {
          String escapedMessage = alertHistory[i].message;
          escapedMessage.replace("\"", "\\\"");

          historyJson += "{";
          historyJson += "\"recipient\":\"" + alertHistory[i].recipient + "\",";
          historyJson += "\"time\":\"" + alertHistory[i].timestamp + "\",";
          historyJson += "\"msg\":\"" + escapedMessage + "\"";
          historyJson += "}";
      } else {
          historyJson += "null";
      }
  }
  historyJson += "]";

  String json = "{\"noise\":" + String(noiseLevel) +
                ",\"threshold\":" + String(threshold) +
                ",\"alert\":" + (alert ? "true" : "false") +
                ",\"alertHistory\":" + historyJson + "}";

  server.send(200, "application/json", json);
}

// ===================================================
// SETUP
// ===================================================
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < MAX_ALERTS; i++) {
    alertHistory[i].recipient = "";
    alertHistory[i].timestamp = "";
    alertHistory[i].message = "";
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

// ===================================================
// LOOP
// ===================================================
void loop() {
  server.handleClient();
}
