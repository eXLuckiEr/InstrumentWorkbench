// Standalone test of the MKS SERVO protocol core (no Qt). Run from the repo root:
//   g++ -std=c++17 -Wall -Wextra -Isdk/include -Iplugins/device/mksservo tools/mkscore_test.cpp -o /tmp/mkscore_test && /tmp/mkscore_test
#include "mkscore.h"
#include <cstdio>
using namespace mks;
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n",__LINE__,#c); return 1;} }while(0)
static bool is(const Frame &f, std::initializer_list<int> bytes) {
    if (f.len != (int)bytes.size()) return false;
    int i = 0; for (int b : bytes) if (f.data[i++] != b) return false; return true;
}
int main(){
    // examples of the manual (data bytes after the ID, with CRC)
    CHECK(is(request(1, code::Encoder), {0x30, 0x31}));
    CHECK(is(request(1, code::WorkMode, {0x05}), {0x82, 0x05, 0x88}));
    CHECK(is(request(1, code::WorkCurrent, {0x0C, 0x80}), {0x83, 0x0C, 0x80, 0x10}));
    CHECK(is(request(1, code::GoHome), {0x91, 0x92}));
    CHECK(is(request(1, code::GoHome, {0x01}), {0x91, 0x01, 0x93}));
    CHECK(is(request(1, code::LimitRemap, {0x00}), {0x9E, 0x00, 0x9F}));
    CHECK(is(request(1, code::SpeedRun, {0x01, 0x40, 0x02}), {0xF6, 0x01, 0x40, 0x02, 0x3A}));
    CHECK(is(request(1, code::SpeedRun, {0x81, 0x40, 0x02}), {0xF6, 0x81, 0x40, 0x02, 0xBA}));
    CHECK(is(request(1, code::SpeedRun, {0x00, 0x00, 0x02}), {0xF6, 0x00, 0x00, 0x02, 0xF9}));
    CHECK(is(request(1, 0x9A, {0x02, 0x01, 0x02, 0x00}), {0x9A, 0x02, 0x01, 0x02, 0x00, 0xA0}));
    CHECK(is(request(1, code::AutoReturn, {0x31, 0x03, 0xE8}), {0x01, 0x31, 0x03, 0xE8, 0x1E}));
    CHECK(is(request(5, code::Encoder), {0x30, 0x35}));
    CHECK(is(request(1, code::Encoder, {}, CrcMode::Fixed6B), {0x30, 0x6B}));
    // homing parameters
    HomeParams hp; hp.speedRpm = 100;
    CHECK(is(request(1, code::HomeParams, homeParamsBytes(hp)), {0x90, 0, 0, 0, 0x64, 0, 0, 0xF5}));
    hp.mode = 1;
    CHECK(is(request(1, code::HomeParams, homeParamsBytes(hp)), {0x90, 0, 0, 0, 0x64, 0, 1, 0xF6}));
    hp.mode = 0; hp.endLimit = 1; hp.trigger = 0;
    CHECK(is(request(1, code::HomeParams, homeParamsBytes(hp)), {0x90, 0, 0, 0, 0x64, 1, 0, 0xF6}));
    HomeOffset ho; ho.offset = 0x2000; ho.currentMa = 600;
    CHECK(is(request(1, code::HomeOffset, homeOffsetBytes(ho)), {0x94, 0, 0, 0x20, 0, 0x02, 0x58, 0x0F}));
    HomeParams back; auto pb = homeParamsBytes(hp);
    CHECK(decodeHomeParams(pb.data(), (int)pb.size(), back) && back.speedRpm==100 && back.endLimit==1);
    HomeOffset hb; auto ob = homeOffsetBytes(ho);
    CHECK(decodeHomeOffset(ob.data(), (int)ob.size(), hb) && hb.offset==0x2000 && hb.currentMa==600);
    // F6 packing
    CHECK(speedRunBytes(0, 320, 2) == (std::vector<std::uint8_t>{0x01, 0x40, 0x02}));
    CHECK(speedRunBytes(1, 320, 2) == (std::vector<std::uint8_t>{0x81, 0x40, 0x02}));
    CHECK(speedRunBytes(0, 100, 5, 50) == (std::vector<std::uint8_t>{0x00, 0x64, 0x05, 0x00, 0x00, 0x32}));
    CHECK(speedRunBytes(0, 9999, 999)[1] == 0xB8);   // clamped to 3000 = 0xBB8

    // answers from the real trace (motor 1: carry -1, value 0x3FFF)
    Frame r; r.id = 1; r.len = 8;
    const std::uint8_t resp[8] = {0x30, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0xFF, 0x6B};
    std::memcpy(r.data, resp, 8);
    std::uint8_t c; const std::uint8_t *p; int n;
    CHECK(parseResponse(r, CrcMode::Sum8, c, p, n) && c == 0x30 && n == 6);
    EncoderValue ev; CHECK(decodeEncoder(p, n, ev) && ev.carry == -1 && ev.value == 0x3FFF && ev.total() == -1);
    r.data[7] = 0x6C;   // broken CRC
    CHECK(!parseResponse(r, CrcMode::Sum8, c, p, n));
    // motor 3: 3FF9 -> -7 counts
    Frame r3; r3.id = 3; r3.len = 8; const std::uint8_t resp3[8] = {0x30, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0xF9, 0x67};
    std::memcpy(r3.data, resp3, 8);
    CHECK(parseResponse(r3, CrcMode::Sum8, c, p, n) && decodeEncoder(p, n, ev) && ev.total() == -7);
    // i48, speed, io, voltage
    const std::uint8_t i48[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0}; std::int64_t v;
    CHECK(decodeI48(i48, 6, v) && v == -16);
    int rpm; const std::uint8_t sp[2] = {0xFF, 0x9C};
    CHECK(decodeSpeed(sp, 2, rpm) && rpm == -100);
    IoState io; const std::uint8_t ios[1] = {0x02};
    CHECK(decodeIo(ios, 1, io) && !io.in1 && io.in2);
    VoltCurrent vc; const std::uint8_t vcb[4] = {0x5D, 0xC0, 0x01, 0xF4};
    CHECK(decodeVoltCurrent(vcb, 4, vc) && vc.millivolts == 24000 && vc.milliamps == 500);
    VersionInfo vi; const std::uint8_t vb[4] = {0x14, 0x01, 0x09, 0x20};
    CHECK(decodeVersion(vb, 4, vi) && vi.calibrated && vi.hardware == 4 && vi.firmware == 0x109 && vi.boot == 0x20);
    CHECK(std::string(hardwareName(4)) == "SERVO57D CAN");
    // wrong frames
    Frame ext = r3; ext.flags |= iwp::can::Extended;
    CHECK(!parseResponse(ext, CrcMode::Sum8, c, p, n));
    printf("ALL OK\n");
    return 0;
}
