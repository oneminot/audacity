/**********************************************************************

  Audacity: A Digital Audio Editor

  ToolBar.cpp
  
  Dominic Mazzoni
  Shane T. Mueller

  See ToolBar.h for details.

**********************************************************************/  

#include "ToolBar.h"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif  /*  */

#ifndef WX_PRECOMP
#include <wx/defs.h>
#include <wx/brush.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/intl.h>
#include <wx/settings.h>
#include <wx/button.h>


#endif  /*  */

#include <wx/image.h>


#include "Audacity.h"
#include "widgets/AButton.h"
#include "widgets/ASlider.h"
#include "ControlToolBar.h"
#include "EditToolBar.h"
#include "MeterToolBar.h"
#include "ImageManipulation.h"
#include "MixerToolBar.h"
#include "TranscriptionToolBar.h"
#include "Project.h"


//Default toolbar images
#include  "../images/ToolBarButtons.h"

#define TOOLBAR_CLOSE_BUTTON 10001

////////////////////////////////////////////////////////////
/// class ToolBarFrame
////////////////////////////////////////////////////////////

class ToolBarFrame {
 public:
   virtual ~ToolBarFrame() {}

   ToolBar *GetToolBar() {
      return mToolBar;
   };

   virtual void DoShow(bool visible = true) = 0;
   virtual void DoMove(wxPoint where) = 0;


 protected:
   ToolBar *mToolBar;
   
};

class ToolBarMiniFrame:public wxMiniFrame, public ToolBarFrame {
public:
   ToolBarMiniFrame(wxWindow * parent, enum ToolBarType tbt);
   virtual ~ToolBarMiniFrame();
   
   void OnClose();
   void OnCloseButton(wxCommandEvent & event);
   void OnCloseWindow(wxCloseEvent & event);
   virtual void DoShow(bool visible);
   virtual void DoMove(wxPoint where);
   virtual void OnMouseEvent(wxMouseEvent& evt);
public:
   DECLARE_EVENT_TABLE()
      ;
private:
   AButton * mDockButton;
   
};

class ToolBarFullFrame:public wxFrame, public ToolBarFrame {
 public:
   ToolBarFullFrame(wxWindow * parent, enum ToolBarType tbt);
   virtual ~ToolBarFullFrame();
   
   void OnCloseWindow(wxCloseEvent & event);
   void OnSize(wxSizeEvent & event);
   virtual void DoShow(bool visible);
   virtual void DoMove(wxPoint where);
    
 public:
   DECLARE_EVENT_TABLE()
};

////////////////////////////////////////////////////////////
/// Methods for ToolBarStub
////////////////////////////////////////////////////////////


/// ToolBarStub Constructer. Requires a ToolBarType.
ToolBarStub::ToolBarStub(wxWindow * Parent, enum ToolBarType tbt) 
{
   // Don't create the frame until the first time we need it
   mToolBarFrame = NULL;
   mFrameParent = Parent;

   mType = tbt;
   mWindowedStatus = false;
   mLoadedStatus = true;

   mTitle = wxT("");
} 


/// ToolBarStub destructer
ToolBarStub::~ToolBarStub() 
{
   if (mToolBarFrame) {
      delete mToolBarFrame;
      mToolBarFrame = NULL;
   }
}


/// This will add a new toolbar to all project windows, 
/// even if one of that type already exists.
void ToolBarStub::LoadAll() 
{
   const bool CREATE_STUB_IF_REQUIRED=true;
   wxUpdateUIEvent evt;
   
   //Add the toolbar to each Window 
   int len = gAudacityProjects.GetCount();
   for (int i = 0; i < len; i++) {
      gAudacityProjects[i]->LoadToolBar(mType, CREATE_STUB_IF_REQUIRED );
      //Add the new toolbar to the ToolBarArray and redraw screen
      gAudacityProjects[i]->HandleResize();
      gAudacityProjects[i]->Refresh();
      gAudacityProjects[i]->OnUpdateMenus(evt);
   }
} 


