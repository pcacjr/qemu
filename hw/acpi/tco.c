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
#include "qemu-common.h"
#include "sysemu/watchdog.h"
#include "hw/i386/ich9.h"

#include "hw/acpi/tco.h"

//#define DEBUG

#ifdef DEBUG
#define TCO_DEBUG(fmt, ...)                                     \
    do {                                                        \
        fprintf(stderr, "%s "fmt, __func__, ## __VA_ARGS__);    \
    } while (0)
#else
#define TCO_DEBUG(fmt, ...) do { } while (0)
#endif

static QEMUTimer *tco_timer;
static unsigned int timeouts_no;

static inline void tco_timer_reload(void)
{
    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    timer_del(tco_timer);
    timer_mod(tco_timer, now + tco_ticks_per_sec());
}

static inline void tco_timer_stop(void)
{
    timer_del(tco_timer);
}

static void tco_timer_expired(void *opaque)
{
    TCOIORegs *tr = opaque;
    ICH9LPCPMRegs *pm = container_of(tr, ICH9LPCPMRegs, tco_regs);
    ICH9LPCState *lpc = container_of(pm, ICH9LPCState, pm);
    uint32_t gcs = pci_get_long(lpc->chip_config + ICH9_LPC_RCBA_GCS);

    tr->tco.rld--;
    if (tr->tco.rld & TCO_RLD_MASK) {
        goto out;
    }

    tr->tco.sts1 |= TCO_TIMEOUT;
    if (++timeouts_no == 2) {
        tr->tco.sts1 |= TCO_SECOND_TO_STS;
        tr->tco.sts1 |= TCO_BOOT_STS;
        timeouts_no = 0;

        if (!(gcs & ICH9_LPC_RCBA_GCS_NO_REBOOT)) {
            watchdog_perform_action();
            tco_timer_stop();
            return;
        }
    }
    tr->tco.rld = tr->tco.tmr;

    if (pm->smi_en & ICH9_PMIO_SMI_EN_TCO_EN) {
        ich9_generate_smi();
    } else {
        ich9_generate_nmi();
    }

out:
    tco_timer_reload();
}

void acpi_pm_tco_init(TCOIORegs *tr)
{
    *tr = TCO_IO_REGS_DEFAULTS_INIT();
    tco_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, tco_timer_expired, tr);
}

uint32_t acpi_pm_tco_ioport_readw(TCOIORegs *tr, uint32_t addr)
{
    switch (addr) {
    case TCO_RLD:
        return tr->tco.rld;
    case TCO_DAT_IN:
        return tr->tco.din;
    case TCO_DAT_OUT:
        return tr->tco.dout;
    case TCO1_STS:
        return tr->tco.sts1;
    case TCO2_STS:
        return tr->tco.sts2;
    case TCO1_CNT:
        return tr->tco.cnt1;
    case TCO2_CNT:
        return tr->tco.cnt2;
    case TCO_MESSAGE1:
        return tr->tco.msg1;
    case TCO_MESSAGE2:
        return tr->tco.msg2;
    case TCO_WDCNT:
        return tr->tco.wdcnt;
    case TCO_TMR:
        return tr->tco.tmr;
    case SW_IRQ_GEN:
        return tr->sw_irq_gen;
    }
    return 0;
}

void acpi_pm_tco_ioport_writew(TCOIORegs *tr, uint32_t addr, uint32_t val)
{
    switch (addr) {
    case TCO_RLD:
        timeouts_no = 0;
        if (can_start_tco_timer(tr)) {
            tr->tco.rld = tr->tco.tmr;
            tco_timer_reload();
        } else {
            tr->tco.rld = val;
        }
        break;
    case TCO_DAT_IN:
        tr->tco.din = val;
        tr->tco.sts1 |= SW_TCO_SMI;
        ich9_generate_smi();
        break;
    case TCO_DAT_OUT:
        tr->tco.dout = val;
        tr->tco.sts1 |= TCO_INT_STS;
        /* TODO: cause an interrupt, as selected by the TCO_INT_SEL bits */
        break;
    case TCO1_STS:
        tr->tco.sts1 = val & TCO1_STS_MASK;
        break;
    case TCO2_STS:
        tr->tco.sts2 = val & TCO2_STS_MASK;
        break;
    case TCO1_CNT:
        val &= TCO1_CNT_MASK;
        /* TCO_LOCK bit cannot be changed once set */
        tr->tco.cnt1 = (val & ~TCO_LOCK) | (tr->tco.cnt1 & TCO_LOCK);
        if (can_start_tco_timer(tr)) {
            tr->tco.rld = tr->tco.tmr;
            tco_timer_reload();
        } else {
            tco_timer_stop();
        }
        break;
    case TCO2_CNT:
        tr->tco.cnt2 = val;
        break;
    case TCO_MESSAGE1:
        tr->tco.msg1 = val;
        break;
    case TCO_MESSAGE2:
        tr->tco.msg2 = val;
        break;
    case TCO_WDCNT:
        tr->tco.wdcnt = val;
        break;
    case TCO_TMR:
        tr->tco.tmr = val;
        break;
    case SW_IRQ_GEN:
        tr->sw_irq_gen = val;
        break;
    }
}
