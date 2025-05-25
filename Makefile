CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -MMD -I. -Icommon -Iclient -Iserver -Ireactor
LDFLAGS = # Add libraries here if needed, e.g. -lpthread if using threads

# Directories
COMMON_DIR = common
CLIENT_DIR = client
SERVER_DIR = server
REACTOR_DIR = reactor
BUILD_DIR = build
BIN_DIR = .

COMMON_SRCS_CPP = $(wildcard $(COMMON_DIR)/*.cpp)
CLIENT_SRCS_CPP = $(wildcard $(CLIENT_DIR)/*.cpp)
SERVER_SRCS_CPP = $(wildcard $(SERVER_DIR)/*.cpp)
REACTOR_SRCS_CPP = $(wildcard $(REACTOR_DIR)/*.cpp)
LAUNCHER_SRC_CPP  = launcher.cpp

COMMON_OBJS = $(patsubst $(COMMON_DIR)/%.cpp, $(BUILD_DIR)/common_%.o, $(COMMON_SRCS_CPP))
CLIENT_OBJS = $(patsubst $(CLIENT_DIR)/%.cpp, $(BUILD_DIR)/client_%.o, $(CLIENT_SRCS_CPP))
SERVER_OBJS = $(patsubst $(SERVER_DIR)/%.cpp, $(BUILD_DIR)/server_%.o, $(SERVER_SRCS_CPP))
REACTOR_OBJS = $(patsubst $(REACTOR_DIR)/%.cpp, $(BUILD_DIR)/reactor_%.o, $(REACTOR_SRCS_CPP))
LAUNCHER_OBJ = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(LAUNCHER_SRC_CPP))

CLIENT_EXE = $(BIN_DIR)/client_app
SERVER_EXE = $(BIN_DIR)/server_app
LAUNCHER_EXE = $(BIN_DIR)/launcher

all: $(CLIENT_EXE) $(SERVER_EXE) $(LAUNCHER_EXE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/common_%.o: $(COMMON_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(COMMON_DIR) -c $< -o $@

$(BUILD_DIR)/reactor_%.o: $(REACTOR_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(REACTOR_DIR) -c $< -o $@

$(BUILD_DIR)/client_%.o: $(CLIENT_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(COMMON_DIR) -I$(CLIENT_DIR) -c $< -o $@

$(BUILD_DIR)/server_%.o: $(SERVER_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(COMMON_DIR) -I$(SERVER_DIR) -c $< -o $@

$(BUILD_DIR)/launcher.o: launcher.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@


$(CLIENT_EXE): $(CLIENT_OBJS) $(COMMON_OBJS) $(REACTOR_OBJS)	
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(SERVER_EXE): $(SERVER_OBJS) $(COMMON_OBJS) $(REACTOR_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(LAUNCHER_EXE): $(LAUNCHER_OBJ)
	$(CXX) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(CLIENT_EXE) $(SERVER_EXE) $(LAUNCHER_EXE)

.PHONY: all clean

-include $(BUILD_DIR)/*.d