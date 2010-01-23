/**********************************************************************

  Audacity: A Digital Audio Editor

  SimpleBlockFile.h

  Joshua Haberman

**********************************************************************/

#ifndef __AUDACITY_SIMPLE_BLOCKFILE__
#define __AUDACITY_SIMPLE_BLOCKFILE__

#include <wx/string.h>
#include <wx/filename.h>

#include "../BlockFile.h"
#include "../DirManager.h"

/// A BlockFile that reads and writes uncompressed data using libsndfile

/// A block file that writes the audio data to an .au file and reads
/// it back using libsndfile.
///
/// There are two ways to construct a simple block file.  One is to
/// supply data and have the constructor write the file.  The other
/// is for when the file already exists and we simply want to create
/// the data structure to refer to it.
class SimpleBlockFile : public BlockFile {
 public:

   // Constructor / Destructor

   /// Create a disk file and write summary and sample data to it
   SimpleBlockFile(wxFileName baseFileName,
                   samplePtr sampleData, sampleCount sampleLen,
                   sampleFormat format);
   /// Create the memory structure to refer to the given block file
   SimpleBlockFile(wxFileName existingFile, sampleCount len,
                   float min, float max, float rms);

   virtual ~SimpleBlockFile();

   // Reading

   /// Read the summary section of the disk file
   virtual bool ReadSummary(void *data);
   /// Read the data section of the disk file
   virtual int ReadData(samplePtr data, sampleFormat format,
                        sampleCount start, sampleCount len);

   /// Create a new block file identical to this one
   virtual BlockFile *Copy(wxFileName newFileName);
   /// Write an XML representation of this file
   virtual void SaveXML(int depth, wxFFile &xmlFile);
   virtual int GetSpaceUsage();
   virtual void Recover();

   static BlockFile *BuildFromXML(DirManager &dm, const wxChar **attrs);
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
// arch-tag: 58e2a052-2894-4c0c-b35d-0a08b3c61cd7

