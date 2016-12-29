#!/usr/bin/env python2
# -*- encoding: utf8 -*-

# This assembler is very dumb and needs severe improvement.
# Maybe I should rewrite it as an LLVM backend. That would
# be much easier to maintain and far less hackish.

# Either way, expect this code to change, a lot. The bytecode
# format probably won't.

import os
import sys
import re
import struct

in_file  = ""
out_file = ""

def usage():
	print("Usage: " + sys.argv[0] + " <input.pco> <output.vco>")

lines = 0
def syn_err(x):
	print("error - " + str(line) + " - " + x)
	exit(2)

def rel_name(x):
	return {
		'firm'     : "00",

		'section0' : "01",
		'section1' : "02",
		'section2' : "03",
		'section3' : "04",

                'exe'      : '00',
		'exe_text' : "01",
		'exe_data' : "02",
		'exe_ro'   : "03",
	}.get(x, "-1")

name = "NO NAME"
desc = "NO DESC"
title = []
ver = "01"
flags = []
uuid = ""
deps = []
size = 0
offsets = {}

def pad_zero_r(x, c):
	while len(x) < c:
		x = x + bytearray([0])
	return x


def pad_zero_l(x, c):
	while len(x) < c:
		x = bytearray([0]) + x
	return x

def cat_list(list):
	retstr = ""
	for str in list:
		retstr += str + " "
	return retstr

