/**********************************************************************

  Audacity: A Digital Audio Editor

  Nyquist.cpp

  Dominic Mazzoni

**********************************************************************/

#include <math.h>

#include <wx/defs.h>
#include <wx/log.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <wx/textfile.h>

#include "../../Audacity.h"
#include "../../AudacityApp.h"
#include "../../LabelTrack.h"
#include "../../Internat.h"

#include "Nyquist.h"

#include <locale.h>

#include <wx/arrimpl.cpp>
WX_DEFINE_OBJARRAY(NyqControlArray);

wxString EffectNyquist::mXlispPath;

extern "C" {
   extern void set_xlisp_path(const char *p);
}

#ifdef USE_NYQUIST
	#ifdef __WXMSW__ // Not for Linux because stdlib.h for gcc defines its own random().
		/* vjohnson */
		extern "C" {
		long random_seed = 1534781L;

		short random(short lo, short hi)
		{
			random_seed *= 13L;
			random_seed += 1874351L;
			return((short)(lo + (((hi + 1 - lo) * ((0x00ffff00 & random_seed) >> 8)) >> 16)));
		}
		}
	#endif
#endif


#define UNINITIALIZED_CONTROL ((double)99999999.99)

wxString EffectNyquist::UnQuote(wxString s)
{
   wxString out;
   int len = s.Length();
   
   if (len>=2 && s.GetChar(0)=='\"' && s.GetChar(len-1)=='\"')
      return s.Mid(1, len-2);
   else
      return s;
}

double EffectNyquist::GetCtrlValue(wxString s)
{
   if (s == "rate") {
      TrackListIterator iter(mWaveTracks);
      return ((WaveTrack *)iter.First())->GetRate();
   }
   else {
      double d;
      s.ToDouble(&d);
      return d;
   }
}

void EffectNyquist::Parse(wxString line)
{
   wxArrayString tokens;

   int i;
   int len = line.Length();
   bool sl = false;
   bool q = false;
   wxString tok = "";
   for(i=1; i<len; i++) {
      if (line.GetChar(i)=='\\')
         sl = true;
      else if (line.GetChar(i)=='"')
         q = !q;
      else {
         if (!q && !sl && line.GetChar(i)==' ' || line.GetChar(i)=='\t') {
            tokens.Add(tok);
            tok = "";
         }
         else if (sl && line.GetChar(i)=='n')
            tok += '\n';
         else
            tok += line.GetChar(i);
         
         sl = false;
      }
   }
   if (tok != "")
      tokens.Add(tok);

   len = tokens.GetCount();
   if (len < 1)
      return;

   if (len==2 && tokens[0]=="nyquist" && tokens[1]=="plug-in") {
      mOK = true;
      return;
   }

   if (len>=2 && tokens[0]=="type") {
      if (tokens[1]=="process")
         mFlags = PROCESS_EFFECT | PLUGIN_EFFECT;
      if (tokens[1]=="generate")
         mFlags = INSERT_EFFECT | PLUGIN_EFFECT;
      if (tokens[1]=="analyze")
         mFlags = ANALYZE_EFFECT | PLUGIN_EFFECT;
      return;
   }

   // We're on verison 1.  Reject if it's an earlier version.
   if (len>=2 && tokens[0]=="version" && tokens[1]!="1") {
      mOK = false;
      return;
   }

   if (len>=2 && tokens[0]=="name") {
      mName = UnQuote(tokens[1]);
      return;
   }

   if (len>=2 && tokens[0]=="action") {
      mAction = UnQuote(tokens[1]);
      return;
   }

   if (len>=2 && tokens[0]=="info") {
      mInfo = UnQuote(tokens[1]);
      return;
   }

   if (len>=8 && tokens[0]=="control") {
      NyqControl ctrl;

      ctrl.var = tokens[1];
      ctrl.name = tokens[2];
      ctrl.type = 0;
      if (tokens[3]=="real")
         ctrl.type = 1;
      ctrl.label = tokens[4];
      ctrl.valStr = tokens[5];
      ctrl.lowStr = tokens[6];
      ctrl.highStr = tokens[7];
      ctrl.val = UNINITIALIZED_CONTROL;

      mControls.Add(ctrl);
   }
}

void EffectNyquist::ParseFile()
{
   wxTextFile f(FILENAME(mFileName.GetFullPath()));
   if (!f.Open())
      return;

   mCmd = "";
   mFlags = PROCESS_EFFECT | PLUGIN_EFFECT;
   mOK = false;
   mControls.Clear();

   int i;
   int len = f.GetLineCount();
   wxString line;
   for(i=0; i<len; i++) {
      line = f[i];
      if (line.Length()>1 && line.GetChar(0)==';')
         Parse(line);
      else
         mCmd += line + "\n";
   }
}

