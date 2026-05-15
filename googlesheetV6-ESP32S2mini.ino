/*
Cambios en la versión 6.4 VERSIÓN PARA ESP32 S2 mini
 se incluye:
analiza si cada dato es distinto del anterior, y sólo envía el dato si el cambio es significativo.	
descarga desde GitHub
Cambio el pin led al 15, porque el 14 no funciona
Ya no promedio 50 lecturas, sino 10
*/


#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiMulti.h>
#include <WiFiManager.h>
#include <HTTPUpdate.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>  //para almacenar vbles en ESP32 memoria no volátil

Adafruit_ADS1115 ads; // Creamos la instancia del objeto ADS1115
WiFiMulti wifiMulti;

#define DHTPIN 5 // en el ESP32 es 4
#define DHTTYPE DHT11
#define SDA_PIN 8 // para el ESP32 S2 mini . (probar 8/9 o 33/34)
#define SCL_PIN 9 // para el ESP32 S2 mini
DHT dht(DHTPIN, DHTTYPE);

const float VERSION_ACTUAL = 6.4;

//const float NIVEL_MAX = 15.0;    // Nivel máximo del sensor en metros
//const float I_MIN = 4.0;         // Corriente mínima sensor (0 metros)
//const float I_MAX = 20.0;        // Corriente máxima sensor (15 metros)
//const float V_MAX = 10.0;// tensión máxima del regulador 4-20 a 0-10V (jumper 2 quitado. Con los dos jumper puestos, sería 0-3.3V)
const int ledPin = 15; // en el ESP32, era el 2
char scriptURL[150] = "";
bool shouldSaveConfig = false;
bool primerLoop = true;
bool adsOK = false;
float voltaje, nivel, temp, hum;
unsigned long previousMillis = 0;
const unsigned long interval = 119364; // aprox 120000  =2 minutos ajustado experimentalmente

// Variables para control de cambios
float last_voltaje = -99.0;
float last_temp = -99.0;
float last_hum = -99.0;

// Define aquí cuánto debe cambiar el valor para considerarse "nuevo"
const float UMBRAL_VOLTAJE = 0.02; // Cambio mínimo de 20mV
const float UMBRAL_TEMP = 0.5;    // Cambio mínimo de 0.5 grados
const float UMBRAL_HUM = 1.0;     // Cambio mínimo de 1% 

const char* URL_VERSION = "https://raw.githubusercontent.com/luisfdezrm-stack/arduino/main/version_actual";
const char* URL_BINARIO = "https://raw.githubusercontent.com/luisfdezrm-stack/arduino/main/googlesheetV6-ESP32S2mini.ino.bin";


// ==========================================
//                SET-UP
// ==========================================
void setup() {
  configurarSerial();
  configurarSistemaArchivos();
  configurarWiFiDesdeFS();
  if (wifiMulti.run() != WL_CONNECTED) {gestionarConexionWifi(); }
  configurarOTA();
  inicializarHardware();
//  LittleFS.remove("/config_url.txt"); //limpia la URL que está guardada en la memoria
  checkParaActualizar();
  Serial.println("\n>>> Sistema Inicializado Correctamente (SETUP)");
 }


// ==========================================
//                LOOP
// ==========================================
void loop() {
  if (primerLoop = true) {Serial.println("\n>>> Inicio del loop (versión 6.3)"); primerLoop = false;}
  ArduinoOTA.handle();      // Mantiene activa la actualización inalámbrica
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ejecutarCicloLectura();
if (haCambiadoElDato()) {
      gestionarEnvioDatos();
      // Guardamos el estado actual como último enviado
      last_voltaje = voltaje;
      last_temp = temp;
      last_hum = hum;
    } else {Serial.println(">>> Datos estables. No se requiere envío."); }
    checkParaActualizar();
  }
}


// ==========================================
//               FUNCIONES
// ==========================================
bool haCambiadoElDato() {
  bool cambioV = abs(voltaje - last_voltaje) >= UMBRAL_VOLTAJE;
  bool cambioT = abs(temp - last_temp) >= UMBRAL_TEMP;
  bool cambioH = abs(hum - last_hum) >= UMBRAL_HUM;
  return (cambioV || cambioT || cambioH);}

