// Dichiarazione librerie
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "SparkFun_SGP30_Arduino_Library.h" 

// Connessione alla rete WiFi
const char *ssid = "IoT_EXT";
const char *password = "CasaIoT2024|";

// Connessione alla pagina web Altervista
const char *serverName = "http://managmentalelmn.altervista.org/inviodati.php";
String apiKeyValue = "tPmAT5Ab3j7F9";

// Connessione alla pagina dati ThingSpeak
const char *apiKey = "X6GREUFAZIYLU48U";

// Connessione al bot Telegram
#define BOTtoken "6861211184:AAEcoKMC2LEoNJ3VQnJs8MdXjbY00dbcxi4"
#define CHAT_ID "5006934134"
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// Pin collegati al sensore BMP180
const int bmp180SDA = 21;  // SDA
const int bmp180SCL = 22;  // SCL
// Gestione sensore pressione
Adafruit_BMP085 bmp;

// Pin collegato al sensore DHT22
#define DHTPIN 4
// Gestione sensore temperatura e umidità
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Pin collegato al sensore del terreno
#define AOUT_PIN 34

// Gestione sensore SGP30
SGP30 mySensor; 

//sensore voltaggio
#define AOUT_VOLT 35

// Dichiarazione variabili a livello globale
float temperature, humidity, pressure;

// Setup
void setup() {
  Serial.begin(115200);

  // Connessione al WiFi
  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print("NON CONNESSO ");
  }
  Serial.println("");
  Serial.println("CONNESSO AL WIFI");

  // Inizializza il sensore BMP180
  bmp.begin();

  // Inizializza il sensore DHT22
  dht.begin();

  //Initializes sensor for air quality readings
  mySensor.begin();
  
  //measureAirQuality should be called in one second increments after a call to initAirQuality
  mySensor.initAirQuality();
}

void loop() {
  delay (15000);
  mySensor.measureAirQuality();
  
  // Lettura dati dal sensore DHT22
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Lettura dati dal sensore BMP180
  pressure = bmp.readPressure() / 100.0F;
  float temperaturebmp = bmp.readTemperature();

  // Lettura dati dal sensore SGP30
  float quantco2 = mySensor.CO2;

  // Lettura sensore terreno
  int value = analogRead(AOUT_PIN);

  // Stampa i dati dei sensori nel monitor seriale
  Serial.println("-------------------");
  Serial.println("");
  Serial.print("Temperatura: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Umidita: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Pressione: ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("TemperaturaBMP: ");
  Serial.print(temperaturebmp);
  Serial.println(" °C");

  Serial.print("Umidita terreno: ");
  Serial.print(value);
  Serial.print("");
 
  int moisturePercentage = map(value, 2600, 1100, 0, 100);
  //-------------------------------------------------------
  //conversione in string per invio come dato al bot
  String SmoisturePercentage = String (moisturePercentage);
  //invio alert al bot telegram
  if (moisturePercentage < 25){
    bot.sendMessage(CHAT_ID, "Terreno secco!");
    bot.sendMessage(CHAT_ID, SmoisturePercentage);
    }
  //-------------------------------------------------------
  Serial.println("");
  Serial.print(moisturePercentage);
  Serial.println(" %");
  Serial.println("");
  Serial.print("CO2: ");
  Serial.print(quantco2);
  Serial.print(" ppm\tTVOC: ");
  Serial.print(mySensor.TVOC);
  Serial.println(" ppb");
  Serial.println("-------------------");
  Serial.println("");

  // Invio dati a ThingSpeak
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;

    // Prepara l'URL API
    String url = "/update?api_key=";
    url += apiKey;
    url += "&field1=";
    url += String(temperature);
    url += "&field2=";
    url += String(humidity);
    url += "&field3=";
    url += String(pressure);
    url += "&field4=";
    url += String(temperaturebmp);
    url += "&field5=";
    url += String(moisturePercentage);

    // Effettua la richiesta HTTP
    if (client.connect("api.thingspeak.com", 80)) {
      Serial.println("Connected to ThingSpeak");

      client.print("GET " + url + " HTTP/1.1\r\n" +
                   "Host: api.thingspeak.com\r\n" +
                   "Connection: close\r\n\r\n");
      delay(500);

      // Leggi e stampa la risposta
      while (client.available()) {
        String line = client.readStringUntil('\r');
        Serial.print(line);
      }

      Serial.println();
      Serial.println("Dati inviati a ThingSpeak");
      Serial.println("");

      client.stop();
    } else {
      Serial.println("Connessione a ThingSpeak fallita");
      Serial.println("");
      bot.sendMessage(CHAT_ID, "Connessione a ThingSpeak fallita");
    }
  }

  // Invio al database Altervista
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    // Inizializza il nome del dominio dove si trova la pagina web
    http.begin(client, serverName);

    // Tipo del contenuto
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Invio dei dati con metodo POST
    String httpRequestData = "api_key=" + apiKeyValue + "&temperatura=" + String(temperature) +
                             "&umidita=" + String(humidity) + "&pressione=" + String(pressure) + "&umiditaterreno=" + String(moisturePercentage) + "&CO2=" + String(quantco2) + "";
    Serial.print("httpRequestData: ");
    Serial.println(httpRequestData);

    // Invia il POST HTTP per ricevere eventuali errori
    int httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0) {
      Serial.print("HTTP codice di risposta: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Codice errore: ");
      Serial.println(httpResponseCode);
      bot.sendMessage(CHAT_ID, "Connessione al database fallita.");
    }

    // Libera le risorse
    http.end();
  } else {
    Serial.println("WiFi disconnesso");
  }

  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("messaggio ricevuto");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  // Attesa di 120 secondi prima di inviare nuovamente i dati
  setDeepSleep();
}

void setDeepSleep() {
  Serial.println("Impostazione della modalità deep sleep...");

  // Configura il tempo di deep sleep in microsecondi
  // Ad esempio, 1 minuto di sleep
  esp_sleep_enable_timer_wakeup(180 * 1000000);

  // Alcune configurazioni aggiuntive se necessario
  // ...

  Serial.println("Entrata in modalità deep sleep...");
  delay(100); // Attendi per assicurarti che la stampa venga completata

  // Esegui la transizione in modalità deep sleep
  esp_deep_sleep_start();
}

//dichiarazione string per conversioni variabili (TemperatureSend, ...)
String ts, hs, ps, vs;

// Stampiamo sul terminale della seriale indicazioni sul messaggio ricevuto
void handleNewMessages(int numNewMessages) {
  Serial.println("Gestione del messaggio");
  Serial.println(String(numNewMessages));
  for (int i = 0; i < numNewMessages; i++) {
    // Verifichiamo che la chat ID sia corretta
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Utente non autorizzato", "");
      continue;
    }

    String text = bot.messages[i].text;
    Serial.println(text);

    if (text == "/weather") {
      ts = String(temperature);
      bot.sendMessage(chat_id, "temperatura:");
      bot.sendMessage(chat_id, ts, "");
      hs = String(humidity);
      bot.sendMessage(chat_id, "umidita:");
      bot.sendMessage(chat_id, hs, "");
      ps = String(pressure);
      bot.sendMessage(chat_id, "pressione:");
      bot.sendMessage(chat_id, ps, "");
    }

     if (text == "/voltage") {
      float volt = analogRead(AOUT_VOLT) * 0.00413946;// read the input
      vs = String(volt);
      bot.sendMessage(chat_id, "volt batteria:");
      bot.sendMessage(chat_id, vs, "");
    }
  }
}
