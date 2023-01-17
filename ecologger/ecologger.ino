/*
Configuração do Cartao SD
- Criar um arquivo config.txt dentro do cartao com duas linhas:
  * A primeira é o intervalo entre as gravações em ms
  * A segunda é o tempo de gravação em ms
  - Exemplo:

  60000
  30000

  Nesse exemplo vai gravar 30 segundos e dormir por 1 minuto

  Para reiniciar as gravações delete a pasta rec
*/
#define RESET_PIN A3
#define SLEEP_DELAY 500
#define LDR_PIN A1
#define USE_LONG_FILE_NAMES 0
#define CARDCS 9  // Card chip select pin
#define RESET 8   // VS1053 reset pin (output)
#define CS 6      // VS1053 chip select pin (output)
#define DCS 7     // VS1053 Data/command select pin (output)
#define DREQ 2    // VS1053 Data request, ideally an Interrupt pin
#define SPI_SPEED SD_SCK_MHZ(50)
#define RECBUFFSIZE 128  // 64 or 128 bytes.

#include <Adafruit_BMP085.h>
#include <DeepSleepScheduler.h>
#include "Adafruit_VS1053.h"
#include "LDR.h"

uint8_t recording_buffer[RECBUFFSIZE];
unsigned int timeSleep = -1;
unsigned int timeRecording = -1;
unsigned long int startTimeLoop = -1;
uint16_t fileNumber = -1;

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

void checkFiles() {

  File recordingNumber;
  fileNumber = 1;
  if (!sd.begin(CARDCS, SPI_SPEED)) {
    Serial.println(F("Cartão SD não identificado"));
    while (1)
      ;
  }
  File dir;
  dir.open("rec", O_READ);
  if (!dir.isDir()) {
    sd.mkdir("rec");
    Serial.println(F("Diretorio rec nao encontrado"));

    Serial.println(F("Criando arquivo de configuracao..."));
    recordingNumber.open("rec/files.txt", O_RDWR | O_CREAT | O_TRUNC);
    recordingNumber.println("1");
    recordingNumber.close();
  } else {
    Serial.println(F("Diretorio rec encontrado"));
    recordingNumber.open("rec/files.txt", O_READ);
    if (recordingNumber.isFile()) {
      String line1 = readLine(recordingNumber);
      recordingNumber.close();
      fileNumber = line1.toInt();
      Serial.print(F("Quantidade de arquivos: "));
      Serial.println(fileNumber);
      fileNumber++;
      recordingNumber.open("rec/files.txt", O_RDWR | O_CREAT | O_TRUNC);
      recordingNumber.println(String(fileNumber));
      recordingNumber.close();
    } else {
      recordingNumber.close();
      Serial.println(F("Diretorio corrompido, limpe o diretorio rec"));
      while (1)
        ;
    }
  }
  dir.close();

  return fileNumber;
}

void createCsv() {
  float value;
  String line;
  int32_t valueint;
  File csvFile; 
  char filename[15];

  sprintf(filename, "rec/%04i.csv", fileNumber);
  Serial.print(F("Criando arquivo csv: "));
  Serial.println(filename);
  csvFile.open(filename, O_RDWR | O_CREAT | O_TRUNC);
  // lum
  value = ldr.get();
  line = "lum;";
  line.concat(value);
  Serial.println(line);
  csvFile.println(line);
  // temperatura
  bmp.readTemperature();
  value = bmp.readTemperature();
  line = "temp;";
  line.concat(value);
  Serial.println(line);
  csvFile.println(line);
  // pressao
  valueint = bmp.readSealevelPressure();
  line = "press;";
  line.concat(valueint);
  Serial.println(line);
  csvFile.println(line);

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

  checkFiles();

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
  // delay(timeSleep);
  // fileNumber = 14;
  scheduler.schedule(sleep);
  // --
  Serial.println(F("Iniciando gravacao!"));
  startTimeLoop = millis();
  musicPlayer.startRecordOgg(true);
  char filename[15];
  sprintf(filename, "rec/%04i.ogg", fileNumber);
  Serial.print(F("Criando arquivo de audio: "));
  Serial.println(filename);
  recording.open(filename, O_RDWR | O_CREAT | O_TRUNC);
  // ----------
  // Serial.println(F(""));
  // File alldir;
  // if (!alldir.open("/")) {
  //   Serial.println(F("Erro nos diretorios"));
  // }
  // displayDirectoryContent(alldir, 0);
}


void loop() {
  if (millis() - startTimeLoop > timeRecording) {
    musicPlayer.stopRecordOgg();
    saveRecordedData(false);
    recording.close();


    createCsv();
    delay(500);

    scheduler.execute();
  } else {
    // createCsv();

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


// void displayDirectoryContent(File& aDirectory, byte tabulation) {
//   File file;
//   char fileName[20];

//   if (!aDirectory.isDir()) return;
//   aDirectory.rewind();

//   while (file.openNext(&aDirectory, O_READ)) {
//     if (!file.isHidden()) {
//       file.getName(fileName, sizeof(fileName));
//       for (uint8_t i = 0; i < tabulation; i++) Serial.write('\t');
//       Serial.print(fileName);

//       if (file.isDir()) {
//         Serial.println(F("/"));
//         displayDirectoryContent(file, tabulation + 1);
//       } else {
//         Serial.write('\t');
//         Serial.print(file.fileSize());
//         Serial.println(F(" bytes"));
//       }
//     }
//     file.close();
//   }
// }