/// This will unload the toolbar from all windows
/// If more than one toolbar of this type exists in a window, this
/// will unload them all
void ToolBarStub::UnloadAll() 
{
   wxUpdateUIEvent evt;
   int len = gAudacityProjects.GetCount();
   for (int i = 0; i < len; i++)
   {
      gAudacityProjects[i]->UnloadToolBar(mType);
      gAudacityProjects[i]->OnUpdateMenus(evt);
   }
} 


/// This will make the floating ToolBarFrame appear at the specified location
void ToolBarStub::ShowWindowedToolBar(wxPoint * where /* = NULL */ ) 
{
   if (!mWindowedStatus) {
      
      if (!mToolBarFrame) {
         //Create a frame with a toolbar of type tbt inside it
         if (mType == MeterToolBarID)
            mToolBarFrame = new ToolBarFullFrame(mFrameParent, mType);
         else
            mToolBarFrame = new ToolBarMiniFrame(mFrameParent, mType);
         
         //Get the newly-created toolbar to get some info from it.
         ToolBar * tempTB = mToolBarFrame->GetToolBar();
         
         mTitle = tempTB->GetTitle();
         mSize = tempTB->GetSize();
      }

      //Move the frame to the mouse position
      if (where) {
         mToolBarFrame->DoMove(*where);
      }
      
      //Show the new window
      mToolBarFrame->DoShow();
   }

   mWindowedStatus = true;
}


/// This will make the floating ToolBarFrame disappear (but it will still exist).
void ToolBarStub::HideWindowedToolBar() 
{
   if (mWindowedStatus) {
      if (mToolBarFrame)
         mToolBarFrame->DoShow(false);
      mWindowedStatus = false;
   }
}

// To Iconize a windowed toolbar we just hide it,
// To de-Iconize a windowed toolbar we just show it. 
void ToolBarStub::Iconize(bool bIconize) 
{
   if (mWindowedStatus && mToolBarFrame) {
      if( bIconize )
         mToolBarFrame->DoShow(false);
      else
         mToolBarFrame->DoShow(true);
   }
}


/// This finds out if a ToolBar of this type is loaded in
/// a given project window
bool ToolBarStub::IsToolBarLoaded(AudacityProject * p) 
{
   return p->IsToolBarLoaded(mType);
}


/// This will return a pointer to the ToolBar inside the member ToolBarFrame
ToolBar * ToolBarStub::GetToolBar() 
{
   return mToolBarFrame ? mToolBarFrame->GetToolBar() : NULL;
}


////////////////////////////////////////////////////////////
/// Methods for ToolBar
////////////////////////////////////////////////////////////

/// Constructor for ToolBar. Should be used by children toolbars
/// to instantiate the initial parameters of the toolbar.
ToolBar::ToolBar(wxWindow * parent, wxWindowID id, const wxPoint & pos, const
      wxSize & size, ToolBarStub * tbs) : 
   wxWindow(parent, id, pos, size, wxFULL_REPAINT_ON_RESIZE),
   mToolBarStub(tbs)
{
   //Set some default values that should be overridden
   mTitle = wxT("Audacity Toolbar");

   wxColour backgroundColour =
      wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);

   mBackgroundBrush.SetColour(backgroundColour);
   mBackgroundPen.SetColour(backgroundColour);

   mBackgroundBitmap = NULL;
   mBackgroundHeight = 0;
   mBackgroundWidth = 0;

   mIdealSize = wxSize(300, size.y);
} 


//  Alternate constructor for Toolbar.  Should probably not be used,  
//  except to create a dummy ToolBar.
/*
ToolBar::ToolBar(wxWindow * parent):wxWindow(parent, -1, wxPoint(1, 1),
              wxSize(300, 20),ToolBarStub * tbs) 
{
   //Set some default values that should be overridden
   mTitle = wxT("Audacity Toolbar");

   wxColour backgroundColour =
      wxSystemSettings::GetSystemColour(wxSYS_COLOUR_3DFACE);

   mBackgroundBrush.SetColour(backgroundColour);
   mBackgroundPen.SetColour(backgroundColour);
   mIdealSize = wxSize(300, 20);
} 

*/

