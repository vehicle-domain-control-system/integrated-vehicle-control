#include "network/net_wire_can.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int nib(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static int check(const char *name,int is_permission,const char *hex){
 NetWireCanEnvelope orig={0},round={0}; NetWireStatus a,b;
 size_t i;
 orig.standard_id=is_permission?0x080u:0x0A0u;orig.fdf=true;orig.brs=true;orig.dlc=11;orig.length=20;
 if(strlen(hex)!=40){fprintf(stderr,"BAD_LEN %s\n",name);return 1;}
 for(i=0;i<20;i++){int h=nib(hex[2*i]),l=nib(hex[2*i+1]);if(h<0||l<0)return 2;orig.bytes[i]=(uint8_t)((h<<4)|l);}
 if(is_permission){NetWireDomainPermissionRaw dto={0};a=net_wire_can_decode_permission(NET_WIRE_CANDIDATE_ONLY,&orig,&dto);if(a==NET_WIRE_OK)b=net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY,&dto,&round);else b=a;}
 else{NetWireCisRearRaw dto={0};a=net_wire_can_decode_rear(NET_WIRE_CANDIDATE_ONLY,&orig,&dto);if(a==NET_WIRE_OK)b=net_wire_can_encode_rear(NET_WIRE_CANDIDATE_ONLY,&dto,&round);else b=a;}
 if(a!=NET_WIRE_OK||b!=NET_WIRE_OK||memcmp(orig.bytes,round.bytes,20)!=0){fprintf(stderr,"FAIL %s decode %d encode %d\n",name,a,b);return 3;}
 printf("PASS historical I1 CAN %s\n",name);return 0;
}
int main(void){
 static const struct{const char *name;int permission;const char *hex;} a[]={
 {"R01_REAR_LE",0,"4433221188776655040302013412785601000000"},
 {"R02_NO_OBJECT",0,"0100000002000000030000006400ffff01000100"},
 {"R03_NO_DATA_MAX",0,"ffffffffffffffffffffffffffffffff00060200"},
 {"R04_ZERO_COUNTERS",0,"0100000000000000000000000000000001000000"},
 {"R05_FAULT_PRESERVE",0,"010000000300000009000000fffff00000030300"},
 {"D01_PERMISSION_LE",1,"efcdab89403020100100ff000344332211665500"},
 {"D02_UNKNOWN",1,"0100000000000000ff03ff000300000000ffff00"},
 {"D03_STALE_RAW_ALLOW_BITS",1,"ffffffffffffffff0101010102ffffffffffff00"},
 {"D04_ZERO_FIELDS",1,"0100000000000000000000000000000000000000"}
 };
 unsigned i;for(i=0;i<sizeof(a)/sizeof(a[0]);i++)if(check(a[i].name,a[i].permission,a[i].hex))return 1;
 printf("PASS all %zu independent Git-tree historical I1 CAN vectors (data-only, not adoption)\n",sizeof(a)/sizeof(a[0]));return 0;
}
