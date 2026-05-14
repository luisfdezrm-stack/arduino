/*
Cambios en la versión 6 VERSIÓM PARA ESP32 S2 mini
 se incluye:
analiza si cada dato es distinto del anterior, y sólo envía el dato si el cambio es significativo.	

*/


#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <LittleFS.h>  //para almacenar vbles en ESP32 memoria no volátil

Adafruit_ADS1115 ads; // Creamos la instancia del objeto ADS1115

#define DHTPIN 5 // en el ESP32 es 4
#define DHTTYPE DHT11
#define SDA_PIN 8 // para el ESP32 S2 mini . (probar 8/9 o 33/34)
#define SCL_PIN 9 // para el ESP32 S2 mini
DHT dht(DHTPIN, DHTTYPE);

//const float NIVEL_MAX = 15.0;    // Nivel máximo del sensor en metros
//const float I_MIN = 4.0;         // Corriente mínima sensor (0 metros)
//const float I_MAX = 20.0;        // Corriente máxima sensor (15 metros)
//const float V_MAX = 10.0;// tensión máxima del regulador 4-20 a 0-10V (jumper 2 quitado. Con los dos jumper puestos, sería 0-3.3V)
const int ledPin = 14; // en el ESP32, era el 2
const char* DEFAULT_SSID = "ctlnsrz16vdfn";
const char* DEFAULT_PASS = "ctlnsrz16vdfne";
char scriptURL[150] = "https://script.google.com/macros/s/AKfycbybdpPHvBwuFokiMDu765gOQx3REcbAIuOuv6_IyFGhVeGuOyxrXeCctPgZBUXIfOduEA/exec";
bool shouldSaveConfig = false;
bool adsOK = false;
float voltaje, nivel, temp, hum;
unsigned long previousMillis = 0;
const unsigned long interval = 111364; // aprox 120000  =2 minutos ajustado experimentalmente

// Variables para control de cambios
float last_voltaje = -99.0;
float last_temp = -99.0;
float last_hum = -99.0;

// Define aquí cuánto debe cambiar el valor para considerarse "nuevo"
const float UMBRAL_VOLTAJE = 0.02; // Cambio mínimo de 20mV
const float UMBRAL_TEMP = 0.5;    // Cambio mínimo de 0.5 grados
const float UMBRAL_HUM = 1.0;     // Cambio mínimo de 1% 


// ==========================================
//                SET-UP
// ==========================================
void setup() {
  configurarSerial();
  configurarSistemaArchivos();
  gestionarConexionWifi();
  configurarOTA();
  inicializarHardware();
  LittleFS.remove("/config_url.txt"); //limpia la URL que está guardada en la memoria
  Serial.println("\n>>> Sistema Inicializado Correctamente (SETUP)");
}



// ==========================================
//                LOOP
// ==========================================
void loop() {
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
    } else {
      Serial.println(">>> Datos estables. No se requiere envío.");
    }
  }
}


// ==========================================
//               FUNCIONES
// ==========================================


bool haCambiadoElDato() {
  bool cambioV = abs(voltaje - last_voltaje) >= UMBRAL_VOLTAJE;
  bool cambioT = abs(temp - last_temp) >= UMBRAL_TEMP;
  bool cambioH = abs(hum - last_hum) >= UMBRAL_HUM;
  return (cambioV || cambioT || cambioH);
}


void saveConfigCallback() {shouldSaveConfig = true;} // Callback de WiFiManager para avisar que debe guardar datos


void configurarSerial() {Serial.begin(115200); delay(100); }


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
    } else {
      Serial.println("Error abriendo /config_url.txt");
    }
  } else {
    Serial.println("No existe /config_url.txt, usando URL por defecto.");
  }
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
    Serial.println("No hay credenciales guardadas o fallo. Probando WiFi por defecto......");
    WiFi.begin(DEFAULT_SSID, DEFAULT_PASS);
     int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }

 if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\nFallback fallido. Abriendo portal...");
      wm.startConfigPortal("ESP32_Sensor_Config");
    }

  }
  // Guardar la URL si se cambió en el portal
  if (shouldSaveConfig) {
 //   strcpy(scriptURL, custom_script_url.getValue()); Sustituyo esta propuesta por Gemini, por la siguiente de ChatGPT
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


void configurarOTA() {
 ArduinoOTA.setHostname("esp32-sensor-nivel");
 // Aquí se podría añadir ArduinoOTA.setPassword("tu_clave");
 ArduinoOTA.begin();
}


void inicializarHardware() {
  Wire.begin(SDA_PIN, SCL_PIN); // especifico para S2 mini
  pinMode(ledPin, OUTPUT);
  dht.begin();
  Serial.println("Iniciando ADS1115...");
  adsOK = ads.begin();
  if (!adsOK) {
  Serial.println("ADS1115 no detectado. Continuando sin él.");
//    ESP.restart();  //eliminado para evitar bloqueos, y permitir actualización OTA
  }
// Ajuste de ganancia para señales de hasta 4.096V (para medir hasta 6.144V, se usaría GAIN_TWOTHIRDS
  ads.setGain(GAIN_ONE); 
  ads.setDataRate(RATE_ADS1115_8SPS);   // AJUSTE A 8 lecturas por segundo, el ADC es más lento pero menos ruido
  Serial.println("ADS1115 configurado a 8 SPS.");
}


void ejecutarCicloLectura() {
  blinkLED();
  leerADS1115();
  leerDHT11();
}


void leerADS1115() {
 if (!adsOK) return;
 long suma = 0;
 for(int i = 0; i < 50; i++) {
 suma += ads.readADC_SingleEnded(0);
 delay(100); //evita leer constantemente el mismo valor
 }
 int sensorValue = suma / 50;
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
  if (WiFi.status() == WL_CONNECTED) {
    enviarAGoogleSheets();
  } else {
    Serial.println("WiFi desconectado. Reintentando...");
     WiFi.reconnect();  // anteriormente WiFi.begin(); 
  }
}

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


void blinkLED() {
  digitalWrite(ledPin, HIGH);
  delay(100);
  digitalWrite(ledPin, LOW);
}

