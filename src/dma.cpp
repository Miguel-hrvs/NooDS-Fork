/*
    Copyright 2019-2023 Hydr8gon

    This file is part of NooDS.

    NooDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NooDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NooDS. If not, see <https://www.gnu.org/licenses/>.
*/

#include "dma.h"
#include "core.h"

void Dma::transfer(int channel) {
    uint32_t dmaControl = dmaCnt[channel];
    int dstAddrCnt = (dmaControl & 0x00600000) >> 21;
    int srcAddrCnt = (dmaControl & 0x01800000) >> 23;
    int mode = (dmaControl & 0x38000000) >> 27;
    int wordCount = dmaControl & 0x001FFFFF;

    if (core->gbaMode && mode == 6 && (channel == 1 || channel == 2)) { // GBA sound DMA
        for (int i = 0; i < 4; i++) {
            uint32_t value = core->memory.read<uint32_t>(cpu, srcAddrs[channel], false);
            core->memory.write<uint32_t>(cpu, dstAddrs[channel], value, false);
            srcAddrs[channel] += (srcAddrCnt == 0) ? 4 : -4;
        }
    } else {
        bool isWordTransfer = dmaControl & BIT(26);
        int addrIncSrc = (srcAddrCnt == 0) ? (isWordTransfer ? 4 : 2) : (isWordTransfer ? -4 : -2);
        int addrIncDst = (dstAddrCnt == 0 || dstAddrCnt == 3) ? (isWordTransfer ? 4 : 2) : (isWordTransfer ? -4 : -2);

        while (wordCount--) {
            if (isWordTransfer) {
                uint32_t value = core->memory.read<uint32_t>(cpu, srcAddrs[channel], false);
                core->memory.write<uint32_t>(cpu, dstAddrs[channel], value, false);
            } else {
                uint16_t value = core->memory.read<uint16_t>(cpu, srcAddrs[channel], false);
                core->memory.write<uint16_t>(cpu, dstAddrs[channel], value, false);
            }
            srcAddrs[channel] += addrIncSrc;
            dstAddrs[channel] += addrIncDst;
        }
    }

    if (mode == 7) {
        wordCounts[channel] -= (dmaControl & 0x001FFFFF) - wordCount;
        if (wordCounts[channel] > 0 && (core->gpu3D.readGxStat() & BIT(25)))
            core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + channel), 1);
        return;
    }

    if ((dmaControl & BIT(25)) && mode != 0) { // Repeat
        wordCounts[channel] = dmaControl & 0x001FFFFF;
        if (dstAddrCnt == 3)
            dstAddrs[channel] = dmaDad[channel];
        if (mode == 7 && (core->gpu3D.readGxStat() & BIT(25)))
            core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + channel), 1);
    } else {
        dmaCnt[channel] &= ~BIT(31);
    }

    if (dmaControl & BIT(30))
        core->interpreter[cpu].sendInterrupt(8 + channel);
}

void Dma::trigger(int mode, uint8_t channels) {
    // ARM7 DMAs don't use the lowest mode bit, so adjust accordingly
    mode <<= (cpu == 1);

    // Schedule a transfer on channels that are enabled and set to the triggered mode
    for (int i = 0; i < 4; i++) {
        if (channels & BIT(i)) {
            uint32_t dmaControl = dmaCnt[i];
            if ((dmaControl & BIT(31)) && ((dmaControl >> 27) == mode))
                core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + i), 1);
        }
    }
}

void Dma::writeDmaSad(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the DMASAD registers
    mask &= ((cpu == 0 || channel != 0) ? 0x0FFFFFFF : 0x07FFFFFF);
    dmaSad[channel] = (dmaSad[channel] & ~mask) | (value & mask);
}

void Dma::writeDmaDad(int channel, uint32_t mask, uint32_t value)
{
    // Write to one of the DMADAD registers
    mask &= ((cpu == 0 || channel == 3) ? 0x0FFFFFFF : 0x07FFFFFF);
    dmaDad[channel] = (dmaDad[channel] & ~mask) | (value & mask);
}

void Dma::writeDmaCnt(int channel, uint32_t mask, uint32_t value)
{
    uint32_t old = dmaCnt[channel];

    // Write to one of the DMACNT registers
    mask &= ((cpu == 0) ? 0xFFFFFFFF : (channel == 3 ? 0xF7E0FFFF : 0xF7E03FFF));
    dmaCnt[channel] = (dmaCnt[channel] & ~mask) | (value & mask);

    // In GXFIFO mode, schedule a transfer on the channel immediately if the FIFO is already half empty
    // All other modes are only triggered at the moment when the event happens
    // For example, if a word from the DS cart is ready before starting a DMA, the DMA will not be triggered
    if ((dmaCnt[channel] & BIT(31)) && ((dmaCnt[channel] & 0x38000000) >> 27) == 7 && (core->gpu3D.readGxStat() & BIT(25)))
        core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + channel), 1);

    // Don't reload the internal registers unless the enable bit changed from 0 to 1
    if ((old & BIT(31)) || !(dmaCnt[channel] & BIT(31)))
        return;

    // Reload the internal registers
    dstAddrs[channel] = dmaDad[channel];
    srcAddrs[channel] = dmaSad[channel];
    wordCounts[channel] = dmaCnt[channel] & 0x001FFFFF;

    // Schedule a transfer on the channel if it's set to immediate mode
    // Reloading seems to be the only trigger for this, so an enabled channel changed to immediate will never transfer
    // This also means that repeating doesn't work; in this case, the enabled bit is cleared after only one transfer
    if (((dmaCnt[channel] & 0x38000000) >> 27) == 0)
        core->schedule(SchedTask(DMA9_TRANSFER0 + (cpu << 2) + channel), 1);
}

uint32_t Dma::readDmaCnt(int channel)
{
    // Read from one of the DMACNT registers
    // The lower half-word isn't readable in GBA mode
    return dmaCnt[channel] & ~(core->gbaMode ? 0x0000FFFF : 0x00000000);
}
