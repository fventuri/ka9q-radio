// $Id: status.c,v 1.30 2022/07/06 02:39:39 karn Exp $
// encode/decode status packets
// Copyright 2020 Phil Karn, KA9Q

#define _GNU_SOURCE 1
#include <assert.h>
#include <string.h>
#if defined(linux)
#include <bsd/string.h>
#endif
#include <math.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>
#include <netdb.h>

#include "misc.h"
#include "status.h"
#include "radio.h"

extern int Mcast_ttl;

union result {
  uint64_t ll;
  uint32_t l;
  float f;
  double d;
};


// Encode 64-bit integer, byte swapped, leading zeroes suppressed
int encode_int64(unsigned char **buf,enum status_type type,uint64_t x){
  unsigned char *cp = *buf;

  *cp++ = type;

  if(x == 0){
    // Compress zero value to zero length
    *cp++ = 0;
    *buf = cp;
    return 2;
  }

  int len = sizeof(x);
  while(len > 0 && (x & 0xff00000000000000LL) == 0){
    x <<= 8;
    len--;
  }
  *cp++ = len;

  for(int i=0; i<len; i++){
    *cp++ = x >> 56;
    x <<= 8;
  }

  *buf = cp;
  return 2+len;
}


// Single null type byte means end of list
int encode_eol(unsigned char **buf){
  unsigned char *bp = *buf;

  *bp++ = EOL;
  *buf = bp;
  return 1;
}

int encode_byte(unsigned char **buf,enum status_type type,unsigned char x){
  unsigned char *cp = *buf;
  *cp++ = type;
  if(x == 0){
    // Compress zero value to zero length
    *cp++ = 0;
    *buf = cp;
    return 2;
  }
  *cp++ = sizeof(x);
  *cp++ = x;
  *buf = cp;
  return 2+sizeof(x);
}

int encode_int16(unsigned char **buf,enum status_type type,uint16_t x){
  return encode_int64(buf,type,(uint64_t)x);
}

int encode_int32(unsigned char **buf,enum status_type type,uint32_t x){
  return encode_int64(buf,type,(uint64_t)x);
}

int encode_int(unsigned char **buf,enum status_type type,int x){
  return encode_int64(buf,type,(uint64_t)x);
}


int encode_float(unsigned char **buf,enum status_type type,float x){
  if(isnan(x))
    return 0; // Never encode a NAN

  union result r;
  r.f = x;
  return encode_int32(buf,type,r.l);
}

int encode_double(unsigned char **buf,enum status_type type,double x){
  if(isnan(x))
    return 0; // Never encode a NAN

  union result r;
  r.d = x;
  return encode_int64(buf,type,r.ll);
}

// Encode byte string without byte swapping
int encode_string(unsigned char **bp,enum status_type type,void const *buf,int buflen){
  unsigned char *cp = *bp;
  *cp++ = type;
  if(buflen > 255)
    buflen = 255;
  *cp++ = buflen;
  memcpy(cp,buf,buflen);
  *bp = cp + buflen;
  return 2+buflen;
}


// Decode byte string without byte swapping
char *decode_string(unsigned char const *cp,int optlen,char *buf,int buflen){
  int n = min(optlen,buflen-1);
  memcpy(buf,cp,n);
  buf[n] = '\0'; // force null termination
  return buf;
}


// Decode encoded variable-length UNSIGNED integers
// At entry, *bp -> length field (not type!)
// Works for byte, short, long, long long
uint64_t decode_int(unsigned char const *cp,int len){
  uint64_t result = 0;
  // cp now points to beginning of abbreviated int
  // Byte swap as we accumulate
  while(len-- > 0)
    result = (result << 8) | *cp++;

  return result;
}


float decode_float(unsigned char const *cp,int len){
  if(len == 0)
    return 0;
  
  if(len == 8)
    return (float)decode_double(cp,len);

  union result r;
  r.ll = decode_int(cp,len);
  return r.f;
}

