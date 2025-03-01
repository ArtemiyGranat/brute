.PHONY: all dev debug release clean check

CFLAGS ?= -O2 -Wall -Wextra -gdwarf-4
ifeq ($(shell uname), FreeBSD)
	LIBS+=-lpthread
else
	CFLAGS+=-pthread
endif
CFLAGS+=-I./crypt

CRYPT_LIB=crypt/libcrypt.a

OBJ_DIR=obj
SRC_DIR=src

OBJ=$(addprefix ${OBJ_DIR}/,brute.o iter.o rec.o common.o main.o multi.o \
	queue.o single.o gen.o semaphore.o async_client.o client_common.o \
	sync_client.o async_server.o sync_server.o server_common.o \
	thread_pool.o log.o)
TARGET=brute

TESTS=test/simple-test.py test/client-server-test.py
PERF_TESTS=test/performance-test.py

ifeq ($(shell uname), Linux)
	# No valgrind on MacOS
	TESTS+=test/valgrind-test.py
endif

all: ${TARGET}

${OBJ_DIR}:
	mkdir -p ${OBJ_DIR}

${TARGET}: ${OBJ} ${CRYPT_LIB}
	${CC} ${CFLAGS} -o ${TARGET} ${OBJ} ${CRYPT_LIB} ${LIBS}

${OBJ_DIR}/%.o: ${SRC_DIR}/%.c | ${OBJ_DIR}
	${CC} ${CFLAGS} -c $< -o $@

dev: CFLAGS += -DLOG_LEVEL=TRACE
dev: clean all

debug: CFLAGS += -DLOG_LEVEL=DEBUG
debug: clean all

release: CFLAGS += -DLOG_LEVEL=ERROR
release: clean all

${CRYPT_LIB}:
	@${MAKE} -C crypt

check: all
	@pytest --hypothesis-show-statistics ${TESTS}

perf: all
	@pytest -rA ${PERF_TESTS}

clean:
	@${RM} ${TARGET}
	@${RM} -r ${OBJ_DIR}
	@${MAKE} -C crypt clean
