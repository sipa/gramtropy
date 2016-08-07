GENERATOR := generator.cpp generator.h bignum.h stringvector.h
gramtropy: $(GENERATOR) main.cpp
	g++ -O3 -g0 -Wall -std=c++11 generator.cpp main.cpp -lm -o gramtropy

generator.js: $(GENERATOR)
	em++ -O3 -g0 -Wall -std=c++11 generator.cpp -s DISABLE_EXCEPTION_CATCHING=0 -s EXPORTED_FUNCTIONS='["_generator_create","_generator_generate","_generator_destroy","_malloc","_free"]' -o generator.js
