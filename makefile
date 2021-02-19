CC=gcc

 all: main.c
	 $(CC) main.c jsons.c JTOK/src/jtok_array.c JTOK/src/jtok_object.c
	 				JTOK/src/jtoke_primitive.c JTOK/src/jtok_shared.c
						JTOK/src/jtok_string.c JTOK/src/jtok_string.c JTOK/src/jtoc.c
	 						-o json_parser.o

 clean:
	 $(RM) json_parser.o
