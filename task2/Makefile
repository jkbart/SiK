CPP = g++
CPPBASIC = -O2 -std=c++23 -Wall -Wextra
CPPEXTRA = -pedantic -Wshadow -Wformat=2 -Wfloat-equal -Wconversion -Wlogical-op -Wshift-overflow=2 -Wduplicated-cond -Wcast-qual -Wcast-align
# fsanitize is bugged on my pc: prints one error line in infinte loop
# CPPOTHER = -fsanitize=address -fsanitize=undefined -fno-sanitize-recover -fstack-protector 
DEBUG = -DDEBUG -g

target: serwer klient
debug: serwer_debug klient_debug

serwer_debug: server.cpp
	$(CPP) $(CPPBASIC) $(CPPEXTRA) $(DEBUG) $< -o kierki-serwer-debug

klient_debug: client.cpp
	$(CPP) $(CPPBASIC) $(CPPEXTRA) $(DEBUG) $< -o kierki-klient-debug

serwer: server.cpp
	$(CPP) $(CPPBASIC) $< -o kierki-serwer

klient: client.cpp
	$(CPP) $(CPPBASIC) $< -o kierki-klient

clean:
	rm -f kierki-serwer kierki-klient kierki-serwer-debug kierki-klient-debug
