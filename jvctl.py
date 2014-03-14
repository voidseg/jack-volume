#!/usr/bin/env python

import pygtk
pygtk.require('2.0')
import gtk
import getopt
import sys
import liblo
import math


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


class VolumeOSC:
	def __init__(self, host, port, protocol, instance):
		self.address = liblo.Address(host, port, protocol)

	def send_master(self, val):
		liblo.send(self.address, "/net/mhcloud/volume/" + instance + "/master", val)

	def send_channel(self, channel, val):
		liblo.send(self.address, "/net/mhcloud/volume/" + instance + "/" + str(channel), val)

class VolumeGUI:

	def delete_event(self, widget, event, data=None):
		print "delete event occurred"
		return False

	def destroy(self, widget, data=None):
		print "destroy signal occurred"
		gtk.main_quit()

	def send_master_osc(self, value):
		self.osc.send_master(value)

	def send_channel_osc(self, channel, value):
		self.osc.send_channel(channel, value)

	def scale_event(self, scale, scroll, value):
		#print "scroll=" + scroll2str(scroll)
		if scroll == gtk.SCROLL_STEP_FORWARD:
			value += 1.0
		elif scroll == gtk.SCROLL_STEP_BACKWARD:
			value -= 1.0
		elif scroll == gtk.SCROLL_PAGE_FORWARD:
			value += 10.0
		elif scroll == gtk.SCROLL_PAGE_BACKWARD:
			value -= 10.0
		value = min(value, 100.0)
		value = max(value, 0.0)

		channel = 0

		for i, val in enumerate(self.vscales):
			if scale == self.vscales[i]:
				#print "scale found"
				channel = i
		
		gain_abs = math.pow(value/100, 2.5)

		if channel == self.nchannels-1:
			self.send_master_osc(gain_abs)
		else:
			self.send_channel_osc(channel, gain_abs)
		scale.set_value(value)
		return True

	def __init__(self, nchannels, host, port, protocol, instance):
		self.osc = VolumeOSC(host, port, protocol, instance)
		self.nchannels = nchannels
		print "channels=" + str(self.nchannels)

		if self.nchannels == 1:
			self.nchannels = 0

		self.nchannels += 1
		self.vscales = [None]*self.nchannels

		self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
		self.window.connect("delete_event", self.delete_event)
		self.window.connect("destroy", self.destroy)
		self.window.set_border_width(10)

		self.table = gtk.Table(2, channels)

		for i, val in enumerate(self.vscales):
			self.vscales[i] = gtk.VScale()
			self.vscales[i].set_digits(1)
			self.vscales[i].set_range(0, 100)
			self.vscales[i].set_value(100.0)
			self.vscales[i].set_inverted(True)
#			self.vscales[i].add_mark(100, gtk.POS_RIGHT, "100")
#			self.vscales[i].add_mark(75, gtk.POS_RIGHT, "75")
#			self.vscales[i].add_mark(50, gtk.POS_RIGHT, "50")
#			self.vscales[i].add_mark(25, gtk.POS_RIGHT, "25")
#			self.vscales[i].add_mark(0, gtk.POS_RIGHT, "0")
			self.vscales[i].connect("change-value", self.scale_event)
			self.vscales[i].show()
			self.table.attach(self.vscales[i], i, i+1, 0, 1)
			if (i != self.nchannels-1):
				self.send_channel_osc(i, 1.0)
				label = gtk.Label("Channel "+str(i+1))
				self.table.attach(label, i, i+1, 1, 2, gtk.FILL, gtk.FILL)
				label.show()
			self.table.attach(self.vscales[i], i, i+1, 0, 1)

		self.send_master_osc(1.0)

		master = gtk.Label("Master")
		self.table.attach(master, self.nchannels-1, self.nchannels, 1, 2, gtk.FILL, gtk.FILL)
		master.show()
		

		self.window.add(self.table)
		self.window.set_size_request(self.nchannels*80, 300)
		self.window.set_position(gtk.WIN_POS_CENTER)
		self.table.show()
		self.window.show()

	def main(self):
		gtk.main()

if __name__ == "__main__":
	channels = 2
	port = 7600
	host = "localhost"
	protocol = liblo.UDP
	instance = "jack-volume"

	try:
		opts, args = getopt.getopt(sys.argv[1:], "c:p:h:j:ut")
	except getopt.GetoptError as err:
		sys.stderr.write(str(err) + '\n')
		sys.stderr.write("usage: " + sys.argv[0] + " [-c <nchannels>] [-p <port>] [-h <host>] [-j <jack-volume_instance>] [-u] [-t]\n")
		sys.stderr.write("options:\n")
		sys.stderr.write("-c  number of channels\n")
		sys.stderr.write("-h  OSC server host\n")
		sys.stderr.write("-p  network port\n")
		sys.stderr.write("-t  send OSC over TCP\n")
		sys.stderr.write("-u  send OSC over UDP\n")
		sys.stderr.write("-j  name of the jack-volume instance\n")
		sys.exit(2)
	for k, v in opts:
		if k == "-c":
			channels = v
		elif k == "-p":
			port = int(v)
		elif k == "-h":
			host = v
		elif k == "-j":
			instance = v
		elif k == "-u":
			protocol = liblo.UDP
		elif k == "-t":
			protocol = liblo.TCP
	channels = int(channels)
	channels = min(channels, 32)
	channels = max(channels, 1)
	try:
		ctrl = VolumeGUI(channels, host, port, protocol, instance)
		ctrl.main()
	except IOError:
		sys.stderr.write("IOError occured:\n")
		sys.stderr.write(str(IOError) + '\n')
