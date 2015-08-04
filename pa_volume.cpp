#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <csignal>
#include <functional>
#include <memory>
#include <string>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <thread>
extern "C" {
	#include <pulse/pulseaudio.h>
}
#include <OSC++/OSCAssociativeNamespace.h>
#include <OSC++/OSCProcessor.h>
#include <OSC++/OSCCallable.h>
#include <OSC++/Transmit.h>
#include <OSC++/OSCPrintCallable.h>
#include <OSC++/InetTCPMaster.h>
#include <OSC++/InetUDPMaster.h>
#include <OSC++/InetTransportManager.h>

#ifndef PAV_ASSERT
#define PAV_ASSERT(cond, msg) if (! (cond)) throw std::runtime_error(msg)
#endif
#ifndef DBG_LINE
#define DBG_LINE() do { printf("%s(): line=%d\n", __FUNCTION__, __LINE__); fflush(stdout); } while (0)
#endif
//#define MIN(a, b) ((a) < (b) ? (a) : (b) )
//#define MAX(a, b) ((a) > (b) ? (a) : (b) )
#define ABS(a) ((a) < 0 ? (-(a)) : (a) )
#if 0
#define fader2lin(fader) (pow((fader), 2.5))
#else
#define fader2lin(fader) (fader)
#endif

#define NAME "pa-volume"
#define STD_CHANNELS 0
#define STD_PORT 7600

static volatile int running = 1;
static void signal_handler(int sig) {
	 running = 0;
}

class pa_context {
private:
	std::unique_ptr<pa_mainloop, void (*)(pa_mainloop*)> pa_ml_ptr;
	std::unique_ptr<pa_context, void (*)(pa_context*)> pa_ctx_ptr;

public:
	pa_context()
	: pa_ml_ptr(pa_mainloop_new(), [](pa_mainloop* pa_ml) { if (pa_ml != nullptr) pa_mainloop_free(pa_ml); }),
	  pa_ctx_ptr(pa_context_new(pa_mainloop_get_api(pa_ml_ptr.get()), NAME), [](pa_context* pa_ctx) { if (pa_ctx != nullptr) pa_context_unref(pa_ctx); })
	{
		int err = pa_context_connect(get_ctx(), NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
		PAV_ASSERT(err >= 0, "pa_context_connect() failed");
		PAV_ASSERT(get_mainloop() != nullptr, "mainloop is null");
		PAV_ASSERT(get_ctx() != nullptr, "context is null");

		int pa_ready = 0;

		pa_context_set_state_callback(get_ctx(), pa_state_cb, &pa_ready);
		while(true) {
			if (pa_ready == 0) {
				pa_mainloop_iterate(get_mainloop(), 1, NULL);
				continue;
			}
			PAV_ASSERT(pa_ready != 2, "can not connect to pulseaudio server");
			break;
		}

		pa_operation* op = pa_context_get_server_info(get_ctx(), server_info_cb, nullptr);
		if (op == nullptr) {
			std::cerr << "pa_context_get_server_info failed: " << pa_strerror(pa_context_errno(get_ctx())) << std::endl;
		}
	}

	virtual ~pa_context() {
		pa_context_disconnect(pa_ctx_ptr.get());
	}

	static void server_info_cb(pa_context *c, const pa_server_info* i, void *userdata) {
		std::cout << "server info: server_name=" << i->server_name << " default_sink_name=" << i->default_sink_name << " server_version=" << i->server_version << std::endl;
	}

	static void pa_state_cb(pa_context* c, void* userdata) {
		pa_context_state_t state;
		int* pa_ready = static_cast<int*>(userdata);
	
		state = pa_context_get_state(c);
		switch  (state) {
			// There are just here for reference
			case PA_CONTEXT_UNCONNECTED:
			case PA_CONTEXT_CONNECTING:
			case PA_CONTEXT_AUTHORIZING:
			case PA_CONTEXT_SETTING_NAME:
			default:
				break;
			case PA_CONTEXT_FAILED:
			case PA_CONTEXT_TERMINATED:
				*pa_ready = 2;
				break;
			case PA_CONTEXT_READY:
				*pa_ready = 1;
				break;
		}
	}

	pa_context* get_ctx() {
		return pa_ctx_ptr.get();
	}
	pa_mainloop* get_mainloop() {
		return pa_ml_ptr.get();
	}
};


class pa_volume {
private:
	pa_context ctx;
	float master_volume;
	int32_t index;

	void pa_set_master_volume(float vol) {
		// this pulseaudio api does not seem to work. Try pactl instead.
		std::ostringstream cmd;
		cmd << "pactl set-sink-volume " << index << " " << std::stoi(pa_sw_volume_from_linear(vol)) << "";
		system(cmd.str().c_str());
		return;
		pa_cvolume pa_vol;
		pa_context* pa_ctx = ctx.get_ctx();
		pa_mainloop* pa_ml = ctx.get_mainloop();
		pa_vol.channels = PA_CHANNELS_MAX;
		for (int i=0; i<pa_vol.channels; i++) {
			pa_vol.values[i] = pa_sw_volume_from_linear(vol);
		}
		pa_operation* op = pa_context_set_sink_volume_by_index(pa_ctx, index, &pa_vol, NULL, NULL);
		if (op == nullptr) {
			std::cerr << "Failed set volume: " << pa_strerror(pa_context_errno(pa_ctx)) << std::endl;
		} else {
			pa_operation_unref(op);
		}
	}

public:
	pa_volume(int32_t index)
	: ctx(),
	  master_volume(0.0),
	  index(index)
	{

	}

