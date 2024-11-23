#include <WiFi.h>
#include <PubSubClient.h>

// Configurações de WiFi
const char* ssid = "wifi_ssid";
const char* password = "password1234";

// Configurações do servidor MQTT
const char *mqtt_broker = "test.mosquitto.org";
const char *topic = "semaforo_input";
const char *output_topic = "semaforo_output";
const int mqtt_port = 1883;

// Instâncias do WiFi e MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Definição dos pinos
const int led1Verde1 = 5;
const int led1Verde2 = 25;
const int ledAmarelo1 = 4;
const int ledAmarelo2 = 27;
const int led1Vermelho1 = 15;
const int led1Vermelho2 = 12;
const int ldr = 34;

int currentLights[] = {0, 0, 0, 0, 0, 0}; // Guarda luzes acesas atualmente

// Variáveis para timers
unsigned long currentMillis = 0;
unsigned long prevMillis = 0; // guarda última troca das luzes do semáforo
unsigned long retryMillis = 0; // guarda última tentativa de conexão com WiFi ou MQTT
unsigned long stallMillis = 0; // guarda o momento que a luz passou a reduzir (stall = permanecer parado, esperar)

const int intervalRetry = 10000; // tempo de intervalo entre tentativas de reconexão 
const int intervalStall = 3000; // tempo mínimo para iniciar modo noturno
const int intervalStallMin = 50; // tempo mínimo para ausência de luz ser considerada
const int minimumLight = 2500; // luz mínima para modo diurno

int intervalGreen = 6000; // intervalo da luz verde até a amarela
int intervalYellow = 2000; // intervalo entre luz amarela até a vermelha
int intervalNight = 500; // intervalo para piscada da luz amarela no modo noturno

// Variáveis de estado
bool ldrOverrule = 0; // ignorar sensor fotoresistor
bool isNight = 0; // modo diurno ou noturno
bool isStalling = 0; // se está detectando ausência de luz mas ainda não está no modo noturno


// Função para conexão/reconexão do WiFi
void connectWiFi() {
    Serial.printf("Connecting to Wi-Fi: %s\n", ssid);
    WiFi.begin(ssid, password);
}

// Função para conexão/reconexão do MQTT
void connectMQTT(PubSubClient &client, const char* topic) {
    if (!client.connected()) {
        String client_id = "semaforo-client-";
        client_id += String(WiFi.macAddress()); // Cria id único usando endereço MAC
        Serial.printf("Attempting to connect as %s to the MQTT broker...\n", client_id.c_str());
        if (client.connect(client_id.c_str())) {
            Serial.println("Connected to MQTT broker.");
            client.subscribe(topic);
            Serial.println("Successfully subscribed to the topic.");
        } else {
            Serial.println("Failed to connect to MQTT broker.");
        }
    }
}

// Callback para quando uma mensagem é recebida
void callback(char* topic, byte* message, unsigned int length) {
    Serial.print("Mensagem recebida no tópico: ");
    Serial.println(topic);

    // Converte a mensagem para String
    String messageTemp;
    for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
    }
    Serial.println("Mensagem: " + messageTemp);

    // Verifica se a mensagem é o comando esperado
    if (messageTemp == "day") {
        Serial.println("Modo diurno ativado.");
        client.publish(output_topic, "Modo diurno ativado.");
        ldrOverrule = 1;
        isNight = 0;
    } else if (messageTemp == "night") {
        Serial.println("Modo noturno ativado.");
        client.publish(output_topic, "Modo noturno ativado.");
        ldrOverrule = 1;
        isNight = 1;
    } else if (messageTemp == "party") {
        Serial.println("modo festinha LIGADO!!!");
        client.publish(output_topic, "modo festinha LIGADO!!!");
        intervalGreen = 100;
        intervalYellow = 100;
        ldrOverrule = 1;
        isNight = 0;
    } else if (messageTemp == "reset") {
        Serial.println("Atributos resetados.");
        client.publish(output_topic, "Atributos resetados.");
        intervalGreen = 6000;
        intervalYellow = 2000;
        intervalNight = 500;
        ldrOverrule = 0;
        isNight = 0;
    }
}

