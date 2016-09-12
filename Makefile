CC = g++
CRYPTO = -lcrypto
FLAGS = -ggdb -std=c++11

all: template template_show border border_show capture locate locate_show stabilize normalize

template: gen_template.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o template gen_template.cpp `pkg-config --libs opencv` $(CRYPTO)

template_show: gen_template_show.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o template_show gen_template_show.cpp `pkg-config --libs opencv` $(CRYPTO)

border: gen_border.cpp gen_border.hpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o border gen_border.cpp `pkg-config --libs opencv` $(CRYPTO)

border_show: gen_border_show.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o border_show gen_border_show.cpp `pkg-config --libs opencv` $(CRYPTO)

capture: capture_image.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o capture capture_image.cpp `pkg-config --libs opencv` $(CRYPTO)

locate: locate_border.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o locate locate_border.cpp `pkg-config --libs opencv` $(CRYPTO)

locate_show: locate_border_show.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o locate_show locate_border_show.cpp `pkg-config --libs opencv` $(CRYPTO)

stabilize: stable_image.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o stabilize stable_image.cpp `pkg-config --libs opencv` $(CRYPTO)

normalize: normalize_image.cpp
	$(CC) $(FLAGS) `pkg-config --cflags opencv` -o normalize normalize_image.cpp `pkg-config --libs opencv` $(CRYPTO)
