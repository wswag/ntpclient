/**
 * The MIT License (MIT)
 * Copyright (c) 2015 by Fabrice Weinberg
 *
 * Changes By: Sebastian Wagner
 *  - Full Timestamp / Delay computation (currently: only rec time is used)
 *  - Sub-Second precision (=> use fractional part)
 *  - code refactoring and deletion of unnecessary parts
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "NTPClient.h"

#pragma pack(1)

// from RFC5905 w/o UDP IPAddr Header
struct NtpTransmit {
        uint8_t    li_version_mode;/* several values */
        uint8_t    stratum;        /* stratum */
        uint8_t    poll;           /* poll interval */
        int8_t     precision;      /* precision */
        uint32_t   rootdelay;      /* root delay */
        uint32_t   rootdisp;       /* root dispersion */
        uint32_t   refid;          /* reference ID */
        NtpTimestamp  reftime;        /* reference time */
        NtpTimestamp  org;            /* origin timestamp */
        NtpTimestamp  rec;            /* receive timestamp */
        NtpTimestamp  xmt;            /* transmit timestamp */
};

#pragma pack()

NTPClient::NTPClient(UDP& udp) {
  this->_udp            = &udp;
}

NTPClient::NTPClient(UDP& udp, int timeOffset) {
  this->_udp            = &udp;
  this->_timeOffset     = timeOffset;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName) {
  this->_udp            = &udp;
  this->_poolServerName = poolServerName;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName, int timeOffset) {
  this->_udp            = &udp;
  this->_timeOffset     = timeOffset;
  this->_poolServerName = poolServerName;
}

NTPClient::NTPClient(UDP& udp, const char* poolServerName, int timeOffset, int updateInterval) {
  this->_udp            = &udp;
  this->_timeOffset     = timeOffset;
  this->_poolServerName = poolServerName;
  this->_updateInterval = updateInterval;
}

void NTPClient::begin() {
  this->begin(NTP_DEFAULT_LOCAL_PORT);
}

void NTPClient::begin(int port) {
  this->_port = port;

  this->_udp->begin(this->_port);

  this->_udpSetup = true;
  _lastTimestamp.secsFrom1900 = NTP_INITIAL_TIMESTAMP;
  _lastTimestamp.secFracs = 0;
  _lastUpdate = 0;
}

void swapBytes(uint8_t* src, uint8_t len)
{
  uint8_t buf[8];
  uint8_t len_ = len < 8 ? len : 8;
  memcpy(buf, src, len_);
  for (int i = 0; i < len_; i++)
  {
    src[i] = buf[len_ - i - 1];
  }
}

void setBigEndianTimestamp(NtpTimestamp& dst, NtpTimestamp value) {
  dst.secFracs = value.secFracs;
  dst.secsFrom1900 = value.secsFrom1900;
  swapBytes((uint8_t*)&dst.secFracs, 4);
  swapBytes((uint8_t*)&dst.secsFrom1900, 4);
}

bool NTPClient::forceUpdate() {
  #ifdef DEBUG_NTPClient
    Serial.println("Update from NTP Server");
  #endif

  auto t1fallback = now(); // when response ts = 0...
  this->sendNTPPacket();

  // Wait till data is there or timeout...
  
  int cb = 0;
  uint32_t t0 = millis();
  timeout.Reset();
  do {
    yield();
    cb = this->_udp->parsePacket();
  } while (cb == 0 && !(millis() - t0 > 1000));

  if (timeout.Pulse())
    return false; // timeout

  // save current time and millis() of update
  NtpTimestamp clientRec = now();

  this->_udp->read(this->_packetBuffer, NTP_PACKET_SIZE);

  NtpTransmit* t = (NtpTransmit*)_packetBuffer;

  // Big Endian..
  swapBytes((uint8_t*)(&t->org.secsFrom1900), 4);
  swapBytes((uint8_t*)(&t->org.secFracs), 4);
  swapBytes((uint8_t*)(&t->rec.secsFrom1900), 4);
  swapBytes((uint8_t*)(&t->rec.secFracs), 4);
  swapBytes((uint8_t*)(&t->xmt.secsFrom1900), 4);
  swapBytes((uint8_t*)(&t->xmt.secFracs), 4);

  // add some robustness...
  if (t->org.secsFrom1900 == 0)
    t->org = t1fallback;

  // add clock offset
  int64_t offset = (((t->rec - t->org) + (t->xmt - clientRec)) / 2);

  _lastTimestamp = now().addOffset(offset);
  _lastUpdate = millis(); // Account for delay in reading the time

  return true;
}

bool NTPClient::update() {
  if ((millis() - this->_lastUpdate >= this->_updateInterval)     // Update after _updateInterval
    || this->_lastUpdate == 0) {                                // Update if there was no update yet.
    if (!this->_udpSetup) this->begin();                         // setup the UDP client if needed
    return this->forceUpdate();
  }
  return true;
}

NtpTimestamp NTPClient::now() {
  uint32_t delta = millis() - _lastUpdate;
  return _lastTimestamp.addMillis(delta);
}

void NTPClient::end() {
  this->_udp->stop();

  this->_udpSetup = false;
}

void NTPClient::setTimeOffset(int timeOffset) {
  this->_timeOffset     = timeOffset;
}

void NTPClient::setUpdateInterval(int updateInterval) {
  this->_updateInterval = updateInterval;
}

void NTPClient::sendNTPPacket() {
  // set all bytes in the buffer to 0
  memset(this->_packetBuffer, 0, NTP_PACKET_SIZE);

  NtpTransmit& frame = *(NtpTransmit*)_packetBuffer;

  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  frame.li_version_mode = 0b11100011;
  frame.stratum = 0;
  frame.poll = 6;
  frame.precision = 0xEC;
  frame.refid = 0; // not significant for clients
  /*this->_packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  this->_packetBuffer[1] = 0;     // Stratum, or type of clock
  this->_packetBuffer[2] = 6;     // Polling Interval
  this->_packetBuffer[3] = 0xEC;  // Peer Clock Precision*/
  // 8 bytes of zero for Root Delay & Root Dispersion
  this->_packetBuffer[12]  = 49;
  this->_packetBuffer[13]  = 0x4E;
  this->_packetBuffer[14]  = 49;
  this->_packetBuffer[15]  = 52;

  // set correct reference and client sending time
  setBigEndianTimestamp(frame.reftime, _lastTimestamp);
  setBigEndianTimestamp(frame.org, now());

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  this->_udp->beginPacket(this->_poolServerName, 123); //NTP requests are to port 123
  this->_udp->write(this->_packetBuffer, NTP_PACKET_SIZE);
  this->_udp->endPacket();
}