ToolBar::~ToolBar()
{
   if (mBackgroundBitmap)
      delete mBackgroundBitmap;

}



   ///This is a generic function that will make a button, given
   ///generic button images.
   ///  Parameters:
   ///
   /// up:           An image of the blank button in its "up" state
   /// down:         An image of the blank button in its "down" state
   /// hilite        An image of the blank button with the hilite halo
   /// foreground:   A color bitmap of the icon on the button when its
   ///                 enabled--Can be a solid field.
   /// disabledfg:   A color bitmap of the icon when its disabled--can be a solid field.
   /// alpha:        A greyscale mask that determines how much of the
   ///                 foreground shows through
   /// id            Button ID
   /// placement     Location on the toolbar
   /// processdownevents      boolean that determine whether the button will process events
   ///                        if it is in the down position (and pop up when clicked in the down position)
   /// xadjust       x-offset to adjust the icon pixmaps from, wrt a centered icon on background image
   /// yadjust       y-offset to adjust the icon pixmaps from, wrt a centered icon on background image
AButton * ToolBar::MakeButton(wxImage * up, wxImage * down,
                              wxImage * hilite,
                              const char **foreground,
                              const char **disabledfg,
                              const char **alpha, wxWindowID id,
                              wxPoint placement,
                              bool processdownevents, wxSize size,
                              int xadjust, int yadjust) 
{





   

   wxImage * color 			= new wxImage(wxBitmap(foreground).ConvertToImage());
   wxImage * color_disabled = new wxImage(wxBitmap(disabledfg).ConvertToImage());
   wxImage * mask 			= new wxImage(wxBitmap(alpha).ConvertToImage());




   //Some images need to be centered on the button.  We calculate the centered values here, and then
   //adjust by xoff/yoff, for maximum control.
 
   int xoff = (size.GetWidth() - color->GetWidth())/2 + xadjust;
   int yoff = (size.GetHeight() - color->GetHeight())/2 + yadjust;
   
   wxImage * up2 			= OverlayImage(up, color, mask, xoff, yoff);
   wxImage * hilite2 		= OverlayImage(hilite, color, mask, xoff, yoff);
   wxImage * down2 			= OverlayImage(down, color, mask, xoff + 1, yoff + 1);
   wxImage * disable2 		= OverlayImage(up, color_disabled, mask, xoff, yoff);

   AButton * button =
      new AButton(this, id, placement, size, up2, hilite2, down2,
            disable2, processdownevents);

   delete color;
   delete color_disabled;
   delete mask;
   delete up2;
   delete down2;
   delete hilite2;
   delete disable2;

   return button;
}

///This changes the state a button (from up to down or vice versa)
void ToolBar::SetButton(bool down, AButton * button) 
{
   if (down)
      button->PushDown();
   else
      button->PopUp();
}


/// This draws the background of a toolbar
void ToolBar::DrawBackground(wxDC &dc, int width, int height)
{

#ifdef USE_AQUA_THEME

   if (mBackgroundWidth < width) {
      if (mBackgroundBitmap)
         delete mBackgroundBitmap;
      
      wxImage *aquaImage = CreateAquaBackground(width, height, 0);
      mBackgroundBitmap = new wxBitmap(aquaImage);
      delete aquaImage;
   }

   wxMemoryDC memDC;
   memDC.SelectObject(*mBackgroundBitmap);

   dc.Blit(0, 0, width, height, &memDC, 0, 0, wxCOPY, FALSE);

#if 0
   height = mIdealSize.GetHeight();

   dc.SetPen(*wxBLACK_PEN);
   dc.DrawLine(27, 0, 27, height - 1);
   dc.DrawLine(55, 0, 55, height - 1);
   dc.DrawLine(83, 0, 83, 27);
   dc.DrawLine(0, 27, 83, 27);
#endif
#else

   dc.SetBrush(mBackgroundBrush);
   dc.SetPen(mBackgroundPen);

   height = mIdealSize.GetHeight();
   dc.DrawRectangle(0, 0, width, height);

#if 0
   // JKC: This code draws a grid of lines around the first few
   // buttons on the toolbar.
   // TODO: Do we want this at all?  
   // If so this should be moved to ControlToolbar.
   // Having it here means that it is also drawn in EditToolBar,
   // which we probably don't want.  (same for the Mac 
   // version in other half of #ifdef).

   dc.SetPen(*wxBLACK_PEN);
   dc.DrawLine(27, 0, 27, height - 1);
   dc.DrawLine(55, 0, 55, height - 1);
   dc.DrawLine(83, 0, 83, height - 1);
   dc.DrawLine(0, 27, 83, 27);
#endif
#endif


}

