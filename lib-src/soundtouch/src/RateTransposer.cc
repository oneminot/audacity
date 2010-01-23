/*****************************************************************************
 * 
 * Sample rate transposer. Changes sample rate by using linear interpolation 
 * together with anti-alias filtering (first order interpolation with anti-
 * alias filtering should be quite adequate for this application)
 *
 * Author        : Copyright (c) Olli Parviainen 2002
 * Author e-mail : oparviai@iki.fi
 * File created  : 13-Jan-2002
 * Last modified : 13-Jan-2002
 *
 * License :
 *
 *  This file is part of SoundTouch sound processing library.
 *
 *  SoundTouch is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  SoundTouch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SoundTouch; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *****************************************************************************/

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "RateTransposer.h"
#include "AAFilter.h"

#ifndef min
#define min(a,b) ((a > b) ? b : a)
#define max(a,b) ((a < b) ? b : a)
#endif

#ifndef SCALE
// fixed-point interpolation routine precision
#if SAMPLES_ARE_SHORT
#define SCALE    10000
#define SCALEI   SCALE
#else
#define SCALEI 10000
#define SCALE  10000.0f
#endif
#endif

// Constructor
RateTransposer::RateTransposer() : FIFOProcessor(&outputBuffer)
{

    uChannels = 2;
    bUseAAFilter = true;

    resetRegisters();

    // Instantiates the anti-alias filter with default tap length
    // of 32
    pAAFilter = new AAFilter(32);

    setRate(1.0f);
}


RateTransposer::~RateTransposer()
{
    delete pAAFilter;
}


void RateTransposer::resetRegisters()
{
    iSlopeCount = 0;
    sPrevSampleL = 
    sPrevSampleR = 0;
}


// Enables/disables the anti-alias filter. Zero to disable, nonzero to enable
void RateTransposer::enableAAFilter(const bool newMode)
{
    bUseAAFilter = newMode;
}


// Returns nonzero if anti-alias filter is enabled.
bool RateTransposer::isAAFilterEnabled() const
{
    return bUseAAFilter;
}


AAFilter *RateTransposer::getAAFilter() const
{
    return pAAFilter;
}


// Transposes the sample rate of the given samples using linear interpolation. 
// 'Mono' version of the routine. Returns the number of samples returned in 
// the "dest" buffer
uint RateTransposer::transposeMono(Sample *dest, const Sample *src, const uint numSamples)
{
    unsigned int i, used;
    BiggerSample temp, vol1;

    used = 0;    
    i = 0;

    // Process the last sample saved from the previous call first...
    while (iSlopeCount <= SCALE) {
        vol1 = SCALE - iSlopeCount;
        temp = vol1 * sPrevSampleL + iSlopeCount * src[0];
        dest[i] = temp / SCALE;
        i++;
        iSlopeCount += uRate;
    }
    // now always (iSlopeCount > SCALE)
    iSlopeCount -= SCALEI;

    for (;;) {
        while (iSlopeCount > SCALE) {
            iSlopeCount -= SCALEI;
            used ++;
            if (used >= numSamples - 1) goto end;
        }
        vol1 = SCALE - iSlopeCount;
        temp = src[used] * vol1 + iSlopeCount * src[used + 1];
        dest[i] = temp / SCALE;

        i++;
        iSlopeCount += uRate;
    }
end:
    // Store the last sample for the next round
    sPrevSampleL = src[numSamples - 1];

    return i;
}


// Transposes the sample rate of the given samples using linear interpolation. 
// 'Stereo' version of the routine. Returns the number of samples returned in 
// the "dest" buffer
uint RateTransposer::transposeStereo(Sample *dest, const Sample *src, const uint numSamples)
{
    unsigned int srcPos, i, used;
    BiggerSample temp, vol1;

    st_assert(numSamples > 0);

    used = 0;    
    i = 0;

    // Process the last sample saved from the sPrevSampleLious call first...
    while (iSlopeCount <= SCALE) {
        vol1 = SCALE - iSlopeCount;
        temp = vol1 * sPrevSampleL + iSlopeCount * src[0];
        dest[2 * i] = temp / SCALE;
        temp = vol1 * sPrevSampleR + iSlopeCount * src[1];
        dest[2 * i + 1] = temp / SCALE;
        i++;
        iSlopeCount += uRate;
    }
    // now always (iSlopeCount > SCALE)
    iSlopeCount -= SCALEI;

    for (;;) {
        while (iSlopeCount > SCALE) {
            iSlopeCount -= SCALEI;
            used ++;
            if (used >= numSamples - 1) goto end;
        }
        srcPos = 2 * used;
        vol1 = SCALE - iSlopeCount;
        temp = src[srcPos] * vol1 + iSlopeCount * src[srcPos + 2];
        dest[2 * i] = temp / SCALE;
        temp = src[srcPos + 1] * vol1 + iSlopeCount * src[srcPos + 3];
        dest[2 * i + 1] = temp / SCALE;

        i++;
        iSlopeCount += uRate;
    }
end:
    // Store the last sample for the next round
    sPrevSampleL = src[2 * numSamples - 2];
    sPrevSampleR = src[2 * numSamples - 1];

    return i;
}


// Sets new target uRate. Normal uRate = 1.0, smaller values represent slower 
// uRate, larger faster uRates.
void RateTransposer::setRate(const float newRate)
{
    float fCutoff;

    uRate = (int)(newRate * SCALE + 0.5f);

    // design a new anti-alias filter
    if (newRate > 1.0f) {
        fCutoff = 0.5f / newRate;
    } else {
        fCutoff = 0.5f * newRate;
    }
    pAAFilter->setCutoffFreq(fCutoff);
}


