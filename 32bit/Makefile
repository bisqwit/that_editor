CXX=i586-pc-msdosdjgpp-g++
CXXFLAGS=-Ofast -g -flto
CPPFLAGS=-std=gnu++17 -Wall -Wextra

INCLUDES=jsf.hh mario.hh vga.hh vec_l.hh vec_lp.hh vec_c.hh vec_cp.hh vec_s.hh vec_sp.hh cpu.h kbhit.hh vecbase.hh

OBJS=main.o mario.o vga.o kbhit.o

#CXXFLAGS += -fsanitize=address

e.exe: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS)

$(OBJS):  $(INCLUDES)


# To install DJGPP on Debian:
#    From http://ap1.pp.fi/djgpp/gcc/
#    Or from a djgpp mirror,
#       Download:
#
# http://mirrors.fe.up.pt/pub/djgpp/rpms/djcrx-2.05-5.x86_64.rpm
# http://mirrors.fe.up.pt/pub/djgpp/rpms/djcross-binutils-2.29.1-1ap.x86_64.rpm
# http://mirrors.fe.up.pt/pub/djgpp/rpms/djcross-gcc-7.2.0/x86_64/djcross-gcc-7.2.0-1ap.x86_64.rpm
# http://mirrors.fe.up.pt/pub/djgpp/rpms/djcross-gcc-7.2.0/x86_64/djcross-gcc-c++-7.2.0-1ap.x86_64.rpm
# http://mirrors.fe.up.pt/pub/djgpp/rpms/djcross-gcc-7.2.0/x86_64/djcross-gcc-info-7.2.0-1ap.x86_64.rpm
# http://mirrors.fe.up.pt/pub/djgpp/rpms/djcross-gcc-7.2.0/x86_64/djcross-gcc-tools-7.2.0-1ap.x86_64.rpm
#
#    Then apt-get install alien, and use alien to convert these into .deb packages.
#    Then just dpkg -i each of them.
#    Done!
#
#    You can get HDPMI32 from https://sourceforge.net/projects/hx-dos/files/
#    Such as: https://sourceforge.net/projects/hx-dos/files/2.17/hxrt217.7z/download
#
