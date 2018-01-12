CXX=i586-pc-msdosdjgpp-g++
CXXFLAGS=-Ofast -g -flto
CPPFLAGS=-std=gnu++17 -Wall -Wextra

e.exe: main.cc jsf.hh mario.hh vga.hh vec_l.hh vec_lp.hh vec_c.hh vec_cp.hh vec_s.hh vec_sp.hh cpu.h
	$(CXX) -o $@ $< $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)
