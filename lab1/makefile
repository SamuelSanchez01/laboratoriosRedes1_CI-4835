# Makefile for Lab I
# Usage:
#   make          build every program
#   make client   build only one of them
#   make clean    remove the binaries

# -Wall -Wextra: turn warnings on.  -g: debug symbols for gdb and valgrind.
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

PROGS = resolver client server

all: $(PROGS)

# Pattern rule: "client" is built from "client.cpp".
# socket.hpp is listed so that editing it rebuilds every program.
# $< is the first prerequisite (the .cpp), $@ is the target (the binary).
# IMPORTANT: the recipe line must start with a TAB, not spaces.
%: %.cpp socket.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(PROGS)

.PHONY: all clean
