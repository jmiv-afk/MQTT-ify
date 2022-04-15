SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror
endif 

ifeq ($(LDFLAGS),)
	LDFLAGS = -lpaho-mqtt3cs
endif 

TARGET_BUILD?=0
ifeq ($(TARGET_BUILD),1)
	CUSTOMFLAGS = -DTARGET_BUILD
endif

TARGET = mqttify 
all: $(TARGET)
default: $(TARGET)

$(TARGET): $(SRCS) 
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS) $(CUSTOMFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f valgrind*

debug:
	make all
	rm -f valgrind*
	valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-out.txt \
         ./$(TARGET)
