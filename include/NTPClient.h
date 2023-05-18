#pragma once

#include <Arduino.h>
#include <Udp.h>

#define SEVENZYYEARS 2208988800UL
#define NTP_PACKET_SIZE 48
#define NTP_DEFAULT_LOCAL_PORT 1337
#define NTP_INITIAL_TIMESTAMP 1577836800 + SEVENZYYEARS

#pragma pack(1)
struct NtpTimestamp
{
  uint32_t secsFrom1900;
  uint32_t secFracs;

  uint64_t toExt();
  static NtpTimestamp fromExt(uint64_t value);
  NtpTimestamp operator+(NtpTimestamp b);
  int64_t operator-(NtpTimestamp b);

  NtpTimestamp operator/(uint32_t b);
  NtpTimestamp addMillis(uint32_t value);
  NtpTimestamp addOffset(int64_t offsetExt);

  uint32_t getEpoch();
  static NtpTimestamp fromEpoch(uint32_t epoch, uint32_t nanosFromEpoch);
  uint32_t getNanosFromEpoch();
  uint32_t getMicrosFromEpoch();

  int getDay();
  int getHours();
  int getMinutes();
  int getSeconds();

};
#pragma pack()

class NTPClient {
  private:
    UDP*          _udp;
    bool          _udpSetup       = false;

    const char*   _poolServerName = "time.nist.gov"; // Default time server
    int           _port           = NTP_DEFAULT_LOCAL_PORT;
    int           _timeOffset     = 0;

    unsigned int  _updateInterval = 60000;  // In ms
    unsigned long  _lastUpdate     = 0;      // In ms

    NtpTimestamp _lastTimestamp;

    byte          _packetBuffer[NTP_PACKET_SIZE];

    void          sendNTPPacket();

  public:
    NTPClient(UDP& udp);
    NTPClient(UDP& udp, int timeOffset);
    NTPClient(UDP& udp, const char* poolServerName);
    NTPClient(UDP& udp, const char* poolServerName, int timeOffset);
    NTPClient(UDP& udp, const char* poolServerName, int timeOffset, int updateInterval);

    /**
     * Starts the underlying UDP client with the default local port
     */
    void begin();

    /**
     * Starts the underlying UDP client with the specified local port
     */
    void begin(int port);

    /**
     * This should be called in the main loop of your application. By default an update from the NTP Server is only
     * made every 60 seconds. This can be configured in the NTPClient constructor.
     *
     * @return true on success, false on failure
     */
    bool update();

    /**
     * This will force the update from the NTP Server.
     *
     * @return true on success, false on failure
     */
    bool forceUpdate();

    /**
     * Changes the time offset. Useful for changing timezones dynamically
     */
    void setTimeOffset(int timeOffset);

    /**
     * Set the update interval to another frequency. E.g. useful when the
     * timeOffset should not be set in the constructor
     */
    void setUpdateInterval(int updateInterval);

    /**
     * @return current time stamp
     */
    NtpTimestamp now();

    /**
     * Stops the underlying UDP client
     */
    void end();
};
