#ifndef ANAIN_PRJ_H_INCLUDED
#define ANAIN_PRJ_H_INCLUDED

#include "hwdefs.h"

#define NUM_SAMPLES 64
#define SAMPLE_TIME ADC_SMPR_SMP_7DOT5CYC

#define ANA_IN_LIST \
   ANA_IN_ENTRY(curpos,       GPIOA, 0) \
   ANA_IN_ENTRY(curneg,       GPIOA, 1) \
   ANA_IN_ENTRY(temp1,        GPIOA, 2) \
   ANA_IN_ENTRY(temp2,        GPIOA, 3) \
   ANA_IN_ENTRY(enalevel,     GPIOA, 4) \

#endif // ANAIN_PRJ_H_INCLUDED
