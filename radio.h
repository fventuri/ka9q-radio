// Internal structures and functions of the 'ka9q-radio' package
// Nearly all internal state is in the 'demod' structure
// More than one can exist in the same program,
// but so far it seems easier to just run separate instances of the 'radio' program.
// Copyright 2018-2023, Phil Karn, KA9Q
#ifndef _RADIO_H
#define _RADIO_H 1

#include <pthread.h>
#include <complex.h>

#include <sys/socket.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <iniparser/iniparser.h>

#include "multicast.h"
#include "osc.h"
#include "status.h"
#include "filter.h"
#include "iir.h"

enum demod_type {
  LINEAR_DEMOD = 0,     // Linear demodulation, i.e., everything else: SSB, CW, DSB, CAM, IQ
  FM_DEMOD,             // Frequency demodulation
  WFM_DEMOD,            // wideband frequency modulation (broadcast)
  SPECT_DEMOD,          // Spectrum analysis pseudo-demod
};

struct demodtab {
  enum demod_type type;
  char name[16];
};

extern struct demodtab Demodtab[];
extern int Ndemod;

// Only one off these, shared with all demod instances
struct frontend {

  // Stuff we maintain about our upstream source
  uint64_t samples;     // Count of raw I/Q samples received
  uint64_t overranges;  // Count of full scale A/D samples

  int M;            // Impulse length of input filter
  int L;            // Block length of input filter

  // Stuff maintained by our upstream source and filled in by the status daemon
  char *description;  // free-form text
  int reference;     // Reference frequency
  int samprate;      // Sample rate on raw input data stream
  int64_t timestamp; // Nanoseconds since GPS epoch 6 Jan 1980 00:00:00 UTC
  double frequency;
  double calibrate;  // Clock frequency error ratio, e.g, +1e-6 means 1 ppm high
  // R820T/828 tuner gains, dB. Informational only; total is reported in rf_gain
  uint8_t lna_gain;
  uint8_t mixer_gain;
  uint8_t if_gain;

  float rf_atten;    // dB (RX888 only)
  float rf_gain;     // dB gain (RX888) or lna_gain + mixer_gain + if_gain
  bool direct_conversion; // Try to avoid DC spike if set
  bool isreal;            // Use real->complex FFT (otherwise complex->complex)
  int bitspersample; // 8, 12 or 16
  bool lock;              // Tuning is locked; clients cannot change
  
  // Limits on usable IF due to aliasing, filtering, etc
  // Less than or equal to +/- samprate/2
  // Straddles 0 Hz for complex, will have same sign for real output from a low IF tuner
  // Usually negative for the 820/828 tuners, which are effectively wideband LSB radios
  float min_IF;
  float max_IF;
  
  /* For efficiency, signal levels now scaled to full A/D range, e.g.,
     16 bit real:    0 dBFS = +87.2984 dB = 32767/sqrt(2) units RMS
     16 bit complex: 0 dBFS = +90.3087 dB = 32767 units RMS
     12 bit real:    0 dBFS = +63.2121 dB = 2047/sqrt(2) units RMS
     12 bit complex: 0 dBFS = +66.2224 dB = 2047 units RMS
      8 bit complex: 0 dBFS = +42.0761 dB = 127 units RMS

      so full A/D range now corresponds to different levels internally, and are scaled
      in radio_status.c when sending status messages
  */
  float if_power;   // Exponentially smoothed power measurement in A/D units (not normalized)
  float if_power_max;
  
  // This structure is updated asynchronously by the front end thread, so it's protected
  pthread_mutex_t status_mutex;
  pthread_cond_t status_cond;     // Signalled whenever status changes
  
  // Entry points for local front end driver
  void *context;         // Stash hardware-dependent control block
  int (*setup)(struct frontend *,dictionary *,char const *); // Get front end ready to go
  int (*start)(struct frontend *);          // Start front end sampling
  double (*tune)(struct frontend *,double); // Tune front end, return actual frequency
  struct filter_in * restrict in; // Input half of fast convolver, shared with all channels
};

extern struct frontend Frontend; // Only one per radio instance

