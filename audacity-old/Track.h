/**********************************************************************

  Audacity: A Digital Audio Editor

  Track.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __TRACK__
#define __TRACK__

class wxString;
class wxDC;
class wxRect;
class GenericStream;
class DirManager;

class VTrack
{
public:
  int collapsedHeight;
  int expandedHeight;

  bool selected;

  double tOffset;

  DirManager *dirManager;

  enum {
	None,
	Wave,
	Note,
	Beat,
	Label
  } kind;

  VTrack(DirManager *projDirManager);

  virtual ~VTrack() {}

  virtual VTrack *Duplicate();

  virtual void Draw(wxDC &dc, wxRect &r, double h, double pps,
		    double sel0, double sel1) {}
	
  virtual void Cut(double t0, double t1, VTrack **dest) { dest = 0; }
  virtual void Copy(double t0, double t1, VTrack **dest) { dest = 0; }
  virtual void Paste(double t, VTrack *src) {}
  virtual void Clear(double t0, double t1) {}

  virtual void SetHeight(int h);

  virtual void Collapse();
  virtual void Expand();
  virtual void Toggle();
  virtual bool IsCollapsed();
  
  virtual void Offset(double t) {
    tOffset += t;
  }

  virtual int GetKind() {return None;}

  virtual bool Load(GenericStream *in, DirManager *dirManager);
  virtual bool Save(GenericStream *out, bool overwrite);

  virtual int GetHeight() {
	return (collapsed? collapsedHeight: expandedHeight);
  }

  virtual double GetMaxLen() {
    return 0.0;
  }

private:
  bool collapsed;

};

/*
  TrackList is a flat linked list of tracks supporting Add,
  Remove, Clear, and Contains, plus serialization of the
  list of tracks.
*/

struct TrackListNode
{
  VTrack *t;
  TrackListNode *next;
  TrackListNode *prev;
};

class TrackList
{
public:
  // Create an empty TrackList
  TrackList();

  // Copy constructor
  TrackList(TrackList *t);

  // Destructor
  ~TrackList();

  // Add a this Track or all children of this TrackGroup
  void Add(VTrack *t);

  // Remove this Track or all children of this TrackGroup
  void Remove(VTrack *t);

  // Make the list empty
  void Clear();

  // Test
  bool Contains(VTrack *t);

  // Iterate functions
  VTrack *First();
  VTrack *Next();
  VTrack *RemoveCurrent(); // returns next

  double GetMaxLen();
  int GetHeight();

  // File I/O
  bool Save(GenericStream *out, bool overwrite);
  bool Load(GenericStream *in, DirManager *dirManager);
  
private:
  TrackListNode *head;
  TrackListNode *tail;
  TrackListNode *cur;
};

#endif
