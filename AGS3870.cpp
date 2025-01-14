//
//    FILE: AGS3870.cpp
//  AUTHOR: Rob Tillaart
//    DATE: 2025-01-14
// VERSION: 0.1.0
// PURPOSE: Arduino library for AGS3870 - Methane CH4 sensor.
//     URL: https://github.com/RobTillaart/AGS3870


#include "AGS3870.h"


//  REGISTERS
#define AGS3870_PPM                   0x00
#define AGS3870_CALIBRATION           0x01
#define AGS3870_VERSION               0x11
#define AGS3870_RESISTANCE            0x20


AGS3870::AGS3870(TwoWire *wire)
{
  _wire    = wire;
  reset();
}


bool AGS3870::begin()
{
  _startTime = millis();  //  PREHEAT TIMING 120 seconds
  return isConnected();
}


bool AGS3870::isConnected()
{
  _wire->beginTransmission(_address);
  bool rv = ( _wire->endTransmission(true) == 0);
  return rv;
}


void AGS3870::reset()
{
  _startTime     = millis();
  _lastRead      = 0;
  _lastPPM       = 0;
  _status        = AGS3870_OK;
  _error         = AGS3870_OK;
}


uint8_t AGS3870::getVersion()
{
  uint8_t version = 0xFF;
  if (_readRegister(AGS3870_VERSION))
  {
    version = _buffer[3];
    if (_CRC8(_buffer, 5) != 0)
    {
      _error = AGS3870_ERROR_CRC;
    }
  }
  return version;
}


uint32_t AGS3870::readPPM()
{
  uint32_t value = _readSensor();
  if (_error == AGS3870_OK)
  {
    _lastRead = millis();
    _lastPPM = value;
  }
  else
  {
    value = _lastPPM;
  }
  return value;
}


uint32_t AGS3870::readResistance()
{
  uint32_t value = 0;
  if (_readRegister(AGS3870_RESISTANCE))
  {
    value =  _buffer[0] * 65536UL;
    value += _buffer[1] * 256;
    value += _buffer[2];
    //  _buffer[3] == unknown ?
    if (_CRC8(_buffer, 5) != 0)
    {
      _error = AGS3870_ERROR_CRC;
    }
  }
  return value * 10;
}


bool AGS3870::manualZeroCalibration(uint16_t value)
{
  _buffer[0] = 0x00;
  _buffer[1] = 0x00;
  _buffer[2] = (uint8_t) (value >> 8);
  _buffer[3] = (uint8_t) (value & 0x00FF);
  _buffer[4] = _CRC8(_buffer, 4);
  return _writeRegister(AGS3870_CALIBRATION);
}


bool AGS3870::getZeroCalibrationData(AGS3870::ZeroCalibrationData &data) {
  if (!_readRegister(AGS3870_CALIBRATION))
  {
    return false;
  }

  if (_CRC8(_buffer, 5) != 0)
  {
    _error = AGS3870_ERROR_CRC;
    return false;
  }

  _error = AGS3870_OK;
  //  Don't pollute the struct given to us, until we've handled all error cases.
  data.status = _getDataMSB();
  data.value = _getDataLSB();
  return true;
}


int AGS3870::lastError()
{
  int e = _error;
  _error = AGS3870_OK;  //  reset error after read
  return e;
}


bool AGS3870::readRegister(uint8_t address, AGS3870::RegisterData &reg) {
  if (!_readRegister(address))
  {
    return false;
  }

  if (_CRC8(_buffer, 5) != 0)
  {
    _error = AGS3870_ERROR_CRC;
    return false;
  }

  _error = AGS3870_OK;
  //  Don't pollute the struct given to us, until we've handled all error cases.
  reg.data[0] = _buffer[0];
  reg.data[1] = _buffer[1];
  reg.data[2] = _buffer[2];
  reg.data[3] = _buffer[3];
  reg.crc = _buffer[4];
  reg.crcValid = true;  //  checked above.
  return true;
}


/////////////////////////////////////////////////////////
//
//  PRIVATE
//
uint32_t AGS3870::_readSensor()
{
  uint32_t value = 0;
  if (_readRegister(AGS3870_PPM))
  {
    _error = AGS3870_OK;
    _status = _buffer[0];
    if (_status & 0x01)
    {
      _error = AGS3870_ERROR_NOT_READY;
    }
    value =  _buffer[1] * 65536UL;
    value += _buffer[2] * 256;
    value += _buffer[3];
    if (_CRC8(_buffer, 5) != 0)
    {
      _error = AGS3870_ERROR_CRC;
    }
  }
  return value;
}


bool AGS3870::_readRegister(uint8_t reg)
{
  _wire->beginTransmission(_address);
  _wire->write(reg);
  _error = _wire->endTransmission(true);
  if (_error != 0)
  {
    //  _error will be I2C error code
    return false;
  }

  if (_wire->requestFrom(_address, (uint8_t)5) != 5)
  {
    _error = AGS3870_ERROR_READ;
    return false;
  }
  for (uint8_t i = 0; i < 5; i++)
  {
    _buffer[i] = _wire->read();
  }
  _error = AGS3870_OK;
  return true;
}


bool AGS3870::_writeRegister(uint8_t reg)
{
  _wire->beginTransmission(_address);
  _wire->write(reg);
  for (uint8_t i = 0; i < 5; i++)
  {
    _wire->write(_buffer[i]);
  }
  _error = _wire->endTransmission(true);
  return (_error == 0);
}


uint16_t AGS3870::_getDataMSB()
{
  return (_buffer[0] << 8) + _buffer[1];
}


uint16_t AGS3870::_getDataLSB()
{
  return (_buffer[2] << 8) + _buffer[3];
}


uint8_t AGS3870::_CRC8(uint8_t * buf, uint8_t size)
{
  uint8_t crc = 0xFF;  //  start value
  for (uint8_t b = 0; b < size; b++)
  {
    crc ^= buf[b];
    for (uint8_t i = 0; i < 8; i++)
    {
      if (crc & 0x80) crc = (crc << 1) ^ 0x31;
      else crc = (crc << 1);
    }
  }
  return crc;
}


//  -- END OF FILE --

