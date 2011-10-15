includes = p2mpclient.h p2mpserver.h
objects = p2mpclient.o p2mpserver.o p2mputil.o
sources = p2mpclient.c p2mpserver.c 
out = p2mpserver p2mpclient

all: $(objects) client server

$(objects): $(includes)

client: 
	cc -o p2mpclient p2mpclient.o p2mputil.o -lm

server: 
	cc -o p2mpserver p2mpserver.o -lm

.PHONY: clean 
clean:
	rm -f $(out) $(objects)


