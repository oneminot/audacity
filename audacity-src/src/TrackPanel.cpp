/**********************************************************************

  Audacity: A Digital Audio Editor

  TrackPanel.cpp

  Dominic Mazzoni
  and lots of other contributors

  AS: The TrackPanel class is responsible for rendering the panel
      displayed to the left of a track.  TrackPanel also takes care
      of the functionality for each of the buttons in that panel.

  This is currently some of the worst code in Audacity.  It's
  not really unreadable, there's just way too much stuff in this
  one file.  Rather than apply a quick fix, the long-term plan
  is to create a GUITrack class that knows how to draw itself
  and handle events.  Then this class just helps coordinate
  between tracks.

**********************************************************************/

/*********************************************************************
 
  JKC: Incremental refactoring started April/2003

  Possibly aiming for Gui classes something like this - it's under
  discussion:

   +----------------------------------------------------+
   |      AdornedRulerPanel                             |
   +----------------------------------------------------+
   +----------------------------------------------------+
   |+------------+ +-----------------------------------+|
   ||            | | (L)  GuiWaveTrack                 ||
   || TrackLabel | +-----------------------------------+|
   ||            | +-----------------------------------+|
   ||            | | (R)  GuiWaveTrack                 ||
   |+------------+ +-----------------------------------+|
   +-------- GuiStereoTrack ----------------------------+
   +----------------------------------------------------+
   |+------------+ +-----------------------------------+|
   ||            | | (L)  GuiWaveTrack                 ||
   || TrackLabel | +-----------------------------------+|
   ||            | +-----------------------------------+|
   ||            | | (R)  GuiWaveTrack                 ||
   |+------------+ +-----------------------------------+|
   +-------- GuiStereoTrack ----------------------------+
    
  With the whole lot sitting in a TrackPanel which forwards 
  events to the sub objects.

  The GuiStereoTrack class will do the special logic for
  Stereo channel grouping.  
  
  The precise names of the classes are subject to revision.
  Have deliberately not created new files for the new classes 
  such as AdornedRulerPanel and TrackLabel - yet.

  TODO: (TrackPanel Refactor)
    - Move menus from current TrackPanel into TrackLabel.
    - Convert TrackLabel from 'flyweight' to heavyweight.
    - Split GuiStereoTrack and GuiWaveTrack out from TrackPanel.

*********************************************************************/
 


#include "Audacity.h"
#include "TrackPanel.h"

#ifdef __MACOSX__
#include <Carbon/Carbon.h>
#endif

#include <math.h>

#if DEBUG_DRAW_TIMING
#include <sys/time.h>
#endif

#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/font.h>
#include <wx/fontenum.h>
#include <wx/log.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <wx/numdlg.h>
#include <wx/choicdlg.h>
#include <wx/textctrl.h>
#include <wx/intl.h>
#include <wx/image.h>

#include "AColor.h"
#include "AudioIO.h"
#include "ControlToolBar.h"
#include "Envelope.h"
#include "LabelTrack.h"
#include "NoteTrack.h"
#include "Track.h"
#include "TrackArtist.h"
#include "Prefs.h"
#include "Project.h"
#include "ViewInfo.h"
#include "WaveTrack.h"
#include "TimeTrack.h"
#include "Experimental.h"

#include "widgets/ASlider.h"
#include "widgets/Ruler.h"

#include <wx/arrimpl.cpp>

WX_DEFINE_OBJARRAY(TrackClipArray);

//This loads the appropriate set of cursors, depending on platform.
#include "../images/Cursors.h"
#include <iostream>



//FIX-ME: The WXMAC code below is obsolete
//#if defined(__WXMAC__) && !defined(__UNIX__)
//#include <Menus.h>
//#endif

#define kLeftInset 4
#define kTopInset 4

// Is the distance between A and B less than D?
template < class A, class B, class DIST > bool within(A a, B b, DIST d)
{
   return (a > b - d) && (a < b + d);
}

template < class LOW, class MID, class HIGH >
    bool between_inclusive(LOW l, MID m, HIGH h)
{
   return (m >= l && m <= h);
}

template < class LOW, class MID, class HIGH >
    bool between_exclusive(LOW l, MID m, HIGH h)
{
   return (m > l && m < h);
}

template < class CLIPPEE, class CLIPVAL >
    void clip_top(CLIPPEE & clippee, CLIPVAL val)
{
   if (clippee > val)
      clippee = val;
}

template < class CLIPPEE, class CLIPVAL >
    void clip_bottom(CLIPPEE & clippee, CLIPVAL val)
{
   if (clippee < val)
      clippee = val;
}

// Probably should move to 'Utils.cpp'.
void SetLabelFont(wxDC * dc)
{
   int fontSize = 10;
#if defined __WXMSW__ || __WXGTK__
   fontSize = 8;
#endif

   wxFont labelFont(fontSize, wxSWISS, wxNORMAL, wxNORMAL);
   dc->SetFont(labelFont);
}

enum {
   TrackPanelFirstID = 2000,
   OnSetNameID,
   OnSetFontID,

   OnMoveUpID,
   OnMoveDownID,

   OnUpOctaveID,
   OnDownOctaveID,

   OnChannelLeftID,
   OnChannelRightID,
   OnChannelMonoID,

   OnRate8ID,
   OnRate11ID,
   OnRate16ID,
   OnRate22ID,
   OnRate44ID,
   OnRate48ID,
   OnRate96ID,
   OnRateOtherID,

   On16BitID,
   On24BitID,
   OnFloatID,

   OnWaveformID,
   OnWaveformDBID,
   OnSpectrumID,
   OnPitchID,

   OnSplitStereoID,
   OnMergeStereoID,

   OnSetTimeTrackRangeID,
   OnCutSelectedTextID,
   OnCopySelectedTextID,
   OnPasteSelectedTextID,
};

BEGIN_EVENT_TABLE(TrackPanel, wxWindow)
    EVT_MOUSE_EVENTS(TrackPanel::OnMouseEvent)
    EVT_CHAR(TrackPanel::OnKeyEvent)
    EVT_PAINT(TrackPanel::OnPaint)
    EVT_CONTEXT_MENU(TrackPanel::OnContextMenu)
    EVT_MENU(OnSetNameID, TrackPanel::OnSetName)
    EVT_MENU(OnSetFontID, TrackPanel::OnSetFont)
    EVT_MENU(OnSetTimeTrackRangeID, TrackPanel::OnSetTimeTrackRange)

    EVT_MENU_RANGE(OnMoveUpID, OnMoveDownID, TrackPanel::OnMoveTrack)
    EVT_MENU_RANGE(OnUpOctaveID, OnDownOctaveID, TrackPanel::OnChangeOctave)
    EVT_MENU_RANGE(OnChannelLeftID, OnChannelMonoID,
               TrackPanel::OnChannelChange)
    EVT_MENU_RANGE(OnWaveformID, OnPitchID, TrackPanel::OnSetDisplay)
    EVT_MENU_RANGE(OnRate8ID, OnRate96ID, TrackPanel::OnRateChange)
    EVT_MENU_RANGE(On16BitID, OnFloatID, TrackPanel::OnFormatChange)
    EVT_MENU(OnRateOtherID, TrackPanel::OnRateOther)
    EVT_MENU(OnSplitStereoID, TrackPanel::OnSplitStereo)
    EVT_MENU(OnMergeStereoID, TrackPanel::OnMergeStereo)

    EVT_MENU(OnCutSelectedTextID, TrackPanel::OnCutSelectedText)
    EVT_MENU(OnCopySelectedTextID, TrackPanel::OnCopySelectedText)
    EVT_MENU(OnPasteSelectedTextID, TrackPanel::OnPasteSelectedText)
END_EVENT_TABLE()


// Use CursorId as a fallback.
wxCursor * MakeCursor( int CursorId, const char * pXpm[36],  int HotX, int HotY )
{
   wxCursor * pCursor;

#ifdef CURSORS_SIZE32
   const int HotAdjust =0;
#else
   const int HotAdjust =8;
#endif

   wxImage Image = wxImage(wxBitmap(pXpm).ConvertToImage());   
   Image.SetMaskColour(255,0,0);
   Image.SetMask();// Enable mask.

#ifdef __WXGTK__
   //
   // Kludge: the wxCursor Image constructor is broken in wxGTK.
   // This code, based loosely on the broken code from the wxGTK source,
   // works around the problem by constructing a 1-bit bitmap and
   // calling the other custom cursor constructor.
   //
   // -DMM
   //

   unsigned char *rgbBits = Image.GetData();
   int w = Image.GetWidth() ;
   int h = Image.GetHeight();
   int imagebitcount = (w*h)/8;
   
   unsigned char *bits = new unsigned char [imagebitcount];
   unsigned char *maskBits = new unsigned char [imagebitcount];

   int i, j, i8;
   unsigned char cMask;
   for (i=0; i<imagebitcount; i++) {
      bits[i] = 0;
      i8 = i * 8;
        
      cMask = 1;
      for (j=0; j<8; j++) {
         if (rgbBits[(i8+j)*3+2] < 127)
            bits[i] = bits[i] | cMask;
         cMask = cMask * 2;
      }
   }

   for (i=0; i<imagebitcount; i++) {
      maskBits[i] = 0x0;
      i8 = i * 8;
      
      cMask = 1;
      for (j=0; j<8; j++) {
         if (rgbBits[(i8+j)*3] < 127 || rgbBits[(i8+j)*3+1] > 127)
            maskBits[i] = maskBits[i] | cMask;
         cMask = cMask * 2;
      }
   }

   pCursor = new wxCursor((const char *)bits, w, h,
                          HotX-HotAdjust, HotY-HotAdjust,
                          (const char *)maskBits,
                          new wxColour(0, 0, 0),
                          new wxColour(255, 255, 255));

#else
   Image.SetOption( wxIMAGE_OPTION_CUR_HOTSPOT_X, HotX-HotAdjust );
   Image.SetOption( wxIMAGE_OPTION_CUR_HOTSPOT_Y, HotY-HotAdjust );
   pCursor = new wxCursor( Image );
#endif

   return pCursor;
}



// Don't warn us about using 'this' in the base member initializer list.
#ifndef __WXGTK__ //Get rid if this pragma for gtk
#pragma warning( disable: 4355 )
#endif
TrackPanel::TrackPanel(wxWindow * parent, wxWindowID id,
                       const wxPoint & pos,
                       const wxSize & size,
                       TrackList * tracks,
                       ViewInfo * viewInfo,
                       TrackPanelListener * listener)
   : wxPanel(parent, id, pos, size, wxWANTS_CHARS),
     mTrackLabel(this),
     mListener(listener),
     mTracks(tracks),
     mViewInfo(viewInfo),
     mBitmap(NULL),
     mAutoScrolling(false)
#ifndef __WXGTK__   //Get rid if this pragma for gtk
#pragma warning( default: 4355 )
#endif
{
   mMouseCapture = IsUncaptured;
   mSlideUpDownOnly = false;
   mLabelTrackStartXPos=-1;

   gPrefs->Read(wxT("/GUI/AdjustSelectionEdges"), &mAdjustSelectionEdges, true);

   mRedrawAfterStop = false;
   mIndicatorShowing = false;

   mPencilCursor  = MakeCursor( wxCURSOR_PENCIL,    DrawCursorXpm,    12, 22);
   mSelectCursor  = MakeCursor( wxCURSOR_IBEAM,     IBeamCursorXpm,   17, 16);
   mEnvelopeCursor= MakeCursor( wxCURSOR_ARROW,     EnvCursorXpm,     16, 16);
   mDisabledCursor= MakeCursor( wxCURSOR_NO_ENTRY,  DisabledCursorXpm,16, 16);
   mSlideCursor   = MakeCursor( wxCURSOR_SIZEWE,    TimeCursorXpm,    16, 16);
   mZoomInCursor  = MakeCursor( wxCURSOR_MAGNIFIER, ZoomInCursorXpm,  19, 15);
   mZoomOutCursor = MakeCursor( wxCURSOR_MAGNIFIER, ZoomOutCursorXpm, 19, 15);
   mLabelCursorLeft  = MakeCursor( wxCURSOR_ARROW,  LabelCursorLeftXpm, 19, 15);
   mLabelCursorRight = MakeCursor( wxCURSOR_ARROW,  LabelCursorRightXpm, 16, 16);

   mArrowCursor = new wxCursor(wxCURSOR_ARROW);
   mSmoothCursor = new wxCursor(wxCURSOR_SPRAYCAN);
   mResizeCursor = new wxCursor(wxCURSOR_SIZENS);
   mRearrangeCursor = new wxCursor(wxCURSOR_HAND);
   mAdjustLeftSelectionCursor = new wxCursor(wxCURSOR_POINT_LEFT);
   mAdjustRightSelectionCursor = new wxCursor(wxCURSOR_POINT_RIGHT);

   // Use AppendCheckItem so we can have ticks beside the items.
   // We would use AppendRadioItem but it only currently works on windows and GTK.
   mRateMenu = new wxMenu();
   mRateMenu->AppendCheckItem(OnRate8ID, wxT("8000 Hz"));
   mRateMenu->AppendCheckItem(OnRate11ID, wxT("11025 Hz"));
   mRateMenu->AppendCheckItem(OnRate16ID, wxT("16000 Hz"));
   mRateMenu->AppendCheckItem(OnRate22ID, wxT("22050 Hz"));
   mRateMenu->AppendCheckItem(OnRate44ID, wxT("44100 Hz"));
   mRateMenu->AppendCheckItem(OnRate48ID, wxT("48000 Hz"));
   mRateMenu->AppendCheckItem(OnRate96ID, wxT("96000 Hz"));
   mRateMenu->AppendCheckItem(OnRateOtherID, _("Other..."));

   mFormatMenu = new wxMenu();
   mFormatMenu->AppendCheckItem(On16BitID, GetSampleFormatStr(int16Sample));
   mFormatMenu->AppendCheckItem(On24BitID, GetSampleFormatStr(int24Sample));
   mFormatMenu->AppendCheckItem(OnFloatID, GetSampleFormatStr(floatSample));

   mWaveTrackMenu = new wxMenu();
   mWaveTrackMenu->Append(OnSetNameID, _("Name..."));
   mWaveTrackMenu->AppendSeparator();
   mWaveTrackMenu->Append(OnMoveUpID, _("Move Track Up"));
   mWaveTrackMenu->Append(OnMoveDownID, _("Move Track Down"));
   mWaveTrackMenu->AppendSeparator();
   mWaveTrackMenu->Append(OnWaveformID, _("Waveform"));
   mWaveTrackMenu->Append(OnWaveformDBID, _("Waveform (dB)"));
   mWaveTrackMenu->Append(OnSpectrumID, _("Spectrum"));
   mWaveTrackMenu->Append(OnPitchID, _("Pitch (EAC)"));
   mWaveTrackMenu->AppendSeparator();
   mWaveTrackMenu->Append(OnChannelMonoID, _("Mono"));
   mWaveTrackMenu->Append(OnChannelLeftID, _("Left Channel"));
   mWaveTrackMenu->Append(OnChannelRightID, _("Right Channel"));
   mWaveTrackMenu->Append(OnMergeStereoID, _("Make Stereo Track"));
   mWaveTrackMenu->Append(OnSplitStereoID, _("Split Stereo Track"));
   mWaveTrackMenu->AppendSeparator();
   mWaveTrackMenu->Append(0, _("Set Sample Format"), mFormatMenu);
   mWaveTrackMenu->AppendSeparator();
   mWaveTrackMenu->Append(0, _("Set Rate"), mRateMenu);

   mNoteTrackMenu = new wxMenu();
   mNoteTrackMenu->Append(OnSetNameID, _("Name..."));
   mNoteTrackMenu->AppendSeparator();
   mNoteTrackMenu->Append(OnMoveUpID, _("Move Track Up"));
   mNoteTrackMenu->Append(OnMoveDownID, _("Move Track Down"));
   mNoteTrackMenu->AppendSeparator();
   mNoteTrackMenu->Append(OnUpOctaveID, _("Up Octave"));
   mNoteTrackMenu->Append(OnDownOctaveID, _("Down Octave"));

   mLabelTrackMenu = new wxMenu();
   mLabelTrackMenu->Append(OnSetNameID, _("Name..."));
   mLabelTrackMenu->AppendSeparator();
   mLabelTrackMenu->Append(OnSetFontID, _("Font..."));
   mLabelTrackMenu->AppendSeparator();
   mLabelTrackMenu->Append(OnMoveUpID, _("Move Track Up"));
   mLabelTrackMenu->Append(OnMoveDownID, _("Move Track Down"));

   mTimeTrackMenu = new wxMenu();
   mTimeTrackMenu->Append(OnSetNameID, _("Name..."));
   mTimeTrackMenu->AppendSeparator();
   mTimeTrackMenu->Append(OnMoveUpID, _("Move Track Up"));
   mTimeTrackMenu->Append(OnMoveDownID, _("Move Track Down"));
   mTimeTrackMenu->AppendSeparator();
   mTimeTrackMenu->Append(OnSetTimeTrackRangeID, _("Set Range..."));

   mLabelTrackLabelMenu = new wxMenu();
   mLabelTrackLabelMenu->Append(OnCutSelectedTextID, _("Cut"));
   mLabelTrackLabelMenu->Append(OnCopySelectedTextID, _("Copy"));
   mLabelTrackLabelMenu->Append(OnPasteSelectedTextID, _("Paste"));

   mTrackArtist = new TrackArtist();
   mTrackArtist->SetInset(1, kTopInset + 1, kLeftInset + 2, 1);

   mCapturedTrack = NULL;
   mPopupMenuTarget = NULL;

   mRuler = new AdornedRulerPanel();

   mTimeCount = 0;
   mTimer.parent = this;
   mTimer.Start(50, FALSE);

   //Initialize the indicator playing state, so that
   //we know that no drawing line needs to be erased.
   mPlayIndicatorExists=false;

   //Initialize a member variable pointing to the current
   //drawing track.
   mDrawingTrack =NULL;

   //More initializations, some of these for no other reason than
   //to prevent runtime memory check warnings
   mZoomStart = -1;
   mZoomEnd = -1;
   mPrevWidth = -1;
   mPrevHeight = -1;
}

TrackPanel::~TrackPanel()
{
   mTimer.Stop();

   if (mBitmap)
      delete mBitmap;

   delete mTrackArtist;
   delete mRuler;

   delete mArrowCursor;
   delete mPencilCursor;
   delete mSelectCursor;
   delete mEnvelopeCursor;
   delete mDisabledCursor;
   delete mSlideCursor;
   delete mResizeCursor;
   delete mSmoothCursor;
   delete mZoomInCursor;
   delete mZoomOutCursor;
   delete mLabelCursorLeft;
   delete mLabelCursorRight;
   delete mRearrangeCursor;
   delete mAdjustLeftSelectionCursor;
   delete mAdjustRightSelectionCursor;

   // Note that the submenus (mRateMenu, ...)
   // are deleted by their parent
   delete mWaveTrackMenu;
   delete mNoteTrackMenu;
   delete mLabelTrackMenu;
   delete mLabelTrackLabelMenu;
   delete mTimeTrackMenu;

   while(!mScreenAtIndicator.IsEmpty())
   {
      delete mScreenAtIndicator[0]->bitmap;
      tpBitmap *tmpDeletedTpBitmap = mScreenAtIndicator[0];
      mScreenAtIndicator.RemoveAt(0);
      delete tmpDeletedTpBitmap;
   }
   mScreenAtIndicator.Clear();

   while(!mPreviousCursorData.IsEmpty())
   {
      delete mPreviousCursorData[0]->bitmap;
      tpBitmap *tmpDeletedTpBitmap = mPreviousCursorData[0];
      mPreviousCursorData.RemoveAt(0);
      delete tmpDeletedTpBitmap;
   }
   mPreviousCursorData.Clear();
}

void TrackPanel::UpdatePrefs()
{
   gPrefs->Read(wxT("/GUI/AutoScroll"), &mViewInfo->bUpdateTrackIndicator,
                true);
   gPrefs->Read(wxT("/GUI/UpdateSpectrogram"), &mViewInfo->bUpdateSpectrogram,
                true);
   gPrefs->Read(wxT("/GUI/AdjustSelectionEdges"), &mAdjustSelectionEdges, true);
}

void TrackPanel::SetStop(bool bStopped)
{
   mViewInfo->bIsPlaying = !bStopped;
   Refresh(false);
}

/// Remember the track we clicked on and why we captured it.
/// We also use this function to clear the record 
/// of the captured track, passing NULL as the track.
void TrackPanel::SetCapturedTrack( Track * t, enum MouseCaptureEnum MouseCapture )
{
   if( t== NULL ) 
   {
      wxASSERT( MouseCapture == IsUncaptured );
   }
   else
   {
      wxASSERT( MouseCapture != IsUncaptured );
   }
   mCapturedTrack = t;
   mMouseCapture = MouseCapture;
}

void TrackPanel::SelectNone()
{
   TrackListIterator iter(mTracks);
   Track *t = iter.First();
   while (t) {
      t->SetSelected(false);
      if (t->GetKind() == Track::Label)
         ((LabelTrack *) t)->Unselect();
      t = iter.Next();
   }
}

/// Set all tracks to 'selected'
void TrackPanel::SelectAllTracks()
{
   TrackListIterator iter(mTracks);
   Track *t = iter.First();
   while (t) {
      t->SetSelected(true);
      t = iter.Next();
   }
}

void TrackPanel::GetTracksUsableArea(int *width, int *height) const
{
   GetSize(width, height);
   *width -= GetLabelWidth();
   // AS: MAGIC NUMBER: What does 2 represent?
   *width -= 2 + kLeftInset;
}

/// Gets the pointer to the AudacityProject that
/// goes with this track panel.
AudacityProject * TrackPanel::GetProject() const
{
   //JKC casting away constness here.
   //Do it in two stages in case 'this' is not a wxWindow.
   //when the compiler will flag the error.
   wxWindow const * const pConstWind = this;
   wxWindow * pWind=(wxWindow*)pConstWind;
#ifdef EXPERIMENTAL_NOTEBOOK
   pWind = pWind->GetParent(); //Page
   wxASSERT( pWind );
   pWind = pWind->GetParent(); //Notebook
   wxASSERT( pWind );
#endif
   pWind = pWind->GetParent(); //MainPanel
   wxASSERT( pWind );
   pWind = pWind->GetParent(); //Project
   wxASSERT( pWind );
   return (AudacityProject*)pWind;
}

/// Replaces cursor with whatever was on screen before the 
/// cursors were drawn.
/// TODO: Merge with RemoveStaleIndicators().
void TrackPanel::RemoveStaleCursors(wxRegionIterator * upd)
{
   while((*upd).HaveRects())
   {
      wxRect rect;
      rect = (*upd).GetRect(); //convert the region to a rectangle

      //Does the rectangle intersect with the cursor?
      //If it does, remove it
      size_t i = 0;
      while(i < mPreviousCursorData.GetCount())
      {
         wxRect cursorRect(mPreviousCursorData[i]->x, mPreviousCursorData[i]->y, mPreviousCursorData[i]->bitmap->GetWidth(), mPreviousCursorData[i]->bitmap->GetHeight());

         if(rect.Intersects(cursorRect))
         {
            //delete this cursor
            delete mPreviousCursorData[i]->bitmap;
            tpBitmap *tmpDeletedTpBitmap = mPreviousCursorData[i];
            mPreviousCursorData.RemoveAt(i);
            delete tmpDeletedTpBitmap;
         }
         else
         {
            i++; //skip over this cursor
         }
      }
      (*upd)++; //move to next update region
   }
   upd->Reset(); //reset the iterator to point to the first update region
}

