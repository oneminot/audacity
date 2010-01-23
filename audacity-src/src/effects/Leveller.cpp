/**********************************************************************

  Audacity: A Digital Audio Editor

  Leveller.cpp

  Lynn Allan

**********************************************************************/
// For compilers that support precompilation, includes "wx.h".
#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
// Include your minimal set of headers here, or wx.h
#include <wx/wx.h>
#endif

#include <math.h>
#include "../Prefs.h"
#include "Leveller.h"

EffectLeveller::EffectLeveller()
{
   Init();
}

#define NUM_PASSES_CHOICES 6
static wxString numPasses[] = { wxT("None-Skip"), wxT("Light"), wxT("Moderate"), wxT("Heavy"), wxT("Heavier"), wxT("Heaviest") };
static double gFrameSum; // odd ... having this as member var crashed on exit

bool EffectLeveller::Init()
{
   mLevellerNumPasses = gPrefs->Read(wxT("/CsPresets/LevellerNumPasses"), 2L);
   if ((mLevellerNumPasses < 0) || (mLevellerNumPasses >= NUM_PASSES_CHOICES)) {  // corrupted Prefs?
      mLevellerNumPasses = 0;
      gPrefs->Write(wxT("/CsPresets/LevellerNumPasses"), 0);
   }
   mLevellerDbChoiceIndex = gPrefs->Read(wxT("/CsPresets/LevellerDbChoiceIndex"), 10L);
   if ((mLevellerDbChoiceIndex < 0) || (mLevellerDbChoiceIndex >= Enums::NumDbChoices)) {  // corrupted Prefs?
      mLevellerDbChoiceIndex = (Enums::NumDbChoices - 1);  //Off-skip
      gPrefs->Write(wxT("/CsPresets/LevellerDbChoiceIndex"), mLevellerDbChoiceIndex);
   }
   mLevellerDbSilenceThreshold = Enums::Db2Signal[mLevellerDbChoiceIndex];

   CalcLevellerFactors();
   
   return true;
}

bool EffectLeveller::CheckWhetherSkipEffect()
{
   bool rc = ((mLevellerDbChoiceIndex >= (Enums::NumDbChoices - 1)) || (mLevellerNumPasses == 0));
   return rc;
}

void EffectLeveller::End()
{
   int frameSum = (int)gFrameSum;
   gPrefs->Write(wxT("/Validate/LevellerFrameSum"), frameSum);
}

// Debug filenames
// X:\CleanSpeech\testsermons\Ifl_050325_0830_LoudSoft_Quiet_Orig_16000.wav
// X:\CleanSpeech\testsermons\Ifl_050325_0830_LoudSoft_HighNoise.wav
// X:\CleanSpeech\testsermons\Ifl_050325_0830_LoudSoft_LowNoise_NormDeclickNr.wav
// X:\CleanSpeech\testsermons\ThatsIt_NormNrDeclicked.wav
// X:\CleanSpeech\LevellerTest.wav
// X:\CleanSpeech\testsermons\Edwards_SinnersInTheHandsOfAnAngryGod_Deut3235.mp3
// X:\CleanSpeech\testsermons\TypicalSpeech.wav
// X:\CleanSpeech\TruncSilenceTest.wav
#define LEVELER_FACTORS 6
static double gLimit[LEVELER_FACTORS] = { 0.0001, 0.0, 0.1, 0.3, 0.5, 1.0 };
static double gAdjLimit[LEVELER_FACTORS];
static double gAddOnValue[LEVELER_FACTORS];
// static double gAdjFactor[LEVELER_FACTORS] = { 0.9, 1.0, 1.1, 1.1, 1.0, 0.9 };
static double gAdjFactor[LEVELER_FACTORS] = { 0.80, 1.00, 1.20, 1.20, 1.00, 0.80 };

