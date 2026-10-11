#include "u3_raw_diag.h"
#include "independent_for_c.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures, case_checks, case_failures;
static const char *current;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr, "%s line %d: %s\n", current, __LINE__, #x); } } while (0)
static void begin(const char *id) { current=id; case_checks=checks; case_failures=failures; }
static void end(void) { printf("CASE %s %s checks=%u failures=%u\n",current,failures==case_failures?"PASS":"FAIL",checks-case_checks,failures-case_failures); }

static u3_raw_frame frame(uint32_t id, const uint8_t *p)
{
    u3_raw_frame f={0}; f.can_id=id; f.fdf=1; f.brs=1;
    f.dlc=11; f.payload=p; f.length=20; f.received_at_ms=1234;
    return f;
}
static void reject(const u3_raw_frame *f, u3_status wanted)
{
    struct { unsigned char left[8]; u3_diagnostic d; unsigned char right[8]; } g, before;
    memset(&g,0xCD,sizeof g); memcpy(&before,&g,sizeof g);
    CHECK(u3_raw_decode(f,&g.d)==wanted);
    CHECK(memcmp(&g,&before,sizeof g)==0);
}
static void same_bytes(const u3_diagnostic *d, const uint8_t *expected)
{
    uint8_t guarded[22]; size_t n=99;
    memset(guarded,0xCD,sizeof guarded);
    CHECK(u3_raw_encode(d,guarded+1,20,&n)==U3_OK);
    CHECK(n==20 && memcmp(guarded+1,expected,20)==0);
    CHECK(guarded[0]==0xCD && guarded[21]==0xCD);
}
static void bad_encode(const u3_diagnostic *d, u3_status wanted)
{
    uint8_t out[22], before[22]; size_t n=99;
    char text[1024], text_before[1024];
    memset(out,0xCD,sizeof out); memcpy(before,out,sizeof out);
    CHECK(u3_raw_encode(d,out+1,20,&n)==wanted);
    CHECK(n==0 && memcmp(out,before,sizeof out)==0);
    memset(text,0xCD,sizeof text); memcpy(text_before,text,sizeof text);
    CHECK(u3_raw_format(d,text,sizeof text,&n)==wanted);
    CHECK(n==0 && memcmp(text,text_before,sizeof text)==0);
}