/// Replaces indicators with whatever was on screen before the 
/// indiocators were drawn.
void TrackPanel::RemoveStaleIndicators(wxRegionIterator * upd)
{
   while((*upd).HaveRects())
   {
      wxRect rect;
      rect = (*upd).GetRect(); //convert the region to a rectangle

      //Does the rectangle intersect with the indicator?
      //If it does, remove it
      size_t i = 0;
      while(i < mScreenAtIndicator.GetCount())
      {
         wxRect indicatorRect(mScreenAtIndicator[i]->x, mScreenAtIndicator[i]->y, mScreenAtIndicator[i]->bitmap->GetWidth(), mScreenAtIndicator[i]->bitmap->GetHeight());

         if(rect.Intersects(indicatorRect))
         {
            //delete this indicator
            delete mScreenAtIndicator[i]->bitmap;
            tpBitmap *tmpDeletedTpBitmap = mScreenAtIndicator[i];
            mScreenAtIndicator.RemoveAt(i);
            delete tmpDeletedTpBitmap;
         }
         else
         {
            i++; //skip over this indicator
         }
      }
      (*upd)++; //move to next update region
   }
   upd->Reset(); //reset the iterator to point to the first update region
}

/// AS: This function draws the cursor things, both in the
///  ruler as seen at the top of the screen, but also in each of the
///  selected tracks.
/// These are the 'vertical lines' through waves and ruler.
/// The main complexity comes from storing/restoring what was
/// previously there in bitmpas so as to make moving the cursor quick.
void TrackPanel::DrawCursors(wxDC * dc)
{
   // The difference between a wxClientDC and a wxPaintDC
   // is that the wxClientDC is used outside of a paint loop,
   // whereas the wxPaintDC is used inside a paint loop
   bool bIsClientDC = false;
   if(!dc)
   {
      bIsClientDC = true;
      dc = new wxClientDC(this);
   }

   wxMemoryDC tmpDrawDC;
   AColor::CursorColor(dc);

   //Erase old cursor
   while(!mPreviousCursorData.IsEmpty())
   {
      tmpDrawDC.SelectObject(*mPreviousCursorData[0]->bitmap);
      dc->Blit(mPreviousCursorData[0]->x, mPreviousCursorData[0]->y, mPreviousCursorData[0]->bitmap->GetWidth(), mPreviousCursorData[0]->bitmap->GetHeight(), &tmpDrawDC, 0, 0);
      tmpDrawDC.SelectObject(wxNullBitmap);
      delete mPreviousCursorData[0]->bitmap;
      tpBitmap *tmpDeletedTpBitmap = mPreviousCursorData[0];
      mPreviousCursorData.RemoveAt(0);
      delete tmpDeletedTpBitmap;
   }
   mPreviousCursorData.Clear();

   int x = GetLeftOffset() +
       int ((mViewInfo->sel0 - mViewInfo->h) * mViewInfo->zoom);

   int y = -mViewInfo->vpos + GetRulerHeight();

   // AS: Ah, no, this is where we draw the blinky thing in the ruler.
   {
      wxCoord top = 1;
      wxCoord bottom = GetRulerHeight() - 2;

      //Save bitmaps of the areas that we are going to overwrite
      wxBitmap *tmpBitmap = new wxBitmap(1, bottom-top);
      tmpDrawDC.SelectObject(*tmpBitmap);

      //Copy the part of the screen into the bitmap, using the memory DC
      tmpDrawDC.Blit(0, 0, tmpBitmap->GetWidth(), tmpBitmap->GetHeight(), dc, x, top);
      tmpDrawDC.SelectObject(wxNullBitmap);

      //Add the bitmap to the array
      tpBitmap *tmpTpBitmap = new tpBitmap;
      tmpTpBitmap->x = x;
      tmpTpBitmap->y = top;
      tmpTpBitmap->bitmap = tmpBitmap;
      mPreviousCursorData.Add(tmpTpBitmap);

      // Draw cursor in ruler
      dc->DrawLine(x, top, x, bottom);
   }

   if (x >= GetLeftOffset()) {
      // Draw cursor in all selected tracks
      TrackListIterator iter(mTracks);
      for (Track * t = iter.First(); t; t = iter.Next()) {
         int height = t->GetHeight();
         if (t->GetSelected() && t->GetKind() == Track::Time)
            {
               TimeTrack *tt = (TimeTrack*)t;
               double t0 = tt->warp( mViewInfo->sel0 - mViewInfo->h );
               int warpedX = GetLeftOffset() + int (t0 * mViewInfo->zoom);
               dc->DrawLine(warpedX, y + kTopInset + 1, warpedX, y + height - 2);
            }
         else if (t->GetSelected())// && t->GetKind() != Track::Label)
         {
            wxCoord top = y + kTopInset + 1;
            wxCoord bottom = y + height - 2;

            //Save bitmaps of the areas that we are going to overwrite
            wxBitmap *tmpBitmap = new wxBitmap(1, bottom-top);
            tmpDrawDC.SelectObject(*tmpBitmap);

            //Copy the part of the screen into the bitmap, using the memory DC
            tmpDrawDC.Blit(0, 0, tmpBitmap->GetWidth(), tmpBitmap->GetHeight(), dc, x, top);
            tmpDrawDC.SelectObject(wxNullBitmap);

            //Add the bitmap to the array
            tpBitmap *tmpTpBitmap = new tpBitmap;
            tmpTpBitmap->x = x;
            tmpTpBitmap->y = top;
            tmpTpBitmap->bitmap = tmpBitmap;
            mPreviousCursorData.Add(tmpTpBitmap);

            dc->DrawLine(x, top, x, bottom); // <-- The whole point of this routine.
         }
         y += height;
      }
   }

   if(bIsClientDC)
   {
      delete dc;
      dc = NULL;
   }
}

/// AS: This gets called on our wx timer events.
void TrackPanel::OnTimer()
{
   mTimeCount++;
   // AS: If the user is dragging the mouse and there is a track that
   //  has captured the mouse, then scroll the screen, as necessary.
   if ((mMouseCapture==IsSelecting) && mCapturedTrack) {
      ScrollDuringDrag();
   }

   wxCommandEvent dummyEvent;
   AudacityProject *p = GetProject();

   // Each time the loop, check to see if we were playing or
   // recording audio, but the stream has stopped.
   if (p->GetAudioIOToken()>0 &&
       !gAudioIO->IsStreamActive(p->GetAudioIOToken())) {
      p->GetControlToolBar()->OnStop(dummyEvent);      
   }

   // Next, check to see if we were playing or recording
   // audio, but now Audio I/O is completely finished.
   // The main point of this is to properly push the state
   // and flush the tracks once we've completely finished
   // recording new state.
   if (p->GetAudioIOToken()>0 &&
       !gAudioIO->IsAudioTokenActive(p->GetAudioIOToken())) {

      if (gAudioIO->GetNumCaptureChannels() > 0) {
         // Tracks are buffered during recording.  This flushes
         // them so that there's nothing left in the append
         // buffers.
         TrackListIterator iter(mTracks);
         for (Track * t = iter.First(); t; t = iter.Next()) {
            if (t->GetKind() == Track::Wave) {
               ((WaveTrack *)t)->Flush();
            }
         }
         MakeParentPushState(_("Recorded Audio"), _("Record"));
      }

      MakeParentRedrawScrollbars();
      p->SetAudioIOToken(0);
      p->RedrawProject();
		//ANSWER-ME: Was DisplaySelection added to solve a repaint problem?        
      DisplaySelection();
   }

   // AS: The "indicator" is the little graphical mark shown in the ruler
   //  that indicates where the current play/record position is.
   if (!gAudioIO->IsPaused() &&
       (mIndicatorShowing || gAudioIO->IsStreamActive(p->GetAudioIOToken())))
   {
      UpdateIndicator();
   }

   // AS: Um, I get the feeling we want to redraw the cursors
   //  every 10 timer ticks or something...
   if ((mTimeCount % 10) == 0 &&
       !mTracks->IsEmpty() && mViewInfo->sel0 == mViewInfo->sel1 
       && (mMouseCapture != IsSelecting)) {
      DrawCursors();
   }

   if(gAudioIO->IsStreamActive(p->GetAudioIOToken()) &&
      gAudioIO->GetNumCaptureChannels()) {

      // Periodically update the display while recording
      
      if (!mRedrawAfterStop) {
         Refresh(false);
         mRedrawAfterStop = true;
      }
      else {
         if ((mTimeCount % 5) == 0) {
            // Refresh only the waveform area, not the labels
            // (This actually speeds up redrawing!)
            wxRect trackRect;
            GetSize(&trackRect.width, &trackRect.height);
            trackRect.x = GetLeftOffset(); 
            trackRect.y = 0;
            trackRect.width -= GetLeftOffset();
            Refresh(false, &trackRect);
         }
         
         if ((mTimeCount % 40) == 0) {
            MakeParentRedrawScrollbars();
         }
      }
   }
   if(mTimeCount > 1000)
      mTimeCount = 0;
}

/// AS: We check on each timer tick to see if we need to scroll.
///  Scrolling is handled by mListener, which is an interface
///  to the window TrackPanel is embedded in.
void TrackPanel::ScrollDuringDrag()
{
   // DM: If we're "autoscrolling" (which means that we're scrolling
   //  because the user dragged from inside to outside the window,
   //  not because the user clicked in the scroll bar), then
   //  the selection code needs to be handled slightly differently.
   //  We set this flag ("mAutoScrolling") to tell the selecting
   //  code that we didn't get here as a result of a mouse event,
   //  and therefore it should ignore the mouseEvent parameter,
   //  and instead use the last known mouse position.  Setting
   //  this flag also causes the Mac to redraw immediately rather
   //  than waiting for the next update event; this makes scrolling
   //  smoother on MacOS 9.

   if (mMouseMostRecentX > mCapturedRect.x + mCapturedRect.width) {
      mAutoScrolling = true;
      mListener->TP_ScrollRight();
   }
   else if (mMouseMostRecentX < mCapturedRect.x) {
      mAutoScrolling = true;
      mListener->TP_ScrollLeft();
   }

   if (mAutoScrolling) {
      // AS: To keep the selection working properly as we scroll,
      //  we fake a mouse event (remember, this function is called
      //  from a timer tick).

      // AS: For some reason, GCC won't let us pass this directly.
      wxMouseEvent e(wxEVT_MOTION);
      HandleSelect(e);
      mAutoScrolling = false;
   }
}

/// AS: This updates the indicator (on a timer tick) that shows
///  where the current play or record position is.  To do this,
///  we cheat a little.  The indicator is drawn during the ruler
///  drawing process (that should probably change, but...), so
///  we create a memory DC and tell the ruler to draw itself there,
///  and then just blit that to the screen.
/// The indicator is a small triangle, red for record, green for play.
void TrackPanel::UpdateIndicator(wxDC * dc)
{
   double indicator = gAudioIO->GetStreamTime();
   bool onScreen = between_inclusive(mViewInfo->h, indicator,
                                     mViewInfo->h + mViewInfo->screen);

   // This displays the audio time, too...
   DisplaySelection();

   // BG: Scroll screen if option is set
   // msmeyer: But only if not playing looped or in one-second mode
   AudacityProject *p = GetProject();
   if (mViewInfo->bUpdateTrackIndicator &&
       p->mLastPlayMode != loopedPlay &&
       p->mLastPlayMode != oneSecondPlay && 
       gAudioIO->IsStreamActive(p->GetAudioIOToken()) &&
       indicator>=0 && !onScreen && !gAudioIO->IsPaused())
      mListener->TP_ScrollWindow(indicator);

   if (!mIndicatorShowing && !onScreen) 
      return;
   
   mIndicatorShowing = (onScreen &&
                        gAudioIO->IsStreamActive(p->GetAudioIOToken()));

   int width, height;
   GetSize(&width, &height);
   height = GetRulerHeight();

   bool bIsClientDC = false;
   if(!dc)
   {
      bIsClientDC = true;
      dc = new wxClientDC(this);
   }
 
   //Draw the line across all tracks specifying where play is
   DrawTrackIndicator(dc);

   wxMemoryDC memDC;
   wxBitmap rulerBitmap;
   rulerBitmap.Create(width, height);

   memDC.SelectObject(rulerBitmap);

   DrawRuler(&memDC, true);
   dc->Blit(0, 0, width, height, &memDC, 0, 0, wxCOPY, FALSE);

   if(bIsClientDC)
   {
      delete dc;
      dc = NULL;
   }
}

/// AS: OnPaint( ) is called during the normal course of 
///  completing a repaint operation.
void TrackPanel::OnPaint(wxPaintEvent & /* event */)
{
   wxPaintDC dc(this);

   wxRegionIterator upd(GetUpdateRegion()); // get the update rect list
   
#ifdef __WXMAC__

   // Mac OS X automatically double-buffers the screen for you,
   // so our bitmap is unneccessary

#if DEBUG_DRAW_TIMING
   struct timeval t1, t2;
   gettimeofday(&t1, NULL);
   std::cout << "."<< std::flush;
#endif

   dc.BeginDrawing();

   DrawTracks(&dc);
   DrawRuler(&dc);
   RemoveStaleIndicators(&upd);
   //UpdateIndicator(&dc);
   RemoveStaleCursors(&upd);
   if( mViewInfo->sel0 == mViewInfo->sel1)
      DrawCursors(&dc);

   dc.EndDrawing();
   
   #if DEBUG_DRAW_TIMING
   gettimeofday(&t2, NULL);
   wxPrintf(wxT("Total: %.3f\n"), 
          (t2.tv_sec + t2.tv_usec*0.000001) - 
          (t1.tv_sec + t1.tv_usec*0.000001));
   std::cout << "."<< std::flush;
   #endif

#else

   int width, height;
   GetSize(&width, &height);
   if (width != mPrevWidth || height != mPrevHeight || !mBitmap) {
      mPrevWidth = width;
      mPrevHeight = height;

      if (mBitmap)
         delete mBitmap;

      mBitmap = new wxBitmap(width, height);
   }

   wxMemoryDC memDC;

   memDC.SelectObject(*mBitmap);

   DrawTracks(&memDC);
   DrawRuler(&memDC);
   RemoveStaleIndicators(&upd);
   //UpdateIndicator(&memDC);
   RemoveStaleCursors(&upd);

   if(mViewInfo->sel0 == mViewInfo->sel1)
      DrawCursors(&memDC);

   dc.Blit(0, 0, width, height, &memDC, 0, 0, wxCOPY, FALSE);
#endif
}

/// AS: Make our Parent (well, whoever is listening to us) push their state.
///  this causes application state to be preserved on a stack for undo ops.
void TrackPanel::MakeParentPushState(wxString desc, wxString shortDesc,
                                     bool consolidate)
{
   mListener->TP_PushState(desc, shortDesc, consolidate);
}

void TrackPanel::MakeParentModifyState()
{
   mListener->TP_ModifyState();
}

void TrackPanel::MakeParentRedrawScrollbars()
{
   mListener->TP_RedrawScrollbars();
}

void TrackPanel::MakeParentResize()
{
   mListener->TP_HandleResize();
}

void TrackPanel::HandleShiftKey(bool down)
{
   mLastMouseEvent.m_shiftDown = down;
   HandleCursor(mLastMouseEvent);
}


/// Used to determine whether it is safe or not to perform certain
/// edits at the moment.
/// @return true if audio is being recorded or is playing.
bool TrackPanel::IsUnsafe()
{
   AudacityProject *p = GetProject();
   bool bUnsafe = (p->GetAudioIOToken()>0 &&
                  gAudioIO->IsStreamActive(p->GetAudioIOToken()));
   return bUnsafe;
}


/// Uses a previously noted 'activity' to determine what 
/// cursor to use.
/// @var mMouseCapture holds the current activity.
bool TrackPanel::SetCursorByActivity( )
{
   bool unsafe = IsUnsafe();
   
   switch( mMouseCapture )
   {
   case IsSelecting:
      SetCursor(*mSelectCursor);
      return true;
   case IsSliding:
      SetCursor( unsafe ? *mDisabledCursor : *mSlideCursor);
      return true;
   case IsEnveloping:
      SetCursor( unsafe ? *mDisabledCursor : *mEnvelopeCursor);
      return true;
   case IsRearranging:
      SetCursor( unsafe ? *mDisabledCursor : *mRearrangeCursor);
      return true;
   case IsAdjustingLabel:
      return true;
   case IsOverCutLine:
      SetCursor( unsafe ? *mDisabledCursor : *mArrowCursor);
   default:
      break;
   }
   return false;
}

/// When in the label, we can either vertical zoom or re-order tracks.
void TrackPanel::SetCursorAndTipWhenInLabel( Track * t, 
         wxMouseEvent &event, const wxChar ** ppTip )
{
   if (event.m_x >= GetVRulerOffset() &&
      t->GetKind() == Track::Wave) {
      *ppTip = _("Click to vertically zoom in, Shift-click to zoom out, Drag to create a particular zoom region.");
      SetCursor(event.ShiftDown()? *mZoomOutCursor : *mZoomInCursor);
   }
   else {
      // Set a status message if over a label
      *ppTip = _("Drag the label vertically to change the order of the tracks.");
      SetCursor(*mArrowCursor);
   }
}

/// When in the resize area we can adjust size or relative size.
void TrackPanel::SetCursorAndTipWhenInVResizeArea( Track * label, 
         bool bLinked, const wxChar ** ppTip )
{
   // Check to see whether it is the first channel of a stereo track
   if (bLinked) {
      // If we are in the label we got here 'by mistake' and we're 
      // not actually in the resize area at all.  (The resize area 
      // is shorter when it is between stereo tracks).

      // IF we are in the label THEN return.
      // Subsequently called functions can detect that a tip and 
      // cursor are still needed.
      if (label) 
         return;
      *ppTip = _("Click and drag to adjust relative size of stereo tracks.");
      SetCursor(*mResizeCursor);
   } else {
      *ppTip = _("Click and drag to resize the track.");
      SetCursor(*mResizeCursor);
   }
}

/// When in a label track, find out if we've hit anything that 
/// would cause a cursor change.
void TrackPanel::SetCursorAndTipWhenInLabelTrack( LabelTrack * pLT, 
       wxMouseEvent & event, const wxChar ** ppTip )
{
   int edge=pLT->OverGlyph(event.m_x, event.m_y);
   if(edge !=0)
   {
      SetCursor(*mArrowCursor);
   }

   //KLUDGE: We refresh the whole Label track when the icon hovered over
   //changes colouration.  As well as being inefficient we are also 
   //doing stuff that should be delegated to the label track itself.
   edge += pLT->mbHitCenter ? 4:0; 
   if( edge != pLT->mOldEdge )
   {
      pLT->mOldEdge = edge;
      //TODO: Avoid full repaint.
      //We should just repaint the rectangle containing the drag-handle.
      Refresh(false);
   }
   // IF edge!=0 THEN we've set the cursor and we're done.
   // signal this by setting the tip.
   if( edge != 0 )
   {
      *ppTip=
         (pLT->mbHitCenter ) ?
         _("Drag one or more label boundaries") :
         _("Drag label boundary");
   }
}

// The select tool can have different cursors and prompts depending on what
// we hover over, most notably when hovering over the selction boundaries.
// Determine and set the cursor and tip accordingly.
void TrackPanel::SetCursorAndTipWhenSelectTool( Track * t, 
        wxMouseEvent & event, wxRect &r, bool bMultiToolMode, const wxChar ** ppTip )
{
   SetCursor(*mSelectCursor);

   //In Multi-tool mode, give multitool prompt if no-special-hit.
   if( bMultiToolMode ) {
      #ifdef __WXMAC__
      /* i18n-hint: This string is for the Mac OS, which uses Command-, as the shortcut for Preferences */
      *ppTip = _("Multi-Tool Mode: Cmd-, for Mouse and Keyboard Preferences");
      #else
      /* i18n-hint: This string is for Windows and Linux, which uses Control-P as the shortcut for Preferences */
      *ppTip = _("Multi-Tool Mode: Ctrl-P for Mouse and Keyboard Preferences");
      #endif
   }
   
   //Make sure we are within the selected track
   if (!t || !t->GetSelected()) 
      return;

   int leftSel = TimeToPosition(mViewInfo->sel0, r.x);
   int rightSel = TimeToPosition(mViewInfo->sel1, r.x);

   // Something is wrong if right edge comes before left edge
   wxASSERT(!(rightSel < leftSel));

   // Adjusting the selection edges can be turned off in
   // the preferences...
   if (!mAdjustSelectionEdges) {
   }
   // Is the cursor over the left selection boundary?
   else if (within(event.m_x, leftSel, SELECTION_RESIZE_REGION)) {
      *ppTip = _("Click and drag to move left selection boundary.");
      SetCursor(*mAdjustLeftSelectionCursor);
   }
   // Is the cursor over the right selection boundary?
   else if (within(event.m_x, rightSel, SELECTION_RESIZE_REGION)) {
      *ppTip = _("Click and drag to move right selection boundary.");
      SetCursor(*mAdjustRightSelectionCursor);
   }
   // It's possible we didn't set the tip and cursor.
   // Subsequently called functions can detect this.
}

/// In this function we know what tool we are using,
/// so set the cursor accordingly.
void TrackPanel::SetCursorAndTipByTool( int tool, 
         wxMouseEvent & event, const wxChar ** /*ppTip*/ )
{
   bool unsafe = IsUnsafe();

   // Change the cursor based on the active tool.
   switch (tool) {
   case selectTool:
      wxFAIL;// should have already been handled 
      break;
   case envelopeTool:
      SetCursor(unsafe ? *mDisabledCursor : *mEnvelopeCursor);
      break;
   case slideTool:
      SetCursor(unsafe ? *mDisabledCursor : *mSlideCursor);
      break;
   case zoomTool:
      SetCursor(event.ShiftDown()? *mZoomOutCursor : *mZoomInCursor);
      break;
   case drawTool:
      if (unsafe)
         SetCursor(*mDisabledCursor);
      else
         SetCursor(event.AltDown()? *mSmoothCursor : *mPencilCursor);
      break;
   }
   // doesn't actually change the tip itself, but it could (should?) do at some
   // future date.
}

/// AS: TrackPanel::HandleCursor( ) sets the cursor drawn at the mouse location.
///  As this procedure checks which region the mouse is over, it is
///  appropriate to establish the message in the status bar.
void TrackPanel::HandleCursor(wxMouseEvent & event)
{
   mLastMouseEvent = event;

   // (1), If possible, set the cursor based on the current activity
   //      ( leave the StatusBar alone ).
   if( SetCursorByActivity() )
      return;

   // (2) If we are not over a track at all, set the cursor to Arrow and 
   //     clear the StatusBar, 
   wxRect r;
   int num;
   Track *label = FindTrack(event.m_x, event.m_y, true, true, &r, &num);
   Track *nonlabel = FindTrack(event.m_x, event.m_y, false, false, &r, &num);
   Track *t = label ? label : nonlabel;

   if (!t) {
      SetCursor(*mArrowCursor);
      mListener->TP_DisplayStatusMessage(wxT(""));
      return;
   }

   // (3) The easy cases are done.
   // Now we've got to hit-test against a number of different possibilities.
   // We could be over the label or a vertical ruler etc...

   // Strategy here is to set the tip when we have determined and
   // set the correct cursor.  We stop doing tests for what we've
   // hit once the tip is not NULL.
   const wxChar *tip = NULL;

   if (label) {
      SetCursorAndTipWhenInLabel( label, event, &tip );
   }

   // Are we within the vertical resize area?
   if ((tip == NULL ) && within(event.m_y, r.y + r.height, TRACK_RESIZE_REGION)) 
   {
      SetCursorAndTipWhenInVResizeArea( label, t->GetLinked(), &tip );
      // tip may still be NULL at this point, in which case we go on looking.
   }

   // Otherwise, we must be over a track of some kind 
   // Is it a label track?
   if ((tip==NULL) && (t->GetKind() == Track::Label))
   {
      // We are over a label track
      SetCursorAndTipWhenInLabelTrack( (LabelTrack*)t, event, &tip );
      // ..and if we haven't yet determined the cursor, 
      // we go on to do all the standard track hit tests.
   }  

   if( tip==NULL )
   {
      ControlToolBar * ctb = mListener->TP_GetControlToolBar();
      if( ctb == NULL )
         return;
      // JKC: DetermineToolToUse is called whenever the mouse 
      // moves.  I had some worries about calling it when in 
      // multimode as it then has to hit-test all 'objects' in
      // the track panel, but performance seems fine in 
      // practice (on a P500).
      int tool = DetermineToolToUse( ctb, event );

      tip = ctb->GetMessageForTool( tool );

      // We don't include the select tool in 
      // SetCursorAndTipByTool() because it's more complex than
      // the other tool cases.
      if( tool != selectTool )
      {
         SetCursorAndTipByTool( tool, event, &tip);
      }
      else
      {
         bool bMultiToolMode = ctb->GetMultiToolDown();
         SetCursorAndTipWhenSelectTool( t, event, r, bMultiToolMode, &tip );
      }
   }

   if (tip)
      mListener->TP_DisplayStatusMessage(tip);
}


