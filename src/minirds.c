/*
 * mpxgen - FM multiplex encoder with Stereo and RDS
 * Copyright (C) 2019 Anthony96922
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include <signal.h>
#include <getopt.h>
#include <pthread.h>
#include <ao/ao.h>

#include "rds.h"
#include "fm_mpx.h"
#include "control_pipe.h"
#include "resampler.h"
#include "lib.h"
#include "ascii_cmd.h"

static uint8_t stop_rds;

static void stop() {
	printf("Received an stopping signal\n");
	stop_rds = 1;
}

static inline void float2char2channel(
	float *inbuf, char *outbuf, size_t frames) {
	uint16_t j = 0, k = 0;
	int16_t sample;
	int8_t lower, upper;

	for (uint16_t i = 0; i < frames; i++) {
		sample = lroundf((inbuf[j]) * 16383.5f);

		/* convert from short to char */
		lower = sample & 0xFF;
		sample >>= 8;
		upper = sample & 0xFF;

		outbuf[k+0] = lower;
		outbuf[k+1] = upper;

		j++;
		k += 2;
	}
}

/* threads */
static void *control_pipe_worker() {
	while (!stop_rds) {
		poll_control_pipe();
		msleep(READ_TIMEOUT_MS);
	}

	close_control_pipe();
	pthread_exit(NULL);
}

static void show_help(char *name) {
	printf(
		"This is MiniRDS, a lightweight RDS encoder.\n"
		"Version %f\n"
		"\n"
		"Usage: %s [options]\n"
		"\n"
		"    -i,--pi           Program Identification code\n"
		"                        [default: 30FE]\n"
		"    -s,--ps           Program Service name\n"
		"                        [default: \" R-AFERA\"]\n"
		"    -r,--rt1           Radio Text 1\n"
		"                        [default: (nothing)]\n"
		"    -p,--pty          Program Type\n"
		"                        [default: 0]\n"
		"    -T,--tp           Traffic Program\n"
		"                        [default: 0]\n"
		"    -A,--af           Alternative Frequency (FM/LF/MF)\n"
		"                        (more than one AF may be passed)\n"
		"    -P,--ptyn         Program Type Name\n"
		"    -l,--lps          Long PS\n"
		"    -e,--ecc          ECC code\n"
		"    -d,--di           DI code\n"
		"    -C,--ctl          FIFO control pipe\n"
		#ifdef RDS2
		"    -I,--img          RDS2 Logo path\n"
		#endif
		"    -h,--help         Show this help text and exit\n"
		"    -v,--version      Show version and exit\n"
		"\n",
		VERSION,
		name
	);
}

static void show_version() {
	printf("MiniRDS version (radio95 Edit, for Radio Afera) %f\n", VERSION);
}

