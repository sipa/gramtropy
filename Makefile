all: gramc gram

CXX=g++

gramc: src/gramc.cpp src/graph.cpp src/graph.h src/expgraph.cpp src/expgraph.h src/export.cpp src/export.h src/expander.cpp src/expander.h src/parser.cpp src/parser.h src/rclist.h src/bignum.h
	$(CXX) -std=c++11 -flto -O2 -Wall src/graph.cpp src/expgraph.cpp src/expander.cpp src/export.cpp src/parser.cpp src/gramc.cpp -o gramc

gram: src/gram.cpp src/interpreter.cpp src/interpreter.h src/import.cpp src/import.h src/strings.h src/bignum.h
	$(CXX) -std=c++11 -flto -std=c++11 -O2 -Wall src/interpreter.cpp src/import.cpp src/gram.cpp -o gram

clean:
	rm -f gram gramc
