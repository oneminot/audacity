/**********************************************************************

  Audacity: A Digital Audio Editor

  WaveTrack.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_WAVETRACK__
#define __AUDACITY_WAVETRACK__

#include "Track.h"
#include "SampleFormat.h"
#include "Sequence.h"
#include "WaveClip.h"

#include <wx/gdicmn.h>
#include <wx/longlong.h>
#include <wx/thread.h>

//
// Tolerance for merging wave tracks (in seconds)
//
#define WAVETRACK_MERGE_POINT_TOLERANCE 0.01

typedef wxLongLong_t longSampleCount; /* 64-bit int */

class Envelope;

class WaveTrack: public Track {

 private:

   //
   // Constructor / Destructor / Duplicator
   //
   // Private since only factories are allowed to construct WaveTracks
   //

   WaveTrack(DirManager * projDirManager, 
             sampleFormat format = (sampleFormat)0,
             double rate = 0);
   WaveTrack(WaveTrack &orig);

   void Init(const WaveTrack &orig);
   virtual Track *Duplicate();

   friend class TrackFactory;

 public:
 
   enum LocationType {
      locationCutLine = 1,
      locationMergePoint
   };

   struct Location {
      // Position of track location
      double pos;
      
      // Type of track location
      LocationType typ;
      
      // Only for typ==locationMergePoint
      int clipidx1; // first clip (left one)
      int clipidx2; // second clip (right one)
   };
      
   virtual ~WaveTrack();
   virtual double GetOffset();
   virtual void SetOffset (double o);

   // Start time is the earliest time there is a clip
   double GetStartTime();

   // End time is where the last clip ends, plus recorded stuff
   double GetEndTime();

   //
   // Identifying the type of track
   //

   virtual int GetKind() const { return Wave; } 

   //
   // WaveTrack parameters
   //

   double GetRate() const;
   void SetRate(double newRate);

   // Multiplicative factor.  Only converted to dB for display.
   float GetGain() const;
   void SetGain(float newGain);

   // -1.0 (left) -> 1.0 (right)
   float GetPan() const;
   void SetPan(float newPan);

   // Takes gain and pan into account
   float GetChannelGain(int channel);

   sampleFormat GetSampleFormat() { return mFormat; }
   bool ConvertToSampleFormat(sampleFormat format);

   //
   // High-level editing
   //

   virtual bool Cut  (double t0, double t1, Track **dest);
   virtual bool Copy (double t0, double t1, Track **dest);
   virtual bool Clear(double t0, double t1);
   virtual bool Paste(double t, Track *src);

   virtual bool Silence(double t0, double t1);
   virtual bool InsertSilence(double t, double len);

   virtual bool SplitAt(double t);
   virtual bool CutAndAddCutLine(double t0, double t1, Track **dest);
   virtual bool ClearAndAddCutLine(double t0, double t1);

   virtual bool SplitCut   (double t0, double t1, Track **dest);
   virtual bool SplitDelete(double t0, double t1);
   virtual bool Join       (double t0, double t1);

   virtual bool Trim (double t0, double t1);


   bool HandleClear(double t0, double t1,
                    bool addCutLines, bool split);

   // Returns true if there are no WaveClips in that region
   bool IsEmpty(double t0, double t1);

   /// You must call Flush after the last Append
   bool Append(samplePtr buffer, sampleFormat format,
               sampleCount len, unsigned int stride=1);
   /// Flush must be called after last Append
   bool Flush();

   bool AppendAlias(wxString fName, sampleCount start,
                    sampleCount len, int channel);

   ///
   /// MM: Now that each wave track can contain multiple clips, we don't
   /// have a continous space of samples anymore, but we simulate it,
   /// because there are alot of places (e.g. effects) using this interface.
   /// This interface makes much sense for modifying samples, but note that
   /// it is not time-accurate, because the "offset" is a double value and
   /// therefore can lie inbetween samples. But as long as you use the
   /// same value for "start" in both calls to "Set" and "Get" it is
   /// guaranteed that the same samples are affected.
   ///
   bool Get(samplePtr buffer, sampleFormat format,
                   longSampleCount start, sampleCount len);
   bool Set(samplePtr buffer, sampleFormat format,
                   longSampleCount start, sampleCount len);
   void GetEnvelopeValues(double *buffer, int bufferLen,
                         double t0, double tstep);
   bool GetMinMax(float *min, float *max,
                  double t0, double t1);

   //
   // MM: We now have more than one sequence and envelope per track, so
   // instead of GetSequence() and GetEnvelope() we have the following
   // function which give the sequence and envelope which is under the
   // given X coordinate of the mouse pointer.
   //
   WaveClip* GetClipAtX(int xcoord);
   Sequence* GetSequenceAtX(int xcoord);
   Envelope* GetEnvelopeAtX(int xcoord);

   //
   // Getting information about the track's internal block sizes
   // for efficiency
   //

   sampleCount GetBestBlockSize(longSampleCount t);
   sampleCount GetMaxBlockSize();
   sampleCount GetIdealBlockSize();