EffectNyquist::EffectNyquist(wxString fName)
{
   mInteractive = false;
   mAction = _("Applying Nyquist Effect...");

   if (fName == "") {
      // Interactive Nyquist
      mOK = true;
      mInteractive = true;
      mCmd = "";
      mName = _("Nyquist Prompt...");
      mFlags = PROCESS_EFFECT | BUILTIN_EFFECT;
      return;
   }

   wxLogNull dontLog;

   mName = wxFileName(fName).GetName();
   mFileName = wxFileName(FILENAME(fName));
   mFileModified = mFileName.GetModificationTime();
   ParseFile();
}

EffectNyquist::~EffectNyquist()
{
}

bool EffectNyquist::SetXlispPath()
{
   wxString fname;

   fname = mXlispPath + wxFILE_SEP_PATH + "nyinit.lsp";
   if (!(::wxFileExists(FILENAME(fname))))
      mXlispPath = "";

   if (mXlispPath == "") {
      wxArrayString audacityPathList = wxGetApp().audacityPathList;
      wxArrayString pathList;
      wxArrayString files;
      unsigned int i;

      for(i=0; i<audacityPathList.GetCount(); i++) {
         wxString prefix = audacityPathList[i] + wxFILE_SEP_PATH;
         wxGetApp().AddUniquePathToPathList(prefix + "nyquist",
                                            pathList);
      }

      wxGetApp().FindFilesInPathList("nyquist.lsp", pathList, wxFILE, files);

      if (files.GetCount() > 0) {
         mXlispPath = ::wxPathOnly(files[0]);
      }
   }

   set_xlisp_path((const char *)mXlispPath);

   fname = mXlispPath + wxFILE_SEP_PATH + "nyinit.lsp";
   return ::wxFileExists(FILENAME(fname));
}

bool EffectNyquist::PromptUser()
{
   if (!SetXlispPath())
      return false;
   
   if (mInteractive) {
      wxString temp;
      wxString title = _("Nyquist Prompt");
      wxString caption = _("Enter Nyquist Command: ");

      temp = wxGetTextFromUser(caption, title,
                               mCmd, mParent, -1, -1, TRUE);
      if (temp == "")
         return false;
      
      mCmd = temp;
      return true;
   }

   if (mFileName.GetModificationTime().IsLaterThan(mFileModified)) {
      ParseFile();
      mFileModified = mFileName.GetModificationTime();
   }

   if (mControls.GetCount() == 0)
      return true;

   for(unsigned int i=0; i<mControls.GetCount(); i++) {
      NyqControl *ctrl = &mControls[i];

      if (ctrl->val == UNINITIALIZED_CONTROL)
         ctrl->val = GetCtrlValue(ctrl->valStr);

      ctrl->low = GetCtrlValue(ctrl->lowStr);
      ctrl->high = GetCtrlValue(ctrl->highStr);

      if (ctrl->high < ctrl->low)
         ctrl->high = ctrl->low + 1;
      if (ctrl->val < ctrl->low)
         ctrl->val = ctrl->low;
      if (ctrl->val > ctrl->high)
         ctrl->val = ctrl->high;

      ctrl->ticks = 1000;
      if (ctrl->type==0 &&
          (ctrl->high - ctrl->low < ctrl->ticks))
         ctrl->ticks = (int)(ctrl->high - ctrl->low);
   }

   NyquistDialog dlog(mParent, -1, mName, mInfo, &mControls);
   dlog.CentreOnParent();
   dlog.ShowModal();

   if (!dlog.GetReturnCode())
      return false;

   return true;
}

