#include <string>
#include <atomic>
#include <cstdio>
#include <vector>
#include <map>
#include <cstring>
#include <algorithm>

/* Utility that finds a hashing for given words for jsf.hh in my editor */

std::string sprintf(const char* fmt, unsigned value)
{
    char Buf[64];
    std::sprintf(Buf, fmt, value);
    return Buf;
}

int main()
{
    static const char* const words[]{
                   "black","blue","green","cyan","red","yellow","magenta","white",
                   "BLACK","BLUE","GREEN","CYAN","RED","YELLOW","MAGENTA","WHITE",
                   "bg_black","bg_blue","bg_green","bg_cyan","bg_red","bg_yellow","bg_magenta","bg_white",
                   "BG_BLACK","BG_BLUE","BG_GREEN","BG_CYAN","BG_RED","BG_YELLOW","BG_MAGENTA","BG_WHITE",
                   "underline","dim","italic","bold","inverse","blink"};
    std::map<std::string, std::string> actions;
    std::map<std::string, unsigned> action_values;

    for(unsigned n=0; n<8; ++n)
    {
      actions[words[n+0]] = sprintf("fg256=%2d", n);
      actions[words[n+8]] = sprintf("fg256=%2d", n+8);
      actions[words[n+16]] = sprintf("bg256=%2d", n);
      actions[words[n+24]] = sprintf("bg256=%2d", n+8);
      if(n<6)
        actions[words[n+32]] = sprintf("flags |= 0x%02X", 1 << (n&7));

      action_values[words[n+0]] = n;
      action_values[words[n+8]] = n+8;
      action_values[words[n+16]] = n+16;
      action_values[words[n+24]] = n+24;
      if(n<6)
        action_values[words[n+32]] = n+32;
    }

    std::atomic<unsigned> best_dist;
    best_dist = 0xFFFFFFF;

    #pragma omp parallel for schedule(static) collapse(4)
    for(unsigned a=1; a<500; ++a)
    for(unsigned b=0; b<256; ++b)
    for(unsigned mod=40; mod<60; ++mod)
    for(unsigned div=1; div<=32; ++div)
    for(unsigned add=0; add<div; ++add)
    {
      if(0) {fail:continue; }
      std::map<unsigned,const char*> data;
      for(const auto s: words)
      {
        unsigned short c = 0;
        unsigned short l = std::strlen(s);
        unsigned short m=0;
        for(unsigned n=0; n<l; ++n)
        {
            unsigned char i = s[n];
            c += (a*i + m);
            m += b;
        }
        c += add;
        c /= div;
        c %= mod;
        if(!data.try_emplace(c, s).second)
          goto fail;
      }
      unsigned min = 0xFFFFFFF, max = 0;
      for(const auto& d: data) { min = std::min(min, d.first); max = std::max(max, d.first); }
      unsigned dist = max-min+1;
      if(dist < best_dist)
      {
          #pragma omp critical
          {
              if(dist < best_dist)
              {
                  best_dist = dist;
                  printf("//Good: %u %u   distance = %u  mod=%u  div=%u\n", a,b,dist, mod,div);
                  printf("            while(*line && *line != ' ' && *line != '\\t') { c += %uu*(unsigned char)*line + i; i+=%u; ++line; }\n",a,b);
                  printf("            unsigned char code = ((c + %uu) / %uu) %% %uu;\n", add,div,mod);
                  std::vector<int> values(dist);
                  for(auto& v: values) v = -1;
                  for(const auto& d: data) values[d.first - min] = action_values.find(d.second)->second;
                  printf("            static const signed char actions[%u] = { ", dist);
                  const char* sep = "";
                  for(auto u: values) { printf("%s%d", sep, u); sep = ","; }
                  printf("};\n");
                  printf("            if(code >= %u && code <= %u) code = actions[code - %u];\n", min,max,min);
                  printf("            switch(code >> 4) { case 0: fg256 = code&15; break;\n"
                         "                                case 1: bg256 = code&15; break;\n"
                         "                                default:flags |= code&15; }\n");
              }
          }
      }
    }

}

