#include <math.h>
#include <wx/log.h>

#include "Spectrum.h"
#include "Prefs.h"
#include "WaveClip.h"
#include "Envelope.h"


#include <wx/listimpl.cpp>
WX_DEFINE_LIST(WaveClipList);

class WaveCache {
public:
   WaveCache(int cacheLen)
   {
      dirty = -1;
      start = -1.0;
      pps = 0.0;
      len = cacheLen;
      min = new float[len];
      max = new float[len];
      rms = new float[len];
      where = new sampleCount[len+1];
   }

   ~WaveCache()
   {
      delete[] min;
      delete[] max;
      delete[] rms;
      delete[] where;
   }

   int          dirty;
   sampleCount  len;
   double       start;
   double       pps;
   sampleCount *where;
   float       *min;
   float       *max;
   float       *rms;
};

class SpecCache {
public:
   SpecCache(int cacheLen, int viewHeight, bool autocorrelation)
   {
      maxFreqPrefOld = -1;
      windowSizeOld = -1;
      dirty = -1;
      start = -1.0;
      pps = 0.0;
      len = cacheLen;
      ac = autocorrelation;
      height = viewHeight;
      freq = new float[len*height];
      where = new sampleCount[len+1];
   }

   ~SpecCache()
   {
      delete[] freq;
      delete[] where;
   }

   int          maxFreqPrefOld;
   int          windowSizeOld;
   int          dirty;
   int          height;
   bool         ac;
   sampleCount  len;
   double       start;
   double       pps;
   sampleCount *where;
   float       *freq;
};


WaveClip::WaveClip(DirManager *projDirManager, sampleFormat format, int rate)
{
   mOffset = 0;
   mRate = rate;
   mSequence = new Sequence(projDirManager, format);
   mEnvelope = new Envelope();
   mWaveCache = new WaveCache(1);
   mSpecCache = new SpecCache(1, 1, false);
   mAppendBuffer = NULL;
   mAppendBufferLen = 0;
   mDirty = 0;
}

WaveClip::WaveClip(WaveClip& orig, DirManager *projDirManager)
{
   // essentially a copy constructor - but you must pass in the
   // current project's DirManager, because we might be copying
   // from one project to another

   mOffset = orig.mOffset;
   mRate = orig.mRate;
   mSequence = new Sequence(*orig.mSequence, projDirManager);
   mEnvelope = new Envelope();
   mEnvelope->Paste(0.0, orig.mEnvelope);
   mEnvelope->SetOffset(orig.GetOffset());
   mEnvelope->SetTrackLen(orig.mSequence->GetNumSamples() / orig.mRate);
   mWaveCache = new WaveCache(1);
   mSpecCache = new SpecCache(1, 1, false);

   for (WaveClipList::Node* it=orig.mCutLines.GetFirst(); it; it=it->GetNext())
      mCutLines.Append(new WaveClip(*it->GetData(), projDirManager));

   mAppendBuffer = NULL;
   mAppendBufferLen = 0;
   mDirty = 0;
}

WaveClip::~WaveClip()
{
   delete mSequence;
   delete mEnvelope;
   delete mWaveCache;
   delete mSpecCache;

   if (mAppendBuffer)
      DeleteSamples(mAppendBuffer);

   mCutLines.DeleteContents(true);
   mCutLines.Clear();
}

void WaveClip::SetOffset(double offset)
{
    mOffset = offset;
    mEnvelope->SetOffset(mOffset);
}

bool WaveClip::GetSamples(samplePtr buffer, sampleFormat format,
                   longSampleCount start, sampleCount len) const
{
   return mSequence->Get(buffer, format, start, len);
}

bool WaveClip::SetSamples(samplePtr buffer, sampleFormat format,
                   longSampleCount start, sampleCount len)
{
   bool bResult = mSequence->Set(buffer, format, start, len);
   MarkChanged();
   return bResult;
}

double WaveClip::GetStartTime() const
{
   // JS: mOffset is the minimum value and it is returned; no clipping to 0
   return mOffset;
}

double WaveClip::GetEndTime() const
{
   longSampleCount numSamples = mSequence->GetNumSamples();
   
   double maxLen = mOffset + double(numSamples+mAppendBufferLen)/mRate;
   // JS: calculated value is not the length;
   // it is a maximum value and can be negative; no clipping to 0
   
   return maxLen;
}

longSampleCount WaveClip::GetStartSample() const
{
   return (longSampleCount)floor(mOffset * mRate + 0.5);
}

