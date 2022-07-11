// $Id: iir.h,v 1.3 2022/06/29 08:25:35 karn Exp $
// Various simple IIR filters
#ifndef _IIR_H
#define _IIR_H 1
#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include "misc.h"
#include "iir.h"

// Experimental complex notch filter
struct notchfilter {
  complex double osc_phase; // Phase of local complex mixer
  complex double osc_step;  // mixer phase increment (frequency)
  complex float dcstate;    // Average signal at mixer frequency
  float bw;                 // Relative bandwidth of notch
};


struct notchfilter *notch_create(double,float);
#define notch_delete(x) free(x)
complex float notch(struct notchfilter *,complex float);

// Goertzel state
struct goertzel {
  float coeff; // 2 * cos(2*pi*f/fs) = 2 * creal(cf)
  complex float cf; // exp(-j*2*pi*f/fs)
  float s0,s1; // IIR filter state, s0 is the most recent
};


// Initialize goertzel state to fractional frequency f
void init_goertzel(struct goertzel *gp,float f);
static inline void reset_goertzel(struct goertzel *gp){
  gp->s0 = gp->s1 = 0;
}

static void inline update_goertzel(struct goertzel *gp,float x){
  float s0save = gp->s0;
  gp->s0 = x + gp->coeff * gp->s0 - gp->s1;
  gp->s1 = s0save;
}
complex float output_goertzel(struct goertzel *gp);


#endif
