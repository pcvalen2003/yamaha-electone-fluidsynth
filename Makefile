CXX = g++
# Definimos los flags de includes y de ALSA para RtMidi
CXXFLAGS = -O3 -Wall -std=c++17 -I/usr/include/rtmidi -D__LINUX_ALSA__
LDFLAGS = -lrtmidi -lyaml-cpp -lpthread

TARGET = electone_core
BUILD_DIR = build
SRCS = main.cpp loader.cpp sequencer.cpp

# Genera la lista de objetos: build/main.o, build/loader.o, etc.
OBJS = $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

# Regla principal
all: $(BUILD_DIR) $(TARGET)

# Crear directorio build si no existe
$(BUILD_DIR):
        mkdir -p $(BUILD_DIR)

# Linkeo final
$(TARGET): $(OBJS)
        $(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compilación de cada .cpp a .o en la carpeta build
$(BUILD_DIR)/%.o: %.cpp
        $(CXX) $(CXXFLAGS) -c $< -o $@

# Limpieza
clean:
        rm -rf $(BUILD_DIR) $(TARGET)