#if 0
// Control parameters for demod state block
struct param {
  pthread_mutex_t mutex;
  double freq;
  double shift;
  double doppler;
  double doppler_rate;
  float min_IF;
  float max_IF;
  float kaiser_beta;
  bool isb;
  enum demod_type demod_type;
  char preset[32];
  bool env;            // Envelope detection in linear mode (settable)
  bool agc;            // Automatic gain control enabled (settable)
  float hangtime;      // AGC hang time, samples (settable)
  float recovery_rate; // AGC recovery rate, amplitude ratio/sample  (settable)
  float threshold;     // AGC threshold above noise, amplitude ratio
  bool pll;         // Linear mode PLL tracking of carrier (settable)
  bool square;      // Squarer on PLL input (settable)
  float loop_bw;    // Loop bw (coherent modes)
  float squelch_open;  // squelch open threshold, power ratio
  float squelch_close; // squelch close threshold
  int squelchtail;     // Frames to hold open after loss of SNR
  int samprate;      // Audio D/A sample rate
  float gain;        // Audio gain to normalize amplitude
  float headroom;    // Audio level headroom, amplitude ratio (settable)
  struct sockaddr_storage data_source_address;    // Source address of our data output
  struct sockaddr_storage data_dest_address;      // Dest of our data outputg (typically multicast)
  char data_dest_string[_POSIX_HOST_NAME_MAX+20]; // Allow room for :portnum
  int channels;   // 1 = mono, 2 = stereo (settable)
  // 'rate' computed from expf(-1.0 / (tc * output.samprate));
  // tc = 75e-6 sec for North American FM broadcasting
  // tc = 1 / (2 * M_PI * 300.) = 530.5e-6 sec for NBFM (300 Hz corner freq)
  float rate;
};
#endif

// Channel state block; there can be many of these
struct channel {
  bool inuse;
#if 0
  struct param param; // not yet used
#endif

  int lifetime;          // Remaining lifetime, seconds
  // Tuning parameters
  struct {
    double freq;         // Desired carrier frequency (settable)
    double shift;        // Post-demod frequency shift (settable)
    double second_LO;
    double doppler;      // (settable)
    double doppler_rate; // (settable)
  } tune;

  struct osc fine,shift;

  // Zero IF pre-demod filter params
  struct {
    struct filter_out *out;
    float min_IF;          // Edges of filter (settable)
    float max_IF;         // (settable)
    // Window shape factor for Kaiser window
    float kaiser_beta;  // settable
    bool isb;           // Independent sideband mode (settable, currently unimplemented)
    float *energies;    // Vector of smoothed bin energies
    int bin_shift;      // FFT bin shift for frequency conversion
    double remainder;   // Frequency remainder for fine tuning
    complex double phase_adjust; // Block rotation of phase
  } filter;

  enum demod_type demod_type;  // Index into demodulator table (Linear, FM, FM Stereo, Spectrum)
  char preset[32];       // name of last mode preset

  struct {               // Used only in linear demodulator
    bool env;            // Envelope detection in linear mode (settable)
    bool agc;            // Automatic gain control enabled (settable)
    float hangtime;      // AGC hang time, samples (settable)
    float recovery_rate; // AGC recovery rate, amplitude ratio/sample  (settable)
    float threshold;     // AGC threshold above noise, amplitude ratio

    bool pll;         // Linear mode PLL tracking of carrier (settable)
    bool square;      // Squarer on PLL input (settable)
    float lock_timer; // PLL lock timer
    bool pll_lock;    // PLL is locked
    float loop_bw;    // Loop bw (coherent modes)
    float cphase;     // Carrier phase change radians (DSB/PSK)
  } linear;
  int hangcount;

  struct {
    struct pll pll;
    bool was_on;
    int lock_count;
  } pll;

  // Signal levels & status, common to all demods
  struct {
    float bb_power;   // Average power of signal after filter but before digital gain, power ratio
    float bb_energy;  // Integrated power, reset by poll
    float foffset;    // Frequency offset Hz (FM, coherent AM, dsb)
    float snr;        // From PLL in linear, moments in FM
    float n0;         // per-demod N0 (experimental)
  } sig;
  
  float squelch_open;  // squelch open threshold, power ratio
  float squelch_close; // squelch close threshold
  int squelchtail;     // Frames to hold open after loss of SNR

  struct {               // Used only in FM demodulator
    float pdeviation;    // Peak frequency deviation Hz (FM)
    float tone_freq;        // PL tone squelch frequency
    struct goertzel tonedetect;
    float tone_deviation; // Measured deviation of tone
    bool threshold;       // Threshold extension
  } fm;

