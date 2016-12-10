all: gramc gram

GENERATOR := generator.cpp generator.h bignum.h stringvector.h
gramtropy: $(GENERATOR) main.cpp
	g++ -O3 -g0 -Wall -std=c++11 generator.cpp main.cpp -lm -o gramtropy

generator.js: $(GENERATOR)
	em++ -O3 -g0 -Wall -std=c++11 generator.cpp -s DISABLE_EXCEPTION_CATCHING=0 -s EXPORTED_FUNCTIONS='["_generator_create","_generator_generate","_generator_destroy","_malloc","_free"]' -o generator.js

gramc: gramc.cpp graph.cpp graph.h expgraph.cpp expgraph.h export.cpp export.h parser.cpp parser.h rclist.h strings.h bignum.h
	g++ -O3 -flto -Wall graph.cpp expgraph.cpp expander.cpp export.cpp parser.cpp gramc.cpp -o gramc

gram: interpreter.cpp interpreter.h import.cpp import.h strings.h bignum.h
	g++ -O3 -flto -Wall interpreter.cpp import.cpp gram.cpp -o gram

gram.dbg: interpreter.cpp interpreter.h import.cpp import.h strings.h bignum.h
	g++ -Og -ggdb3 -Wall interpreter.cpp import.cpp gram.cpp -o gram.dbg
