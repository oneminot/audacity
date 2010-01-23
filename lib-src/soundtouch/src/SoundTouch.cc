/*****************************************************************************
 *
 * A main class for tempo/pitch/rate adjusting routines. 
 *
 * Initialize the object by setting up the sound stream parameters with the
 * 'setSampleRate' and 'setChannels' functions, and set the desired
 * tempo/pitch/rate settings with the corresponding functions.
 *
 * Notes:
 * * This class behaves like an first-in-first-out pipe: The samples to be 
 * processed are fed into one of the pipe with the 'putSamples' function,
 * and the ready, processed samples are read from the other end with the 
 * 'receiveSamples' function. 
 *
 * * The tempo/pitch/rate control parameters may freely be altered during 
 * processing.
 *
 * * The processing routines introduce a certain 'latency' between the
 * input and output, so that the inputted samples aren't immediately 
 * transferred to the output, and neither the number of output samples 
 * immediately available after inputting some samples isn't in direct 
 * relationship to the number of previously inputted samples.
 *
 * * This class utilizes classes 'tempochanger' to change tempo of the
 * sound (without changing pitch) and 'transposer' to change rate
 * (that is, both tempo and pitch) of the sound. The third available control 
 * 'pitch' (change pitch but maintain tempo) is produced by suitably 
 * combining the two preceding controls.
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


#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <stdexcept>

using namespace std;

#include "SoundTouch.h"
#include "TDStretch.h"
#include "RateTransposer.h"
#include "mmx.h"


SoundTouch::SoundTouch()
{
    // Initialize rate transposer and tempo changer instances

    pRateTransposer = new RateTransposer;
    pTDStretch = new TDStretch;

    setOutPipe(pTDStretch);

    rate = tempo = 0;

    virtualPitch = 
    virtualRate = 
    virtualTempo = 1.0;

    calcEffectiveRateAndTempo();

    channels = 0;
    bSrateSet = false;
}



SoundTouch::~SoundTouch()
{
    delete pRateTransposer;
    delete pTDStretch;
}



// Sets the number of channels, 1 = mono, 2 = stereo
void SoundTouch::setChannels(const uint numChannels)
{
    if (numChannels != 1 && numChannels != 2) {
        throw runtime_error("Illegal number of channels");
    }
    channels = numChannels;
    pRateTransposer->setChannels(numChannels);
    pTDStretch->setChannels(numChannels);
}



// Sets new rate control value. Normal rate = 1.0, smaller values
// represent slower rate, larger faster rates.
void SoundTouch::setRate(const float newRate)
{
    virtualRate = newRate;
    calcEffectiveRateAndTempo();
}



// Sets new rate control value as a difference in percents compared
// to the original rate (-50 .. +100 %)
void SoundTouch::setRateChange(const float newRate)
{
    virtualRate = 1.0f + 0.01f * newRate;
    calcEffectiveRateAndTempo();
}



// Sets new tempo control value. Normal tempo = 1.0, smaller values
// represent slower tempo, larger faster tempo.
void SoundTouch::setTempo(const float newTempo)
{
    virtualTempo = newTempo;
    calcEffectiveRateAndTempo();
}



// Sets new tempo control value as a difference in percents compared
// to the original tempo (-50 .. +100 %)
void SoundTouch::setTempoChange(const float newTempo)
{
    virtualTempo = 1.0f + 0.01f * newTempo;
    calcEffectiveRateAndTempo();
}



// Sets new pitch control value. Original pitch = 1.0, smaller values
// represent lower pitches, larger values higher pitch.
void SoundTouch::setPitch(const float newPitch)
{
    virtualPitch = newPitch;
    calcEffectiveRateAndTempo();
}



// Sets pitch change in octaves compared to the original pitch
// (-1.00 .. +1.00)
void SoundTouch::setPitchOctaves(const float newPitch)
{
    virtualPitch = (float)exp(0.69314718056f * newPitch);
    calcEffectiveRateAndTempo();
}



// Sets pitch change in semi-tones compared to the original pitch
// (-12 .. +12)
void SoundTouch::setPitchSemiTones(const int newPitch)
{
    setPitchOctaves((float)newPitch / 12.0f);
}



void SoundTouch::setPitchSemiTones(const float newPitch)
{
    setPitchOctaves(newPitch / 12.0f);
}


// Calculates 'effective' rate and tempo values from the
// nominal control values.
void SoundTouch::calcEffectiveRateAndTempo()
{
    float oldTempo = tempo;
    float oldRate = rate;

    tempo = virtualTempo / virtualPitch;
    rate = virtualPitch * virtualRate;

    if (rate != oldRate) pRateTransposer->setRate(rate);
    if (tempo != oldTempo) pTDStretch->setTempo(tempo);

    if (rate > 1.0f) {
        if (output != pRateTransposer) {
            FIFOSamplePipe *transOut;

            st_assert(output == pTDStretch);
            // move samples in the current output buffer to the output of pRateTransposer
            transOut = pRateTransposer->getOutput();
            transOut->moveSamples(*output);
            // move samples in tempo changer's input to pitch transposer's input
            pRateTransposer->moveSamples(*pTDStretch->getInput());

            output = pRateTransposer;
        }
    } else {
        if (output != pTDStretch) {
            FIFOSamplePipe *tempoOut;

            st_assert(output == pRateTransposer);
            // move samples in the current output buffer to the output of pTDStretch
            tempoOut = pTDStretch->getOutput();
            tempoOut->moveSamples(*output);
            // move samples in pitch transposer's store buffer to tempo changer's input
            pTDStretch->moveSamples(*pRateTransposer->getStore());

            output = pTDStretch;

        }
    }
}


// Sets sample rate.
void SoundTouch::setSampleRate(const uint srate)
{
    bSrateSet = true;
    // set sample rate, leave other tempo changer parameters as they are.
    pTDStretch->setParameters(srate);
}


// Adds 'numSamples' pcs of samples from the 'samples' memory position into
// the input of the object.
void SoundTouch::putSamples(const Sample *samples, const uint numSamples)
{
    if (bSrateSet == false) {
        throw runtime_error("SoundTouch : Sample rate not defined");
    } else if (channels == 0) {
        throw runtime_error("SoundTouch : Number of channels not defined");
    }

    // Transpose the rate of the new samples if necessary
    if (rate == 1.0f) {
        // The rate value is same as the original, simply evaluate the tempo changer. 
        st_assert(output == pTDStretch);
        if (pRateTransposer->isEmpty() == 0) {
            // yet flush the last samples in the pitch transposer buffer
            // (may happen if 'rate' changes from a non-zero value to zero)
            pTDStretch->moveSamples(*pRateTransposer);
        }
        pTDStretch->putSamples(samples, numSamples);
    } else if (rate < 1.0f) {
        // transpose the rate down, output the transposed sound to tempo changer buffer
        st_assert(output == pTDStretch);
        pRateTransposer->putSamples(samples, numSamples);
        pTDStretch->moveSamples(*pRateTransposer);
    } else {
        st_assert(rate > 1.0f);
        // evaluate the tempo changer, then transpose the rate up, 
        st_assert(output == pRateTransposer);
        pTDStretch->putSamples(samples, numSamples);
        pRateTransposer->moveSamples(*pTDStretch);
    }
}


// Flushes the last samples from the processing pipeline to the output.
// Clears also the internal processing buffers.
//
// Note: This function is meant for extracting the last samples of a sound
// stream. This function may introduce additional blank samples in the end
// of the sound stream, and thus it's not recommended to call this function
// in the middle of a sound stream.
void SoundTouch::flush()
{
    int i;
    uint nOut;
    Sample buff[128];

    nOut = numSamples();

    memset(buff, 0, 128 * sizeof(Sample));
    // "Push" the last active samples out from the processing pipeline by
    // feeding blank samples into the processing pipeline until new, 
    // processed samples appear in the output (not however, more than 
    // 8ksamples in any case)
    for (i = 0; i < 128; i ++) {
        putSamples(buff, 64);
        if (numSamples() != nOut) break;  // new samples have appeared in the output!
    }

    // Clear working buffers
    pRateTransposer->clear();
    pTDStretch->clearInput();
    // yet leave the 'tempoChanger' output intouched as that's where the
    // flushed samples are!
}


// Changes a setting controlling the processing system behaviour. See the
// 'SETTING_...' defines for available setting ID's.
bool SoundTouch::setSetting(const uint settingId, const uint value)
{
    switch (settingId) {
        case SETTING_USE_AA_FILTER :
            // enables / disabless anti-alias filter
            pRateTransposer->enableAAFilter((value != 0) ? true : false);
            return true;

        case SETTING_AA_FILTER_LENGTH :
            // sets anti-alias filter length
            pRateTransposer->getAAFilter()->setLength(value);
            return true;

        case SETTING_USE_QUICKSEEK :
            // enables / disables tempo routine quick seeking algorithm
            pTDStretch->enableQuickSeek((value != 0) ? true : false);
            return true;

        default :
            return false;
    }
}


// Reads a setting controlling the processing system behaviour. See the
// 'SETTING_...' defines for available setting ID's.
//
// Returns the setting value.
uint SoundTouch::getSetting(const uint settingId) const
{
    switch (settingId) {
        case SETTING_USE_AA_FILTER :
            return pRateTransposer->isAAFilterEnabled();

        case SETTING_AA_FILTER_LENGTH :
            return pRateTransposer->getAAFilter()->getLength();

        case SETTING_USE_QUICKSEEK :
            return pTDStretch->isQuickSeekEnabled();

        default :
            return 0;
    }
}


// Clears all the samples in the object's output and internal processing
// buffers.
void SoundTouch::clear()
{
    pRateTransposer->clear();
    pTDStretch->clear();
}
