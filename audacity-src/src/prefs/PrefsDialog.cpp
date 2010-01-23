/**********************************************************************

  Audacity: A Digital Audio Editor

  PrefsDialog.cpp

  Joshua Haberman

**********************************************************************/

#include <wx/defs.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/intl.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/sizer.h>

#include "../Audacity.h"
#include "../Project.h"

#include "../Prefs.h"

#include "PrefsDialog.h"
#include "PrefsPanel.h"

#include "AudioIOPrefs.h"
#include "DirectoriesPrefs.h"
#include "FileFormatPrefs.h"
#include "GUIPrefs.h"
#include "BatchPrefs.h"
#include "KeyConfigPrefs.h"
#include "QualityPrefs.h"
#include "SpectrumPrefs.h"
#include "MousePrefs.h"


enum {
   CategoriesID = 1000
};

BEGIN_EVENT_TABLE(PrefsDialog, wxDialog)
   EVT_BUTTON(wxID_OK, PrefsDialog::OnOK)
   EVT_BUTTON(wxID_CANCEL, PrefsDialog::OnCancel)
END_EVENT_TABLE()

bool gPrefsDialogVisible = false;

PrefsDialog::PrefsDialog(wxWindow * parent):
   wxDialog(parent, -1, _("Audacity Preferences"), wxDefaultPosition,
         wxDefaultSize, wxDIALOG_MODAL | wxCAPTION | wxTHICK_FRAME)
{
#ifdef __WXMAC__
   mMacHiddenFrame = new wxFrame(NULL, -1, wxT(""), wxPoint(5000, 5000),
                        wxSize(100, 100));
   wxMenuBar *blankMenuBar = new wxMenuBar();
   mMacHiddenFrame->SetMenuBar(blankMenuBar);
   blankMenuBar->MacInstallMenuBar();
   mMacHiddenFrame->Show();
#endif

   gPrefsDialogVisible = true;

   wxRect rect = GetRect();
   if(rect.x < 0) rect.x = 0;
   if(rect.y < 0) rect.y = 0;
   SetSize(rect);

   wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

   mCategories = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize
#ifdef __WXGTK__
                                ,wxNB_LEFT
#endif
                                );
   topSizer->Add(mCategories, 1, wxGROW | wxALL, 0);

   /* All panel additions belong here */
   mCategories->AddPage(new AudioIOPrefs(mCategories), _("Audio I/O"));
   mCategories->AddPage(new QualityPrefs(mCategories), _("Quality"));
   mCategories->AddPage(new FileFormatPrefs(mCategories), _("File Formats"));
   mCategories->AddPage(new SpectrumPrefs(mCategories), _("Spectrograms"));
   mCategories->AddPage(new DirectoriesPrefs(mCategories), _("Directories"));
   mCategories->AddPage(new GUIPrefs(mCategories), _("Interface"));
   mCategories->AddPage(new BatchPrefs(mCategories), _("Batch"));
   mCategories->AddPage(new KeyConfigPrefs(mCategories), _("Keyboard"));
   mCategories->AddPage(new MousePrefs(mCategories), _("Mouse"));

   long selected = gPrefs->Read(wxT("/Prefs/PrefsCategory"), 0L);
   if (selected < 0 || selected >= mCategories->GetPageCount())
      mSelected = 0;

   mCategories->SetSelection(selected);

   mOK = new wxButton(this,
                      wxID_OK, _("OK"));

#ifndef TARGET_CARBON
   mOK->SetDefault();
   mOK->SetFocus();
#endif

   mCancel = new wxButton(this,
                          wxID_CANCEL,
                          _("Cancel"));

   wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);

   buttonSizer->Add(mCancel, 0, wxALL, 7);
   buttonSizer->Add(mOK, 0, wxALL, 7);
   
   topSizer->Add(buttonSizer, 0, wxALIGN_RIGHT);

   wxBoxSizer *outSizer = new wxBoxSizer( wxVERTICAL );
   outSizer->Add(topSizer, 0, wxGROW|wxALL, TOP_LEVEL_BORDER);

   SetAutoLayout(true);
   SetSizer(outSizer);
   outSizer->Fit(this);
   outSizer->SetSizeHints(this);

   #ifdef __MACOS9__
   // Until sizing works properly on the Mac
   SetSize(525, 350);
   #endif

   #ifdef __MACOSX__
   // Until sizing works properly on the Mac
   SetSize(620, 350);
   #endif

   #ifdef __WXMSW__
   // Because it looks nice (tm)   (you can see all the tabs at once)
   SetSize(525, 363);
   #endif

   // Center after all that resizing, but make sure it doesn't end up
   // off-screen
   CentreOnParent();

   #ifdef __WXMAC__
   wxPoint where = GetPosition();
   if (where.x < 2)
      where.x = 2;
   if (where.y < 44)
      where.y = 44;
   if (where != GetPosition())
      Move(where);
   #endif
}

void PrefsDialog::OnCancel(wxCommandEvent & event)
{
   EndModal(0);
}

void PrefsDialog::OnOK(wxCommandEvent & event)
{
   int i;
   unsigned int j;

   gPrefs->Write(wxT("/Prefs/PrefsCategory"), (long)mCategories->GetSelection());
   for (i = 0; i < mCategories->GetPageCount(); i++) {
      PrefsPanel *panel = (PrefsPanel *) mCategories->GetPage(i);

      /* The dialog doesn't end until all the input is valid */
      if (!panel->Apply()) {
         mCategories->SetSelection(i);
         mSelected = i;
         return;
      }
   }

   // BG: Send all Audacity projects a preference update notification
   for(j = 0; j < gAudacityProjects.GetCount(); j++)
   {
      gAudacityProjects[j]->UpdatePrefs();
   }
   EndModal(0);
}

PrefsDialog::~PrefsDialog()
{
#ifdef __WXMAC__
   mMacHiddenFrame->Destroy();
#endif
   gPrefsDialogVisible = false;
}

void PrefsDialog::SelectPageByName(wxString pageName)
{
   int n = mCategories->GetPageCount();

   int i;

   for(i=0; i<n; i++)
      if (mCategories->GetPageText(i) == pageName) {
         mCategories->SetSelection(i);
         return;
      }
}


void PrefsDialog::ShowTempDirPage()
{
   SelectPageByName(_("Directories"));   
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
// arch-tag: b305b538-1d2e-4acf-a997-95023d10a7bd