/// AS: This function handles various ways of starting and extending
///  selections.  These are the selections you make by clicking and
///  dragging over a waveform.
void TrackPanel::HandleSelect(wxMouseEvent & event)
{
   // AS: Ok, did the user just click the mouse, release the mouse,
   //  or drag?
   if (event.ButtonDown(1)) {
      wxRect r;
      int num;

      Track *t = FindTrack(event.m_x, event.m_y, false, false, &r, &num);

      // AS: Now, did they click in a track somewhere?  If so, we want
      //  to extend the current selection or start a new selection, 
      //  depending on the shift key.  If not, cancel all selections.
      if (t)
         SelectionHandleClick(event, t, r, num);
      else
         SelectNone();

      Refresh(false);
      
   } else if (event.ButtonUp(1) || event.ButtonUp(3)) {
      SetCapturedTrack( NULL );
      //Send the new selection state to the undo/redo stack:
      MakeParentModifyState();
      
   } else if (event.ButtonDClick(1) && !event.ShiftDown()) {
      if (!mCapturedTrack) {
         wxRect r;
         int num;
         mCapturedTrack =
            FindTrack(event.m_x, event.m_y, false, false, &r, &num);
         if (!mCapturedTrack)
            return;
      }

      // Deselect all other tracks and select this one.
      SelectNone();
      
      mTracks->Select(mCapturedTrack);

      // Default behavior: select whole track
      mViewInfo->sel0 = mCapturedTrack->GetOffset();
      mViewInfo->sel1 = mCapturedTrack->GetEndTime();

      // Special case: if we're over a clip in a WaveTrack,
      // select just that clip
      if (mCapturedTrack->GetKind() == Track::Wave) {
         WaveTrack *w = (WaveTrack *)mCapturedTrack;
         WaveClip *selectedClip = w->GetClipAtX(event.m_x);
         if (selectedClip) {
            mViewInfo->sel0 = selectedClip->GetOffset();
            mViewInfo->sel1 = selectedClip->GetEndTime();      
         }
      }

      Refresh(false);
      SetCapturedTrack( NULL );
      MakeParentModifyState();
   } 
   SelectionHandleDrag(event);
}

/// AS: This function gets called when we're handling selection
///  and the mouse was just clicked.
void TrackPanel::SelectionHandleClick(wxMouseEvent & event,
        Track * pTrack, wxRect r, int num)
{
   mCapturedTrack = pTrack;
   mCapturedRect = r;
   mCapturedNum = num;

   mMouseClickX = event.m_x;
   mMouseClickY = event.m_y;
   bool startNewSelection = true;
   mMouseCapture=IsSelecting;

   if (event.ShiftDown()) {
      // If the shift button is down, extend the current selection.
      ExtendSelection(event.m_x, r.x);
      DisplaySelection();
      return;
   }

   //Make sure you are within the selected track
   if (pTrack && pTrack->GetSelected()) {
      int leftSel = TimeToPosition(mViewInfo->sel0, r.x);
      int rightSel = TimeToPosition(mViewInfo->sel1, r.x);
      wxASSERT(leftSel <= rightSel);
      // Adjusting selection edges can be turned off in the
      // preferences now
      if (!mAdjustSelectionEdges) {
      }
      // Is the cursor over the left selection boundary?
      else if (within(event.m_x, leftSel, SELECTION_RESIZE_REGION)) {
         // Pin the right selection boundary
         mSelStart = mViewInfo->sel1;
         startNewSelection = false;
      }
      // Is the cursor over the right selection boundary?
      else if (within(event.m_x, rightSel, SELECTION_RESIZE_REGION)) {
         // Pin the left selection boundary
         mSelStart = mViewInfo->sel0;
         startNewSelection = false;
      }
   }

   if (startNewSelection) {
      // If we didn't move a selection boundary, start a new selection
      SelectNone();
      StartSelection(event.m_x, r.x);
      mTracks->Select(pTrack);
      DisplaySelection();
   }

   //Determine if user clicked on a label track. 
   if (pTrack->GetKind() == Track::Label) {
      ((LabelTrack *) pTrack)->HandleMouse(event, r,//mCapturedRect,
           mViewInfo->h, mViewInfo->zoom,
           &mViewInfo->sel0, &mViewInfo->sel1);
      // IF the user clicked a label, THEN select all other tracks too
      if (((LabelTrack *) pTrack)->IsSelected()) {
         SelectAllTracks();
      }
   }
}


/// AS: Reset our selection markers.
void TrackPanel::StartSelection(int mouseXCoordinate, int trackLeftEdge)
{
   mSelStart = mViewInfo->h + ((mouseXCoordinate - trackLeftEdge)
                               / mViewInfo->zoom);
   mViewInfo->sel0 = mSelStart;
   mViewInfo->sel1 = mSelStart;
}

/// AS: If we're dragging to extend a selection (or actually,
///  if the screen is scrolling while you're selecting), we
///  handle it here.
void TrackPanel::SelectionHandleDrag(wxMouseEvent & event)
{
   // AS: If we're not in the process of selecting (set in
   //  the SelectionHandleClick above), fuhggeddaboudit.
   if ( mMouseCapture!=IsSelecting)
      return;

   // Also fuhggeddaboudit if we're not dragging and not autoscrolling.
   if (!event.Dragging() && !mAutoScrolling) 
      return;

   wxRect r      = mCapturedRect;
   int num       = mCapturedNum;
   Track *pTrack = mCapturedTrack;

   // AS: Note that FindTrack will replace r and num's values.
   if (!pTrack)
      pTrack = FindTrack(event.m_x, event.m_y, false, false, &r, &num);

   // Also fuhggeddaboudit if not in a track.
   if (!pTrack) 
      return;

   int x = mAutoScrolling ? mMouseMostRecentX : event.m_x;
   int y = mAutoScrolling ? mMouseMostRecentY : event.m_y;

   // JKC: Logic to prevent a selection smaller than 5 pixels to
   // prevent accidental dragging when selecting.
   // (if user really wants a tiny selection, they should zoom in).
   // Can someone make this value of '5' configurable in
   // preferences?
   const int minimumSizedSelection = 5; //measured in pixels
   int SelStart=TimeToPosition( mSelStart, r.x); //cvt time to pixels.
   // Abandon this drag if selecting < 5 pixels.
   if(abs( SelStart-x) < minimumSizedSelection)
       return;

   ExtendSelection(x, r.x);
   DisplaySelection();

   // Handle which tracks are selected
   int num2;
   if (0 != FindTrack(x, y, false, false, NULL, &num2)) {
      // The tracks numbered num...num2 should be selected

      TrackListIterator iter(mTracks);
      Track *t = iter.First();
      for (int i = 1; t; i++, t = iter.Next()) {
         if ((i >= num && i <= num2) || (i >= num2 && i <= num))
            mTracks->Select(t);
      }
   }

   // Refresh the ruler.
   wxRect RulerRect;
   mRuler->GetSize(&RulerRect.width, &RulerRect.height);
   RulerRect.x=0;
   RulerRect.y=0;
   Refresh(false, &RulerRect);

   // Refresh the tracks, excluding the TrackLabel as 
   // this gives a small speed improvement.
   wxRect trackRect;
   GetSize( &trackRect.width, &trackRect.height);
   trackRect.y = RulerRect.height;
   trackRect.x = GetLeftOffset(); 
   trackRect.width -= GetLeftOffset();
   Refresh(false, &trackRect);

#ifdef __WXMAC__
#if ((wxMAJOR_VERSION == 2) && (wxMINOR_VERSION <= 4))
   if (mAutoScrolling)
      MacUpdateImmediately();
#endif
#endif
}

/// Extend the existing selection
void TrackPanel::ExtendSelection(int mouseXCoordinate, int trackLeftEdge)
{
   double selend = PositionToTime(mouseXCoordinate, trackLeftEdge);

   // Edit the selection boundary nearest the mouse click.
   if (fabs(selend - mViewInfo->sel0) < fabs(selend - mViewInfo->sel1))
      mSelStart = mViewInfo->sel1;
   else
      mSelStart = mViewInfo->sel0;

   clip_bottom(selend, 0.0);

   mViewInfo->sel0 = wxMin(mSelStart, selend);
   mViewInfo->sel1 = wxMax(mSelStart, selend);
}

/// DM: Converts a position (mouse X coordinate) to 
///  project time, in seconds.  Needs the left edge of
///  the track as an additional parameter.
double TrackPanel::PositionToTime(int mouseXCoordinate, int trackLeftEdge) const
{
   return mViewInfo->h + ((mouseXCoordinate - trackLeftEdge)
                          / mViewInfo->zoom);
}


/// STM: Converts a project time to screen x position.
int TrackPanel::TimeToPosition(double projectTime, int trackLeftEdge) const
{
   return static_cast <
       int >(mViewInfo->zoom * (projectTime - mViewInfo->h) +
             trackLeftEdge);
}

/// AS: HandleEnvelope gets called when the user is changing the
///  amplitude envelope on a track.
void TrackPanel::HandleEnvelope(wxMouseEvent & event)
{
   if (event.ButtonDown(1)) {
      wxRect r;
      int num;
      mCapturedTrack = FindTrack(event.m_x, event.m_y, false, false, &r, &num);

      if (!mCapturedTrack)
         return;

      mCapturedRect = r;
      mCapturedRect.y += kTopInset;
      mCapturedRect.height -= kTopInset;
      mCapturedNum = num;
   }
   // AS: if there's actually a selected track, then forward all of the
   //  mouse events to its envelope.
   if (mCapturedTrack)
      ForwardEventToEnvelope(event);

   if (event.ButtonUp(1)) {
      mCapturedTrack = NULL;
      MakeParentPushState(
         _("Adjusted envelope."),
         _("Envelope"),
         true /* consolidate these actions --
               see UndoManager */
         );
   }
}

/// We've established we're a time track.  
/// send events for its envelope.
void TrackPanel::ForwardEventToTimeTrackEnvelope(wxMouseEvent & event)
{
   TimeTrack *ptimetrack = (TimeTrack *) mCapturedTrack;
   Envelope *pspeedenvelope = ptimetrack->GetEnvelope();
   
   wxRect envRect = mCapturedRect;
   envRect.y++;
   envRect.height -= 2;
   bool needUpdate =
      pspeedenvelope->MouseEvent(
         event, envRect,
         mViewInfo->h, mViewInfo->zoom,
         false,0.,1.);
   if( needUpdate )
   {
      //ptimetrack->Draw();
      Refresh(false);
   }
}

/// We've established we're a wave track.  
/// send events for its envelope.
void TrackPanel::ForwardEventToWaveTrackEnvelope(wxMouseEvent & event)
{
   WaveTrack *pwavetrack = (WaveTrack *) mCapturedTrack;
   Envelope *penvelope = pwavetrack->GetEnvelopeAtX(event.GetX());

   // Possibly no-envelope, for example when in spectrum view mode.
   // if so, then bail out.
   if (!penvelope)
      return;
   
   // AS: WaveTracks can be displayed in several different formats.
   //  This asks which one is in use. (ie, Wave, Spectrum, etc)
   int display = pwavetrack->GetDisplay();
   bool needUpdate = false;
   
   // AS: If we're using the right type of display for envelope operations
   //  ie one of the Wave displays
   if (display <= 1) {
      bool dB = (display == 1);
      
      // AS: Then forward our mouse event to the envelope.
      // It'll recalculate and then tell us whether or not to redraw.
      wxRect envRect = mCapturedRect;
      envRect.y++;
      envRect.height -= 2;
      float zoomMin, zoomMax;
      pwavetrack->GetDisplayBounds(&zoomMin, &zoomMax);
      needUpdate = penvelope->MouseEvent(
         event, envRect,
         mViewInfo->h, mViewInfo->zoom,
         dB, zoomMin, zoomMax);
      
      // If this track is linked to another track, make the identical
      // change to the linked envelope:
      WaveTrack *link = (WaveTrack *) mTracks->GetLink(mCapturedTrack);
      if (link) {
         Envelope *e2 = link->GetEnvelopeAtX(event.GetX());
         // There isn't necessarily an envelope there; no guarantee a
         // linked track has the same WaveClip structure...
         if (e2) {
            wxRect envRect = mCapturedRect;
            envRect.y++;
            envRect.height -= 2;
            float zoomMin, zoomMax;
            pwavetrack->GetDisplayBounds(&zoomMin, &zoomMax);
            needUpdate |= e2->MouseEvent(event, envRect,
                                         mViewInfo->h, mViewInfo->zoom, dB,
                                         zoomMin, zoomMax);
         }
      }
   }
   if (needUpdate) {
      Refresh(false);
   }
}


/// AS: The Envelope class actually handles things at the mouse
///  event level, so we have to forward the events over.  Envelope
///  will then tell us wether or not we need to redraw.
/// AS: I'm not sure why we can't let the Envelope take care of
///  redrawing itself.  ?
void TrackPanel::ForwardEventToEnvelope(wxMouseEvent & event)
{
   if (mCapturedTrack && mCapturedTrack->GetKind() == Track::Time)
   {
      ForwardEventToTimeTrackEnvelope( event );
   }
   else if (mCapturedTrack && mCapturedTrack->GetKind() == Track::Wave)
   {
      ForwardEventToWaveTrackEnvelope( event );
   }
}

void TrackPanel::HandleSlide(wxMouseEvent & event)
{
   if (event.ButtonDown(1))
      StartSlide(event);

   if (mMouseCapture != IsSliding)
      return;

   if (event.Dragging() && mCapturedTrack)
      DoSlide(event);

   if (event.ButtonUp(1)) {
      SetCapturedTrack( NULL );

      if (!mDidSlideVertically && mHSlideAmount==0)
         return;

      MakeParentRedrawScrollbars();

      wxString msg;
      bool consolidate;
      if (mDidSlideVertically) {
         msg.Printf(_("Moved clip to another track"));
         consolidate = false;
      }
      else {
         wxString direction = mHSlideAmount>0 ? _("right") : _("left");
         msg.Printf(_("Time shifted tracks/clips %s %.02f seconds"),
                    direction.c_str(), fabs(mHSlideAmount));
         consolidate = true;
      }
      MakeParentPushState(msg, _("Time Shift"), consolidate);
   }
}

/// AS: Pepare for sliding.
void TrackPanel::StartSlide(wxMouseEvent & event)
{
   wxRect r;
   int num;

   mHSlideAmount = 0.0;
   mDidSlideVertically = false;
   
   Track *vt = FindTrack(event.m_x, event.m_y, false, false, &r, &num);
   if (!vt) 
      return;

   ControlToolBar * ctb = mListener->TP_GetControlToolBar();
   bool multiToolModeActive = (ctb && ctb->GetMultiToolDown());

   if (vt->GetKind() == Track::Wave && !event.ShiftDown() &&
       !multiToolModeActive)
   {
      WaveTrack* wt = (WaveTrack*)vt;
      mCapturedClip = wt->GetClipAtX(event.m_x);
      if (mCapturedClip == NULL)
         return;

      // The captured clip is the focus, but we need to create a list
      // of all clips that have to move, also...
      
      mCapturedClipArray.Clear();

      double clickTime = 
         PositionToTime(event.m_x, GetLeftOffset());
      bool clickedInSelection =
         (wt->GetSelected() &&
          clickTime >= mViewInfo->sel0 &&
          clickTime < mViewInfo->sel1);

      if (clickedInSelection) {
         // Loop through every clip and include it if it overlaps the
         // selection.  This will get mCapturedClip (and its stereo pair)
         // automatically

         mCapturedClipIsSelection = true;

         TrackListIterator iter(mTracks);
         Track *t = iter.First();
         while (t) {
            if (t->GetSelected()) {
               if (t->GetKind() == Track::Wave) {
                  WaveTrack *wt = (WaveTrack *)t;
                  WaveClipList::Node* it;
                  for(it=wt->GetClipIterator(); it; it=it->GetNext()) {
                     WaveClip *clip = it->GetData();
                     double clip0 = clip->GetStartTime();
                     double clip1 = clip->GetEndTime();
                     
                     if (clip0 <= mViewInfo->sel1 &&
                         clip1 >= mViewInfo->sel0) {
                        mCapturedClipArray.Add(TrackClip(wt, clip));
                     }
                  }      
               }
               else {
                  mCapturedClipArray.Add(TrackClip(t, NULL));
               }
            }
            t = iter.Next();
         }
      }
      else {
         // Only add mCapturedClip, and possibly its stereo partner,
         // to the list of clips to move.

         mCapturedClipIsSelection = false;

         mCapturedClipArray.Add(TrackClip(wt, mCapturedClip));

         Track *partner = mTracks->GetLink(wt);
         if (partner && partner->GetKind()==Track::Wave) {
            WaveClip *clip = ((WaveTrack *)partner)->GetClipAtX(event.m_x);
            if (clip) {
               mCapturedClipArray.Add(TrackClip(partner, clip));
            }
         }
      }


   } else {
      mCapturedClip = NULL;
      mCapturedClipArray.Clear();
   }
   
   mSlideUpDownOnly = event.ControlDown();

   mCapturedTrack = vt;
   mCapturedRect = r;
   mCapturedNum = num;

   mMouseClickX = event.m_x;
   mMouseClickY = event.m_y;

   mSelStart = mViewInfo->h + ((event.m_x - r.x) / mViewInfo->zoom);
   mMouseCapture = IsSliding;
}

// Slide tracks horizontally, or slide clips horizontally or vertically
// (e.g. moving clips between tracks).
// GM: DoSlide now implementing snap-to
// samples functionality based on sample rate. 
void TrackPanel::DoSlide(wxMouseEvent & event)
{
   int i;

   // find which track the mouse is currently in (mouseTrack) -
   // this may not be the same as the one we started in...
   WaveTrackArray tracks = mTracks->GetWaveTrackArray(false);
   WaveTrack* mouseTrack = NULL;
   for (i=0; i<(int)tracks.GetCount(); i++)
   {
      WaveTrack* track = tracks.Item(i);
      wxRect r;
      track->GetDisplayRect(&r);
      if (event.m_y >= r.y && event.m_y < r.y+r.height)
      {
         mouseTrack = track;
         break;
      }
   }

   // Start by undoing the current slide amount; everything
   // happens relative to the original horizontal position of
   // each clip...
   if (mCapturedClip) {
      for(i=0; i<mCapturedClipArray.GetCount(); i++) {
         if (mCapturedClipArray[i].clip)
            mCapturedClipArray[i].clip->Offset(-mHSlideAmount);
         else
            mCapturedClipArray[i].track->Offset(-mHSlideAmount);
      }
   }
   else
      mCapturedTrack->Offset(-mHSlideAmount);
   if (mCapturedClipIsSelection) {
      // Slide the selection, too
      mViewInfo->sel0 -= mHSlideAmount;
      mViewInfo->sel1 -= mHSlideAmount;
   }
   mHSlideAmount = 0.0;

   double desiredSlideAmount = (event.m_x - mMouseClickX) / mViewInfo->zoom;

   // TODO: adjust desiredSlideAmount by snapping track to nearby
   // snap points...

   //If the mouse is over a track that isn't the captured track,
   //drag the clip to the mousetrack
   if (mCapturedClip && mouseTrack && mouseTrack != mCapturedTrack &&
       !mCapturedClipIsSelection)
   {
      // Make sure we always have the first linked track of a stereo track
      if (!mouseTrack->GetLinked() && mTracks->GetLink(mouseTrack))
         mouseTrack = (WaveTrack*)mTracks->GetLink(mouseTrack);

      // Temporary apply the offset because we want to see if the
      // track fits with the desired offset
      for(i=0; i<mCapturedClipArray.GetCount(); i++)
         if (mCapturedClipArray[i].clip)
            mCapturedClipArray[i].clip->Offset(desiredSlideAmount);
      // See if it can be moved
      if (MoveClipToTrack(mCapturedClip,
                          (WaveTrack*)mCapturedTrack, mouseTrack)) {
         mCapturedTrack = mouseTrack;
         mDidSlideVertically = true;

         // Make the offset permanent; start from a "clean slate"
         mHSlideAmount = 0.0;
         desiredSlideAmount = 0.0;
         mMouseClickX = event.m_x;
      }
      else {
         // Undo the offset
         for(i=0; i<mCapturedClipArray.GetCount(); i++)
            if (mCapturedClipArray[i].clip)
               mCapturedClipArray[i].clip->Offset(-desiredSlideAmount);
      }

      Refresh(false);
   }

   // Implement sliding within the track(s)
   if (mSlideUpDownOnly)
      return;
      
   // Determine desired amount to slide
   mHSlideAmount = desiredSlideAmount;

   if (mHSlideAmount == 0.0) {
      Refresh(false);
      return;
   }

   if (mCapturedClip) {
      double allowed;
      double initialAllowed;
      double safeBigDistance = 1000 + 2.0 * (mTracks->GetEndTime() -
                                             mTracks->GetStartTime());

      do {
         initialAllowed = mHSlideAmount;

         unsigned int i, j;
         for(i=0; i<mCapturedClipArray.GetCount(); i++) {
            WaveTrack *track = (WaveTrack *)mCapturedClipArray[i].track;
            WaveClip *clip = mCapturedClipArray[i].clip;
            
            if (clip) {
               // Move all other selected clips totally out of the way
               // temporarily because they're all moving together and
               // we want to find out if OTHER clips are in the way,
               // not one of the moving ones
               for(j=0; j<mCapturedClipArray.GetCount(); j++) {
                  WaveClip *clip2 = mCapturedClipArray[j].clip;
                  if (clip2 && clip2 != clip)
                     clip2->Offset(-safeBigDistance);
               }
               
               if (track->CanOffsetClip(clip, mHSlideAmount, &allowed)) {
                  mHSlideAmount = allowed;
               }
               else
                  mHSlideAmount = 0.0;
               
               for(j=0; j<mCapturedClipArray.GetCount(); j++) {
                  WaveClip *clip2 = mCapturedClipArray[j].clip;
                  if (clip2 && clip2 != clip)
                     clip2->Offset(safeBigDistance);
               }
            }               
         }
      } while (mHSlideAmount != initialAllowed);

      if (mHSlideAmount != 0.0) {
         unsigned int i;
         for(i=0; i<mCapturedClipArray.GetCount(); i++) {
            Track *track = mCapturedClipArray[i].track;
            WaveClip *clip = mCapturedClipArray[i].clip;
            if (clip)
               clip->Offset(mHSlideAmount);
            else
               track->Offset(mHSlideAmount);
         }
      }

      if (mCapturedClipIsSelection) {
         // Slide the selection, too
         mViewInfo->sel0 += mHSlideAmount;
         mViewInfo->sel1 += mHSlideAmount;
      }
   }
   else {
      // For non wavetracks...
      mCapturedTrack->Offset(mHSlideAmount);
   }

   Refresh(false);
}


/// AS: This function takes care of our different zoom 
///  possibilities.  It is possible for a user to just
///  "zoom in" or "zoom out," but it is also possible 
///  for a user to drag and select an area that he
///  or she wants to be zoomed in on.  We use mZoomStart
///  and mZoomEnd to track the beggining and end of such
///  a zoom area.  Note that the ViewInfo member
///  mViewInfo actually keeps track of our zoom constant,
///  so we achieve zooming by altering the zoom constant
///  and forcing a refresh.
void TrackPanel::HandleZoom(wxMouseEvent & event)
{
   if (event.ButtonDown() || event.ButtonDClick(1)) {
      HandleZoomClick( event );
   }
   else if (event.Dragging()) {
      HandleZoomDrag( event );
   }
   else if (event.ButtonUp()) {
      HandleZoomButtonUp( event );
   }
}

