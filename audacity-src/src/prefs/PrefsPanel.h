/**********************************************************************

  Audacity: A Digital Audio Editor

  PrefsPanel.h

  Joshua Haberman

  The interface works like this: Each panel in the preferences dialog
  must derive from PrefsPanel. You must override Apply() with code
  to validate fields (returning false if any are bad), updating the
  global preferences object gPrefs, and instructing the applicable parts
  of the program to re-read the preference options.

  To actually add a the new panel, edit the PrefsDialog constructor
  to append the panel to its list of panels.

**********************************************************************/

#ifndef __AUDACITY_PREFS_PANEL__
#define __AUDACITY_PREFS_PANEL__

#include <wx/panel.h>

class wxWindow;
class wxStaticBoxSizer;
class wxBoxSizer;

/* A few constants for an attempt at semi-uniformity */
#define PREFS_FONT_SIZE     8

/* these are spacing guidelines: ie. radio buttons should have a 5 pixel
 * border on each side */
#define RADIO_BUTTON_BORDER    5
#define TOP_LEVEL_BORDER       5
#define GENERIC_CONTROL_BORDER 5

class PrefsPanel:public wxPanel {

 public:
   PrefsPanel(wxWindow * parent):wxPanel(parent, -1) {
      /* I'm not sure if we should be setting this... I'll play around
       * and see what looks best on different platforms under
       * differing circumstances...*/
//    SetFont(wxFont(PREFS_FONT_SIZE, wxDEFAULT, wxNORMAL, wxNORMAL));
   }
   
   virtual ~ PrefsPanel() {}
   virtual bool Apply() = 0;

 protected:
   wxBoxSizer *topSizer;
   wxBoxSizer *outSizer;

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
// arch-tag: 0ed163a8-b4fa-4992-bd7e-89657c56eeb8

