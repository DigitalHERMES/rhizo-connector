# Rhizo-connector: A connector to different HF modems
# Copyright (C) 2018 Rhizomatica
# Author: Rafael Diniz <rafael@riseup.net>
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#


PREFIX=/usr
CC=gcc
CFLAGS=-g -Wall -std=gnu11 -pthread

all: rz-hf-connector

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

%.o : %.c %.h
	gcc -c $(CFLAGS) $< -o $@

rz-hf-connector : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

install:
	install rz-hf-connector $(PREFIX)/bin

clean:
	rm -f rz-hf-connector *.o *~
