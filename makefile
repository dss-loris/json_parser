CC=gcc

 all: main.c
	 $(CC) main.c jsons_parser.c 				\
	 			JTOK/src/jtok_array.c JTOK/src/jtok_object.c JTOK/src/jtok_primitive.c\
				JTOK/src/jtok_shared.c JTOK/src/jtok_string.c JTOK/src/jtok.c \
	 			-o json_parser.o ;

 clean:
	 $(RM) json_parser.o
