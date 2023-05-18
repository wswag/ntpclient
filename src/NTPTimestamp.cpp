#include "NTPClient.h"

uint64_t NtpTimestamp::toExt() {
    return ((uint64_t)secsFrom1900 << 32) | secFracs;
}

NtpTimestamp NtpTimestamp::fromExt(uint64_t value) {
    NtpTimestamp result;
    result.secsFrom1900 = value >> 32;
    result.secFracs = value & 0xFFFFFFFF;
    return result;
}

NtpTimestamp NtpTimestamp::operator+(NtpTimestamp b) {
    // convert to uint64 to allow easier calculation
    uint64_t _a = toExt();
    uint64_t _b = b.toExt();
    uint64_t c = _a + _b;
    return fromExt(c);
}

int64_t NtpTimestamp::operator-(NtpTimestamp b) {
    // convert to uint64 to allow easier calculation
    uint64_t _a = toExt();
    uint64_t _b = b.toExt();
    int64_t c = _a - _b;
    return c;
}

NtpTimestamp NtpTimestamp::operator/(uint32_t b) {
    // convert to uint64 to allow easier calculation
    uint64_t _a = toExt();
    uint64_t c = _a / b;
    return fromExt(c);
}

NtpTimestamp NtpTimestamp::addMillis(uint32_t value) {
    uint64_t _a = toExt();
    uint64_t c = _a + ((uint64_t)value * 0xFFFFFFFF) / 1000;
    return fromExt(c);
}

NtpTimestamp NtpTimestamp::addOffset(int64_t offsetExt) {
    uint64_t _a = toExt();
    uint64_t c = (uint64_t)(_a + offsetExt);
    return fromExt(c);
}

uint32_t NtpTimestamp::getEpoch() {
    return secsFrom1900 - SEVENZYYEARS;
}

NtpTimestamp NtpTimestamp::fromEpoch(uint32_t epoch, uint32_t nanosFromEpoch) {
    NtpTimestamp result;
    result.secsFrom1900 = epoch + SEVENZYYEARS;
    result.secFracs = (((uint64_t)nanosFromEpoch << 32) / 1000 / 1000 / 1000);
    return result;
}

uint32_t NtpTimestamp::getNanosFromEpoch() {
    return ((uint64_t)secFracs * 1000 * 1000 * 1000) >> 32;
}

uint32_t NtpTimestamp::getMicrosFromEpoch() {
    return ((uint64_t)secFracs * 1000 * 1000) >> 32;
}

int NtpTimestamp::getDay() {
    return (((getEpoch()  / 86400L) + 4 ) % 7); //0 is Sunday
}

int NtpTimestamp::getHours() {
    return ((getEpoch()  % 86400L) / 3600);
}

int NtpTimestamp::getMinutes() {
    return ((getEpoch() % 3600) / 60);
}

int NtpTimestamp::getSeconds() {
    return (getEpoch() % 60);
}