bool EffectNyquist::Process()
{
   TrackListIterator iter(mWaveTracks);
   mCurTrack[0] = (WaveTrack *) iter.First();
   mOutputTime = mT1 - mT0;
   mCount = 0;
   mProgress = 0;
   while (mCurTrack[0]) {
      mCurNumChannels = 1;
      double trackStart = mCurTrack[0]->GetStartTime();
      double trackEnd = mCurTrack[0]->GetEndTime();
      double t0 = mT0 < trackStart? trackStart: mT0;
      double t1 = mT1 > trackEnd? trackEnd: mT1;
      if (t1 >= t0) {
         if (mCurTrack[0]->GetLinked()) {
            mCurNumChannels = 2;

            mCurTrack[1] = (WaveTrack *)iter.Next();
            if (mCurTrack[1]->GetRate() != mCurTrack[0]->GetRate()) {
               wxMessageBox(_("Sorry, cannot apply effect on stereo tracks "
                            "where the tracks don't match."), "Nyquist",
                            wxOK | wxCENTRE, mParent);
               return false;
            }
            mCurStart[1] = mCurTrack[1]->TimeToLongSamples(t0);
         }

         mCurStart[0] = mCurTrack[0]->TimeToLongSamples(t0);
         longSampleCount end = mCurTrack[0]->TimeToLongSamples(t1);
         mCurLen = (sampleCount)(end - mCurStart[0]);

         if (!ProcessOne())
            return false;
      }

      mCurTrack[0] = (WaveTrack *) iter.Next();
      mCount += mCurNumChannels;
   }

   mT1 = mT0 + mOutputTime;

   return true;
}


int EffectNyquist::GetCallback(float *buffer, int ch,
                               long start, long len)
{
   if (mCurBuffer[ch]) {
      if ((mCurStart[ch] + start) < mCurBufferStart[ch] ||
          (mCurStart[ch] + start)+len >
          mCurBufferStart[ch]+mCurBufferLen[ch]) {
         delete[] mCurBuffer[ch];
         mCurBuffer[ch] = NULL;
      }
   }

   if (!mCurBuffer[ch]) {
      mCurBufferStart[ch] = (mCurStart[ch] + start);
      mCurBufferLen[ch] = mCurTrack[ch]->GetBestBlockSize(mCurBufferStart[ch]);
      if (mCurBufferLen[ch] < len)
         mCurBufferLen[ch] = mCurTrack[ch]->GetIdealBlockSize();
      if (mCurBufferStart[ch] + mCurBufferLen[ch] > mCurStart[ch] + mCurLen)
         mCurBufferLen[ch] = mCurStart[ch] + mCurLen - mCurBufferStart[ch];
      mCurBuffer[ch] = NewSamples(mCurBufferLen[ch], floatSample);
      if (!mCurTrack[ch]->Get(mCurBuffer[ch], floatSample,
                              mCurBufferStart[ch], mCurBufferLen[ch])) {

         printf("GET error\n");

         return -1;
      }
   }

   long offset = (mCurStart[ch] + start) - mCurBufferStart[ch];
   CopySamples(mCurBuffer[ch] + offset*SAMPLE_SIZE(floatSample), floatSample,
               (samplePtr)buffer, floatSample,
               len);

   if (ch==0) {
      double progress = 1.0*(start+len)/mCurLen;
      if (progress > mProgress)
         mProgress = progress;

      if (TotalProgress(mProgress))
         return -1;
   }

   return 0;
}

int EffectNyquist::PutCallback(float *buffer, int channel,
                               long start, long len)
{
   
   if (mOutputTrack[channel]->Append((samplePtr)buffer, floatSample, len))
      return 0;  // success
   else
      return -1; // failure
}

int EffectNyquist::StaticGetCallback(float *buffer, int channel,
                                     long start, long len,
                                     void *userdata)
{
   EffectNyquist *This = (EffectNyquist *)userdata;
   return This->GetCallback(buffer, channel, start, len);
}

int EffectNyquist::StaticPutCallback(float *buffer, int channel,
                                     long start, long len,
                                     void *userdata)
{
   EffectNyquist *This = (EffectNyquist *)userdata;
   return This->PutCallback(buffer, channel, start, len);
}