/// DMM: Vertical zooming (triggered by clicking in the
/// vertical ruler)
void TrackPanel::HandleVZoom(wxMouseEvent & event)
{
   if (event.ButtonDown() || event.ButtonDClick()) {
      HandleVZoomClick( event );
   }
   else if (event.Dragging()) {
      HandleVZoomDrag( event );
   }
   else if (event.ButtonUp()) {
      HandleVZoomButtonUp( event );
   }
}

/// Zoom button down, record the position.
void TrackPanel::HandleZoomClick(wxMouseEvent & event)
{
   mZoomStart = event.m_x;
   mZoomEnd = event.m_x;
}

/// Zoom drag
void TrackPanel::HandleZoomDrag(wxMouseEvent & event)
{
   mZoomEnd = event.m_x;
   if (IsDragZooming()){
      Refresh(false);
   }
}

/// Zoom button up
void TrackPanel::HandleZoomButtonUp(wxMouseEvent & event)
{
   if (mZoomEnd < mZoomStart) {
      int temp = mZoomEnd;
      mZoomEnd = mZoomStart;
      mZoomStart = temp;
   }

   if (IsDragZooming())
      DragZoom(GetLabelWidth()+1);
   else
      DoZoomInOut(event, GetLabelWidth()+1);

   mZoomEnd = mZoomStart = 0;

   MakeParentRedrawScrollbars();
   Refresh(false);
}

// AS: This actually sets the Zoom value when you're done doing
//  a drag zoom.
void TrackPanel::DragZoom(int trackLeftEdge)
{
   double left = PositionToTime(mZoomStart, trackLeftEdge);
   double right = PositionToTime(mZoomEnd, trackLeftEdge);

   mViewInfo->zoom *= mViewInfo->screen / (right - left);
   if (mViewInfo->zoom > gMaxZoom)
      mViewInfo->zoom = gMaxZoom;
   if (mViewInfo->zoom <= gMinZoom)
      mViewInfo->zoom = gMinZoom;

   mViewInfo->h = left;
}

// AS: This handles normal Zoom In/Out, if you just clicked;
//  IOW, if you were NOT dragging to zoom an area.
// AS: MAGIC NUMBER: We've got several in this function.
void TrackPanel::DoZoomInOut(wxMouseEvent & event, int trackLeftEdge)
{
   double center_h = PositionToTime(event.m_x, trackLeftEdge);

   if (event.RightUp() || event.RightDClick() || event.ShiftDown())
      mViewInfo->zoom = wxMax(mViewInfo->zoom / 2.0, gMinZoom);
   else
      mViewInfo->zoom = wxMin(mViewInfo->zoom * 2.0, gMaxZoom);

   if (event.MiddleUp() || event.MiddleDClick())
      mViewInfo->zoom = 44100.0 / 512.0;        // AS: Reset zoom.

   double new_center_h = PositionToTime(event.m_x, trackLeftEdge);

   mViewInfo->h += (center_h - new_center_h);
}

/// VZoom click
void TrackPanel::HandleVZoomClick( wxMouseEvent & event )
{
   if (mCapturedTrack)
      return;
   mCapturedTrack = FindTrack(event.m_x, event.m_y, true, false,
                              &mCapturedRect, &mCapturedNum);
   if (!mCapturedTrack)
      return;
   if (mCapturedTrack->GetKind() != Track::Wave)
      return;

   mMouseCapture = IsVZooming;
   mZoomStart = event.m_y;
   mZoomEnd = event.m_y;
}

/// VZoom drag
void TrackPanel::HandleVZoomDrag( wxMouseEvent & event )
{
   mZoomEnd = event.m_y;
   if (IsDragZooming()){
      Refresh(false);
   }
}

/// VZoom Button up.
/// There are three cases:
///   1) Drag-zooming; we already have a min and max
///   2) Zoom out; ensure we don't go too small.
///   3) Zoom in; ensure we don't go too large.
void TrackPanel::HandleVZoomButtonUp( wxMouseEvent & event )
{
   if (!mCapturedTrack)
      return;

   mMouseCapture = IsUncaptured;
   if (mCapturedTrack->GetKind() != Track::Wave)
      return;

   WaveTrack *track = (WaveTrack *)mCapturedTrack;
   WaveTrack *partner = (WaveTrack *)mTracks->GetLink(track);
   int height = track->GetHeight();
   int ypos = mCapturedRect.y;

   // Ensure start and end are in order (swap if not).
   if (mZoomEnd < mZoomStart) {
      int temp = mZoomEnd;
      mZoomEnd = mZoomStart;
      mZoomStart = temp;
   }

   float min, max, c, l;
   track->GetDisplayBounds(&min, &max);

   if (IsDragZooming()) {
      // Drag Zoom
      float p1, p2, tmin, tmax;
      tmin=min;
      tmax=max;

      p1 = (mZoomStart - ypos) / (float)height;
      p2 = (mZoomEnd - ypos) / (float)height;
      max = (tmax * (1.0-p1) + tmin * p1);
      min = (tmax * (1.0-p2) + tmin * p2);

      // Enforce maximum vertical zoom
      if (max - min < 0.2) {
         c = (min+max/2);
         min = c-0.1;
         max = c+0.1;
      }
   }
   else if (event.ShiftDown() || event.ButtonUp(3)) {
      // Zoom OUT
      // Zoom out to -1.0...1.0 first, then, and only
      // then, if they click again, allow one more
      // zoom out.
      if (min <= -1.0 && max >= 1.0) {
         min = -2.0;
         max = 2.0;
      }
      else {
         c = 0.5*(min+max);
         l = (c - min);
         min = wxMax( -1.0, c - 2*l);
         max = wxMin(  1.0, c + 2*l);
      }
   }
   else {
      // Zoom IN
      float p1;

      // Zoom in centered on cursor
      if (min < -1.0 || max > 1.0) {
         min = -1.0;
         max = 1.0;
      }
      else {
         c = 0.5*(min+max);
         // Enforce maximum vertical zoom
         l = wxMax( 0.1, (c - min));

         p1 = (mZoomStart - ypos) / (float)height;
         c = (max * (1.0-p1) + min * p1);
         min = c - 0.5*l;
         max = c + 0.5*l;
      }
   }

   track->SetDisplayBounds(min, max);
   if (partner)
      partner->SetDisplayBounds(min, max);

   mZoomEnd = mZoomStart = 0;
   Refresh(false);
   mCapturedTrack = NULL;
   MakeParentModifyState();
}

/// Determines if we can edit samples in a wave track.
/// Also pops up warning messages in certain cases where we can't.
///  @return true if we can edit the samples, false otherwise.
bool TrackPanel::IsSampleEditingPossible( wxMouseEvent & event )
{
   //If the mouse isn't over a track, exit the function and don't do anything
   if(mDrawingTrack == NULL) 
      return false;
   
   //Also exit if it's not a WaveTrack
   if(mDrawingTrack->GetKind() != Track::Wave)
      return false;
   
   // Get out of here if we shouldn't be drawing right now:
   // If we aren't displaying the waveform, Display a message dialog
   if(((WaveTrack *)mDrawingTrack)->GetDisplay() != WaveTrack::WaveformDisplay)
   {
      wxMessageBox(_("Draw currently only works with linear-view waveforms."), wxT("Notice"));
      return false;
   }
   
   //Get rate in order to calculate the critical zoom threshold
   //Find out the zoom level
   double rate = ((WaveTrack *)mDrawingTrack)->GetRate();
   bool showPoints = (mViewInfo->zoom / rate > 3.0);

   //If we aren't zoomed in far enough, show a message dialog.
   if(!showPoints)
   {
      wxMessageBox(_("You are not zoomed in enough. Zoom in until you can see the individual samples."), wxT("Notice"));
      return false;
   }
   return true;
}

/// We're in a track view and zoomed enough to see the samples.
/// Someone has just clicked the mouse.  What do we do?
void TrackPanel::HandleSampleEditingClick( wxMouseEvent & event )
{
   //declare a rectangle to determine clicking position
   wxRect r;
   int dummy;
   
   //Get the track the mouse is over, and save it away for future events
   //TODO: Should mCapturedTrack take the place of mDrawingTrack??
   mDrawingTrack = FindTrack(event.m_x, event.m_y, false, false, &r, &dummy);
   mDrawingTrackTop=r.y;

   if( !IsSampleEditingPossible( event ) )
   {
      mDrawingTrack = NULL;
      if( HasCapture() )
         ReleaseMouse();
      return;
   }

   //If we are still around, we are drawing in earnest.  Set some member data structures up:
   //First, calculate the starting sample.  To get this, we need the time
   double t0 = PositionToTime(event.m_x, GetLeftOffset());
   double rate = ((WaveTrack *)mDrawingTrack)->GetRate();
   float newLevel;   //Declare this for use later

   //convert t0 to samples
   mDrawingStartSample = (sampleCount) (double)(t0 * rate + 0.5 );
   
   //Now, figure out what the value of that sample is.      
   //First, get the sequence of samples so you can mess with it
   //Sequence *seq = ((WaveTrack *)mDrawingTrack)->GetSequence();


   //Determine how drawing should occur.  If alt is down, 
   //do a smoothing, instead of redrawing.
   if( event.m_altDown ) 
   {
      //*************************************************
      //***  ALT-DOWN-CLICK (SAMPLE SMOOTHING)        ***
      //*************************************************
      //
      //  Smoothing works like this:  There is a smoothing kernel radius constant that
      //  determines how wide the averaging window is.  Plus, there is a smoothing brush radius, 
      //  which determines how many pixels wide around the selected pixel this smoothing is applied.
      //
      //  Samples will be replaced by a mixture of the original points and the smoothed points, 
      //  with a triangular mixing probability whose value at the center point is 
      //  SMOOTHING_PROPORTION_MAX and at the far bounds is SMOOTHING_PROPORTION_MIN

      //Get the region of samples around the selected point
      int sampleRegionSize = 1 + 2 * (SMOOTHING_KERNEL_RADIUS + SMOOTHING_BRUSH_RADIUS);
      float *sampleRegion = new float[sampleRegionSize];
      float * newSampleRegion = new float[1 + 2 * SMOOTHING_BRUSH_RADIUS];
      
      //Get a sample  from the track to do some tricks on.
      ((WaveTrack*)mDrawingTrack)->Get((samplePtr)sampleRegion, floatSample, 
                                       (int)mDrawingStartSample - SMOOTHING_KERNEL_RADIUS - SMOOTHING_BRUSH_RADIUS,
                                       sampleRegionSize);
      int i, j;

      //Go through each point of the smoothing brush and apply a smoothing operation.
      for(j = -SMOOTHING_BRUSH_RADIUS; j <= SMOOTHING_BRUSH_RADIUS; j++){
         float sumOfSamples = 0;
         for (i= -SMOOTHING_KERNEL_RADIUS; i <= SMOOTHING_KERNEL_RADIUS; i++){
            //Go through each point of the smoothing kernel and find the average
            
            //The average is a weighted average, scaled by a weighting kernel that is simply triangular
            // A triangular kernel across N items, with a radius of R ( 2 R + 1 points), if the farthest:
            // points have a probability of a, the entire triangle has total probability of (R + 1)^2.
            //      For sample number i and middle brush sample M,  (R + 1 - abs(M-i))/ ((R+1)^2) gives a 
            //   legal distribution whose total probability is 1.
            //
            //
            //                weighting factor                       value  
            sumOfSamples += (SMOOTHING_KERNEL_RADIUS + 1 - abs(i)) * sampleRegion[i + j + SMOOTHING_KERNEL_RADIUS + SMOOTHING_BRUSH_RADIUS];

         }
         newSampleRegion[j + SMOOTHING_BRUSH_RADIUS] = sumOfSamples/((SMOOTHING_KERNEL_RADIUS + 1) *(SMOOTHING_KERNEL_RADIUS + 1) );
      }
      

      // Now that the new sample levels are determined, go through each and mix it appropriately
      // with the original point, according to a 2-part linear function whose center has probability
      // SMOOTHING_PROPORTION_MAX and extends out SMOOTHING_BRUSH_RADIUS, at which the probability is
      // SMOOTHING_PROPORTION_MIN.  _MIN and _MAX specify how much of the smoothed curve make it through.
      
      float prob;
      
      for(j=-SMOOTHING_BRUSH_RADIUS; j <= SMOOTHING_BRUSH_RADIUS; j++){

         prob = SMOOTHING_PROPORTION_MAX - (float)abs(j)/SMOOTHING_BRUSH_RADIUS * (SMOOTHING_PROPORTION_MAX - SMOOTHING_PROPORTION_MIN);

         newSampleRegion[j+SMOOTHING_BRUSH_RADIUS] =
            newSampleRegion[j + SMOOTHING_BRUSH_RADIUS] * prob + 
            sampleRegion[SMOOTHING_BRUSH_RADIUS + SMOOTHING_KERNEL_RADIUS + j] * (1 - prob);
      }
      //Set the sample to the point of the mouse event
      ((WaveTrack*)mDrawingTrack)->Set((samplePtr)newSampleRegion, floatSample, mDrawingStartSample - SMOOTHING_BRUSH_RADIUS, 1 + 2 * SMOOTHING_BRUSH_RADIUS);

      //Clean this up right away to avoid a memory leak
      delete[] sampleRegion;
      delete[] newSampleRegion;
   } 
   else 
   {
      //*************************************************
      //***   PLAIN DOWN-CLICK (NORMAL DRAWING)       ***
      //*************************************************

      //Otherwise (e.g., the alt button is not down) do normal redrawing, based on the mouse position.
      // Calculate where the mouse is located vertically (between +/- 1)
      ((WaveTrack*)mDrawingTrack)->Get((samplePtr)&mDrawingStartSampleValue, floatSample,(int) mDrawingStartSample, 1);
      float zoomMin, zoomMax;
      ((WaveTrack *)mDrawingTrack)->GetDisplayBounds(&zoomMin, &zoomMax);
      newLevel = zoomMax -
         ((event.m_y - mDrawingTrackTop)/(float)mDrawingTrack->GetHeight()) *
         (zoomMax - zoomMin);

      //Take the envelope into account
      Envelope *env = ((WaveTrack *)mDrawingTrack)->GetEnvelopeAtX(event.GetX());

      if (env)
      {
         double envValue = env->GetValue(t0);
         if (envValue > 0)
            newLevel /= envValue;
         else
            newLevel = 0;
   
         //Make sure the new level is between +/-1
         newLevel = newLevel >  1.0 ?  1.0: newLevel;
         newLevel = newLevel < -1.0 ? -1.0: newLevel;
      }

      //Set the sample to the point of the mouse event
      ((WaveTrack*)mDrawingTrack)->Set((samplePtr)&newLevel, floatSample, mDrawingStartSample, 1);
   }

   //Set the member data structures for drawing
   mDrawingLastDragSample=mDrawingStartSample;
   mDrawingLastDragSampleValue = newLevel;

   //Redraw the region of the selected track
   Refresh(false);
}

void TrackPanel::HandleSampleEditingDrag( wxMouseEvent & event )
{
   //*************************************************
   //***    DRAG-DRAWING                           ***
   //*************************************************

   //The following will happen on a drag or a down-click.
   // The point should get re-drawn at the location of the mouse.
   //Exit if the mDrawingTrack is null.
   if( mDrawingTrack == NULL)
      return;
   
   //Exit dragging if the alt key is down--Don't allow left-right dragging for smoothing operation
   if (event.m_altDown)
      return;

   //Get the rate of the sequence, for use later
   float rate = ((WaveTrack *)mDrawingTrack)->GetRate();
   sampleCount s0;     //declare this for use below.  It designates the sample number which to draw.

   // Figure out what time the click was at
   double t0 = PositionToTime(event.m_x, GetLeftOffset());
   float newLevel;
   //Find the point that we want to redraw at. If the control button is down, 
   //adjust only the originally clicked-on sample 

   //*************************************************
   //***   CTRL-DOWN (Hold Initial Sample Constant ***
   //*************************************************

   if( event.m_controlDown) {
      s0 = mDrawingStartSample;
   } 
   else 
   {
      //*************************************************
      //***    Normal CLICK-drag  (Normal drawing)    ***
      //*************************************************

      //Otherwise, adjust the sample you are dragging over right now.
      //convert this to samples
      s0 = (sampleCount) (double)(t0 * rate + 0.5);
   }

   //Sequence *seq = ((WaveTrack *)mDrawingTrack)->GetSequence();
   ((WaveTrack*)mDrawingTrack)->Get((samplePtr)&mDrawingStartSampleValue, floatSample, (int)mDrawingStartSample, 1);
   
   //Otherwise, do normal redrawing, based on the mouse position.
   // Calculate where the mouse is located vertically (between +/- 1)


   float zoomMin, zoomMax;
   ((WaveTrack *)mDrawingTrack)->GetDisplayBounds(&zoomMin, &zoomMax);
   newLevel = zoomMax -
      ((event.m_y - mDrawingTrackTop)/(float)mDrawingTrack->GetHeight()) *
      (zoomMax - zoomMin);

   //Take the envelope into account
   Envelope *env = ((WaveTrack *)mDrawingTrack)->GetEnvelopeAtX(event.GetX());
   if (env)
   {
      double envValue = env->GetValue(t0);
      if (envValue > 0)
         newLevel /= envValue;
      else
         newLevel = 0;

      //Make sure the new level is between +/-1
      newLevel = newLevel >  1.0 ?  1.0: newLevel;
      newLevel = newLevel < -1.0 ? -1.0: newLevel;
   }

   //Now, redraw all samples between current and last redrawn sample
   
   float tmpvalue;
   //Handle cases of 0 or 1 special, to improve speed
   //JKC I don't think this makes any noticeable difference to speed
   // whatsoever!  The real reason for the special case is probably to 
   // avoid division by zero....
   if(abs(s0 - mDrawingLastDragSample) <= 1){
      ((WaveTrack*)mDrawingTrack)->Set((samplePtr)&newLevel,  floatSample, s0, 1);         
   }
   else 
   {
      //Go from the smaller to larger sample. 
      int start = wxMin( s0, mDrawingLastDragSample) +1;
      int end   = wxMax( s0, mDrawingLastDragSample);
      for(sampleCount i= start; i<= end; i++) {
         //This interpolates each sample linearly:
         tmpvalue=mDrawingLastDragSampleValue + (newLevel - mDrawingLastDragSampleValue)  * 
            (float)(i-mDrawingLastDragSample)/(s0-mDrawingLastDragSample );
         ((WaveTrack*)mDrawingTrack)->Set((samplePtr)&tmpvalue, floatSample, i, 1);
      }
   }
   //Update the member data structures.
   mDrawingLastDragSample=s0;
   mDrawingLastDragSampleValue = newLevel;

   //Redraw the region of the selected track
   Refresh(false);
}

void TrackPanel::HandleSampleEditingButtonUp( wxMouseEvent & event )
{
   //*************************************************
   //***    UP-CLICK  (Finish drawing)             ***
   //*************************************************
   
   //On up-click, send the state to the undo stack
   mDrawingTrack=NULL;       //Set this to NULL so it will catch improper drag events.
   MakeParentPushState(_("Moved Sample"),
                       _("Sample Edit"),
                       true /* consolidate */);
}


// BG: This handles adjusting individual samples by hand using the draw tool(s)
//  Stm:
// There are several member data structure for handling drawing:
//   mDrawingTrack:               keeps track of which track you clicked down on, so drawing doesn't 
//                                jump to a new track
//   mDrawingTrackTop:            The top position of the drawing track--makes drawing easier.
//   mDrawingStartSample:         The sample you clicked down on, so that you can hold it steady
//   mDrawingStartSampleValue:    The original value of the initial sample
//   mDrawingLastDragSample:      When drag-drawing, this keeps track of the last sample you dragged over,
//                                so it can smoothly redraw samples that got skipped over
//   mDrawingLastDragSampleValue: The value of the last 
void TrackPanel::HandleSampleEditing(wxMouseEvent & event)
{
   if (event.ButtonDown(1) ) {
      HandleSampleEditingClick( event);
   } else if (event.Dragging()) {
      HandleSampleEditingDrag( event );
   }  else if(event.ButtonUp()) {
      HandleSampleEditingButtonUp( event );
   }
}


// AS: This is for when a given track gets the x.
void TrackPanel::HandleClosing(wxMouseEvent & event)
{
   AudacityProject *p = GetProject(); //lda

   Track *t = mCapturedTrack;
   wxRect r = mCapturedRect;

   wxRect closeRect;
   mTrackLabel.GetCloseBoxRect(r, closeRect);

   wxClientDC dc(this);

   if (event.Dragging())
      mTrackLabel.DrawCloseBox(&dc, r, closeRect.Inside(event.m_x, event.m_y));
   else if (event.ButtonUp(1)) {
      mTrackLabel.DrawCloseBox(&dc, r, false);
      if (closeRect.Inside(event.m_x, event.m_y)) {
         if (!gAudioIO->IsStreamActive(p->GetAudioIOToken()))
            RemoveTrack(t);
      }
      SetCapturedTrack( NULL );
   }
   // BG: There are no more tracks on screen
   if (mTracks->IsEmpty()) {
      //BG: Set zoom to normal
      mViewInfo->zoom = 44100.0 / 512.0;

      //STM: Set selection to 0,0
      mViewInfo->sel0 = 0.0;
      mViewInfo->sel1 = 0.0;

      mListener->TP_RedrawScrollbars();
      mListener->TP_DisplayStatusMessage(wxT("")); //STM: Clear message if all tracks are removed
      
      Refresh(false);
   }
}

// This actually removes the specified track.  Called from HandleClosing.
void TrackPanel::RemoveTrack(Track * toRemove)
{
   TrackListIterator iter(mTracks);

   Track *partner = mTracks->GetLink(toRemove);
   wxString name;

   Track *t = iter.First();
   while (t) {
      if (t == toRemove || t == partner) {
         name = t->GetName();
         delete t;
         t = iter.RemoveCurrent();
      } else
         t = iter.Next();
   }

   MakeParentPushState(
      wxString::Format(_("Removed track '%s.'"),
      name.c_str()),
      _("Track Remove"));
   MakeParentRedrawScrollbars();
   MakeParentResize();
   Refresh(false);
}

// AS: Handle when the mute or solo button is pressed for some track.
void TrackPanel::HandleMutingSoloing(wxMouseEvent & event, bool solo)
{
   Track *t = mCapturedTrack;
   wxRect r = mCapturedRect;

   if( t==NULL ){
      wxASSERT(false);// Soloing or muting but no captured track!
      SetCapturedTrack( NULL );
      return;
   }

   wxRect buttonRect;
   mTrackLabel.GetMuteSoloRect(r, buttonRect, solo);

   wxClientDC dc(this);

   if (event.Dragging()){
         mTrackLabel.DrawMuteSolo(&dc, r, t, buttonRect.Inside(event.m_x, event.m_y),
                   solo);
   }
   else if (event.ButtonUp(1) )
      {
         
         if (buttonRect.Inside(event.m_x, event.m_y)) 
            {
               if(solo)
                  
                  OnTrackSolo(t,event.ShiftDown());
               else
                  OnTrackMute(t,event.ShiftDown());
            }  
         SetCapturedTrack( NULL );
         // mTrackLabel.DrawMuteSolo(&dc, r, t, false, solo);
         Refresh(false);
      }
}

