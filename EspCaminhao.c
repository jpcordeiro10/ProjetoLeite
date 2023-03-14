// Firmware da ESP do caminhao responsavel por:
// - Ler a temperatura
// - Receber dados do gps
// - Recebimento do nível do tanque e da temperatura do tanque 
// - Gravar tudo no cartao SD

/* --- Inclusao de bibliotecas --- */
#include <esp_now.h>
#include <WiFi.h>
// Temperatura
#include <OneWire.h>
#include <DallasTemperature.h>
// GPS
#include <TinyGPSPlus.h>
// SD Card
#include <SD.h>
#include <SPI.h>
/* ------------------------------- */

/* --- Pinos --- */ 
// Numero de leituras de temperatura
#define NUMERODEMEDIDAS 10
// Temperatura
#define oneWireBus 22  
// GPS
#define PIN_SPI_SD_CS          5
#define PIN_SPI_SD_MOSI       23
#define PIN_SPI_SD_MISO       19
#define PIN_SPI_SD_CLK        18
SPIClass sd_spi = SPIClass(3);
/* ------------------------------- */
/* ---- Operadores Logicos */
#define true 1
#define false 0
int Envia, Recebe;
/* --- Objetos das bibliotecas --- */
// Temperatura    
    OneWire oneWire(oneWireBus);
    DallasTemperature sensors(&oneWire);
// GPS
    TinyGPSPlus gps;
// SD Card
    File myFile;
/* ------------------------------- */

/* --- Variaveis --- */
// Variaveis para Leitura da Temperatura
float Acumulador = 0, TemperaturaLida, TempCalibrada, MediaTemp, CorrecaoTemp;
// Variavel de recebimento
float TemperaturaCaminhao, TemperaturaTanque, Volume;
// P/ GPS
unsigned long lastDisplayTime = 0;
/* ------------------------------- */

 // Struct que utilizada para gravar no arquivo dentro do cartao SD
typedef struct Dados SENSORES;
struct Dados{
  float latitude;
  float longitude;
  float altura;
  float volume_tanque;
  float temperatura_tanque;
  float temperatura_caminhao;
  float velocidade; 
  int numero_de_satelites;
  int dia, mes, ano;
};
SENSORES dados;
/* --- Struct para envio --- */
typedef struct struct_message {
    float Temperatura;
    float Volume = 0;
} struct_message;
struct_message Readings;
/* --- Struct para recebimento --- */
typedef struct struct_received {
    float Temperatura;
    float Volume;
} struct_received;
struct_received Recebimento;
/* ------------------------------- */
// Endereço MAC da ESP que ira receber
// EC:62:60:9B:98:24
uint8_t broadcastAddress[] = {0xEC,0x62,0x60,0x9B,0x98,0x24}; 

/* --- Funcoes --- */ 
// Inicializa cartão sd
void InicializaCartao(){ // Tem que trocar o numero dentro do SD begin de acordo com o pino CS do microcontrolador
    Serial.print("Initializing SD card...");
    if (!SD.begin(PIN_SPI_SD_CS)) {
        Serial.println("initialization failed!");
    while (!SD.begin(PIN_SPI_SD_CS));
    }
    Serial.println("initialization done.");
    File myFile = SD.open("/teste.csv");
  
    if(!myFile) {
      Serial.println("File doesn't exist");
      Serial.println("Creating file...");
      myFile = SD.open("/teste.csv", FILE_WRITE);
      myFile.print("Latitude");
      myFile.print(",");
      myFile.print("Longitude");
      myFile.print(",");
      myFile.print("Altitude");
      myFile.print(",");
      myFile.print("Ano");
      myFile.print(",");
      myFile.print("Mes");
      myFile.print(",");
      myFile.print("Dia");
      myFile.print(",");
      myFile.print("Velocidade(km/h)");
      myFile.print(",");
      myFile.print("Satelites");
      myFile.print(",");
      myFile.print("Volume(L)");
      myFile.print(",");
      myFile.print("TemperaturaCaminhao");
      myFile.print(",");
      myFile.println("TemperaturaTanque");
    }
    else {
      Serial.println("File already exists");  
    }
    myFile.close();
}
// Leitura do modulo GPS
void LeituraGPS(){ // Funcao que realiza a leitura da string de dados obtida pelo modulo GPS e grava na Struct
    Serial.println("GPS");
//    delay(2000);
    if (Serial2.available() > 0){
        gps.encode(Serial2.read());
        while (gps.location.isValid()&& (millis() - lastDisplayTime >= 2000)){
            // mostra a latitude
            Serial.print("Latitude= "); 
            dados.latitude = gps.location.lat();
            Serial.println(dados.latitude, 6); 
            
            // mostra a longitude
            Serial.print("Longitude= ");
            dados.longitude = gps.location.lng(); 
            Serial.println(dados.longitude, 6); 
            
            // Altitude em metros 
            Serial.print("Altitude in meters = "); 
            dados.altura = gps.altitude.meters();
            Serial.println(dados.altura); 

            // ano (2000+) 
            Serial.print("Year = "); 
            dados.ano =  gps.date.year();
            Serial.println(dados.ano); 
            
            // mes (1-12)
            Serial.print("Month = "); 
            dados.mes = gps.date.month();
            Serial.println(dados.mes); 
            
            // dia (1-31) 
            Serial.print("Day = "); 
            dados.dia = gps.date.day();
            Serial.println(dados.dia); 

            // velocidade em km/h 
            Serial.print("Speed in km/h = "); 
            dados.velocidade = gps.speed.kmph();
            Serial.println(dados.velocidade); 
        
            // número de satélites em uso
            Serial.print("Number os satellites in use = ");
            dados.numero_de_satelites = gps.satellites.value(); 
            Serial.println(dados.numero_de_satelites);
            
            // volume do tanque
            Serial.print("Volume em Litros [L] = ");
            Serial.println(dados.volume_tanque);

            // temperatura do leite tanque
            Serial.print("Temperatura do leite [°C] = ");
            Serial.println(dados.temperatura_tanque);

            // temperatura do leite caminhao
            Serial.print("Temperatura do leite [°C] = ");
            Serial.println(dados.temperatura_caminhao);
            lastDisplayTime = millis();
            LeituraTemperatura();
            GravacaoDeDados();
        }
    }              
}
void LeituraTemperatura(){ // Faz a leitura e a correcao da temperatura
    sensors.requestTemperatures();
    sensors.getTempCByIndex(0);

    int i;
    Acumulador = 0;
    for (i = 0; i <= NUMERODEMEDIDAS; i++){ // Pode alterar as medidas no define
        TemperaturaLida = sensors.getTempCByIndex(0);
        Acumulador = TemperaturaLida + Acumulador;
    }
    MediaTemp = Acumulador / NUMERODEMEDIDAS; //Calcula a media de temperatura
    Readings.Temperatura = MediaTemp; // Grava a media de temperatura na Struct de envio
    TemperaturaCaminhao = MediaTemp; // Muda a temperatura de variavel
    dados.temperatura_caminhao = TemperaturaCaminhao; // Grava a temperatura do caminhao na Struct de gravação de arquivo
    Serial.print("Temperatura Lida [L]: ");
    Serial.println(TemperaturaLida);   
    Serial.print("Temperatura Media [L]: ");
    Serial.println(TemperaturaCaminhao);
    Envia = 1;  
}