longSampleCount WaveClip::GetEndSample() const
{
   return GetStartSample() + mSequence->GetNumSamples();
}

//
// Getting high-level data from the track for screen display and
// clipping calculations
//

bool WaveClip::GetWaveDisplay(float *min, float *max, float *rms,
                               sampleCount *where,
                               int numPixels, double t0,
                               double pixelsPerSecond)
{
   if (mWaveCache &&
       mWaveCache->dirty == mDirty &&
       mWaveCache->start == t0 &&
       mWaveCache->len >= numPixels &&
       mWaveCache->pps == pixelsPerSecond) {

      memcpy(min, mWaveCache->min, numPixels*sizeof(float));
      memcpy(max, mWaveCache->max, numPixels*sizeof(float));
      memcpy(rms, mWaveCache->rms, numPixels*sizeof(float));
      memcpy(where, mWaveCache->where, (numPixels+1)*sizeof(sampleCount));
      return true;
   }

   WaveCache *oldCache = mWaveCache;

   mWaveCache = new WaveCache(numPixels);
   mWaveCache->pps = pixelsPerSecond;
   mWaveCache->start = t0;
   double tstep = 1.0 / pixelsPerSecond;

   sampleCount x;

   for (x = 0; x < mWaveCache->len + 1; x++) {
      mWaveCache->where[x] =
         (sampleCount) floor(t0 * mRate +
                             ((double) x) * mRate * tstep + 0.5);
   }

   sampleCount s0 = mWaveCache->where[0];
   sampleCount s1 = mWaveCache->where[mWaveCache->len];
   int p0 = 0;
   int p1 = mWaveCache->len;

   // Optimization: if the old cache is good and overlaps
   // with the current one, re-use as much of the cache as
   // possible
   if (oldCache->dirty == mDirty &&
       oldCache->pps == pixelsPerSecond &&
       oldCache->where[0] < mWaveCache->where[mWaveCache->len] &&
       oldCache->where[oldCache->len] > mWaveCache->where[0]) {

      s0 = mWaveCache->where[mWaveCache->len];
      s1 = mWaveCache->where[0];
      p0 = mWaveCache->len;
      p1 = 0;

      for (x = 0; x < mWaveCache->len; x++)

         if (mWaveCache->where[x] >= oldCache->where[0] &&
             mWaveCache->where[x] <= oldCache->where[oldCache->len - 1]) {

            int ox =
                int ((double (oldCache->len) *
                      (mWaveCache->where[x] -
                       oldCache->where[0])) /(oldCache->where[oldCache->len] -
                                             oldCache->where[0]) + 0.5);

            mWaveCache->min[x] = oldCache->min[ox];
            mWaveCache->max[x] = oldCache->max[ox];
            mWaveCache->rms[x] = oldCache->rms[ox];
         } else {
            if (mWaveCache->where[x] < s0) {
               s0 = mWaveCache->where[x];
               p0 = x;
            }
            if (mWaveCache->where[x + 1] > s1) {
               s1 = mWaveCache->where[x + 1];
               p1 = x + 1;
            }
         }
   }

   if (p1 > p0) {

      /* handle values in the append buffer */

      int numSamples = mSequence->GetNumSamples();
      int a;

      for(a=p0; a<p1; a++)
         if (mWaveCache->where[a+1] > numSamples)
            break;

      if (a < p1) {
         int i;

         sampleFormat seqFormat = mSequence->GetSampleFormat();

         for(i=a; i<p1; i++) {
            sampleCount left = mWaveCache->where[i] - numSamples;
            sampleCount right = mWaveCache->where[i+1] - numSamples;

            //wxCriticalSectionLocker locker(mAppendCriticalSection);

            if (left < 0)
               left = 0;
            if (right > mAppendBufferLen)
               right = mAppendBufferLen;

            if (right > left) {
               float *b;
               sampleCount len = right-left;
               sampleCount j;

               if (seqFormat == floatSample)
                  b = &((float *)mAppendBuffer)[left];
               else {
                  b = new float[len];
                  CopySamples(mAppendBuffer + left*SAMPLE_SIZE(seqFormat),
                              seqFormat,
                              (samplePtr)b, floatSample, len);
               }

               float max = b[0];
               float min = b[0];
               float sumsq = b[0] * b[0];

               for(j=1; j<len; j++) {
                  if (b[j] > max)
                     max = b[j];
                  if (b[j] < min)
                     min = b[j];
                  sumsq += b[j]*b[j];
               }

               mWaveCache->min[i] = min;
               mWaveCache->max[i] = max;
               mWaveCache->rms[i] = (float)sqrt(sumsq / len);

               if (seqFormat != floatSample)
                  delete[] b;
            }
         }         

         // So that the sequence doesn't try to write any
         // of these values
         p1 = a;
      }

      if (p1 > p0) {
         if (!mSequence->GetWaveDisplay(&mWaveCache->min[p0],
                                        &mWaveCache->max[p0],
                                        &mWaveCache->rms[p0],
                                        p1-p0,
                                        &mWaveCache->where[p0],
                                        mRate / pixelsPerSecond))
            return false;
      }
   }

   mWaveCache->dirty = mDirty;
   delete oldCache;

   memcpy(min, mWaveCache->min, numPixels*sizeof(float));
   memcpy(max, mWaveCache->max, numPixels*sizeof(float));
   memcpy(rms, mWaveCache->rms, numPixels*sizeof(float));
   memcpy(where, mWaveCache->where, (numPixels+1)*sizeof(sampleCount));

   return true;
}

