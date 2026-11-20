// Standalone test of the plot data structure (no Qt needed). Run from the repo root:
//   g++ -std=c++17 -O2 -Isdk/src tools/plotcore_test.cpp -o /tmp/plotcore_test && /tmp/plotcore_test
#include "plotcore.h"
#include <cstdio>
#include <cstdlib>
#include <random>
#include <chrono>
using namespace iwp::detail;
#define CHECK(c) do{ if(!(c)){ printf("FAIL line %d: %s\n",__LINE__,#c); return 1;} }while(0)
int main(){
    std::mt19937_64 rng(1);
    for (int round=0; round<6; ++round) {
        const bool regular = round%2==0;
        SeriesCore s(5000, regular);   // cap rounds to 8192
        std::vector<float> all;
        double t=0;
        const int total = 60000 + round*777;
        int64_t n=0;
        while (n<total) {
            int b = 1 + rng()%700; std::vector<float> v(b);
            for (auto&x: v){ x = float(std::sin(n*0.01) + (rng()%1000)/500.0 - 1); if (rng()%5000==0) x=NAN; ++n; all.push_back(x);} 
            t += 0.01 + (rng()%100)/10000.0;
            s.append(v.data(), b, t);
        }
        CHECK(s.end()==n); CHECK(s.first()==n-8192);
        // value
        for (int k=0;k<200;++k){ int64_t i = s.first()+rng()%8192; float a=s.value(i), e=all[i]; if(std::isnan(e)) continue; CHECK(a==e);}        
        // minmax vs brute force
        for (int k=0;k<3000;++k){
            int64_t a = s.first()-50 + rng()%8300, b = a + rng()%(k%3==0?9000:300);
            float mn,mx; s.minMax(a,b,&mn,&mx);
            float lo=INFINITY,hi=-INFINITY;
            for (int64_t i=std::max(a,s.first()); i<std::min(b,s.end()); ++i){ float x=s.value(i); lo=std::min(lo,x); hi=std::max(hi,x);}            
            CHECK(mn==lo && mx==hi);
        }
        // time monotonic
        double prev=-1; for (int64_t i=s.first(); i<s.end(); ++i){ double tt=s.time(i); CHECK(tt>=prev); prev=tt; }
        // indexAtTime == brute force
        for (int k=0;k<500;++k){
            double tt = s.time(s.first()) - 0.05 + (s.time(s.end()-1)-s.time(s.first())+0.1)*(rng()%10000)/10000.0;
            int64_t got = s.indexAtTime(tt), want = s.end();
            for (int64_t i=s.first(); i<s.end(); ++i) if (s.time(i)>=tt){ want=i; break; }
            if (got!=want) { printf("round %d t=%f got %lld want %lld\n",round,tt,(long long)got,(long long)want); return 1; }
        }
        // indexAtColumns == per-column indexAtTime (also for ranges that start before/after the data)
        for (int k=0;k<20;++k){
            const int n = 1 + int(rng()%700);
            double t0 = s.time(s.first()) - 1.0 + double(rng()%3000)/1000.0, step = double(rng()%2000+1)/1000.0/n;
            std::vector<int64_t> out(size_t(n)+1);
            double tl = t0 + step*n + 1e-9;
            s.indexAtColumns(t0, step, n, tl, out.data());
            for (int c=0;c<=n;++c){
                int64_t want = s.indexAtTime(c<n ? t0+step*c : tl);
                if (out[size_t(c)]!=want){ printf("cols round %d c=%d got %lld want %lld\n",round,c,(long long)out[size_t(c)],(long long)want); return 1; }
            }
        }
    }
    // timing quality: regular stream 400 Hz in jittery batches -> sample spacing stays ~2.5 ms
    { SeriesCore s(1<<16,true); double t=0; int64_t n=0; std::mt19937 r(2); double sentT=0;
      for (int b=0;b<400;++b){ sentT+=0.0333; int cnt = 13; t = sentT + (int(r()%100)-50)/5000.0; // +-10ms jitter
        std::vector<float> v(cnt,1.f); s.append(v.data(),cnt,t); n+=cnt; }
      double maxd=0,mind=1; for(int64_t i=s.first()+200;i<s.end();++i){ double d=s.time(i)-s.time(i-1); maxd=std::max(maxd,d); mind=std::min(mind,d);} 
      printf("spacing min %.5f max %.5f (true %.5f)\n",mind,maxd,0.0333/13); }
    // benchmark
    { SeriesCore s(1<<22,true); std::vector<float> v(3333); for(size_t i=0;i<v.size();++i) v[i]=float(i%100);
      auto t0=std::chrono::steady_clock::now(); double tt=0;
      for(int b=0;b<600;++b){ tt+=0.0333; s.append(v.data(),(int)v.size(),tt);} // 2M samples = 20 s at 100 kHz
      auto t1=std::chrono::steady_clock::now();
      printf("append 2M samples: %.1f ms\n", std::chrono::duration<double,std::milli>(t1-t0).count());
      // frame: 1500 columns over last 1.5M samples
      auto t2=std::chrono::steady_clock::now(); float mn,mx; double acc=0;
      for (int rep=0;rep<30;++rep){ int64_t e=s.end()-rep*7; int64_t W=1500000; for(int c=0;c<1500;++c){ int64_t a=e-W+c*W/1500, b=e-W+(c+1)*W/1500; s.minMax(a-1,b,&mn,&mx); acc+=mx-mn; } }
      auto t3=std::chrono::steady_clock::now();
      printf("30 frames x 1500 columns: %.2f ms per frame (acc %.0f)\n", std::chrono::duration<double,std::milli>(t3-t2).count()/30, acc); }
    puts("ALL OK"); return 0; }