double decode_double(unsigned char const *cp,int len){
  if(len == 0)
    return 0;
  
  if(len == 4)
    return (double)decode_float(cp,len);

  union result r;
  r.ll = decode_int(cp,len);
  return r.d;
}

// The Linux/UNIX socket data structures are a real mess...
int encode_socket(unsigned char **buf,enum status_type type,void const *sock){
  struct sockaddr_in const *sin = sock;
  struct sockaddr_in6 const *sin6 = sock;
  unsigned char *bp = *buf;
  int optlen = 0;

  switch(sin->sin_family){
  case AF_INET:
    optlen = 6;
    *bp++ = type;
    *bp++ = optlen;
    memcpy(bp,&sin->sin_addr.s_addr,4); // Already in network order
    bp += 4;
    memcpy(bp,&sin->sin_port,2);
    bp += 2;
    break;
  case AF_INET6:
    optlen = 10;
    *bp++ = type;
    *bp++ = optlen;
    memcpy(bp,&sin6->sin6_addr,8);
    bp += 8;
    memcpy(bp,&sin6->sin6_port,2);
    bp += 2;
    break;
  default:
    return 0; // Invalid, don't encode anything
  }
  *buf = bp;
  return optlen;
}


struct sockaddr *decode_socket(void *sock,unsigned char const *val,int optlen){
  struct sockaddr_in *sin = (struct sockaddr_in *)sock;
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sock;

  if(optlen == 6){
    sin->sin_family = AF_INET;
    memcpy(&sin->sin_addr.s_addr,val,4);
    memcpy(&sin->sin_port,val+4,2);
    return sock;
  } else if(optlen == 10){
    sin6->sin6_family = AF_INET6;
    memcpy(&sin6->sin6_addr,val,8);
    memcpy(&sin6->sin6_port,val+8,2);
    return sock;
  }
  return NULL;
}


// Generate random time uniformly distributed between (now + base, now + base + rrange)
void random_time(struct timespec *tv,unsigned int base,unsigned int rrange){
  struct timespec now;
  clock_gettime(CLOCK_REALTIME,&now);

  tv->tv_sec = now.tv_sec;
  tv->tv_nsec = now.tv_nsec + 1000 * (base + (random() % rrange));
  if(tv->tv_nsec >= 1000000000){
    // Reduce to < 1 billion nanoseconds
    tv->tv_sec += tv->tv_nsec / 1000000000;
    tv->tv_nsec = tv->tv_nsec % 1000000000;
  }
}

// Send empty poll command on specified descriptor
void send_poll(int fd,int ssrc){
  unsigned char cmdbuffer[128];
  unsigned char *bp = cmdbuffer;
  *bp++ = 1; // Command
  if(ssrc != 0)
    encode_int(&bp,OUTPUT_SSRC,ssrc); // poll specific SSRC
  
  uint32_t tag = random();
  encode_int(&bp,COMMAND_TAG,tag);

  encode_eol(&bp);
  int const command_len = bp - cmdbuffer;
  if(send(fd, cmdbuffer, command_len, 0) != command_len)
    perror("poll command send");
}


// Extract SSRC; 0 means not present (reserved value)
int get_ssrc(unsigned char const *buffer,int length){
  unsigned char const *cp = buffer;
  
  while(cp - buffer < length){
    enum status_type const type = *cp++; // increment cp to length field
    
    if(type == EOL)
      break; // end of list, no length
    
    unsigned int const optlen = *cp++;
    if(cp - buffer + optlen >= length)
      break; // invalid length; we can't continue to scan
    
    switch(type){
    case EOL: // Shouldn't get here
      goto done;
    case OUTPUT_SSRC:
      return decode_int(cp,optlen);
      break;
    default:
      break; // Ignore on this pass
    }
    cp += optlen;
  }
 done:;
  return 0; // broadcast
}



