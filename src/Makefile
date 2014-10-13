include ../config.mk

C_FILES		:= *.c

OBJ		:= $(C_FILES:%.c=%.o)

CPPFLAGS	+= -I../include -I../daslib/include

LIB		= ../lib/libtcp_socket.a

default:	lib

lib: $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $(OBJ)

clean:
	-rm -f $(OBJ) $(LIB)
