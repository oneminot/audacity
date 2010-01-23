/**********************************************************************

  Audacity: A Digital Audio Editor

  AudioUnitEffect.h

  Dominic Mazzoni

**********************************************************************/

#include <wx/dialog.h>

#include "../Effect.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>
#include <Carbon/Carbon.h>
#include <AudioUnit/AUNTComponent.h>
#include <AudioUnit/AudioUnitProperties.h>
#include <AudioUnit/AudioUnitCarbonView.h>

class AudioUnitEffect:public Effect {

 public:

   AudioUnitEffect(wxString name, Component component);
   virtual ~AudioUnitEffect();

   virtual wxString GetEffectName();
   
   virtual wxString GetEffectAction();

   virtual int GetEffectFlags();
 
   virtual bool Init();

   virtual bool PromptUser();
   
   virtual bool Process();
   
   virtual void End();

 private:
   bool SetRateAndChannels(AudioUnit unit,
                           int numChannels, Float64 sampleRate);

   void GetSamples(WaveTrack *track,
                   longSampleCount *start,
                   sampleCount *len);

   bool ProcessStereo(int count, WaveTrack * left, WaveTrack *right,
                      longSampleCount lstart, longSampleCount rstart,
                      sampleCount len);

   bool DoRender(AudioUnit unit, int numChannels,
                 float *leftBuffer, float *rightBuffer,
                 int len, int unitBlockSize,
                 AudioTimeStamp *timeStamp);

   bool CopyParameters(AudioUnit srcUnit, AudioUnit dstUnit);

   static OSStatus
      SimpleAudioRenderCallback(void *inRefCon, 
                                AudioUnitRenderActionFlags *inActionFlags,
                                const AudioTimeStamp *inTimeStamp, 
                                UInt32 inBusNumber,
                                UInt32 inNumFrames, 
                                AudioBufferList *ioData);

   Component GetCarbonViewComponent(OSType subtype);
 
   wxString    mName;
   Component   mComponent;
   AudioUnit   mUnit;
   bool        mSupportsMono;
   bool        mSupportsStereo;
   float      *mLeftBufferForCallback;
   float      *mRightBufferForCallback;
};


// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: 