int main(void)
{
    u3_raw_frame f; u3_diagnostic rear, permission, d, second;
    uint8_t b[21]; char text[1024], other[1024]; size_t n, m;
    unsigned i,j;

    begin("T01"); f=frame(0xA0,ref_rear);
    CHECK(u3_raw_decode(&f,&rear)==U3_OK);
    CHECK(rear.can_id==0xA0 && rear.ide==0 && rear.fdf==1 && rear.brs==1 && rear.rtr==0 && rear.dlc==11);
    CHECK(rear.received_at_ms==1234 && memcmp(rear.raw,ref_rear,20)==0);
    CHECK(rear.fields.rear.boot_id==UINT32_C(0x11223344));
    CHECK(rear.fields.rear.tx_sequence==UINT32_C(0x01020304));
    CHECK(rear.fields.rear.update_sequence==9 && rear.fields.rear.source_age_ms==300);
    CHECK(rear.fields.rear.rear_distance_deci_cm==400 && rear.fields.rear.validity==1);
    CHECK(rear.fields.rear.quality_reason==0 && rear.fields.rear.proximity_status==0);
    same_bytes(&rear,ref_rear);
    CHECK(u3_raw_format(&rear,text,sizeof text,&n)==U3_OK);
    CHECK(strstr(text,"distance_raw_0_1cm=400")!=NULL && strstr(text,"proximity=0(VALID_DISTANCE)")!=NULL);
    CHECK(strstr(text,"source_age_ms=300")!=NULL && strstr(text,"degraded=0")!=NULL);
    printf("DIAGNOSTIC %s\n",text); end();

    begin("T02"); f=frame(0x80,ref_permission);
    CHECK(u3_raw_decode(&f,&permission)==U3_OK);
    CHECK(permission.fields.permission.boot_id==UINT32_C(0x11223344));
    CHECK(permission.fields.permission.tx_sequence==UINT32_C(0x01020304));
    CHECK(permission.fields.permission.update_sequence==1 && permission.fields.permission.source_age_ms==65535);
    CHECK(permission.fields.permission.vehicle_power_permission==255 && permission.fields.permission.power_permission_quality==3);
    CHECK(permission.fields.permission.power_operation_state==255 && permission.fields.permission.operation_allowed==0 && permission.fields.permission.operation_quality==3);
    same_bytes(&permission,ref_permission);
    CHECK(u3_raw_format(&permission,text,sizeof text,&n)==U3_OK);
    CHECK(strstr(text,"power_raw=255(UNKNOWN)")!=NULL && strstr(text,"operation_quality=3(NO_DATA)")!=NULL);
    CHECK(strstr(text,"SATURATED_LOWER_BOUND")!=NULL && strstr(text,"policy=UNRESOLVED_CONTRACT")!=NULL);
    CHECK(strstr(text,"degraded=1")!=NULL && strstr(text,"ALLOW ")==NULL && strstr(text,"DENY")==NULL);
    printf("DIAGNOSTIC %s\n",text); end();

    begin("T03"); f=frame(0xA1,ref_rear); reject(&f,U3_ERR_UNSUPPORTED);
    f.can_id=0; reject(&f,U3_ERR_UNSUPPORTED); end();
    begin("T04"); f=frame(0x800,ref_rear); reject(&f,U3_ERR_ID);
    f.can_id=UINT32_MAX; reject(&f,U3_ERR_ID);
    f=frame(0xA0,ref_rear); f.ide=1; reject(&f,U3_ERR_ID); end();
    begin("T05"); f=frame(0xA0,ref_rear); f.fdf=0; reject(&f,U3_ERR_PROFILE);
    f.fdf=2; reject(&f,U3_ERR_PROFILE); end();
    begin("T06"); f=frame(0x80,ref_permission); f.brs=0; reject(&f,U3_ERR_PROFILE);
    f.brs=2; reject(&f,U3_ERR_PROFILE); end();
    begin("T07"); f=frame(0xA0,ref_rear); f.rtr=1; reject(&f,U3_ERR_PROFILE);
    f.rtr=2; reject(&f,U3_ERR_PROFILE); end();
    begin("T08"); f=frame(0xA0,ref_rear);
    for(i=0;i<256;++i) { f.dlc=(uint8_t)i; if(i!=11) reject(&f,U3_ERR_DLC); }
    end();
    begin("T09"); f=frame(0xA0,ref_rear);
    for(i=0;i<20;++i) { f.length=i; reject(&f,U3_ERR_LENGTH); }
    f=frame(0x80,ref_permission); f.length=19; reject(&f,U3_ERR_LENGTH); end();
    begin("T10"); f=frame(0xA0,ref_rear); f.length=21; reject(&f,U3_ERR_LENGTH);
    f.length=(size_t)-1; reject(&f,U3_ERR_LENGTH); end();
    begin("T11"); memcpy(b,ref_rear,20); f=frame(0xA0,b);
    for(i=1;i<256;++i) { b[19]=(uint8_t)i; reject(&f,U3_ERR_PADDING); }
    memcpy(b,ref_permission,20); f=frame(0x80,b); b[19]=1; reject(&f,U3_ERR_PADDING); end();

    begin("T12");
    /* Independent code tables from candidate source, no I1-generated oracle. */
    for(j=16;j<=18;++j) for(i=0;i<256;++i) {
        unsigned max=j==16?1:j==17?9:5;
        memcpy(b,ref_rear,20); b[j]=(uint8_t)i; f=frame(0xA0,b);
        if(i>max) reject(&f,U3_ERR_VALUE);
        else { CHECK(u3_raw_decode(&f,&d)==U3_OK); CHECK(d.raw[j]==i); same_bytes(&d,b); }
    }
    for(j=8;j<=12;++j) for(i=0;i<256;++i) {
        int valid=(j==8 || j==10)?(i==0 || i==1 || i==255):(j==11?i<=1:i<=3);
        memcpy(b,ref_permission,20); b[j]=(uint8_t)i; f=frame(0x80,b);
        if(!valid) reject(&f,U3_ERR_VALUE);
        else { CHECK(u3_raw_decode(&f,&d)==U3_OK); CHECK(d.raw[j]==i); same_bytes(&d,b); }
    }
    memcpy(b,ref_rear,20); memset(b,0,4); f=frame(0xA0,b); reject(&f,U3_ERR_RESERVED);
    memcpy(b,ref_permission,20); memset(b,0,4); f=frame(0x80,b); reject(&f,U3_ERR_RESERVED);
    d=rear; d.fields.rear.validity=2; bad_encode(&d,U3_ERR_VALUE);
    d=rear; d.fields.rear.quality_reason=10; bad_encode(&d,U3_ERR_VALUE);
    d=rear; d.fields.rear.proximity_status=6; bad_encode(&d,U3_ERR_VALUE);
    d=rear; d.fields.rear.boot_id=0; bad_encode(&d,U3_ERR_RESERVED);
    d=permission; d.fields.permission.vehicle_power_permission=2; bad_encode(&d,U3_ERR_VALUE);
    d=permission; d.fields.permission.power_permission_quality=4; bad_encode(&d,U3_ERR_VALUE);
    d=permission; d.fields.permission.power_operation_state=2; bad_encode(&d,U3_ERR_VALUE);
    d=permission; d.fields.permission.operation_allowed=2; bad_encode(&d,U3_ERR_VALUE);
    d=permission; d.fields.permission.operation_quality=4; bad_encode(&d,U3_ERR_VALUE);
    d=permission; d.fields.permission.boot_id=0; bad_encode(&d,U3_ERR_RESERVED); end();

    begin("T13");
    for(i=0;i<6;++i) {
        static const char *const names[]={"VALID_DISTANCE","NO_OBJECT","UNAVAILABLE","FAULT","RECOVERING","INACTIVE"};
        memcpy(b,ref_rear,20); b[18]=(uint8_t)i;
        if(i!=0) { b[14]=255; b[15]=255; }
        if(i>=2) { b[16]=0; b[17]=(uint8_t)(i==3?3:i==4?8:6); }
        f=frame(0xA0,b); CHECK(u3_raw_decode(&f,&d)==U3_OK);
        CHECK(d.fields.rear.proximity_status==i && d.fields.rear.validity==b[16]);
        CHECK(u3_raw_format(&d,text,sizeof text,&n)==U3_OK && strstr(text,names[i])!=NULL);
        CHECK(strstr(text,"CLEAR")==NULL); same_bytes(&d,b);
    } end();

    begin("T14");
    for(i=0;i<=1;++i) {
        memcpy(b,ref_permission,20); b[8]=(uint8_t)i; b[9]=0;
        b[10]=0; b[11]=0; b[12]=0; b[17]=0; b[18]=0;
        f=frame(0x80,b); CHECK(u3_raw_decode(&f,&d)==U3_OK);
        CHECK(d.fields.permission.vehicle_power_permission==i && d.fields.permission.power_permission_quality==0);
        CHECK(u3_raw_format(&d,text,sizeof text,&n)==U3_OK);
        CHECK(strstr(text,"RAW_BIT")!=NULL && strstr(text,"policy=UNRESOLVED_CONTRACT")!=NULL);
        CHECK(strstr(text,"DENY")==NULL && strstr(text,"policy=ALLOW")==NULL);
        same_bytes(&d,b);
    } end();

    begin("T15");
    for(i=65534;i<=65535;++i) for(j=0;j<2;++j) {
        unsigned offset=j?17:12;
        memcpy(b,j?ref_permission:ref_rear,20); b[offset]=(uint8_t)i; b[offset+1]=255;
        f=frame(j?0x80:0xA0,b); CHECK(u3_raw_decode(&f,&d)==U3_OK);
        CHECK((j?d.fields.permission.source_age_ms:d.fields.rear.source_age_ms)==i);
        CHECK(u3_raw_format(&d,text,sizeof text,&n)==U3_OK);
        CHECK(i!=65535 || strstr(text,"SATURATED_LOWER_BOUND")!=NULL); same_bytes(&d,b);
    }
    end();

    begin("T16");
    for(j=0;j<2;++j) {
        memcpy(b,j?ref_permission:ref_rear,20); f=frame(j?0x80:0xA0,b);
        CHECK(u3_raw_decode(&f,&d)==U3_OK);
        b[4]=(uint8_t)(b[4]+1); f.received_at_ms=9999;
        CHECK(u3_raw_decode(&f,&second)==U3_OK);
        if(j) {
            CHECK(d.fields.permission.update_sequence==second.fields.permission.update_sequence);
            CHECK(d.fields.permission.source_age_ms==second.fields.permission.source_age_ms);
        } else {
            CHECK(d.fields.rear.update_sequence==second.fields.rear.update_sequence);
            CHECK(d.fields.rear.source_age_ms==second.fields.rear.source_age_ms);
        }
        f.received_at_ms=UINT32_MAX; CHECK(u3_raw_decode(&f,&second)==U3_OK);
        CHECK(second.received_at_ms==UINT32_MAX); /* RX fact only, no timer exists. */
        same_bytes(&second,b);
        memset(b,255,8); /* literal boot/TX maximum; source sequence and AGE unchanged */
        CHECK(u3_raw_decode(&f,&second)==U3_OK); same_bytes(&second,b);
        memset(b+4,0,4); CHECK(u3_raw_decode(&f,&second)==U3_OK); same_bytes(&second,b);
    } end();

    begin("T17");
    f=frame(0xA0,ref_rear); reject(NULL,U3_ERR_ARGUMENT);
    CHECK(u3_raw_decode(&f,NULL)==U3_ERR_ARGUMENT); f.payload=NULL; reject(&f,U3_ERR_ARGUMENT);
    for(i=0;i<20;++i) {
        uint8_t guarded[22], before[22]; memset(guarded,0xCD,sizeof guarded); memcpy(before,guarded,sizeof before);
        n=99; CHECK(u3_raw_encode(&rear,guarded+1,i,&n)==U3_ERR_CAPACITY);
        CHECK(n==0 && memcmp(guarded,before,sizeof guarded)==0);
        n=99; CHECK(u3_raw_encode(&permission,guarded+1,i,&n)==U3_ERR_CAPACITY);
        CHECK(n==0 && memcmp(guarded,before,sizeof guarded)==0);
    }
    CHECK(u3_raw_encode(NULL,b,sizeof b,&n)==U3_ERR_ARGUMENT && n==0);
    CHECK(u3_raw_encode(&rear,NULL,sizeof b,&n)==U3_ERR_ARGUMENT && n==0);
    CHECK(u3_raw_encode(&rear,b,sizeof b,NULL)==U3_ERR_ARGUMENT);
    CHECK(u3_raw_format(&rear,text,sizeof text,&n)==U3_OK);
    CHECK(u3_raw_format(&rear,other,sizeof other,&m)==U3_OK && m==n && strcmp(text,other)==0);
    for(i=0;i<=n;++i) {
        char before[1024]; memset(other,0xCD,sizeof other); memcpy(before,other,sizeof before);
        CHECK(u3_raw_format(&rear,other,i,&m)==U3_ERR_CAPACITY);
        CHECK(m==0 && memcmp(other,before,sizeof other)==0);
    }
    CHECK(u3_raw_format(&rear,other,n+1,&m)==U3_OK && m==n && strcmp(text,other)==0);
    CHECK(u3_raw_format(NULL,other,sizeof other,&m)==U3_ERR_ARGUMENT && m==0);
    CHECK(u3_raw_format(&rear,NULL,sizeof other,&m)==U3_ERR_ARGUMENT && m==0);
    CHECK(u3_raw_format(&rear,other,sizeof other,NULL)==U3_ERR_ARGUMENT);
    d=rear; d.raw[14]^=1; CHECK(u3_raw_format(&d,other,sizeof other,&m)==U3_ERR_VALUE && m==0);
    d=rear; d.can_id=0xA1; bad_encode(&d,U3_ERR_UNSUPPORTED);
    d=rear; d.fdf=0; bad_encode(&d,U3_ERR_PROFILE);
    CHECK(u3_raw_format(&permission,text,sizeof text,&n)==U3_OK);
    for(i=0;i<=n;++i) {
        char before[1024]; memset(other,0xCD,sizeof other); memcpy(before,other,sizeof before);
        CHECK(u3_raw_format(&permission,other,i,&m)==U3_ERR_CAPACITY);
        CHECK(m==0 && memcmp(other,before,sizeof other)==0);
    }
    CHECK(u3_raw_format(&permission,other,n+1,&m)==U3_OK && m==n && strcmp(text,other)==0);
    memcpy(b,ref_rear,20); f=frame(0xA0,b);
    CHECK(u3_raw_decode(&f,&d)==U3_OK);
    memset(b,0,20); /* Successful record owns raw bytes and typed facts. */
    CHECK(memcmp(d.raw,ref_rear,20)==0 && d.fields.rear.rear_distance_deci_cm==400);
    CHECK(u3_raw_format(&d,text,sizeof text,&n)==U3_OK); end();
    printf("SUMMARY cases=17 checks=%u failures=%u\n",checks,failures);
    return failures?1:0;
}
