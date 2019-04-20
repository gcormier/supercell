// _BV(3)
#include <Wire.h>
#include <avr/pgmspace.h>
#include "supercell.h"
#include "BQ2589x.h"

// i2c Address for the IC
#define BATTERY_PMIC_ID   0x6b

#define STATUS_INTERVAL   5000L
#define WATCHDOG_INTERVAL 2000L

#define PRINTBIN(Num) for (uint32_t t = (1UL<< (sizeof(Num)*8)-1); t; t >>= 1) Serial.write(Num  & t ? '1' : '0'); // Prints a binary number with leading zeros (Automatic Handling)
#define PRINTBINL(Num) for (int i=0;i<(sizeof(Num)*8);i++) Serial.write(((Num >> i) & 1) == 1 ? '1' : '0'); // Prints a binary number with following Placeholder Zeros  (Automatic Handling)

byte reg00 = B00000000;
byte reg03 = B01010000;

// Charging Current
byte reg04 = B00001000;

// Precharge current
byte reg05 = B00100011;

// 4.35v
//byte reg06 = B01111110;
// 4.3v
//byte reg06 = B01110010;
// 4.2v

byte reg06 = B01011010;
byte reg07 = B11101101;


//byte reg0d = B11111111;

byte regNumber;
unsigned long currentMillis = 0L;
unsigned long lastMillis = 0L;
unsigned long watchdogMillis = 0L;
byte inputBuffer[32];

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
  delay(200);

  writeParameter(0x00, reg00);
  writeParameter(0x06, reg06);
  writeParameter(0x07, reg07);
  writeParameter(0x04, reg04);
  writeParameter(0x05, reg05);
  writeParameter(0x03, reg03);
  //writeParameter(0x0d, reg0d);
  toggleADC(true);

  byte r02 = readParameter(0x02);
  //bitClear(r02, BQ2589X_AUTO_DPDM_EN_SHIFT);
  bitClear(r02, 4);
  writeParameter(0x02, r02);

  byte r09 = readParameter(0x09);
  bitClear(r09, 2); // BATFET Full system reset enable
  writeParameter(0x09, r09);
}


void loop()
{
  currentMillis = millis();

  if (currentMillis - watchdogMillis > WATCHDOG_INTERVAL)
  {
    watchdogMillis = currentMillis;
    resetWatchdog();
  }
  
  if ((currentMillis - lastMillis) > STATUS_INTERVAL)
  {
    lastMillis = currentMillis;
    Serialprint("\nBattery voltage is %imV\n", getBatteryMilliVoltage());
    Serialprint("Charge current is %imA\n\n", getChargeCurrent());

    printStatus();
  }
  

  if (Serial.available())
  {
    Serial.readBytes(inputBuffer, 8);
    if (inputBuffer[0] == 'e')
    {
      enableCharge();
      Serial.println("Enable charge");
    }
    else if (inputBuffer[0] == 'd')
    {
      disableCharge();
      Serial.println("Disable charge");
    }
    else if (inputBuffer[0] == 'p')
    {
      byte a = readParameter(0x03);
      a = a ^ _BV(7);
      writeParameter(0x03, a);
      Serial.println("bat load toggle");
      
    }
    else if (inputBuffer[0] == 'R')
    {
      byte a = 0;
      bitSet(a, 7);
      writeParameter(0x14, a);
      Serial.println("RESET");

    }
    else if (inputBuffer[0] == '0')
    {
      byte a = B00000000;
      bitSet(a, 4);
      
      writeParameter(0x00, a);
      Serial.println("set reg00 bit4 (800mA)");

    }
    else if (inputBuffer[0] == '?')
    {
      byte regValue;
      for (regNumber = 0; regNumber <= 0x14; regNumber++)
      {
        regValue = readParameter(regNumber);
        Serialprint("Register %2x has value ", regNumber);
        PRINTBIN(regValue);
        Serial.println();
      }
    }
    else if (inputBuffer[0] == '_')
    {
      //_05,4,1
      byte inputRegister = (inputBuffer[1] - 48) * 10 + (inputBuffer[2] - 48);
      byte bitPosition = inputBuffer[4] - 48;
      byte binaryValue = inputBuffer[6] - 48;

      char tempRegister[3];
      tempRegister[0] = inputBuffer[1];
      tempRegister[1] = inputBuffer[2];
      tempRegister[2] = '\0';

      byte hexRegister = (byte)strtol(tempRegister, NULL, 16);
      Serialprint("Setting register 0x%02X, bit %i, value=%i\n\n", hexRegister, bitPosition, binaryValue);
      
      byte existingRegisterValue = readParameter(hexRegister);
      if (binaryValue == 1)
        bitSet(existingRegisterValue, bitPosition);
      else
        bitClear(existingRegisterValue, bitPosition);

      writeParameter(hexRegister, existingRegisterValue);
      

    }
  }

  
}

void enableCharge()
{
  byte r3;
  r3 = readParameter(0x03);
  bitSet(r3, 4);
  writeParameter(0x03, r3);
}

void disableCharge()
{
  byte r3;
  r3 = readParameter(0x03);
  bitClear(r3, 4);
  writeParameter(0x03, r3);
}

