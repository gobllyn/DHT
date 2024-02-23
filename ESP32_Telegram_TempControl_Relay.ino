#include <Wire.h> 
//#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h> // Include the EEPROM library

#define DHTPIN 23
#define RELAY_PIN 18 // Exemplo de pino para o relé

#define DHTTYPE DHT11 // DHT 11

#define WIFI_SSID "YOUr_SSID"
#define WIFI_PASSWORD "YOURPASSWORD"

#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

DHT dht(DHTPIN, DHTTYPE);

const unsigned long BOT_MTBS = 1000; // tempo médio entre mensagens do bot
unsigned long bot_lasttime = 0;       // última vez que as mensagens foram verificadas
unsigned long lastRelayChangeTime = 0; // momento da última mudança de estado do relé
float temperatureC;
float humidity;

float TEMP_THRESHOLD_HIGH; // Limite de temperatura alta em Celsius
float TEMP_THRESHOLD_LOW;  // Limite de temperatura baixa em Celsius

bool shouldControlRelay = true; // Variável de controle para determinar se deve controlar o relé

// LCD configuration
//LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD I2C address and dimensions

void setup()
{
  Serial.begin(9600);
  Serial.println(F("Teste do DHT11!"));

  pinMode(RELAY_PIN, OUTPUT); // Define o pino do relé como saída
  digitalWrite(RELAY_PIN, HIGH); // Turn off the relay
  dht.begin();

  Serial.print("Conectando à rede WiFi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nConectado à WiFi. Endereço IP: ");
  Serial.println(WiFi.localIP());

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // Load temperature thresholds from EEPROM
  EEPROM.begin(512);
  EEPROM.get(0, TEMP_THRESHOLD_HIGH);
  EEPROM.get(sizeof(float), TEMP_THRESHOLD_LOW);
  EEPROM.end();

  // Verifica se os dados do sensor DHT11 são válidos após várias tentativas
  int attempts = 0;
  while (attempts < 3) // Tente até 3 vezes
  {
    humidity = dht.readHumidity();
    temperatureC = dht.readTemperature();

    // Verifica se os dados do sensor são válidos
    if (!isnan(temperatureC) && !isnan(humidity))
    {
      // Dados válidos obtidos, saia do loop
      break;
    }

    // Mensagem de erro e espera antes de tentar novamente
    Serial.println("Failed to read from DHT sensor! Retrying...");
    delay(2000); // Aguarde 2 segundos antes de tentar novamente
    attempts++;
  }

  // Se os dados do sensor ainda forem inválidos após as tentativas
  if (attempts >= 3)
  {
    // Envie uma mensagem de alerta para o Telegram
    sendAlertMessage("Erro no Sensor DHT11", "Falha ao ler dados do sensor DHT11 após múltiplas tentativas.");
  }
}

void loop()
{
  // Exibe as informações no monitor serial
  Serial.print("Temperatura: ");
  Serial.print(temperatureC);
  Serial.print("°C, Humidade: ");
  Serial.print(humidity);
  Serial.print("%, Estado da Valvula de Rega: ");
  Serial.print(digitalRead(RELAY_PIN) == LOW ? "Ligado" : "Desligado");
  Serial.print(", Controlo de temperatura: ");
  Serial.println(shouldControlRelay ? "Ligado" : "Desligado");

  // Verifica se é hora de enviar o comando /start
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages)
    {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
}

