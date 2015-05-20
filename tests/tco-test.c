/*
 * QEMU ICH9 TCO emulation tests
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
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdlib.h>

#include "libqtest.h"
#include "libqos/pci.h"
#include "libqos/pci-pc.h"
#include "hw/pci/pci_regs.h"
#include "hw/i386/ich9.h"
#include "hw/acpi/ich9.h"
#include "hw/acpi/tco.h"

#define PM_IO_BASE_ADDR    0xb000
#define RCBA_BASE_ADDR     0xfed1c000

#define TCO_SECS_TO_TICKS(secs) ((secs) * 10 / 16)

typedef struct {
    QPCIDevice *dev;
    void *lpc_base;
    void *tco_io_base;
} TestData;

static void test_init(TestData *d)
{
    QPCIBus *bus;

    qtest_start("-machine q35");

    bus = qpci_init_pc();
    d->dev = qpci_device_find(bus, QPCI_DEVFN(0x1f, 0x00));
    g_assert(d->dev != NULL);

    /* map PCI-to-LPC bridge interface BAR */
    d->lpc_base = qpci_iomap(d->dev, 0, NULL);

    qpci_device_enable(d->dev);

    g_assert(d->lpc_base != NULL);

    /* set ACPI PM I/O space base address */
    qpci_config_writel(d->dev, (uintptr_t)d->lpc_base + ICH9_LPC_PMBASE,
                       PM_IO_BASE_ADDR | 0x1);
    /* enable ACPI I/O */
    qpci_config_writeb(d->dev, (uintptr_t)d->lpc_base + ICH9_LPC_ACPI_CTRL,
                       0x80);
    /* set Root Complex BAR */
    qpci_config_writel(d->dev, (uintptr_t)d->lpc_base + ICH9_LPC_RCBA,
                       RCBA_BASE_ADDR | 0x1);

    d->tco_io_base = (void *)((uintptr_t)PM_IO_BASE_ADDR + 0x60);
}

static void stop_tco(const TestData *d)
{
    uint32_t val;

    val = qpci_io_readw(d->dev, d->tco_io_base + TCO1_CNT);
    val |= TCO_TMR_HLT;
    qpci_io_writew(d->dev, d->tco_io_base + TCO1_CNT, val);
}

static void start_tco(const TestData *d)
{
    uint32_t val;

    val = qpci_io_readw(d->dev, d->tco_io_base + TCO1_CNT);
    val &= ~TCO_TMR_HLT;
    qpci_io_writew(d->dev, d->tco_io_base + TCO1_CNT, val);
}

static void load_tco(const TestData *d)
{
    qpci_io_writew(d->dev, d->tco_io_base + TCO_RLD, 4);
}

static void set_tco_timeout(const TestData *d, uint16_t secs)
{
    qpci_io_writew(d->dev, d->tco_io_base + TCO_TMR,
                   TCO_SECS_TO_TICKS(secs));
}

static void clear_tco_status(const TestData *d)
{
    qpci_io_writew(d->dev, d->tco_io_base + TCO1_STS, 0x0008);
    qpci_io_writew(d->dev, d->tco_io_base + TCO2_STS, 0x0002);
    qpci_io_writew(d->dev, d->tco_io_base + TCO2_STS, 0x0004);
}

static void reset_on_second_timeout(bool enable)
{
    uint32_t val;

    val = readl(RCBA_BASE_ADDR + ICH9_LPC_RCBA_GCS);
    if (enable) {
        val &= ~ICH9_LPC_RCBA_GCS_NO_REBOOT;
    } else {
        val |= ICH9_LPC_RCBA_GCS_NO_REBOOT;
    }
    writel(RCBA_BASE_ADDR + ICH9_LPC_RCBA_GCS, val);
}

