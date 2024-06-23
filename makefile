SERVER = server.c
CLIENT = client.c

LIB_DIR = libs
OBJ_DIR = obj

COMP = gcc
CFLAGS = -Wall -std=gnu99

DEPS = lib.o

all: server client

server: $(DEPS)
	@$(COMP) $(CFLAGS) $(OBJ_DIR)/*.o $(SERVER) -o $@ $(LFLAGS)
	@echo "[SERVER] Successfully built. To run: ./$@"

client: $(DEPS)
	@$(COMP) $(CFLAGS) $(OBJ_DIR)/*.o $(CLIENT) -o $@ $(LFLAGS)
	@echo "[CLIENT] Successfully built. To run: ./$@"

# OBJECTS -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

lib.o: $(LIB_DIR)/*.h $(LIB_DIR)/*.c | $(OBJ_DIR)
	@$(COMP) $(CFLAGS) -c $(LIB_DIR)/*.c -o $(OBJ_DIR)/$@

$(OBJ_DIR):
	@mkdir -p $@

# CLEANING -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

clean:
	@rm -rf $(OBJ_DIR) server client
	@echo "Successfully erased compiled files."