int main(int argc, char **argv) {
	int opt;
	char control_pipe[51];
	struct rds_params_t rds_params = {
		.ps = " R-AFERA",
		.rt1 = "",
		.pi = 0x30FE,
		.ecc = 0xE2,
		.lps = "Radio AFERA tel. 618750213",
		.tp = 1,
		.pty = 11
	};
	float volume = 100.0f;

	/* buffers */
	float *mpx_buffer;
	float *out_buffer;
	char *dev_out;

	int8_t r;
	size_t frames;

	/* SRC */
	SRC_STATE *src_state;
	SRC_DATA src_data;

	/* AO */
	ao_device *device;
	ao_sample_format format;

	/* pthread */
	pthread_attr_t attr;
	pthread_t control_pipe_thread;
	pthread_mutex_t control_pipe_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t control_pipe_cond;

	const char	*short_opt = "R:i:s:r:p:T:A:P:l:e:L:d:C:"
	#ifdef RDS2
	"I:"
	#endif
	"hv";

	struct option	long_opt[] =
	{
		{"rds",		required_argument, NULL, 'R'},
		{"pi",		required_argument, NULL, 'i'},
		{"ps",		required_argument, NULL, 's'},
		{"rt1",		required_argument, NULL, 'r'},
		{"pty",		required_argument, NULL, 'p'},
		{"tp",		required_argument, NULL, 'T'},
		{"af",		required_argument, NULL, 'A'},
		{"ptyn",	required_argument, NULL, 'P'},
		{"lps",    	required_argument, NULL, 'l'},
		{"ecc",    	required_argument, NULL, 'e'},
		{"lic",    	required_argument, NULL, 'L'},
		{"di",    	required_argument, NULL, 'd'},
		{"ctl",		required_argument, NULL, 'C'},
		#ifdef RDS2
		{"img",		required_argument, NULL, 'I'},
		#endif

		{"help",	no_argument, NULL, 'h'},
		{"version",	no_argument, NULL, 'v'},
		{ 0,		0,		0,	0 }
	};

	memset(control_pipe, 0, 51);

keep_parsing_opts:

	opt = getopt_long(argc, argv, short_opt, long_opt, NULL);
	if (opt == -1) goto done_parsing_opts;

	switch (opt) {
		case 'i': /* pi */
			rds_params.pi = strtoul(optarg, NULL, 16);
			break;

		case 's': /* ps */
			memcpy(rds_params.ps, xlat((unsigned char *)optarg), PS_LENGTH);
			break;

		case 'r': /* rt1 */
			memcpy(rds_params.rt1, xlat((unsigned char *)optarg), RT_LENGTH);
			break;

		case 'p': /* pty */
			rds_params.pty = strtoul(optarg, NULL, 10);
			break;

		case 'T': /* tp */
			rds_params.tp = strtoul(optarg, NULL, 10);
			break;

		case 'A': /* af */
			if (add_rds_af(&rds_params.af, strtof(optarg, NULL)) == 1) return 1;
			break;

		case 'P': /* ptyn */
			memcpy(rds_params.ptyn, xlat((unsigned char *)optarg), PTYN_LENGTH);
			break;

		case 'l': /* lps */
			memcpy(rds_params.lps, (unsigned char *)optarg, LPS_LENGTH);
			break;

		case 'e': /* ecc */
			rds_params.ecc = strtoul(optarg, NULL, 16);
			break;

		case 'L': /* lic */
			rds_params.lic = strtoul(optarg, NULL, 16);
			break;

		case 'C': /* ctl */
			memcpy(control_pipe, optarg, 50);
			break;

		#ifdef RDS2
		case 'I': /* img */
			memcpy(rds_params.rds2_image_path, optarg, 50);
			break;
		#endif

		case 'v': /* version */
			show_version();
			return 0;

		case 'h': /* help */
		case '?':
		default:
			show_help(argv[0]);
			return 1;
	}

	goto keep_parsing_opts;

done_parsing_opts:

	/* Initialize pthread stuff */
	pthread_mutex_init(&control_pipe_mutex, NULL);
	pthread_cond_init(&control_pipe_cond, NULL);
	pthread_attr_init(&attr);

	/* Setup buffers */
	mpx_buffer = malloc(NUM_MPX_FRAMES_IN * 2 * sizeof(float));
	out_buffer = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(float));
	dev_out = malloc(NUM_MPX_FRAMES_OUT * 2 * sizeof(int16_t) * sizeof(char));

	/* Gracefully stop the encoder on SIGINT or SIGTERM */
	signal(SIGINT, stop);
	signal(SIGTERM, stop);

	/* Initialize the baseband generator */
	fm_mpx_init(MPX_SAMPLE_RATE);
	set_output_volume(volume);

	/* Initialize the RDS modulator */
	init_rds_encoder(rds_params);

	/* AO format */
	memset(&format, 0, sizeof(struct ao_sample_format));
	format.channels = 1;
	format.bits = 16;
	format.rate = OUTPUT_SAMPLE_RATE;
	format.byte_format = AO_FMT_LITTLE;

	ao_initialize();

	device = ao_open_live(ao_default_driver_id(), &format, NULL);
	if (device == NULL) {
		fprintf(stderr, "Error: cannot open sound device.\n");
		ao_shutdown();
		goto exit;
	}

	/* SRC out (MPX -> output) */
	memset(&src_data, 0, sizeof(SRC_DATA));
	src_data.input_frames = NUM_MPX_FRAMES_IN;
	src_data.output_frames = NUM_MPX_FRAMES_OUT;
	src_data.src_ratio =
		(double)OUTPUT_SAMPLE_RATE / (double)MPX_SAMPLE_RATE;
	src_data.data_in = mpx_buffer;
	src_data.data_out = out_buffer;

	r = resampler_init(&src_state, 1);
	if (r < 0) {
		fprintf(stderr, "Could not create output resampler.\n");
		goto exit;
	}

	/* Initialize the control pipe reader */
	if (control_pipe[0]) {
		if (open_control_pipe(control_pipe) == 0) {
			fprintf(stderr, "Reading control commands on %s.\n", control_pipe);
			/* Create control pipe polling worker */
			r = pthread_create(&control_pipe_thread, &attr, control_pipe_worker, NULL);
			if (r < 0) {
				fprintf(stderr, "Could not create control pipe thread.\n");
				control_pipe[0] = 0;
				goto exit;
			} else {
				fprintf(stderr, "Created control pipe thread.\n");
			}
		} else {
			fprintf(stderr, "Failed to open control pipe: %s.\n", control_pipe);
			control_pipe[0] = 0;
		}
	}

	for (;;) {
		fm_rds_get_frames(mpx_buffer, NUM_MPX_FRAMES_IN);

		if (resample(src_state, src_data, &frames) < 0) break;

		float2char2channel(out_buffer, dev_out, frames);

		/* num_bytes = audio frames( * channels) * bytes per sample */
		if (!ao_play(device, dev_out, frames * sizeof(int16_t))) {
			fprintf(stderr, "Error: could not play audio.\n");
			break;
		}

		if (stop_rds) {
			fprintf(stderr, "Stopping the loop...\n");
			break;
		}
	}

	resampler_exit(src_state);

exit:
	if (control_pipe[0]) {
		/* shut down threads */
		fprintf(stderr, "Waiting for pipe thread to shut down.\n");
		pthread_cond_signal(&control_pipe_cond);
		pthread_join(control_pipe_thread, NULL);
	}

	pthread_attr_destroy(&attr);

	fm_mpx_exit();
	exit_rds_encoder();

	free(mpx_buffer);
	free(out_buffer);
	free(dev_out);

	return 0;
}
