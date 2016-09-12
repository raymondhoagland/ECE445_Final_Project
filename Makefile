CC = g++
CRYPTO = -lcrypto
FLAGS = -ggdb
OBJS = Border.o

Border.o: gen_border.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o `basename opencvtest.cpp .cpp` gen_border.cpp `pkg-config --libs opencv` $(CRYPTO)

Template.o: gen_template.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o `basename opencvtest.cpp .cpp` gen_template.cpp `pkg-config --libs opencv` $(CRYPTO)
