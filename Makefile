SHELL := /bin/bash

.PHONY: all

all:
	gcc -Wall -O2 iheaders.c -o iheaders

debug:
	gcc -Wall -ggdb iheaders.c -o iheaders

install:
	cp ./iheaders /usr/bin/iheaders

uninstall:
	rm /usr/bin/iheaders

clean:
	rm iheaders
