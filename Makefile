# Переменные для компилятора и флагов
CXX = g++
CXXFLAGS = -std=c++17 -O2
LIBS = -lX11 -lGL -lm
TARGET = moscow

# Команда по умолчанию (просто 'make') скомпилирует бинарник
all:
	$(CXX) moscow.cpp -o $(TARGET) $(CXXFLAGS) $(LIBS)

# Команда очистки ('make clean') удалит скомпилированный файл
clean:
	rm -f $(TARGET)
