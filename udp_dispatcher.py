#!/usr/bin/env python

import getopt
import sys
import socket
#import liblo
#import math
#import threading
#import time


class Host:
	def __init__(self, ip, port):
		self._ip = ip
		self._port = port

	@property
	def ip(self):
		return self._ip

	@property
	def port(self):
		return self._port

class Dispatcher:
	def __init__(self, port, dst_hosts):
		print "listen port=" + str(port)
		self.port = port
		self.hosts = dst_hosts
		for h in dst_hosts:
			print "dst_host=" + h[0] + " port=" + str(h[1])

	def run(self):
		s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		try:
			s.bind(("localhost", self.port))
		except socket.error, err:
			print "Couldn't bind server on %r" % (self.bind, )
			sys.exit(1)
		while True:
			try:
				(datagram, addr) = s.recvfrom(4096)
			except KeyboardInterrupt:
				break
			if not datagram:
				break
#			print "packet received from: " + str(addr)
			if not addr in hosts:
				hosts.append(addr)
			for h in hosts:
				if h != addr:
					s.sendto(datagram, h)
		s.close()

if __name__ == "__main__":
	port = 7600
	hosts = []

	try:
		opts, args = getopt.getopt(sys.argv[1:], "p:")
	except getopt.GetoptError as err:
		sys.stderr.write(str(err) + '\n')
		sys.stderr.write("usage: " + sys.argv[0] + " [-p <port>]\n")
		sys.exit(2)
	for k, v in opts:
		if k == "-p":
			port = int(v)

	while True:
		try:
			line = sys.stdin.readline()
		except KeyboardInterrupt:
			sys.exit(1)
		if not line:
			break
		tokens = line.split()
		if len(tokens) != 2:
			sys.stderr.write("malformed host line: " + line)
			continue
		try:
			prt = int(tokens[1])
			if prt < 0 or prt >= pow(2, 16):
				raise ValueError("invalid port number %d" % (prt))
		except ValueError, err:
			sys.stderr.write("malformed host line: " + line)
			sys.stderr.write(str(err) + '\n')
			continue
		hosts.append((tokens[0], prt))

	dispatch = Dispatcher(port, hosts)
	dispatch.run()