void EffectLeveller::CalcLevellerFactors()
{
   gFrameSum            = 0.0;
   gLimit[1]            = mLevellerDbSilenceThreshold;
   int    prev          = 0;
   double addOnValue    = 0.0;
   double prevLimit     = 0.0;
   double limit         = gLimit[0];
   gAddOnValue[0]       = addOnValue;
   double lowerAdjLimit = 0.0;
   double adjFactor     = gAdjFactor[0];
   double upperAdjLimit = gLimit[0] * adjFactor;
   double prevAdjLimit  = upperAdjLimit;
   gAdjLimit[0]         = upperAdjLimit;

   for (int f = 1; f < LEVELER_FACTORS; ++f) {
      prev          = f - 1;
      adjFactor     = gAdjFactor[f];
      lowerAdjLimit = gAdjLimit[prev];
      prevLimit     = gLimit[prev];
      limit         = gLimit[f];
      prevAdjLimit  = gAdjLimit[prev];
      addOnValue    = prevAdjLimit - (adjFactor * prevLimit);
      upperAdjLimit = (adjFactor * limit) + addOnValue;
         
      gAddOnValue[f] = addOnValue;
      gAdjLimit[f]   = (adjFactor * limit) + addOnValue;
   }
}

bool EffectLeveller::PromptUser()
{
   LevellerDialog dlog(mParent, -1, _("Leveller"));
   dlog.mLevellerDbChoiceIndex = mLevellerDbChoiceIndex;
   dlog.mLevellerNumPasses = mLevellerNumPasses;
   dlog.TransferDataToWindow();

   dlog.CentreOnParent();
   dlog.ShowModal();

   if (!dlog.GetReturnCode()) {
      return false;
   }
   mLevellerNumPasses = dlog.mLevellerNumPasses;
   mLevellerDbChoiceIndex = dlog.mLevellerDbChoiceIndex;
   mLevellerDbSilenceThreshold = Enums::Db2Signal[mLevellerDbChoiceIndex];
   gPrefs->Write(wxT("/CsPresets/LevellerDbChoiceIndex"), mLevellerDbChoiceIndex);
   gPrefs->Write(wxT("/CsPresets/LevellerNumPasses"), mLevellerNumPasses);

   CalcLevellerFactors();

   return true;
}

bool EffectLeveller::TransferParameters( Shuttle & shuttle )
{  
   shuttle.TransferEnum(wxT("Db"),mLevellerDbChoiceIndex,Enums::NumDbChoices,Enums::GetDbChoices());
   shuttle.TransferInt(wxT("Passes"),mLevellerNumPasses,1);
   return true;
}

float EffectLeveller::LevelOneFrame(float frameInBuffer)
{
   float  curFrame;
   float  fabsCurFrame;
   float  curSign;

   curFrame = frameInBuffer;
   if (curFrame < 0.0) {
      curSign = -1.0;
   }
   else {
      curSign = 1.0;
   }
   fabsCurFrame = (float)fabs(curFrame);
   gFrameSum += fabsCurFrame;

   for (int f = 0; f < LEVELER_FACTORS; ++f) {
     if (fabsCurFrame <= gLimit[f]) {
        curFrame *= (float)gAdjFactor[f];
        curFrame += (float)(gAddOnValue[f] * curSign);
        return curFrame;
     }
   }
   return (float)0.99;
}

bool EffectLeveller::ProcessSimpleMono(float *buffer, sampleCount len)
{
   for (int pass = 0; pass < mLevellerNumPasses; ++pass) {
      for (int i = 0; i < len; ++i) {
         buffer[i] = LevelOneFrame(buffer[i]);
      }
   }
   return true;
}

//----------------------------------------------------------------------------
// LevellerDialog
//----------------------------------------------------------------------------

#define ID_DB_SILENCE_THRESHOLD_CHOICE 7001
#define ID_DB_NUM_PASSES_CHOICE 7002

BEGIN_EVENT_TABLE(LevellerDialog,wxDialog)
   EVT_BUTTON( wxID_OK, LevellerDialog::OnOk )
   EVT_BUTTON( wxID_CANCEL, LevellerDialog::OnCancel )
END_EVENT_TABLE()

