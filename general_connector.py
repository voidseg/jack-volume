#!/usr/bin/env python

import pygtk
pygtk.require('2.0')
import gtk
import getopt
import sys
import liblo
import math
import threading
import time

gtk.gdk.threads_init()

def db_to_coeff(db):
	if db > -318.8:
		return math.pow(10.0, db * 0.05)
	else:
		return 0.0

def coeff_to_db(coeff):
	if coeff == 0.0:
		return float('-inf')
	else:
		return 20.0 * math.log10(coeff)

def fader_to_db(fader):
	if fader == 0.0:
		return float('-inf')
	log = math.log10(fader * 1.3422)
	if fader > 0.745:
		return 78.25 * log
	else:
		return 93.44 * log

def db_to_fader(db):
	if db > -318.8:
		if db > 0.0:
			return 0.745 * math.pow(10.0, db*0.01277955272)
		else:
			return 0.745 * math.pow(10.0, db*0.01070205479)
	else:
		return 0.0

def scroll2str(scroll):
	if scroll == gtk.SCROLL_NONE:
		return "SCROLL_NONE"
	elif scroll == gtk.SCROLL_JUMP:
		return "SCROLL_JUMP"
	elif scroll == gtk.SCROLL_STEP_BACKWARD:
		return "SCROLL_STEP_BACKWARD"
	elif scroll == gtk.SCROLL_STEP_FORWARD:
		return "SCROLL_STEP_FORWARD"
	elif scroll == gtk.SCROLL_PAGE_BACKWARD:
		return "SCROLL_PAGE_BACKWARD"
	elif scroll == gtk.SCROLL_PAGE_FORWARD:
		return "SCROLL_PAGE_FORWARD"
	elif scroll == gtk.SCROLL_STEP_UP:
		return "SCROLL_STEP_UP"
	elif scroll == gtk.SCROLL_STEP_DOWN:
		return "SCROLL_STEP_DOWN"
	elif scroll == gtk.SCROLL_PAGE_UP:
		return "SCROLL_PAGE_UP"
	elif scroll == gtk.SCROLL_PAGE_DOWN:
		return "SCROLL_PAGE_DOWN"
	elif scroll == gtk.SCROLL_STEP_LEFT:
		return "SCROLL_STEP_LEFT"
	elif scroll == gtk.SCROLL_STEP_RIGHT:
		return "SCROLL_STEP_RIGHT"
	elif scroll == gtk.SCROLL_PAGE_LEFT:
		return "SCROLL_PAGE_LEFT"
	elif scroll == gtk.SCROLL_PAGE_RIGHT:
		return "SCROLL_PAGE_RIGHT"
	elif scroll == gtk.SCROLL_START:
		return "SCROLL_START"
	elif scroll == gtk.SCROLL_END:
		return "SCROLL_END"


class VolumeOSCThread(threading.Thread):
	def __init__(self, volumeosc, oscserver):
		threading.Thread.__init__(self)
		self.daemon = True
		self.volumeosc = volumeosc
		self.oscserver = oscserver
	
	def run(self):
		while self.volumeosc.server_active:
			self.oscserver.recv(100)