bool EffectNyquist::ProcessOne()
{
   // libnyquist breaks except in LC_NUMERIC=="C".
   //
   // MB: setlocale is not thread-safe.  Should use uselocale()
   //     if available, or fix libnyquist to be locale-independent.
   setlocale(LC_NUMERIC, "C");

   nyx_rval rval;

   nyx_init();
   nyx_set_input_audio(StaticGetCallback, (void *)this,
                       mCurNumChannels,
                       mCurLen, mCurTrack[0]->GetRate());

   wxString cmd;
   for(unsigned int j=0; j<mControls.GetCount(); j++)
      cmd = cmd+wxString::Format("(setf %s %f)\n",
                                 (const char *)mControls[j].var,
                                 mControls[j].val);
   
   cmd += mCmd;

   int i;
	for (i = 0; i < mCurNumChannels; i++)
		mCurBuffer[i] = NULL;
   rval = nyx_eval_expression(cmd);

   if (rval == nyx_string) {
      wxMessageBox(nyx_get_string(), "Nyquist",
                   wxOK | wxCENTRE, mParent);
      nyx_cleanup();
      return true;
   }

   if (rval == nyx_double) {
      wxString str;
      str.Printf(_("Nyquist returned the value: %f"), nyx_get_double());
      wxMessageBox(str, "Nyquist",
                   wxOK | wxCENTRE, mParent);
      nyx_cleanup();
      return true;
   }

   if (rval == nyx_int) {
      wxString str;
      str.Printf(_("Nyquist returned the value: %d"), nyx_get_int());
      wxMessageBox(str, "Nyquist",
                   wxOK | wxCENTRE, mParent);
      nyx_cleanup();
      return true;
   }

   if (rval == nyx_labels) {
      int numLabels = nyx_get_num_labels();
      int l;
      LabelTrack *ltrack = NULL;

      TrackListIterator iter(mTracks);
      Track *t = iter.First();
      while(t && !ltrack) {
         if (t->GetKind() == Track::Label)
            ltrack = (LabelTrack *)t;
         t = iter.Next();
      }
      
      if (!ltrack) {
         ltrack = mFactory->NewLabelTrack();
         mTracks->Add((Track *)ltrack);
      }

      for(l=0; l<numLabels; l++) {
         double t;
         const char *str;

         nyx_get_label(l, &t, &str);

         ltrack->AddLabel(t + mT0, t + mT0, str);
      }

      return true;
   }

   if (rval != nyx_audio) {
      wxMessageBox(_("Nyquist did not return audio.\n"), "Nyquist",
                   wxOK | wxCENTRE, mParent);
      nyx_cleanup();
      return false;
   }
   
   int outChannels;

   outChannels = nyx_get_audio_num_channels();
   if (outChannels > mCurNumChannels) {
      wxMessageBox(_("Nyquist returned too many audio channels.\n"),
                   "Nyquist",
                   wxOK | wxCENTRE, mParent);
      return false;
   }

   double rate = mCurTrack[0]->GetRate();
   for(i=0; i<outChannels; i++) {
      sampleFormat format = mCurTrack[i]->GetSampleFormat();
      mOutputTrack[i] = mFactory->NewWaveTrack(format);

      if (outChannels == mCurNumChannels)
         rate = mCurTrack[i]->GetRate();

      mOutputTrack[i]->SetRate( rate );
      mCurBuffer[i] = NULL;
   }

   nyx_get_audio(StaticPutCallback, (void *)this);

   for(i=0; i<outChannels; i++) {
      mOutputTrack[i]->Flush();
      if (mCurBuffer[i])
         DeleteSamples(mCurBuffer[i]);
      mOutputTime = mOutputTrack[i]->GetEndTime();
   }

   for(i=0; i<mCurNumChannels; i++) {
      WaveTrack *out;

      if (outChannels == mCurNumChannels)
         out = mOutputTrack[i];
      else
         out = mOutputTrack[0];

      mCurTrack[i]->Clear(mT0, mT1);
      mCurTrack[i]->Paste(mT0, out);
   }

   for(i=0; i<outChannels; i++)
      delete mOutputTrack[i];

   nyx_cleanup();

   // Reset locale
   setlocale(LC_NUMERIC, "");
   
   return true;
}

/**********************************************************/

#define ID_NYQ_SLIDER 2000
#define ID_NYQ_TEXT   3000

BEGIN_EVENT_TABLE(NyquistDialog, wxDialog)
   EVT_BUTTON(wxID_OK, NyquistDialog::OnOk)
   EVT_BUTTON(wxID_CANCEL, NyquistDialog::OnCancel)
   EVT_COMMAND_RANGE(ID_NYQ_SLIDER, ID_NYQ_SLIDER+99,
                     wxEVT_COMMAND_SLIDER_UPDATED, NyquistDialog::OnSlider)
   EVT_COMMAND_RANGE(ID_NYQ_TEXT, ID_NYQ_TEXT+99,
                      wxEVT_COMMAND_TEXT_UPDATED, NyquistDialog::OnText)
END_EVENT_TABLE()

