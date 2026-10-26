/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * el_snprintf against the C library for the formats the panel uses. */
#include "el_fmt.h"
#include <stdio.h>
#include <string.h>
int main(void){ char a[64],b[64]; int bad=0;
#define T(...) do{ el_snprintf(a,sizeof a,__VA_ARGS__); snprintf(b,sizeof b,__VA_ARGS__); if(strcmp(a,b)){bad++;printf("MISMATCH [%s] [%s]\n",a,b);} }while(0)
 T("U%02u %s",3u,"KUPI"); T("TRANSPOSE %+d",-5); T("TRANSPOSE %+d",7); T("%+d",0); T("LEN %u  GATE %u%%",16u,50u); T("%s%d","C",-1); T("%s O%+d","X",2); T("STEP %02u %s %s%s%s%s",12u,"C4","A","B","",""); T("%d.%d:1",1,8); T("-%d DB",12); T("%u x %s = %u BARS",4u,"1/16",8u);
 el_snprintf(a,5,"%s","ABCDEFG"); if(strcmp(a,"ABCD")){bad++;puts("trunc");}
 printf(bad ? "fmt: el_snprintf FAIL\n" : "fmt: el_snprintf like snprintf ok\n"); return bad; }
