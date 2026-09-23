# 编译器
CXX = g++

# 编译参数：打开警告，C++11 + 线程支持
CXXFLAGS = -Wall -Wextra -std=c++11 -pthread

# 目标文件名
TARGET = timer

# 源文件（timer.cc 由 main.cpp 直接 include，所以只编一个翻译单元）
SRC = main.cpp

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

# 用 AddressSanitizer 跑一遍，检查内存泄漏和 use-after-free
asan: clean
	$(CXX) $(CXXFLAGS) -fsanitize=address -g -O1 $(SRC) -o $(TARGET)_asan
	./$(TARGET)_asan

# 清理（Windows 下顺带删 .exe）
clean:
	rm -f $(TARGET) $(TARGET).exe $(TARGET)_asan $(TARGET)_asan.exe
