// Version linked into radiod
#undef DEBUG_AGC
#define _GNU_SOURCE 1
#include <assert.h>
#include <pthread.h>
#include <libairspyhf/airspyhf.h>
#include <errno.h>
#include <iniparser/iniparser.h>
#if defined(linux)
#include <bsd/string.h>
#endif

#include "conf.h"
#include "misc.h"
#include "multicast.h"
#include "status.h"
#include "radio.h"
#include "config.h"

extern int Status_ttl;

// Global variables set by config file options
extern char Metadata_dest_string[];
extern char const *Iface;
extern int IP_tos;
extern int Verbose;
extern int Overlap;
extern const char *App_path;
extern int Verbose;
static int const Bufsize = 16384;

// Anything generic should be in 'struct frontend' section 'sdr' in radio.h
struct sdrstate {
  struct frontend *frontend;  // Avoid references to external globals
  struct airspyhf_device *device;    // Opaque pointer

  uint32_t sample_rates[20];
  uint64_t SN; // Serial number

  // Tuning
  char const *frequency_file; // Local file to store frequency in case we restart

  pthread_t cmd_thread;
  pthread_t monitor_thread;
};

static double set_correct_freq(struct sdrstate *sdr,double freq);
static void decode_airspyhf_commands(struct sdrstate *,uint8_t *,int);
static void send_airspyhf_status(struct sdrstate *,int);
static int rx_callback(airspyhf_transfer_t *transfer);
static void *airspyhf_cmd(void *);
static void *airspyhf_monitor(void *p);
static double true_freq(uint64_t freq);

