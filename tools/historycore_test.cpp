// Standalone test of the console command history (no Qt). Run from the repo root:
//   g++ -std=c++17 -Wall -Iplugins/device/console tools/historycore_test.cpp -o /tmp/historycore_test && /tmp/historycore_test
#include "historycore.h"
#include <cstdio>
using namespace cmdhist;
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n",__LINE__,#c); return 1;} }while(0)
int main(){
    // normalization = what is sent
    CHECK(HistoryCore::normalize("  *IDN?  \r\n\r\n  :VOLT 5\r\n") == "*IDN?\n:VOLT 5");
    CHECK(HistoryCore::normalize(" \n\t\r\n").empty());
    CHECK(HistoryCore::lines("a\n\nb\r\n c ").size() == 3);
    CHECK(HistoryCore::lines("a\n\nb\r\n c ")[2] == "c");

    HistoryCore h(3);
    CHECK(!h.add("   \n"));
    CHECK(h.add("a")); CHECK(h.add("b")); CHECK(h.add("c"));
    CHECK(h.ordered()[0].text=="c" && h.ordered()[2].text=="a");
    // repeat moves to front, no duplicates, whitespace-insensitive
    CHECK(h.add(" a \r\n")); CHECK(h.ordered().size()==3 && h.ordered()[0].text=="a");
    // limit: the oldest goes
    CHECK(h.add("d")); CHECK(h.ordered().size()==3 && !h.contains("b"));
    // pinned: on top, not pushed out
    CHECK(h.setPinned("c", true)); CHECK(h.isPinned("c")); CHECK(h.ordered()[0].text=="c");
    CHECK(h.add("e")); CHECK(h.add("f")); CHECK(h.add("g"));
    CHECK(h.contains("c"));                       // survived 3 newer ones
    CHECK(h.ordered().size()==4);                  // 1 pinned + 3 newest
    CHECK(h.ordered()[1].text=="g");
    // re-sending a pinned entry keeps it pinned
    CHECK(h.add("c")); CHECK(h.isPinned("c"));
    // pinning an unknown text adds it
    CHECK(h.setPinned("x\ny", true)); CHECK(h.contains("x\ny") && h.isPinned("x\ny"));
    // multi-line entry identity
    CHECK(h.add("x\r\n\r\ny")); CHECK(h.ordered().size()==5);
    // unpin, remove
    CHECK(h.setPinned("x\ny", false)); CHECK(!h.isPinned("x\ny"));
    CHECK(h.remove("x\ny")); CHECK(!h.remove("x\ny"));
    // clear keeps pinned
    h.clear(true); CHECK(h.ordered().size()==1 && h.ordered()[0].text=="c");
    h.clear(false); CHECK(h.ordered().empty());
    // lowering the limit trims; assign() sanitizes (dupes, blanks)
    HistoryCore g(2);
    g.assign({{"1",false},{" 1 ",false},{"",false},{"2",true},{"3",false},{"4",false}});
    CHECK(g.ordered().size()==3);                  // 2 (pinned) + two newest unpinned: 1, 3
    CHECK(g.ordered()[0].text=="2" && g.ordered()[1].text=="1" && g.ordered()[2].text=="3");
    g.setLimit(1); CHECK(g.ordered().size()==2);
    printf("ALL OK\n");
    return 0;
}