  // Used by spectrum analysis only
  // Coherent bin bandwidth = block rate in Hz
  // Coherent bin spacing = block rate * 1 - ((M-1)/(L+M-1))
  struct {
    float bin_bw;     // Requested bandwidth (hz) of noncoherent integration bin
    int bin_count;    // Requested bin count
    float *bin_data;  // Array of real floats with bin_count elements
  } spectrum;

  // Output
  struct {
    int samprate;      // Audio D/A sample rate
    float gain;        // Audio gain to normalize amplitude
    float sum_gain_sq; // Sum of squared gains, for averaging
    float headroom;    // Audio level headroom, amplitude ratio (settable)
    // RTP network streaming
    bool silent;       // last packet was suppressed (used to generate RTP mark bit)
    struct rtp_state rtp;
    
    struct sockaddr_storage data_source_address;    // Source address of our data output
    struct sockaddr_storage data_dest_address;      // Dest of our data outputg (typically multicast)
    char data_dest_string[_POSIX_HOST_NAME_MAX+20]; // Allow room for :portnum
    
    int data_fd;    // File descriptor for multicast output
    int rtcp_fd;    // File descriptor for RTP control protocol
    int sap_fd;     // Session announcement protocol (SAP) - experimental
    int channels;   // 1 = mono, 2 = stereo (settable)
    float energy;   // Output energy since last poll

    float deemph_state_left;
    float deemph_state_right;
    uint64_t samples;
    bool pacing;     // Pace output packets
  } output;

  // Used only when FM deemphasis is enabled
  struct {
    complex float state; // stereo filter state
    float gain;     // Empirically set
    // 'rate' computed from expf(-1.0 / (tc * output.samprate));
    // tc = 75e-6 sec for North American FM broadcasting
    // tc = 1 / (2 * M_PI * 300.) = 530.5e-6 sec for NBFM (300 Hz corner freq)
    float rate;
  } deemph;

  uint32_t commands;
  uint32_t command_tag;
  uint64_t blocks_since_poll; // Used for averaging signal levels

  pthread_t sap_thread;
  pthread_t rtcp_thread;
  pthread_t demod_thread;
  // Set this flag to ask demod_thread to terminate.
  // pthread_cancel() can't be used because we're usually waiting inside of a mutex, and deadlock will occur
  bool terminate;
  float tp1,tp2; // Spare test points
};

extern struct channel *Channel_list;
extern int Channel_list_length;
extern int Active_channel_count;
extern int const Channel_alloc_quantum;
extern pthread_mutex_t Channel_list_mutex;

extern int Status_fd;  // File descriptor for receiver status
extern int Ctl_fd;     // File descriptor for receiving user commands

extern char const *Presetfile;
extern int Verbose;
extern float Blocktime; // Common to all receiver slices. NB! Milliseconds, not seconds
extern uint64_t Metadata_packets;

// Functions/methods to control a channel instance
struct channel *create_chan(uint32_t ssrc);
struct channel *lookup_chan(uint32_t ssrc);
struct channel *setup_chan(uint32_t ssrc);
void free_chan(struct channel **);

char const *demod_name_from_type(enum demod_type type);
int demod_type_from_name(char const *name);
int loadpreset(struct channel *chan,dictionary const *table,char const *preset);
int set_defaults(struct channel *chan);

double set_freq(struct channel * restrict ,double);
int compute_tuning(int N, int M, int samprate,int *shift,double *remainder, double freq);
int start_demod(struct channel * restrict chan);
int kill_demod(struct channel ** restrict chan);
double set_first_LO(struct channel const * restrict, double);
int downconvert(struct channel *chan);
float scale_voltage_out2FS(struct frontend *frontend);
float scale_power_out2FS(struct frontend *frontend);
float scale_ADvoltage2FS(struct frontend const *frontend);
float scale_ADpower2FS(struct frontend const *frontend);

// Helper threads
void *sap_send(void *);
void *radio_status(void *);
void *chan_reaper(void *);

// Demodulator thread entry points
void *demod_fm(void *);
void *demod_wfm(void *);
void *demod_linear(void *);
void *demod_spectrum(void *);
void *demod_null(void *);

// Send output to multicast streams
int send_output(struct channel * restrict ,const float * restrict,int,bool);
#endif
