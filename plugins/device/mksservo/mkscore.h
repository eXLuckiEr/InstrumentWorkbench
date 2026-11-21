#pragma once
// MKS SERVO42D/57D (CAN) protocol without Qt: request frames with checksum, response checks and
// decoders, builders of the commands for endstops/home/motion. Header only, testable on its own
// (tools/mkscore_test.cpp). Reference: "MKS SERVO42&57D CAN User Manual V1.0.9".
//
// A frame: CAN ID = the motor address (1..2047, default 1; 0 = broadcast, no answer), data =
// code, parameters (big endian), CRC. CRC = (ID + all bytes before it) & 0xFF, or the fixed 0x6B
// if the motor is set to that mode (command 4CH).
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "iwp/cancore.h"

namespace mks {

using iwp::can::Frame;

enum class CrcMode { Sum8, Fixed6B };

namespace code {
// read-only
constexpr std::uint8_t ReadConfig = 0x00;        // 00 <code>: read a configuration parameter
constexpr std::uint8_t AutoReturn = 0x01;
constexpr std::uint8_t Encoder = 0x30;           // carry(i32) + value(u16)
constexpr std::uint8_t EncoderAccum = 0x31;      // i48
constexpr std::uint8_t Speed = 0x32;             // i16 rpm
constexpr std::uint8_t Pulses = 0x33;            // i32
constexpr std::uint8_t IoStatus = 0x34;          // u8
constexpr std::uint8_t EncoderRaw = 0x35;        // i48
constexpr std::uint8_t VoltCurrent = 0x38;       // u16 mV, u16 mA
constexpr std::uint8_t AngleError = 0x39;        // i32 (51200 = 360 deg)
constexpr std::uint8_t EnableState = 0x3A;       // u8
constexpr std::uint8_t HomeState = 0x3B;         // u8, u8
constexpr std::uint8_t StallRelease = 0x3D;
constexpr std::uint8_t StallState = 0x3E;
constexpr std::uint8_t Version = 0x40;
constexpr std::uint8_t UserId = 0x42;
// write
constexpr std::uint8_t Crc = 0x4C;
constexpr std::uint8_t Calibrate = 0x80;
constexpr std::uint8_t WorkMode = 0x82;
constexpr std::uint8_t WorkCurrent = 0x83;
constexpr std::uint8_t Microstep = 0x84;
constexpr std::uint8_t CanBitrate = 0x8A;
constexpr std::uint8_t CanId = 0x8B;
constexpr std::uint8_t HoldCurrent = 0x9B;
constexpr std::uint8_t HomeParams = 0x90;
constexpr std::uint8_t GoHome = 0x91;
constexpr std::uint8_t SetZero = 0x92;
constexpr std::uint8_t HomeOffset = 0x94;
constexpr std::uint8_t LimitRemap = 0x9E;
constexpr std::uint8_t MotorStatus = 0xF1;
constexpr std::uint8_t Enable = 0xF3;
constexpr std::uint8_t SpeedRun = 0xF6;
constexpr std::uint8_t MoveRel = 0xF4;          // relative move by encoder coordinate (16384 per turn)
constexpr std::uint8_t EmergencyStop = 0xF7;
} // namespace code

inline std::uint8_t checksum(std::uint16_t id, const std::uint8_t *data, int n, CrcMode mode)
{
    if (mode == CrcMode::Fixed6B)
        return 0x6B;
    unsigned sum = id;
    for (int i = 0; i < n; ++i)
        sum += data[i];
    return static_cast<std::uint8_t>(sum & 0xFF);
}

// Request frame: ID, code, parameters, CRC. At most 6 parameter bytes (DLC 8).
inline Frame request(std::uint16_t id, std::uint8_t cmd, const std::vector<std::uint8_t> &params = {},
                     CrcMode mode = CrcMode::Sum8)
{
    Frame f;
    f.id = id;
    int n = 0;
    f.data[n++] = cmd;
    for (std::uint8_t b : params) {
        if (n >= 7)
            break;
        f.data[n++] = b;
    }
    f.data[n] = checksum(id, f.data, n, mode);
    f.len = static_cast<std::uint8_t>(n + 1);
    return f;
}

// An answer: classic standard frame with a valid CRC, at least code + CRC. On success `cmd` is its code and
// `params` / `count` point to the parameter bytes (between the code and the CRC).
inline bool parseResponse(const Frame &f, CrcMode mode, std::uint8_t &cmd, const std::uint8_t *&params, int &count)
{
    if (f.extended() || f.fd() || f.rtr() || f.len < 2)
        return false;
    const int n = f.len - 1;   // bytes before the CRC
    if (checksum(static_cast<std::uint16_t>(f.id), f.data, n, mode) != f.data[n])
        return false;
    cmd = f.data[0];
    params = f.data + 1;
    count = n - 1;
    return true;
}

// ---- big-endian helpers ----
inline std::uint32_t be32(const std::uint8_t *p)
{
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3];
}
inline std::int64_t beSigned(const std::uint8_t *p, int bytes)
{
    std::uint64_t v = 0;
    for (int i = 0; i < bytes; ++i)
        v = (v << 8) | p[i];
    const std::uint64_t sign = std::uint64_t(1) << (bytes * 8 - 1);
    if (v & sign)
        v |= ~((sign << 1) - 1);   // sign extension (for bytes < 8)
    return static_cast<std::int64_t>(v);
}
inline void putBe(std::vector<std::uint8_t> &out, std::uint64_t v, int bytes)
{
    for (int i = bytes - 1; i >= 0; --i)
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

// ---- decoders (params without code and CRC; false if too short) ----
struct EncoderValue {
    std::int32_t carry = 0;
    std::uint16_t value = 0;                       // 0..0x3FFF within a turn
    std::int64_t total() const { return std::int64_t(carry) * 0x4000 + value; }   // counts, 16384 per turn
    double degrees() const { return double(total()) * 360.0 / 16384.0; }
};
inline bool decodeEncoder(const std::uint8_t *p, int n, EncoderValue &out)
{
    if (n < 6)
        return false;
    out.carry = static_cast<std::int32_t>(be32(p));
    out.value = static_cast<std::uint16_t>((p[4] << 8) | p[5]);
    return true;
}
inline bool decodeI48(const std::uint8_t *p, int n, std::int64_t &out)
{
    if (n < 6)
        return false;
    out = beSigned(p, 6);
    return true;
}
inline bool decodeSpeed(const std::uint8_t *p, int n, int &rpm)
{
    if (n < 2)
        return false;
    rpm = static_cast<int>(beSigned(p, 2));
    return true;
}
inline bool decodeAngleError(const std::uint8_t *p, int n, double &degrees)
{
    if (n < 4)
        return false;
    degrees = double(beSigned(p, 4)) * 360.0 / 51200.0;
    return true;
}
struct IoState {
    bool in1 = false, in2 = false, out1 = false, out2 = false;
    std::uint8_t raw = 0;
};
// bit0 IN_1, bit1 IN_2, bit2 OUT_1, bit3 OUT_2 (with the limit remapping on, bit0 = En, bit1 = Dir)
inline bool decodeIo(const std::uint8_t *p, int n, IoState &out)
{
    if (n < 1)
        return false;
    out.raw = p[0];
    out.in1 = p[0] & 1;
    out.in2 = p[0] & 2;
    out.out1 = p[0] & 4;
    out.out2 = p[0] & 8;
    return true;
}
struct VoltCurrent {
    int millivolts = 0, milliamps = 0;
};
inline bool decodeVoltCurrent(const std::uint8_t *p, int n, VoltCurrent &out)
{
    if (n < 4)
        return false;
    out.millivolts = (p[0] << 8) | p[1];
    out.milliamps = (p[2] << 8) | p[3];
    return true;
}
struct VersionInfo {
    bool calibrated = false;
    int hardware = 0;       // 1 S42D_485, 2 S42D_CAN, 3 S57D_485, 4 S57D_CAN, 5 S28D_485, 6 S28D_CAN, 7 S35D_485, 8 S35D_CAN
    int firmware = 0;       // 12 bits: 0x109 = V1.0.9
    int boot = 0;           // 0x10 RS485, 0x20 CAN
};
// Byte 1: b7-b4 calibration, b3-b0 hardware; bytes 2-3: firmware (the manual says 12 bits); byte 4: boot.
inline bool decodeVersion(const std::uint8_t *p, int n, VersionInfo &out)
{
    if (n < 4)
        return false;
    out.calibrated = (p[0] >> 4) != 0;
    out.hardware = p[0] & 0x0F;
    out.firmware = ((p[1] << 8) | p[2]) & 0x0FFF;
    out.boot = p[3];
    return true;
}
inline const char *hardwareName(int hw)
{
    static const char *names[] = {"?", "SERVO42D RS485", "SERVO42D CAN", "SERVO57D RS485", "SERVO57D CAN",
                                  "SERVO28D RS485", "SERVO28D CAN", "SERVO35D RS485", "SERVO35D CAN"};
    return (hw >= 0 && hw <= 8) ? names[hw] : "?";
}
inline const char *runStatusName(int s)
{
    switch (s) {
    case 0: return "query failed";
    case 1: return "stopped";
    case 2: return "accelerating";
    case 3: return "decelerating";
    case 4: return "full speed";
    case 5: return "homing";
    case 6: return "calibrating";
    default: return "?";
    }
}
inline const char *homeStateName(int s)
{
    switch (s) {
    case 0: return "in progress";
    case 1: return "done";
    case 2: return "failed";
    default: return "?";
    }
}

// ---- builders of the parameter bytes ----
struct HomeParams {
    std::uint8_t trigger = 0;      // level when the switch is closed: 0 low, 1 high
    std::uint8_t direction = 0;    // 0 CW, 1 CCW
    std::uint16_t speedRpm = 100;  // 0..3000
    std::uint8_t endLimit = 0;     // limit function: 0 off, 1 on
    std::uint8_t mode = 0;         // 0 origin switch, 1 mechanical limit, 2 single turn
};
inline std::vector<std::uint8_t> homeParamsBytes(const HomeParams &h)
{
    std::vector<std::uint8_t> p{h.trigger, h.direction};
    putBe(p, h.speedRpm, 2);
    p.push_back(h.endLimit);
    p.push_back(h.mode);
    return p;
}
inline bool decodeHomeParams(const std::uint8_t *p, int n, HomeParams &h)
{
    if (n < 6)
        return false;
    h.trigger = p[0];
    h.direction = p[1];
    h.speedRpm = static_cast<std::uint16_t>((p[2] << 8) | p[3]);
    h.endLimit = p[4];
    h.mode = p[5];
    return true;
}
struct HomeOffset {
    std::uint32_t offset = 0x2000;  // counts, 0x4000 = one turn
    std::uint16_t currentMa = 600;  // mechanical limit homing current
};
inline std::vector<std::uint8_t> homeOffsetBytes(const HomeOffset &h)
{
    std::vector<std::uint8_t> p;
    putBe(p, h.offset, 4);
    putBe(p, h.currentMa, 2);
    return p;
}
inline bool decodeHomeOffset(const std::uint8_t *p, int n, HomeOffset &h)
{
    if (n < 6)
        return false;
    h.offset = be32(p);
    h.currentMa = static_cast<std::uint16_t>((p[4] << 8) | p[5]);
    return true;
}
// F6: run at a speed (dir 0/1, 0..3000 rpm, acc 0..255), optionally for runTime * 10 ms (0 = until stopped)
inline std::vector<std::uint8_t> speedRunBytes(int dir, int rpm, int acc, std::uint32_t runTime10ms = 0)
{
    if (rpm < 0) rpm = 0;
    if (rpm > 3000) rpm = 3000;
    if (acc < 0) acc = 0;
    if (acc > 255) acc = 255;
    std::vector<std::uint8_t> p;
    p.push_back(static_cast<std::uint8_t>(((dir ? 1 : 0) << 7) | ((rpm >> 8) & 0x0F)));
    p.push_back(static_cast<std::uint8_t>(rpm & 0xFF));
    p.push_back(static_cast<std::uint8_t>(acc));
    if (runTime10ms)
        putBe(p, runTime10ms & 0xFFFFFF, 3);
    return p;
}

// F4: relative move by encoder counts (16384 = one turn, sign = direction): speed(2) acc relAxis(3, int24).
inline std::vector<std::uint8_t> moveRelBytes(int rpm, int acc, int counts)
{
    if (rpm < 0) rpm = 0;
    if (rpm > 3000) rpm = 3000;
    if (acc < 0) acc = 0;
    if (acc > 255) acc = 255;
    if (counts > 8388607) counts = 8388607;
    if (counts < -8388607) counts = -8388607;
    std::vector<std::uint8_t> p;
    putBe(p, static_cast<std::uint64_t>(rpm), 2);
    p.push_back(static_cast<std::uint8_t>(acc));
    putBe(p, static_cast<std::uint64_t>(static_cast<std::uint32_t>(counts)) & 0xFFFFFF, 3);
    return p;
}
inline int degreesToCounts(double degrees)
{
    const double c = degrees * 16384.0 / 360.0;
    return static_cast<int>(c < 0 ? c - 0.5 : c + 0.5);
}

// Which parameter is a "write" command with a one-byte status answer? (for the log)
inline std::string describeStatus(std::uint8_t cmd, int status)
{
    (void)cmd;
    return status == 1 ? "ok" : (status == 0 ? "failed" : "status " + std::to_string(status));
}

} // namespace mks