void TrackPanel::HandleMinimizing(wxMouseEvent & event)
{
   Track *t = mCapturedTrack;
   wxRect r = mCapturedRect;

   if( t==NULL ){
      SetCapturedTrack( NULL );
      return;
   }

   wxRect buttonRect;
   mTrackLabel.GetMinimizeRect(r, buttonRect, t->GetMinimized());

   wxClientDC dc(this);

   if (event.Dragging()) {
      mTrackLabel.DrawMinimize(&dc, r, t, buttonRect.Inside(event.m_x, event.m_y), t->GetMinimized());
   }
   else if (event.ButtonUp(1)) {
      if (buttonRect.Inside(event.m_x, event.m_y))
      {
         t->SetMinimized(!t->GetMinimized());
         if (mTracks->GetLink(t))
            mTracks->GetLink(t)->SetMinimized(t->GetMinimized());
         MakeParentRedrawScrollbars();
         MakeParentModifyState();
      }

      SetCapturedTrack( NULL );

      mTrackLabel.DrawMinimize(&dc, r, t, false, t->GetMinimized());
      Refresh(false);
   }
}

void TrackPanel::HandleSliders(wxMouseEvent &event, bool pan)
{
   LWSlider *slider;

   if (pan)
      slider = mTrackLabel.mPans[mCapturedNum];
   else
      slider = mTrackLabel.mGains[mCapturedNum];

   slider->OnMouseEvent(event);

   //If we have a double-click, do this...
   if(event.ButtonDClick(1))
      {
         mMouseCapture = IsUncaptured;
      }

   float newValue = slider->Get();
   WaveTrack *link = (WaveTrack *)mTracks->GetLink(mCapturedTrack);
   
   if (pan) {
      ((WaveTrack *)mCapturedTrack)->SetPan(newValue);
      if (link)
         link->SetPan(newValue);
   }
   else {
      ((WaveTrack *)mCapturedTrack)->SetGain(newValue);
      if (link)
         link->SetGain(newValue);
   }
   
   if (event.ButtonUp()) {
      MakeParentPushState(pan ? _("Moved pan slider") : _("Moved gain slider"),
                          pan ? _("Pan") : _("Gain"),
                          true /* consolidate */);
      SetCapturedTrack( NULL );
   }

   this->Refresh(false);
}

void TrackPanel::OnContextMenu(wxContextMenuEvent & event)
{
   OnTrackMenu(GetFirstSelectedTrack());
}

// AS: This function gets called when a user clicks on the
//  title of a track, dropping down the menu.
void TrackPanel::DoPopupMenu(wxMouseEvent & event, wxRect & titleRect,
                             Track * t, wxRect & r, int num)
{
   ReleaseMouse();
   mPopupMenuTarget = t;
   {
      wxClientDC dc(this);
      mTrackLabel.DrawTitleBar(&dc, r, t, true);
   }
 
   OnTrackMenu(t);

   Track *t2 = FindTrack(event.m_x, event.m_y, true, true, &r, &num);
   if (t2 == t) {
      wxClientDC dc(this);
      mTrackLabel.DrawTitleBar(&dc, r, t, false);
   }
}

// AS: This handles when the user clicks on the "Label" area
//  of a track, ie the part with all the buttons and the drop
//  down menu, etc.
void TrackPanel::HandleLabelClick(wxMouseEvent & event)
{
   // AS: If not a click, ignore the mouse event.
   if (!(event.ButtonDown(1) || event.ButtonDClick(1)))
      return;

   AudacityProject *p = GetProject();
   bool unsafe = (p->GetAudioIOToken()>0 &&
                  gAudioIO->IsStreamActive(p->GetAudioIOToken()));

   wxRect r;
   int num;

   Track *t = FindTrack(event.m_x, event.m_y, true, true, &r, &num);

   // AS: If the user clicked outside all tracks, make nothing
   //  selected.
   if (!t) {
      SelectNone();
      Refresh(false);
      return;
   }

   bool second = false;
   if (!t->GetLinked() && mTracks->GetLink(t))
      second = true;

   wxRect closeRect;
   mTrackLabel.GetCloseBoxRect(r, closeRect);

   // AS: If they clicked on the x (ie, close button) on this
   //  track, then we capture the mouse and display the x
   //  as depressed.  Somewhere else, when the mouse is released,
   //  we'll see if we're still supposed to close the track.
   if (!second && closeRect.Inside(event.m_x, event.m_y)) {
      wxClientDC dc(this);
      mTrackLabel.DrawCloseBox(&dc, r, true);
      mMouseCapture = IsClosing;
      mCapturedTrack = t;
      mCapturedRect = r;
      return;
   }

   wxRect titleRect;
   mTrackLabel.GetTitleBarRect(r, titleRect);

   // AS: If the clicked on the title area, show the popup menu.
   if (!second && titleRect.Inside(event.m_x, event.m_y)) {
      DoPopupMenu(event, titleRect, t, r, num);
      return;
   }

   // MM: Check minimize buttons on WaveTracks. Must be before
   //     solo/mute buttons, sliders etc.
   if (!second) {
      if (MinimizeFunc(t, r, event.m_x, event.m_y))
         return;
   }

   // DM: Check Mute and Solo buttons on WaveTracks:
   if (!second && t->GetKind() == Track::Wave) {
      if (MuteSoloFunc(t, r, event.m_x, event.m_y, false) ||
          MuteSoloFunc(t, r, event.m_x, event.m_y, true))
         return;
   }
   // DM: Check Gain and Pan on WaveTracks:
   if (!second && t->GetKind() == Track::Wave) {
      if (GainFunc(t, r, event,
                   num-1, event.m_x, event.m_y))
         return;
   }
   // DM: Check Gain and Pan on WaveTracks:
   if (!second && t->GetKind() == Track::Wave) {
      if (PanFunc(t, r, event,
                  num-1, event.m_x, event.m_y))
         return;
   }
      // DM: If it's a NoteTrack, it has special controls
      if (!second && t && t->GetKind() == Track::Note) {
         wxRect midiRect;
         mTrackLabel.GetTrackControlsRect(r, midiRect);
         if (midiRect.Inside(event.m_x, event.m_y)) {
            ((NoteTrack *) t)->LabelClick(midiRect, event.m_x, event.m_y,
                                          event.RightDown()
                                          || event.RightDClick());
            Refresh(false);
            return;
         }
      }
   // DM: If they weren't clicking on a particular part of a track label,
   //  deselect other tracks and select this one.

   // JH: also, capture the current track for rearranging, so the user
   //  can drag the track up or down to swap it with others
   if (!unsafe) {
      SetCapturedTrack( t, IsRearranging );
      TrackPanel::CalculateRearrangingThresholds(event);
   }

   // AS: If the shift botton is being held down, then just invert 
   //  the selection on this track.
   if (event.ShiftDown()) {
      mTracks->Select(t, !t->GetSelected());
      Refresh(false);
      return;
   }

   SelectNone();
   mTracks->Select(t);
   mViewInfo->sel0 = t->GetOffset();
   mViewInfo->sel1 = t->GetEndTime();

   Refresh(false);  
   if (!unsafe)
      MakeParentModifyState();
}

// JH: the user is dragging one of the tracks: change the track order
//   accordingly
void TrackPanel::HandleRearrange(wxMouseEvent & event)
{
   // are we finishing the drag?
   if (event.ButtonUp(1)) {
      SetCapturedTrack( NULL );
      SetCursor(*mArrowCursor);
      return;
   }
   if (event.m_y < mMoveUpThreshold)
      mTracks->MoveUp(mCapturedTrack);
   else if (event.m_y > mMoveDownThreshold)
      mTracks->MoveDown(mCapturedTrack);
   else
      return;

   // JH: if we moved up or down, recalculate the thresholds
   TrackPanel::CalculateRearrangingThresholds(event);
   Refresh(false);
}

// JH: figure out how far the user must drag the mouse up or down
//   before the track will swap with the one above or below
void TrackPanel::CalculateRearrangingThresholds(wxMouseEvent & event)
{
   wxASSERT(mCapturedTrack);

   // JH: this will probably need to be tweaked a bit, I'm just
   //   not sure what formula will have the best feel for the
   //   user.
   if (mTracks->CanMoveUp(mCapturedTrack))
      mMoveUpThreshold =
          event.m_y - mTracks->GetPrev(mCapturedTrack)->GetHeight();
   else
      mMoveUpThreshold = INT_MIN;

   if (mTracks->CanMoveDown(mCapturedTrack))
      mMoveDownThreshold =
          event.m_y + mTracks->GetNext(mCapturedTrack)->GetHeight();
   else
      mMoveDownThreshold = INT_MAX;
}

bool TrackPanel::GainFunc(Track * t, wxRect r, wxMouseEvent &event,
                          int index, int x, int y)
{
   wxRect sliderRect;
   mTrackLabel.GetGainRect(r, sliderRect);
   if (!sliderRect.Inside(x, y)) 
      return false;

   SetCapturedTrack( t, IsGainSliding);
   mCapturedRect = r;
   mCapturedNum = index;
   HandleSliders(event, false);

   return true;
}

bool TrackPanel::PanFunc(Track * t, wxRect r, wxMouseEvent &event,
                         int index, int x, int y)
{
   wxRect sliderRect;
   mTrackLabel.GetPanRect(r, sliderRect);
   if (!sliderRect.Inside(x, y))
      return false;

   SetCapturedTrack( t, IsPanSliding);
   mCapturedRect = r;
   mCapturedNum = index;
   HandleSliders(event, true);

   return true;
}

// AS: Mute or solo the given track (t).  If solo is true, we're 
//  soloing, otherwise we're muting.  Basically, check and see 
//  whether x and y fall within the  area of the appropriate button.
bool TrackPanel::MuteSoloFunc(Track * t, wxRect r, int x, int y,
                              bool solo)
{
   wxRect buttonRect;
   mTrackLabel.GetMuteSoloRect(r, buttonRect, solo);
   if (!buttonRect.Inside(x, y)) 
      return false;

   wxClientDC dc(this);
   SetCapturedTrack( t, solo ? IsSoloing : IsMuting);
   mCapturedRect = r;

   mTrackLabel.DrawMuteSolo(&dc, r, t, true, solo);
   return true;
}

bool TrackPanel::MinimizeFunc(Track * t, wxRect r, int x, int y)
{
   wxRect buttonRect;
   mTrackLabel.GetMinimizeRect(r, buttonRect, t->GetMinimized());
   if (!buttonRect.Inside(x, y)) 
      return false;

   wxClientDC dc(this);
   SetCapturedTrack( t, IsMinimizing );
   mCapturedRect = r;

   mTrackLabel.DrawMinimize(&dc, r, t, true, t->GetMinimized());
   return true;
}

/// DM: ButtonDown means they just clicked and haven't released yet.
///  We use this opportunity to save which track they clicked on,
///  and the initial height of the track, so as they drag we can
///  update the track size.
void TrackPanel::HandleResizeClick( wxMouseEvent & event )
{
   wxRect r;
   wxRect rLabel;
   int num;

   // DM: Figure out what track is about to be resized
   Track *t = FindTrack(event.m_x, event.m_y, false, false, &r, &num);
   Track *label = FindTrack(event.m_x, event.m_y, true, true, &rLabel, &num);

   if (t && t->GetMinimized())
   {
      // Jump out of minimized mode when resizing.
      // Initial height will be height as a minimized track.
      t->SetHeight( t->GetHeight());
      t->SetMinimized(false);
   }

   // If the click is at the bottom of a non-linked track label, we
   // treat it as a normal resize.  If the label is of a linked track,
   // we ignore the click.

   if (label && !label->GetLinked())
   {
      t = label;
   }

   if (!t) 
      return;

   Track *prev = mTracks->GetPrev(t);
   Track *next = mTracks->GetNext(t);

   // DM: Capture the track so that we continue to resize
   //  THIS track, no matter where the user moves the mouse
   SetCapturedTrack( t, IsResizing );
   //mCapturedRect = r;
   //mCapturedNum = num;

   mMouseClickX = event.m_x;
   mMouseClickY = event.m_y;

   //STM:  Determine whether we should rescale one or two tracks

   if (mTracks->GetLink(prev) == t) {
      if(prev->GetMinimized())
      {
         prev->SetHeight( prev->GetHeight());
         prev->SetMinimized(false);
      }
      // mCapturedTrack is the lower track
      mInitialTrackHeight = t->GetHeight();
      mInitialUpperTrackHeight = prev->GetHeight();
      SetCapturedTrack( t, IsResizingBelowLinkedTracks );
   } else if (next && mTracks->GetLink(t) == next) {
      // mCapturedTrack is the upper track
      mInitialTrackHeight = next->GetHeight();
      mInitialUpperTrackHeight = t->GetHeight();
      SetCapturedTrack( t, IsResizingBetweenLinkedTracks );
   } else {
      // DM: Save the initial mouse location and the initial height
      mInitialTrackHeight = t->GetHeight();
   }
}

/// DM: This happens when the button is released from a drag.
///  Since we actually took care of resizing the track when
///  we got drag events, all we have to do here is clean up.
///  We also modify the undo state (the action doesn't become
///  undo-able, but it gets merged with the previous undo-able
///  event).
void TrackPanel::HandleResizeButtonUp(wxMouseEvent & event)
{
   SetCapturedTrack( NULL );
   MakeParentRedrawScrollbars();
   MakeParentModifyState();
}

// DM: Resize dragging means that the mouse button IS down and has moved
//  from its initial location.  By the time we get here, we
//  have already received a ButtonDown() event and saved the
//  track being resized in mCapturedTrack.
void TrackPanel::HandleResizeDrag(wxMouseEvent & event)
{
   int delta = (event.m_y - mMouseClickY);

   //STM: We may be dragging one or two (stereo) tracks.  
   // If two, resize proportionally if we are dragging the lower track, and
   // adjust compensatively if we are dragging the upper track.
   switch( mMouseCapture )
   {
   case IsResizingBelowLinkedTracks:
      {
         Track *prev = mTracks->GetPrev(mCapturedTrack);

         double proportion = static_cast < double >(mInitialTrackHeight)
             / (mInitialTrackHeight + mInitialUpperTrackHeight);

         int newTrackHeight = static_cast < int >
             (mInitialTrackHeight + delta * proportion);

         int newUpperTrackHeight = static_cast < int >
             (mInitialUpperTrackHeight + delta * (1.0 - proportion));

         //make sure neither track is smaller than its minimum height
         if (newTrackHeight < mCapturedTrack->GetMinimizedHeight())
            newTrackHeight = mCapturedTrack->GetMinimizedHeight();
         if (newUpperTrackHeight < prev->GetMinimizedHeight())
            newUpperTrackHeight = prev->GetMinimizedHeight();

         mCapturedTrack->SetHeight(newTrackHeight);
         prev->SetHeight(newUpperTrackHeight);
         break;
      }
   case IsResizingBetweenLinkedTracks:
      {
         Track *next = mTracks->GetNext(mCapturedTrack);
         int newUpperTrackHeight = mInitialUpperTrackHeight + delta;
         int newTrackHeight = mInitialTrackHeight - delta;

         // make sure neither track is smaller than its minimum height
         if (newTrackHeight < next->GetMinimizedHeight()) {
            newTrackHeight = next->GetMinimizedHeight();
            newUpperTrackHeight =
                mInitialUpperTrackHeight + mInitialTrackHeight - next->GetMinimizedHeight();
         }
         if (newUpperTrackHeight < mCapturedTrack->GetMinimizedHeight()) {
            newUpperTrackHeight = mCapturedTrack->GetMinimizedHeight();
            newTrackHeight =
                mInitialUpperTrackHeight + mInitialTrackHeight - mCapturedTrack->GetMinimizedHeight();
         }

         mCapturedTrack->SetHeight(newUpperTrackHeight);
         next->SetHeight(newTrackHeight);
         break;
      }
   case IsResizing:
      {
         int newTrackHeight = mInitialTrackHeight + delta;
         if (newTrackHeight < mCapturedTrack->GetMinimizedHeight())
            newTrackHeight = mCapturedTrack->GetMinimizedHeight();
         mCapturedTrack->SetHeight(newTrackHeight);
         break;
      }
   default:
      // don't refresh in this case.
      return;
   }
   
   Refresh(false);
}

/// DM: HandleResize gets called when:
///  1. A mouse-down event occurs in the "resize region" of a track,
///     i.e. to change its vertical height.
///  2. A mouse event occurs and mIsResizing==true (i.e. while
///     the resize is going on)
void TrackPanel::HandleResize(wxMouseEvent & event)
{
   if (event.ButtonDown(1)) {
      HandleResizeClick( event );
   } 
   else if (event.ButtonUp(1)) 
   {
      HandleResizeButtonUp( event );
   }
   else if (event.Dragging()) {
      HandleResizeDrag( event );
   }
}

// MM: Handle mouse wheel rotation (for zoom in/out and vertical scrolling)
void TrackPanel::HandleWheelRotation(wxMouseEvent & event)
{
   int steps = event.m_wheelRotation /
      (event.m_wheelDelta > 0 ? event.m_wheelDelta : 120);

   if (event.ShiftDown())
   {
      // MM: Scroll left/right when used with Shift key down
      mListener->TP_ScrollWindow(
         mViewInfo->h +
         50.0 * -steps / mViewInfo->zoom);
   } else if (event.ControlDown())
   {
      // MM: Zoom in/out when used with Control key down
      // MM: I don't understand what trackLeftEdge does
      int trackLeftEdge = GetLabelWidth()+1;
      
      double center_h = PositionToTime(event.m_x, trackLeftEdge);
      if (steps < 0)
         mViewInfo->zoom = wxMax(mViewInfo->zoom / (2.0 * -steps), gMinZoom);
      else
         mViewInfo->zoom = wxMin(mViewInfo->zoom * (2.0 * steps), gMaxZoom);

      double new_center_h = PositionToTime(event.m_x, trackLeftEdge);
      mViewInfo->h += (center_h - new_center_h);
      
      MakeParentRedrawScrollbars();
      Refresh(false);
   } else
   {
      // MM: Zoom up/down when used without modifier keys
      mListener->TP_ScrollUpDown(-steps * 4);
   }
}

// AS: Handle key presses by the user.  Notably, play and stop when the
//  user presses the spacebar.  Also, LabelTracks can be typed into.
void TrackPanel::OnKeyEvent(wxKeyEvent & event)
{
   if (event.ControlDown() || event.AltDown()) {
      event.Skip();
      return;
   }

   TrackListIterator iter(mTracks);

   // Send keyboard input to the first selected LabelTrack
   // There currently isn't a way to efficiently work with
   // more than one LabelTrack
   for (Track * t = iter.First(); t; t = iter.Next()) {
      if (t->GetKind() == Track::Label && t->GetSelected() && ((LabelTrack*)t)->getKeyOn()) {
         ((LabelTrack *) t)->KeyEvent((mViewInfo->sel0), (mViewInfo->sel1),
                                      event);
         
         Refresh(false);
         MakeParentPushState(_("Modified Label"),
                             _("Label Edit"),
                             true /* consolidate */);
         event.Skip();
         return;
      }
   }
   
   switch (event.GetKeyCode()) {
   case WXK_DELETE:
   case WXK_BACK:
      GetProject()->Clear();
      break;
   case WXK_SPACE:
      mListener->TP_OnPlayKey();
      break;
   case WXK_PRIOR:
      // BG: Page right
		// Up and down are reversed from pre 1.3.0 Audacity versions.
//lda      mListener->TP_ScrollWindow((mViewInfo->h + mViewInfo->screen) - (mViewInfo->screen / 6));
      mListener->TP_ScrollWindow((mViewInfo->h - mViewInfo->screen) + (mViewInfo->screen / 6));
      break;
   case WXK_NEXT:
      // BG: Page left
//lda      mListener->TP_ScrollWindow((mViewInfo->h - mViewInfo->screen) + (mViewInfo->screen / 6));
      mListener->TP_ScrollWindow((mViewInfo->h + mViewInfo->screen) - (mViewInfo->screen / 6));
      break;
   case WXK_RIGHT:
      // BG: Scroll right
      mListener->TP_ScrollWindow((mViewInfo->h + mViewInfo->screen) -
                                 (mViewInfo->screen * .95));
      break;
   case WXK_LEFT:
      // BG: Scroll left
      mListener->TP_ScrollWindow((mViewInfo->h - mViewInfo->screen) +
                                 (mViewInfo->screen * .95));
      break;
   case WXK_HOME:
      // BG: Skip to beginning
      // JS: Changed so that the selection is not changed
      //     without modifiers
      if (event.ShiftDown()) {
         // move first selection edge to start
         // direction of the selection is preserved
         // if sel0 == sel1, sel0 comes first
         if (mViewInfo->sel0 <= mViewInfo->sel1) {
            mViewInfo->sel0 = mTracks->GetStartTime();
         } else {
            mViewInfo->sel1 = mTracks->GetStartTime();
         }
      }
      mListener->TP_ScrollWindow(mTracks->GetStartTime());
      break;
   case WXK_END:
      // BG: Skip to end
      // JS: Changed so that the selection is not changed
      //     without modifiers
      if (event.ShiftDown()) {
         // move last selection edge to end
         // direction of the selection is preserved
         // if sel0 == sel1, sel0 comes first
         if (mViewInfo->sel0 <= mViewInfo->sel1) {
            mViewInfo->sel1 = mTracks->GetEndTime();
         } else {
            mViewInfo->sel0 = mTracks->GetEndTime();
         }
      }
      mListener->TP_ScrollWindow(mTracks->GetEndTime());
      break;
   case WXK_TAB:
      mListener->TP_GiveFocus(!event.ShiftDown());
      break;
   }
   event.Skip();
}

// AS: This handles just generic mouse events.  Then, based
//  on our current state, we forward the mouse events to
//  various interested parties.
void TrackPanel::OnMouseEvent(wxMouseEvent & event)
{
   mListener->TP_HasMouse();
   
   if (event.m_wheelRotation != 0)
      HandleWheelRotation(event);
   
   if (!mAutoScrolling) {
      mMouseMostRecentX = event.m_x;
      mMouseMostRecentY = event.m_y;
   }

   if (event.ButtonDown(1)) {
      mCapturedTrack = NULL;
      
      // The activate event is used to make the 
      // parent window 'come alive' if it didn't have focus.
      wxActivateEvent e;
      GetParent()->ProcessEvent(e);

      // wxTimers seem to be a little unreliable, so this
      // "primes" it to make sure it keeps going for a while...

      // When this timer fires, we call TrackPanel::OnTimer and
      // possibly update the screen for offscreen scrolling.
      mTimer.Stop();
      mTimer.Start(50, FALSE);
   }

   if (event.ButtonDown()) {
      SetFocus();
      CaptureMouse();
   }
   else if (event.ButtonUp()) {
      if (HasCapture())
         ReleaseMouse();
   }

   switch( mMouseCapture )
   {
   case IsVZooming: 
      HandleVZoom(event);
      break;
   case IsClosing:
      HandleClosing(event);
      mMouseCapture = IsUncaptured; // JKC: Do we need this??
      break;
   case IsMuting:
      HandleMutingSoloing(event, false);
      break;
   case IsSoloing:
      HandleMutingSoloing(event, true);
      break;
   case IsResizing: 
   case IsResizingBetweenLinkedTracks:
   case IsResizingBelowLinkedTracks:
      HandleResize(event);
      HandleCursor(event);
      break;
   case IsRearranging:
      HandleRearrange(event);
      break;
   case IsGainSliding:
      HandleSliders(event, false);
      break;
   case IsPanSliding:
      HandleSliders(event, true);
      break;
   case IsMinimizing:
      HandleMinimizing(event);
      break;
   default: //includes case of IsUncaptured
      HandleTrackSpecificMouseEvent(event);
      break;
   }

   //EnsureVisible should be called after the up-click.
   if (event.ButtonUp()) {
      wxRect r;
      int num;

      Track *t = FindTrack(event.m_x, event.m_y, false, false, &r, &num);
      if (t)
         EnsureVisible(t);
   }


}