LevellerDialog::LevellerDialog(wxWindow *parent, wxWindowID id,
                             const wxString &title ) :
   wxDialog( parent, id, title)
{
   wxBoxSizer       *mainSizer = new wxBoxSizer(wxVERTICAL);
   wxControl        *item;

   wxStaticText *statText = new wxStaticText(this, -1,
                           _("Leveller by Lynn Allan"));
   mainSizer->Add(statText, 0, wxALIGN_CENTRE | wxALL, 5);
   statText = new wxStaticText(this, -1,
                           _("Equalize soft and loud sections ... a key to better listening"));
   mainSizer->Add(statText, 0, wxALIGN_CENTRE | wxALL, 5);

   wxStaticBoxSizer *group = new wxStaticBoxSizer(new wxStaticBox(this, -1, _("Degree of Leveling")), wxVERTICAL);;
   item = new wxStaticText(this, -1,
                           _("You want to preclude the listener feeling the need to increase\nthe volume during soft sections and decreasing during loud parts.\nSome distortion can result from excessive leveling, so try to minimize\nthis with good equipment, recording technique, the speaker not\nusing too much dynamic range, and perhaps 'working the mic'."),
                           wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT );
   group->Add(item, 0, wxALIGN_CENTRE|wxALL, 5 );
   wxBoxSizer *hSizer = new wxBoxSizer(wxHORIZONTAL);
//   wxString numPasses[] = { "None-Skip", "Light", "Moderate", "Heavy", "Heavier", "Heaviest" };
   statText = new wxStaticText(this, -1, _("Degree of Leveling: "));
   hSizer->Add(statText, 0, wxALIGN_CENTRE | wxALL, 5);

   mLevellerNumPassesChoice = new wxChoice(this, ID_DB_NUM_PASSES_CHOICE,
                                       wxDefaultPosition, wxSize(72, -1), NUM_PASSES_CHOICES,
                                       numPasses);
   hSizer->Add(mLevellerNumPassesChoice, 0, wxALIGN_LEFT|wxALL, 5);
   group->Add(hSizer, 0, wxALIGN_CENTRE|wxALL, 5 );
   mainSizer->Add(group, 0, wxALIGN_CENTRE | wxALL, 5);

   group = new wxStaticBoxSizer(new wxStaticBox(this, -1,
                                                _("Noise Threshold (Hiss/Hum/Ambient Noise)")), wxVERTICAL);
   item = new wxStaticText(this, -1,
                           _("Below the Noise Threshold, the Signal Strength is reduced.\nAbove this Threshold, the Signal Strength is equalized so that\nsoft parts are made louder and loud parts are made softer.\n\nGenerally, recordings with low noise can use lower settings. so\ngood equipment, technique, and NoiseRemoval help out a LOT."),
                           wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT );
   group->Add(item, 0, wxALIGN_CENTRE|wxALL, 5 );

   hSizer = new wxBoxSizer(wxHORIZONTAL);
   statText = new wxStaticText(this, -1, _("Theshold for Noise: "));
   hSizer->Add(statText, 0, wxALIGN_CENTRE | wxALL, 5);

   mLevellerDbSilenceThresholdChoice = new wxChoice(this, ID_DB_SILENCE_THRESHOLD_CHOICE,
      wxDefaultPosition, wxSize(64, -1), Enums::NumDbChoices,
                                       Enums::GetDbChoices());
   hSizer->Add(mLevellerDbSilenceThresholdChoice, 0, wxALIGN_CENTER | wxALL, 4);
   group->Add(hSizer, 0, wxALIGN_CENTRE|wxALL, 5 );
   mainSizer->Add(group, 0, wxALIGN_CENTRE | wxALL, 5);

   hSizer = new wxBoxSizer(wxHORIZONTAL);
   wxButton *cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   hSizer->Add(cancel, 0, wxALIGN_CENTRE|wxALL, 5);

   wxButton *ok = new wxButton(this, wxID_OK, _("OK"));
   ok->SetDefault();
   ok->SetFocus();
   hSizer->Add(ok, 0, wxALIGN_CENTRE|wxALL, 5);

   mainSizer->Add(hSizer, 0, wxALIGN_CENTRE|wxALIGN_CENTER_VERTICAL|wxALL, 5);

   SetAutoLayout(true);
   SetSizer(mainSizer);
   mainSizer->Fit(this);
   mainSizer->SetSizeHints(this);
}

bool LevellerDialog::TransferDataToWindow()
{
   mLevellerDbSilenceThresholdChoice->SetSelection(mLevellerDbChoiceIndex);
   mLevellerNumPassesChoice->SetSelection(mLevellerNumPasses);

   TransferDataFromWindow();

   return true;
}

bool LevellerDialog::TransferDataFromWindow()
{
   mLevellerDbChoiceIndex = mLevellerDbSilenceThresholdChoice->GetSelection();
   mLevellerNumPasses = mLevellerNumPassesChoice->GetSelection();

   return true;
}

void LevellerDialog::OnOk(wxCommandEvent &event)
{
   TransferDataFromWindow();

   EndModal(true);
}

void LevellerDialog::OnCancel(wxCommandEvent &event)
{
   EndModal(false);
}

