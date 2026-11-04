CXX      = g++
CXXFLAGS = -std=c++17 -O2
LDLIBS   = -lglfw -lGL -lm

moscow: moscow.cpp
	$(CXX) moscow.cpp -o moscow $(CXXFLAGS) $(LDLIBS)

clean:
	rm -f moscow

.PHONY: clean