class VolumeOSC():
	def __init__(self, protocol, instance, host_a, port_a, local_port_a, host_b, port_b, local_port_b):
		self.address_a = liblo.Address(host_a, port_a, protocol)
		self.address_b = liblo.Address(host_b, port_b, protocol)
		self.server_active = False
		try:
			searching_port = True
			ntrials = 0
			while searching_port:
				try:
					self.server_a = liblo.Server(local_port_a)
					searching_port = False
				except liblo.ServerError, err:
					local_port_a = local_port_a + 1
					ntrials = ntrials + 1
					if ntrials > 1000:
						raise liblo.ServerError(99, "stop searching free port", None)
			searching_port = True
			ntrials = 0
			while searching_port:
				try:
					self.server_b = liblo.Server(local_port_b)
					searching_port = False
				except liblo.ServerError, err:
					local_port_b = local_port_b + 1
					ntrials = ntrials + 1
					if ntrials > 1000:
						raise liblo.ServerError(99, "stop searching free port", None)

			self.server_a.add_method("/net/mhcloud/volume/" + instance + "/master", "f", self.callback_master_gain_a)
			self.server_a.add_method("/net/mhcloud/volume/" + instance + "/master/mute", "i", self.callback_master_mute_a)
			self.server_b.add_method("/net/mhcloud/volume/" + instance + "/master", "f", self.callback_master_gain_b)
			self.server_b.add_method("/net/mhcloud/volume/" + instance + "/master/mute", "i", self.callback_master_mute_b)
			for i in range(channels):
				self.server_a.add_method("/net/mhcloud/volume/" + instance + "/" + str(i), "f", self.callback_channel_gain_a, i)
				self.server_a.add_method("/net/mhcloud/volume/" + instance + "/" + str(i) + "/mute", "i", self.callback_channel_mute_a, i)
				self.server_b.add_method("/net/mhcloud/volume/" + instance + "/" + str(i), "f", self.callback_channel_gain_b, i)
				self.server_b.add_method("/net/mhcloud/volume/" + instance + "/" + str(i) + "/mute", "i", self.callback_channel_mute_b, i)
			self.server_active = True
		except liblo.ServerError, err:
			sys.stderr.write("OSC server error occured:\n")
			sys.stderr.write(str(err) + '\n')

		t1 = VolumeOSCThread(self, self.server_a)
		t1.start()
		while self.server_active:
			self.server_b.recv(100)



	def callback_master_gain_a(self, path, args):
		val = float(args[0])
		val = max(0.0, val)
		val = min(1.0, val)
		coeff = db_to_coeff(fader_to_db(val))
		liblo.send(self.address_b, "/net/mhcloud/volume/" + instance + "/master", coeff)

	def callback_master_mute_a(self, path, args):
		mute = int(args[0]!=0)
		liblo.send(self.address_b, "/net/mhcloud/volume/" + instance + "/master/mute", mute)

	def callback_channel_gain_a(self, path, args, types, src, data):
		val = float(args[0])
		val = max(0.0, val)
		val = min(1.0, val)
		coeff = db_to_coeff(fader_to_db(val))
		channel = data
		liblo.send(self.address_b, "/net/mhcloud/volume/" + instance + "/" + str(channel), coeff)

	def callback_channel_mute_a(self, path, args, types, src, data):
		mute = int(args[0]!=0)
		channel = data
		liblo.send(self.address_b, "/net/mhcloud/volume/" + instance + "/" + str(channel) + "/mute", int(mute))


	def callback_master_gain_b(self, path, args):
		val = float(args[0])
		fader = db_to_fader(coeff_to_db(val))
		liblo.send(self.address_a, "/net/mhcloud/volume/" + instance + "/master", fader)

	def callback_master_mute_b(self, path, args):
		mute = int(args[0]!=0)
		liblo.send(self.address_a, "/net/mhcloud/volume/" + instance + "/master/mute", int(mute))

	def callback_channel_gain_b(self, path, args, types, src, data):
		val = float(args[0])
		fader = db_to_fader(coeff_to_db(val))
		channel = data
		liblo.send(self.address_a, "/net/mhcloud/volume/" + instance + "/" + str(channel), fader)

	def callback_channel_mute_b(self, path, args, types, src, data):
		mute = int(args[0]!=0)
		channel = data
		liblo.send(self.address_a, "/net/mhcloud/volume/" + instance + "/" + str(channel) + "/mute", int(mute))



if __name__ == "__main__":
	channels = 2
	protocol = liblo.UDP
	instance = "jack-volume"
	host_a = "motog"
	port_a = 7604
	localport_a = 7603
	host_b = "media"
	port_b = 7601
	localport_b = 7602

	try:
		opts, args = getopt.getopt(sys.argv[1:], "c:p:h:g:j:s:")
	except getopt.GetoptError as err:
		sys.stderr.write(str(err) + '\n')
		sys.stderr.write("usage: " + sys.argv[0] + " [-c <nchannels>] [-p <jv-port>] [-h <host>] [-g <generic-host>] [-j <jack-volume_instance>] [-s <listening-port>]\n")
		sys.stderr.write("options:\n")
		sys.stderr.write("-c  number of channels\n")
		sys.stderr.write("-p  udp port of jack-volume\n")
		sys.stderr.write("-h  jack-volume hostname\n")
		sys.stderr.write("-g  generic device hostname\n")
		sys.stderr.write("-j  name of the jack-volume instance\n")
		sys.stderr.write("-s  OSC UDP listening port\n")
		sys.exit(2)
	for k, v in opts:
		if k == "-c":
			channels = v
		elif k == "-p":
			port_b = int(v)
			localport_b = port_b + 1
		elif k == "-h":
			host_b = v
		elif k == "-g":
			host_a = v
		elif k == "-j":
			instance = v
		elif k == "-s":
			localport_a = int(v)
			port_a = localport_a + 1
	channels = int(channels)
	channels = min(channels, 32)
	channels = max(channels, 1)
	try:
		osc = VolumeOSC(protocol, instance, host_a, port_a, localport_a, host_b, port_b, localport_b)
	except IOError:
		sys.stderr.write("IOError occured:\n")
		sys.stderr.write(str(IOError) + '\n')
	except KeyboardInterrupt:
		sys.stdout.write("program quit\n")
