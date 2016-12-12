all: gramc gram

gramc: gramc.cpp graph.cpp graph.h expgraph.cpp expgraph.h export.cpp export.h expander.cpp expander.h parser.cpp parser.h rclist.h bignum.h
	g++ -std=c++11 -O2 -flto -Wall graph.cpp expgraph.cpp expander.cpp export.cpp parser.cpp gramc.cpp -o gramc

gram: gram.cpp interpreter.cpp interpreter.h import.cpp import.h strings.h bignum.h
	g++ -std=c++11 -O2 -flto -Wall interpreter.cpp import.cpp gram.cpp -o gram
