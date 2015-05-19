/*
 * QEMU ICH9 TCO emulation
 *
 * Copyright (c) 2015 Paulo Alcantara <pcacjr@zytor.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef HW_ACPI_TCO_H
#define HW_ACPI_TCO_H

#include "qemu/typedefs.h"
#include "qemu-common.h"

/* TCO I/O register offsets */
enum {
    TCO_RLD           = 0x00,
    TCO_DAT_IN        = 0x02,
    TCO_DAT_OUT       = 0x03,
    TCO1_STS          = 0x04,
    TCO2_STS          = 0x06,
    TCO1_CNT          = 0x08,
    TCO2_CNT          = 0x0a,
    TCO_MESSAGE1      = 0x0c,
    TCO_MESSAGE2      = 0x0d,
    TCO_WDCNT         = 0x0e,
    SW_IRQ_GEN        = 0x10,
    TCO_TMR           = 0x12,
};

/* TCO I/O register defaults */
enum {
    TCO_RLD_DEFAULT         = 0x0000,
    TCO_DAT_IN_DEFAULT      = 0x00,
    TCO_DAT_OUT_DEFAULT     = 0x00,
    TCO1_STS_DEFAULT        = 0x0000,
    TCO2_STS_DEFAULT        = 0x0000,
    TCO1_CNT_DEFAULT        = 0x0000,
    TCO2_CNT_DEFAULT        = 0x0008,
    TCO_MESSAGE1_DEFAULT    = 0x00,
    TCO_MESSAGE2_DEFAULT    = 0x00,
    TCO_WDCNT_DEFAULT       = 0x00,
    TCO_TMR_DEFAULT         = 0x0004,
    SW_IRQ_GEN_DEFAULT      = 0x03,
};

/* TCO I/O register control/status bits */
enum {
    SW_TCO_SMI           = (1 << 1),
    TCO_INT_STS          = (1 << 2),
    TCO_LOCK             = (1  << 12),
    TCO_TMR_HLT          = (1 << 11),
    TCO_TIMEOUT          = (1 << 3),
    TCO_SECOND_TO_STS    = (1 << 1),
    TCO_BOOT_STS         = (1 << 2),
};

/* TCO I/O registers mask bits */
enum {
    TCO_RLD_MASK     = 0x3ff,
    TCO1_STS_MASK    = 0xe870,
    TCO2_STS_MASK    = 0xfff8,
    TCO1_CNT_MASK    = 0xfeff,
    TCO_TMR_MASK     = 0x3ff,
};

typedef struct TCOIORegs {
    struct {
        uint16_t rld;
        uint8_t din;
        uint8_t dout;
        uint16_t sts1;
        uint16_t sts2;
        uint16_t cnt1;
        uint16_t cnt2;
        uint8_t msg1;
        uint8_t msg2;
        uint8_t wdcnt;
        uint16_t tmr;
    } tco;
    uint8_t sw_irq_gen;
} TCOIORegs;

#define TCO_IO_REGS_DEFAULTS_INIT()             \
    (TCOIORegs) {                               \
        .tco = {                                \
            .rld      = TCO_RLD_DEFAULT,        \
            .din      = TCO_DAT_IN_DEFAULT,     \
            .dout     = TCO_DAT_OUT_DEFAULT,    \
            .sts1     = TCO1_STS_DEFAULT,       \
            .sts2     = TCO2_STS_DEFAULT,       \
            .cnt1     = TCO1_CNT_DEFAULT,       \
            .cnt2     = TCO2_CNT_DEFAULT,       \
            .msg1     = TCO_MESSAGE1_DEFAULT,   \
            .msg2     = TCO_MESSAGE2_DEFAULT,   \
            .wdcnt    = TCO_WDCNT_DEFAULT,      \
            .tmr      = TCO_TMR_DEFAULT,        \
        },                                      \
        .sw_irq_gen = SW_IRQ_GEN_DEFAULT        \
    }

/* tco.c */
void acpi_pm_tco_init(TCOIORegs *tr);
uint32_t acpi_pm_tco_ioport_readw(TCOIORegs *tr, uint32_t addr);
void acpi_pm_tco_ioport_writew(TCOIORegs *tr, uint32_t addr, uint32_t val);

/* As per ICH9 spec, the internal timer has an error of ~0.6s on every tick */
static inline int64_t tco_ticks_per_sec(void)
{
    return 600000000LL;
}

static inline int is_valid_tco_time(uint32_t val)
{
    /* values of 0 or 1 will be ignored by ICH */
    return val > 1;
}

static inline int can_start_tco_timer(TCOIORegs *tr)
{
    return !(tr->tco.cnt1 & TCO_TMR_HLT) && is_valid_tco_time(tr->tco.tmr);
}

#endif /* HW_ACPI_TCO_H */
