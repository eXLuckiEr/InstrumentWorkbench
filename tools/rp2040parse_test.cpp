// Run from the repo root:
//   g++ -std=c++17 -Wall -Wextra -Iplugins/device/rp2040 tools/rp2040parse_test.cpp -o /tmp/rp2040parse_test && /tmp/rp2040parse_test
#include "rp2040parse.h"
#include <cstdio>
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n",__LINE__,#c); return 1;} }while(0)
int main()
{
    auto i = rp2040::parseIdn("IWP,IWP-RP2040-BASE,E661410403A1B2C3,0.1\r\n");
    CHECK(i.ok && i.manufacturer == "IWP" && i.model == "IWP-RP2040-BASE" && i.serial == "E661410403A1B2C3" && i.firmware == "0.1");
    i = rp2040::parseIdn("IWP, IWP-X ,UID, 1.2,beta");
    CHECK(i.ok && i.model == "IWP-X" && i.serial == "UID" && i.firmware == "1.2,beta");
    CHECK(!rp2040::parseIdn("garbage").ok);
    CHECK(!rp2040::parseIdn("a,b,c").ok);
    CHECK(!rp2040::parseIdn("").ok);
    CHECK(rp2040::unquote("\"E661\"\r\n") == "E661" && rp2040::unquote("plain") == "plain" && rp2040::unquote("\"") == "\"");
    long long a = 0, b = 0;
    CHECK(rp2040::parseTwoNumbers("151232, 148000\r\n", a, b) && a == 151232 && b == 148000);
    CHECK(!rp2040::parseTwoNumbers("123", a, b) && !rp2040::parseTwoNumbers("1,x", a, b) && !rp2040::parseTwoNumbers(",5", a, b));
    // GPIO:MAP?
    auto pins = rp2040::parseGpioMap("\"0,UART0_TX,OUT,1,NONE\",\"1,UART0_RX,IN,1,UP\",\"26,ADC0,IN,0,NONE\"\r\n");
    CHECK(pins.size() == 3);
    CHECK(pins[0].pin == 0 && pins[0].function == "UART0_TX" && pins[0].output && pins[0].level && pins[0].pull == "NONE");
    CHECK(pins[1].pin == 1 && !pins[1].output && pins[1].pull == "UP");
    CHECK(pins[2].function == "ADC0" && !pins[2].level);
    pins = rp2040::parseGpioMap("\"3,SIO,OUT,0,DOWN\", \"bad\", \"x,SIO,IN,0,NONE\",\"4,PWM2_A,OUT,1,BOTH\"");
    CHECK(pins.size() == 2 && pins[0].pin == 3 && pins[1].pin == 4 && pins[1].pull == "BOTH");
    CHECK(rp2040::parseGpioMap("").empty());
    rp2040::GpioPin one;
    CHECK(rp2040::parseGpioLine("16,SPI0_RX,IN,0,NONE", one) && one.pin == 16 && one.function == "SPI0_RX");
    CHECK(rp2040::splitList("\"a,b\",c").size() == 2);
    printf("ALL OK\n");
    return 0;
}
