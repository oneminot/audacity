/**********************************************************************

  Audacity: A Digital Audio Editor

  LoadEffects.cpp

  Dominic Mazzoni

**********************************************************************/

#include "LoadEffects.h"
#include "../Audacity.h"

#include "Effect.h"

#include "Amplify.h"
// #include "AvcCompressor.h"
#include "BassBoost.h"
#include "ChangeSpeed.h"
#include "Compressor.h"
#include "Echo.h"
#include "Equalization.h"
#include "Fade.h"
#include "Filter.h"
#include "Invert.h"
#include "Leveller.h"
#include "Noise.h"
#include "NoiseRemoval.h"
#include "Normalize.h"
#include "Phaser.h"
#include "Repeat.h"
#include "Reverse.h"
#include "Silence.h"
#include "StereoToMono.h"
#include "ToneGen.h"
#include "TruncSilence.h"
#include "Wahwah.h"

#ifdef USE_SOUNDTOUCH
#include "ChangePitch.h"
#include "ChangeTempo.h"
#endif

#ifdef USE_WAVELET
#include "WaveletDenoise.h"
#endif

#ifdef USE_NYQUIST
#include "nyquist/LoadNyquist.h"
#endif

#ifdef USE_AUDIO_UNITS
#include "audiounits/LoadAudioUnits.h"
#endif

// VST is separate now
//#ifdef __WXMAC__
//#include "VST/LoadVSTMac.h"
//#endif

#if defined(__WXMSW__) && !defined(__CYGWIN__)
#include "VST/LoadVSTWin.h"
#endif

#ifdef USE_LADSPA
#include "ladspa/LoadLadspa.h"
#endif

void LoadEffects()
{
   // Generate menu
   Effect::RegisterEffect(new EffectNoise());
   Effect::RegisterEffect(new EffectSilence());
   Effect::RegisterEffect(new EffectToneGen());

   // Effect menu
   
   Effect::RegisterEffect(new EffectAmplify());

   //Commented out now that the Compressor effect works better
   //Effect::RegisterEffect(new EffectAvcCompressor());

   const int SIMPLE_EFFECT = BUILTIN_EFFECT | PROCESS_EFFECT;
   // In this list, designating an effect as 'SIMPLE_EFFECT' just means
   // that it should be included in even the most basic of menus.
   // This was introduced for CleanSpeech mode.
   Effect::RegisterEffect(new EffectBassBoost());
   Effect::RegisterEffect(new EffectChangeSpeed());
	#ifdef USE_SOUNDTOUCH
		Effect::RegisterEffect(new EffectChangePitch());
		Effect::RegisterEffect(new EffectChangeTempo());
	#endif
   Effect::RegisterEffect(new EffectCompressor());
   Effect::RegisterEffect(new EffectEcho());
   Effect::RegisterEffect(new EffectEqualization());
   Effect::RegisterEffect(new EffectFadeIn(), SIMPLE_EFFECT);
   Effect::RegisterEffect(new EffectFadeOut(), SIMPLE_EFFECT);
   Effect::RegisterEffect(new EffectFilter());
   Effect::RegisterEffect(new EffectInvert());
   Effect::RegisterEffect(new EffectLeveller(), SIMPLE_EFFECT);
   Effect::RegisterEffect(new EffectNoiseRemoval(), SIMPLE_EFFECT);
   Effect::RegisterEffect(new EffectNormalize(), SIMPLE_EFFECT);
   Effect::RegisterEffect(new EffectPhaser());
   Effect::RegisterEffect(new EffectRepeat());
   Effect::RegisterEffect(new EffectReverse());
   Effect::RegisterEffect(new EffectStereoToMono(), HIDDEN_EFFECT);// NOT in normal effects list.
   Effect::RegisterEffect(new EffectTruncSilence(), SIMPLE_EFFECT);
   Effect::RegisterEffect(new EffectWahwah());

   // Analyze menu
   // [nothing built-in, but plug-ins might go here]


#ifdef USE_WAVELET
   Effect::RegisterEffect(new EffectWaveletDenoise());
#endif

#ifdef USE_NYQUIST
   LoadNyquistPlugins();
#endif

   // VST is separate now
   //#if defined(__WXMAC__) || defined(__WXMSW__)  && !defined(__CYGWIN__)
   //LoadVSTPlugins();
   //#endif

#ifdef USE_LADSPA
   LoadLadspaPlugins();
#endif

#ifdef USE_AUDIO_UNITS
   LoadAudioUnits();
#endif
}

void UnloadEffects()
{
   Effect::UnregisterEffects();
}

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: 3a6c5930-9015-4fd0-96f2-2eb31da1c785

