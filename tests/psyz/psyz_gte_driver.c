/* psyz GTE driver: the psyz software GTE fed like test_gte --driver.
 *
 * Copyright (c) 2026 sh2o Contributors. MIT License.
 * Inspired by SH2ZAM and SH4ZAM; see NOTICE.md.
 */

/* tests/psyz_gte_compare.py links this file with the psyz GTE sources, compiled
 * in place and unchanged. No psyz text is in this file.
 * In: one case per stdin line, "<cmd hex|-> <n> <reg> <value hex> ..." (n
 * pairs). Register 0-31 goes to Psyz_GteDataWrite, 32-63 to Psyz_GteCtrlWrite
 * (register - 32), in the listed order; then Psyz_GteCommand(cmd) unless cmd
 * is "-". The psyz GTE state is not cleared between lines.
 * Out: 64 hex words, registers 0-31 from Psyz_GteDataRead, 32-63 from
 * Psyz_GteCtrlRead. */
#include <stdio.h>
#include <string.h>

unsigned int Psyz_GteDataRead(unsigned idx);
void Psyz_GteDataWrite(unsigned idx,unsigned int v);
unsigned int Psyz_GteCtrlRead(unsigned idx);
void Psyz_GteCtrlWrite(unsigned idx,unsigned int v);
void Psyz_GteCommand(unsigned int cmd);

/* The psyz logger (matrix stack overflow, square root domain): silent. */
void psyz_log(unsigned int level,const char *file,unsigned int line,const char *func,const char *fmt,...) {
    (void)level;
    (void)file;
    (void)line;
    (void)func;
    (void)fmt;
}

int main(void) {
    char cmd[32];
    while(scanf("%31s",cmd)==1) {
        int n,i;
        if(scanf("%d",&n)!=1)
            return 2;
        for(i=0;i<n;i++) {
            unsigned r,v;
            if(scanf("%u %x",&r,&v)!=2||r>63)
                return 2;
            if(r<32)
                Psyz_GteDataWrite(r,v);
            else
                Psyz_GteCtrlWrite(r-32,v);
        }
        if(strcmp(cmd,"-")!=0) {
            unsigned c;
            if(sscanf(cmd,"%x",&c)!=1)
                return 2;
            Psyz_GteCommand(c);
        }
        for(i=0;i<64;i++)
            printf(i?" %08x":"%08x",i<32?Psyz_GteDataRead((unsigned)i):Psyz_GteCtrlRead((unsigned)i-32));
        printf("\n");
    }
    return 0;
}
