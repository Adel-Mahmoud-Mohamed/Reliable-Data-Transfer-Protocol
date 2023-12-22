# Compiler
CXX = g++

# Source files
SERVER_SRCS = packetBuilder.cpp \
              Reliable-Worker.cpp \
              server.cpp

# Output binaries
SERVER_OUT = my_server

server: $(SERVER_SRCS)
	$(CXX) $(SERVER_SRCS) -o $(SERVER_OUT)

clean:
	rm -f $(SERVER_OUT)
