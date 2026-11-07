/* SPDX-License-Identifier: MPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include "song.h"
void dk_midi_out(uint32_t a,uint32_t b,uint32_t c){(void)a;(void)b;(void)c;}
int main(void){
    int e,b,i,total=0,visible=0,hidden=0;dk_preset_t tmp;
    dk_init();dk_song_clear();
    for(e=0;e<TAPE.neng;e++)for(b=0;b<DK_NBLK;b++){
        int n=preset_visible_count(e,b),raw=preset_count(e,b);
        assert(n>0 || raw==0);
        for(i=0;i<n;i++){
            int id=preset_visible_id(e,b,i);
            assert(id>=0 && preset_block(id)==b);
            assert(preset_visible_index(e,id)==i);
        }
        assert(preset_visible_id(e,b,n)==-1);
        for(i=0;i<raw;i++){
            int id=preset_id(b,i);const dk_preset_t *p=preset_of(e,id,&tmp);
            assert(p && p->name && p->name[0] && p->eng==e);
            if(preset_visible_index(e,id)<0)hidden++;
        }
        visible+=n;total+=raw;
    }
    assert(total==420 && visible==354 && hidden==66);
    for(i=0;i<12;i++){
        e=TAPE.keys_set[i][0];assert(preset_visible_index(e,TAPE.keys_set[i][1])>=0);
    }
    puts("PASS: 354 visible / 66 retired; all 420 raw IDs load; browser roundtrips and KEYS shortcuts preserved");
}
