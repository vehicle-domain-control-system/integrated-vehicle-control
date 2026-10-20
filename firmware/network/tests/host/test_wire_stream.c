#include "network/net_wire_uart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY
/* Independent Python binascii.crc_hqx(...,0xffff), fixed structural payloads.
   Embedded SYNC is deliberately opaque. These are not typed DTO examples. */
static const uint8_t golden_c[] = {
 0xa5,0x5a,0x01,0x60,0x13,0x00,0x34,0x12,0xa5,0x5a,0x02,0x03,0x04,0x05,0x06,
 0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x46,0xee};
static const uint8_t golden_m[] = {
 0xa5,0x5a,0x02,0x01,0x26,0x00,0x34,0x12,0x00,0x01,0x02,0x03,0x04,0x05,0x06,
 0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,
 0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,
 0x25,0xef,0x48};
static NetWireUartFrame frame_c(void)
{ NetWireUartFrame f = {0}; f.length = sizeof(golden_c); memcpy(f.bytes, golden_c, f.length); return f; }
static NetWireStatus init(NetWireUartStream *s, NetWireLink link, bool equality)
{ const NetWireStreamPolicy p = {true,50u,equality}; return net_wire_uart_stream_init(s,PROFILE,link,NET_WIRE_UPLINK,&p); }
static NetWireStatus feed(NetWireUartStream *s,const uint8_t *b,const uint64_t *t,
 size_t n,uint64_t now,bool timer,size_t *used,NetWireUartFrame *out)
{ const NetWireStreamInput in = {b,t,n,now,true,timer}; return net_wire_uart_stream_next(s,&in,used,out); }
static int crc_and_golden(void)
{
 uint16_t crc=0u; NetWireUartFrame f=frame_c(),encoded; NetWireUartPayload p;
 CHECK(net_wire_crc16((const uint8_t *)"123456789",9u,&crc)==NET_WIRE_OK && crc==0x29b1u);
 CHECK(net_wire_crc16(NULL,0u,&crc)==NET_WIRE_OK && crc==0xffffu);
 CHECK(net_wire_crc16(NULL,1u,&crc)==NET_WIRE_INVALID_ARGUMENT && crc==0xffffu);
 CHECK(net_wire_crc16(golden_c,NET_WIRE_UART_FRAME_MAX+1u,&crc)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_OK);
 CHECK(p.type==0x60u && p.sequence==0x1234u && p.length==19u && memcmp(p.bytes,golden_c+8u,19u)==0);
 CHECK(net_wire_uart_encode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,0x60u,0x1234u,golden_c+8u,19u,&encoded)==NET_WIRE_OK);
 CHECK(encoded.length==sizeof(golden_c) && memcmp(encoded.bytes,golden_c,sizeof(golden_c))==0);
 f.length=sizeof(golden_m); memcpy(f.bytes,golden_m,f.length);
 CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_M,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_OK);
 CHECK(p.length==38u && p.sequence==0x1234u && memcmp(p.bytes,golden_m+8u,38u)==0);
 return 0;
}
static int frame_rejections(void)
{
 size_t i; NetWireUartFrame f, before=frame_c(); NetWireUartPayload p, old;
 static const size_t offsets[]={0u,1u,2u,3u,4u,5u,8u,27u,28u};
 memset(&p,0x5a,sizeof(p)); old=p;
 for(i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i){
  f=before; f.bytes[offsets[i]]^=0x80u;
  CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)!=NET_WIRE_OK);
  CHECK(memcmp(&p,&old,sizeof(p))==0);
 }
 f=before;
 CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_DOWNLINK,&f,&p)==NET_WIRE_MALFORMED);
 CHECK(net_wire_uart_decode(NET_WIRE_NOT_BOUND,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_NOT_BOUND_STATUS);
 CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_M,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_MALFORMED);
 f.length=0u; CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_MALFORMED);
 f=before; --f.length; CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_MALFORMED);
 f=before; ++f.length; CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_MALFORMED);
 f=before; f.length=139u; CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_MALFORMED);
 f=before; f.bytes[4]=0u; CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_MALFORMED);
 f=before; f.bytes[4]=128u; CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_MALFORMED);
 f=before; f.bytes[4]=0u;f.bytes[5]=19u; CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_MALFORMED);
 f.length=41u; memcpy(f.bytes,golden_m,sizeof(golden_m)); f.bytes[4]=31u;
 CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_M,NET_WIRE_UPLINK,&f,&p)==NET_WIRE_MALFORMED);
 CHECK(memcmp(&p,&old,sizeof(p))==0);
 return 0;
}
static int frame_arguments(void)
{
 NetWireUartFrame f=frame_c(),old=f; uint8_t bytes[129]={0}; uint16_t crc;
 union { NetWireUartFrame frame; NetWireUartPayload payload; uint16_t crc; } alias;
 CHECK(net_wire_uart_encode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,0x60,1,bytes,129u,&f)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(net_wire_uart_encode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,0x60,1,bytes,128u,&f)==NET_WIRE_MALFORMED);
 CHECK(net_wire_uart_encode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,0x60,1,bytes,0u,&f)==NET_WIRE_MALFORMED);
 CHECK(net_wire_uart_encode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,0x60,1,NULL,19u,&f)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(net_wire_uart_encode(PROFILE,NET_WIRE_UART_C,NET_WIRE_DOWNLINK,0x60,1,bytes,19u,&f)==NET_WIRE_MALFORMED);
 CHECK(net_wire_uart_encode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,0xff,1,bytes,19u,&f)==NET_WIRE_UNSUPPORTED);
 CHECK(net_wire_uart_encode(NET_WIRE_NOT_BOUND,NET_WIRE_UART_C,NET_WIRE_UPLINK,0x60,1,bytes,19u,&f)==NET_WIRE_NOT_BOUND_STATUS);
 CHECK(net_wire_uart_encode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,0x60,1,f.bytes,19u,&f)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(memcmp(&f,&old,sizeof(f))==0);
 alias.frame=f;
 CHECK(net_wire_uart_decode(PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&alias.frame,&alias.payload)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(memcmp(&alias.frame,&f,sizeof(f))==0);
 CHECK(net_wire_crc16((const uint8_t *)&alias,sizeof(uint16_t),&alias.crc)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(net_wire_crc16(bytes,1u,NULL)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(net_wire_crc16(bytes,1u,&crc)==NET_WIRE_OK);
 return 0;
}
static int fragments_and_isolation(void)
{
 NetWireUartStream c,m; NetWireUartFrame out,old; size_t i,used;
 CHECK(init(&c,NET_WIRE_UART_C,false)==NET_WIRE_OK); CHECK(init(&m,NET_WIRE_UART_M,false)==NET_WIRE_OK);
 memset(&out,0x5a,sizeof(out)); old=out;
 for(i=0;i<sizeof(golden_c);++i){
  uint64_t t=100u+i;
  CHECK(feed(&c,golden_c+i,&t,1u,99999u,false,&used,&out)==(i+1u==sizeof(golden_c)?NET_WIRE_OK:NET_WIRE_NEED_MORE));
  CHECK(used==1u); if(i+1u<sizeof(golden_c)) CHECK(memcmp(&out,&old,sizeof(out))==0);
 }
 CHECK(out.captured_at_ms==128u && memcmp(out.bytes,golden_c,sizeof(golden_c))==0 && m.count==0u);
 for(i=0;i<sizeof(golden_m);++i){uint64_t t=200u+i;
  CHECK(feed(&m,golden_m+i,&t,1u,99999u,false,&used,&out)==(i+1u==sizeof(golden_m)?NET_WIRE_OK:NET_WIRE_NEED_MORE));}
 CHECK(out.captured_at_ms==247u && c.last_capture_ms==128u);
 return 0;
}
static int concatenation_and_resync(void)
{
 uint8_t bytes[128]; uint64_t times[128]; NetWireUartStream s; NetWireUartFrame out; size_t i,used,offset;
 CHECK(init(&s,NET_WIRE_UART_C,false)==NET_WIRE_OK);
 bytes[0]=0xa5;memcpy(bytes+1u,golden_c,sizeof(golden_c));bytes[1u+27u]^=1u;
 memcpy(bytes+1u+sizeof(golden_c),golden_c,sizeof(golden_c));
 memcpy(bytes+1u+2u*sizeof(golden_c),golden_c,sizeof(golden_c));
 for(i=0;i<1u+3u*sizeof(golden_c);++i)times[i]=100u;
 CHECK(feed(&s,bytes,times,1u+3u*sizeof(golden_c),90000u,true,&used,&out)==NET_WIRE_OK);
 CHECK(used==1u+2u*sizeof(golden_c) && out.captured_at_ms==100u && s.bad_crc==1u && s.discarded_bytes>0u);
 offset=used;
 CHECK(feed(&s,bytes+offset,times+offset,1u+3u*sizeof(golden_c)-offset,90000u,true,&used,&out)==NET_WIRE_OK);
 CHECK(used==sizeof(golden_c) && memcmp(out.bytes,golden_c,sizeof(golden_c))==0);
 /* Impossible exact length rejected at header, without waiting for fake body. */
 CHECK(init(&s,NET_WIRE_UART_C,false)==NET_WIRE_OK);
 memcpy(bytes,golden_c,8u);bytes[4]=128u;memcpy(bytes+8u,golden_c,sizeof(golden_c));
 CHECK(feed(&s,bytes,times,8u+sizeof(golden_c),100u,true,&used,&out)==NET_WIRE_OK && s.bad_headers==1u);
 return 0;
}
static int expiry_and_original_times(void)
{
 NetWireUartStream s; NetWireUartFrame out; uint64_t t[80]; uint8_t b[80]; size_t i,used;
 for(i=0;i<sizeof(golden_c);++i)t[i]=i==0u?100u:150u;
 CHECK(init(&s,NET_WIRE_UART_C,false)==NET_WIRE_OK);
 CHECK(feed(&s,golden_c,t,sizeof(golden_c),9999u,true,&used,&out)==NET_WIRE_OK && out.captured_at_ms==150u);
 CHECK(init(&s,NET_WIRE_UART_C,true)==NET_WIRE_OK);
 CHECK(feed(&s,golden_c,t,sizeof(golden_c),150u,true,&used,&out)==NET_WIRE_NEED_MORE && s.assembly_timeouts>0u);
 /* CRC-invalid outer frame contains a valid frame suffix. Original times
    of that suffix must survive one-byte resync; 51ms cannot become fresh. */
 CHECK(init(&s,NET_WIRE_UART_C,false)==NET_WIRE_OK);
 memcpy(b,golden_c,8u);memcpy(b+8u,golden_c,sizeof(golden_c));
 for(i=0;i<8u+sizeof(golden_c);++i)t[i]=100u;
 t[8u+sizeof(golden_c)-1u]=151u;
 CHECK(feed(&s,b,t,8u+sizeof(golden_c),151u,true,&used,&out)==NET_WIRE_NEED_MORE && s.bad_crc>0u && s.assembly_timeouts>0u);
 CHECK(init(&s,NET_WIRE_UART_C,false)==NET_WIRE_OK); t[0]=100u;
 CHECK(feed(&s,golden_c,t,1u,100u,true,&used,&out)==NET_WIRE_NEED_MORE);
 CHECK(feed(&s,NULL,NULL,0u,150u,true,&used,&out)==NET_WIRE_NEED_MORE && s.count==1u);
 CHECK(feed(&s,NULL,NULL,0u,151u,true,&used,&out)==NET_WIRE_NEED_MORE && s.count==0u && s.assembly_timeouts==1u);
 return 0;
}
static int stream_atomicity(void)
{
 NetWireUartStream s,old; NetWireUartFrame out,before; size_t used=77u; uint64_t times[2]={101u,100u};
 NetWireStreamInput in={golden_c,times,2u,101u,true,true}; NetWireStreamPolicy policy={false,50u,false};
 memset(&s,0,sizeof(s)); old=s;
 CHECK(net_wire_uart_stream_init(&s,PROFILE,NET_WIRE_UART_C,NET_WIRE_UPLINK,&policy)==NET_WIRE_CONTRACT_HOLD);
 CHECK(memcmp(&s,&old,sizeof(s))==0);
 policy.candidate_time_policy_selected=true;
 CHECK(net_wire_uart_stream_init(&s,NET_WIRE_NOT_BOUND,NET_WIRE_UART_C,NET_WIRE_UPLINK,&policy)==NET_WIRE_NOT_BOUND_STATUS);
 CHECK(init(&s,NET_WIRE_UART_C,false)==NET_WIRE_OK); old=s;memset(&out,0x5a,sizeof(out));before=out;
 CHECK(net_wire_uart_stream_next(&s,&in,&used,&out)==NET_WIRE_TIME_HOLD);
 CHECK(used==77u && memcmp(&s,&old,sizeof(s))==0 && memcmp(&out,&before,sizeof(out))==0);
 in.length=1u;in.timestamps_unambiguous=false;
 CHECK(net_wire_uart_stream_next(&s,&in,&used,&out)==NET_WIRE_TIME_HOLD);
 in.timestamps_unambiguous=true;in.now_ms=100u;
 CHECK(net_wire_uart_stream_next(&s,&in,&used,&out)==NET_WIRE_TIME_HOLD);
 in.length=513u; CHECK(net_wire_uart_stream_next(&s,&in,&used,&out)==NET_WIRE_INVALID_ARGUMENT);
 in.length=1u;in.now_ms=101u;in.bytes=s.bytes;
 CHECK(net_wire_uart_stream_next(&s,&in,&used,&out)==NET_WIRE_INVALID_ARGUMENT);
 in.bytes=golden_c;
 CHECK(net_wire_uart_stream_next(&s,&in,&s.count,&out)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(net_wire_uart_stream_next(&s,&in,&out.length,&out)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(memcmp(&s,&old,sizeof(s))==0 && memcmp(&out,&before,sizeof(out))==0);
 CHECK(net_wire_uart_stream_next(NULL,&in,&used,&out)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(net_wire_uart_stream_next(&s,NULL,&used,&out)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(net_wire_uart_stream_next(&s,&in,NULL,&out)==NET_WIRE_INVALID_ARGUMENT);
 CHECK(net_wire_uart_stream_next(&s,&in,&used,NULL)==NET_WIRE_INVALID_ARGUMENT);
 in.length=0u;in.bytes=NULL;in.captured_at_ms=NULL;in.now_ms=140u;
 CHECK(net_wire_uart_stream_next(&s,&in,&used,&out)==NET_WIRE_NEED_MORE);
 old=s;used=77u;in.now_ms=130u;
 CHECK(net_wire_uart_stream_next(&s,&in,&used,&out)==NET_WIRE_TIME_HOLD);
 CHECK(used==77u && memcmp(&s,&old,sizeof(s))==0 && memcmp(&out,&before,sizeof(out))==0);
 CHECK(net_wire_uart_stream_reset(&s,true)==NET_WIRE_OK);old=s;
 in.bytes=golden_c;in.captured_at_ms=times;in.length=1u;in.now_ms=150u;
 CHECK(net_wire_uart_stream_next(&s,&in,&used,&out)==NET_WIRE_TIME_HOLD && memcmp(&s,&old,sizeof(s))==0);
 return 0;
}
static int reset_and_saturation(void)
{
 NetWireUartStream s; NetWireUartFrame out;size_t used;uint64_t t[29];uint8_t b[29];size_t i;
 CHECK(init(&s,NET_WIRE_UART_C,false)==NET_WIRE_OK);
 for(i=0;i<29u;++i)t[i]=100u;
 CHECK(feed(&s,golden_c,t,5u,100u,true,&used,&out)==NET_WIRE_NEED_MORE);
 s.byte_losses=UINT32_MAX; CHECK(net_wire_uart_stream_reset(&s,true)==NET_WIRE_OK && s.count==0u && s.has_last_capture && s.byte_losses==UINT32_MAX);
 s.discarded_bytes=UINT32_MAX;s.bad_headers=UINT32_MAX;s.bad_crc=UINT32_MAX;s.assembly_timeouts=UINT32_MAX;
 memcpy(b,golden_c,29u);b[27]^=1u;
 CHECK(feed(&s,b,t,29u,100u,true,&used,&out)==NET_WIRE_NEED_MORE && s.bad_crc==UINT32_MAX && s.discarded_bytes==UINT32_MAX);
 b[4]=0u; CHECK(feed(&s,b,t,29u,100u,true,&used,&out)==NET_WIRE_NEED_MORE && s.bad_headers==UINT32_MAX);
 CHECK(feed(&s,golden_c,t,1u,100u,true,&used,&out)==NET_WIRE_NEED_MORE);
 CHECK(feed(&s,NULL,NULL,0u,151u,true,&used,&out)==NET_WIRE_NEED_MORE && s.assembly_timeouts==UINT32_MAX);
 return 0;
}
static uint32_t random_state=UINT32_C(0x5eeda731);
static uint32_t next_random(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static int deterministic_stream_replay(void)
{
 uint8_t b[256];uint64_t t[256];NetWireUartStream s;NetWireUartFrame out;size_t round,total=0u;
 CHECK(init(&s,NET_WIRE_UART_C,false)==NET_WIRE_OK);
 for(round=0u;round<3000u;++round){
  size_t noise=next_random()%96u,n,i,pos=0u,frames=0u;
  for(i=0u;i<noise;++i){b[i]=(uint8_t)next_random(); if(b[i]==0xa5u)b[i]=0x00u;}
  memcpy(b+noise,golden_c,sizeof(golden_c));b[noise+27u]^=1u;
  memcpy(b+noise+sizeof(golden_c),golden_c,sizeof(golden_c));n=noise+2u*sizeof(golden_c);
  for(i=0u;i<n;++i)t[i]=round;
  while(pos<n){size_t used=0u,chunk=1u+next_random()%19u;NetWireStatus status;
   if(chunk>n-pos)chunk=n-pos;
   status=feed(&s,b+pos,t+pos,chunk,round,false,&used,&out);
   CHECK(status==NET_WIRE_OK || status==NET_WIRE_NEED_MORE);CHECK(used>0u && used<=chunk);pos+=used;
   if(status==NET_WIRE_OK){++frames;CHECK(out.length==sizeof(golden_c) && memcmp(out.bytes,golden_c,sizeof(golden_c))==0 && out.captured_at_ms==round);}
  }
  CHECK(frames==1u);total+=frames;
 }
 printf("candidate stream replay seed=0x5eeda731 rounds=3000 accepted=%lu\n",(unsigned long)total);
 return 0;
}
typedef int (*Test)(void);
int main(int argc,char **argv)
{
 static const struct {const char *name;Test run;} cases[]={
 {"crc_and_golden",crc_and_golden},{"frame_rejections",frame_rejections},{"frame_arguments",frame_arguments},
 {"fragments_and_isolation",fragments_and_isolation},{"concatenation_and_resync",concatenation_and_resync},
 {"expiry_and_original_times",expiry_and_original_times},{"stream_atomicity",stream_atomicity},
 {"reset_and_saturation",reset_and_saturation},{"deterministic_stream_replay",deterministic_stream_replay}};
 size_t i;for(i=0u;i<sizeof(cases)/sizeof(cases[0]);++i)if(argc==1 || strcmp(argv[1],cases[i].name)==0){int r=cases[i].run();if(r)return r;if(argc>1)return 0;}return argc==1?0:2;
}