	virtual ~pa_volume() {

	}

	void master_fader(float gain_fader) {
		if (gain_fader != master_volume) {
			master_volume = gain_fader;
			pa_set_master_volume(master_volume);
		}
	}
	void master_mute(bool mute) {
		if (mute) {
			pa_set_master_volume(0.0);
		} else {
			pa_set_master_volume(master_volume);
		}
	}
	void channel_fader(int32_t index, float gain_fader) {
	}
	void channel_mute(int32_t index, bool mute) {
	}

	const char* get_client_name() {
		return NAME;
	}
};


template<class VOL>
class VolumeCallable : public OSCCallable {
public:
	VolumeCallable(VOL& volume)
	: OSCCallable(),
	  unpack(),
	  volume(volume) {
		unpack.init();
	}
	virtual ~VolumeCallable() {
	}

	void call(const std::string& data, Transmit *const reply) {
		int32_t index = 1;

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
		const char* client_name = volume.get_client_name();
		std::string::size_type pos = address.find(client_name) + strlen(client_name) + 1;
		std::string item = address.substr(pos, sizeof("master")-1);
		//std::cout << "client_name=" << client_name << " pos=" << pos << " item=" << item << std::endl;
		if (0 == item.compare("master")) {
			index = -1;
		} else {
			index = std::stoi(item);
			if (index < 0) {
				index = -1;
			}
		}

		if (address.find("/mute") != address.npos) {
			int32_t mute_val = 0;
			// parse mute
			if (!unpack.unpackInt(&mute_val)) {
				return;
			}
			bool mute = mute_val != 0;
			//std::cout << "mute: channel=" << index << " mute=" << std::string(mute ? "true" : "false") << std::endl;
			// set mute of item
			try {
				if (index < 0) {
					volume.master_mute(mute);
				} else {
					volume.channel_mute(index, mute);
				}
			} catch (std::exception& ex) {
				std::cerr << "exception while setting mute of channel=" << index << " mute=" << mute << ": " << ex.what() << std::endl << std::flush;
			}
		} else {
			float new_gain_lin = 0.0;

			// parse volume
			if (!unpack.unpackFloat(&new_gain_lin)) {
				return;
			}

			//std::cout << "new gain= " << new_gain_lin << std::endl;

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
	}

private:
	OSCUnpacker unpack;
	VOL& volume;
};

static void close_and_die() {
	exit(1);
}

static void usage() {
	std::cerr << "usage: " NAME " [-p <osc_port>] [-n <number_of_channels>] [-i <device_index>]" << std::endl;
	exit(EXIT_FAILURE);
}

static void start_udp_thread(InetUDPMaster* master_udp, InetTransportManager* transport_udp, uint16_t port) {
	try {
		PAV_ASSERT(master_udp->startlisten(port), "failed to listen on udp port " + std::to_string(port) + "\nmaybe port is already in use?");
		std::cout << "start listening on udp port " << port << std::endl;
		while (running) {
			transport_udp->runCycle(1000);
		}
	} catch(std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}

int main(int argc, char** argv) {
	const char* client_name = NULL;
	std::string server_name = "";
	int num_channels = STD_CHANNELS;
	uint16_t port = STD_PORT;
	int32_t index = 0;

	client_name = strrchr(argv[0], '/');
	if (client_name == NULL) {
		client_name = argv[0];
	} else {
		client_name++;
	}
	std::cout << "client_name=" << client_name << std::endl;

	for (int i=1; i<argc; i++) {
		if (i+1 == argc) {
			usage();
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
		if (std::string(argv[i]).compare("-i") == 0) {
			index = std::stoi(argv[i+1]);
			if (index < 0) {
				std::cerr << "index out of range: index=" << index << std::endl;
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

	pa_volume volume(index);


	InetTransportManager transMan;
	InetTransportManager transport_udp;
	OSCAssociativeNamespace nspace;
	OSCProcessor processor;
	InetTCPMaster tcpMaster;
	InetUDPMaster udpMaster;
	VolumeCallable<pa_volume> volume_call(volume);
	nspace.add("/net/mhcloud/volume/" + std::string(client_name) + "/master", &volume_call);
	nspace.add("/net/mhcloud/volume/" + std::string(client_name) + "/master/mute", &volume_call);
	for (int i=0; i<num_channels; i++) {
		nspace.add("/net/mhcloud/volume/" + std::string(client_name) + "/" + std::to_string(i), &volume_call);
		nspace.add("/net/mhcloud/volume/" + std::string(client_name) + "/" + std::to_string(i) + "/mute", &volume_call);
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

	std::thread udp(start_udp_thread, &udpMaster, &transport_udp, port);

	try {
		PAV_ASSERT(tcpMaster.startlisten(port), "failed to listen on tcp port " + std::to_string(port) + "\nmaybe port is already in use?");
		std::cout << "start listening on tcp port " << port << std::endl;
		while (running) {
			transMan.runCycle(1000);
		}
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
		udp.join();
	}

	if (!running) {
		printf("%s: signal received, closing ...\n", NAME);
	}
	exit(EXIT_SUCCESS);
}