bool WaveClip::GetSpectrogram(float *freq, sampleCount *where,
                               int numPixels, int height,
                               double t0, double pixelsPerSecond,
                               bool autocorrelation)
{
   int maxFreqPref = gPrefs->Read(wxT("/Spectrum/MaxFreq"), 8000);
   int windowSize = gPrefs->Read(wxT("/Spectrum/FFTSize"), 256);
   
   if (mSpecCache &&
       mSpecCache->maxFreqPrefOld == maxFreqPref &&
       mSpecCache->windowSizeOld == windowSize &&
       mSpecCache->dirty == mDirty &&
       mSpecCache->start == t0 &&
       mSpecCache->ac == autocorrelation &&
       mSpecCache->height == height &&
       mSpecCache->len >= numPixels &&
       mSpecCache->pps == pixelsPerSecond) {
      memcpy(freq, mSpecCache->freq, numPixels*height*sizeof(float));
      memcpy(where, mSpecCache->where, (numPixels+1)*sizeof(sampleCount));
      return true;
   }

   SpecCache *oldCache = mSpecCache;

   mSpecCache = new SpecCache(numPixels, height, autocorrelation);
   mSpecCache->pps = pixelsPerSecond;
   mSpecCache->start = t0;

   sampleCount x;

   bool *recalc = new bool[mSpecCache->len + 1];

   for (x = 0; x < mSpecCache->len + 1; x++) {
      recalc[x] = true;
      // purposely offset the display 1/2 bin to the left (as compared
      // to waveform display to properly center response of the FFT
      mSpecCache->where[x] =
         (sampleCount)floor((t0*mRate) + (x*mRate/pixelsPerSecond) + 1.);
   }

   // Optimization: if the old cache is good and overlaps
   // with the current one, re-use as much of the cache as
   // possible
   if (oldCache->dirty == mDirty &&
       oldCache->maxFreqPrefOld == maxFreqPref &&
       oldCache->windowSizeOld == windowSize &&
       oldCache->pps == pixelsPerSecond &&
       oldCache->height == height &&
       oldCache->ac == autocorrelation &&
       oldCache->where[0] < mSpecCache->where[mSpecCache->len] &&
       oldCache->where[oldCache->len] > mSpecCache->where[0]) {

      for (x = 0; x < mSpecCache->len; x++)
         if (mSpecCache->where[x] >= oldCache->where[0] &&
             mSpecCache->where[x] <= oldCache->where[oldCache->len]) {

            int ox = (int) ((double (oldCache->len) *
                      (mSpecCache->where[x] - oldCache->where[0]))
                       / (oldCache->where[oldCache->len] -
                                             oldCache->where[0]) + 0.5);
            if (ox >= 0 && ox < oldCache->len &&
                mSpecCache->where[x] == oldCache->where[ox]) {

               for (sampleCount i = 0; i < (sampleCount)height; i++)
                  mSpecCache->freq[height * x + i] =
                     oldCache->freq[height * ox + i];

               recalc[x] = false;
            }
         }
   }

   float *buffer = new float[windowSize];
   mSpecCache->maxFreqPrefOld = maxFreqPref;
   mSpecCache->windowSizeOld = windowSize;

   for (x = 0; x < mSpecCache->len; x++)
      if (recalc[x]) {

         sampleCount start = mSpecCache->where[x];
         sampleCount len = windowSize;
         sampleCount i;

         if (start <= 0 || start >= mSequence->GetNumSamples()) {

            for (i = 0; i < (sampleCount)height; i++)
               mSpecCache->freq[height * x + i] = 0;

         } else {
         
            float *adj = buffer;
            start -= windowSize >> 1;
            
            if (start < 0) {
               for (i = start; i < 0; i++)
                  *adj++ = 0;
               len += start;
               start = 0;
            }

            if (start + len > mSequence->GetNumSamples()) {
               len = mSequence->GetNumSamples() - start;
               for (i = len; i < (sampleCount)windowSize; i++)
                  adj[i] = 0;
            }

            if (len > 0)
               mSequence->Get((samplePtr)adj, floatSample, start, len);

            ComputeSpectrum(buffer, windowSize, height,
                            maxFreqPref, windowSize,
                            mRate, &mSpecCache->freq[height * x],
                            autocorrelation);
         }
      }

   delete[]buffer;
   delete[]recalc;
   delete oldCache;

   mSpecCache->dirty = mDirty;
   memcpy(freq, mSpecCache->freq, numPixels*height*sizeof(float));
   memcpy(where, mSpecCache->where, (numPixels+1)*sizeof(sampleCount));

   return true;
}

