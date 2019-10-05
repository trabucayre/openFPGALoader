EXEC_NAME=cycloader
SRC= $(wildcard *.cpp) 
OBJS= $(SRC:.cpp=.o)
LDFLAGS=-lm -g -Wall -std=c++11 $(shell pkg-config --libs libftdipp1)
CXXFLAGS=-g -Wall -std=c++11 $(shell pkg-config --cflags libftdipp1)

all:$(EXEC_NAME)

$(EXEC_NAME):$(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

install:
	cp -f cycloader /usr/local/bin
	mkdir -p /usr/local/share/cycloader
	cp -f test_sfl.svf /usr/local/share/cycloader

clean:
	rm -rf *.o
	rm -f $(EXEC_NAME)
	rm -f *.c~ *.h~ Makefile~
	rm -f out*.dat
