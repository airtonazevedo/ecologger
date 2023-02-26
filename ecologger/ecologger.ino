/*
Configuração do Cartao SD
- Criar um arquivo config.txt dentro do cartao com duas linhas:
  * A primeira é o intervalo entre as gravações em ms
  * A segunda é o tempo de gravação em ms
  - Exemplo:

  60000
  30000

  Nesse exemplo vai gravar 30 segundos e dormir por 1 minuto

*/
#define SLEEP_DELAY 500
#define USE_UTF8_LONG_NAMES 1

#define RESET_PIN A3 
#define LDR_PIN A1
#define THREE_WIRE_IO 4
#define THREE_WIRE_SCLK 5
#define THREE_WIRE_CE 3
#define DHTTYPE DHT11   
#define DHTPIN A0    
#define CARDCS 9  // Card chip select pin
#define RESET 8   // VS1053 reset pin (output)
#define CS 6      // VS1053 chip select pin (output)
#define DCS 7     // VS1053 Data/command select pin (output)
#define DREQ 2    // VS1053 Data request, ideally an Interrupt pin
#define SPI_SPEED SD_SCK_MHZ(50)
#define RECBUFFSIZE 64  // 64 or 128 bytes.
#define countof(a) (sizeof(a) / sizeof(a[0]))

#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Adafruit_BMP085.h>
#include <DeepSleepScheduler.h>
#include "Adafruit_VS1053.h"
#include "LDR.h"

ThreeWire myWire(THREE_WIRE_IO, THREE_WIRE_SCLK, THREE_WIRE_CE);  // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

uint8_t recording_buffer[RECBUFFSIZE];
unsigned long timeSleep = -1;
unsigned long timeRecording = -1;
unsigned long int startTimeLoop = -1;
char fileName[20];
SdFat sd;
File recording;

Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(RESET, CS, DCS, DREQ, CARDCS);

LDR ldr(LDR_PIN);
Adafruit_BMP085 bmp;

void reset() {
  Serial.println(F("Reiniciando"));
  delay(100);
  digitalWrite(RESET_PIN, LOW);
}

void sleep() {
  Serial.println(F("Entrando em repouso"));
  scheduler.scheduleDelayed(reset, timeSleep);
}

String readLine(File& file) {
  String line;
  char ch;
  while (file.available()) {
    ch = file.read();
    if (ch == '\n') {  // Is the tag starting? / as result this is the tag ending
      return line;     // Return what you have so far (ID)
    } else {           // Everything's OK, append to the string
      line += ch;
    }
  }
  return line;
}


void createCsv() {
  float value;
  String line = "";
  int32_t valueint;
  File csvFile;
  Serial.println(F("Abrindo arquivo csv: "));
  csvFile.open("data.csv", O_RDWR | O_CREAT | O_AT_END);
  // data
  line.concat(fileName);
  line.concat(";");
  // lum
  value = ldr.get();
  line.concat(value);
  line.concat(";");
  // temperatura
  bmp.readTemperature();
  value = bmp.readTemperature();
  line.concat(value);
  line.concat(";");
  // pressao
  valueint = bmp.readSealevelPressure();
  line.concat(valueint);
  Serial.println(line);
  csvFile.println(line);


  RtcDateTime dt = Rtc.GetDateTime();
  if (!csvFile.timestamp(T_WRITE, dt.Year(), dt.Month(), dt.Day(), dt.Hour(),
                         dt.Minute(), dt.Second())) {
    Serial.println(F("Não foi possivel obter a data do csv"));
  }
  csvFile.close();
}