static void test_tco_defaults(void)
{
    TestData d;

    test_init(&d);
    g_assert_cmpint(qpci_io_readw(d.dev, d.tco_io_base + TCO_RLD), ==,
                    TCO_RLD_DEFAULT);
    /* TCO_DAT_IN & TCO_DAT_OUT */
    g_assert_cmpint(qpci_io_readw(d.dev, d.tco_io_base + TCO_DAT_IN), ==,
                    (TCO_DAT_OUT_DEFAULT << 8) | TCO_DAT_IN_DEFAULT);
    /* TCO1_STS & TCO2_STS */
    g_assert_cmpint(qpci_io_readl(d.dev, d.tco_io_base + TCO1_STS), ==,
                    (TCO2_STS_DEFAULT << 16) | TCO1_STS_DEFAULT);
    /* TCO1_CNT & TCO2_CNT */
    g_assert_cmpint(qpci_io_readl(d.dev, d.tco_io_base + TCO1_CNT), ==,
                    (TCO2_CNT_DEFAULT << 16) | TCO1_CNT_DEFAULT);
    /* TCO_MESSAGE1 & TCO_MESSAGE2 */
    g_assert_cmpint(qpci_io_readw(d.dev, d.tco_io_base + TCO_MESSAGE1), ==,
                    (TCO_MESSAGE2_DEFAULT << 8) | TCO_MESSAGE1_DEFAULT);
    g_assert_cmpint(qpci_io_readb(d.dev, d.tco_io_base + TCO_WDCNT), ==,
                    TCO_WDCNT_DEFAULT);
    g_assert_cmpint(qpci_io_readb(d.dev, d.tco_io_base + SW_IRQ_GEN), ==,
                    SW_IRQ_GEN_DEFAULT);
    g_assert_cmpint(qpci_io_readw(d.dev, d.tco_io_base + TCO_TMR), ==,
                    TCO_TMR_DEFAULT);
    qtest_end();
}

static void test_tco_timeout(void)
{
    TestData d;
    uint32_t val;
    int ret;

    test_init(&d);

    stop_tco(&d);
    clear_tco_status(&d);
    reset_on_second_timeout(false);
    set_tco_timeout(&d, 4);
    load_tco(&d);
    start_tco(&d);
    clock_step(4 * 1000000000LL);

    /* test first timeout */
    val = qpci_io_readw(d.dev, d.tco_io_base + TCO1_STS);
    ret = val & TCO_TIMEOUT ? 1 : 0;
    g_assert(ret == 1);

    /* test clearing timeout bit */
    val |= TCO_TIMEOUT;
    qpci_io_writew(d.dev, d.tco_io_base + TCO1_STS, val);
    val = qpci_io_readw(d.dev, d.tco_io_base + TCO1_STS);
    ret = val & TCO_TIMEOUT ? 1 : 0;
    g_assert(ret == 0);

    /* test second timeout */
    clock_step(4 * 1000000000LL);
    val = qpci_io_readw(d.dev, d.tco_io_base + TCO1_STS);
    ret = val & TCO_TIMEOUT ? 1 : 0;
    g_assert(ret == 1);
    val = qpci_io_readw(d.dev, d.tco_io_base + TCO1_STS);
    ret = val & TCO_SECOND_TO_STS ? 1 : 0;
    g_assert(ret == 1);

    stop_tco(&d);
    qtest_end();
}

static void test_tco_second_timeout_reboot(void)
{
    TestData d;

    test_init(&d);

    stop_tco(&d);
    clear_tco_status(&d);
    reset_on_second_timeout(true);
    set_tco_timeout(&d, 16);
    load_tco(&d);
    start_tco(&d);
    clock_step(32 * 1000000000LL);
    g_assert(!strcmp(qdict_get_str(qmp(""), "event"), "RESET"));

    stop_tco(&d);
    qtest_end();
}

static void test_tco_ticks_counter(void)
{
    TestData d;
    int secs = 8;
    unsigned int ticks;

    test_init(&d);

    stop_tco(&d);
    clear_tco_status(&d);
    set_tco_timeout(&d, secs);
    load_tco(&d);
    start_tco(&d);

    ticks = TCO_SECS_TO_TICKS(secs);
    do {
        g_assert_cmpint(qpci_io_readw(d.dev, d.tco_io_base + TCO_RLD), ==,
                        ticks);
        clock_step(600000000LL);
        ticks--;
    } while (!(qpci_io_readw(d.dev, d.tco_io_base + TCO1_STS) & TCO_TIMEOUT));

    stop_tco(&d);
    qtest_end();
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    qtest_add_func("tco/defaults", test_tco_defaults);
    qtest_add_func("tco/timeout/no_reboot", test_tco_timeout);
    qtest_add_func("tco/timeout/reboot", test_tco_second_timeout_reboot);
    qtest_add_func("tco/counter", test_tco_ticks_counter);
    return g_test_run();
}