bool WaveClip::GetMinMax(float *min, float *max,
                          double t0, double t1)
{
   *min = float(0.0);
   *max = float(0.0);

   if (t0 > t1)
      return false;

   if (t0 == t1)
      return true;

   longSampleCount s0, s1;

   TimeToSamplesClip(t0, &s0);
   TimeToSamplesClip(t1, &s1);

   return mSequence->GetMinMax(s0, s1-s0, min, max);
}

void WaveClip::ConvertToSampleFormat(sampleFormat format)
{
   bool result;
   result = mSequence->ConvertToSampleFormat(format);
   MarkChanged();
   wxASSERT(result);
}

void WaveClip::UpdateEnvelopeTrackLen()
{
   mEnvelope->SetTrackLen(((double)mSequence->GetNumSamples()) / mRate);
}

void WaveClip::TimeToSamplesClip(double t0, longSampleCount *s0) const
{
   if (t0 < mOffset)
      *s0 = 0;
   else if (t0 > mOffset + double(mSequence->GetNumSamples())/mRate)
      *s0 = mSequence->GetNumSamples();
   else
      *s0 = (longSampleCount)floor(((t0 - mOffset) * mRate) + 0.5);
}

void WaveClip::ClearDisplayRect()
{
   mDisplayRect.x = mDisplayRect.y = -1;
   mDisplayRect.width = mDisplayRect.height = -1;
}

void WaveClip::SetDisplayRect(const wxRect& r)
{
   mDisplayRect = r;
}

void WaveClip::GetDisplayRect(wxRect* r)
{
   *r = mDisplayRect;
}

bool WaveClip::Append(samplePtr buffer, sampleFormat format,
                      sampleCount len, unsigned int stride /* = 1 */)
{
   //wxLogDebug(wxT("Append: len=%i\n"), len);
   
   sampleCount maxBlockSize = mSequence->GetMaxBlockSize();
   sampleCount blockSize = mSequence->GetIdealAppendLen();
   sampleFormat seqFormat = mSequence->GetSampleFormat();

   if (!mAppendBuffer)
      mAppendBuffer = NewSamples(maxBlockSize, seqFormat);

   for(;;) {
      if (mAppendBufferLen >= blockSize) {
         bool success =
            mSequence->Append(mAppendBuffer, seqFormat, blockSize);
         if (!success)
            return false;
         memmove(mAppendBuffer,
                 mAppendBuffer + blockSize * SAMPLE_SIZE(seqFormat),
                 (mAppendBufferLen - blockSize) * SAMPLE_SIZE(seqFormat));
         mAppendBufferLen -= blockSize;
         blockSize = mSequence->GetIdealAppendLen();
      }

      if (len == 0)
         break;

      int toCopy = maxBlockSize - mAppendBufferLen;
      if (toCopy > len)
         toCopy = len;

      CopySamples(buffer, format,
                  mAppendBuffer + mAppendBufferLen * SAMPLE_SIZE(seqFormat),
                  seqFormat,
                  toCopy,
                  true, // high quality
                  stride);

      mAppendBufferLen += toCopy;
      buffer += toCopy * SAMPLE_SIZE(format) * stride;
      len -= toCopy;
   }

   UpdateEnvelopeTrackLen();
   MarkChanged();

   return true;
}

