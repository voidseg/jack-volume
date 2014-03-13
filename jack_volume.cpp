#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <csignal>
#include <string>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <thread>
extern "C" {
	#include <jack/jack.h>
}
#include <OSCAssociativeNamespace.h>
#include <OSCProcessor.h>
#include <OSCCallable.h>
#include <Transmit.h>
#include <OSCPrintCallable.h>
#include <InetTCPMaster.h>
#include <InetUDPMaster.h>
#include <InetTransportManager.h>

#ifndef JV_ASSERT
#define JV_ASSERT(cond, msg) if (! (cond)) throw std::runtime_error(msg)
#endif
#ifndef DBG_LINE
#define DBG_LINE() do { printf("%s(): line=%d\n", __FUNCTION__, __LINE__); fflush(stdout); } while (0)
#endif
#define MIN(a, b) ((a) < (b) ? (a) : (b) )
#define MAX(a, b) ((a) > (b) ? (a) : (b) )
#define ABS(a) ((a) < 0 ? (-(a)) : (a) )
#define fader2lin(fader) (pow((fader), 2.5))

#define NAME "jack-volume"
#define STD_CHANNELS 2
#define STD_PORT 7600

typedef jack_default_audio_sample_t* jack_buffer_t;

static volatile int running = 1;
static void signal_handler(int sig) {
	 running = 0;
}

class jack_client {
public:
	jack_client(const std::string client_name,
			jack_options_t options,
			jack_status_t* status,
			const std::string server_name)
	: client(NULL),
	  client_name(NULL) {
		client = jack_client_open(client_name.c_str(), options, status, server_name.c_str());
		if (client == NULL) {
			if (*status & JackServerFailed) {
				fprintf(stderr, "unable to connect to JACK server\n");
			}
			throw std::runtime_error("jack_client_open() failed");
		}
	}

	virtual ~jack_client() {
		jack_client_close(client);
	}

	int activate() {
		return jack_activate(client);
	}

	void set_process_callback(JackProcessCallback process, void* arg) {
		jack_set_process_callback(client, process, arg);
	}

	void on_shutdown(JackShutdownCallback callback, void* arg) {
		jack_on_shutdown(client, callback, arg);
	}

	jack_port_t* port_register(std::string port_name, std::string port_type, unsigned long flags, unsigned long buffer_size) {
		return jack_port_register(client, port_name.c_str(), port_type.c_str(), flags, buffer_size);
	}

	const char* get_client_name() {
		if (client_name == NULL) {
			client_name = jack_get_client_name(client);
		}
		return client_name;
	}

private:
	jack_client_t* client;
	const char* client_name;
};

