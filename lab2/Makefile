# Makefile for Lab II
#   make          build every program
#   make server   build only one of them
#   make clean    remove the binaries

# -pthread: the server uses std::thread
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -pthread

PROGS = server client udp_echo dnsq

all: $(PROGS)

# Every program depends on its .cpp and on the shared headers.
# The recipe line must start with a TAB.
%: %.cpp net.hpp socket.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(PROGS)

.PHONY: all clean