bool TrackPanel::HandleTrackLocationMouseEvent(WaveTrack * track, wxRect &r, wxMouseEvent &event)
{
   // FIX-ME: Disable this and return true when CutLines aren't showing?
   // (Don't use gPerfs-> for the fix as registry access is slow).

   if (mMouseCapture == WasOverCutLine)
   {
      // Needed to avoid select events right after IsOverCutLine event on button up
      if (event.ButtonUp()) {
         mMouseCapture = IsUncaptured;
         return false;
      }
      return true;
   }
   
   if (mMouseCapture == IsOverCutLine)
   {
      if (!mCapturedTrackLocationRect.Inside(event.m_x, event.m_y))
      {
         SetCapturedTrack( NULL );
         SetCursorByActivity();
         return false;
      }
      
      bool handled = false;

      if (event.LeftDown())
      {
         if (mCapturedTrackLocation.typ == WaveTrack::locationCutLine)
         {
            // When user presses left button on cut line, expand the line again
            double cutlineStart = 0, cutlineEnd = 0;
            if (track->ExpandCutLine(mCapturedTrackLocation.pos, &cutlineStart, &cutlineEnd))
            {
               WaveTrack* linked = (WaveTrack*)mTracks->GetLink(track);
               if (linked)
                  linked->ExpandCutLine(mCapturedTrackLocation.pos);
               mViewInfo->sel0 = cutlineStart;
               mViewInfo->sel1 = cutlineEnd;
               DisplaySelection();
               MakeParentPushState(_("Expanded Cut Line"), _("Expand"), false );
               handled = true;
            }
         } else if (mCapturedTrackLocation.typ == WaveTrack::locationMergePoint)
         {
            track->MergeClips(mCapturedTrackLocation.clipidx1, mCapturedTrackLocation.clipidx2);
            WaveTrack* linked = (WaveTrack*)mTracks->GetLink(track);
            if (linked)
               linked->MergeClips(mCapturedTrackLocation.clipidx1, mCapturedTrackLocation.clipidx2);
            handled = true;
         }
         Refresh(false);
      }

      if (!handled && event.RightDown())
      {
         track->RemoveCutLine(mCapturedTrackLocation.pos);
         WaveTrack* linked = (WaveTrack*)mTracks->GetLink(track);
         if (linked)
            linked->RemoveCutLine(mCapturedTrackLocation.pos);
         MakeParentPushState(_("Removed Cut Line"), _("Remove"), false );
         handled = true;
      }
      
      if (handled)
      {
         SetCapturedTrack( NULL );
         mMouseCapture = WasOverCutLine; 
         Refresh(false);
         return true;
      }

      return true;
   }

   for (int i=0; i<track->GetNumCachedLocations(); i++)
   {
      WaveTrack::Location loc = track->GetCachedLocation(i);
      
      double x = (loc.pos - mViewInfo->h) * mViewInfo->zoom;
      if (x >= 0 && x < r.width)
      {
         wxRect locRect;
         locRect.x = int( r.x + x ) - 5;
         locRect.width = 11;
         locRect.y = r.y;
         locRect.height = r.height;
         if (locRect.Inside(event.m_x, event.m_y))
         {
            SetCapturedTrack(track, IsOverCutLine);
            mCapturedTrackLocation = loc;
            mCapturedTrackLocationRect = locRect;
            SetCursorByActivity();
            return true;
         }
      }
   }

   return false;
}

/// Event has happened on a track and it has been determined to be a label track.
/// TODO: This fucntion is one of a large number of functions in 
/// TrackPanel which suitably modified belong in other classes.
bool TrackPanel::HandleLabelTrackMouseEvent(LabelTrack * lTrack, wxRect &r, wxMouseEvent & event)
{
   if(event.ButtonDown(1))      
   {
      lTrack->SetKeyOn(true);

      TrackListIterator iter(mTracks);
      Track *n = iter.First();
 
      while (n) {
         if (n->GetKind() == Track::Label && lTrack != n) {
            ((LabelTrack *)n)->ResetFlags();
         }
         n = iter.Next();
      }

      //If the button was pressed, check to see if we are over
      //a glyph (this is the second of three calls to the function).
      //std::cout << ((LabelTrack*)pTrack)->OverGlyph(event.m_x, event.m_y) << std::endl;
      if(lTrack->OverGlyph(event.m_x, event.m_y))
      {
         SetCapturedTrack(lTrack, IsAdjustingLabel);
      }
   } else if (event.Dragging()) {
      ;
   } else if( event.ButtonUp(1)) {
      SetCapturedTrack( NULL );
   }
   
   lTrack->HandleMouse(event, r,//mCapturedRect,
      mViewInfo->h, mViewInfo->zoom, &mViewInfo->sel0, &mViewInfo->sel1);
   
   if (event.ButtonDown(3)) {
      // popup menu for editing
      Refresh(false);
     
      if ((lTrack->getSelectedIndex() != -1) && lTrack->OverTextBox(lTrack->GetLabel(lTrack->getSelectedIndex()), event.m_x, event.m_y)) {
         mPopupMenuTarget = lTrack;
         mLabelTrackLabelMenu->Enable(OnCutSelectedTextID, lTrack->IsTextSelected());
         mLabelTrackLabelMenu->Enable(OnCopySelectedTextID, lTrack->IsTextSelected());
         mLabelTrackLabelMenu->Enable(OnPasteSelectedTextID, lTrack->IsTextClipSupported());
         PopupMenu(mLabelTrackLabelMenu, event.m_x + 1, event.m_y + 1);
         // it's an invalid dragging event
         lTrack->SetWrongDragging(true);
      }
      return true;
   }
   
   //If we are adjusting a label on a labeltrack, do not do anything 
   //that follows. Instead, redraw the track.
   if(mMouseCapture == IsAdjustingLabel)
   {
      Refresh(false);
      return true;
   }

   // handle dragging
   if(event.Dragging()) {
      // locate the initial mouse position
      if (event.LeftIsDown()) {
         if (mLabelTrackStartXPos == -1) {
            mLabelTrackStartXPos = event.m_x;
            mLabelTrackStartYPos = event.m_y;

            if ((lTrack->getSelectedIndex() != -1) && 
               lTrack->OverTextBox(
                  lTrack->GetLabel(lTrack->getSelectedIndex()), 
                  mLabelTrackStartXPos,
                  mLabelTrackStartYPos)) 
            {
               mLabelTrackStartYPos = -1;
            }
         }
         // if initial mouse position in the text box
         // then only drag text
         if (mLabelTrackStartYPos == -1) {
            Refresh(false);
            return true;
         }
      }
   }
  
   // handle mouse left button up
   if (event.LeftUp()) {
      mLabelTrackStartXPos = -1;
   }

   // handle shift+ctrl down
   /*if (event.ShiftDown()) { // && event.ControlDown()) {
      lTrack->SetHighlightedByKey(true);
      Refresh(false);
      return;
   }*/

   // handle shift+mouse left button
   if (event.ShiftDown() && event.ButtonDown() && (lTrack->getSelectedIndex() != -1)) {
      // if the mouse is clicked in text box, set flags
      if (lTrack->OverTextBox(lTrack->GetLabel(lTrack->getSelectedIndex()), event.m_x, event.m_y)) {
         lTrack->SetInBox(true);
         lTrack->SetDragXPos(event.m_x);
         lTrack->SetResetCursorPos(true);
         Refresh(false);
         return true;
      }
   }
   // return false, there is more to do...
   return false;
}

/// AS: I don't really understand why this code is sectioned off
///  from the other OnMouseEvent code.
void TrackPanel::HandleTrackSpecificMouseEvent(wxMouseEvent & event)
{
   wxRect r;
   wxRect rLabel;
   int dummy;

   AudacityProject *p = GetProject();
   bool unsafe = (p->GetAudioIOToken()>0 &&
                  gAudioIO->IsStreamActive(p->GetAudioIOToken()));

   FindTrack(event.m_x, event.m_y, false, false, &r, &dummy);
   FindTrack(event.m_x, event.m_y, true, true, &rLabel, &dummy);

   //call HandleResize if I'm over the border area 
   if (event.ButtonDown(1) &&
       (within(event.m_y, r.y + r.height, TRACK_RESIZE_REGION)
        || within(event.m_y, rLabel.y + rLabel.height,
                  TRACK_RESIZE_REGION))) {
      HandleResize(event);
      HandleCursor(event);
      return;
   }

   //Determine if user clicked on the track's left-hand label
   if (!mCapturedTrack && event.m_x < GetLabelWidth()) {
      if (event.m_x >= GetVRulerOffset()) {
         HandleVZoom(event);
         HandleCursor(event);
      }
      else {
         HandleLabelClick(event);
         HandleCursor(event);
      }
      return;
   }

   //Determine if user clicked on a label track.
   //If so, use MouseDown handler for the label track.
   int num;
   Track *pTrack = FindTrack(event.m_x, event.m_y, false, false, &r, &num);
   
   if (pTrack && (pTrack->GetKind() == Track::Label)) 
   {
      if(HandleLabelTrackMouseEvent( (LabelTrack *) pTrack, r, event ))
         return;
   }

   if (pTrack && (pTrack->GetKind() == Track::Wave) && 
       (mMouseCapture == IsUncaptured || mMouseCapture == IsOverCutLine ||
        mMouseCapture == WasOverCutLine))
   {
      if (HandleTrackLocationMouseEvent( (WaveTrack *) pTrack, r, event ))
         return;
   }

   ControlToolBar * pCtb = mListener->TP_GetControlToolBar();
   if( pCtb == NULL )
      return;

   int toolToUse = DetermineToolToUse(pCtb, event);

   switch (toolToUse) {
   case selectTool:
      HandleSelect(event);
      break;
   case envelopeTool:
      if (!unsafe)
         HandleEnvelope(event);
      break;
   case slideTool:
      if (!unsafe)
         HandleSlide(event);
      break;
   case zoomTool:
      HandleZoom(event);
      break;
   case drawTool:
      if (!unsafe)
         HandleSampleEditing(event);
      break;
   }

   if ((event.Moving() || event.ButtonUp(1))  &&
       (mMouseCapture == IsUncaptured ))
//       (mMouseCapture != IsSelecting ) && 
//       (mMouseCapture != IsEnveloping) &&
//       (mMouseCapture != IsSliding) )
   {
      HandleCursor(event);
   }
   if (event.ButtonUp(1)) {
      mCapturedTrack = NULL;
   }
}

/// If we are in multimode, looks at the type of track and where we are on it to 
/// determine what object we are hovering over and hence what tool to use.
/// @param pCtb - A pointer to the control tool bar
/// @param event - Mouse event, with info about position and what mouse buttons are down.
int TrackPanel::DetermineToolToUse( ControlToolBar * pCtb, wxMouseEvent & event)
{
   int currentTool = pCtb->GetCurrentTool();

   // Unless in Multimode keep using the current tool.
   if( !pCtb->GetMultiToolDown() )
      return currentTool;

   // We NEVER change tools whilst we are dragging.
   if( event.Dragging() )
      return currentTool;

   // Just like dragging.
   // But, this event might be the final button up
   // so keep the same tool.
//   if( mIsSliding || mIsSelecting || mIsEnveloping )
   if( mMouseCapture != IsUncaptured )
      return currentTool;

   // So now we have to find out what we are near to..
   wxRect r;
   int num;

   Track *pTrack = FindTrack(event.m_x, event.m_y, false, false, &r, &num);
   if( !pTrack )
      return currentTool;

   int trackKind = pTrack->GetKind();
   currentTool = selectTool; // the default.

   if( event.ButtonIsDown(3) || event.ButtonUp(3)){
      currentTool = zoomTool;
   } else if( trackKind == Track::Time ){
      currentTool = envelopeTool;
   } else if( trackKind == Track::Label ){
      currentTool = selectTool;
   } else if( trackKind != Track::Wave) {
      currentTool = selectTool;
   // So we are in a wave track.
   // From here on the order in which we hit test determines 
   // which tool takes priority in the rare cases where it
   // could be more than one.
   } else if( HitTestEnvelope( pTrack, r, event ) ){
      currentTool = envelopeTool;
   } else if( HitTestSlide( pTrack, r, event )){
      currentTool = slideTool;
   } else if( HitTestSamples( pTrack, r, event )){
      currentTool = drawTool;
   }

   //Use the false argument since in multimode we don't 
   //want the button indicating which tool is in use to be updated.
   pCtb->SetCurrentTool( currentTool, false );
   return currentTool;
}

/// Function that tells us if the mouse event landed on a 
/// envelope boundary.
bool TrackPanel::HitTestEnvelope(Track *track, wxRect &r, wxMouseEvent & event)
{
   WaveTrack *wavetrack = (WaveTrack *)track;
   Envelope *envelope = wavetrack->GetEnvelopeAtX(event.GetX());

   if (!envelope)
      return false;   

   int displayType = wavetrack->GetDisplay();
   // Not an envelope hit, unless we're using a type of wavetrack display 
   // suitable for envelopes operations, ie one of the Wave displays.
   if ( displayType > 1) 
      return false;  // No envelope, not a hit, so return.

   // Get envelope point, range 0.0 to 1.0
   bool dB = (displayType == 1);
   double envValue = envelope->GetValueAtX( 
      event.m_x, r, mViewInfo->h, mViewInfo->zoom );

   float zoomMin, zoomMax;
   wavetrack->GetDisplayBounds(&zoomMin, &zoomMax);

   // Get y position of envelope point.
   float dBr = gPrefs->Read(wxT("/GUI/EnvdBRange"), ENV_DB_RANGE);
   int yValue = GetWaveYPosNew( envValue,
      zoomMin, zoomMax,      
      r.height, dB, true, dBr, false ) + r.y;

   // Get y position of center line
   int ctr = GetWaveYPosNew( 0.0,
      zoomMin, zoomMax,      
      r.height, dB, true, dBr, false ) + r.y;
  
   // Get y distance of mouse from center line (in pixels).
   int yMouse = abs(ctr - event.m_y);
   // Get y distance of envelope from center line (in pixels)
   yValue = abs(ctr-yValue);
   
   // JKC: It happens that the envelope is actually drawn offset from its 
   // 'true' position (it is 3 pixels wide).  yMisalign is really a fudge
   // factor to allow us to hit it exactly, but I wouldn't dream of 
   // calling it yFudgeFactor :)
   const int yMisalign = 2; 
   // Perhaps yTolerance should be put into preferences?
   const int yTolerance = 5; // how far from envelope we may be and count as a hit.
   int distance;

   // For amplification using the envelope we introduced the idea of contours.
   // The contours have the same shape as the envelope, which may be partially off-screen.
   // The contours are closer in to the center line.
   int ContourSpacing = (int) (r.height / (2* (zoomMax-zoomMin)));
   const int MaxContours = 2;

   // Adding ContourSpacing/2 selects a region either side of the contour.
   int yDisplace = yValue - yMisalign - yMouse  + ContourSpacing/2;
   if (yDisplace > (MaxContours * ContourSpacing))
      return false;
   // Subtracting the ContourSpacing/2 we added earlier ensures distance is centred on the contour.
   distance = abs( ( yDisplace % ContourSpacing ) - ContourSpacing/2);
   return( distance < yTolerance );
}

/// Function that tells us if the mouse event landed on an 
/// editable sample
bool TrackPanel::HitTestSamples(Track *track, wxRect &r, wxMouseEvent & event)
{
   WaveTrack *wavetrack = (WaveTrack *)track;
   //Get rate in order to calculate the critical zoom threshold
   double rate = wavetrack->GetRate();

   //Find out the zoom level
   bool showPoints = (mViewInfo->zoom / rate > 3.0);
   if( !showPoints )
      return false;

   int displayType = wavetrack->GetDisplay();
   if ( displayType > 1) 
      return false;  // Not a wave, so return.

   float oneSample;
   double pps = mViewInfo->zoom;
   double tt = (event.m_x - r.x) / pps + mViewInfo->h;
   int    s0 = (int)(tt * rate + 0.5);

   // Just get one sample.
   wavetrack->Get((samplePtr)&oneSample, floatSample, s0, 1);
   
   // Get y distance of envelope point from center line (in pixels).
   bool dB = (displayType == 1);
   float zoomMin, zoomMax;

   wavetrack->GetDisplayBounds(&zoomMin, &zoomMax);
   float dBr = gPrefs->Read(wxT("/GUI/EnvdBRange"), ENV_DB_RANGE);
   
   double envValue = 1.0;
   Envelope* env = wavetrack->GetEnvelopeAtX(event.GetX());
   if (env)
      envValue = env->GetValue(tt);

   int yValue = GetWaveYPosNew( oneSample * envValue, 
      zoomMin, zoomMax,      
      r.height, dB, true, dBr, false) + r.y;   

   // Get y position of mouse (in pixels)
   int yMouse = event.m_y;

   // Perhaps yTolerance should be put into preferences?
   const int yTolerance = 10; // More tolerance on samples than on envelope.
   return( abs( yValue -  yMouse ) < yTolerance );   
}

/// Function that tells us if the mouse event landed on a 
/// time-slider that allows us to time shift the sequence.
bool TrackPanel::HitTestSlide(Track *track, wxRect &r, wxMouseEvent & event)
{
   // Perhaps we should delegate this to TrackArtist as only TrackArtist
   // knows what the real sizes are??

   // The drag Handle width includes border, width and a little extra margin.
   const int adjustedDragHandleWidth = 14;
   // The hotspot for the cursor isn't at its centre.  Adjust for this. 
   const int hotspotOffset = 5;

   // We are doing an approximate test here - is the mouse in the right or left border?
   if( event.m_x + hotspotOffset < r.x + adjustedDragHandleWidth)
      return true;

   if( event.m_x + hotspotOffset > r.x + r.width - adjustedDragHandleWidth)
      return true;

   return false;
}

/// This draws the play indicator as a vertical line on each of the tracks
void TrackPanel::DrawTrackIndicator(wxDC * dc)
{
   // Draw indicator
   double ind = gAudioIO->GetStreamTime();
   int indp = 0;
   wxMemoryDC tmpDrawDC;

   //Erase the indicator
   while(!mScreenAtIndicator.IsEmpty())
   {
      tmpDrawDC.SelectObject(*mScreenAtIndicator[0]->bitmap);
      dc->Blit(mScreenAtIndicator[0]->x, mScreenAtIndicator[0]->y, mScreenAtIndicator[0]->bitmap->GetWidth(), mScreenAtIndicator[0]->bitmap->GetHeight(), &tmpDrawDC, 0, 0);
      tmpDrawDC.SelectObject(wxNullBitmap);
      delete mScreenAtIndicator[0]->bitmap;
      tpBitmap *tmpDeletedTpBitmap = mScreenAtIndicator[0];
      mScreenAtIndicator.RemoveAt(0);
      delete tmpDeletedTpBitmap;
   }
   mScreenAtIndicator.Clear();

   if (ind >= mViewInfo->h && ind <= (mViewInfo->h + mViewInfo->screen)) {
      indp = GetLeftOffset() + int ((ind - mViewInfo->h) * mViewInfo->zoom);

      AColor::IndicatorColor(dc, (gAudioIO->GetNumCaptureChannels() ? false : true));
         
      //Get the size of the trackpanel region, so we know where to redraw
      int width, height;
      GetSize(&width, &height);   

      int x = indp;
      int y = -mViewInfo->vpos + GetRulerHeight();

      if (x >= GetLeftOffset())
      {
         // Draw cursor in all selected tracks
         TrackListIterator iter(mTracks);

         // Iterate through each track
         for (Track * t = iter.First(); t; t = iter.Next())
         {
            int height = t->GetHeight();
            if ( t->GetKind() != Track::Label)
            {
               wxCoord top = y + kTopInset + 1;
               wxCoord bottom = y + height - 2;

               //Save bitmaps of the areas that we are going to overwrite
               wxBitmap *tmpBitmap = new wxBitmap(1, bottom-top);
               tmpDrawDC.SelectObject(*tmpBitmap);

               //Copy the part of the screen into the bitmap, using the memory DC
               tmpDrawDC.Blit(0, 0, tmpBitmap->GetWidth(), tmpBitmap->GetHeight(), dc, x, top);
               tmpDrawDC.SelectObject(wxNullBitmap);

               //Add the bitmap to the array
               tpBitmap *tmpTpBitmap = new tpBitmap;
               tmpTpBitmap->x = x;
               tmpTpBitmap->y = top;
               tmpTpBitmap->bitmap = tmpBitmap;
               mScreenAtIndicator.Add(tmpTpBitmap);

               //Draw the new indicator in its correct location
               dc->DrawLine(x, top, x, bottom);
            }
            //Increment y so you draw on the proper track
            y += height;
         }
      }
   }
}

double TrackPanel::GetMostRecentXPos()
{
   return mViewInfo->h +
      (mMouseMostRecentX - GetLabelWidth()) / mViewInfo->zoom;
}

int TrackPanel::GetRulerHeight()
{ 
   return AdornedRulerPanel::GetRulerHeight();
}

/// This function overrides Refresh() of wxWindow so that the 
/// boolean play indicator can be set to false, so that an old play indicator that is
/// no longer there won't get  XORed (to erase it), thus redrawing it on the 
/// TrackPanel
void TrackPanel::Refresh(bool eraseBackground /* = TRUE */,
                         const wxRect *rect /* = NULL */)
{
   mPlayIndicatorExists = false;
   wxWindow::Refresh(eraseBackground, rect);
   DisplaySelection();
}

/// AS: Draw the actual track areas.  We only draw the borders
///  and the little buttons and menues and whatnot here, the
///  actual contents of each track are drawn by the TrackArtist.
void TrackPanel::DrawTracks(wxDC * dc)
{
   wxRect clip;
   GetSize(&clip.width, &clip.height);

   wxRect panelRect = clip;
   panelRect.y = -mViewInfo->vpos;

   // Make room for ruler
   panelRect.y += GetRulerHeight();
   panelRect.height -= GetRulerHeight();

   wxRect tracksRect = panelRect;
   tracksRect.x += GetLabelWidth();
   tracksRect.width -= GetLabelWidth();

   ControlToolBar *pCtb = mListener->TP_GetControlToolBar();
   //No control tool bar?  All bets are off.  Could happen as the application closes.
   if( pCtb == NULL )
      return;

   bool bMultiToolDown = pCtb->GetMultiToolDown();
   bool envelopeFlag   = pCtb->GetEnvelopeToolDown() || bMultiToolDown;
   bool samplesFlag    = pCtb->GetDrawToolDown() || bMultiToolDown;
   bool sliderFlag     = bMultiToolDown;

   // The track artist actually draws the stuff inside each track
   mTrackArtist->DrawTracks(mTracks, *dc, tracksRect,
                            clip, mViewInfo, 
                            envelopeFlag, samplesFlag, sliderFlag);

   DrawEverythingElse(dc, panelRect, clip);
}


/// Draws 'Everything else'.  In particular it draws:
///  1) Drop shadow for tracks and vertical rulers.
///  2) Zooming Indicators.
///  3) Fills in space below the tracks. 
void TrackPanel::DrawEverythingElse(wxDC * dc, const wxRect panelRect,
                                    const wxRect clip)
{
   // We draw everything else
   TrackListIterator iter(mTracks);

   wxRect trackRect = panelRect;
   wxRect r;

   int i = 0;
   for (Track * t = iter.First(); t; t = iter.Next()) {
      DrawEverythingElse(t, dc, r, trackRect, i);
      i++;
   }

   if (IsDragZooming()  && (mMouseCapture != IsAdjustingLabel))
      DrawZooming(dc, clip);

   // Paint over the part below the tracks
   GetSize(&trackRect.width, &trackRect.height);
   AColor::Dark(dc, false);
   dc->DrawRectangle(trackRect);
}

