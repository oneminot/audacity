/**********************************************************************

  Audacity: A Digital Audio Editor

  NoiseRemoval.h

  Dominic Mazzoni
  Vaughan Johnson (Preview)

**********************************************************************/

#ifndef __AUDACITY_EFFECT_NOISE_REMOVAL__
#define __AUDACITY_EFFECT_NOISE_REMOVAL__

#include "Effect.h"

#include <wx/dialog.h>
#include <wx/slider.h>

class wxButton;
class wxSizer;
class wxString;

class Envelope;
class WaveTrack;

class EffectNoiseRemoval: public Effect {
   
public:
   
   EffectNoiseRemoval();
   virtual ~EffectNoiseRemoval();

   virtual wxString GetEffectName() {
      return wxString(_("Noise Removal..."));
   }
   
   virtual wxString GetEffectAction() {
      if (mDoProfile)
         return wxString(_("Creating Noise Profile"));
      else
         return wxString(_("Removing Noise"));
   }
   
   virtual bool PromptUser();
   virtual bool TransferParameters( Shuttle & shuttle );
   
   virtual bool Init();
   virtual bool CheckWhetherSkipEffect();
   virtual bool Process();
   
private:
   void CleanSpeechMayReadNoisegate();
   void CleanSpeechMayWriteNoiseGate();

   bool ProcessOne(int count, WaveTrack * track,
                   longSampleCount start, sampleCount len);

   void GetProfile(sampleCount len,
                   float *buffer);
   void RemoveNoise(sampleCount len,
                    float *buffer);
   
   Envelope *mEnvelope;

   int       windowSize;
   float    *mNoiseGate;
   float    *sum;
   float    *sumsq;
   float    *smoothing;
   int      *profileCount;
   
   bool      mDoProfile;
   bool      mHasProfile;
   int       mLevel;

friend class NoiseRemovalDialog;
};

// WDR: class declarations

//----------------------------------------------------------------------------
// NoiseRemovalDialog
//----------------------------------------------------------------------------

// Declare window functions

class NoiseRemovalDialog: public wxDialog
{
public:
   // constructors and destructors
   NoiseRemovalDialog(EffectNoiseRemoval * effect,
								wxWindow *parent, wxWindowID id, 
								const wxString &title,
								const wxPoint& pos = wxDefaultPosition,
								const wxSize& size = wxDefaultSize,
								long style = wxDEFAULT_DIALOG_STYLE );

   wxSizer *MakeNoiseRemovalDialog(bool call_fit = true, bool set_sizer = true);
   
private:
   // handlers
   void OnGetProfile( wxCommandEvent &event );
   void OnPreview(wxCommandEvent &event);
   void OnRemoveNoise( wxCommandEvent &event );
   void OnCancel( wxCommandEvent &event );
   
private:
	EffectNoiseRemoval * m_pEffect;

public:
//TIDY-ME: Is mLevel needed in the dialog??
   int  mLevel;
   wxButton * m_pButton_GetProfile;
   wxSlider * m_pSlider;
   wxButton * m_pButton_Preview;
   wxButton * m_pButton_RemoveNoise;
   
private:
   DECLARE_EVENT_TABLE()
};

#endif

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: c42ae8d9-7625-4bf9-a719-e5d082430ed5

