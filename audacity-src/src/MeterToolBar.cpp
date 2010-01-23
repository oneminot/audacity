/**********************************************************************

  Audacity: A Digital Audio Editor

  MeterToolBar.cpp

  Dominic Mazzoni
 
  See MeterToolBar.h for details

**********************************************************************/

#include <wx/defs.h>
#include <wx/intl.h>
#include <wx/dc.h>
#include <wx/dcclient.h>
#include <wx/event.h>
#include <wx/dcclient.h> // wxPaintDC
#include "MeterToolBar.h"
#include "Audacity.h"
#include "widgets/Meter.h"

////////////////////////////////////////////////////////////
/// Methods for MeterToolBar
////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(MeterToolBar, wxWindow)
   EVT_SIZE(MeterToolBar::OnSize)
   EVT_PAINT(MeterToolBar::OnPaint)
   EVT_CHAR(MeterToolBar::OnKeyEvent)
END_EVENT_TABLE()

//Standard contructor
MeterToolBar::MeterToolBar(wxWindow * parent)
   : ToolBar(parent, -1, wxPoint(1, 1), wxSize(200, 55),gMeterToolBarStub)
{
   InitializeMeterToolBar();
}

//Another constructor
MeterToolBar::MeterToolBar(wxWindow * parent, wxWindowID id,
                           const wxPoint & pos, const wxSize & size):
  ToolBar(parent, id, pos, size,gMeterToolBarStub)
{
   InitializeMeterToolBar();
}

// This sets up the MeterToolBar, initializing all the important values
// and creating the buttons.
void MeterToolBar::InitializeMeterToolBar()
{
   mIdealSize = wxSize(200, 55);
   mTitle = _("Audacity Meter Toolbar");
   SetLabel(_("Meter"));
   mType = MeterToolBarID;

   mPlayMeter = new Meter(this, -1, false,
                          wxPoint(0, 0),
                          wxSize(99, 55));
   mRecordMeter = new Meter(this, -1, true,
                            wxPoint(100, 0),
                            wxSize(99, 55));

   mPlayMeter->SetLabel( wxT("Meter-Play"));
   mRecordMeter->SetLabel( wxT("Meter-Record"));
   #if wxUSE_TOOLTIPS
   mPlayMeter->SetToolTip(_("Output level meter"));
   mRecordMeter->SetToolTip(_("Input level meter - click to monitor input"));
   #endif
}

MeterToolBar::~MeterToolBar()
{
}

void MeterToolBar::OnSize(wxSizeEvent & evt)
{
   int width, height;
   GetClientSize(&width, &height);

   if (width > height && height > 120) {
      // Two stacked horizontal meters
      mPlayMeter->SetSize(0, 0, width-2, height/2 - 1);
      mRecordMeter->SetSize(0, height/2, width-2, height/2 - 1);
      mPlayMeter->SetStyle(Meter::HorizontalStereo);
      mRecordMeter->SetStyle(Meter::HorizontalStereo);
   }
   else if (width > height) {
      // Two horizontal, side-by-side
      mPlayMeter->SetSize(0, 0, width/2 - 3, height);
//lda      mRecordMeter->SetSize(width/2-2, 0, width/2 - 3, height);
      mRecordMeter->SetSize(width/2 - 6, 0, width/2 - 6, height);
      mPlayMeter->SetStyle(Meter::HorizontalStereo);
      mRecordMeter->SetStyle(Meter::HorizontalStereo);
   }
   else {
      // Two vertical, side-by-side
      mPlayMeter->SetSize(0, 0, width/2 - 2, height);
      mRecordMeter->SetSize(width/2 - 1, 0, width/2 - 2, height);
      mPlayMeter->SetStyle(Meter::VerticalStereo);
      mRecordMeter->SetStyle(Meter::VerticalStereo);
   }
}

void MeterToolBar::OnPaint(wxPaintEvent & evt)
{
   wxPaintDC dc(this);

   int width, height;
   GetSize(&width, &height);

   DrawBackground(dc, width, height);
}

void MeterToolBar::EnableDisableButtons()
{
}

void MeterToolBar::OnKeyEvent(wxKeyEvent & event)
{
   event.Skip();
}

void MeterToolBar::PlaceButton(int i, wxWindow *pWind)
{
   wxSize Size;
   if( i==0 )
   {
      mxButtonPos = 0;
   }
   Size = pWind->GetSize();
   pWind->SetSize( mxButtonPos, 0, Size.GetX(), Size.GetY());
   mxButtonPos+=Size.GetX()+1;
//   mIdealSize = wxSize(mxButtonPos+3, 27);
//   SetSize(mIdealSize );
}