int airspyhf_setup(struct frontend * const frontend,dictionary * const Dictionary,char const * const section){
  assert(Dictionary != NULL);

  struct sdrstate * const sdr = calloc(1,sizeof(struct sdrstate));
  // Cross-link generic and hardware-specific control structures
  sdr->frontend = frontend;
  frontend->sdr.context = sdr;
  {
    char const *device = config_getstring(Dictionary,section,"device",NULL);
    if(strcasecmp(device,"airspyhf") != 0)
      return -1; // Not for us
  }
  {
    char const *p = config_getstring(Dictionary,section,"status",NULL);
    if(p != NULL)
      strlcpy(frontend->input.metadata_dest_string,p,sizeof(frontend->input.metadata_dest_string));
    else {
      // Default to "fe-" followed by receiver metadata target, e.g., "fe-hf.local"
      char *tmp = NULL;
      asprintf(&tmp,"fe-%s",Metadata_dest_string);
      strlcpy(frontend->input.metadata_dest_string,tmp,sizeof(frontend->input.metadata_dest_string));
      FREE(tmp);
    }
  }
  Status_ttl = config_getint(Dictionary,section,"ttl",Status_ttl);
  {
    char const *sn = config_getstring(Dictionary,section,"serial",NULL);
    if(sn != NULL){
      char *endptr = NULL;
      sdr->SN = 0;
      sdr->SN = strtoull(sn,&endptr,16);
      if(endptr == NULL || *endptr != '\0'){
	fprintf(stdout,"Invalid serial number %s in section %s\n",sn,section);
	return -1;
      }
    } else {
      // Serial number not specified, enumerate and pick one
      int n_serials = 100; // ridiculously large
      uint64_t serials[n_serials];

      n_serials = airspyhf_list_devices(serials,n_serials); // Return actual number
      if(n_serials == 0){
	fprintf(stdout,"No airspyhf devices found\n");
	return -1;
      }
      fprintf(stdout,"Discovered airspyhf device serials:");
      for(int i = 0; i < n_serials; i++){
	fprintf(stdout," %llx",(long long)serials[i]);
      }
      fprintf(stdout,"\n");
      fprintf(stdout,"Selecting %llx; to select another, add 'serial = ' to config file\n",(long long)serials[0]);
      sdr->SN = serials[0];
    }
  }
  {
    int ret = airspyhf_open_sn(&sdr->device,sdr->SN);
    if(ret != AIRSPYHF_SUCCESS){
      fprintf(stdout,"airspyhf_open(%llx) failed\n",(long long)sdr->SN);
      return -1;
    } 
  }
  {
    airspyhf_lib_version_t version;
    airspyhf_lib_version(&version);

    const int VERSION_LOCAL_SIZE = 128; // Library doesn't define, but says should be >= 128
    char hw_version[VERSION_LOCAL_SIZE];
    airspyhf_version_string_read(sdr->device,hw_version,sizeof(hw_version));

    fprintf(stdout,"Airspyhf serial %llx, hw version %s, library version %d.%d.%d\n",
	    (long long unsigned)sdr->SN,
	    hw_version,
	    version.major_version,version.minor_version,version.revision);
  }
  // Initialize hardware first
  {
    // Get and list sample rates
    int ret __attribute__ ((unused));
    ret = airspyhf_get_samplerates(sdr->device,sdr->sample_rates,0);
    assert(ret == AIRSPYHF_SUCCESS);
    int const number_sample_rates = sdr->sample_rates[0];
    fprintf(stdout,"%'d sample rates:",number_sample_rates);
    if(number_sample_rates == 0){
      fprintf(stdout,"error, no valid sample rates!\n");
      return -1;
    }
    ret = airspyhf_get_samplerates(sdr->device,sdr->sample_rates,number_sample_rates);
    assert(ret == AIRSPYHF_SUCCESS);
    for(int n = 0; n < number_sample_rates; n++){
      fprintf(stdout," %'d",sdr->sample_rates[n]);
      if(sdr->sample_rates[n] < 1)
	break;
    }
    fprintf(stdout,"\n");
  }
  // Default to first (highest) sample rate on list
  frontend->sdr.samprate = config_getint(Dictionary,section,"samprate",sdr->sample_rates[0]);
  frontend->sdr.isreal = false;
  frontend->sdr.calibrate = config_getdouble(Dictionary,section,"calibrate",0);

  fprintf(stdout,"Set sample rate %'u Hz\n",frontend->sdr.samprate);
  {
    int ret __attribute__ ((unused));
    ret = airspyhf_set_samplerate(sdr->device,(uint32_t)frontend->sdr.samprate);
    assert(ret == AIRSPYHF_SUCCESS);
  }
  {
    int const hf_agc = config_getboolean(Dictionary,section,"hf-agc",false); // default off
    airspyhf_set_hf_agc(sdr->device,hf_agc);

    int const agc_thresh = config_getboolean(Dictionary,section,"agc-thresh",false); // default off
    airspyhf_set_hf_agc_threshold(sdr->device,agc_thresh);

    int const hf_att = config_getboolean(Dictionary,section,"hf-att",false); // default off
    airspyhf_set_hf_att(sdr->device,hf_att);

    int const hf_lna = config_getboolean(Dictionary,section,"hf-lna",false); // default off
    airspyhf_set_hf_lna(sdr->device,hf_lna);

    int const lib_dsp = config_getboolean(Dictionary,section,"lib-dsp",true); // default on
    airspyhf_set_lib_dsp(sdr->device,lib_dsp);

    fprintf(stdout,"HF AGC %d, AGC thresh %d, hf att %d, hf-lna %d, lib-dsp %d\n",
	    hf_agc,agc_thresh,hf_att,hf_lna,lib_dsp);
  }
  {
    char const *p = config_getstring(Dictionary,section,"description",NULL);
    if(p != NULL){
      strlcpy(frontend->sdr.description,p,sizeof(frontend->sdr.description));
      fprintf(stdout,"%s: ",frontend->sdr.description);
    }
  }
  double init_frequency = config_getdouble(Dictionary,section,"frequency",0);
  if(init_frequency != 0)
    frontend->sdr.lock = 1;
  {
    char *tmp;
    int ret __attribute__ ((unused));
    // space is malloc'ed by asprintf but not freed
    ret = asprintf(&tmp,"%s/tune-airspyhf.%llx",VARDIR,(unsigned long long)sdr->SN);
    if(ret != -1){
      sdr->frequency_file = tmp;
    }
  }
  if(init_frequency == 0){
    // If not set on command line, load saved frequency
    FILE *fp = fopen(sdr->frequency_file,"r+");
    if(fp == NULL)
      fprintf(stdout,"Can't open tuner state file %s: %s\n",sdr->frequency_file,strerror(errno));
    else {
      fprintf(stdout,"Using tuner state file %s\n",sdr->frequency_file);
      int r;
      if((r = fscanf(fp,"%lf",&init_frequency)) < 0)
	fprintf(stdout,"Can't read stored freq. r = %'d: %s\n",r,strerror(errno));
      fclose(fp);
    }
  }
  if(init_frequency == 0){
    // Not set in config file or from cache file. Use fallback to cover 2m
    init_frequency = 10e6; // Fallback default
    fprintf(stdout,"Fallback default frequency %'.3lf Hz\n",init_frequency);
  }
  fprintf(stdout,"Setting initial frequency %'.3lf Hz, %s\n",init_frequency,frontend->sdr.lock ? "locked" : "not locked");
  set_correct_freq(sdr,init_frequency);
  return 0;
}
int airspyhf_startup(struct frontend *frontend){
  struct sdrstate *sdr = (struct sdrstate *)frontend->sdr.context;
  pthread_create(&sdr->cmd_thread,NULL,airspyhf_cmd,sdr);
  pthread_create(&sdr->monitor_thread,NULL,airspyhf_monitor,sdr);
  return 0;
}