void saveConfigCallback() {shouldSaveConfig = true;} // Callback de WiFiManager para avisar que debe guardar datos

void configurarSerial() {Serial.begin(115200); delay(100); }

void checkParaActualizar() {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  http.setUserAgent("ESP32-S2-Mini");
  Serial.println("Comprobando actualizaciones en GitHub...");
  http.begin(client, URL_VERSION); 
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    float version_remota = payload.toFloat();
    // 2. Comparar: ¿Es la versión de la web mayor que la mía?
    if (version_remota > VERSION_ACTUAL) {
      Serial.printf("Nueva versión detectada: %.1f. Actualizando...\n", version_remota);
     // Configuramos el timeout para descargas pesadas
      httpUpdate.setLedPin(ledPin, LOW); // Opcional: parpadea el LED durante la descarga
        // 3. Ejecutar la descarga del binario solo si la versión es superior
      t_httpUpdate_return ret = httpUpdate.update(client, URL_BINARIO);
    switch (ret) {
    case HTTP_UPDATE_FAILED: Serial.printf("Error: %s\n", httpUpdate.getLastErrorString().c_str()); break;
    case HTTP_UPDATE_NO_UPDATES: Serial.println("No hay actualizaciones."); break;
    case HTTP_UPDATE_OK: Serial.println("Actualización terminada! Nueva versión zzzzzzzzzzzzzzzzzzzzzzzz descargado desde github zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"); break;
  }
    } else {Serial.println("El firmware está al día."); }
  }
  else { Serial.printf("Error al conectar con GitHub para verificar versión. Código HTTP: %d\n", httpCode);  }
  http.end();  
}

void configurarSistemaArchivos() {
  if (!LittleFS.begin(false)) {    // Intentar montar sin formatear
    Serial.println("Error montando LittleFS. Intentando formatear...");
    if (!LittleFS.begin(true)) {     // Intentar formatear solo si falla
      Serial.println("Error crítico: no se pudo montar ni formatear LittleFS.");
      return;
    }
    Serial.println("LittleFS formateado correctamente.");
  }
  if (LittleFS.exists("/config_url.txt")) {    // Leer archivo si existe
    File f = LittleFS.open("/config_url.txt", "r");
    if (f) {
      String storedURL = f.readString();
      storedURL.trim();  // elimina saltos de línea y espacios
      strncpy(scriptURL, storedURL.c_str(), sizeof(scriptURL) - 1); //Copia segura al buffer
      scriptURL[sizeof(scriptURL) - 1] = '\0';
      Serial.println("URL cargada desde LittleFS:");
      Serial.println(scriptURL);
      f.close();
    } else {Serial.println("Error abriendo /config_url.txt"); }
  } else {Serial.println("No existe /config_url.txt, usando URL por defecto."); }
}

void gestionarConexionWifi() {
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);
  // Añadimos campo para la URL de Google Script
  WiFiManagerParameter custom_script_url("script", "Google Script URL", scriptURL, 150);
  wm.addParameter(&custom_script_url);
  wm.setConfigPortalTimeout(180); // 3 minutos intentando portal (opcional)
  // Intentar conectar (Si falla crea AP "ESP32_Sensor_Config")
  if (!wm.autoConnect("ESP32_Sensor_Config")) {
    Serial.println("No hay credenciales guardadas o fallo. Abrimos portal como Punto de acceso, AP: ESP32_Sensor_Config......");
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }
 if (WiFi.status() != WL_CONNECTED) { Serial.println("\nFallback fallido. Abriendo portal..."); wm.startConfigPortal("ESP32_Sensor_Config"); }
  }
  if (shouldSaveConfig) {    // Guardar la URL si se cambió en el portal
strncpy(scriptURL, custom_script_url.getValue(), sizeof(scriptURL) - 1);
scriptURL[sizeof(scriptURL) - 1] = '\0';
    File f = LittleFS.open("/config_url.txt", "w");
    if (f) {
      f.print(scriptURL);
      f.close();
      Serial.println("Nueva URL guardada en LittleFS.");
      shouldSaveConfig = false; 
    }
  }
 Serial.print("IP: "); Serial.println(WiFi.localIP());
}