/// AS: Note that this is being called in a loop and that the parameter values
///  are expected to be maintained each time through.
void TrackPanel::DrawEverythingElse(Track * t, wxDC * dc, wxRect & r,
                                    wxRect & trackRect, int index)
{
   trackRect.height = t->GetHeight();

   // Draw label area
   SetLabelFont(dc);
   dc->SetTextForeground(wxColour(0, 0, 0));

   int labelw = GetLabelWidth();
   int vrul = GetVRulerOffset();

   // If this track is linked to the next one, display a common
   // border for both, otherwise draw a normal border
   r = trackRect;

   bool skipBorder = false;
   if (t->GetLinked())
      r.height += mTracks->GetLink(t)->GetHeight();
   else if (mTracks->GetLink(t))
      skipBorder = true;

   if (!skipBorder)
      DrawOutside(t, dc, r, labelw, vrul, trackRect, index);

   // Believe it of not, we can speed up redrawing if we don't
   // redraw the vertical ruler when only the waveform data has
   // changed.

   int width, height;
   GetSize(&width, &height);
   wxRegion region = GetUpdateRegion();

#if DEBUG_DRAW_TIMING
   wxRect rbox = region.GetBox();
   wxPrintf(wxT("Update Region: %d %d %d %d\n"),
          rbox.x, rbox.y, rbox.width, rbox.height);
#endif
   wxRegionContain contain = region.Contains(0, 0, GetLeftOffset(), height);
   if (contain == wxPartRegion || contain == wxInRegion) {
      r = trackRect;
      r.x += GetVRulerOffset();
      r.y += kTopInset;
      r.width = GetVRulerWidth();
      r.height -= (kTopInset + 2);
      mTrackArtist->DrawVRuler(t, dc, r);
   }

   trackRect.y += t->GetHeight();
}

void TrackPanel::DrawZooming(wxDC * dc, const wxRect clip)
{
   // Draw zooming indicator that shows the region that will
   // be zoomed into when the user clicks and drags with a
   // zoom cursor.  Handles both vertical and horizontal
   // zooming.

   wxRect r;

   dc->SetBrush(*wxTRANSPARENT_BRUSH);
   dc->SetPen(*wxBLACK_DASHED_PEN);

   if (mMouseCapture==IsVZooming) {
      r.y = mZoomStart;
      r.x = GetVRulerOffset();
      r.width = 10000;
      r.height = mZoomEnd - mZoomStart;
   }
   else {
      r.x = mZoomStart;
      r.y = -1;
      r.width = mZoomEnd - mZoomStart;
      r.height = clip.height + 2;
   }
      
   dc->DrawRectangle(r);
}


void TrackPanel::DrawOutside(Track * t, wxDC * dc, const wxRect rec,
                             const int labelw, const int vrul,
                             const wxRect trackRect, int index)
{
   wxRect r = rec;

   DrawOutsideOfTrack(t, dc, r);

   r.x += kLeftInset;
   r.y += kTopInset;
   r.width -= kLeftInset * 2;
   r.height -= kTopInset;

   mTrackLabel.DrawBackground(dc, r, t->GetSelected(), labelw);
   DrawBordersAroundTrack(t, dc, r, labelw, vrul);
   DrawShadow(t, dc, r);

   r.width = mTrackLabel.GetTitleWidth();
   mTrackLabel.DrawCloseBox(dc, r, false);
   mTrackLabel.DrawTitleBar(dc, r, t, false);

   mTrackLabel.DrawMinimize(dc, r, t, (mMouseCapture==IsMinimizing), t->GetMinimized());

   if (t->GetKind() == Track::Wave) {
      mTrackLabel.DrawMuteSolo(dc, r, t, (mMouseCapture == IsMuting), false);
      mTrackLabel.DrawMuteSolo(dc, r, t, (mMouseCapture == IsSoloing), true);

      mTrackLabel.DrawSliders(dc, (WaveTrack *)t, r, index);
   }

   r = trackRect;

   if (t->GetKind() == Track::Wave && !t->GetMinimized()) {
      int offset;

      #ifdef __WXMAC__
      offset = 8;
      #else
      offset = 16;
      #endif
      
      if (r.y + 22 + 12 < rec.y + rec.height - 19)
         dc->DrawText(TrackSubText(t), r.x + offset, r.y + 22);
         
      if (r.y + 38 + 12 < rec.y + rec.height - 19)
         dc->DrawText(GetSampleFormatStr
                      (((WaveTrack *) t)->GetSampleFormat()), r.x + offset,
                      r.y + 38);
                      
   } else if (t->GetKind() == Track::Note) {
      wxRect midiRect;
      mTrackLabel.GetTrackControlsRect(trackRect, midiRect);
      ((NoteTrack *) t)->DrawLabelControls(*dc, midiRect);

   }
}

void TrackPanel::DrawOutsideOfTrack(Track * t, wxDC * dc, const wxRect r)
{
   // Fill in area outside of the track
   AColor::Dark(dc, false);
   wxRect side = r;
   side.width = kLeftInset;
   dc->DrawRectangle(side);
   side = r;
   side.height = kTopInset;
   dc->DrawRectangle(side);
   side = r;
   side.x += side.width - kTopInset;
   side.width = kTopInset;
   dc->DrawRectangle(side);

   if (t->GetLinked()) {
      side = r;
      side.y += t->GetHeight() - 1;
      side.height = kTopInset + 1;
      dc->DrawRectangle(side);
   }
}

/// The following 2 functions move to the previous and next tracks,
/// selecting after movement.
///
void TrackPanel::OnPrevTrack()
{

   Track * firstSelected = NULL;
   Track * prevSelected = NULL;
   Track * link = NULL;
   firstSelected = GetFirstSelectedTrack();

   if(firstSelected)
      {
         
         link = mTracks->GetLink(firstSelected);
         
         if(link)
         {
            //Determine whether first selected is the first 
            //or second stereo track.
            if(mTracks->GetPrev(firstSelected) == link)
               {
                  firstSelected = link;
               }
         }
         
         //get the previous track.
         prevSelected = mTracks->GetPrev(firstSelected);
      }


   //If there is nothing chosen as 'prevselected', choose the first track.
   if(!prevSelected)
      {
         //Nothing is selected, so choose the very last track.
         TrackListIterator iter = TrackListIterator(mTracks);
         firstSelected = iter.First();
		 do
            {
               prevSelected = firstSelected;
		    }
		 while( (firstSelected = iter.Next()) );
      }


   //Get rid of all selections
   SelectNone();

   
   //Now, select the track, if it exists
   if( prevSelected)
      {
         EnsureVisible(prevSelected);
         prevSelected->SetSelected(true);

         //If the newly selected track is also stereo,
         //also select it
         link = mTracks->GetLink(prevSelected);

         if(link)
            {
               link->SetSelected(true);
            }
      }

   Refresh(false);
}

void TrackPanel::OnNextTrack()
{

   Track * firstSelected = NULL;
   Track * nextSelected = NULL;
   Track * link = NULL;
   firstSelected = GetFirstSelectedTrack();

   if(firstSelected)
      {
         
         link = mTracks->GetLink(firstSelected);
         
         if(link)
         {
            //Determine whether first selected is the first 
            //or second stereo track.
            if(mTracks->GetNext(firstSelected) == link)
               {
                  firstSelected = link;
               }
         }
         
         //get the next track.
         nextSelected = mTracks->GetNext(firstSelected);
      }


   //If there is nothing chosen as 'nextselected', choose the first track.
   if(!nextSelected)
      {
         //Nothing is selected, so choose the very first track.
         TrackListIterator iter = TrackListIterator(mTracks);
         nextSelected = iter.First();
      }


   //Get rid of all selections
   SelectNone();

   
   //Now, select the track, if it exists
   if( nextSelected)
      {
         EnsureVisible(nextSelected);
         nextSelected->SetSelected(true);
      
   //If the newly selected track is also stereo,
   //also select it
   link = mTracks->GetLink(nextSelected);

   if(link)
      {
         link->SetSelected(true);
      }
      }

   Refresh(false);
}


//The following functions operate controls on specified tracks,
//This will pop up the track panning dialog for specified track
void TrackPanel::OnTrackPan(Track * t)
{
   
   if(!t) return;
   //Make a dummy mouse event
   wxMouseEvent evt = wxMouseEvent(wxEVT_LEFT_DCLICK);
   LWSlider * slider = NULL;
   
   //Find out which track we are on.
   int tracknum = FindTrackNum(t);
   if(tracknum)
      {
         slider = mTrackLabel.mPans[tracknum-1];
         slider->OnMouseEvent(evt);
      }


   SetTrackPan(t, slider);
}

void TrackPanel::OnTrackPanLeft(Track * t)
{
   
   if(!t) return;

   LWSlider * slider = NULL;
   
   //Find out which track we are on.
   int tracknum = FindTrackNum(t);
   if(tracknum)
      {
         slider = mTrackLabel.mPans[tracknum-1];
		 slider->Decrease( 1 );
      }

   SetTrackPan(t, slider);
}

void TrackPanel::OnTrackPanRight(Track * t)
{
   
   if(!t) return;

   LWSlider * slider = NULL;
   
   //Find out which track we are on.
   int tracknum = FindTrackNum(t);
   if(tracknum)
      {
         slider = mTrackLabel.mPans[tracknum-1];
		 slider->Increase( 1 );
      }

   SetTrackPan(t, slider);
}

void TrackPanel::SetTrackPan(Track * t, LWSlider * s)
{
   float newValue = s->Get();

   WaveTrack *link = (WaveTrack *)mTracks->GetLink(t);
   ((WaveTrack*)t)->SetPan(newValue);
   if (link)
      link->SetPan(newValue);

   MakeParentPushState(_("Adjusted Pan"), _("Pan"), true );

   Refresh(false);
}

//This will pop up the track gain dialog for specified track
void TrackPanel::OnTrackGain(Track * t)
{
   if(!t) return;
   wxMouseEvent evt = wxMouseEvent(wxEVT_LEFT_DCLICK);
   LWSlider * slider = NULL;

   //Find out which track we are on.
   int tracknum = FindTrackNum(t);
   if(tracknum)
      {
         slider = mTrackLabel.mGains[tracknum-1];
         slider->OnMouseEvent(evt);
      }

   SetTrackGain(t, slider);
}

void TrackPanel::OnTrackGainInc(Track * t)
{
   if(!t) return;

   LWSlider * slider = NULL;

   //Find out which track we are on.
   int tracknum = FindTrackNum(t);
   if(tracknum)
      {
         slider = mTrackLabel.mGains[tracknum-1];
		 slider->Increase( 1 );
	  }

   SetTrackGain(t, slider);
}

void TrackPanel::OnTrackGainDec(Track * t)
{
   if(!t) return;
   LWSlider * slider = NULL;

   //Find out which track we are on.
   int tracknum = FindTrackNum(t);
   if(tracknum)
      {
         slider = mTrackLabel.mGains[tracknum-1];
		 slider->Decrease( 1 );
	  }

   SetTrackGain(t, slider);
}

void TrackPanel::SetTrackGain(Track * t, LWSlider * s)
{
   float newValue = s->Get();

   WaveTrack *link = (WaveTrack *)mTracks->GetLink(t);
   ((WaveTrack*)t)->SetGain(newValue);
   if (link)
      link->SetGain(newValue);

   MakeParentPushState(_("Adjusted gain"), _("Gain"), true );

   Refresh(false);
}

void TrackPanel::OnTrackMenu(Track * t)
{
   if(t) 
      {
         mPopupMenuTarget = t;

         bool canMakeStereo = false;
         Track *next = mTracks->GetNext(t);
         
         wxMenu *theMenu = NULL;
         if (t->GetKind() == Track::Time)
            theMenu = mTimeTrackMenu;
         
         if (t->GetKind() == Track::Wave) {
            theMenu = mWaveTrackMenu;
            if (next && !t->GetLinked() && !next->GetLinked()
                && t->GetKind() == Track::Wave
                && next->GetKind() == Track::Wave)
               canMakeStereo = true;
            
            theMenu->Enable(OnMergeStereoID, canMakeStereo);
            theMenu->Enable(OnSplitStereoID, t->GetLinked());
            theMenu->Enable(OnChannelMonoID, !t->GetLinked());
            theMenu->Enable(OnChannelLeftID, !t->GetLinked());
            theMenu->Enable(OnChannelRightID, !t->GetLinked());
            
            int display = ((WaveTrack *) t)->GetDisplay();
            
            theMenu->Enable(OnWaveformID, display != WaveTrack::WaveformDisplay);
            theMenu->Enable(OnWaveformDBID,
                            display != WaveTrack::WaveformDBDisplay);
            theMenu->Enable(OnSpectrumID, display != WaveTrack::SpectrumDisplay);
            theMenu->Enable(OnPitchID, display != WaveTrack::PitchDisplay);
            
            WaveTrack * track = (WaveTrack *)t;
            SetMenuCheck(*mRateMenu, IdOfRate((int) track->GetRate()));
            SetMenuCheck(*mFormatMenu, IdOfFormat(track->GetSampleFormat()));
         }
         
         if (t->GetKind() == Track::Note)
            theMenu = mNoteTrackMenu;
         
         if (t->GetKind() == Track::Label)
            theMenu = mLabelTrackMenu;
         
         if (theMenu) {
            theMenu->Enable(OnMoveUpID, mTracks->CanMoveUp(t));
            theMenu->Enable(OnMoveDownID, mTracks->CanMoveDown(t));
            
#ifdef __WXMAC__
            ::InsertMenu((MenuRef) mRateMenu->GetHMenu(), -1);
            ::InsertMenu((MenuRef) mFormatMenu->GetHMenu(), -1);
#endif

            //We need to find the location of the menu rectangle.
            wxRect r = FindTrackRect(t,true);
            wxRect titleRect;
            mTrackLabel.GetTitleBarRect(r,titleRect);

            PopupMenu(theMenu, titleRect.x + 1,
                      titleRect.y + titleRect.height + 1);
            
#ifdef __WXMAC__
            ::DeleteMenu(mFormatMenu->MacGetMenuId());
            ::DeleteMenu(mRateMenu->MacGetMenuId());
#endif
         }
         
         
      }
   SetCapturedTrack(NULL);
   Refresh(false);
}

void TrackPanel::OnTrackMute(Track * t, bool shiftDown)
{
   if(t)
      {         
         if (shiftDown) 
            {
               // Shift-click mutes/solos this track and unmutes/unsolos other tracks.
               TrackListIterator iter(mTracks);
               Track *i = iter.First();
               while (i) {
                  if (i == t) {
                     i->SetMute(true);
                  }
                  else {
                     i->SetMute(false);
                  }
                  i = iter.Next();
               }
            }
         else {
            // Normal click just toggles this track.
               t->SetMute(!t->GetMute());
         }
      }

   Refresh(false);
}


void TrackPanel::OnTrackSolo(Track * t, bool shiftDown)
{
   if(t)
      {         
         if (shiftDown) 
            {
               // Shift-click mutes/solos this track and unmutes/unsolos other tracks.
               TrackListIterator iter(mTracks);
               Track *i = iter.First();
               while (i) {
                  if (i == t) {
                     i->SetSolo(true);

                  }
                  else {
                     i->SetSolo(false);
                     
                  }
                  i = iter.Next();
               }
            }
         else {
            // Normal click just toggles this track.

               t->SetSolo(!t->GetSolo());
         }
      }
   Refresh(false);

}

void TrackPanel::OnTrackClose(Track * t)
{

   AudacityProject *p = GetProject();
   if (!gAudioIO->IsStreamActive(p->GetAudioIOToken()))
      RemoveTrack(t);

   SetCapturedTrack( NULL );
   // BG: There are no more tracks on screen
   if (mTracks->IsEmpty()) {
      //BG: Set zoom to normal
      mViewInfo->zoom = 44100.0 / 512.0;

      //STM: Set selection to 0,0
      mViewInfo->sel0 = 0.0;
      mViewInfo->sel1 = 0.0;
      
      mListener->TP_RedrawScrollbars();
      mListener->TP_DisplayStatusMessage(wxT(""));        //STM: Clear message if all tracks are removed
   }
   Refresh(false);
}


Track * TrackPanel::GetFirstSelectedTrack()
{

   TrackListIterator iter(mTracks);
 
   Track * t;
   for ( t = iter.First();t!=NULL;t=iter.Next())
      {
         //Find the first selected track
         if(t->GetSelected())
            {
               return t;
            }

      }
   //if nothing is selected, return the first track
   t = iter.First();

   if(t)
      return t;
   else
      return NULL;
}

void TrackPanel::EnsureVisible(Track * t)
{
   TrackListIterator iter(mTracks);
   Track *it = NULL;
   Track *nt = NULL;
   //int i = 0;


   int trackTop = 0;
   int trackHeight =0;

   for (it = iter.First(); it; it = iter.Next())
   {
      trackTop += trackHeight;
      trackHeight =  it->GetHeight();


      //find the second track if this is stereo
      if (it->GetLinked())
         {
            nt = iter.Next();
            trackHeight += nt->GetHeight();
         }
      else
         {
            nt = it;
         }


      //We have found the track we want to ensure is visible.
      if ((it == t) || (nt == t))
         {
            
            //Get the size of the trackpanel.
            int width, height;
            GetSize(&width, &height);

            // Debugging line: //std::cout << trackTop << " " << mViewInfo->vpos << " " << trackHeight << " " << height << std::endl;
            if( trackTop < mViewInfo->vpos || trackTop + trackHeight > mViewInfo->vpos + height)
               {   
                  mListener->TP_ScrollUpDown(-mViewInfo->vpos);
                  mListener->TP_ScrollUpDown(trackTop / mViewInfo->scrollStep);
                  break;
               }
         }
   }
   Refresh(false);
}

void TrackPanel::DrawRuler( wxDC * dc, bool text )
{
   AudacityProject *p = GetProject();
   bool bIndicators = gAudioIO->IsStreamActive(p->GetAudioIOToken());
   mRuler->indicatorPos = bIndicators ? gAudioIO->GetStreamTime() : 0.0;

   wxRect r;
   GetSize( &r.width, &r.height );
   r.x = 0;
   r.y = 0;
   r.height = GetRulerHeight() - 1;
   mRuler->SetSize( r );
   mRuler->SetLeftOffset( GetLeftOffset() );

      
   bool bRecording = (gAudioIO->GetNumCaptureChannels() ? false : true);

   mRuler->DrawAdornedRuler( dc, mViewInfo, text, bIndicators, bRecording );
}


void TrackPanel::DrawBordersAroundTrack(Track * t, wxDC * dc,
                                        const wxRect r, const int vrul,
                                        const int labelw)
{
   // Borders around track and label area
   dc->SetPen(*wxBLACK_PEN);
   dc->DrawLine(r.x, r.y, r.x + r.width - 1, r.y);      // top
   dc->DrawLine(r.x, r.y, r.x, r.y + r.height - 1);     // left
   dc->DrawLine(r.x, r.y + r.height - 2, r.x + r.width - 1, r.y + r.height - 2);        // bottom
   dc->DrawLine(r.x + r.width - 2, r.y, r.x + r.width - 2, r.y + r.height - 1); // right
   dc->DrawLine(vrul, r.y, vrul, r.y + r.height - 1);
   dc->DrawLine(labelw, r.y, labelw, r.y + r.height - 1);       // between vruler and track

   if (t->GetLinked()) {
      int h1 = r.y + t->GetHeight() - kTopInset;
      dc->DrawLine(vrul, h1 - 2, r.x + r.width - 1, h1 - 2);
      dc->DrawLine(vrul, h1 + kTopInset, r.x + r.width - 1,
                   h1 + kTopInset);
   }

   dc->DrawLine(r.x, r.y + 16, mTrackLabel.GetTitleWidth(), r.y + 16);      // title bar
   dc->DrawLine(r.x + 16, r.y, r.x + 16, r.y + 16);     // close box
}

void TrackPanel::DrawShadow(Track * /* t */ , wxDC * dc, const wxRect r)
{
   AColor::Dark(dc, false);
   // bottom
   dc->DrawLine(r.x + 0, r.y + r.height - 1,
                r.x + 1, r.y + r.height - 1);
   // right
   dc->DrawLine(r.x + r.width - 1, r.y + 0,
                r.x + r.width - 1, r.y + 1);   

   // shadow
   dc->SetPen(*wxBLACK_PEN);
   // bottom
   dc->DrawLine(r.x + 1,       r.y + r.height - 1,
                r.x + r.width, r.y + r.height - 1);
   // right
   dc->DrawLine(r.x + r.width - 1, r.y + 1,
                r.x + r.width - 1, r.y + r.height);
}

/// AS: Returns the string to be displayed in the track label
///  indicating whether the track is mono, left, right, or 
///  stereo and what sample rate it's using.
wxString TrackPanel::TrackSubText(Track * t)
{
   wxString s = wxString::Format(wxT("%dHz"),
                                 (int) (((WaveTrack *) t)->GetRate() +
                                        0.5));
   if (t->GetLinked())
      s = _("Stereo, ") + s;
   else {
      if (t->GetChannel() == Track::MonoChannel)
         s = _("Mono, ") + s;
      else if (t->GetChannel() == Track::LeftChannel)
         s = _("Left, ") + s;
      else if (t->GetChannel() == Track::RightChannel)
         s = _("Right, ") + s;
   }

   return s;
}

/// AS: Handle the menu options that change a track between
///  left channel, right channel, and mono.
int channels[] = { Track::LeftChannel, Track::RightChannel,
   Track::MonoChannel
};

const wxChar *channelmsgs[] = { _("'left' channel"), _("'right' channel"),
   _("'mono'")
};

void TrackPanel::OnChannelChange(wxCommandEvent & event)
{
   int id = event.GetId();
   wxASSERT(id >= OnChannelLeftID && id <= OnChannelMonoID);
   wxASSERT(mPopupMenuTarget);
   mPopupMenuTarget->SetChannel(channels[id - OnChannelLeftID]);
   MakeParentPushState(wxString::Format(_("Changed '%s' to %s"),
                                        mPopupMenuTarget->GetName().
                                        c_str(),
                                        channelmsgs[id -
                                                    OnChannelLeftID]),
                       _("Channel"));
   mPopupMenuTarget = NULL;
   Refresh(false);
}

/// AS: Split a stereo track into two tracks...
void TrackPanel::OnSplitStereo(wxCommandEvent &event)
{
   wxASSERT(mPopupMenuTarget);
   mPopupMenuTarget->SetLinked(false);
   MakeParentPushState(wxString::Format(_("Split stereo track '%s'"),
                                        mPopupMenuTarget->GetName().
                                        c_str()),
                       _("Split"));

   Refresh(false);
}

/// AS: Merge two tracks into one stereo track ??
void TrackPanel::OnMergeStereo(wxCommandEvent &event)
{
   wxASSERT(mPopupMenuTarget);
   mPopupMenuTarget->SetLinked(true);
   Track *partner = mTracks->GetLink(mPopupMenuTarget);
   if (partner) {
      // Set partner's parameters to match target.
      partner->Merge(*mPopupMenuTarget);

      mPopupMenuTarget->SetChannel(Track::LeftChannel);
      partner->SetChannel(Track::RightChannel);
      MakeParentPushState(wxString::Format(_("Made '%s' a stereo track"),
                                           mPopupMenuTarget->GetName().
                                           c_str()),
                          _("Make Stereo"));
   } else
      mPopupMenuTarget->SetLinked(false);

   Refresh(false);
}

/// AS: Set the Display mode based on the menu choice in the Track Menu.
///  Note that gModes MUST BE IN THE SAME ORDER AS THE MENU CHOICES!!
///  const wxChar *gModes[] = { wxT("waveform"), wxT("waveformDB"),
///  wxT("spectrum"), wxT("pitch") };
void TrackPanel::OnSetDisplay(wxCommandEvent & event)
{
   int id = event.GetId();
   wxASSERT(id >= OnWaveformID && id <= OnPitchID);
   wxASSERT(mPopupMenuTarget
            && mPopupMenuTarget->GetKind() == Track::Wave);

   ((WaveTrack *) mPopupMenuTarget)->SetDisplay(id - OnWaveformID);
   Track *partner = mTracks->GetLink(mPopupMenuTarget);
   if (partner)
      ((WaveTrack *) partner)->SetDisplay(id - OnWaveformID);
   MakeParentModifyState();
   mPopupMenuTarget = NULL;
   Refresh(false);
}

/// AS: Sets the sample rate for a track, and if it is linked to
///  another track, that one as well.
void TrackPanel::SetRate(Track * pTrack, double rate)
{
   ((WaveTrack *) pTrack)->SetRate(rate);
   Track *partner = mTracks->GetLink(pTrack);
   if (partner)
      ((WaveTrack *) partner)->SetRate(rate);
   MakeParentPushState(wxString::Format(_("Changed '%s' to %d Hz"),
                                        pTrack->GetName().c_str(), rate),
                       _("Rate Change"));
}