NyquistDialog::NyquistDialog(wxWindow * parent, wxWindowID id,
                             const wxString & title,
                             wxString info,
                             NyqControlArray *controlArray)
   :wxDialog(parent, id, title)
{
   mControls = controlArray;
   mInHandler = false;

   wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer *hSizer;
   wxButton   *button;

   if (info.Length() > 0) {
      wxControl  *item;
      item = new wxStaticText(this, -1, info);
      mainSizer->Add(item, 0, wxALIGN_LEFT | wxALL, 5);
   }

   wxFlexGridSizer *grid = new wxFlexGridSizer(4, 0, 0);

   for(unsigned int i=0; i<mControls->GetCount(); i++) {
      wxControl  *item;
      NyqControl *ctrl = &((*mControls)[i]);
      int val = (int)(0.5 + ctrl->ticks * (ctrl->val - ctrl->low)/
                      (ctrl->high - ctrl->low));

      item = new wxStaticText(this, -1, ctrl->name);
      grid->Add(item, 0, wxALIGN_RIGHT | wxALL, 5);

      item = new wxTextCtrl(this, ID_NYQ_TEXT+i, "",
                            wxDefaultPosition, wxSize(60, -1));
      grid->Add(item, 0, wxALIGN_CENTRE | wxALL, 5);

      item = new wxSlider(this, ID_NYQ_SLIDER+i, val, 0, ctrl->ticks,
                          wxDefaultPosition, wxSize(150, -1));
      grid->Add(item, 0, wxALIGN_CENTRE | wxALL, 5);

      item = new wxStaticText(this, -1, ctrl->label);
      grid->Add(item, 0, wxALIGN_LEFT | wxALL, 5);
   }
   mainSizer->Add(grid, 0, wxALIGN_CENTRE | wxALL, 5);

   hSizer = new wxBoxSizer(wxHORIZONTAL);

   button = new wxButton(this, wxID_CANCEL, _("Cancel"));
   hSizer->Add(button, 0, wxALIGN_CENTRE | wxALL, 5);

   button = new wxButton(this, wxID_OK, _("OK"));
   button->SetDefault();
   button->SetFocus();
   hSizer->Add(button, 0, wxALIGN_CENTRE | wxALL, 5);

   mainSizer->Add(hSizer, 0, wxALIGN_CENTRE | wxALL, 5);

   wxCommandEvent dummy;
   OnSlider(dummy);

   SetAutoLayout(true);
   SetSizer(mainSizer);
   mainSizer->Fit(this);
   mainSizer->SetSizeHints(this);
}

void NyquistDialog::OnSlider(wxCommandEvent & /* event */)
{
   if (mInHandler)
      return; // prevent recursing forever
   mInHandler = true;

   for(unsigned int i=0; i<mControls->GetCount(); i++) {
      NyqControl *ctrl = &((*mControls)[i]);
      wxSlider *slider = (wxSlider *)FindWindow(ID_NYQ_SLIDER + i);
      wxTextCtrl *text = (wxTextCtrl *)FindWindow(ID_NYQ_TEXT + i);
      wxASSERT(slider && text);
      
      int val = slider->GetValue();
      ctrl->val = (val / (double)ctrl->ticks)*
         (ctrl->high - ctrl->low) + ctrl->low;
      
      wxString valStr;
      if (ctrl->type == 1) {
         if (ctrl->high - ctrl->low < 1)
            valStr.Printf("%.3f", ctrl->val);
         else if (ctrl->high - ctrl->low < 10)
            valStr.Printf("%.2f", ctrl->val);
         else if (ctrl->high - ctrl->low < 100)
            valStr.Printf("%.1f", ctrl->val);
         else
            valStr.Printf("%d", (int)floor(ctrl->val + 0.5));
      }
      else
         valStr.Printf("%d", (int)floor(ctrl->val + 0.5));
      
      text->SetValue(valStr);
   }

   mInHandler = false;
}

void NyquistDialog::OnText(wxCommandEvent & /* event */)
{
   if (mInHandler)
      return; // prevent recursing forever
   mInHandler = true;

   for(unsigned int i=0; i<mControls->GetCount(); i++) {
      NyqControl *ctrl = &((*mControls)[i]);
      wxSlider *slider = (wxSlider *)FindWindow(ID_NYQ_SLIDER + i);
      wxTextCtrl *text = (wxTextCtrl *)FindWindow(ID_NYQ_TEXT + i);
      wxASSERT(slider && text);

      wxString valStr = text->GetValue();
      valStr.ToDouble(&ctrl->val);
      int pos = (int)floor((ctrl->val - ctrl->low) /
                           (ctrl->high - ctrl->low) * ctrl->ticks + 0.5);
      if (pos < 0)
         pos = 0;
      if (pos > ctrl->ticks)
         pos = ctrl->ticks;
      slider->SetValue(pos);
   }   

   mInHandler = false;   
}

void NyquistDialog::OnOk(wxCommandEvent & /* event */)
{
   // Transfer data

   EndModal(true);
}

void NyquistDialog::OnCancel(wxCommandEvent & /* event */)
{
   EndModal(false);
}

