CXX=g++
CPPFLAGS = -std=c++11 -Wall -W -pedantic
CXXFLAGS += -O1

e: attr.o char32.o printf.o syntax.o editor.o cpuinfo.o
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)


include depfun.mak
