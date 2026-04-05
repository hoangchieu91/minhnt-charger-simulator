CC = gcc
CFLAGS = -Wall -Wextra -I../Modules/Inc -I../App/Inc

# Paths
HDRS = ../Modules/Inc/relay_ctrl.h
SRCS = test_relay_ctrl.c ../Modules/Src/relay_ctrl.c
OBJS = $(SRCS:.c=.o)
TARGET = test_runner

# DLT645 Tests
HDRS2 = ../Modules/Inc/dlt645_meter.h
SRCS2 = test_dlt645.c ../Modules/Src/dlt645_meter.c
OBJS2 = $(SRCS2:.c=.o)
TARGET2 = test_runner_dlt

# App Machine Tests
HDRS3 = ../App/Inc/app_main.h ../Modules/Inc/relay_ctrl.h ../Modules/Inc/led_rgw.h
SRCS3 = test_app_main.c ../App/Src/app_main.c ../Modules/Src/relay_ctrl.c ../Modules/Src/led_rgw.c
OBJS3 = $(SRCS3:.c=.o)
TARGET3 = test_runner_app

all: $(TARGET) $(TARGET2) $(TARGET3)

$(TARGET): $(OBJS) $(HDRS)
	$(CC) $(OBJS) -o $(TARGET)

$(TARGET2): $(OBJS2) $(HDRS2)
	$(CC) $(OBJS2) -o $(TARGET2)

$(TARGET3): $(OBJS3) $(HDRS3)
	$(CC) $(OBJS3) -o $(TARGET3)

test: $(TARGET)
	./$(TARGET)

test_dlt: $(TARGET2)
	./$(TARGET2)

test_app: $(TARGET3)
	./$(TARGET3)

test_all: test test_dlt test_app

clean:
	rm -f $(OBJS) $(TARGET) $(OBJS2) $(TARGET2) $(OBJS3) $(TARGET3)
