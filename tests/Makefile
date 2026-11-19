CFLAGS = -Wall -Wextra -Werror -I../Core/Inc

.PHONY: all clean

all: test_crsf test_teleop
	./test_crsf
	./test_teleop

test_crsf: test_crsf.c ../Core/Src/crsf.c ../Core/Inc/crsf.h
	gcc $(CFLAGS) -o $@ test_crsf.c ../Core/Src/crsf.c

test_teleop: test_teleop.c ../Core/Src/teleop.c ../Core/Inc/teleop.h
	gcc $(CFLAGS) -o $@ test_teleop.c ../Core/Src/teleop.c

clean:
	rm -f test_crsf test_teleop