static void *airspyhf_monitor(void *p){
  struct sdrstate *sdr = (struct sdrstate *)p;
  assert(sdr != NULL);
  pthread_setname("airspyhf-mon");

  realtime();
  int ret __attribute__ ((unused));
  ret = airspyhf_start(sdr->device,rx_callback,sdr);
  assert(ret == AIRSPYHF_SUCCESS);
  send_airspyhf_status(sdr,1); // Tell the world we're alive
  fprintf(stdout,"airspyhf running\n");
  // Periodically poll status to ensure device hasn't reset
  while(1){
    sleep(1);
    if(!airspyhf_is_streaming(sdr->device))
      break; // Device seems to have bombed. Exit and let systemd restart us
  }
  fprintf(stdout,"Device is no longer streaming, exiting\n");
  airspyhf_close(sdr->device);
  return NULL;
}

// Thread to send metadata and process commands
void *airspyhf_cmd(void *arg){
  // Send status, process commands
  pthread_setname("airspyhf-cmd");
  assert(arg != NULL);
  struct sdrstate * const sdr = (struct sdrstate *)arg;
  struct frontend *frontend = sdr->frontend;
  
  if(frontend->input.ctl_fd < 3 || frontend->input.fe_status_fd < 3) 
    return NULL; // Nothing to do

  while(1){
    uint8_t buffer[Bufsize];
    int const length = recv(frontend->input.fe_status_fd,buffer,sizeof(buffer),0);
    if(length > 0){
      // Parse entries
      if(buffer[0] == 0)
	continue;  // Ignore our own status messages

      frontend->sdr.commands++;
      decode_airspyhf_commands(sdr,buffer+1,length-1);
      send_airspyhf_status(sdr,1);
    }
  }
}

static void decode_airspyhf_commands(struct sdrstate *sdr,uint8_t *buffer,int length){
  struct frontend * const frontend = sdr->frontend;

  uint8_t *cp = buffer;

  while(cp - buffer < length){
    int ret __attribute__((unused)); // Won't be used when asserts are disabled
    enum status_type const type = *cp++; // increment cp to length field
    
    if(type == EOL)
      break; // End of list
    
    unsigned int optlen = *cp++;
    if(optlen & 0x80){
      // length is >= 128 bytes; fetch actual length from next N bytes, where N is low 7 bits of optlen
      int length_of_length = optlen & 0x7f;
      optlen = 0;
      while(length_of_length > 0){
	optlen <<= 8;
	optlen |= *cp++;
	length_of_length--;
      }
    }
    if(cp - buffer + optlen >= length)
      break; // Invalid length
    
    switch(type){
    case EOL: // Shouldn't get here
      break;
    case COMMAND_TAG:
      frontend->sdr.command_tag = decode_int(cp,optlen);
      break;
    case CALIBRATE:
      frontend->sdr.calibrate = decode_double(cp,optlen);
      break;
    case RADIO_FREQUENCY:
      if(!frontend->sdr.lock){
	double const f = decode_double(cp,optlen);
	set_correct_freq(sdr,f);
      }
      break;
    default: // Ignore all others
      break;
    }
    cp += optlen;
  }    
}  

