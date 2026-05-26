# A-2.3 dry build — Linux/WSL only (no H100 required)
LIB = lib2adic_shim.so
LIB_EVAL = lib2adic_shim_eval.so
SRC = lib2adic_shim.c shim_identity_v1.c shim_map_epoch.c shim_reserve_v1.c shim_reservation_v1.c shim_granularity_v1.c shim_alloc_v1.c shim_leaf_v1.c shim_pack_v1.c shim_2adic_v1.c shim_gqa_v1.c
CC ?= gcc
HWATOM_SHIM_VERSION ?= 0.2.0
HWATOM_BUILD_ID ?= b_shim_20260526a
CFLAGS ?= -std=c11 -Wall -Wextra -fPIC -O2 -DHWATOM_BUILD_ID=\"$(HWATOM_BUILD_ID)\" -DHWATOM_SHIM_VERSION=\"$(HWATOM_SHIM_VERSION)\"
CFLAGS_EVAL = $(CFLAGS) -DHWATOM_EVAL_SHIM_BUILD=1
LDFLAGS ?= -shared -ldl -lpthread

TEST_SRC = shim_map_epoch.c shim_reservation_v1.c shim_reserve_v1.c shim_alloc_v1.c shim_leaf_v1.c shim_pack_v1.c shim_2adic_v1.c
TEST_BIN = test_reserve_mock_v1
TEST_A25_BIN = test_alloc_mock_v1
TEST_A25_MAIN = ../../tests/shim/test_alloc_mock_v1.c

TEST_A26_BIN = test_leaf_mock_v1
TEST_A26_MAIN = ../../tests/shim/test_leaf_mock_v1.c

TEST_A45B_BIN = test_frag_predicate_v1
TEST_A45B_MAIN = ../../tests/shim/test_frag_predicate_v1.c

TEST_KCAP_BIN = test_pack_kcap_v1
TEST_KCAP_MAIN = ../../tests/shim/test_pack_kcap_v1.c

TEST_2ADIC_BIN = test_2adic_motor_mock_v1
TEST_2ADIC_MAIN = ../../tests/shim/test_2adic_motor_mock_v1.c

.PHONY: all eval clean test-a24 test-a25 test-a26 test-a45b test-kcap test-2adic test-shim

all: $(LIB)

eval: $(LIB_EVAL)

$(LIB): $(SRC) shim_internal.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC)

$(LIB_EVAL): $(SRC) shim_internal.h
	$(CC) $(CFLAGS_EVAL) $(LDFLAGS) -o $@ $(SRC)

test-a24: $(TEST_BIN)
	./$(TEST_BIN)

TEST_MAIN = ../../tests/shim/test_reserve_mock_v1.c

$(TEST_BIN): $(TEST_MAIN) $(TEST_SRC) shim_internal.h
	$(CC) $(CFLAGS) -I. -DHWATOM_SHIM_TEST -o $@ $(TEST_MAIN) $(TEST_SRC) -lpthread

test-a25: $(TEST_A25_BIN)
	./$(TEST_A25_BIN)

$(TEST_A25_BIN): $(TEST_A25_MAIN) $(TEST_SRC) shim_internal.h
	$(CC) $(CFLAGS) -I. -DHWATOM_SHIM_TEST -o $@ $(TEST_A25_MAIN) $(TEST_SRC) -lpthread

test-a26: $(TEST_A26_BIN)
	./$(TEST_A26_BIN)

$(TEST_A26_BIN): $(TEST_A26_MAIN) shim_leaf_v1.c shim_internal.h
	$(CC) $(CFLAGS) -I. -DHWATOM_SHIM_TEST -o $@ $(TEST_A26_MAIN) shim_leaf_v1.c -lpthread

test-a45b: $(TEST_A45B_BIN)
	./$(TEST_A45B_BIN)

$(TEST_A45B_BIN): $(TEST_A45B_MAIN) shim_leaf_v1.c shim_internal.h
	$(CC) $(CFLAGS) -I. -DHWATOM_SHIM_TEST -o $@ $(TEST_A45B_MAIN) shim_leaf_v1.c -lpthread

$(TEST_KCAP_BIN): $(TEST_KCAP_MAIN) $(TEST_SRC) shim_internal.h
	$(CC) $(CFLAGS) -I. -DHWATOM_SHIM_TEST -o $@ $(TEST_KCAP_MAIN) $(TEST_SRC) -lpthread

test-kcap: $(TEST_KCAP_BIN)
	./$(TEST_KCAP_BIN)

$(TEST_2ADIC_BIN): $(TEST_2ADIC_MAIN) shim_2adic_v1.c shim_internal.h
	$(CC) $(CFLAGS) -I. -DHWATOM_SHIM_TEST -o $@ $(TEST_2ADIC_MAIN) shim_2adic_v1.c -lpthread

test-2adic: $(TEST_2ADIC_BIN)
	./$(TEST_2ADIC_BIN)

test-shim: test-a24 test-a25 test-a26 test-a45b test-kcap test-2adic

clean:
	rm -f $(LIB) $(LIB_EVAL) $(TEST_BIN) $(TEST_A25_BIN) $(TEST_A26_BIN) $(TEST_A45B_BIN) $(TEST_KCAP_BIN) $(TEST_2ADIC_BIN)