void printStatus()
{
  Serial.println("Status\n------");
  byte r0b = readParameter(0x0b);
  byte stat = r0b >> 5;
  if (stat == B010)
    Serial.println("VBUS Status : Adapter (3.25A)");
  else if (stat == B000)
    Serial.println("VBUS Status : No Input");
  else
  {
    Serial.print("VBUS Status : **Unknown"); PRINTBIN(r0b); Serial.println();
  }
  
  stat = r0b << 3;
  stat = stat >> 6;
  
  if (stat == B00)
    Serial.println("Charging Status : Not charging");
  else if (stat == B01)
    Serial.println("Charging Status : Pre-charging");
  else if (stat == B10)
    Serial.println("Charging Status : Charging");
  else if (stat == B11)
    Serial.println("Charging Status : Termination Complete");

  if (bitRead(r0b, 2) == 1)
    Serial.println("Power Status : Good");
  else if (bitRead(r0b, 2) == 0)
    Serial.println("Power Status : **Bad");

  if (bitRead(r0b, 0) == 0)
    Serial.println("VSYS Status : Not in regulation");
  else if (bitRead(r0b, 0) == 1)
    Serial.println("VSYS Status : **VSYSMIN Regulation");

  byte r0e = readParameter(0x0e);
  if (bitRead(r0e, 7) == 0)
    Serial.println("Thermal Status : Not in regulation");
  else if (bitRead(r0e, 7) == 1)
    Serial.println("Thermal Status : **Thermal Regulation");

  byte r13 = readParameter(0x13);
  if (bitRead(r13, 7) == 0)
    Serial.println("VINDPM Status : Not in VINDPM");
  else if (bitRead(r13, 7) == 1)
    Serial.println("VINDPM Status : ** VINDPM");

  if (bitRead(r13, 6) == 0)
    Serial.println("IINDPM Status : Not in IINDPM");
  else if (bitRead(r13, 6) == 1)
    Serial.println("IINDPM Status : ** IINDPM");



}


void resetWatchdog()
{
  // Get register 0x03 and reset the watchdog bit
  byte currentVal = readParameter(0x03);
  bitSet(currentVal, 6);
  writeParameter(0x03, currentVal);
}

void toggleADC(bool enabled)
{
  byte reg02 = readParameter(0x02);
  if (!enabled) // Presently enabled, need to disable
  {
    bitClear(reg02, 6);
    bitClear(reg02, 7);
  }
  else if (enabled) // Not enabled, need to enable
  {
    bitSet(reg02, 6);
    bitSet(reg02, 7);
  }
  writeParameter(0x02, reg02);
}

uint16_t getChargeCurrent()
{
  byte vChargeStat = readParameter(0x12);
  uint16_t current = 0;

  if (bitRead(vChargeStat, 0))
    current += 50;

  if (bitRead(vChargeStat, 1))
    current += 100;

  if (bitRead(vChargeStat, 2))
    current += 200;

  if (bitRead(vChargeStat, 3))
    current += 400;

  if (bitRead(vChargeStat, 4))
    current += 800;

  if (bitRead(vChargeStat, 5))
    current += 1600;

  if (bitRead(vChargeStat, 6))
    current += 3200;

  return current;
}

float getBusVoltage()
{
  byte vBusStat = readParameter(0x11);
  float vBus = 2.60f;

  if (vBusStat & _BV(0))
    vBus += 0.100f;

  if (vBusStat & _BV(1))
    vBus += 0.200f;

  if (vBusStat & _BV(2))
    vBus += 0.400f;

  if (vBusStat & _BV(3))
    vBus += 0.800f;

  if (vBusStat & _BV(4))
    vBus += 1.600f;

  if (vBusStat & _BV(5))
    vBus += 3.200f;

  if (vBusStat & _BV(6))
    vBus += 6.400f;

  return vBus;
}

float getSystemVoltage()
{
  byte vSysStat = readParameter(0x0f);
  float vSys = 2.304f;

  if (vSysStat & _BV(0))
    vSys += 0.020f;

  if (vSysStat & _BV(1))
    vSys += 0.040f;

  if (vSysStat & _BV(2))
    vSys += 0.080f;

  if (vSysStat & _BV(3))
    vSys += 0.160f;

  if (vSysStat & _BV(4))
    vSys += 0.320f;

  if (vSysStat & _BV(5))
    vSys += 0.640f;

  if (vSysStat & _BV(6))
    vSys += 1.280f;

  return vSys;
}
uint16_t getBatteryMilliVoltage()
{
  byte vBatStat = readParameter(0x0e);
  uint16_t voltage = 2304;
  
  if (bitRead(vBatStat, 0))
    voltage += 20;
  
  if (bitRead(vBatStat, 1))
    voltage += 40;

  if (bitRead(vBatStat, 2))
    voltage += 80;

  if (bitRead(vBatStat, 3))
    voltage += 160;

  if (bitRead(vBatStat, 4))
    voltage += 320;

  if (bitRead(vBatStat, 5))
    voltage += 640;

  if (bitRead(vBatStat, 6))
    voltage += 1280;


  return voltage;
}

void checkPowerStatus()
{
  byte status = readParameter(0x0b);
  Serial.println(status & _BV(0));
    
}

byte writeParameter(byte reg, byte value)
{
  Wire.beginTransmission(BATTERY_PMIC_ID);
  Wire.write(reg);
  Wire.write(value);
  byte success = Wire.endTransmission();
  if (success != 0)
    Serialprint("writeParemeter() got non-success value %i\n", success);
  return success;

  //Wire.requestFrom(BATTERY_PMIC_ID, 1);
  //Wire.read();
}

byte readParameter(byte reg)
{
  Wire.beginTransmission(BATTERY_PMIC_ID);
  Wire.write(reg);
  Wire.endTransmission();
  
  Wire.requestFrom(BATTERY_PMIC_ID, 1);

  return Wire.read();
}