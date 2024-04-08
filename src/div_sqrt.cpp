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

#include <cmath>

#include "div_sqrt.h"
#include "core.h"

void DivSqrt::divide()
{
    divCnt &= ~BIT(14);
    if (divDenom == 0) divCnt |= BIT(14);

    int divMode = divCnt & 0x0003;
    int64_t num = static_cast<int64_t>(divNumer);
    int32_t den = static_cast<int32_t>(divDenom);

    if (divMode == 0) // 32-bit / 32-bit
    {
        int32_t n = static_cast<int32_t>(num);
        if (n == INT32_MIN && den == -1) // Overflow
            divResult = (n ^ (0xFFFFFFFFULL << 32)), divRemResult = 0;
        else if (den != 0)
            divResult = n / den, divRemResult = n % den;
        else
            divResult = ((n < 0) ? 1 : -1) ^ (0xFFFFFFFFULL << 32), divRemResult = n;
    }
    else // 64-bit / 32-bit or 64-bit / 64-bit
    {
        if ((divMode == 1 || divMode == 3) && num == INT64_MIN && den == -1) // Overflow
            divResult = num, divRemResult = 0;
        else if (den != 0)
            divResult = num / den, divRemResult = num % den;
        else
            divResult = (num < 0) ? 1 : -1, divRemResult = num;
    }
}

void DivSqrt::squareRoot()
{
    // Calculate the square root result
    sqrtResult = (sqrtCnt & 0x0001) ? sqrtl(sqrtParam) : sqrt((uint32_t)sqrtParam);
}

void DivSqrt::writeDivCnt(uint16_t mask, uint16_t value)
{
    // Write to the DIVCNT register
    mask &= 0x0003;
    divCnt = (divCnt & ~mask) | (value & mask);

    divide();
}

void DivSqrt::writeDivNumerL(uint32_t mask, uint32_t value)
{
    // Write to the DIVNUMER register
    divNumer = (divNumer & ~((uint64_t)mask)) | (value & mask);

    divide();
}

void DivSqrt::writeDivNumerH(uint32_t mask, uint32_t value)
{
    // Write to the DIVNUMER register
    divNumer = (divNumer & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);

    divide();
}

void DivSqrt::writeDivDenomL(uint32_t mask, uint32_t value)
{
    // Write to the DIVDENOM register
    divDenom = (divDenom & ~((uint64_t)mask)) | (value & mask);

    divide();
}

void DivSqrt::writeDivDenomH(uint32_t mask, uint32_t value)
{
    // Write to the DIVDENOM register
    divDenom = (divDenom & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);

    divide();
}

void DivSqrt::writeSqrtCnt(uint16_t mask, uint16_t value)
{
    // Write to the SQRTCNT register
    mask &= 0x0001;
    sqrtCnt = (sqrtCnt & ~mask) | (value & mask);

    squareRoot();
}

void DivSqrt::writeSqrtParamL(uint32_t mask, uint32_t value)
{
    // Write to the DIVDENOM register
    sqrtParam = (sqrtParam & ~((uint64_t)mask)) | (value & mask);

    squareRoot();
}

void DivSqrt::writeSqrtParamH(uint32_t mask, uint32_t value)
{
    // Write to the SQRTPARAM register
    sqrtParam = (sqrtParam & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);

    squareRoot();
}
