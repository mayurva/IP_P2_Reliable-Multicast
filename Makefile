includes = p2mpclient.h p2mpserver.h
objects = p2mpclient.o p2mpserver.o clientutil.o serverutil.o
sources = p2mpclient.c p2mpserver.c clientutil.c serverutil.c
out = p2mpserver p2mpclient

all: $(objects) client server

$(objects): $(includes)

client: 
	cc -o p2mpclient p2mpclient.o clientutil.o -lm -lpthread

server: 
	cc -o p2mpserver p2mpserver.o serverutil.o -lm -lpthread

.PHONY: clean 
clean:
	rm -f $(out) $(objects)