void handleNewMessages(int numNewMessages)
{
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID)
    {
      bot.sendMessage(chat_id, "Utilizador não autorizado", "");
    }
    else
    {
      String text = bot.messages[i].text;

      if (text == "/viveiro")
      {
        String welcome = "Viveiro CARES Dashboard Rega.\n";
        welcome += "\nTemperatura: " + String(temperatureC) + "°C\n";
        welcome += "Humidade: " + String(humidity) + "%\n";
        welcome += "Limite Superior de Temperatura: " + String(TEMP_THRESHOLD_HIGH) + "°C\n";
        welcome += "Limite Inferior de Temperatura: " + String(TEMP_THRESHOLD_LOW) + "°C\n";
        welcome += "Estado da valvula de rega: " + String(digitalRead(RELAY_PIN) == LOW ? "Ligado" : "Desligado") + "\n";
        welcome += "Controlo de temperatura: " + String(shouldControlRelay ? "Ligado" : "Desligado") + "\n";
        welcome += "\n\nComandos: ";
        welcome += "\n/viveiro : Menu Geral \n";
        //welcome += "/temp : Temperatura \n";
      //  welcome += "/hum : Humidade\n";
        welcome += "/temphum : Temperatura e Humidade\n";
        welcome += "/regaon : Rega on \n";
        welcome += "/regaoff : Rega off\n";
        welcome += "/controltempon : Control de Temperatura Ligado \n";
        welcome += "/controltempoff : Controle de Temperatura Desligado\n";
        welcome += "/sethighlimit [+ o valor]: Define o limite superior de temperatura\n";
        welcome += "/setlowlimit [+ o valor]: Define o limite inferior de temperatura\n";
        bot.sendMessage(chat_id, welcome, "Markdown");
      }

      if (text == "/temp")
      {
        String msg = "A temperatura é " + String(temperatureC) + "°C";
        bot.sendMessage(chat_id, msg, "");
      }
      if (text == "/hum")
      {
        String msg = "A humidade é " + String(humidity) + "%";
        bot.sendMessage(chat_id, msg, "");
      }
      if (text == "/temphum")
      {
        String msg = "A temperatura é " + String(temperatureC) + "°C\n";
        msg += "A humidade é " + String(humidity) + "%";
        bot.sendMessage(chat_id, msg, "");
      }
//     
      // Adiciona comandos para controlar o relé
      if (text == "/regaon")
      {
        shouldControlRelay = false; // Desativa o controle do relé com base na temperatura
        digitalWrite(RELAY_PIN, LOW); // Liga o relé
        bot.sendMessage(chat_id, "Rega LIGADA", "");
      }
      if (text == "/regaoff")
      {
        shouldControlRelay = false; // Desativa o controle do relé com base na temperatura
        digitalWrite(RELAY_PIN, HIGH); // Desliga o relé
        bot.sendMessage(chat_id, "Rega Desligada", "");
      }
      // Adiciona comandos para ativar/desativar o controle de temperatura do relé
      if (text == "/controltempon")
      {
        shouldControlRelay = true;
        bot.sendMessage(chat_id, "Controle de temperatura do relé ativado", "");
      }
      if (text == "/controltempoff")
      {
        shouldControlRelay = false;
        bot.sendMessage(chat_id, "Controle de temperatura desativado", "");
      }
      // Adiciona comandos para definir os limites de temperatura
      if (text.startsWith("/sethighlimit"))
      {
        TEMP_THRESHOLD_HIGH = text.substring(14).toFloat();
        saveThresholdsToEEPROM();
        bot.sendMessage(chat_id, "Limite superior de temperatura definido para " + String(TEMP_THRESHOLD_HIGH) + "°C", "");
      }
      if (text.startsWith("/setlowlimit"))
      {
        TEMP_THRESHOLD_LOW = text.substring(13).toFloat();
        saveThresholdsToEEPROM();
        bot.sendMessage(chat_id, "Limite inferior de temperatura definido para " + String(TEMP_THRESHOLD_LOW) + "°C", "");
      }
    }
  }
}

// Função para enviar mensagens de alerta
void sendAlertMessage(String title, String message)
{
  bot.sendMessage(CHAT_ID, title + "\n" + message, "");
}

// Função para salvar os limites de temperatura na EEPROM
void saveThresholdsToEEPROM()
{
  EEPROM.begin(512);
  EEPROM.put(0, TEMP_THRESHOLD_HIGH);
  EEPROM.put(sizeof(float), TEMP_THRESHOLD_LOW);
  EEPROM.commit();
  EEPROM.end();
}
