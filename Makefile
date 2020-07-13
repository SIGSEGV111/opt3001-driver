.PHONY: clean all

all: opt3001-csv

opt3001-csv: opt3001-csv.cpp opt3001.cpp opt3001.hpp Makefile
	g++ -Wall -Wextra -flto -O3 -march=native -fdata-sections -ffunction-sections -Wl,--gc-sections opt3001-csv.cpp opt3001.cpp -o opt3001-csv

clean:
	rm -vf opt3001-csv
