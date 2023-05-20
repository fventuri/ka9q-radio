// $Id: status.c,v 1.31 2022/08/05 06:35:10 karn Exp $
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
#include <sys/un.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdint.h>
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


// Encode 64-bit integer, big endian, leading zeroes suppressed
// The nice thing about big-endian encoding with suppressed leading zeroes
// is that all (unsigned) integer types can be easily encoded
// by simply casting them to uint64_t, without wasted space
int encode_int64(uint8_t **buf,enum status_type type,uint64_t x){
  uint8_t *cp = *buf;

  *cp++ = type;

  if(x == 0){
    // Compress zero value to zero length
    *cp++ = 0;
    *buf = cp;
    return 2;
  }

  int len = sizeof(x);
  while(len > 0 && ((x >> 56) == 0)){
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


// Special case: single null type byte means end of list
int encode_eol(uint8_t **buf){
  uint8_t *bp = *buf;

  *bp++ = EOL;
  *buf = bp;
  return 1;
}

int encode_byte(uint8_t **buf,enum status_type type,uint8_t x){
  uint8_t *cp = *buf;
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

int encode_int16(uint8_t **buf,enum status_type type,uint16_t x){
  return encode_int64(buf,type,(uint64_t)x);
}

int encode_int32(uint8_t **buf,enum status_type type,uint32_t x){
  return encode_int64(buf,type,(uint64_t)x);
}

int encode_int(uint8_t **buf,enum status_type type,int x){
  return encode_int64(buf,type,(uint64_t)x);
}


// Floating types are also byte-swapped to big-endian order
int encode_float(uint8_t **buf,enum status_type type,float x){
  if(isnan(x))
    return 0; // Never encode a NAN

  union result r;
  r.f = x;
  return encode_int32(buf,type,r.l);
}

int encode_double(uint8_t **buf,enum status_type type,double x){
  if(isnan(x))
    return 0; // Never encode a NAN

  union result r;
  r.d = x;
  return encode_int64(buf,type,r.ll);
}

// Encode byte string without byte swapping
int encode_string(uint8_t **bp,enum status_type const type,void const *buf,unsigned int const buflen){
  uint8_t const *orig_bpp = *bp;
  uint8_t *cp = *bp;
  *cp++ = type;

  if(buflen < 128){
    // send length directly
    *cp++ = buflen;
  } else if(buflen < 65536){
    // Length is 2 bytes, big endian
    *cp++ = 0x80 | 2;
    *cp++ = buflen >> 8;
    *cp++ = buflen;
  } else if(buflen < 16777216){
    *cp++ = 0x80 | 3;
    *cp++ = buflen >> 16;
    *cp++ = buflen >> 8;
    *cp++ = buflen;
  } else { // Handle more than 4 GB??
    *cp++ = 0x80 | 4;
    *cp++ = buflen >> 24;
    *cp++ = buflen >> 16;
    *cp++ = buflen >> 8;
    *cp++ = buflen;
  }
  memcpy(cp,buf,buflen);
  cp += buflen;
  *bp = cp;
  return cp - orig_bpp;
}
// Unique to spectrum energies
// array -> vector of 32-bit floats
// size = number of floats
// Sent in big endian order just like other floats
// Because it can be very long, handle large sizes
int encode_vector(uint8_t **bp,enum status_type type,float *array,int size){
  uint8_t const *orig_bp = *bp;
  uint8_t *cp = *bp;
  *cp++ = type;

  int const bytes = sizeof(*array) * size; // Number of bytes in data
  if(bytes < 128){
    *cp++ = bytes;    // Send length directly
  } else if(bytes < 65536){
    *cp++ = 0x80 | 2; // length takes 2 bytes
    *cp++ = bytes >> 8;
    *cp++ = bytes;
  } else if(bytes < 16777216){
    *cp++ = 0x80 | 3;
    *cp++ = bytes >> 16;
    *cp++ = bytes >> 8;
    *cp++ = bytes;
  } else {
    *cp++ = 0x80 | 4;
    *cp++ = bytes >> 24;
    *cp++ = bytes >> 16;
    *cp++ = bytes >> 8;
    *cp++ = bytes;
  }
  // Encode the individual array elements
  // Right now they're DC....maxpositive maxnegative...minnegative
  for(int i=0;i < size;i++){
    // Swap but don't bother compressing leading zeroes for now
    union {
      uint32_t i;
      float f;
    } foo;
    foo.f = array[i];
    *cp++ = foo.i >> 24;
    *cp++ = foo.i >> 16;
    *cp++ = foo.i >> 8;
    *cp++ = foo.i;
  }
  *bp = cp;
  return cp - orig_bp;
}



// Decode byte string without byte swapping
// NB! optlen has already been 'fixed' by the caller in case it's >= 128
char *decode_string(uint8_t const *cp,int optlen,char *buf,int buflen){
  int n = min(optlen,buflen-1);
  memcpy(buf,cp,n);
  buf[n] = '\0'; // force null termination
  return buf;
}


// Decode encoded variable-length UNSIGNED integers
// At entry, *bp -> length field (not type!)
// Works for byte, short/int16_t, long/int32_t, long long/int64_t
uint64_t decode_int(uint8_t const *cp,int len){
  uint64_t result = 0;
  // cp now points to beginning of abbreviated int
  // Byte swap as we accumulate
  while(len-- > 0)
    result = (result << 8) | *cp++;

  return result;
}


float decode_float(uint8_t const *cp,int len){
  if(len == 0)
    return 0;
  
  if(len == 8)
    return (float)decode_double(cp,len);

  union result r;
  r.ll = decode_int(cp,len);
  return r.f;
}

double decode_double(uint8_t const *cp,int len){
  if(len == 0)
    return 0;
  
  if(len == 4)
    return (double)decode_float(cp,len);

  union result r;
  r.ll = decode_int(cp,len);
  return r.d;
}

// The Linux/UNIX socket data structures are a real mess...
int encode_socket(uint8_t **buf,enum status_type type,void const *sock){
  struct sockaddr_in const *sin = sock;
  struct sockaddr_in6 const *sin6 = sock;
  struct sockaddr_un const *sun = sock;
  uint8_t *bp = *buf;
  int optlen = 0;

  switch(sin->sin_family){
  case AF_UNIX:
    optlen = strlen(sun->sun_path) + 1;
    *bp++ = type;
    *bp++ = optlen; // TEST IF IT's MORE THAN 128 <- fixxs
    memcpy(bp,&sun->sun_path,optlen);
    bp += optlen;
    break;
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


struct sockaddr *decode_socket(void *sock,uint8_t const *val,int optlen){
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
// AF_UNIX (or AF_LOCAL) from a string with the path
struct sockaddr *decode_local_socket(void *sock,uint8_t const *val,int optlen){
  struct sockaddr_un *sun = (struct sockaddr_un *)sock;
  sun->sun_family = AF_UNIX;
  memcpy(sun->sun_path,val,optlen);
  if(optlen < sizeof(sun->sun_path))
    sun->sun_path[optlen] = '\0'; // ensure null termination

  return sock;
}

// Generate random GPS time uniformly distributed between (now + base, now + base + rrange)
// Args are in nanosec
int64_t random_time(int64_t base,int64_t rrange){
  return gps_time_ns() + base + random() % rrange;
}

// Send empty poll command on specified descriptor
// Return command tag
uint32_t send_poll(int fd,int ssrc){
  uint8_t cmdbuffer[128];
  uint8_t *bp = cmdbuffer;
  *bp++ = 1; // Command
  if(ssrc != 0)
    encode_int(&bp,OUTPUT_SSRC,ssrc); // poll specific SSRC
  
  uint32_t tag = random();
  encode_int(&bp,COMMAND_TAG,tag);

  encode_eol(&bp);
  int const command_len = bp - cmdbuffer;
  if(send(fd, cmdbuffer, command_len, 0) != command_len)
    perror("poll command send");
  return tag;
}


// Extract SSRC; 0 means not present (reserved value)
uint32_t get_ssrc(uint8_t const *buffer,int length){
  uint8_t const *cp = buffer;
  
  while(cp - buffer < length){
    enum status_type const type = *cp++; // increment cp to length field
    
    if(type == EOL)
      break; // end of list, no length
    
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
  return 0;
}
// Extract command tag
uint32_t get_tag(uint8_t const *buffer,int length){
  uint8_t const *cp = buffer;
  
  while(cp - buffer < length){
    enum status_type const type = *cp++; // increment cp to length field
    
    if(type == EOL)
      break; // end of list, no length
    
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
      break; // invalid length; we can't continue to scan
    
    switch(type){
    case EOL: // Shouldn't get here
      goto done;
    case COMMAND_TAG:
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



