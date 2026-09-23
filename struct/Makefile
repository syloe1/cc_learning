# 编译器
CC = gcc

# 编译参数：打开所有警告
CFLAGS = -Wall -std=c11

# 目标文件名
TARGET = struct

# 源文件
SRC = struct.c

# 默认 make 执行的命令
all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# 清理（Linux/macOS 下删 struct，Windows 下顺带删 struct.exe）
clean:
	rm -f $(TARGET) $(TARGET).exe