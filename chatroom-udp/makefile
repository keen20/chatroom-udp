.PHONY:clean all
CC=g++
BIN=chatserver chatclient
FLAG=-Wall -g
all:$(BIN)
%.o:%.cpp
	@echo "compling!"
	$(CC) $(FLAG) -c $< -o $@
	@echo "complile finlished !"
chatserver:chatserver.o
	@echo "linking!"
	$(CC) $(FLAG) $^ -o $@
	@echo "linked finlished !"
chatclient:chatclient.o
	@echo "linking!"
	$(CC) $(FLAG) $^ -o $@
	@echo "linked finlished !"
clean:
	rm -f $(BIN)
	@echo "clean finlished !"

