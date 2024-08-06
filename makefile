SERVER = server.c
CLIENT = client.c

LIB_DIR = libs
OBJ_DIR = obj

COMP = gcc
CFLAGS = -Wall -std=gnu99

DEPS = socket.o packet.o protocol.o

all: server client

server: $(DEPS)
	@$(COMP) $(CFLAGS) $(OBJ_DIR)/*.o $(SERVER) -o $@ $(LFLAGS)
	@echo "[SERVER] Successfully built. To run: sudo ./$@ <network interface>"

client: $(DEPS)
	@$(COMP) $(CFLAGS) $(OBJ_DIR)/*.o $(CLIENT) -o $@ $(LFLAGS)
	@echo "[CLIENT] Successfully built. To run: sudo ./$@ <network interface>"

# OBJECTS -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

protocol.o: $(LIB_DIR)/protocol.h $(LIB_DIR)/protocol.c | $(OBJ_DIR)
	@$(COMP) $(CFLAGS) -c $(LIB_DIR)/protocol.c -o $(OBJ_DIR)/$@

socket.o: $(LIB_DIR)/socket.h $(LIB_DIR)/socket.c | $(OBJ_DIR)
	@$(COMP) $(CFLAGS) -c $(LIB_DIR)/socket.c -o $(OBJ_DIR)/$@

packet.o: $(LIB_DIR)/packet.h $(LIB_DIR)/packet.c | $(OBJ_DIR)
	@$(COMP) $(CFLAGS) -c $(LIB_DIR)/packet.c -o $(OBJ_DIR)/$@

$(OBJ_DIR):
	@mkdir -p $@

# CLEANING -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

clean:
	@rm -rf $(OBJ_DIR) server client
	@echo "Successfully erased compiled files."