bool WaveClip::AppendAlias(wxString fName, sampleCount start,
                            sampleCount len, int channel)
{
   bool result = mSequence->AppendAlias(fName, start, len, channel);
   if (result)
   {
      UpdateEnvelopeTrackLen();
      MarkChanged();
   }
   return result;
}

bool WaveClip::Flush()
{
   //wxLogDebug(wxT("Flush!"));

   bool success = true;
   sampleFormat seqFormat = mSequence->GetSampleFormat();

   //wxLogDebug(wxT("mAppendBufferLen=%i\n"), mAppendBufferLen);
   //wxLogDebug(wxT("previous sample count %i\n"), mSequence->GetNumSamples());

   if (mAppendBufferLen > 0) {
      success = mSequence->Append(mAppendBuffer, seqFormat, mAppendBufferLen);
      if (success) {
         mAppendBufferLen = 0;
         UpdateEnvelopeTrackLen();
         MarkChanged();
      }
   }

   //wxLogDebug(wxT("now sample count %i\n"), mSequence->GetNumSamples());

   return success;
}

bool WaveClip::HandleXMLTag(const wxChar *tag, const wxChar **attrs)
{
   if (!wxStrcmp(tag, wxT("waveclip")))
   {
      while(*attrs) {
         const wxChar *attr = *attrs++;
         const wxChar *value = *attrs++;
         
         if (!value)
            break;
         
         if (!wxStrcmp(attr, wxT("offset")))
            Internat::CompatibleToDouble(wxString(value), &mOffset);
         
      }
      return true;
   }

   return false;
}

void WaveClip::HandleXMLEndTag(const wxChar *tag)
{
   if (!wxStrcmp(tag, wxT("waveclip")))
      UpdateEnvelopeTrackLen();
}

XMLTagHandler *WaveClip::HandleXMLChild(const wxChar *tag)
{
   if (!wxStrcmp(tag, wxT("sequence")))
      return mSequence;
   else if (!wxStrcmp(tag, wxT("envelope")))
      return mEnvelope;
   else if (!wxStrcmp(tag, wxT("waveclip")))
   {
      // Nested wave clips are cut lines
      WaveClip *newCutLine = new WaveClip(mSequence->GetDirManager(),
                                mSequence->GetSampleFormat(), mRate);
      mCutLines.Append(newCutLine);
      return newCutLine;
   } else
      return NULL;
}

void WaveClip::WriteXML(int depth, FILE *fp)
{
   int i;

   for(i=0; i<depth; i++)
      fprintf(fp, "\t");
   fprintf(fp, "<waveclip ");
   fprintf(fp, "offset=\"%s\" ", (const char *)Internat::ToString(mOffset, 8).mb_str());
   fprintf(fp, ">\n");

   mSequence->WriteXML(depth+1, fp);
   mEnvelope->WriteXML(depth+1, fp);

   for (WaveClipList::Node* it=mCutLines.GetFirst(); it; it=it->GetNext())
      it->GetData()->WriteXML(depth+1, fp);

   for(i=0; i<depth; i++)
      fprintf(fp, "\t");
   fprintf(fp, "</waveclip>\n");
}

bool WaveClip::CreateFromCopy(double t0, double t1, WaveClip* other)
{
   longSampleCount s0, s1;

   other->TimeToSamplesClip(t0, &s0);
   other->TimeToSamplesClip(t1, &s1);

   Sequence* oldSequence = mSequence;
   mSequence = NULL;
   if (!other->mSequence->Copy(s0, s1, &mSequence))
   {
      mSequence = oldSequence;
      return false;
   }

   delete oldSequence;
   delete mEnvelope;
   mEnvelope = new Envelope();
   mEnvelope->CopyFrom(other->mEnvelope, t0, t1);

   MarkChanged();

   return true;
}

bool WaveClip::Paste(double t0, WaveClip* other)
{
   longSampleCount s0;
   TimeToSamplesClip(t0, &s0);

   if (mSequence->Paste(s0, other->mSequence))
   {
      MarkChanged();
      mEnvelope->Paste(t0, other->mEnvelope);
      OffsetCutLines(t0, other->GetEndTime()-other->GetStartTime());
      
      // Paste cut lines contained in pasted clip
      for (WaveClipList::Node* it=other->mCutLines.GetFirst(); it; it=it->GetNext())
      {
         WaveClip* cutline = it->GetData();
         WaveClip* newCutLine = new WaveClip(*cutline,
                                             mSequence->GetDirManager());
         newCutLine->Offset(t0 - mOffset);
         mCutLines.Append(newCutLine);
      }
      
      return true;
   }

   return false;
}

