CPP = g++
CPPBASIC = -O2 -std=c++23# -Wall -Wextra
#CPPEXTRA = -pedantic -Wshadow -Wformat=2 -Wfloat-equal -Wconversion -Wlogical-op -Wshift-overflow=2 -Wduplicated-cond -Wcast-qual -Wcast-align
# fsanitize is bugged on my pc: prints one error line in infinte loop
# CPPOTHER = -fsanitize=address -fsanitize=undefined -fno-sanitize-recover -fstack-protector 
DEBUG = -DDEBUG -g

target: serwer klient
debug: serwer_debug klient_debug
clean: clean

serwer_debug: server.cpp
	$(CPP) $(CPPBASIC) $(CPPEXTRA) $(DEBUG) $< -o serwer

klient_debug: client.cpp
	$(CPP) $(CPPBASIC) $(CPPEXTRA) $(DEBUG) $< -o klient

serwer: server.cpp
	$(CPP) $(CPPBASIC) $< -o $@

klient: client.cpp
	$(CPP) $(CPPBASIC) $< -o $@

clean:
	rm klient serwer