def parse_op(token_list, instr_offs):
	global title
	global desc
	global name
	global ver
	global flags
	global uuid
	global deps
	s = len(token_list) # Get size.
	if s == 0:
		return bytearray() # Empty.

	elif token_list[0][-1:] == ":":
		if instr_offs == None: # Label.
			offsets[token_list[0][:-1]] = size
		return bytearray()

	elif token_list[0] == "#": # Comment.
		if s < 3:
			return bytearray() # Nope.
		elif token_list[1] == "$name": # Meta: name
			name = cat_list(token_list[2:])
		elif token_list[1] == "$desc": # Description
			desc = cat_list(token_list[2:])
		elif token_list[1] == "$title": # Title list
			title = token_list[2:]
		elif token_list[1] == "$ver": # Version
			ver = token_list[2]
		elif token_list[1] == "$uuid": # UUID
			uuid = token_list[2]
		elif token_list[1] == "$flags": # Flags
			flags = token_list[2:]
		elif token_list[1] == "$deps": # Flags
			deps = token_list[2:]

		return bytearray()

	if token_list[0] == "nop": # Nop. Expects 0 args.
		return bytearray.fromhex("00")
	elif token_list[0] == "rel": # Rel. Expects one argument. Possibly requires mapping.
		if s != 2:
			syn_err("expected one argument")

		index = rel_name(token_list[1])
		if index == "-1":
			# TODO - Check if an integer was passed.
			syn_err("invalid argument")

		return bytearray.fromhex("01" + index)
	elif token_list[0] == "find":
		if s != 2:
			syn_err("invalid number of arguments")

		if token_list[1][:1] == "\"": # Quoted string.
			data = bytearray(eval(token_list[1]).encode('utf-8'))
			return bytearray.fromhex("02") + bytearray([len(data)]) + data
		elif token_list[1][:2] == "u\"": # Wide quoted string.
			data = bytearray(eval(token_list[1]).encode('utf-16le'))
			return bytearray.fromhex("02") + bytearray([len(data)]) + data
		else:
			# We cut corners and calculate stuff manually.
			return bytearray.fromhex("02") + bytearray([len(token_list[1]) / 2]) + bytearray.fromhex(token_list[1])
	elif token_list[0] == "back":
		if s != 2:
			syn_err("invalid number of arguments")

		return bytearray.fromhex("03" + token_list[1])
	elif token_list[0] == "fwd":
		if s != 2:
			syn_err("invalid number of arguments")

		return bytearray.fromhex("04" + token_list[1])
	elif token_list[0] == "set":
		if s != 2:
			syn_err("invalid number of arguments")

		if token_list[1][:1] == "\"": # Quoted string.
			data = bytearray(eval(token_list[1]).encode('utf-8'))
			return bytearray.fromhex("05") + bytearray([len(data)]) + data
		elif token_list[1][:2] == "u\"": # Wide quoted string.
			data = bytearray(eval(token_list[1]).encode('utf-16le'))
			return bytearray.fromhex("05") + bytearray([len(data)]) + data
		else:
			# We cut corners and calculate stuff manually.
			return bytearray.fromhex("05") + bytearray([len(token_list[1]) / 2]) + bytearray.fromhex(token_list[1])
	elif token_list[0] == "test":
		if s != 2:
			syn_err("invalid number of arguments")

		if token_list[1][:1] == "\"": # Quoted string.
			data = bytearray(eval(token_list[1]).encode('utf-8'))
			return bytearray.fromhex("06") + bytearray([len(data)]) + data
		elif token_list[1][:2] == "u\"": # Wide quoted string.
			data = bytearray(eval(token_list[1]).encode('utf-16le'))
			return bytearray.fromhex("06") + bytearray([len(data)]) + data
		else:
			# We cut corners and calculate stuff manually.
			return bytearray.fromhex("06") + bytearray([len(token_list[1]) / 2]) + bytearray.fromhex(token_list[1])
	elif token_list[0] == "jmp":
		if s != 2:
			syn_err("invalid number of arguments")

		if instr_offs == None:
			return bytearray.fromhex("070000")
		else:
			val = bytearray(struct.pack(">H", offsets[token_list[1]]))
			val.reverse()
			return bytearray.fromhex("07") + pad_zero_r(val, 2)
	elif token_list[0] == "rewind":
		return bytearray.fromhex("08")
	elif token_list[0] == "and":
		if s != 2:
			syn_err("invalid number of arguments")

		# We cut corners and calculate stuff manually.
		return bytearray.fromhex("09") + bytearray([len(token_list[1]) / 2]) + bytearray.fromhex(token_list[1])
	elif token_list[0] == "or":
		if s != 2:
			syn_err("invalid number of arguments")

		# We cut corners and calculate stuff manually.
		return bytearray.fromhex("0A") + bytearray([len(token_list[1]) / 2]) + bytearray.fromhex(token_list[1])
	elif token_list[0] == "xor":
		if s != 2:
			syn_err("invalid number of arguments")

		# We cut corners and calculate stuff manually.
		return bytearray.fromhex("0B") + bytearray([len(token_list[1]) / 2]) + bytearray.fromhex(token_list[1])
	elif token_list[0] == "not":
		if s != 2:
			syn_err("invalid number of arguments")

		return bytearray.fromhex("0C") + bytearray.fromhex(token_list[1])
	elif token_list[0] == "ver":
		if s != 2:
			syn_err("invalid number of arguments")

		return bytearray.fromhex("0D") + bytearray.fromhex(token_list[1])
	elif token_list[0] == "clf":
		return bytearray.fromhex("0E")
	elif token_list[0] == "seek":
		if s != 2:
			syn_err("invalid number of arguments")

		val = bytearray.fromhex(token_list[1])
		val.reverse()
		return bytearray.fromhex("0F") + val
	elif token_list[0] == "jmpeq":
		if s != 2:
			syn_err("invalid number of arguments")

		if instr_offs == None:
			return bytearray.fromhex("070000")
		else:
			val = bytearray(struct.pack(">H", offsets[token_list[1]]))
			val.reverse()
			return bytearray.fromhex("17") + pad_zero_r(val, 2)
	elif token_list[0] == "jmpne":
		if s != 2:
			syn_err("invalid number of arguments")

		if instr_offs == None:
			return bytearray.fromhex("070000")
		else:
			val = bytearray(struct.pack(">H", offsets[token_list[1]]))
			val.reverse()
			return bytearray.fromhex("27") + pad_zero_r(val, 2)
	elif token_list[0] == "jmplt":
		if s != 2:
			syn_err("invalid number of arguments")

		if instr_offs == None:
			return bytearray.fromhex("070000")
		else:
			val = bytearray(struct.pack(">H", offsets[token_list[1]]))
			val.reverse()
			return bytearray.fromhex("37") + pad_zero_r(val, 2)
	elif token_list[0] == "jmpgt":
		if s != 2:
			syn_err("invalid number of arguments")

		if instr_offs == None:
			return bytearray.fromhex("070000")
		else:
			val = bytearray(struct.pack(">H", offsets[token_list[1]]))
			val.reverse()
			return bytearray.fromhex("47") + pad_zero_r(val, 2)
	elif token_list[0] == "jmple":
		if s != 2:
			syn_err("invalid number of arguments")

		if instr_offs == None:
			return bytearray.fromhex("070000")
		else:
			val = bytearray(struct.pack(">H", offsets[token_list[1]]))
			val.reverse()
			return bytearray.fromhex("57") + pad_zero_r(val, 2)
	elif token_list[0] == "jmpge":
		if s != 2:
			syn_err("invalid number of arguments")

		if instr_offs == None:
			return bytearray.fromhex("070000")
		else:
			val = bytearray(struct.pack(">H", offsets[token_list[1]]))
			val.reverse()
			return bytearray.fromhex("67") + pad_zero_r(val, 2)
	elif token_list[0] == "jmpf":
		if s != 2:
			syn_err("invalid number of arguments")

		if instr_offs == None:
			return bytearray.fromhex("070000")
		else:
			val = bytearray(struct.pack(">H", offsets[token_list[1]]))
			val.reverse()
			return bytearray.fromhex("77") + pad_zero_r(val, 2)
	elif token_list[0] == "jmpnf":
		if s != 2:
			syn_err("invalid number of arguments")

		if instr_offs == None:
			return bytearray.fromhex("070000")
		else:
			val = bytearray(struct.pack(">H", offsets[token_list[1]]))
			val.reverse()
			return bytearray.fromhex("87") + pad_zero_r(val, 2)
	elif token_list[0] == "n3ds": # Sets the eq flag if this is an n3ds.
		return bytearray.fromhex("10")
	elif token_list[0] == "abort":
		return bytearray.fromhex("18")
	elif token_list[0] == "aborteq":
		return bytearray.fromhex("28")
	elif token_list[0] == "abortne":
		return bytearray.fromhex("38")
	elif token_list[0] == "abortlt":
		return bytearray.fromhex("48")
	elif token_list[0] == "abortgt":
		return bytearray.fromhex("58")
	elif token_list[0] == "abortf":
		return bytearray.fromhex("68")
	elif token_list[0] == "abortnf":
		return bytearray.fromhex("78")