// Gravacao de dados no cartao SD
void GravacaoDeDados(){ // Funcao que realiza a gravacao da Struct dentro de um arquivo .csv dentro do cartao SD
    // Note que os dados são separados por virgulas e a cada Enter "println" marca o inicio de uma nova sentenca obtida
    // Isso facilita a tabulação de um .csv dentro do Excel
    Serial.println("Gravação");
    myFile = SD.open("/teste.csv", FILE_APPEND);
        if (myFile)
            {
            myFile.print(dados.latitude);
            myFile.print(",");
            myFile.print(dados.longitude);
            myFile.print(",");
            myFile.print(dados.altura);
            myFile.print(",");
            myFile.print(dados.ano);
            myFile.print(",");
            myFile.print(dados.mes);
            myFile.print(",");
            myFile.print(dados.dia);
            myFile.print(",");
            myFile.print(dados.velocidade);
            myFile.print(",");
            myFile.print(dados.numero_de_satelites);
            myFile.print(",");
            myFile.print(dados.volume_tanque);
            myFile.print(",");
            myFile.print(CorrecaoTemp);
            myFile.print(",");
            myFile.println(TemperaturaTanque);
            
            Serial.println("Gravou");
            }
    myFile.close();
    
}

void onReceiveData(const uint8_t *mac, const uint8_t *data, int len) {
 
  Serial.print("Received from MAC: "); 
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5)Serial.print(":");
  } // Printa o endereco MAC recebido do hardware 
  Serial.print("\n");
  
  memcpy(&Recebimento, data, sizeof(Recebimento));
  TemperaturaTanque = Recebimento.Temperatura; // Recebe a temperatura do caminhao
  Volume = Recebimento.Volume; // Recebe o volume do tanque e grava
  CorrecaoTemp = TemperaturaCaminhao - TemperaturaTanque; // Corrige a temperatura
  dados.temperatura_tanque = TemperaturaTanque; // Grava a temperatura do tanque do produtor na Struct de gravacao 
  dados.volume_tanque = Volume; // Grava o volume do tanque na Struct de gravacao
 
}
void setup (){
    // UART com o monitor serial do PC
    Serial.begin(115200);
    // UART com o modulo GPS
    Serial2.begin(9600);
    sensors.begin();
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
    InicializaCartao();
}

void loop(){
    LeituraGPS();
    /* --- Envio das leituras --- */
    if(Envia == 1){
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &Readings, sizeof(Readings));
        if (result == ESP_OK) {
        Serial.println("Sent with success");
        } else {
        Serial.println("Error sending the data");
        }
        Envia = 0;
        Recebe = 1;
    }
    /* ------------------------ */
    /* --- Recebimento ------- */
    
    if(Recebe == 1){
        if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
        }
        Recebe = 0;
        Envio = 1;
        esp_now_register_recv_cb(onReceiveData);
    }
    /* ---------------------- */
}
