.PHONY: clean
CFLAGS=-O2 -Wall -Wpedantic -Wextra -pthread -gdwarf-4
OBJ=brute.o iter.o rec.o common.o main.o multi.o queue.o single.o gen.o semaphore.o client.o server.o thread_pool.o
TARGET=main
LIBS+=crypt/libcrypt.a
CFLAGS+=-I./crypt

TESTS=test/test.py
WITH_PERF_TEST ?= true

ifeq ($(shell uname), Linux)
	# No valgrind on MacOS
	TESTS+=test/valgrind-test.py
endif

ifeq (${WITH_PERF_TEST}, true)
	TESTS+=test/performance-test.py
endif

all: ${TARGET}

${TARGET}: ${OBJ} ${LIBS}
	@${CC} ${CFLAGS} ${OBJ} ${LIBS} -o ${TARGET}

crypt/libcrypt.a: 
	@${MAKE} -C crypt

clean:
	@${RM} main *.o
	@${MAKE} -C crypt clean

check:
	@pytest ${TESTS}