def flag_convert(x):
	flags = 0
	for f in x:
		if f == "require":
			flags &= 0x1
		if f == "devmode":
			flags &= 0x2
		if f == "noabort":
			flags &= 0x4
	return pad_zero_r(bytearray([flags]), 4)

try:
	# Read input and output files.
	in_file  = sys.argv[1]
	out_file = sys.argv[2]
except:
	usage()
	exit(1)

with open(in_file, "r") as ins:
	with open(out_file, "wb") as writ:
		bytecode = bytearray()

		# We have to do two passes because of JMP.
		# One to figure out the opcode offsets, one
		# to actually parse everything.

		# FIXME - We need label support ASAP. The AGB and TWL patches I wrote make my head hurt.

		for line in ins:
			lines += 1
			tokens = re.split("\s+", line.strip("\n")) # Split by whitespace.
			bytes = parse_op(tokens, None) # Parse.
			if bytes:
				size += len(bytes)

		ins.seek(0)

		for line in ins:
			lines += 1
			tokens = re.split("\s+", line.strip("\n")) # Split by whitespace.
			bytes = parse_op(tokens, offsets) # Parse.
			if bytes:
				bytecode += bytes

		data  = bytearray("AIDA")
		data += bytearray.fromhex(ver)
		data += pad_zero_r(bytearray(name),         64)
		data += pad_zero_r(bytearray(desc),         256)
		data += pad_zero_r(bytearray.fromhex(uuid), 8)
		data += flag_convert(flags)
		data += struct.pack('I', len(title))
		data += struct.pack('I', len(deps))
		data += struct.pack('I', size)
		if title:
			for f in title:
				tid = bytearray.fromhex(f) # Endianness.
				tid.reverse()
				data += tid
		if deps:
			for f in deps:
				data += pad_zero_r(bytearray.fromhex(f), 8)
		data += bytecode
		writ.write(data)