   //
   // XMLTagHandler callback methods for loading and saving
   //

   virtual bool HandleXMLTag(const wxChar *tag, const wxChar **attrs);
   virtual void HandleXMLEndTag(const wxChar *tag);
   virtual XMLTagHandler *HandleXMLChild(const wxChar *tag);
   virtual void WriteXML(int depth, FILE *fp);

   // Returns true if an error occurred while reading from XML
   virtual bool GetErrorOpening();

   //
   // Lock and unlock the track: you must lock the track before
   // doing a copy and paste between projects.
   //

   bool Lock();
   bool Unlock();

   // Utility function to convert between times in seconds
   // and sample positions

   longSampleCount TimeToLongSamples(double t0);

   // Get access to the clips in the tracks. This is used by
   // track artists and also by TrackPanel when sliding...it would
   // be cleaner if this could be removed, though...
   WaveClipList::Node* GetClipIterator() { return mClips.GetFirst(); }

   // Create new clip and add it to this track. Returns a pointer
   // to the newly created clip.
   WaveClip* CreateClip();

   // Get access to the last clip, or create a clip, if there is not
   // already one.
   WaveClip* GetLastOrCreateClip();

   // Get the linear index of a given clip (-1 if the clip is not found)
   int GetClipIndex(WaveClip* clip);

   // Get the nth clip in this WaveTrack (will return NULL if not found).
   // Use this only in special cases (like getting the linked clip), because
   // it is much slower than GetClipIterator().
   WaveClip* GetClipByIndex(int index);

   // Get number of clips in this WaveTrack
   int GetNumClips() const;

   // Before calling 'Offset' on a clip, use this function to see if the
   // offsetting is allowed with respect to the other clips in this track.
   // This function can optionally return the amount that is allowed for offsetting
   // in this direction maximally.
   bool CanOffsetClip(WaveClip* clip, double amount, double *allowedAmount=NULL);

   // Before moving a clip into a track (or inserting a clip), use this 
   // function to see if the times are valid (i.e. don't overlap with
   // existing clips).
   bool CanInsertClip(WaveClip* clip);

   // Move a clip into a new track. This will remove the clip
   // in this cliplist and add it to the cliplist of the
   // other clip. No fancy additional stuff is done.
   void MoveClipToTrack(int clipIndex, WaveTrack* dest);
   void MoveClipToTrack(WaveClip *clip, WaveTrack* dest);
   
   // Merge two clips, that is append data from clip2 to clip1,
   // then remove clip2 from track.
   // clipidx1 and clipidx2 are indices into the clip list.
   bool MergeClips(int clipidx1, int clipidx2);

   // Set/get rectangle that this WaveClip fills on screen. This is
   // called by TrackArtist while actually drawing the tracks and clips.
   void SetDisplayRect(const wxRect& r) { mDisplayRect = r; }
   void GetDisplayRect(wxRect* r) { *r = mDisplayRect; }

   // Cache special locations (e.g. cut lines) for later speedy access
   void UpdateLocationsCache();

   // Get number of cached locations
   int GetNumCachedLocations() { return mDisplayNumLocations; }
   Location GetCachedLocation(int index) { return mDisplayLocations[index]; }

   // Expand cut line (that is, re-insert audio, then delete audio saved in cut line)
   bool ExpandCutLine(double cutLinePosition, double* cutlineStart = NULL, double* cutlineEnd = NULL);

   // Remove cut line, without expanding the audio in it
   bool RemoveCutLine(double cutLinePosition);

   // This track has been merged into a stereo track.  Copy shared parameters
   // from the new partner.
   virtual void Merge(const Track &orig);

   //
   // The following code will eventually become part of a GUIWaveTrack
   // and will be taken out of the WaveTrack class:
   //
   
   enum {
      WaveformDisplay,
      WaveformDBDisplay,
      SpectrumDisplay,
      PitchDisplay
   } WaveTrackDisplay;

   void SetDisplay(int display) {mDisplay = display;}
   int GetDisplay() {return mDisplay;}

   void GetDisplayBounds(float *min, float *max);
   void SetDisplayBounds(float min, float max);

 protected:

   //
   // Protected variables
   //

   WaveClipList mClips;

   wxRect        mDisplayRect;
   sampleFormat  mFormat;
   int           mRate;
   float         mGain;
   float         mPan;


   //
   // Data that should be part of GUIWaveTrack
   // and will be taken out of the WaveTrack class:
   //
   float         mDisplayMin;
   float         mDisplayMax;
   int           mDisplay; // type of display, from WaveTrackDisplay enum
   int           mDisplayNumLocations;
   int           mDisplayNumLocationsAllocated;
   Location*       mDisplayLocations;

   //
   // Protected methods
   //

 private:

   //
   // Private variables
   //

   wxCriticalSection mFlushCriticalSection;
   wxCriticalSection mAppendCriticalSection;

};

#endif // __AUDACITY_WAVETRACK__

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: bf4c1c84-7a1d-4689-86fd-f6926b5d3f9a