// Static function.
ToolBar * ToolBar::MakeToolBar(wxWindow *parent, enum ToolBarType tbt)
{
   ToolBar *tb = NULL;
   
   switch (tbt) {
   case ControlToolBarID:
      tb = new ControlToolBar(parent);
      break;
   case MixerToolBarID:
      tb = new MixerToolBar(parent);
      break;
   case EditToolBarID:
      tb = new EditToolBar(parent);
      break;
   case MeterToolBarID:
      tb = new MeterToolBar(parent);
      break;
   case TranscriptionToolBarID:
      tb  = new TranscriptionToolBar(parent);
      break;
   case NoneID:
   default:
         break;
   }

   return tb;
}

// Virtual function.
// Leaves buttons positioned as-is unless
// a derived toolbar over-rides it.
void ToolBar::PlaceButton(int i, wxWindow *pWind)
{

}


////////////////////////////////////////////////////////////
/// Methods for ToolBarMiniFrame and ToolBarFullFrame
////////////////////////////////////////////////////////////
    
BEGIN_EVENT_TABLE(ToolBarMiniFrame, wxMiniFrame) 
   EVT_CLOSE(ToolBarMiniFrame::OnCloseWindow)
   EVT_MOUSE_EVENTS(ToolBarMiniFrame::OnMouseEvent)
   EVT_BUTTON(TOOLBAR_CLOSE_BUTTON, ToolBarMiniFrame::OnCloseButton)
END_EVENT_TABLE()  

///Constructor for a ToolBarMiniFrame. You give it a type and
///It will create a toolbar of that type inside the frame.
ToolBarMiniFrame::ToolBarMiniFrame(wxWindow * parent, enum ToolBarType tbt)
   : wxMiniFrame(gParentWindow, -1, wxT(""), wxPoint(1, 1),
                 wxSize(20, 20),
                 wxSTAY_ON_TOP | wxMINIMIZE_BOX | wxCAPTION
                 | ((parent == NULL)?0x0:wxFRAME_FLOAT_ON_PARENT))
{


   mToolBar = ToolBar::MakeToolBar(this, tbt);
   mToolBar->Move(20,0);
   
   //mDockButton = new wxButton(this,TOOLBAR_CLOSE_BUTTON,">|<",wxPoint(0,0),wxSize(15,mToolBar->GetHeight()));

   wxImage * up,* down, *over, *bad;

   if(mToolBar->GetSize().y > 50)
      {
         up   = new wxImage(wxBitmap(DockUp).ConvertToImage());
         down = new wxImage(wxBitmap(DockDown).ConvertToImage());
         over = new wxImage(wxBitmap(DockOver).ConvertToImage());
         bad  = new wxImage(wxBitmap(DockUp).ConvertToImage());
         
      }
   else
      {
         up   = new wxImage(wxBitmap(DockUpShort).ConvertToImage());
         down = new wxImage(wxBitmap(DockDownShort).ConvertToImage());
         over = new wxImage(wxBitmap(DockOverShort).ConvertToImage());
         bad  = new wxImage(wxBitmap(DockUpShort).ConvertToImage());
      }

   mDockButton =
      new AButton(this, TOOLBAR_CLOSE_BUTTON, wxPoint(-1,-1),
                  wxSize(up->GetWidth(),up->GetHeight()),
                  up, over, down, bad,
                  false);
   //delete up;
   //delete down;
   //delete over;


   mDockButton->SetToolTip(_("Dock Toolbar"));

   SetTitle(mToolBar->GetTitle());
   SetSize(wxSize(mToolBar->GetSize().x + 20,
                  mToolBar->GetSize().y + TOOLBAR_HEIGHT_OFFSET));
}