static void send_airspyhf_status(struct sdrstate *sdr,int full){
  struct frontend * const frontend = sdr->frontend;

  frontend->input.metadata_packets++;

  uint8_t packet[2048],*bp;
  bp = packet;
  
  *bp++ = 0;   // Command/response = response
  
  encode_int32(&bp,COMMAND_TAG,frontend->sdr.command_tag);
  encode_int64(&bp,CMD_CNT,frontend->sdr.commands);
  
  frontend->sdr.timestamp = gps_time_ns();
  encode_int64(&bp,GPS_TIME,frontend->sdr.timestamp);

  if(frontend->sdr.description[0] != '\0')
    encode_string(&bp,DESCRIPTION,frontend->sdr.description,strlen(frontend->sdr.description));

  encode_int32(&bp,INPUT_SAMPRATE,frontend->sdr.samprate);
  encode_int64(&bp,OUTPUT_METADATA_PACKETS,frontend->input.metadata_packets);

  // Front end
  encode_double(&bp,CALIBRATE,frontend->sdr.calibrate);

  // Tuning
  encode_double(&bp,RADIO_FREQUENCY,frontend->sdr.frequency);
  encode_int32(&bp,LOCK,frontend->sdr.lock);

  encode_byte(&bp,DEMOD_TYPE,0); // actually LINEAR_MODE
  encode_int32(&bp,OUTPUT_SAMPRATE,frontend->sdr.samprate);
  encode_int32(&bp,OUTPUT_CHANNELS,1);
  encode_int32(&bp,DIRECT_CONVERSION,1);
  // Receiver inverts spectrum, use lower side
  encode_float(&bp,HIGH_EDGE,+0.43 * frontend->sdr.samprate); // empirical for airspyhf
  encode_float(&bp,LOW_EDGE,-0.43 * frontend->sdr.samprate); // Should look at the actual filter curves

  encode_eol(&bp);
  int len = bp - packet;
  assert(len < sizeof(packet));
  send(frontend->input.ctl_fd,packet,len,0);
}

static bool Name_set = false;
// Callback called with incoming receiver data from A/D
static int rx_callback(airspyhf_transfer_t *transfer){
  assert(transfer != NULL);
  struct sdrstate * const sdr = (struct sdrstate *)transfer->ctx;
  assert(sdr != NULL);
  struct frontend * const frontend = sdr->frontend;
  assert(frontend != NULL);

  if(!Name_set){
    pthread_setname("airspyhf-cb");
    Name_set = true;
  }
  if(transfer->dropped_samples){
    fprintf(stdout,"dropped %'lld\n",(long long)transfer->dropped_samples);
  }
  int const sampcount = transfer->sample_count;
  float complex * const wptr = frontend->in->input_write_pointer.c;
  float complex * const up = (float complex *)transfer->samples;
  assert(wptr != NULL);
  assert(up != NULL);
  float in_energy = 0;
  for(int i=0; i < sampcount; i++){
    float complex x;
    x = up[i];
    in_energy += x * x;
    wptr[i] = x;
  }
  frontend->input.samples += sampcount;
  write_cfilter(frontend->in,NULL,sampcount); // Update write pointer, invoke FFT
  frontend->sdr.output_level = in_energy / sampcount;
  return 0;
}

static double true_freq(uint64_t freq_hz){
  return (double)freq_hz; // Placeholder
}

// set the airspyhf tuner to the requested frequency, applying:
// TCXO calibration offset
// the TCXO calibration offset is a holdover from the Funcube dongle and doesn't
// really fit the Airspyhf with its internal factory calibration
// All this really works correctly only with a gpsdo, forcing the calibration offset to 0
static double set_correct_freq(struct sdrstate *sdr,double freq){
  struct frontend *frontend = sdr->frontend;
  int64_t intfreq = round((freq)/ (1 + frontend->sdr.calibrate));
  int ret __attribute__((unused)) = AIRSPYHF_SUCCESS; // Won't be used when asserts are disabled
  ret = airspyhf_set_freq(sdr->device,intfreq);
  assert(ret == AIRSPYHF_SUCCESS);
  double const tf = true_freq(intfreq);
  frontend->sdr.frequency = tf * (1 + frontend->sdr.calibrate);
  FILE *fp = fopen(sdr->frequency_file,"w");
  if(fp){
    if(fprintf(fp,"%lf\n",frontend->sdr.frequency) < 0)
      fprintf(stdout,"Can't write to tuner state file %s: %s\n",sdr->frequency_file,strerror(errno));
    fclose(fp);
    fp = NULL;
  }
  return frontend->sdr.frequency;
}
double airspyhf_tune(struct frontend *frontend,double f){
  struct sdrstate *sdr = frontend->sdr.context;
  return set_correct_freq(sdr,f);
}