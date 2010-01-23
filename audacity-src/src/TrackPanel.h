/**********************************************************************

  Audacity: A Digital Audio Editor

  TrackPanel.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_TRACK_PANEL__
#define __AUDACITY_TRACK_PANEL__

#include <wx/dynarray.h>
#include <wx/timer.h>
#include <wx/window.h>
#include <wx/panel.h>

//Stm:  The following included because of the sampleCount struct.
#include "Sequence.h"  
#include "WaveClip.h"
#include "WaveTrack.h"
  
class wxMenu;
class wxRect;

class TrackList;
class Track;
class TrackPanel;
class TrackArtist;
class WaveTrack;
class LabelTrack;
class Ruler;
class AdornedRulerPanel;
class LWSlider;
class ControlToolBar; //Needed because state of controls can affect what gets drawn.
class AudacityProject;

struct ViewInfo;

struct tpBitmap
{
   wxBitmap *bitmap;
   wxCoord x;
   wxCoord y;
};

WX_DEFINE_ARRAY(tpBitmap *, tpBitmapArray);
WX_DEFINE_ARRAY(LWSlider *, LWSliderArray);

class TrackClip
{
 public:
   TrackClip(Track *t, WaveClip *c) { track = t; clip = c; }
   Track *track;
   WaveClip *clip;
};

WX_DECLARE_OBJARRAY(TrackClip, TrackClipArray);

class TrackPanelListener {

 public:
   TrackPanelListener(){};
   virtual ~TrackPanelListener(){};

   virtual void TP_DisplaySelection() = 0;
   virtual void TP_DisplayStatusMessage(wxString msg) = 0;
   virtual int TP_GetCurrentTool() = 0;
   virtual ControlToolBar * TP_GetControlToolBar() = 0;
   virtual void TP_OnPlayKey() = 0;
   virtual void TP_PushState(wxString shortDesc, wxString longDesc,
                             bool consolidate = false) = 0;
   virtual void TP_ModifyState() = 0;
   virtual void TP_RedrawScrollbars() = 0;
   virtual void TP_ScrollLeft() = 0;
   virtual void TP_ScrollRight() = 0;
   virtual void TP_ScrollWindow(double scrollto) = 0;
   virtual void TP_ScrollUpDown(int delta) = 0;
   virtual void TP_HasMouse() = 0;
   virtual void TP_HandleResize() = 0;
   virtual void TP_GiveFocus(bool bForward) = 0;
};


/// The TrackLabel is shown to the side of a track 
/// It has the menus, pan and gain controls displayed in it.
///
/// In its current implementation it is not derived from a
/// wxWindow.  Following the original coding style, it has 
/// been coded as a 'flyweight' class, which is passed 
/// state as needed, except for the array of gains and pans.
/// 
/// An alternative way to code this is to have an instance
/// of this class for each instance displayed.
/// 
class TrackLabel
{
public:
   TrackLabel(wxWindow * pParentIn);
   ~TrackLabel();

   int GetTitleWidth() const { return 100; }
private:
   void MakeMoreSliders();
   void EnsureSufficientSliders(int index);

   void DrawBackground(wxDC * dc, const wxRect r, bool bSelected, const int labelw);
   void DrawCloseBox(wxDC * dc, const wxRect r, bool down);
   void DrawTitleBar(wxDC * dc, const wxRect r, Track * t, bool down);
   void DrawMuteSolo(wxDC * dc, const wxRect r, Track * t, bool down, bool solo);
   void DrawVRuler(wxDC * dc, const wxRect r, Track * t);
   void DrawSliders(wxDC *dc, WaveTrack *t, wxRect r, int index);
   void DrawMinimize(wxDC * dc, const wxRect r, Track * t, bool down, bool minimized);

   void GetTrackControlsRect(const wxRect r, wxRect &dest) const;
   void GetCloseBoxRect(const wxRect r, wxRect &dest) const;
   void GetTitleBarRect(const wxRect r, wxRect &dest) const;
   void GetMuteSoloRect(const wxRect r, wxRect &dest, bool solo) const;
   void GetGainRect(const wxRect r, wxRect &dest) const;
   void GetPanRect(const wxRect r, wxRect &dest) const;
   void GetMinimizeRect(const wxRect r, wxRect &dest, bool minimized) const;

public:
   LWSliderArray mGains;
   LWSliderArray mPans;
   wxWindow * pParent;

   friend class TrackPanel;
};



const int DragThreshold = 3;// Anything over 3 pixels is a drag, else a click.

/// The TrackPanel manages multiple tracks and their TrackLabels.
/// Note that with stereo tracks there will be one TrackLabel
/// being used by two wavetracks.
class TrackPanel:public wxPanel {
 public:


   TrackPanel(wxWindow * parent,
              wxWindowID id,
              const wxPoint & pos,
              const wxSize & size,
              TrackList * tracks,
              ViewInfo * viewInfo, TrackPanelListener * listener);

   virtual ~ TrackPanel();

   void UpdatePrefs();

   void OnPaint(wxPaintEvent & event);
   void OnMouseEvent(wxMouseEvent & event);
   void OnKeyEvent(wxKeyEvent & event);

   void OnContextMenu(wxContextMenuEvent & event);

   double GetMostRecentXPos();

   void OnTimer();

   int GetRulerHeight();
   int GetLeftOffset() const { return GetLabelWidth() + 1;}

   void GetTracksUsableArea(int *width, int *height) const;

   void SelectNone();

   void SetStop(bool bStopped);

   virtual void Refresh(bool eraseBackground = TRUE,
                        const wxRect *rect = (const wxRect *) NULL);
   void DisplaySelection();

   void SetSelectionFormat(int iformat);
   void SetSnapTo(int snapto);

   void HandleShiftKey(bool down);
   AudacityProject * GetProject() const;

   void OnPrevTrack();
   void OnNextTrack();
   void OnTrackPan(Track * t);
   void OnTrackPanLeft(Track * t);
   void OnTrackPanRight(Track * t);
   void OnTrackGain(Track * t);
   void OnTrackGainDec(Track * t);
   void OnTrackGainInc(Track * t);
   void OnTrackMenu(Track * t);
   void OnTrackMute(Track * t, bool shiftdown);
   void OnTrackSolo(Track * t, bool shiftdown);
   void OnTrackClose(Track * t);
   Track * GetFirstSelectedTrack();

   void EnsureVisible(Track * t);

   void TakeFocus(bool bForward);

 private:

   bool IsUnsafe();
   bool HandleLabelTrackMouseEvent(LabelTrack * lTrack, wxRect &r, wxMouseEvent & event);
   bool HandleTrackLocationMouseEvent(WaveTrack * track, wxRect &r, wxMouseEvent &event);
   void HandleTrackSpecificMouseEvent(wxMouseEvent & event);
   void DrawCursors(wxDC * dc = NULL);
   void RemoveStaleCursors(wxRegionIterator * upd);

   void ScrollDuringDrag();
   void UpdateIndicator(wxDC * dc = NULL);
   void RemoveStaleIndicators(wxRegionIterator * upd);

   // Working out where to dispatch the event to.
   int DetermineToolToUse( ControlToolBar * pCtb, wxMouseEvent & event);
   bool HitTestEnvelope(Track *track, wxRect &r, wxMouseEvent & event);
   bool HitTestSamples(Track *track, wxRect &r, wxMouseEvent & event);
   bool HitTestSlide(Track *track, wxRect &r, wxMouseEvent & event);

   // AS: Selection handling
   void HandleSelect(wxMouseEvent & event);
   void SelectionHandleDrag(wxMouseEvent &event);
   void SelectionHandleClick(wxMouseEvent &event, 
			     Track* pTrack, wxRect r, int num);
   void StartSelection (int, int);
   void ExtendSelection(int, int);
   void SelectAllTracks();


   // AS: Cursor handling
   bool SetCursorByActivity( );
   void SetCursorAndTipWhenInLabel( Track * t, wxMouseEvent &event, const wxChar ** ppTip );
   void SetCursorAndTipWhenInVResizeArea( Track * label, bool blinked, const wxChar ** ppTip );
   void SetCursorAndTipWhenInLabelTrack( LabelTrack * pLT, wxMouseEvent & event, const wxChar ** ppTip );
   void SetCursorAndTipWhenSelectTool( Track * t, wxMouseEvent & event, wxRect &r, bool bMultiToolMode, const wxChar ** ppTip );
   void SetCursorAndTipByTool( int tool, wxMouseEvent & event, const wxChar **ppTip );
   void HandleCursor(wxMouseEvent & event);

   // AS: Envelope editing handlers
   void HandleEnvelope(wxMouseEvent & event);
   void ForwardEventToTimeTrackEnvelope(wxMouseEvent & event);
   void ForwardEventToWaveTrackEnvelope(wxMouseEvent & event);
   void ForwardEventToEnvelope(wxMouseEvent &event);

   // AS: Track sliding handlers
   void HandleSlide(wxMouseEvent & event);
   void StartSlide(wxMouseEvent &event);
   void DoSlide(wxMouseEvent &event);

   // AS: Handle zooming into tracks
   void HandleZoom(wxMouseEvent & event);
   void HandleZoomClick(wxMouseEvent & event);
   void HandleZoomDrag(wxMouseEvent & event);
   void HandleZoomButtonUp(wxMouseEvent & event);

   void DragZoom(int x);
   void DoZoomInOut(wxMouseEvent &event, int x_center);

   void HandleVZoom(wxMouseEvent & event);
   void HandleVZoomClick(wxMouseEvent & event);
   void HandleVZoomDrag(wxMouseEvent & event);
   void HandleVZoomButtonUp(wxMouseEvent & event);

   // Handle sample editing using the 'draw' tool.
   bool IsSampleEditingPossible( wxMouseEvent & event );
   void HandleSampleEditing(wxMouseEvent & event);
   void HandleSampleEditingClick( wxMouseEvent & event );
   void HandleSampleEditingDrag( wxMouseEvent & event );
   void HandleSampleEditingButtonUp( wxMouseEvent & event );

   // MM: Handle mouse wheel rotation
   void HandleWheelRotation(wxMouseEvent & event);

   void DoPopupMenu(wxMouseEvent &event, wxRect& titleRect, 
		    Track* t, wxRect &r, int num);

   // Handle resizing.
   void HandleResizeClick(wxMouseEvent & event);
   void HandleResizeDrag(wxMouseEvent & event);
   void HandleResizeButtonUp(wxMouseEvent & event);
   void HandleResize(wxMouseEvent & event);

   void HandleLabelClick(wxMouseEvent & event);
   void HandleRearrange(wxMouseEvent & event);
   void CalculateRearrangingThresholds(wxMouseEvent & event);
   void HandleClosing(wxMouseEvent & event);
   void HandleMutingSoloing(wxMouseEvent & event, bool solo);
   void HandleMinimizing(wxMouseEvent & event);
   void HandleSliders(wxMouseEvent &event, bool pan);
   bool MuteSoloFunc(Track *t, wxRect r, int x, int f, bool solo);
   bool MinimizeFunc(Track *t, wxRect r, int x, int f);
   bool GainFunc(Track * t, wxRect r, wxMouseEvent &event,
                 int index, int x, int y);
   bool PanFunc(Track * t, wxRect r, wxMouseEvent &event,
                int index, int x, int y);
   void MakeParentRedrawScrollbars();
   
   // AS: Pushing the state preserves state for Undo operations.
   void MakeParentPushState(wxString desc, wxString shortDesc,
                            bool consolidate = false);
   void MakeParentModifyState();

   void MakeParentResize();

   void OnSetName(wxCommandEvent &event);

   void OnSetFont(wxCommandEvent &event);

   void OnMoveTrack    (wxCommandEvent &event);
   void OnChangeOctave (wxCommandEvent &event);
   void OnChannelChange(wxCommandEvent &event);
   void OnSetDisplay   (wxCommandEvent &event);
   void OnSetTimeTrackRange (wxCommandEvent &event);

   void SetMenuCheck( wxMenu & menu, int newId );
   void SetRate(Track *pTrack, double rate);
   void OnRateChange(wxCommandEvent &event);
   void OnRateOther(wxCommandEvent &event);

   void OnFormatChange(wxCommandEvent &event);

   void OnSplitStereo(wxCommandEvent &event);
   void OnMergeStereo(wxCommandEvent &event);
   void OnCutSelectedText(wxCommandEvent &event);
   void OnCopySelectedText(wxCommandEvent &event);
   void OnPasteSelectedText(wxCommandEvent &event);

   void SetTrackPan(Track * t, LWSlider * s);
   void SetTrackGain(Track * t, LWSlider * s);

   void RemoveTrack(Track * toRemove);

   // Find track info by coordinate
   Track *FindTrack(int mouseX, int mouseY, bool label, bool link,
                     wxRect * trackRect = NULL, int *trackNum = NULL);

   int FindTrackNum(Track * target);
   wxRect FindTrackRect(Track * target, bool label);

//   int GetTitleWidth() const { return 100; }
   int GetTitleOffset() const { return 0; }
   int GetVRulerWidth() const { return 36;}
   int GetVRulerOffset() const { return GetTitleOffset() + mTrackLabel.GetTitleWidth();}
   int GetLabelWidth() const { return mTrackLabel.GetTitleWidth() + GetVRulerWidth();}

private:
   void DrawRuler(wxDC * dc, bool text = true);
   void DrawTrackIndicator(wxDC *dc);

   void DrawTracks(wxDC * dc);

   void DrawEverythingElse(wxDC *dc, const wxRect panelRect, const wxRect clip);
   void DrawEverythingElse(Track *t, wxDC *dc, wxRect &r, wxRect &wxTrackRect,
                           int index);
   void DrawOutside(Track *t, wxDC *dc, const wxRect rec, const int labelw, 
                    const int vrul, const wxRect trackRect, int index);
   void DrawZooming(wxDC* dc, const wxRect clip);

   void DrawShadow            (Track *t, wxDC* dc, const wxRect r);
   void DrawBordersAroundTrack(Track *t, wxDC* dc, const wxRect r, const int labelw, const int vrul);
   void DrawOutsideOfTrack    (Track *t, wxDC* dc, const wxRect r);

   int IdOfRate( int rate );
   int IdOfFormat( int format );

   //JKC: These two belong in the label track.
   int mLabelTrackStartXPos;
   int mLabelTrackStartYPos;
   
   wxString TrackSubText(Track *t);

   bool MoveClipToTrack(WaveClip *clip, WaveTrack* src, WaveTrack* dst);

   TrackLabel mTrackLabel;

   TrackPanelListener *mListener;

   TrackList *mTracks;
   ViewInfo *mViewInfo;

   AdornedRulerPanel *mRuler;


   TrackArtist *mTrackArtist;

   class AudacityTimer:public wxTimer {
   public:
     virtual void Notify() { parent->OnTimer(); }
     TrackPanel *parent;
   } mTimer;
   

   //This stores the parts of the screen that get overwritten by the indicator
   tpBitmapArray mScreenAtIndicator;
   
   // This indicates whether the last indicator drawing
   // existed, so that we can draw over it to erase it
   bool mPlayIndicatorExists;

   tpBitmapArray mPreviousCursorData;

   int mTimeCount;

   wxBitmap *mBitmap;
   int mPrevWidth;
   int mPrevHeight;

   double mSelStart;

   Track *mCapturedTrack;
   WaveClip *mCapturedClip;
   TrackClipArray mCapturedClipArray;
   bool mCapturedClipIsSelection;
   WaveTrack::Location mCapturedTrackLocation;
   wxRect mCapturedTrackLocationRect;
   wxRect mCapturedRect;
   int mCapturedNum;

   // When sliding horizontally, the moving clip may automatically
   // snap to the beginning and ending of other clips, or to label
   // starts and stops.  When you start sliding, SlideSnapFromPoints
   // gets populated with the start and stop times of selected clips,
   // and SlideSnapToPoints gets populated with the start and stop times
   // of other clips.  In both cases, times that are within 3 pixels
   // of another at the same zoom level are eliminated; you can't snap
   // when there are two things arbitrarily close at that zoom level.
   wxBaseArrayDouble mSlideSnapFromPoints;
   wxBaseArrayDouble mSlideSnapToPoints;
   wxArrayInt mSlideSnapLinePixels;

   // The amount that clips are sliding horizontally; this allows
   // us to undo the slide and then slide it by another amount
   double mHSlideAmount;

   bool mDidSlideVertically;

   bool mRedrawAfterStop;

   bool mIndicatorShowing;

   wxMouseEvent mLastMouseEvent;

   int mMouseClickX;
   int mMouseClickY;

   int mMouseMostRecentX;
   int mMouseMostRecentY;

   int mZoomStart;
   int mZoomEnd;

   Track * mDrawingTrack;          // Keeps track of which track you are drawing on between events cf. HandleDraw()
   int mDrawingTrackTop;           // Keeps track of the top position of the drawing track.
   sampleCount mDrawingStartSample;   // sample of last click-down
   float mDrawingStartSampleValue;    // value of last click-down
   sampleCount mDrawingLastDragSample; // sample of last drag-over
   float mDrawingLastDragSampleValue;  // value of last drag-over
 
   double PositionToTime(int mouseXCoordinate,
                         int trackLeftEdge) const;
   int TimeToPosition(double time,
                      int trackLeftEdge) const;

   int mInitialTrackHeight;
   int mInitialUpperTrackHeight;
   bool mAutoScrolling;

   enum   MouseCaptureEnum
   {
      IsUncaptured=0,   // This is the normal state for the mouse
      IsVZooming,
      IsClosing,
      IsSelecting,
      IsAdjustingLabel,
      IsResizing,
      IsResizingBetweenLinkedTracks,
      IsResizingBelowLinkedTracks,
      IsRearranging,
      IsSliding,
      IsEnveloping,
      IsMuting,
      IsSoloing,
      IsGainSliding,
      IsPanSliding,
      IsMinimizing,
      IsOverCutLine,
      WasOverCutLine
   };

   enum MouseCaptureEnum mMouseCapture;
   void SetCapturedTrack( Track * t, enum MouseCaptureEnum MouseCapture=IsUncaptured );

   bool mAdjustSelectionEdges;
   bool mSlideUpDownOnly;

   // JH: if the user is dragging a track, at what y
   //   coordinate should the dragging track move up or down?
   int mMoveUpThreshold;
   int mMoveDownThreshold;
   
   bool IsDragZooming() const { return abs(mZoomEnd - mZoomStart) > DragThreshold;}

   wxCursor *mArrowCursor;
   wxCursor *mPencilCursor;
   wxCursor *mSelectCursor;
   wxCursor *mResizeCursor;
   wxCursor *mSlideCursor;
   wxCursor *mEnvelopeCursor;
   wxCursor *mSmoothCursor;
   wxCursor *mZoomInCursor;
   wxCursor *mZoomOutCursor;
   wxCursor *mLabelCursorLeft;
   wxCursor *mLabelCursorRight;
   wxCursor *mRearrangeCursor;
   wxCursor *mDisabledCursor;
   wxCursor *mAdjustLeftSelectionCursor;
   wxCursor *mAdjustRightSelectionCursor;

   wxMenu *mWaveTrackMenu;
   wxMenu *mNoteTrackMenu;
   wxMenu *mTimeTrackMenu;
   wxMenu *mLabelTrackMenu;
   wxMenu *mRateMenu;
   wxMenu *mFormatMenu;
   wxMenu *mLabelTrackLabelMenu;

   Track *mPopupMenuTarget;

 public:

   DECLARE_EVENT_TABLE()
};

//This constant determines the size of the vertical region (in pixels) around
//the bottom of a track that can be used for vertical track resizing.
#define TRACK_RESIZE_REGION 5

//This constant determines the size of the horizontal region (in pixels) around
//the right and left selection bounds that can be used for horizontal selection adjusting
#define SELECTION_RESIZE_REGION 3

#define SMOOTHING_KERNEL_RADIUS 3
#define SMOOTHING_BRUSH_RADIUS 5
#define SMOOTHING_PROPORTION_MAX 0.7
#define SMOOTHING_PROPORTION_MIN 0.0

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
// arch-tag: 1f8c3d0e-849e-4f3c-95b5-9ead0789f999
