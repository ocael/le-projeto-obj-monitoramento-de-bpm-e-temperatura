#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ----- CONFIGURAÇÕES DO DISPLAY OLED -----
#define OLED_W 128
#define OLED_H 64
#define OLED_RST -1
Adafruit_SSD1306 tela(OLED_W, OLED_H, &Wire, OLED_RST);

// ----- PINOS DO SISTEMA -----
#define PINO_PULSO 35
#define LED_BAIXO 2
#define LED_OK 4
#define LED_ALTO 5
#define BUZZER 25

// ----- PINOS DO DHT22 -----
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ----- LEDS PARA TEMPERATURA -----
#define LED_TEMP_BAIXA 12
#define LED_TEMP_ALTA 14

// ----- REDE E MQTT -----
const char* wifiSSID = "Wokwi-GUEST";
const char* wifiPASS = "";
const char* brokerMQTT = "test.mosquitto.org";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ----- LIMITES DE FREQUÊNCIA -----
int limiteMin = 50;
int limiteMax = 120;

// ----- LIMITES DE TEMPERATURA AMBIENTAL -----
float tempMin = 16.0;
float tempMax = 30.0;

// =========================================================
//  INICIALIZAÇÃO DO WIFI
// =========================================================
void conectarWiFi() {
  Serial.print("WiFi >> Conectando em: ");
  Serial.println(wifiSSID);

  WiFi.begin(wifiSSID, wifiPASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("*");
    delay(400);
  }
  Serial.println("\nWiFi >> Pronto!");
}

// =========================================================
//  RECONEXÃO AO SERVIDOR MQTT
// =========================================================
void reconectarMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT >> Tentando acesso...");

    if (mqtt.connect("CardioNode32")) {
      Serial.println(" OK");
    } else {
      Serial.print(" Falhou (cod:");
      Serial.print(mqtt.state());
      Serial.println("). Novo teste em 4s");
      delay(4000);
    }
  }
}

// =========================================================
//  ALERTA SONORO
// =========================================================
void beepAlerta() {
  for (int n = 0; n < 2; n++) {
    tone(BUZZER, 1200);
    delay(180);
    noTone(BUZZER);
    delay(200);
  }
}

// =========================================================
//  DESLIGAR TODOS OS LEDS DE BPM
// =========================================================
void resetarLedsBPM() {
  digitalWrite(LED_BAIXO, LOW);
  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_ALTO, LOW);
}

// =========================================================
//  DESLIGAR TODOS OS LEDS DE TEMPERATURA
// =========================================================
void resetarLedsTemp() {
  digitalWrite(LED_TEMP_BAIXA, LOW);
  digitalWrite(LED_TEMP_ALTA, LOW);
}

// =========================================================
//  SETUP PRINCIPAL
// =========================================================
void setup() {
  Serial.begin(115200);

  pinMode(LED_BAIXO, OUTPUT);
  pinMode(LED_OK, OUTPUT);
  pinMode(LED_ALTO, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(LED_TEMP_BAIXA, OUTPUT);
  pinMode(LED_TEMP_ALTA, OUTPUT);

  dht.begin();

  resetarLedsBPM();
  resetarLedsTemp();

  if (!tela.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED >> ERRO FATAL");
    while (1);
  }

  tela.clearDisplay();
  tela.setTextSize(1);
  tela.setTextColor(SSD1306_WHITE);
  tela.setCursor(0, 28);
  tela.println("Sistema Vital");
  tela.display();
  delay(1800);

  conectarWiFi();
  mqtt.setServer(brokerMQTT, 1883);
}

// =========================================================
//  LOOP PRINCIPAL
// =========================================================
void loop() {

  if (!mqtt.connected()) {
    reconectarMQTT();
  }
  mqtt.loop();

  // ----- SENSOR DE BATIMENTOS -----
  int leituraBruta = analogRead(PINO_PULSO);
  int bpm = map(leituraBruta, 300, 600, 40, 180);
  bpm = constrain(bpm, 0, 200);

  // ----- TEMPERATURA -----
  float temp = dht.readTemperature();

  resetarLedsBPM();      // Apenas os LEDs de BPM
  // Não reseta mais os LEDs de temperatura

  String faixaBPM;
  String faixaTemp;

  tela.clearDisplay();
  tela.setCursor(0, 0);

  // =====================================================
  //  CLASSIFICAÇÃO DO BPM
  // =====================================================
  if (bpm <= limiteMin) {
    faixaBPM = "Abaixo";
    digitalWrite(LED_BAIXO, HIGH);
    beepAlerta();
    tela.println("Ritmo: Abaixo");
  } else if (bpm >= limiteMax) {
    faixaBPM = "Alto";
    digitalWrite(LED_ALTO, HIGH);
    beepAlerta();
    tela.println("Ritmo: Alto");
  } else {
    faixaBPM = "Normal";
    digitalWrite(LED_OK, HIGH);
    tela.println("Ritmo: Normal");
  }

  tela.setCursor(0, 12);
  tela.print("BPM: ");
  tela.println(bpm);

  // ----- LINHA SEPARADORA -----
  tela.drawLine(0, 24, OLED_W, 24, SSD1306_WHITE);

  // =====================================================
  //  CLASSIFICAÇÃO DA TEMPERATURA AMBIENTAL
  // =====================================================
  resetarLedsTemp(); // reseta antes de atualizar temperatura

  if (temp <= tempMin) {
    faixaTemp = "Baixa";
    digitalWrite(LED_TEMP_BAIXA, HIGH);
  } else if (temp >= tempMax) {
    faixaTemp = "Alta";
    digitalWrite(LED_TEMP_ALTA, HIGH);
  } else {
    faixaTemp = "Normal";
  }

  tela.setCursor(0, 28);
  tela.print("Temp: ");
  tela.print(temp);
  tela.println("C");

  tela.setCursor(0, 40);
  tela.print("Estado: ");
  tela.println(faixaTemp);

  tela.display();

  // ----- Monitor Serial -----
  Serial.print("BPM: ");
  Serial.print(bpm);
  Serial.print(" | Estado: ");
  Serial.println(faixaBPM);

  Serial.print("Temperatura: ");
  Serial.print(temp);
  Serial.print(" | Estado: ");
  Serial.println(faixaTemp);

  // ----- MQTT -----
  char msgBPM[10];
  sprintf(msgBPM, "%d", bpm);
  mqtt.publish("monitor/cardiaco/valor", msgBPM);
  mqtt.publish("monitor/cardiaco/estado", faixaBPM.c_str());

  char msgTemp[10];
  dtostrf(temp, 4, 1, msgTemp);
  mqtt.publish("monitor/temperatura/valor", msgTemp);
  mqtt.publish("monitor/temperatura/estado", faixaTemp.c_str());

  delay(1800);
}