void configurarWiFiDesdeFS() {
  if (LittleFS.exists("/wifi_creds.txt")) {
    File f = LittleFS.open("/wifi_creds.txt", "r");
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      int sep = line.indexOf(';');
      if (sep != -1) {
        String ssid = line.substring(0, sep);
        String pass = line.substring(sep + 1);
        wifiMulti.addAP(ssid.c_str(), pass.c_str());
        Serial.printf("Añadida red: %s\n", ssid.c_str());
      }
    }
    f.close();
  }
  Serial.println("Conectando WiFi...");
  if (wifiMulti.run() == WL_CONNECTED) {Serial.print("Conectado a: "); Serial.println(WiFi.SSID()); }
}

void configurarOTA() { ArduinoOTA.setHostname("esp32-sensor-nivel"); ArduinoOTA.begin();}
 
void inicializarHardware() {
  Wire.begin(SDA_PIN, SCL_PIN); // especifico para S2 mini
  pinMode(ledPin, OUTPUT);
  dht.begin();
  Serial.println("Iniciando ADS1115 en pines 8(SDA) y 9(SCL)...");
  adsOK = ads.begin();
  if (!adsOK) { Serial.println("ADS1115 no detectado. Continuando sin él."); }
// Ajuste de ganancia para señales de hasta 4.096V (para medir hasta 6.144V, se usaría GAIN_TWOTHIRDS
  ads.setGain(GAIN_ONE); 
  ads.setDataRate(RATE_ADS1115_8SPS);   // AJUSTE A 8 lecturas por segundo, el ADC es más lento pero menos ruido
  Serial.println("ADS1115 configurado a 8 SPS.");
}

void ejecutarCicloLectura() { blinkLED(); leerADS1115(); leerDHT11(); }

void leerADS1115() {
 if (!adsOK) return;
 long suma = 0;
 for(int i = 0; i < 10; i++) {
 suma += ads.readADC_SingleEnded(0);
 delay(10); }
 int sensorValue = suma / 10;
 float voltaje0 = sensorValue * 0.1875 / 1000.0;
 voltaje = voltaje0;
// El factor depende de la ganancia elegida (para TWOTHIRDS es 0.1875mV por bit)
// Factor ajustado manualmente tras calibración real (no coincide con GAIN_ONE teórico)
// voltaje0 = adc0 * 0.1875 / 1000.0;
// nivel = NIVEL_MAX * voltaje / V_MAX;  // 1V = 1.5m, 3V = 4.5m
  nivel = (1.524 * voltaje) - 0.283;
  Serial.printf("Lectura ADS: %d | Voltaje: %.2fV | Nivel: %.2f m\n", sensorValue, voltaje, nivel);
}

void gestionarEnvioDatos() {
  if (WiFi.status() == WL_CONNECTED) { enviarAGoogleSheets(); } 
    else { Serial.println("WiFi desconectado. Reintentando..."); WiFi.reconnect(); } }

void enviarAGoogleSheets() {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, scriptURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String postData = "voltage=" + String(voltaje, 2) +
                    "&nivel=" + String(nivel, 2) +
                    "&temperatura=" + String(temp, 0) +
                    "&humedad=" + String(hum, 0);
  int httpCode = http.POST(postData);
  if (httpCode > 0) {
    Serial.println(">>> Datos enviados con éxito.");
  } else {
    Serial.printf("Error de envío HTTP: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void leerDHT11() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) { // Comprobar si la lectura falló
    Serial.println("Error al leer del sensor DHT!");
    temp = 99; hum = 99;
    return;
  }
  temp = t; hum = h;
  Serial.printf("Temp: %.1f C | Humedad: %.1f %%\n", temp, hum);
}

void blinkLED() {digitalWrite(ledPin, HIGH); delay(100); digitalWrite(ledPin, LOW); }
