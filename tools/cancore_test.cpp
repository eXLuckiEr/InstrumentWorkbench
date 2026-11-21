// Standalone test of the CAN core (no Qt). Run from the repo root:
//   g++ -std=c++17 -Wall -Isdk/include -Iplugins/endpoint/canslcan tools/cancore_test.cpp -o /tmp/cancore_test && /tmp/cancore_test
#include "iwp/cancore.h"
#include "slcancodec.h"
#include <cstdio>
using namespace iwp::can;
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n",__LINE__,#c); return 1;} }while(0)
int main(){
    // DLC
    CHECK(dlcToLen(8)==8 && dlcToLen(9)==12 && dlcToLen(15)==64 && dlcToLen(16)==-1);
    CHECK(lenToDlc(8,false)==8 && lenToDlc(9,false)==-1 && lenToDlc(12,true)==9 && lenToDlc(64,true)==15 && lenToDlc(13,true)==-1);

    // hex bytes
    std::uint8_t b[64]; int n; std::string err;
    CHECK(parseHexBytes("DE AD be ef",b,64,n,err) && n==4 && b[0]==0xDE && b[3]==0xEF);
    CHECK(parseHexBytes("DEADBEEF",b,64,n,err) && n==4);
    CHECK(parseHexBytes("0x01,0x02 3",b,64,n,err) && n==3 && b[2]==3);
    CHECK(parseHexBytes("",b,64,n,err) && n==0);
    CHECK(!parseHexBytes("ABC",b,64,n,err));
    CHECK(!parseHexBytes("ZZ",b,64,n,err));
    CHECK(!parseHexBytes("00 11 22",b,2,n,err));

    // text frames
    Frame f;
    CHECK(parseFrame("123#DEADBEEF",f,err) && f.id==0x123 && !f.extended() && f.len==4 && f.data[0]==0xDE);
    CHECK(formatFrame(f)=="123#DEADBEEF");
    CHECK(parseFrame("18FF0001#01 02",f,err) && f.extended() && f.id==0x18FF0001 && f.len==2);
    CHECK(formatFrame(f)=="18FF0001#0102");
    CHECK(parseFrame("001#30",f,err) && f.id==1 && f.len==1 && f.data[0]==0x30);
    CHECK(parseFrame("7E0#R",f,err) && f.rtr() && f.len==0);
    CHECK(parseFrame("7E0#R4",f,err) && f.rtr() && f.len==4 && formatFrame(f)=="7E0#R4");
    CHECK(parseFrame("123##1"+std::string("00112233445566778899AABB"),f,err) && f.fd() && f.has(Brs) && !f.has(Esi) && f.len==12);
    CHECK(formatFrame(f)=="123##100112233445566778899AABB");
    CHECK(parseFrame("123##0",f,err) && f.fd() && f.len==0);
    CHECK(!parseFrame("800#00",f,err));
    CHECK(!parseFrame("123#001122334455667788",f,err));          // 9 bytes classic
    CHECK(!parseFrame("123##100112233445566778899",f,err));      // 10 bytes FD
    CHECK(!parseFrame("123",f,err));
    CHECK(!parseFrame("XYZ#00",f,err));
    CHECK(!parseFrame("123##9",f,err));
    f = Frame(); f.id=0x10; f.len=2; f.data[0]=1; f.data[1]=2; f.tsUs=12345678;
    CHECK(formatLogLine(f,"can0")=="(12.345678) can0 010#0102");

    // sequences
    std::vector<SequenceItem> seq; int line;
    CHECK(parseSequence("# comment\n001#30\n\nwait 100\n  001#31 \n// x\nsleep 5\n",seq,err,line) && seq.size()==4);
    CHECK(!seq[0].isDelay && seq[1].isDelay && seq[1].delayMs==100 && seq[3].delayMs==5);
    CHECK(!parseSequence("001#30\nbad\n",seq,err,line) && line==2);
    CHECK(!parseSequence("wait x\n",seq,err,line) && line==1);

    // filter
    IdFilter flt;
    CHECK(flt.parse("",err) && flt.empty() && flt.accepts(5));
    CHECK(flt.parse("123, 100-10F, 7E0:7F0",err));
    CHECK(flt.accepts(0x123) && flt.accepts(0x105) && !flt.accepts(0x110) && flt.accepts(0x7E5) && !flt.accepts(0x7D0));
    CHECK(flt.parse("!7FF",err) && flt.accepts(1) && !flt.accepts(0x7FF));
    CHECK(flt.parse("100-1FF;!150",err) && flt.accepts(0x120) && !flt.accepts(0x150) && !flt.accepts(0x200));
    CHECK(!flt.parse("1G",err));
    CHECK(!flt.parse("20-10",err));
    CHECK(flt.accepts(0x120));   // a failed parse keeps the previous rules

    // slcan
    using namespace iwp::can::slcan;
    parseFrame("123#DEADBEEF",f,err);
    CHECK(encode(f)=="t1234DEADBEEF\r");
    Frame g; CHECK(decode("t1234DEADBEEF",g) && g.id==0x123 && g.len==4 && g.data[3]==0xEF && !g.extended());
    parseFrame("18FF0001#0102",f,err);
    CHECK(encode(f)=="T18FF000120102\r");
    CHECK(decode("T18FF000120102",g) && g.extended() && g.id==0x18FF0001 && g.len==2);
    parseFrame("7E0#R4",f,err);
    CHECK(encode(f)=="r7E04\r");
    CHECK(decode("r7E04",g) && g.rtr() && g.len==4);
    parseFrame("123##100112233445566778899AABB",f,err);
    CHECK(encode(f)=="b123900112233445566778899AABB\r");
    CHECK(decode("b123900112233445566778899AABB",g) && g.fd() && g.has(Brs) && g.len==12);
    parseFrame("123##0"+std::string("0011"),f,err);
    CHECK(encode(f)=="d12320011\r");
    CHECK(decode("d12320011",g) && g.fd() && !g.has(Brs) && g.len==2);
    CHECK(!decode("",g));
    CHECK(!decode("z",g));
    CHECK(!decode("t1234DEAD",g));            // data too short
    CHECK(!decode("t1239",g));                // dlc 9 on a classic frame
    CHECK(!decode("tG004",g));
    CHECK(decode("t1231AAFFFF",g) && g.len==1);   // trailing timestamp ignored
    CHECK(nominalCode(500000)=='6' && nominalCode(1000000)=='8' && nominalCode(12345)=='\0');
    CHECK(dataCode(2000000)=='2' && dataCode(3)=='\0');

    // bus time: 8-byte standard frame at 500k ~ 111 bits = 222 us
    Frame h; h.len=8;
    double t = busTimeSeconds(h,500000,0);
    CHECK(t>0.00021 && t<0.00023);
    printf("ALL OK\n");
    return 0;
}
