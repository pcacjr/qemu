/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/****************************************************************
 * PCI Device Resource Comsumption
 ****************************************************************/

Scope(\_SB.PCI0) {
    Device (PDRC) {
        Name (_HID, EISAID("PNP0C02"))
        Name (_UID, 1)

        Name (PDRS, ResourceTemplate() {
	    Memory32Fixed(ReadWrite, 0xfed1c000, 0x00004000)
        })

        Method (_CRS, 0, Serialized) {
            Return(PDRS)
        }
    }
}

Scope(\_SB) {
    OperationRegion (RCRB, SystemMemory, 0xfed1c000, 0x4000)
    Field (RCRB, DWordAcc, Lock, Preserve) {
        Offset(0x3000),
	TCTL, 8,
	, 24,
	Offset(0x3400),
	RTCC, 32,
	HPTC, 32,
	GCSR, 32,
    }
}
