CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -Iinclude

TARGET = a.out

SOURCES = src/main.cpp \
          src/shell.cpp \
          src/prompt.cpp \
          src/parser.cpp \
          src/builtins.cpp \
          src/executor.cpp \
          src/ls.cpp \
          src/redirection.cpp \
          src/pipeline.cpp \
          src/signals.cpp

OBJECTS = $(SOURCES:.cpp=.o)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)