.PHONY: all dev debug release clean check

CFLAGS=-O2 -Wall -Wextra -gdwarf-4
ifeq ($(shell uname), FreeBSD)
	LIBS+=-lpthread
else
	CFLAGS+=-pthread
endif
CFLAGS+=-I./crypt
OBJ=brute.o iter.o rec.o common.o main.o multi.o queue.o single.o gen.o semaphore.o async_client.o client_common.o sync_client.o async_server.o sync_server.o server_common.o thread_pool.o log.o
TARGET=brute
LIBS+=crypt/libcrypt.a

TESTS=test/simple-test.py test/client-server-test.py
WITH_PERF_TEST ?= false

ifeq ($(shell uname), Linux)
	# No valgrind on MacOS
	TESTS+=test/valgrind-test.py
endif

ifeq (${WITH_PERF_TEST}, true)
	TESTS+=test/performance-test.py
endif

all: ${TARGET}

dev: CFLAGS += -DLOG_LEVEL=TRACE
dev: clean all

debug: CFLAGS += -DLOG_LEVEL=DEBUG
debug: clean all

release: CFLAGS += -DLOG_LEVEL=ERROR
release: clean all

${TARGET}: ${OBJ} ${LIBS}
	${CC} ${CFLAGS} -o ${TARGET} ${OBJ} ${LIBS} 

crypt/libcrypt.a:
	@${MAKE} -C crypt

clean:
	@${RM} ${TARGET} *.o
	@${MAKE} -C crypt clean

check: all
	@LOG_LEVEL=trace pytest --hypothesis-show-statistics ${TESTS}
