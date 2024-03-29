#ifndef PinMode_PRJ_H_INCLUDED
#define PinMode_PRJ_H_INCLUDED

#include "hwdefs.h"

/* Here you specify generic IO pins, i.e. digital input or outputs.
 * Inputs can be floating (INPUT_FLT), have a 30k pull-up (INPUT_PU)
 * or pull-down (INPUT_PD) or be an output (OUTPUT)
*/

#define DIG_IO_LIST \
    DIG_IO_ENTRY(i2c_di,     GPIOB, GPIO14,  PinMode::INPUT_FLT)   \
    DIG_IO_ENTRY(i2c_scl,    GPIOB, GPIO13,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(i2c_do,     GPIOB, GPIO15,  PinMode::OUTPUT)      \
    DIG_IO_ENTRY(nextena_out,GPIOA, GPIO9,   PinMode::OUTPUT)      \
    DIG_IO_ENTRY(selfena_out,GPIOA, GPIO8,   PinMode::OUTPUT)      \
    DIG_IO_ENTRY(led_out,    GPIOA, GPIO5,   PinMode::OUTPUT)      \

#endif // PinMode_PRJ_H_INCLUDED