// Função para trocar estado de todos os LEDs facilmente
// 0 = apagado, 1 = aceso
void updatelights(bool r1, bool y1, bool g1, bool r2, bool y2, bool g2) {
  //Semáforo 1
  digitalWrite(led1Vermelho1, r1);
  digitalWrite(ledAmarelo1, y1);
  digitalWrite(led1Verde1, g1);
  // Semáforo 2
  digitalWrite(led1Vermelho2, r2);
  digitalWrite(ledAmarelo2, y2);
  digitalWrite(led1Verde2, g2);

// Salva estado atual de cada LED e o tempo da mudança
  currentLights[0] = r1;
  currentLights[1] = y1;
  currentLights[2] = g1;
  currentLights[3] = r2;
  currentLights[4] = y2;
  currentLights[5] = g2;
  prevMillis = currentMillis;
}

// modo diurno
void day() {
  // luz vermelha semaforo 1
  if (currentLights[0]) {
    // se luz amarela semaforo 2
    if (currentLights[4]) {
      if (currentMillis - prevMillis >= intervalYellow) {
        updatelights(0, 0, 1, 1, 0, 0);
      };
    } // senao luz amarela semaforo 2
    else {
      if (currentMillis - prevMillis >= intervalGreen) {
        updatelights(1, 0, 0, 0, 1, 0);
      };
    }
    // luz vermelha semaforo 2
  } else if (currentLights[3]) {
    // se luz amarela semaforo 1
    if (currentLights[1]) {
      if (currentMillis - prevMillis >= intervalYellow) {
        updatelights(1, 0, 0, 0, 0, 1);
      };
    } // senao luz amarela semaforo 1
    else {
      if (currentMillis - prevMillis >= intervalGreen) {
        updatelights(0, 1, 0, 1, 0, 0);
      };
    }
  } else {
    updatelights(0, 0, 1, 1, 0, 0);
  }
}

// modo noturno
void night() {
  // Desliga todos os LEDs exceto os amarelos
  if (currentMillis - prevMillis >= intervalNight) {
    if (currentLights[1] && currentLights[4]){
      updatelights(0, 0, 0, 0, 0, 0);
    } else {
      updatelights(0, 1, 0, 0, 1, 0);
    };
  };
}

// checagem do ldr (se estiver ativado)
void ldrCheck() {
  if (analogRead(ldr) < minimumLight){ // se luz atual menor do que luz mínima
    if(!isNight){
      if (!isStalling){
        stallMillis = currentMillis;
        isStalling = 1; // inicia espera para trocar para modo noturno
      } else {
        if (currentMillis - stallMillis >= intervalStall){
          isNight = 1;
          isStalling = 0; // finaliza espera, ativa modo noturno
        }
      }
    }
  } else { // se luz atual maior do que luz mínima
    if (!isStalling){
      if (isNight){
        isNight = 0; // desativa modo noturno
      }
    } else {
      isStalling = 0; // finaliza espera antecipadamente se luz for detectada
      if (currentMillis - stallMillis >= intervalStallMin){ // se estava em espera e saiu depois do tempo mínimo
        Serial.println("Vruuuum!");
        if (client.connected()){
          client.publish(output_topic, "Carro detectado!"); // se conectado, publica momento que carro foi detectado
        }
      }
    }
  }
}

void setup() {
  // Configura os pinos como saída
  pinMode(led1Verde1, OUTPUT);
  pinMode(led1Verde2, OUTPUT);
  pinMode(ledAmarelo1, OUTPUT);
  pinMode(ledAmarelo2, OUTPUT);
  pinMode(led1Vermelho1, OUTPUT);
  pinMode(led1Vermelho2, OUTPUT);
  pinMode(ldr, INPUT);
  analogSetAttenuation(ADC_11db); // uma vez que o fotorresistor está usando tensão maior (pino 3.3v), a atenuação é aumentada

  Serial.begin(115200);

  connectWiFi();

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  currentMillis = millis(); // marca momento atual para timers

  // roda checagem de conexão depois do intervalo especificado
  if (currentMillis - retryMillis >= intervalRetry) {
    if (WiFi.status() != WL_CONNECTED) {
        retryMillis = currentMillis;
        Serial.println("Wi-Fi lost. Reconnecting...");
        connectWiFi();
    } else if (!client.connected()) {
        retryMillis = currentMillis;
        connectMQTT(client, topic);
    };
  };

  if (client.connected()){
  client.loop(); // Mantém a conexão MQTT
  };

  if (!ldrOverrule){
    ldrCheck();
  }
  
  if (isNight) {
    night();
  } else {
    day();
  }
  delay(50); // delay entre cada loop, evitando sobrecarregar o dispositivo
}