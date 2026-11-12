CC ?= gcc
CFLAGS = -O2 -Wall -Wextra -I../../components/detector/include -I../../components/dbn_common/include
all: test_detector test_proto test_click wavtool fake_sender
test_click: test_click.c ../../components/dbn_common/dbn_click.c ../../components/dbn_common/include/dbn_click.h
	$(CC) $(CFLAGS) -o $@ test_click.c ../../components/dbn_common/dbn_click.c
test_detector: test_detector.c ../../components/detector/detector.c ../../components/detector/include/detector.h
	$(CC) $(CFLAGS) -o $@ test_detector.c ../../components/detector/detector.c -lm
test_proto: test_proto.c ../../components/dbn_common/dbn_common.c
	$(CC) $(CFLAGS) -o $@ $^
wavtool: wavtool.c ../../components/detector/detector.c ../../components/detector/include/detector.h
	$(CC) $(CFLAGS) -o $@ wavtool.c ../../components/detector/detector.c -lm
fake_sender: fake_sender.c ../../components/dbn_common/dbn_common.c
	$(CC) $(CFLAGS) -o $@ $^
test: test_detector test_proto test_click fake_sender
	./test_proto && ./test_click && ./test_detector && python3 test_record.py
clean:
	rm -f test_detector test_proto test_click wavtool fake_sender
.PHONY: all test clean
