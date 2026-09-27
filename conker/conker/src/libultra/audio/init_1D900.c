#include <os_internal.h>
#include <ultraerror.h>
#include "n_synthInternals.h"


extern Acmd *(func_1001E530)(s32, Acmd *);
void func_1001CF38(void *, f32 arg1);


void n_alSynSetFXMix( N_ALVoice *v, u8 fxmix) {
    ALParam  *update;

    if (v->pvoice) {
        update = __n_allocParam();
        ALFailIf(update == 0, ERR_ALSYN_NO_UPDATE);

        update->delta  = n_syn->paramSamples + v->pvoice->offset;
        update->type   = AL_FILTER_SET_FXAMT;
        update->data.i = fxmix;
        update->next   = 0;
        n_alEnvmixerParam(v->pvoice, AL_FILTER_ADD_UPDATE, update);
    }
}

s32 func_1001D9B0( s16 arg0) {
    N_ALMainBus *sp4;

    sp4 = n_syn->mainBus;
    if (sp4->filter.handler == func_1001E530) {
        return n_syn->auxBus[arg0].sources;
    } else {
        return 0;
    }
}

// Returns the last effect slot (fx_array[7]) of auxiliary bus `bus`, or NULL when the
// main bus's filter isn't func_1001E530 (the handler n_alSynNew installs on it; see
// n_synthesizer.c), i.e. before the synthesizer is set up that way. A sibling of
// func_1001D9B0, which returns the same bus's sources. What the game keeps in that last
// slot isn't identified yet. Compiled unoptimised (-g), hence the explicit else.
ALFx *func_1001DA28(s16 bus) {
    N_ALMainBus *mainBus;

    mainBus = n_syn->mainBus;
    if (mainBus->filter.handler == func_1001E530) {
        return n_syn->auxBus[bus].fx_array[7];
    } else {
        return NULL;
    }
}

void func_1001DAA0(arg0, arg1, arg2)
    s32 arg0;
    s16 arg1;
    s32 arg2;
{
    s32 sp1C = arg0;
    func_1001ED6C(sp1C, arg1, arg2);
}

void func_1001DAE4(ALVoiceConfig *arg0, s16 arg1, s32 *arg2) {
    if (arg1 == 8) {
        arg0->fxBus = (f32) *arg2 * 0.1f;
    } else if (arg1 == 9) {
        arg0->priority = *arg2;
    }
    func_1001CF38(arg0, n_syn->outputRate);
}