class jack_volume : public jack_client {
public:
	jack_volume(int num_channels, const std::string client_name,
			jack_options_t options,
			jack_status_t* status,
			const std::string server_name)
	: jack_client(client_name, options, status, server_name),
	  num_channels(num_channels),
	  ports_out(),
	  ports_in(),
	  m_master_gain(1.0),
   	  channels_gain() {

		ports_out.resize(this->num_channels);
		ports_in.resize(this->num_channels);
		channels_gain.resize(this->num_channels);

		set_process_callback(jack_volume::process, this);
		on_shutdown(jack_volume::shutdown, this);

		for (int i=0; i<this->num_channels; i++) {
			ports_out[i] = port_register("output_" + std::to_string(i+1), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
			JV_ASSERT(ports_out[i] != NULL, "cannot create jack ports");
			ports_in[i] = port_register("input_" + std::to_string(i+1), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
			JV_ASSERT(ports_in[i] != NULL, "cannot create jack ports");
			
			channels_gain[i] = 1.0;
		}
	}
	virtual ~jack_volume() {

	}

	float master_lin() const {
		return m_master_gain;
	}

	void master_lin(float new_master_lin) {
		m_master_gain = new_master_lin;
	}

	void master_fader(float gain_fader) {
		master_lin(fader2lin(gain_fader));
	}

	float channel_lin(int32_t index) const {
		if (index >= 0 && index < (int32_t)channels_gain.size()) {
			return channels_gain[index];
		} else {
			return 0.0;
		}
	}

	void channel_lin(int32_t index, float new_gain_lin) {
		if (index < 0) {
			return;
		}
		JV_ASSERT(!isnan(new_gain_lin), "new_gain_lin is not a number");
		JV_ASSERT(index < (int32_t)channels_gain.size(), "channels_gain: index out of bounds: size=" + std::to_string(channels_gain.size()) + " index=" + std::to_string(index));
		new_gain_lin = MIN(new_gain_lin, 1.0);
		new_gain_lin = MAX(new_gain_lin, 0.0);
		channels_gain[index] = new_gain_lin;
	}

	void channel_fader(int32_t index, float gain_fader) {
		channel_lin(index, fader2lin(gain_fader));
	}

	int channels() const {
		return num_channels;
	}

	jack_port_t* get_port_out(uint32_t index) {
		jack_port_t* ret = NULL;
		if (index<ports_out.size()) {
			ret = ports_out[index];
		}
		return ret;
	}

	jack_port_t* get_port_in(uint32_t index) {
		jack_port_t* ret = NULL;
		if (index<ports_in.size()) {
			ret = ports_in[index];
		}
		return ret;
	}

	static void shutdown(void* arg) {
		exit(EXIT_FAILURE);
	}

	static int process(jack_nframes_t nframes, void* arg) {
		jack_volume* volume = (jack_volume*) arg;
		if (arg == NULL) {
			return -1;
		}

		const int num_channels = volume->channels();

		jack_buffer_t outs[num_channels];
		jack_buffer_t ins[num_channels];

		for (int i=0; i<num_channels; i++) {
			jack_port_t* cur_port = NULL;

			cur_port = volume->get_port_out(i);
			if (cur_port == NULL) {
				return -1;
			}
			outs[i] = (jack_buffer_t)jack_port_get_buffer(cur_port, nframes);
			cur_port = volume->get_port_in(i);
			if (cur_port == NULL) {
				return -1;
			}
			ins[i] = (jack_buffer_t)jack_port_get_buffer(cur_port, nframes);

			for (jack_nframes_t frame=0; frame<nframes; frame++) {
				outs[i][frame] = ins[i][frame] * volume->master_lin() * volume->channel_lin(i);
			}
		}
		return 0;
	}

private:
	int num_channels;
	std::vector<jack_port_t*> ports_out;
	std::vector<jack_port_t*> ports_in;
	float m_master_gain;
	std::vector<float> channels_gain;
};

class VolumeCallable : public OSCCallable {
public:
	VolumeCallable(jack_volume& volume)
	: OSCCallable(),
	  unpack(),
	  volume(volume) {
		unpack.init();
	}
	virtual ~VolumeCallable() {
	}

	void call(const std::string& data, Transmit *const reply) {
		int32_t index = 1;
		float new_gain_lin = 0.0;

		unpack.reset();
		unpack.setData(data);
		//std::cout << data << std::endl << std::flush;

		// unpack OSC address
		std::string address;
		if (!unpack.unpackString(&address)) {
			return;
		}
		// skip tag string
		if (!unpack.skipString()) {
			return;
		}

		// parse item to change
		std::string::size_type pos = address.find(volume.get_client_name());
		std::string item = address.substr(pos+strlen(volume.get_client_name())+1);
		//std::cout << "item=" << item << std::endl;
		if (0 == item.compare("master")) {
			index = -1;
		} else {
			index = std::stoi(item);
			if (index < 0) {
				index = -1;
			}
		}
		// parse volume
		if (!unpack.unpackFloat(&new_gain_lin)) {
			return;
		}

		std::cout << "new gain= " << new_gain_lin << std::endl;

		// set new volume to item
		try {
			if (index < 0) {
				volume.master_fader(new_gain_lin);
			} else {
				volume.channel_fader(index, new_gain_lin);
			}
		} catch (std::exception& ex) {
			std::cerr << "exception while setting volume for channel=" << index << " gain=" << new_gain_lin << ": " << ex.what() << std::endl << std::flush;
		}
	}

private:
	OSCUnpacker unpack;
	jack_volume& volume;
};

static void close_and_die() {
	exit(1);
}

static void usage() {
	std::cerr << "usage: jack-volume [-c <jack_client_name>] [-s <jack_server_name>] [-p <osc_port>] [-n <number_of_channels>]" << std::endl;
	exit(EXIT_FAILURE);
}

static void start_udp_thread(InetUDPMaster* master_udp, InetTransportManager* transport_udp, uint16_t port) {
	master_udp->startlisten(port);
	while (running) {
		transport_udp->runCycle(1000);
	}
}

int main(int argc, char** argv) {
	const char* client_name = NULL;
	std::string server_name = "";
	jack_options_t options = JackNullOption;
	jack_status_t status;
	int num_channels = STD_CHANNELS;
	uint16_t port = STD_PORT;

	client_name = strrchr(argv[0], '/');
	if (client_name == NULL) {
		client_name = argv[0];
	} else {
		client_name++;
	}

	for (int i=1; i<argc; i++) {
		if (i+1 == argc) {
			usage();
		}
		if (std::string(argv[i]).compare("-c") == 0) {
			client_name = argv[i+1];
			i++;
			continue;
		}
		if (std::string(argv[i]).compare("-s") == 0) {
			server_name = argv[i+1];
			i++;
			continue;
		}
		if (std::string(argv[i]).compare("-p") == 0) {
			port = std::stoi(argv[i+1]);
			if (port < 0 || port > ((uint16_t)0xFFFF)) {
				std::cerr << "port out of range: port=" << port << std::endl;
				usage();
			}
			i++;
			continue;
		}
		if (std::string(argv[i]).compare("-n") == 0) {
			num_channels = std::stoi(argv[i+1]);
			if (num_channels < 1) {
				std::cerr << "unvalid number of channels: num_channels=" << num_channels << std::endl;
				usage();
			}
			i++;
			continue;
		}
		usage();
	}

	jack_volume jack_volume(num_channels, std::string(client_name), options, &status, server_name);
	if (status & JackServerStarted) {
		printf("JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_volume.get_client_name();
		printf("unique name assigned: %s\n", client_name);
	}

	InetTransportManager transMan;
	InetTransportManager transport_udp;
	OSCAssociativeNamespace nspace;
	OSCProcessor processor;
	InetTCPMaster tcpMaster;
	InetUDPMaster udpMaster;
	VolumeCallable volume_call(jack_volume);
	nspace.add("/net/mhcloud/volume/" + std::string(client_name) + "/master", &volume_call);
	for (int i=0; i<num_channels; i++) {
		nspace.add("/net/mhcloud/volume/" + std::string(client_name) + "/" + std::to_string(i), &volume_call);
	}
	processor.setNamespace(&nspace);
	tcpMaster.setProcessor(&processor);
	tcpMaster.setTransportManager(&transMan);
	udpMaster.setProcessor(&processor);
	udpMaster.setTransportManager(&transport_udp);

#if 1
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	if (sigemptyset(&sa.sa_mask) < 0) {
		close_and_die();
	}
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		perror("sigaction");
		close_and_die();
	}
	if (sigaction(SIGTERM, &sa, NULL) < 0) {
		perror("sigaction");
		close_and_die();
	}
	if (sigaction(SIGHUP, &sa, NULL) < 0) {
		perror("sigaction");
		close_and_die();
	}
	if (sigaction(SIGQUIT, &sa, NULL) < 0) {
		perror("sigaction");
		close_and_die();
	}
#endif

	if (jack_volume.activate()) {
		fprintf(stderr, "cannot activate client\n");
		close_and_die();
	}

	std::thread udp(start_udp_thread, &udpMaster, &transport_udp, port);

	tcpMaster.startlisten(port);
	while (running) {
		transMan.runCycle(1000);
	}

	printf("%s: signal received, closing ...\n", NAME);
	exit(EXIT_SUCCESS);
}
