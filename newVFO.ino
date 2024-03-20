/*
This VFO is expected to be eventually a part of the stationmaster hardware.
My plan is to move from the current Raspberry Pi to a Mac Mini.  This will
allow me to get away from the GPIO interface to the band switch and also
eliminate the diode encoder and just use the Arduino pins.  The speed of the
Arduino Uno R4 Minima may also allow me to implement split operation (and RIT).
*/

const byte address = 0x81;
int resetPin = D12;
int clockPin = A5;
int updatePin = D13;
int dataPin = A4;

int bandPin4 = D4;
int bandPin2 = D5;
int bandPin1 = D6;
int xmtPin = D8;

int split = 0;
int xFreq;
int rFreq;
char b;
unsigned char bnd[3];
int rcvMsgLen = 8;
char band = 0;

unsigned char rBuff[10];
unsigned char wBuff[3];

void initBuffers();
void initvfo();
void runvfo(int, int);
void streamData(int);
char readBand();
char readBandOnce();
void sendBand(unsigned char[], int);
void sendAck();
void sendNack();
unsigned int calcCRC(unsigned char[], int);
bool checkCRC(unsigned char[], int);


void setup() {
  //Serial.begin(115200);
  //while (!Serial) delay(10);
  Serial1.begin(115200);
  while (!Serial1) delay(10);

  pinMode(resetPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(updatePin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  initvfo();
}

void loop() {
  initBuffers();
  int n = 0;
  int validAddress = 0;
  n = Serial1.available();
  //Serial.print(n);
  if (n == rcvMsgLen) {
    for (int i = 0; i < n; ++i) {
      int s = Serial1.read();
      rBuff[i] = s;
    }
  }
  if (rBuff[0] == address) {
    validAddress = 1;
  }
  if (validAddress) {
    band = readBandOnce();
    switch (rBuff[1]) {
      case 0x01:  //test if I am here
        if (checkCRC(rBuff, rcvMsgLen)) {
          sendAck();
        } else {
          sendNack();
        }
        break;
      case 0x02:  //set split
        if (checkCRC(rBuff, rcvMsgLen)) {
          split = rBuff[2];
          sendAck();
        } else {
          sendNack();
        }
        break;
      case 0x03:  //set transmit frequency
        if (checkCRC(rBuff, rcvMsgLen)) {
          xFreq = rBuff[2] << 24 | rBuff[3] << 16 | rBuff[4] << 8 | rBuff[5];
          runvfo(xFreq, rFreq);
          sendAck();
        } else {
          sendNack();
        }
        break;
      case 0x04:  //set recieve frequency
        if (checkCRC(rBuff, rcvMsgLen)) {
          rFreq = rBuff[2] << 24 | rBuff[3] << 16 | rBuff[4] << 8 | rBuff[5];
          sendAck();
        } else {
          sendNack();
        }
        break;
      case 0x05:
        if (checkCRC(rBuff, rcvMsgLen)) {
          bnd[0] = band;
          sendBand(bnd, 3);
        } else {
          sendNack();
        }
    }
  }
  //delay(50);
}


void initvfo() {
  //set all pins to  low
  digitalWrite(resetPin, LOW);
  digitalWrite(clockPin, LOW);
  digitalWrite(updatePin, LOW);
  digitalWrite(dataPin, LOW);

  //reset the chip
  digitalWrite(resetPin, HIGH);
  delayMicroseconds(1);
  digitalWrite(resetPin, LOW);
  streamData(171798692);
  streamData(171798692);
}

void runvfo(int xFreq, int rFreq) {
  //These two lines commented until I implement the split feature
  //xPhase := (xFreq * math.Pow(2.0, 32.0)) / 125.0
  //xP := int32(xPhase)
  streamData(xFreq);
}

void streamData(int s) {
  int dd;
  //Serial.println(s, HEX);
  for (int i = 0; i < 40; i++) {
    if (i < 32) {
      dd = s & 0x00000001;
      s = s >> 1;
    } else {
      dd = 0;
    }
    //Serial.print(dd);
    //Serial.print(" ");
    digitalWrite(dataPin, dd);
    digitalWrite(clockPin, HIGH);
    //delayMicroseconds(1);
    digitalWrite(clockPin, LOW);
    //s = s >> 1;
  }
  //Serial.println("");
  delayMicroseconds(1);
  digitalWrite(updatePin, HIGH);
  delayMicroseconds(1);
  digitalWrite(updatePin, LOW);
}

void sendBand(unsigned char msg[], int nBytes) {
  unsigned int crc = calcCRC(msg, nBytes - 2);
  msg[nBytes - 2] = crc >> 8;
  msg[nBytes - 1] = crc;
  //for (int i = 0; i < nBytes; i++) {
  //if (Serial.availableForWrite() == 3) {
    Serial1.write(msg, 3);
  //}
  //Serial.print(msg[i]);
  //Serial.print(" ");
  //}
  //Serial.println("");
}

char readBand() {
  while (true) {
    char n1 = readBandOnce();
    delay(100);
    char n2 = readBandOnce();
    delay(100);
    char n3 = readBandOnce();
    delay(100);
    if (n1 == n2 && n2 == n3) {
      return n1;
    }
  }
}


char readBandOnce() {
  char b = 0x00;
  if (digitalRead(bandPin4) == HIGH) {
    b = b | 0x04;
  }
  if (digitalRead(bandPin2) == HIGH) {
    b = b | 0x02;
  }
  if (digitalRead(bandPin1) == HIGH) {
    b = b | 0x01;
  }
  if (digitalRead(xmtPin) == HIGH) {
    b = b | 0X08;
  }
  return b;
}

void sendAck() {
  Serial1.write((char)0xFF);
}

void sendNack() {
  Serial1.write((char)0x00);
}

//call this function with the exact number of bytes you want processed
unsigned int calcCRC(unsigned char msg[], int nBytes) {
  //unsigned char msg[nBytes];
  unsigned int crc = 0x0000;
  for (int byte = 0; byte < nBytes; byte++) {
    crc = crc ^ ((unsigned int)msg[byte] << 8);
    for (unsigned char bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc = crc << 1;
      }
    }
  }
  return crc;
}

bool checkCRC(unsigned char msg[], int nBytes) {
  unsigned int crc = calcCRC(msg, nBytes - 2);
  unsigned char byte1 = crc >> 8;  //High byte
  unsigned char byte0 = crc;       //Low byte
  if (byte1 == msg[nBytes - 2] && byte0 == msg[nBytes - 1]) {
    return true;
  }
  return false;
}

void initBuffers() {
  for (int i = 0; i < 3; i++) {
    wBuff[i] = 0;
  }
  for (int i = 0; i < 8; i++) {
    rBuff[i] = 0;
  }
}
