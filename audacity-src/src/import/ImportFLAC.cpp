/**********************************************************************

  Audacity: A Digital Audio Editor

  ImportFLAC.cpp

  Copyright 2004  Sami Liedes
  Based on ImportPCM.cpp by Dominic Mazzoni
  Licensed under the GNU General Public License v2 or later

**********************************************************************/

#include <wx/defs.h>
#include "../Audacity.h"

#include "Import.h"
#include "ImportPlugin.h"

#ifndef USE_LIBFLAC

void GetFLACImportPlugin(ImportPluginList *importPluginList,
                        UnusableImportPluginList *unusableImportPluginList)
{
   UnusableImportPlugin* flacIsUnsupported =
      new UnusableImportPlugin(wxT("FLAC"),
                               wxStringList(wxT("flac"), wxT("flc"), NULL));

   unusableImportPluginList->Append(flacIsUnsupported);
}

#else /* USE_LIBFLAC */

#include "../Internat.h"
#include "ImportFLAC.h"

#include <wx/string.h>
#include <wx/utils.h>
#include <wx/intl.h>
#include <wx/ffile.h>

#include "FLAC++/decoder.h"

#include "../FileFormats.h"
#include "../Prefs.h"
#include "../WaveTrack.h"
#include "ImportPlugin.h"

class FLACImportFileHandle;

class MyFLACFile : public FLAC::Decoder::File
{
 public:
   MyFLACFile(FLACImportFileHandle *handle) : mFile(handle) {}
 private:
   friend class FLACImportFileHandle;
   FLACImportFileHandle *mFile;
 protected:
   virtual FLAC__StreamDecoderWriteStatus write_callback(const FLAC__Frame *frame,
							 const FLAC__int32 * const buffer[]);
   virtual void metadata_callback(const FLAC__StreamMetadata *metadata);
   virtual void error_callback(FLAC__StreamDecoderErrorStatus status);
};


class FLACImportPlugin : public ImportPlugin
{
 public:
   FLACImportPlugin():
      ImportPlugin(wxStringList(wxT("flac"),NULL))
   {
   }

   ~FLACImportPlugin() { }

   wxString GetPluginFormatDescription();
   ImportFileHandle *Open(wxString Filename);
};


class FLACImportFileHandle : public ImportFileHandle
{
   friend class MyFLACFile;
public:
   FLACImportFileHandle(wxString name);
   ~FLACImportFileHandle();

   bool Init();

   void SetProgressCallback(progress_callback_t function,
                            void *userData);
   wxString GetFileDescription();
   int GetFileUncompressedBytes();
   bool Import(TrackFactory *trackFactory, Track ***outTracks,
               int *outNumTracks);
private:
   wxString              mName;
   sampleFormat          mFormat;
   MyFLACFile           *mFile;
   void                 *mUserData;
   progress_callback_t   mProgressCallback;
   unsigned long         mSampleRate;
   unsigned long         mNumChannels;
   unsigned long         mBitsPerSample;
   FLAC__uint64          mNumSamples;
   FLAC__uint64          mSamplesDone;
   bool                  mStreamInfoDone;
   WaveTrack           **mChannels;
};


void MyFLACFile::metadata_callback(const FLAC__StreamMetadata *metadata)
{
   if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO)
      return;			// ignore
   mFile->mSampleRate=metadata->data.stream_info.sample_rate;
   mFile->mNumChannels=metadata->data.stream_info.channels;
   mFile->mBitsPerSample=metadata->data.stream_info.bits_per_sample;
   mFile->mNumSamples=metadata->data.stream_info.total_samples;

   if (mFile->mBitsPerSample<=16) {
      if (mFile->mFormat<int16Sample)
	 mFile->mFormat=int16Sample;
   } else if (mFile->mBitsPerSample<=24) {
      if (mFile->mFormat<int24Sample)
	 mFile->mFormat=int24Sample;
   } else
      mFile->mFormat=floatSample;
   mFile->mStreamInfoDone=true;
}

void MyFLACFile::error_callback(FLAC__StreamDecoderErrorStatus status)
{
   //wxASSERT(0);
}


