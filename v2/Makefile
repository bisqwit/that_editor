CXX=g++ -std=c++11

#CXX=i586-pc-msdosdjgpp-g++ -D_GLIBCXX_USE_C99_STDINT_TR1 -std=gnu++11
# DJGPP requires -D_GLIBCXX_USE_C99_STDINT_TR1 -std=gnu++11
# Otherwise e.g. uint_least32_t, strcasecmp won't compile.

#CXX=i686-w64-mingw32-g++ -std=c++0x

CPPFLAGS = -Wall -W -pedantic
CXXFLAGS += -O0 -g

e: attr.o char32.o printf.o syntax.o editor.o cpuinfo.o
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)



test: test.o syntax.o attr.o
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)


include depfun.mak
