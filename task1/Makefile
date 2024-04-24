CPP = g++
CPPBASIC = -O3 -pedantic -std=c++20 
CPPWARNINGS = -Wall -Wextra -Wshadow -Wformat=2 -Wfloat-equal -Wconversion -Wlogical-op -Wshift-overflow=2 -Wduplicated-cond -Wcast-qual -Wcast-align
# fsanitize is bugged on my pc: prints one error line in infinte loop
# CPPOTHER = -fsanitize=address -fsanitize=undefined -fno-sanitize-recover -fstack-protector 
DEBUG = -DDEBUG -g

target: ppcbs ppcbc
debug: server client

server: server.cpp
	$(CPP) $(CPPBASIC) $(CPPWARNINGS) $(CPPOTHER) $(DEBUG) $< -o $@

client: client.cpp
	$(CPP) $(CPPBASIC) $(CPPWARNINGS) $(CPPOTHER) $(DEBUG) $< -o $@

ppcbs: server.cpp
	$(CPP) $(CPPBASIC) $< -o $@

ppcbc: client.cpp
	$(CPP) $(CPPBASIC) $< -o $@
