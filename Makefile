CC := gcc
MPICC := mpicc
CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -O2 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wformat=2
CFLAGS += -Wno-deprecated-declarations
LDLIBS := -lcrypto

COMMON_SOURCES := src/common.c src/file_io.c src/des_crypto.c src/search.c
COMMON_OBJECTS := $(COMMON_SOURCES:src/%.c=build/%.o)
PUBLIC_HEADERS := $(wildcard include/project2/*.h)

.PHONY: all core mpi test test-cli test-mpi test-all demo demo-mpi clean

all: core mpi

core: bin/des_tool bin/bruteforce_seq

mpi: bin/bruteforce_mpi bin/bruteforce_mpi_cyclic bin/bruteforce_mpi_dynamic

bin/des_tool: apps/des_tool.c $(COMMON_OBJECTS) $(PUBLIC_HEADERS) | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) apps/des_tool.c $(COMMON_OBJECTS) -o $@ $(LDLIBS)

bin/bruteforce_seq: apps/bruteforce_seq.c $(COMMON_OBJECTS) $(PUBLIC_HEADERS) | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) apps/bruteforce_seq.c $(COMMON_OBJECTS) -o $@ $(LDLIBS)

bin/bruteforce_mpi: apps/bruteforce_mpi.c $(COMMON_OBJECTS) $(PUBLIC_HEADERS) | bin
	$(MPICC) $(CPPFLAGS) $(CFLAGS) apps/bruteforce_mpi.c $(COMMON_OBJECTS) -o $@ $(LDLIBS)

bin/bruteforce_mpi_cyclic: apps/bruteforce_mpi.c $(COMMON_OBJECTS) $(PUBLIC_HEADERS) | bin
	$(MPICC) $(CPPFLAGS) $(CFLAGS) -DPROJECT2_MPI_CYCLIC \
		apps/bruteforce_mpi.c $(COMMON_OBJECTS) -o $@ $(LDLIBS)

bin/bruteforce_mpi_dynamic: apps/bruteforce_mpi_dynamic.c $(COMMON_OBJECTS) $(PUBLIC_HEADERS) | bin
	$(MPICC) $(CPPFLAGS) $(CFLAGS) apps/bruteforce_mpi_dynamic.c \
		$(COMMON_OBJECTS) -o $@ $(LDLIBS)

bin/test_core: tests/test_core.c $(COMMON_OBJECTS) $(PUBLIC_HEADERS) | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_core.c $(COMMON_OBJECTS) -o $@ $(LDLIBS)

build/%.o: src/%.c $(PUBLIC_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build bin:
	mkdir -p $@

test: bin/test_core
	./bin/test_core

test-cli: core
	bash tests/test_cli.sh

test-mpi: all
	bash tests/test_mpi.sh

test-all: test test-cli test-mpi

demo: core
	./bin/des_tool encrypt --input data/mensaje.txt --output build/mensaje.des --key 42
	./bin/des_tool decrypt --input build/mensaje.des --output build/mensaje.dec.txt --key 42
	cmp data/mensaje.txt build/mensaje.dec.txt
	./bin/bruteforce_seq --input build/mensaje.des --phrase "es una prueba de" --max-key 1000

demo-mpi: all demo
	mpirun -np 4 ./bin/bruteforce_mpi --input build/mensaje.des \
		--phrase "es una prueba de" --max-key 1000 --check-interval 16
	mpirun -np 4 ./bin/bruteforce_mpi_cyclic --input build/mensaje.des \
		--phrase "es una prueba de" --max-key 1000 --check-interval 16
	mpirun -np 4 ./bin/bruteforce_mpi_dynamic --input build/mensaje.des \
		--phrase "es una prueba de" --max-key 1000 --chunk-size 16

clean:
	rm -rf build bin
