# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11 -Wall

# Directories
SERVER_DIR = server
CLIENT_DIR = client

# Source files
SERVER_SRCS = $(SERVER_DIR)/packetBuilder.cpp \
              $(SERVER_DIR)/Reliable-Worker.cpp \
              $(SERVER_DIR)/server.cpp

CLIENT_SRCS = $(CLIENT_DIR)/\packetBuilder.cpp \
              $(CLIENT_DIR)/client.cpp

# Output binaries
SERVER_OUT = server
CLIENT_OUT = client

all: server client

server: $(SERVER_SRCS)
    $(CXX) $(CXXFLAGS) $(SERVER_SRCS) -o $(SERVER_OUT)

client: $(CLIENT_SRCS)
    $(CXX) $(CXXFLAGS) $(CLIENT_SRCS) -o $(CLIENT_OUT)

clean:
    rm -f $(SERVER_OUT) $(CLIENT_OUT)

clean_server:
    rm -f $(SERVER_OUT)

clean_client:
    rm -f $(CLIENT_OUT)