void setup() {
  digitalWrite(RESET_PIN, HIGH);
  delay(200);
  pinMode(RESET_PIN, OUTPUT);
  // Configuracao do monitor serial
  Serial.begin(9600);

  // Configuracao do led
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);
  // Configuracao do relogio
  Rtc.Begin();
  // Configuracao do SD
  if (!sd.begin(CARDCS, SPI_SPEED)) {
    Serial.println(F("Cartão SD não identificado"));
    while (1)
      ;
  }

  File config;
  config.open("config.txt", O_RDWR);
  if (config) {
    int lineNumber = 0;
    String line1 = readLine(config);
    String line2 = readLine(config);
    timeSleep = line1.toInt();
    timeRecording = line2.toInt();
    Serial.print(F("Tempo dormindo: "));
    Serial.println(timeSleep);
    Serial.print(F("Tempo gravando: "));
    Serial.println(timeRecording);
    config.close();
  } else {
    Serial.println(F("Arquivo config.txt nao encontrado"));
    while (1)
      ;
  }

  // Configuracao do gravador
  if (!musicPlayer.begin()) {
    Serial.println(F("VS1053 not found"));
    while (1)
      ;
  }
  musicPlayer.setVolume(10, 10);
  if (!musicPlayer.prepareRecordOgg("v16k1q05.img")) {
    Serial.println(F("Não foi possivel carregar o v16k1q05"));
    while (1)
      ;
  }

  while (!bmp.begin()) {}

  Serial.println(F("Setup concluido"));
  scheduler.schedule(sleep);
  // --
  Serial.println(F("Iniciando gravacao!"));
  RtcDateTime dt = Rtc.GetDateTime();
  char datestring[15];

  snprintf_P(fileName,
             countof(fileName),
             PSTR("%02u%02u%02u%02u%02u%02u"),
             dt.Year() - 2000,
             dt.Month(),
             dt.Day(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  startTimeLoop = millis();
  musicPlayer.startRecordOgg(true);
  Serial.print(F("Criando arquivo de audio: "));
  char audioFile[18];
  sprintf(audioFile, "%s.ogg", fileName);
  Serial.println(audioFile);
  recording.open(audioFile, O_RDWR | O_CREAT | O_TRUNC);
  if (!recording.timestamp(T_CREATE, dt.Year(), dt.Month(), dt.Day(), dt.Hour(),
                           dt.Minute(), dt.Second())) {
    Serial.println(F("Não foi possivel obter a data de criacao"));
  }
}


void loop() {
  if (millis() - startTimeLoop > timeRecording) {
    musicPlayer.stopRecordOgg();
    saveRecordedData(false);
    RtcDateTime dt = Rtc.GetDateTime();
    if (!recording.timestamp(T_WRITE, dt.Year(), dt.Month(), dt.Day(), dt.Hour(),
                             dt.Minute(), dt.Second())) {
      Serial.println(F("Não foi possivel obter a data de gravacao"));
    }
    recording.close();

    createCsv();
    sd.end();
    delay(500);

    scheduler.execute();
  } else {
    saveRecordedData(true);
  }
}

uint16_t saveRecordedData(boolean isrecord) {
  uint16_t written = 0;

  // read how many words are waiting for us
  uint16_t wordswaiting = musicPlayer.recordedWordsWaiting();

  // try to process 256 words (512 bytes) at a time, for best speed
  while (wordswaiting > 256) {
    //Serial.print(F("Waiting: ")); Serial.println(wordswaiting);
    // for example 128 bytes x 4 loops = 512 bytes
    for (int x = 0; x < 512 / RECBUFFSIZE; x++) {
      // fill the buffer!
      for (uint16_t addr = 0; addr < RECBUFFSIZE; addr += 2) {
        uint16_t t = musicPlayer.recordedReadWord();
        //Serial.println(t, HEX);
        recording_buffer[addr] = t >> 8;
        recording_buffer[addr + 1] = t;
      }
      if (!recording.write(recording_buffer, RECBUFFSIZE)) {
        Serial.print(F("Couldn't write "));
        Serial.println(RECBUFFSIZE);
        while (1)
          ;
      }
    }
    // flush 512 bytes at a time
    recording.flush();
    written += 256;
    wordswaiting -= 256;
  }

  wordswaiting = musicPlayer.recordedWordsWaiting();
  if (!isrecord) {
    Serial.print(wordswaiting);
    Serial.println(F(" remaining"));
    // wrapping up the recording!
    uint16_t addr = 0;
    for (int x = 0; x < wordswaiting - 1; x++) {
      // fill the buffer!
      uint16_t t = musicPlayer.recordedReadWord();
      recording_buffer[addr] = t >> 8;
      recording_buffer[addr + 1] = t;
      if (addr > RECBUFFSIZE) {
        if (!recording.write(recording_buffer, RECBUFFSIZE)) {
          Serial.println(F("Couldn't write!"));
          while (1)
            ;
        }
        recording.flush();
        addr = 0;
      }
    }
    if (addr != 0) {
      if (!recording.write(recording_buffer, addr)) {
        Serial.println(F("Couldn't write!"));
        while (1)
          ;
      }
      written += addr;
    }
    musicPlayer.sciRead(VS1053_SCI_AICTRL3);
    if (!(musicPlayer.sciRead(VS1053_SCI_AICTRL3) & (1 << 2))) {
      recording.write(musicPlayer.recordedReadWord() & 0xFF);
      written++;
    }
    recording.flush();
  }

  return written;
}