ToolBarMiniFrame::~ToolBarMiniFrame() 
{
   delete mToolBar;
   delete mDockButton;
}


void ToolBarMiniFrame::OnMouseEvent(wxMouseEvent& evt)
{

   //The following is prototype code for allowing a double-click
   //close the toolbar.  Commented out in lieu of a close button
   //that is being tested.

#if 0
     if(evt.ButtonDClick(1))
      {
         AudacityProject * p  = GetActiveProject();
         ToolBar* tb = ToolBarFrame::GetToolBar();
         ToolBarStub *tbs= tb->GetToolBarStub();
         p->FloatToolBar(tbs);
      }
#endif
}



/// This hides the floating toolbar, effectively 'hiding' the window
void ToolBarMiniFrame::OnClose() 
{
   mDockButton->Toggle();
   AudacityProject * p = GetActiveProject();
   ToolBarStub * tbs = ToolBarFrame::GetToolBar()->GetToolBarStub();
   p->FloatToolBar(tbs);
} 

void ToolBarMiniFrame::OnCloseButton(wxCommandEvent & WXUNUSED(event)) 
{
   OnClose();
} 

void ToolBarMiniFrame::OnCloseWindow(wxCloseEvent & WXUNUSED(event)) 
{
   
   OnClose();
} 

void ToolBarMiniFrame::DoShow(bool visible)
{
   Show(visible);
}

void ToolBarMiniFrame::DoMove(wxPoint where)
{
   Move(where);
}


    
BEGIN_EVENT_TABLE(ToolBarFullFrame, wxFrame) 
   EVT_CLOSE(ToolBarFullFrame::OnCloseWindow)
   EVT_SIZE(ToolBarFullFrame::OnSize)
END_EVENT_TABLE()  

///Constructor for a ToolBarFullFrame. You give it a type and
///It will create a toolbar of that type inside the frame.
ToolBarFullFrame::ToolBarFullFrame(wxWindow * parent, enum ToolBarType tbt)
   : wxFrame(gParentWindow, -1, wxT(""), wxPoint(1, 1),
             wxSize(20, 20),
             wxSTAY_ON_TOP | wxMINIMIZE_BOX | wxCAPTION
             | wxRESIZE_BORDER
             | ((parent == NULL)?0x0:wxFRAME_FLOAT_ON_PARENT))
{
   mToolBar = ToolBar::MakeToolBar(this, tbt);
   SetTitle(mToolBar->GetTitle());

   // This is a hack for now
   if (tbt == MeterToolBarID)
      SetSize(300, 150);
   else
      SetSize(wxSize(mToolBar->GetSize().x,
                     mToolBar->GetSize().y));
}

ToolBarFullFrame::~ToolBarFullFrame() 
{
   delete mToolBar;
}

/// This hides the floating toolbar, effectively 'hiding' the window
void ToolBarFullFrame::OnCloseWindow(wxCloseEvent & WXUNUSED(event)) 
{
   this->Hide();
} 

void ToolBarFullFrame::OnSize(wxSizeEvent & event)
{
   mToolBar->SetSize(GetClientSize());
}

void ToolBarFullFrame::DoShow(bool visible)
{
   Show(visible);
}

void ToolBarFullFrame::DoMove(wxPoint where)
{
   Move(where);
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
// arch-tag: 2f4ec75c-bdb7-4889-96d1-5d00abc41027