bool WaveClip::InsertSilence(double t, double len)
{
   longSampleCount s0;
   TimeToSamplesClip(t, &s0);
   sampleCount slen = (sampleCount)floor(len * mRate + 0.5);
   
   if (!GetSequence()->InsertSilence(s0, slen))
   {
      wxASSERT(false);
      return false;
   }
   OffsetCutLines(t, len);
   GetEnvelope()->InsertSpace(t, len);
   MarkChanged();
   
   return true;
}

bool WaveClip::Clear(double t0, double t1)
{
   longSampleCount s0, s1;
   
   TimeToSamplesClip(t0, &s0);
   TimeToSamplesClip(t1, &s1);
   
   if (GetSequence()->Delete(s0, s1-s0))
   {
      GetEnvelope()->CollapseRegion(t0, t1);
      if (t0 < GetStartTime())
         Offset(-(GetStartTime() - t0));

      MarkChanged();
      return true;
   }

   return false;
}

bool WaveClip::ClearAndAddCutLine(double t0, double t1)
{
   if (t0 > GetEndTime() || t1 < GetStartTime())
      return true; // time out of bounds
      
   WaveClip *newClip = new WaveClip(mSequence->GetDirManager(),
                                    mSequence->GetSampleFormat(),
                                    mRate);
   double clip_t0 = t0;
   double clip_t1 = t1;
   if (clip_t0 < GetStartTime())
      clip_t0 = GetStartTime();
   if (clip_t1 > GetEndTime())
      clip_t1 = GetEndTime();

   if (!newClip->CreateFromCopy(clip_t0, clip_t1, this))
      return false;
   newClip->SetOffset(clip_t0-mOffset);

   // Sort out cutlines that belong to the new cutline
   WaveClipList::Node* nextIt = (WaveClipList::Node*)-1;

   for (WaveClipList::Node* it = mCutLines.GetFirst(); it; it=nextIt)
   {
      nextIt = it->GetNext();
      WaveClip* clip = it->GetData();
      double cutlinePosition = mOffset + clip->GetOffset();
      if (cutlinePosition >= t0 && cutlinePosition <= t1)
      {
         clip->SetOffset(cutlinePosition - newClip->GetOffset() - mOffset);
         newClip->mCutLines.Append(clip);
         mCutLines.DeleteNode(it);
      } else
      if (cutlinePosition >= t1)
      {
         clip->Offset(clip_t0-clip_t1);
      }
   }

   if (Clear(t0, t1))
   {
      mCutLines.Append(newClip);
      return true;
   }

   delete newClip;
   return false;
}

bool WaveClip::ExpandCutLine(double cutLinePosition, double* cutlineStart, double* cutlineEnd)
{
   for (WaveClipList::Node* it = mCutLines.GetFirst(); it; it=it->GetNext())
   {
      WaveClip* cutline = it->GetData();
      if (fabs(mOffset + cutline->GetOffset() - cutLinePosition) < 0.0001)
      {
         if (cutlineStart)
            *cutlineStart = mOffset+cutline->GetStartTime();
         if (cutlineEnd)
            *cutlineEnd = mOffset+cutline->GetEndTime();
         if (!Paste(mOffset+cutline->GetOffset(), cutline))
            return false;
         delete cutline;
         mCutLines.DeleteNode(it);
         return true;
      }
   }

   return false;
}

bool WaveClip::RemoveCutLine(double cutLinePosition)
{
   for (WaveClipList::Node* it = mCutLines.GetFirst(); it; it=it->GetNext())
   {
      if (fabs(mOffset + it->GetData()->GetOffset() - cutLinePosition) < 0.0001)
      {
         delete it->GetData();
         mCutLines.DeleteNode(it);
         return true;
      }
   }

   return false;
}

void WaveClip::RemoveAllCutLines()
{
   while (!mCutLines.IsEmpty())
   {
      WaveClipList::Node* head = mCutLines.GetFirst();
      delete head->GetData();
      mCutLines.DeleteNode(head);
   }
}

void WaveClip::OffsetCutLines(double t0, double len)
{
   for (WaveClipList::Node* it = mCutLines.GetFirst(); it; it=it->GetNext())
   {
      WaveClip* cutLine = it->GetData();
      if (mOffset + cutLine->GetOffset() >= t0)
         cutLine->Offset(len);
   }
}