// Outputs as many samples of the 'outputBuffer' as possible, and if there's
// any room left, outputs also as many of the incoming samples as possible.
// The goal is to drive the outputBuffer empty.
//
// It's allowed for 'output' and 'input' parameters to point to the same
// memory position.
void RateTransposer::flushStoreBuffer()
{
    if (storeBuffer.isEmpty()) return;

    outputBuffer.moveSamples(storeBuffer);
}


// Adds 'numSamples' pcs of samples from the 'samples' memory position into
// the input of the object.
void RateTransposer::putSamples(const Sample *samples, const uint numSamples)
{
    processSamples(samples, numSamples);
}


// Transposes up the sample rate, causing the observed playback 'rate' of the
// sound to decrease
void RateTransposer::upsample(const Sample *src, const uint numSamples)
{
    int count, sizeTemp, num;

    // If the parameter 'uRate' value is smaller than 'SCALE', first transpose
    // the samples and then apply the anti-alias filter to remove aliasing.

    // First check that there's enough room in 'storeBuffer' 
    // (+16 is to reserve some slack in the destination buffer)
    sizeTemp = (int)((SCALEI * (double)numSamples / uRate) + 16);

    // Transpose the samples, store the result into the end of "storeBuffer"
    count = transpose(storeBuffer.ptrEnd(sizeTemp), src, numSamples);
    storeBuffer.putSamples(count);

    // Apply the anti-alias filter to samples in "store output", output the
    // result to "dest"
    num = storeBuffer.numSamples();
    count = pAAFilter->evaluate(outputBuffer.ptrEnd(num), 
        storeBuffer.ptrBegin(), num, uChannels);
    outputBuffer.putSamples(count);

    // Remove the processed samples from "storeBuffer"
    storeBuffer.receiveSamples(count);
}



// Transposes down the sample rate, causing the observed playback 'rate' of the
// sound to increase
void RateTransposer::downsample(const Sample *src, const uint numSamples)
{
    int count, sizeTemp;

    // If the parameter 'uRate' value is larger than 'SCALE', first apply the
    // anti-alias filter to remove high frequencies (prevent them from folding
    // over the lover frequencies), then transpose. */

    // Add the new samples to the end of the storeBuffer */
    storeBuffer.putSamples(src, numSamples);

    // Anti-alias filter the samples to prevent folding and output the filtered 
    // data to tempBuffer. Note : because of the FIR filter length, the
    // filtering routine takes in 'filter_length' more samples than it outputs.
    st_assert(tempBuffer.isEmpty());
    sizeTemp = storeBuffer.numSamples();

    count = pAAFilter->evaluate(tempBuffer.ptrEnd(sizeTemp), 
        storeBuffer.ptrBegin(), sizeTemp, uChannels);

    // Remove the filtered samples from 'storeBuffer'
    storeBuffer.receiveSamples(count);

    // Transpose the samples (+16 is to reserve some slack in the destination buffer)
    sizeTemp = (int)((SCALEI * (double)numSamples / uRate) + 16);

    count = transpose(outputBuffer.ptrEnd(sizeTemp), tempBuffer.ptrBegin(), count);
    outputBuffer.putSamples(count);
}


// Transposes sample rate by applying anti-alias filter to prevent folding. 
// Returns amount of samples returned in the "dest" buffer.
// The maximum amount of samples that can be returned at a time is set by
// the 'set_returnBuffer_size' function.
void RateTransposer::processSamples(const Sample *src, const uint numSamples)
{
    uint count;
    uint sizeReq;

    if (numSamples == 0) return;
    st_assert(pAAFilter);

    if (uRate == SCALE) {
        // Rate not changed from the original; simply pass through the samples
        flushStoreBuffer();
        outputBuffer.putSamples(src, numSamples);
        resetRegisters();
        return;
    }

    // If anti-alias filter is turned off, simply transpose without applying
    // the filter
    if (bUseAAFilter == false) {
        sizeReq = SCALEI * numSamples / uRate + 1;
        count = transpose(outputBuffer.ptrEnd(sizeReq), src, numSamples);
        outputBuffer.putSamples(count);
        return;
    }

    // Transpose with anti-alias filter
    if (uRate < SCALE) {
        upsample(src, numSamples);
    } else  {
        downsample(src, numSamples);
    }
}


// Transposes the sample rate of the given samples using linear interpolation. 
// Returns the number of samples returned in the "dest" buffer
inline uint RateTransposer::transpose(Sample *dest, const Sample *src, const uint numSamples)
{
    if (uChannels == 2) {
        return transposeStereo(dest, src, numSamples);
    } else {
        return transposeMono(dest, src, numSamples);
    }
}


// Sets the number of channels, 1 = mono, 2 = stereo
void RateTransposer::setChannels(const uint numchannels)
{
    if (uChannels == numchannels) return;

    st_assert(numchannels == 1 || numchannels == 2);
    uChannels = numchannels;

    storeBuffer.setChannels(uChannels);
    tempBuffer.setChannels(uChannels);
    outputBuffer.setChannels(uChannels);

    // Inits the linear interpolation registers
    sPrevSampleL = 
    sPrevSampleR = 0;
    iSlopeCount = 0;
}


// Clears all the samples in the object
void RateTransposer::clear()
{
    outputBuffer.clear();
    storeBuffer.clear();
}


// Returns nonzero if there aren't any samples available for outputting.
uint RateTransposer::isEmpty()
{
    int res;

    res = FIFOProcessor::isEmpty();
    if (res == 0) return 0;
    return storeBuffer.isEmpty();
}
