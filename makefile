TARGET=$(shell basename `pwd`)
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:%.c=%.o)

CFLAGS+=$(shell pkg-config --cflags sndfile)
LDFLAGS+=$(shell pkg-config --libs sndfile)

all: $(TARGET)

$(OBJECTS): $(SOURCES)

$(TARGET): $(OBJECTS)
	$(CC) -g -o $(TARGET) $(OBJECTS) $(LDFLAGS) -lm

clean:
	$(RM) $(OBJECTS) $(TARGET)

.PHONY: all clean