FLAC__StreamDecoderWriteStatus MyFLACFile::write_callback(const FLAC__Frame *frame,
							  const FLAC__int32 * const buffer[])
{
   short *tmp=new short[frame->header.blocksize];

   for (unsigned int chn=0; chn<mFile->mNumChannels; chn++) {
      for (unsigned int s=0; s<frame->header.blocksize; s++)
	 tmp[s]=buffer[chn][s];
      mFile->mChannels[chn]->Append((samplePtr)tmp,
				   int16Sample,
				   frame->header.blocksize);
   }

   delete [] tmp;

   mFile->mSamplesDone += frame->header.blocksize;

   if (mFile->mProgressCallback &&
       mFile->mProgressCallback(mFile->mUserData,
                                float(mFile->mSamplesDone)/
                                mFile->mNumSamples))
      return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

   return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


void GetFLACImportPlugin(ImportPluginList *importPluginList,
			 UnusableImportPluginList *unusableImportPluginList)
{
   importPluginList->Append(new FLACImportPlugin);
}


wxString FLACImportPlugin::GetPluginFormatDescription()
{
    return wxT("FLAC");
}


ImportFileHandle *FLACImportPlugin::Open(wxString filename)
{
   FLACImportFileHandle *handle = new FLACImportFileHandle(filename);

   bool success = handle->Init();
   if (!success) {
      delete handle;
      return NULL;
   }

   return handle;
}


FLACImportFileHandle::FLACImportFileHandle(wxString name):
   mName(name),
   mFile(new MyFLACFile(this)),
   mUserData(NULL),
   mProgressCallback(NULL),
   mSamplesDone(0),
   mStreamInfoDone(false)
{
   mFormat = (sampleFormat)
      gPrefs->Read(wxT("/SamplingRate/DefaultProjectSampleFormat"), floatSample);
}

bool FLACImportFileHandle::Init()
{
   bool success = mFile->set_filename(mName.fn_str());
   if (!success) {
      return false;
   }
   FLAC::Decoder::File::State state = mFile->init();
   if (state != FLAC__FILE_DECODER_OK) {
      return false;
   }
   mFile->process_until_end_of_metadata();
   state = mFile->get_state();
   if (state != FLAC__FILE_DECODER_OK) {
      return false;
   }
   return true;
}

void FLACImportFileHandle::SetProgressCallback(progress_callback_t progressCallback,
                                      void *userData)
{
   mProgressCallback = progressCallback;
   mUserData = userData;
}


wxString FLACImportFileHandle::GetFileDescription()
{
   return wxT("FLAC");
}


int FLACImportFileHandle::GetFileUncompressedBytes()
{
   // TODO: Get Uncompressed byte count.
   return 0;
}


bool FLACImportFileHandle::Import(TrackFactory *trackFactory,
				  Track ***outTracks,
				  int *outNumTracks)
{
   wxASSERT(mStreamInfoDone);
   
   *outNumTracks = mNumChannels;

   mChannels = new WaveTrack *[*outNumTracks];

   int c;
   for (c = 0; c < *outNumTracks; c++) {
      mChannels[c] = trackFactory->NewWaveTrack(mFormat);
      mChannels[c]->SetRate(mSampleRate);
      
      if (*outNumTracks == 2)
         switch (c) {
         case 0:
            mChannels[c]->SetChannel(Track::LeftChannel);
            break;
         case 1:
            mChannels[c]->SetChannel(Track::RightChannel);
            break;
         default:
            mChannels[c]->SetChannel(Track::MonoChannel);
         }
      else
	 mChannels[c]->SetChannel(Track::MonoChannel);
   }

   if (*outNumTracks == 2)
      mChannels[0]->SetLinked(true);

   mFile->process_until_end_of_file();
   
   *outTracks = new Track *[*outNumTracks];
   for(c = 0; c < *outNumTracks; c++) {
      mChannels[c]->Flush();
      (*outTracks)[c] = mChannels[c];
   }
   delete[] mChannels;

   return true;
}


FLACImportFileHandle::~FLACImportFileHandle()
{
   mFile->finish();
   delete mFile;
}

#endif /* USE_LIBFLAC */
