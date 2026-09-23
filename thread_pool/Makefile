# Makefile for C++11 ThreadPool
CXX      := g++
CXXFLAGS := -std=c++11 -pthread -Wall -Wextra -O2
TARGET   := threadpool

# Optional: ThreadSanitizer build
# CXXFLAGS := -std=c++11 -pthread -fsanitize=thread -g -O1

.PHONY: all clean run tsan

all: $(TARGET)

$(TARGET): main.cpp ThreadPool.h
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

tsan: clean
	$(CXX) -std=c++11 -pthread -fsanitize=thread -g -O1 main.cpp -o $(TARGET)_tsan
	./$(TARGET)_tsan

clean:
	rm -f $(TARGET) $(TARGET)_tsan
