CPP = g++
CPPBASIC = -O2 -pedantic -std=c++20 
CPPWARNINGS = -Wall -Wextra -Wshadow -Wformat=2 -Wfloat-equal -Wconversion -Wlogical-op -Wshift-overflow=2 -Wduplicated-cond -Wcast-qual -Wcast-align
# CPPOTHER = -fsanitize=address -fsanitize=undefined -fno-sanitize-recover -fstack-protector

%: %.cpp
	$(CPP) $(CPPBASIC) $(CPPWARNINGS) $(CPPOTHER) $< -o $@