/// DM: Handles the selection from the Format submenu of the
///  track menu.
void TrackPanel::OnFormatChange(wxCommandEvent & event)
{
   int id = event.GetId();
   wxASSERT(id >= On16BitID && id <= OnFloatID);
   wxASSERT(mPopupMenuTarget
            && mPopupMenuTarget->GetKind() == Track::Wave);

   sampleFormat newFormat = int16Sample;

   switch (id) {
   case On16BitID:
      newFormat = int16Sample;
      break;
   case On24BitID:
      newFormat = int24Sample;
      break;
   case OnFloatID:
      newFormat = floatSample;
      break;
   default:
      // ERROR -- should not happen
      break;
   }

   ((WaveTrack *) mPopupMenuTarget)->ConvertToSampleFormat(newFormat);
   Track *partner = mTracks->GetLink(mPopupMenuTarget);
   if (partner)
      ((WaveTrack *) partner)->ConvertToSampleFormat(newFormat);

   MakeParentPushState(wxString::Format(_("Changed '%s' to %s"),
                                        mPopupMenuTarget->GetName().
                                        c_str(),
                                        GetSampleFormatStr(newFormat)),
                       _("Format Change"));

   SetMenuCheck( *mFormatMenu, id );
   mPopupMenuTarget = NULL;
   MakeParentRedrawScrollbars();
   Refresh(false);
}

/// Converts a format enumeration to a wxWindows menu item Id.
int TrackPanel::IdOfFormat( int format )
{
   switch (format) {
   case int16Sample:
      return On16BitID;
   case int24Sample:
      return On24BitID;
   case floatSample:
      return OnFloatID;
   default:
      // ERROR -- should not happen
      wxASSERT( false );
      break;
   }
   return OnFloatID;// Compiler food.
}

/// Puts a check mark at a given position in a menu, clearing all other check marks.
void TrackPanel::SetMenuCheck( wxMenu & menu, int newId )
{
   wxMenuItemList & list = menu.GetMenuItems();
   wxMenuItem * item;
   int id;

   for ( wxwxMenuItemListNode * node = list.GetFirst(); node; node = node->GetNext() )
   {
      item = node->GetData();
      id = item->GetId();
      menu.Check( id, id==newId );
   }
}

/// AS: Ok, this function handles the selection from the Rate
///  submenu of the track menu, except for "Other" (see OnRateOther).
///  gRates MUST CORRESPOND DIRECTLY TO THE RATES AS LISTED IN THE MENU!!
///  IN THE SAME ORDER!!
const int nRates=7;
int gRates[nRates] = { 8000, 11025, 16000, 22050, 44100, 48000, 96000 };
void TrackPanel::OnRateChange(wxCommandEvent & event)
{
   int id = event.GetId();
   wxASSERT(id >= OnRate8ID && id <= OnRate96ID);
   wxASSERT(mPopupMenuTarget
            && mPopupMenuTarget->GetKind() == Track::Wave);

   SetMenuCheck( *mRateMenu, id );
   SetRate(mPopupMenuTarget, gRates[id - OnRate8ID]);

   mPopupMenuTarget = NULL;
   MakeParentRedrawScrollbars();

   Refresh(false);
}

/// Converts a sampling rate to a wxWindows menu item id
int TrackPanel::IdOfRate( int rate )
{
   for(int i=0;i<nRates;i++) {
      if( gRates[i] == rate ) 
         return i+OnRate8ID;
   }
   return OnRateOtherID;
}

void TrackPanel::OnRateOther(wxCommandEvent &event)
{
   wxASSERT(mPopupMenuTarget
            && mPopupMenuTarget->GetKind() == Track::Wave);

   wxString defaultStr;
   defaultStr.Printf(wxT("%d"),
                     (int) (((WaveTrack *) mPopupMenuTarget)->GetRate() +
                            0.5));

   // AS: TODO: REMOVE ARTIFICIAL CONSTANTS!!
   // AS: Make a real dialog box out of this!!
   double theRate;
   do {
      wxString rateStr =
          wxGetTextFromUser(_("Enter a sample rate in Hz (per second) between 1 and 100000:"),
                            _("Set Rate"), defaultStr);

      // AS: Exit if they type in nothing.
      if (wxT("") == rateStr)
         return;

      if (rateStr.ToDouble(&theRate) && theRate >= 1 && theRate <= 100000)
         break;
      else
         wxMessageBox(_("Invalid rate."));

   } while (1);

   SetMenuCheck( *mRateMenu, event.GetId() );
   SetRate(mPopupMenuTarget, theRate);

   mPopupMenuTarget = NULL;
   MakeParentRedrawScrollbars();
   Refresh(false);
}

void TrackPanel::OnSetTimeTrackRange(wxCommandEvent & /*event*/)
{
   TimeTrack *t = (TimeTrack*)mPopupMenuTarget;

   if (t) {
      long lower = t->GetRangeLower();
      long upper = t->GetRangeUpper();
      
      lower = wxGetNumberFromUser(_("Change lower speed limit (%) to:"),
                                  _("Lower speed limit"),
                                  _("Lower speed limit"),
                                  lower,
                                  13,
                                  1200);

      upper = wxGetNumberFromUser(_("Change upper speed limit (%) to:"),
                                  _("Upper speed limit"),
                                  _("Upper speed limit"),
                                  upper,
                                  lower+1,
                                  1200);

      if( lower >= 13 && upper <= 1200 && lower < upper ) {
         t->SetRangeLower(lower);
         t->SetRangeUpper(upper);
         MakeParentPushState(wxString::Format(_("Set range to '%d' - '%d'"),
                                              lower,
                                              upper),
                             _("Set Range"));
         Refresh(false);
      }
   }
   mPopupMenuTarget = NULL;
}

/// AS: Move a track up or down, depending.
void TrackPanel::OnMoveTrack(wxCommandEvent & event)
{
   wxASSERT(event.GetId() == OnMoveUpID || event.GetId() == OnMoveDownID);
   if (mTracks->Move(mPopupMenuTarget, OnMoveUpID == event.GetId())) {
      MakeParentPushState(wxString::Format(_("Moved '%s' %s"),
                                           mPopupMenuTarget->GetName().
                                           c_str(),
                                           event.GetId() ==
                                           OnMoveUpID ? _("up") :
                                           _("down")),
                          _("Move Track"));
      Refresh(false);
   }
}

/// AS: This only applies to MIDI tracks.  Presumably, it shifts the
///  whole sequence by an octave.
void TrackPanel::OnChangeOctave(wxCommandEvent & event)
{
   wxASSERT(event.GetId() == OnUpOctaveID
            || event.GetId() == OnDownOctaveID);
   wxASSERT(mPopupMenuTarget->GetKind() == Track::Note);
   NoteTrack *t = (NoteTrack *) mPopupMenuTarget;

   bool bDown = (OnDownOctaveID == event.GetId());
   t->SetBottomNote(t->GetBottomNote() + ((bDown) ? -12 : 12));

   MakeParentModifyState();
   Refresh(false);
}

void TrackPanel::OnSetName(wxCommandEvent &event)
{
   Track *t = mPopupMenuTarget;

   if (t) {
      wxString defaultStr = t->GetName();
      wxString newName = wxGetTextFromUser(_("Change track name to:"),
                                           _("Track Name"), defaultStr);
      if (newName != wxT(""))
         t->SetName(newName);
      MakeParentPushState(wxString::Format(_("Renamed '%s' to '%s'"),
                                           defaultStr.c_str(),
                                           newName.c_str()),
                          _("Name Change"));

      Refresh(false);
   }
}

/// cut selected text if cut menu item is selected
void TrackPanel::OnCutSelectedText(wxCommandEvent &event)
{
   Track *t = mPopupMenuTarget;
   ((LabelTrack *)t)->CutSelectedText();
   Refresh(false);
}

/// copy selected text if copy menu item is selected
void TrackPanel::OnCopySelectedText(wxCommandEvent &event)
{
   Track *t = mPopupMenuTarget;
   ((LabelTrack *)t)->CopySelectedText();
   Refresh(false);
}

/// paste selected text if paste menu item is selected
void TrackPanel::OnPasteSelectedText(wxCommandEvent &event)
{
   Track *t = mPopupMenuTarget;
   ((LabelTrack *)t)->PasteSelectedText();
   Refresh(false);
}

class MyFontEnumerator : public wxFontEnumerator
{
  public:
   wxArrayString facenames;
   
  protected:
   virtual bool OnFacename(const wxString& facename)
   {
      facenames.Add(facename);
      return true;
   }
};

void TrackPanel::OnSetFont(wxCommandEvent &event)
{
   MyFontEnumerator fontEnumerator;
   
   fontEnumerator.EnumerateFacenames(wxFONTENCODING_SYSTEM, false);
   int nFacenames = fontEnumerator.facenames.GetCount();
   wxString *facenames = new wxString[nFacenames];
   int i;
   for (i = 0; i < nFacenames; i++)
      facenames[i] = fontEnumerator.facenames[i];
   
   i = wxGetSingleChoiceIndex(_("Choose a font for Label Tracks"),
                              _("Label Track Font"),
                              nFacenames, facenames, this);

   delete [] facenames;
   
   if ( i == -1 )
      return;

   wxString facename = fontEnumerator.facenames[i];

   gPrefs->Write(wxT("/GUI/LabelFontFacename"), facename);

   LabelTrack::ResetFont();

   Refresh(false);
}

/// Determines which track is under the mouse 
///  @param mouseX - mouse X position.
///  @param mouseY - mouse Y position.
///  @param label  - true iff the X Y position is relative to side-panel with the labels in it.
///  @param *trackRect - returns track rectangle.
///  @param *trackNum  - returns track number.
Track *TrackPanel::FindTrack(int mouseX, int mouseY, bool label, bool link,
                              wxRect * trackRect, int *trackNum)
{
   wxRect r;
   r.x = 0;
   r.y = -mViewInfo->vpos;
   r.y += GetRulerHeight();
   r.y += kTopInset;
   GetSize(&r.width, &r.height);

   if (label) {
      r.width = GetLabelWidth() - kLeftInset;
   } else {
      r.x += GetLabelWidth() + 1;
      r.width -= GetLabelWidth() - 3;
   }

   TrackListIterator iter(mTracks);

   int n = 1;

   for (Track * t = iter.First(); t;
        r.y += t->GetHeight(), n++, t = iter.Next()) {
      r.height = t->GetHeight();

      if (link && t->GetLinked()) {
         Track *link = mTracks->GetLink(t);
         r.height += link->GetHeight();
      }
      

      //Determine whether the mouse is inside 
      //the current rectangle.  If so, recalculate
      //the proper dimensions and return.
      if (r.Inside(mouseX, mouseY)) {
         if (trackRect) {
            r.y -= kTopInset;
            if (label) {
               r.x += kLeftInset;
               r.width -= kLeftInset;
               r.y += kTopInset;
               r.height -= kTopInset;
            }
            *trackRect = r;
         }
         if (trackNum)
            *trackNum = n;
         return t;
      }
   }

   if (mouseY >= r.y && trackNum)
      *trackNum = n - 1;

   return NULL;
}


//This tells you the (1-based) index of the track you pass to it.
int TrackPanel::FindTrackNum(Track * target)
{
   if(!target) return 0;

   TrackListIterator iter(mTracks);
   int n = 1;

   for (Track * t = iter.First(); t;
        n++, t = iter.Next()) 
      {
         if(target == t)
            {
               return n;
            }

      }

   return 0;
}

//This finds the rectangle of a given track, either the
//of the label 'adornment' or the track itself
wxRect TrackPanel::FindTrackRect(Track * target, bool label)
{
   if(!target) return wxRect(0,0,0,0);

   bool linked = target->GetLinked();

   wxRect r;
   r.x = 0;
   r.y = -mViewInfo->vpos;
   r.y += GetRulerHeight();
   r.y += kTopInset;
   GetSize(&r.width, &r.height);

   TrackListIterator iter(mTracks);

   int n = 1;

   for (Track * t = iter.First(); t;
        r.y += t->GetHeight(), n++, t = iter.Next()) 
      {
         r.height = t->GetHeight();
         
         if (linked && t->GetLinked())
            {
               Track *link = mTracks->GetLink(t);
               r.height += link->GetHeight();
            }
         
         if ( t == target)
            {
               r.y -= kTopInset;
               if (label) 
                  {
                     r.x += kLeftInset;
                     r.width -= kLeftInset;
                     r.y += kTopInset;
                     r.height -= kTopInset;
                  }
               return r;
            }

      }
   return r;
}


/// Displays the bounds of the selection in the status bar.
void TrackPanel::DisplaySelection()
{
   if (!mListener)
      return;

   // DM: Note that the Selection Bar can actually MODIFY the selection
   // if snap-to mode is on!!!
   mListener->TP_DisplaySelection();
}

bool TrackPanel::MoveClipToTrack(WaveClip *clip,
                                 WaveTrack* src, WaveTrack* dst)
{
   WaveClip *clip2 = NULL;
   WaveTrack *src2 = NULL;
   WaveTrack *dst2 = NULL;

   // Make sure we have the first track of two stereo tracks
   // with both source and destination
   if (!src->GetLinked() && mTracks->GetLink(src)) {
      src = (WaveTrack*)mTracks->GetLink(src);
      if (mCapturedClipArray.GetCount() == 2) {
         if (mCapturedClipArray[0].clip == clip)
            clip = mCapturedClipArray[1].clip;
         else
            clip = mCapturedClipArray[0].clip;
      }
   }
   if (!dst->GetLinked() && mTracks->GetLink(dst))
      dst = (WaveTrack*)mTracks->GetLink(dst);

   if (mCapturedClipArray.GetCount() == 2) {
      if (mCapturedClipArray[0].clip == clip)
         clip2 = mCapturedClipArray[1].clip;
      else
         clip2 = mCapturedClipArray[0].clip;
   }

   // Get the second track of two stereo tracks
   src2 = (WaveTrack*)mTracks->GetLink(src);
   dst2 = (WaveTrack*)mTracks->GetLink(dst);

   if ((src2 && !dst2) || (dst2 && !src2))
      return false; // cannot move stereo- to mono track or other way around

   if (!dst->CanInsertClip(clip))
      return false;

   if (clip2) {
      if (!dst2->CanInsertClip(clip2))
         return false;
   }

   src->MoveClipToTrack(clip, dst);
   if (src2)
      src2->MoveClipToTrack(clip2, dst2);

   if (mCapturedClipArray.GetCount() == 2) {
      if (mCapturedClipArray[0].clip == clip) {
         mCapturedClipArray[0].track = dst;
         mCapturedClipArray[1].track = dst2;
      }
      else {
         mCapturedClipArray[0].track = dst2;
         mCapturedClipArray[1].track = dst;
      }
   }
   else {
      mCapturedClipArray[0].track = dst;
   }

   return true;
}

void TrackPanel::TakeFocus(bool bForward)
{
	SetFocus();
}

/**********************************************************************

  TrackLabel code is destined to move out of this file.
  Code should become a lot cleaner when we have sizers.  

**********************************************************************/

TrackLabel::TrackLabel(wxWindow * pParentIn)
{
   //To prevent flicker, we create an initial set of 16 sliders
   //which won't ever be shown.
   pParent = pParentIn;
   int i;
   for(i=0; i<16; i++)
      MakeMoreSliders();
}

TrackLabel::~TrackLabel()
{
   unsigned int i;
   for(i=0; i<mGains.Count(); i++)
      delete mGains[i];
   for(i=0; i<mPans.Count(); i++)
      delete mPans[i];
}

void TrackLabel::GetCloseBoxRect(const wxRect r, wxRect & dest) const
{
   dest.x = r.x;
   dest.y = r.y;
   dest.width = 16;
   dest.height = 16;
}

void TrackLabel::GetTitleBarRect(const wxRect r, wxRect & dest) const
{
   dest.x = r.x + 16;
   dest.y = r.y;
   dest.width = GetTitleWidth() - r.x - 16;
   dest.height = 16;
}

void TrackLabel::GetMuteSoloRect(const wxRect r, wxRect & dest, bool solo) const
{
   dest.x = r.x + 8;
   dest.y = r.y + 50;
   dest.width = 36;
   dest.height = 16;

   if (solo)
      dest.x += 36 + 8;
}

void TrackLabel::GetGainRect(const wxRect r, wxRect & dest) const
{
   dest.x = r.x + 7;
   dest.y = r.y + 70;
   dest.width = 84;
   dest.height = 25;
}

void TrackLabel::GetPanRect(const wxRect r, wxRect & dest) const
{
   dest.x = r.x + 7;
   dest.y = r.y + 100;
   dest.width = 84;
   dest.height = 25;
}

void TrackLabel::GetMinimizeRect(const wxRect r, wxRect &dest, bool minimized) const
{
   dest.x = r.x + 2;
   dest.width = 92;
   dest.y = r.y + r.height - 18;
   dest.height = 14;
}

void TrackLabel::DrawBackground(wxDC * dc, const wxRect r, bool bSelected,
                             const int labelw)
{
   // fill in label
   wxRect fill = r;
   fill.width = labelw - r.x;
   AColor::Medium(dc, bSelected);
   dc->DrawRectangle(fill);
   fill=wxRect( r.x+1, r.y+17, fill.width - 38, fill.height-20); 
   AColor::Bevel( *dc, true, fill );
}

void TrackLabel::GetTrackControlsRect(const wxRect r, wxRect & dest) const
{
   dest = r;
   dest.width = GetTitleWidth();
   dest.x += 4 + kLeftInset;
   dest.width -= (8 + kLeftInset);
   dest.y += 18 + kTopInset;
   dest.height -= (24 + kTopInset);
}

void TrackLabel::DrawCloseBox(wxDC * dc, const wxRect r, bool down)
{
   const int xSize=7;
   const int offset=5;

   dc->SetPen(*wxBLACK_PEN);
   // close "x"
   dc->DrawLine(r.x + offset  ,         r.y + offset, r.x + offset+xSize  , r.y + offset+xSize);
   dc->DrawLine(r.x + offset+1,         r.y + offset, r.x + offset+xSize+1, r.y + offset+xSize);
   dc->DrawLine(r.x + xSize + offset  , r.y + offset, r.x + offset,         r.y + offset+xSize);
   dc->DrawLine(r.x + xSize + offset-1, r.y + offset, r.x + offset-1,       r.y + offset+xSize);
   wxRect bev;
   GetCloseBoxRect(r, bev);
   bev.Inflate(-1, -1);
   AColor::Bevel(*dc, !down, bev);
}

void TrackLabel::DrawTitleBar(wxDC * dc, const wxRect r, Track * t,
                              bool down)
{
   wxRect bev;
   GetTitleBarRect(r, bev);
   bev.Inflate(-1, -1);
   AColor::Bevel(*dc, true, bev);

   // Draw title text
   SetLabelFont(dc);
   wxString titleStr = t->GetName();
   int allowableWidth = GetTitleWidth() - 38 - kLeftInset;
   long textWidth, textHeight;
   dc->GetTextExtent(titleStr, &textWidth, &textHeight);
   while (textWidth > allowableWidth) {
      titleStr = titleStr.Left(titleStr.Length() - 1);
      dc->GetTextExtent(titleStr, &textWidth, &textHeight);
   }
   dc->DrawText(titleStr, r.x + 19, r.y + 2);

   // Pop-up triangle
   dc->SetPen(*wxBLACK_PEN);
   int xx = r.x + GetTitleWidth() - 16 - kLeftInset;
   int yy = r.y + 5;
   int triWid = 11;
   while (triWid >= 1) {
      dc->DrawLine(xx, yy, xx + triWid, yy);
      xx++;
      yy++;
      triWid -= 2;
   }

   AColor::Bevel(*dc, !down, bev);
}

/// AS: Draw the Mute or the Solo button, depending on the value of solo.
void TrackLabel::DrawMuteSolo(wxDC * dc, const wxRect r, Track * t,
                              bool down, bool solo)
{
   wxRect bev;
   GetMuteSoloRect(r, bev, solo);
   bev.Inflate(-1, -1);
   
   if (bev.y + bev.height >= r.y + r.height - 19)
      return; // don't draw mute and solo buttons, because they don't fit into track label
      
   (solo) ? AColor::Solo(dc, t->GetSolo(), t->GetSelected()) :
       AColor::Mute(dc, t->GetMute(), t->GetSelected(), t->GetSolo());
   dc->DrawRectangle(bev);

   long textWidth, textHeight;
   wxString str = (solo) ? _("Solo") : _("Mute");

   SetLabelFont(dc);
   dc->GetTextExtent(str, &textWidth, &textHeight);
   dc->DrawText(str, bev.x + (bev.width - textWidth) / 2, bev.y + 2);

   AColor::Bevel(*dc, (solo?t->GetSolo():t->GetMute()) == down, bev);

   if (solo && !down) {
      // Update the mute button, which may be grayed out depending on
      // the state of the solo button.
      DrawMuteSolo(dc, r, t, false, false);
   }
}

void TrackLabel::DrawMinimize(wxDC * dc, const wxRect r, Track * t, bool down, bool minimized)
{
   wxRect bev;
   GetMinimizeRect(r, bev, minimized);
    
   AColor::Solo(dc, false, t->GetSelected());
   dc->DrawRectangle(bev);
    
   AColor::Dark(dc, t->GetSelected());

   // Calculate center
   int x = bev.x + bev.width / 2;
   int y = bev.y + bev.height / 2;

   wxPoint pts[3];

   if (minimized)
   {
      pts[0].x = x - 4;
      pts[0].y = y - 3;
      pts[1].x = x + 4;
      pts[1].y = y - 3;
      pts[2].x = x;
      pts[2].y = y + 3;

   } else
   {
      pts[0].x = x;
      pts[0].y = y - 3;
      pts[1].x = x + 4;
      pts[1].y = y + 3;
      pts[2].x = x - 4;
      pts[2].y = y + 3;
   }
   
   dc->DrawPolygon(3, pts);

   AColor::Bevel(*dc, !down, bev);
}

void TrackLabel::MakeMoreSliders()
{
   wxRect r(0, 0, 1000, 1000);
   wxRect gainRect;
   wxRect panRect;

   GetGainRect(r, gainRect);
   GetPanRect(r, panRect);

   /* i18n-hint: Title of the Gain slider, used to adjust the volume */
   LWSlider *slider = new LWSlider(pParent, _("Gain"),
                                   wxPoint(gainRect.x, gainRect.y),
                                   wxSize(gainRect.width, gainRect.height),
                                   DB_SLIDER);
   mGains.Add(slider);
   
   /* i18n-hint: Title of the Pan slider, used to move the sound left or right stereoscopically */
   slider = new LWSlider(pParent, _("Pan"),
                         wxPoint(panRect.x, panRect.y),
                         wxSize(panRect.width, panRect.height),
                         PAN_SLIDER);
   mPans.Add(slider);
}

void TrackLabel::EnsureSufficientSliders(int index)
{
   while (mGains.Count() < (unsigned int)index+1 ||
          mPans.Count() < (unsigned int)index+1)
      MakeMoreSliders();
}

void TrackLabel::DrawSliders(wxDC *dc, WaveTrack *t, wxRect r, int index)
{
   wxRect gainRect;
   wxRect panRect;

   EnsureSufficientSliders( index );

   GetGainRect(r, gainRect);
   GetPanRect(r, panRect);

   if (gainRect.y + gainRect.height < r.y + r.height - 19) {
      mGains[index]->Move(wxPoint(gainRect.x, gainRect.y));
      mGains[index]->Set(t->GetGain());
      mGains[index]->OnPaint(*dc, t->GetSelected());
   }

   if (panRect.y + panRect.height < r.y + r.height - 19) {
      mPans[index]->Move(wxPoint(panRect.x, panRect.y));
      mPans[index]->Set(t->GetPan());
      mPans[index]->OnPaint(*dc, t->GetSelected());
   }
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
// arch-tag: 5bb3d18e-9ba7-47c3-beef-29f5d791442a

