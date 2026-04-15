# 编译器设置
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11

# 源文件列表
SRC_FILES := clock.cpp config.h generate_data_file.cpp random.cpp table.h

# 目标文件列表
OBJ_FILES := $(patsubst %.cpp, %.o, $(SRC_FILES))

# 生成可执行文件
main: $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $^ -o $@

# 编译所有源文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	rm -f $(OBJ_FILES) main
