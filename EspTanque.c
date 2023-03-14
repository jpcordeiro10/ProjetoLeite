// Firmware da ESP do tanque responsavel por:
// - Leitura da distancia do sensor ultrassonico
// - Calculo do nivel do tanque
// - Leitura da temperatura com a calibracao
// - Uso do ESPNOW para envio
// - Envio da média da temperatura e do nível do tanque [Wireless]

/* --- Inclusao de bibliotecas --- */
// Envio [ESPNOW]
#include <esp_now.h>
#include <WiFi.h>
// Temperatura
#include <OneWire.h>
#include <DallasTemperature.h>
// Sensor de nivel 
#include <Adafruit_VL53L0X.h>
#include <Wire.h>
#include <math.h>

/* ------------------------------- */
/* --- Pinos --- */
// Temperatura
#define oneWireBus 22

// SENSOR DE NIVEL (LASER)
#define I2C_SDA 33 
#define I2C_SCL 32
/* ------------------------------- */

/* --- Constantes --- */
#define NUMERODEMEDIDAS 10
#define PI_APROXIMADO 3.1415927
#define DIAMETRO     10.8 //[cm]   valor utilizado para teste, valor para o modelo de caminhão escolhido ----> 200 [cm] 
#define ALTURA_TANQUE  20.5  //[cm]  valor utilizado para teste, valor para o modelo de caminhão escolhido ----> 250 [cm]
/* ------------------------------- */

/* --- Objetos das bibliotecas --- */
// Temperatura    
    OneWire oneWire(oneWireBus);
    DallasTemperature sensors(&oneWire);
// Sensor a laser
    Adafruit_VL53L0X lox = Adafruit_VL53L0X();  
/* ------------------------------- */

/* --- Variaveis --- */
// Endereço MAC
uint8_t broadcastAddress[] = {0x08, 0x3A,0xF2,0xAB,0xAC,0x70}; // Endereço MAC da ESP que ira receber
// Variaveis para Leitura da Temperatura
float Acumulador = 0, TemperaturaLida, TempCalibrada, MediaTemp, CorrecaoTemp;
// Variavel de recebimento
float TemperaturaCaminhao, TemperaturaTanque;
// Variaveis para calculo do nivel 
float r = DIAMETRO/2, h;
float distancia_laser;
float area_tanque;
float distancia;
float Volume; 
/* --- Struct para envio --- */
typedef struct struct_message {
    float Temperatura;
    float Volume;
} struct_message;
struct_message Readings;
/* --- Struct para recebimento --- */
typedef struct struct_received {
    float Temperatura;
    float Volume;
} struct_received;
struct_received Recebimento;



void InicializaLaser(){ // Funcao para inicialização do sensor a laser
    Serial.println("Adafruit VL53L0X test");
    if (!lox.begin()){ // Teste do sensor a laser aguarda enquanto não ler
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
    }
}
void LeituraLaser(){ // Leitura do sensor a laser
    VL53L0X_RangingMeasurementData_t measure;

    lox.rangingTest(&measure, false);
    
    if (measure.RangeStatus != 4) {
      distancia_laser = measure.RangeMilliMeter;
      Serial.print("Leitura laser(mm): ");
      Serial.println(distancia_laser);
     } else {
      Serial.println("out of range");
  }
}
void CalculoVolumeTanque(){
    distancia = distancia_laser/10;
        
    h = ALTURA_TANQUE - distancia; // altura do líquido
    area_tanque = ((PI_APROXIMADO * (r*r))/2);
    Volume = ((h * area_tanque)/1000); // volume final calculado em litros [L]
    Readings.Volume = Volume;   
    Serial.print("Volume(L): ");
    Serial.println(Volume);
}
void LeituraTemperatura(){ // Faz a leitura e a correcao da temperatura
    sensors.requestTemperatures();
    sensors.getTempCByIndex(0);

    int i;
    for (i = 0; i <= NUMERODEMEDIDAS; i++){ // Pode alterar as medidas no define
        TemperaturaLida = sensors.getTempCByIndex(0);
        Acumulador = TemperaturaLida + Acumulador;
        Serial.print("Temperatura: ");
        Serial.print(TemperaturaLida);
    }
    MediaTemp = Acumulador / NUMERODEMEDIDAS;
    Readings.Temperatura = MediaTemp;
    TemperaturaTanque = MediaTemp;
    Serial.print("Temperatura: ");
    Serial.print(TemperaturaTanque);    
}
// Funcao abaixo realiza o recebimento dos dados
void onReceiveData(const uint8_t *mac, const uint8_t *data, int len) {
  Serial.print("Received from MAC: "); 
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5)Serial.print(":");
  } // Printa o endereco MAC recebido do hardware 
  Serial.print("\n");
  
  memcpy(&Recebimento, data, sizeof(Recebimento));
  TemperaturaCaminhao = Recebimento.Temperatura; // Recebe a temperatura do caminhao
}
void setup(){
    Serial.begin(115200); // Inicia a Serial
    Wire.begin(I2C_SDA, I2C_SCL); // Inicia o protocolo I2C
    sensors.begin(); // Inicializa a leitura da temperatura
    InicializaLaser();
    
    WiFi.mode(WIFI_STA);
    Serial.println();
    Serial.println(WiFi.macAddress());
    
    if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
    // register peer
    esp_now_peer_info_t peerInfo;

    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
            
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
    }
    
}

void loop(){
    /* --- leitura dos sensores --- */
    LeituraLaser();
    CalculoVolumeTanque();
    LeituraTemperatura();
    /* --------------------------  */
    /* --- Envio das leituras --- */
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &Readings, sizeof(Readings));
    if (result == ESP_OK) {
    Serial.println("Sent with success");
    } else {
    Serial.println("Error sending the data");
    }
    /* ------------------------ */
    /* --- Recebimento ------- */
    if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
    }
    esp_now_register_recv_cb(onReceiveData);
    /* ---------------------- */
}
