/**********************************************************************

  Audacity: A Digital Audio Editor

  ViewInfo.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_VIEWINFO__
#define __AUDACITY_VIEWINFO__

const double gMaxZoom = 6000000,
             gMinZoom = 0.001;

struct ViewInfo {

   // Current selection (in seconds)

   double sel0;
   double sel1;

   // Scroll info

   int vpos;                    // vertical scroll pos

   double h;                    // h pos in secs
   double screen;               // screen width in secs
   double total;                // total width in secs
   double zoom;                 // pixels per second
   double lastZoom;

   // Actual scroll bar positions, in pixels
   int sbarH;
   int sbarScreen;
   int sbarTotal;

   int scrollStep;

   // Other stuff, mainly states (true or false) related to autoscroll and
   // drawing the waveform. Maybe this should be put somewhere else?

   bool bUpdateSpectrogram;
   bool bRedrawWaveform;
   bool bUpdateTrackIndicator;

   bool bIsPlaying;
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
// arch-tag: 961486e3-84e6-451d-98fb-2715a925ed28

