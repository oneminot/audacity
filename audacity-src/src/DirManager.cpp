/**********************************************************************

  Audacity: A Digital Audio Editor

  DirManager.cpp

  Dominic Mazzoni
  Monty

**********************************************************************/

#include <wx/defs.h>
#include <wx/app.h>
#include <wx/dir.h>
#include <wx/log.h>
#include <wx/filefn.h>
#include <wx/hash.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/timer.h>
#include <wx/intl.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/object.h>

// chmod
#ifdef __UNIX__
#include <sys/types.h>
#include <sys/stat.h>
#endif

// seed random number generator
#include <time.h>

#include "Audacity.h"
#include "AudacityApp.h"
#include "BlockFile.h"
#include "blockfile/LegacyBlockFile.h"
#include "blockfile/LegacyAliasBlockFile.h"
#include "blockfile/SimpleBlockFile.h"
#include "blockfile/SilentBlockFile.h"
#include "blockfile/PCMAliasBlockFile.h"
#include "DirManager.h"
#include "Internat.h"
#include "Prefs.h"
#include "widgets/Warning.h"
#include "widgets/MultiDialog.h"

#include "prefs/PrefsDialog.h"

// Static class variables

int DirManager::numDirManagers = 0;
bool DirManager::dontDeleteTempFiles = false;

wxString DirManager::globaltemp;

// Methods

DirManager::DirManager()
{
   wxLogDebug(wxT("DirManager: Created new instance"));

   mRef = 1; // MM: Initial refcount is 1 by convention
   
   // set up local temp subdir
   wxString numstring;
   numstring.Printf(wxT("project%d"),numDirManagers);
   mytemp=globaltemp + wxFILE_SEP_PATH + numstring;

   numDirManagers++;

   projPath = wxT("");
   projName = wxT("");

   mLoadingTarget = NULL;

   // toplevel pool hash is fully populated to begin 
   {
      int i;
      for(i=0; i< 256; i++) dirTopPool[i]=0;
   }

   // Make sure there is plenty of space for temp files
   wxLongLong freeSpace = 0;

   {
      //create a temporary null log to capture the log dialog
      //that wxGetDiskSpace creates.  It gets destroyed when
      //it goes out of context.
		//JKC: Please explain why.
      wxLogNull logNo;
      if (wxGetDiskSpace(globaltemp, NULL, &freeSpace)) {
         if (freeSpace < 1048576) {
            ShowWarningDialog(NULL, wxT("DiskSpaceWarning"),
                              _("Warning: there is very little free disk space left on this volume.\nPlease select another temporary directory in your preferences."));
         }
      }
   }
   // this need not be strictly uniform or random, but it should give
   // unclustered numbers
   srand(time(0));
}

DirManager::~DirManager()
{
   wxASSERT(mRef == 0); // MM: Otherwise, we shouldn't delete it

   numDirManagers--;
   if (numDirManagers == 0) {
      CleanTempDir(false);
      //::wxRmdir(temp);
   }
}

// behavior of dash_rf_enum is tailored to our two uses and thus not
// entirely strightforward.  It recurses depth-first from the passed
// in directory into its subdirs according to optional dirspec
// matching, building a list of directories and [optionally] files to
// be rm()ed in the listed order.  The dirspec is not applied to
// subdirs of subdirs. Files in the passed in directory will not be
// enumerated.  Also, the passed-in directory is the last entry added
// to the list.

static int rm_dash_rf_enumerate_i(wxString dirpath, 
                                  wxStringList *flist, 
                                  wxString dirspec,
                                  int files_p,int dirs_p,
                                  int progress_count,int progress_bias,
                                  const wxChar *prompt, wxProgressDialog **progress){

   int count=0;
   bool cont;

   wxDir dir(dirpath);
   if(dir.IsOpened()){
      wxString name;

      if(files_p){
         cont= dir.GetFirst(&name, dirspec, wxDIR_FILES);
         while ( cont ){
            wxString filepath=dirpath + wxFILE_SEP_PATH + name;
            
            count++;
            flist->Add(filepath);
            
            cont = dir.GetNext(&name);
            
            if (prompt && progress && !*progress && wxGetElapsedTime(false) > 500)
               *progress =
                  new wxProgressDialog(_("Progress"),
                                       prompt,
                                       1000,
                                       NULL,
                                       wxPD_REMAINING_TIME | wxPD_AUTO_HIDE);

            if (progress && *progress)
               (*progress)->Update(int ((count+progress_bias * 1000.0) / 
                                        progress_count));

         }
      }

      cont= dir.GetFirst(&name, dirspec, wxDIR_DIRS);
      while ( cont ){
         wxString subdirpath=dirpath + wxFILE_SEP_PATH + name;
         count+=rm_dash_rf_enumerate_i(subdirpath,flist,wxEmptyString,
                                     files_p,dirs_p,progress_count,
                                     count+progress_bias,prompt,progress);  
         cont = dir.GetNext(&name);
      }
   }
   
   if(dirs_p){
      flist->Add(dirpath);
      count++;
   }

   return count;
}


static int rm_dash_rf_enumerate_prompt(wxString dirpath,
                                       wxStringList *flist, 
                                       wxString dirspec,
                                       int files_p,int dirs_p,
                                       int progress_count,
                                       const wxChar *prompt){

   wxProgressDialog *progress = NULL;
   wxStartTimer();

   int count=rm_dash_rf_enumerate_i(dirpath, flist, dirspec, files_p,dirs_p,
                                    progress_count,0,
                                    prompt, &progress);
   
   if (progress)
      delete progress;

   return count;
}

static int rm_dash_rf_enumerate(wxString dirpath,
                                wxStringList *flist, 
                                wxString dirspec,
                                int files_p,int dirs_p){

   return rm_dash_rf_enumerate_i(dirpath, flist, dirspec, files_p,dirs_p,
                                    0,0,0,0);

}


static void rm_dash_rf_execute(wxStringList fnameList, 
                               int count, int files_p, int dirs_p,
                               const wxChar *prompt){

   wxChar **array = fnameList.ListToArray();   
   wxProgressDialog *progress = NULL;
   wxStartTimer();

   for (int i = 0; i < count; i++) {
      wxChar *file = array[i];
      if(files_p){
         wxRemoveFile(file);
      }
      if(dirs_p){
         wxRmdir(file);
      }

      if (prompt && !progress && wxGetElapsedTime(false) > 500)
         progress =
            new wxProgressDialog(_("Progress"),
                                 prompt,
                                 1000,
                                 NULL,
                                 wxPD_REMAINING_TIME | wxPD_AUTO_HIDE);
      
      if (progress)
         progress->Update(int ((i * 1000.0) / count));
   }
   
   if (progress)
      delete progress;
   
   delete [] array;
}

// static
void DirManager::CleanTempDir(bool startup)
{
   wxStringList fnameList;
   int count = 0;

   if (dontDeleteTempFiles)
      return;

   // don't count the global temp directory, which this will find and
   // list last
   count=rm_dash_rf_enumerate(globaltemp,&fnameList,wxT("project*"),1,1)-1;
   
   if (count == 0) 
      return;

   if (startup) {
//lda ... CleanSpeech not that concerned about temp files ... allow end-users to ignore this warning
      int warn = gPrefs->Read(wxT("/GUI/WarnAboutTempFiles"), true);
      int action = wxYES;
      if (warn == 1) {
         wxString prompt = _("Audacity found temporary files that were not deleted or saved \nthe last time you used Audacity. \n\nAudacity can't recover them automatically, but if you choose not \nto delete them, you can recover them manually. \n\nDelete temporary files?");
      
         action = wxMessageBox(prompt,
                               wxT("Warning"),
                               wxYES_NO | wxICON_EXCLAMATION,
                               NULL);
      }
      if (action != wxYES) {
         dontDeleteTempFiles = true;
         return;
      }
   }

   rm_dash_rf_execute(fnameList,count,1,1,_("Cleaning up temporary files"));
}

bool DirManager::SetProject(wxString & projPath, wxString & projName,
                            bool create)
{
   wxString oldPath = this->projPath;
   wxString oldName = this->projName;
   wxString oldFull = projFull;
   wxString oldLoc = projFull;
   if (oldLoc == wxT(""))
      oldLoc = mytemp;

   if (projPath == wxT(""))
      projPath = FROMFILENAME(::wxGetCwd());

   this->projPath = projPath;
   this->projName = projName;
   this->projFull = projPath + wxFILE_SEP_PATH + projName;

   wxString cleanupLoc1=oldLoc;
   wxString cleanupLoc2=projFull;

   if (create) {
      if (!wxDirExists(FILENAME(projFull)))
         if (!wxMkdir(FILENAME(projFull)))
            return false;

      #ifdef __UNIX__
      chmod(FILENAME(projFull).fn_str(), 0775);
      #endif

   } else {
      #ifndef __WXMAC__
      if (!wxDirExists(FILENAME(projFull)))
         return false;
      #endif
   }

   /* Move all files into this new directory.  Files which are
      "locked" get copied instead of moved.  (This happens when
      we perform a Save As - the files which belonged to the last
      saved version of the old project must not be moved,
      otherwise the old project would not be safe. */

   wxProgressDialog *progress = NULL;
   int total=blockFileHash.size();
   int count=0;
   wxStartTimer();

   BlockHash::iterator i=blockFileHash.begin();
   bool success = true;
   while(i != blockFileHash.end() && success) {
      BlockFile *b = i->second;
      
      if (b->IsLocked())
         success = CopyToNewProjectDirectory(b);
      else{
         success = MoveToNewProjectDirectory(b);
      }


      if (!progress && wxGetElapsedTime(false) > 500)
         progress =
            new wxProgressDialog(_("Progress"),
                                 _("Saving project data files"),
                                 1000,
                                 NULL,
                                 wxPD_REMAINING_TIME | 
                                 wxPD_AUTO_HIDE);
      
      if (progress)
         progress->Update(int ((count * 1000.0) / total));

      i++;
      count++;
   }

   if (!success) {
      // If the move failed, we try to move/copy as many files
      // back as possible so that no damage was done.  (No sense
      // in checking for errors this time around - there's nothing
      // that could be done about it.)
      // Likely causes: directory was not writeable, disk was full

      projFull = oldLoc;

      BlockHash::iterator i=blockFileHash.begin();
      while(i != blockFileHash.end()) {
         BlockFile *b = i->second;
         MoveToNewProjectDirectory(b);

         if (progress && count>=0)
            progress->Update(int ((count * 1000.0) / total));

         i++;
         count--;
      }

      this->projFull = oldFull;
      this->projPath = oldPath;
      this->projName = oldName;

      if (progress)
         delete progress;

      return false;
   }

   if (progress)
      delete progress;

   // Some subtlety; SetProject is used both to move a temp project
   // into a permanent home as well as just set up path variables when
   // loading a project; in this latter case, the movement code does
   // nothing because SetProject is called before there are any
   // blockfiles.  Cleanup code trigger is the same
   if(blockFileHash.size()>0){
      // Clean up after ourselves; look for empty directories in the old
      // and new project directories.  The easiest way to do this is to
      // recurse depth-first and rmdir every directory seen in old and
      // new; rmdir will fail on non-empty dirs.
      
      wxStringList dirlist;
      count=rm_dash_rf_enumerate(cleanupLoc1,&dirlist,wxEmptyString,0,1);
      count+=rm_dash_rf_enumerate(cleanupLoc2,&dirlist,wxEmptyString,0,1);
      
      if(count)
         rm_dash_rf_execute(dirlist,count,0,1,_("Cleaning up cache directories"));
   }
   return true;
}

wxString DirManager::GetProjectName()
{
   return projName;
}

wxLongLong DirManager::GetFreeDiskSpace()
{
   wxLongLong freeSpace = -1;
   wxString path = projPath;

   if (projPath == wxT(""))
      path = mytemp;
   {
      wxLogNull logNo;

      if (!wxGetDiskSpace(FILENAME(path), NULL, &freeSpace))
         freeSpace = -1;
   }
   return freeSpace;
}

wxFileName DirManager::MakeBlockFilePath(wxString value){
   
   wxFileName dir;
   dir.AssignDir(projFull != wxT("")? projFull: mytemp);
   
   if(value.GetChar(0)==wxT('d')){
      // this file is located in a subdiretory tree 
      int location=value.Find(wxT('b'));
      wxString subdir=value.Mid(0,location);
      dir.AppendDir(subdir);
      
      if(!dir.DirExists())dir.Mkdir();
   }
   
   if(value.GetChar(0)==wxT('e')){
      // this file is located in a new style two-deep subdirectory tree 
      wxString topdir=value.Mid(0,3);
      wxString middir=wxT("d");
      middir.Append(value.Mid(3,2));
      
      dir.AppendDir(topdir);
      dir.AppendDir(middir);
      if(!dir.DirExists())dir.Mkdir(0777,wxPATH_MKDIR_FULL);
   }
   return dir;
}

bool DirManager::AssignFile(wxFileName &fileName,
			    wxString value,
                            bool diskcheck){
   wxFileName dir=MakeBlockFilePath(value);

   if(diskcheck){
      // verify that there's no possible collision on disk.  If there
      // is, log the problem and return FALSE so that MakeBlockFileName 
      // can try again

      wxDir checkit(dir.GetFullPath());
      if(!checkit.IsOpened()) return FALSE;
      
      // this code is only valid if 'value' has no extention; that
      // means, effectively, AssignFile may be called with 'diskcheck'
      // set to true only if called from MakeFileBlockName().
      
      wxString filespec;
      filespec.Printf(wxT("%s.*"),value.c_str());
      if(checkit.HasFiles(filespec)){
         // collision with on-disk state!
         wxString collision;
         checkit.GetFirst(&collision,filespec);
         
         wxLogWarning(_("Audacity found an orphaned blockfile %s! \nPlease consider saving and reloading the project to perform a complete project check.\n"),
                      collision.c_str());
         
         return FALSE;
      }
   }
   fileName.Assign(dir.GetFullPath(),value);
   return TRUE;
}

static inline unsigned int hexchar_to_int(unsigned int x)
{
   if(x<48U)return 0;
   if(x<58U)return x-48U;
   if(x<65U)return 10U;
   if(x<71U)return x-55U;
   if(x<97U)return 10U;
   if(x<103U)return x-87U;
   return 15U;
}

int DirManager::BalanceMidAdd(int topnum, int midkey)
{
   // enter the midlevel directory if it doesn't exist

   if(dirMidPool.find(midkey) == dirMidPool.end() &&
         dirMidFull.find(midkey) == dirMidFull.end()){
      dirMidPool[midkey]=0;

      // increment toplevel directory fill
      dirTopPool[topnum]++;
      if(dirTopPool[topnum]>=256){
         // this toplevel is now full; move it to the full hash
         dirTopPool.erase(topnum);
         dirTopFull[topnum]=256;
      }
      return 1;
   }
   return 0;
}

void DirManager::BalanceFileAdd(int midkey)
{
   // increment the midlevel directory usage information
   if(dirMidPool.find(midkey) != dirMidPool.end()){
      dirMidPool[midkey]++;
      if(dirMidPool[midkey]>=256){
         // this middir is now full; move it to the full hash
         dirMidPool.erase(midkey);
         dirMidFull[midkey]=256;
      }
   }else{
      // this case only triggers in absurdly large projects; we still
      // need to track directory fill even if we're over 256/256/256
      dirMidPool[midkey]++;
   }
}

void DirManager::BalanceInfoAdd(wxString file)
{
   const wxChar *s=file.c_str();
   if(s[0]==wxT('e')){
      // this is one of the modern two-deep managed files 
      // convert filename to keys 
      unsigned int topnum = (hexchar_to_int(s[1]) << 4) | 
         hexchar_to_int(s[2]);
      unsigned int midnum = (hexchar_to_int(s[3]) << 4) | 
         hexchar_to_int(s[4]);
      unsigned int midkey=topnum<<8|midnum;

      BalanceMidAdd(topnum,midkey);
      BalanceFileAdd(midkey);
   }
}

// Note that this will try to clean up directories out from under even
// locked blockfiles; this is actually harmless as the rmdir will fail
// on non-empty directories.
void DirManager::BalanceInfoDel(wxString file)
{
   const wxChar *s=file.c_str();
   if(s[0]==wxT('e')){
      // this is one of the modern two-deep managed files 

      unsigned int topnum = (hexchar_to_int(s[1]) << 4) | 
         hexchar_to_int(s[2]);
      unsigned int midnum = (hexchar_to_int(s[3]) << 4) | 
         hexchar_to_int(s[4]);
      unsigned int midkey=topnum<<8|midnum;

      // look for midkey in the mid pool
      if(dirMidFull.find(midkey) != dirMidFull.end()){
         // in the full pool

         if(--dirMidFull[midkey]<256){
            // move out of full into available
            dirMidPool[midkey]=dirMidFull[midkey];
            dirMidFull.erase(midkey);
         }
      }else{
         if(--dirMidPool[midkey]<1){
            // erasing the key here is OK; we have provision to add it
            // back if its needed (unlike the dirTopPool hash)
            dirMidPool.erase(midkey);

            // delete the actual directory
            wxString dir=(projFull != wxT("")? projFull: mytemp);
            dir += wxFILE_SEP_PATH;
            dir += file.Mid(0,3);
            dir += wxFILE_SEP_PATH;
            dir += wxT("d");
            dir += file.Mid(3,2);
            wxFileName::Rmdir(dir);

            // also need to remove from toplevel
            if(dirTopFull.find(topnum) != dirTopFull.end()){
               // in the full pool
               if(--dirTopFull[topnum]<256){
                  // move out of full into available
                  dirTopPool[topnum]=dirTopFull[topnum];
                  dirTopFull.erase(topnum);
               }
            }else{
               if(--dirTopPool[topnum]<1){
                  // do *not* erase the hash entry from dirTopPool
                  // *do* delete the actual directory
                  wxString dir=(projFull != wxT("")? projFull: mytemp);
                  dir += wxFILE_SEP_PATH;
                  dir += file.Mid(0,3);
                  wxFileName::Rmdir(dir);
               }
            }
         }
      }
   }
}

// only determines appropriate filename and subdir balance; does not
// perform maintainence
wxFileName DirManager::MakeBlockFileName()
{
   wxFileName ret;
   wxString baseFileName;

   unsigned int filenum,midnum,topnum,midkey;

   while(1){

      /* blockfiles are divided up into heirarchical directories.
         Each toplevel directory is represented by "e" + two unique
         hexadecimal digits, for a total possible number of 256
         toplevels.  Each toplevel contains up to 256 subdirs named
         "d" + two hex digits.  Each subdir contains 'a number' of
         files.  */

      filenum=0;
      midnum=0;
      topnum=0;

      // first action: if there is no available two-level directory in
      // the available pool, try to make one

      if(dirMidPool.empty()){
         
         // is there a toplevel directory with space for a new subdir?

         if(!dirTopPool.empty()){

            // there's still a toplevel with room for a subdir

            DirHash::iterator i = dirTopPool.begin();
            int newcount        = 0;
            topnum              = i->first;
            

            // search for unused midlevels; linear search adequate
            // add 32 new topnum/midnum dirs full of  prospective filenames to midpool
            for(midnum=0;midnum<256;midnum++){
               midkey=(topnum<<8)+midnum;
               if(BalanceMidAdd(topnum,midkey)){
                  newcount++;
                  if(newcount>=32)break;
               }
            }

            if(dirMidPool.empty()){
               // all the midlevels in this toplevel are in use yet the
               // toplevel claims some are free; this implies multiple
               // internal logic faults, but simply giving up and going
               // into an infinite loop isn't acceptible.  Just in case,
               // for some reason, we get here, dynamite this toplevel so
               // we don't just fail.
               
               // this is 'wrong', but the best we can do given that
               // something else is also wrong.  It will contain the
               // problem so we can keep going without worry.
               dirTopPool.erase(topnum);
               dirTopFull[topnum]=256;
            }
            continue;
         }
      }

      if(dirMidPool.empty()){
         // still empty, thus an absurdly large project; all dirs are
         // full to 256/256/256; keep working, but fall back to 'big
         // filenames' and randomized placement

         filenum = rand();
         midnum  = (int)(256.*rand()/(RAND_MAX+1.));
         topnum  = (int)(256.*rand()/(RAND_MAX+1.));
         midkey=(topnum<<8)+midnum;

            
      }else{
         
         DirHash::iterator i = dirMidPool.begin();
         midkey              = i->first;

         // split the retrieved 16 bit directory key into two 8 bit numbers
         topnum = midkey >> 8;
         midnum = midkey & 0xff;
         filenum = (int)(4096.*rand()/(RAND_MAX+1.));

      }

      baseFileName.Printf(wxT("e%02x%02x%03x"),topnum,midnum,filenum);

      if(blockFileHash.find(baseFileName) == blockFileHash.end()){
         // not in the hash, good. 
         if(AssignFile(ret,baseFileName,TRUE)==FALSE){
            
            // this indicates an on-disk collision, likely due to an
            // orphaned blockfile.  We should try again, but first
            // alert the balancing info there's a phantom file here;
            // if the directory is nearly full of orphans we neither
            // want performance to suffer nor potentially get into an
            // infinite loop if all possible filenames are taken by
            // orphans (unlikely but possible)
            BalanceFileAdd(midkey);
 
         }else break;
      }
   }
   // FIX-ME: Might we get here without midkey having been set?
   BalanceFileAdd(midkey);
   return ret;
}

BlockFile *DirManager::NewSimpleBlockFile(
                                 samplePtr sampleData, sampleCount sampleLen,
                                 sampleFormat format)
{
   wxFileName fileName = MakeBlockFileName();

   BlockFile *newBlockFile =
       new SimpleBlockFile(fileName, sampleData, sampleLen, format);

   blockFileHash[fileName.GetName()]=newBlockFile;

   return newBlockFile;
}

BlockFile *DirManager::NewAliasBlockFile(
                                 wxString aliasedFile, sampleCount aliasStart,
                                 sampleCount aliasLen, int aliasChannel)
{
   wxFileName fileName = MakeBlockFileName();

   BlockFile *newBlockFile =
       new PCMAliasBlockFile(fileName,
                             aliasedFile, aliasStart, aliasLen, aliasChannel);

   blockFileHash[fileName.GetName()]=newBlockFile;
   aliasList.Add(aliasedFile);

   return newBlockFile;
}

// Adds one to the reference count of the block file,
// UNLESS it is "locked", then it makes a new copy of
// the BlockFile.
BlockFile *DirManager::CopyBlockFile(BlockFile *b)
{
   if (!b->IsLocked()) {
      b->Ref();
      return b;
   }

   wxFileName newFile = MakeBlockFileName();

   // We assume that the new file should have the same extension
   // as the existing file
   newFile.SetExt(b->GetFileName().GetExt());

   if( !wxCopyFile(FILENAME(b->GetFileName().GetFullPath()),
                   FILENAME(newFile.GetFullPath())) )
      return NULL;

   BlockFile *b2 = b->Copy(newFile);

   if (b2 == NULL)
      return NULL;

   blockFileHash[newFile.GetName()]=b2;
   aliasList.Add(newFile.GetFullPath());

   return b2;
}

bool DirManager::HandleXMLTag(const wxChar *tag, const wxChar **attrs)
{
   if( mLoadingTarget == NULL )
      return false;

   if( !wxStricmp(tag, wxT("silentblockfile")) ) {
      // Silent blocks don't actually have a file associated, so
      // we don't need to worry about the hash table at all
      *mLoadingTarget = SilentBlockFile::BuildFromXML(*this, attrs);
      return true;
   }
   else if ( !wxStricmp(tag, wxT("simpleblockfile")) )
      *mLoadingTarget = SimpleBlockFile::BuildFromXML(*this, attrs);
   else if( !wxStricmp(tag, wxT("pcmaliasblockfile")) )
      *mLoadingTarget = PCMAliasBlockFile::BuildFromXML(*this, attrs);
   else if( !wxStricmp(tag, wxT("blockfile")) ||
            !wxStricmp(tag, wxT("legacyblockfile")) ) {
      // Support Audacity version 1.1.1 project files

      int i=0;
      bool alias = false;

      while(attrs[i]) {
         if (!wxStricmp(attrs[i], wxT("alias"))) {
            if (wxAtoi(attrs[i+1])==1)
               alias = true;
         }
         i++;
         if (attrs[i])
            i++;
      }

      if (alias)
         *mLoadingTarget = LegacyAliasBlockFile::BuildFromXML(projFull, attrs);
      else      
         *mLoadingTarget = LegacyBlockFile::BuildFromXML(projFull, attrs,
                                                         mLoadingBlockLen,
                                                         mLoadingFormat);
   }
   else
      return false;
      
   //
   // If the block we loaded is already in the hash table, then the
   // object we just loaded is a duplicate, so we delete it and
   // return a reference to the existing object instead.
   //

   wxString name = (*mLoadingTarget)->GetFileName().GetName();    
   BlockFile *retrieved = blockFileHash[name];
   if (retrieved) {
      // Lock it in order to delete it safely, i.e. without having
      // it delete the file, too...
      (*mLoadingTarget)->Lock();
      delete (*mLoadingTarget);

      Ref(retrieved); // Add one to its reference count
      *mLoadingTarget = retrieved;
      return true;
   }

   // This is a new object
   blockFileHash[name]=*mLoadingTarget;
   // MakeBlockFileName wasn't used so we must add the directory
   // balancing information
   BalanceInfoAdd(name);

   return true;
}

bool DirManager::MoveToNewProjectDirectory(BlockFile *f)
{
   wxFileName newFileName;
   wxFileName oldFileName=f->mFileName;
   AssignFile(newFileName,f->mFileName.GetFullName(),FALSE); 

   if ( !(newFileName == f->mFileName) ) {
      bool ok = wxRenameFile(FILENAME(f->mFileName.GetFullPath()),
                             FILENAME(newFileName.GetFullPath()));

      if (ok)
         f->mFileName = newFileName;
      else {
         ok = wxCopyFile(FILENAME(f->mFileName.GetFullPath()),
                         FILENAME(newFileName.GetFullPath()));
         if (ok) {
            wxRemoveFile(FILENAME(f->mFileName.GetFullPath()));
            f->mFileName = newFileName;
         }
         else
            return false;
      }
   }

   return true;
}

bool DirManager::CopyToNewProjectDirectory(BlockFile *f)
{
   wxFileName newFileName;
   AssignFile(newFileName,f->mFileName.GetFullName(),FALSE); 

   if ( !(newFileName == f->mFileName) ) {
      bool ok = wxCopyFile(FILENAME(f->mFileName.GetFullPath()),
                           FILENAME(newFileName.GetFullPath()));
      if (ok) {
         f->mFileName = newFileName;
      }
      else
         return false;
   }

   return true;
}

void DirManager::Ref(BlockFile * f)
{
   f->Ref();
   //  wxPrintf(wxT("Ref(%d): %s\n"),f->refCount, f->fullPath.c_str());
}

int DirManager::GetRefCount(BlockFile * f)
{
   return f->mRefCount;
}

void DirManager::Deref(BlockFile * f)
{
   wxString theFileName = f->GetFileName().GetName();

   //  wxPrintf(wxT("Deref(%d): %s\n"),f->mRefCount-1, f->mRullPath.c_str());

   if (f->Deref()) {
      // If Deref() returned true, the reference count reached zero
      // and this block is no longer needed.  Remove it from the hash
      // table.

      blockFileHash.erase(theFileName);
      BalanceInfoDel(theFileName);

   }
}

bool DirManager::EnsureSafeFilename(wxFileName fName)
{
   // Quick check: If it's not even in our alias list,
   // then the file name is A-OK.

   if (!aliasList.Member(fName.GetFullPath()))
      return true;

   // If any of the following commands fail, your guess is as
   // good as mine why.  The following error message is the
   // best we can do - we'll use it if any of the renames,
   // creates, or deletes fail.
   wxString errStr =
     _( "Error: is directory write-protected or disk full?" );

   /* i18n-hint: 'old' is part of a filename used when a file is renamed. */
   // Figure out what the new name for the existing file would be.  
   /* i18n-hint: e.g. Try to go from "mysong.wav" to "mysong-old1.wav". */
   // Keep trying until we find a filename that doesn't exist.

   wxFileName renamedFile = fName;
   int i = 0;
   do {
      i++;
      /* i18n-hint: This is the pattern for filenames that are created
         when a file needs to be backed up to a different name.  For
         example, mysong would become mysong-old1, mysong-old2, etc. */
      renamedFile.SetName(wxString::Format(_("%s-old%d"), fName.GetName().c_str(), i));
   } while (wxFileExists(FILENAME(renamedFile.GetFullPath())));

   // Test creating a file by that name to make sure it will
   // be possible to do the rename

   wxFile testFile(FILENAME(renamedFile.GetFullPath()), wxFile::write);
   if (!testFile.IsOpened()) {
      wxMessageBox(errStr);
      return false;
   }
   if (!wxRemoveFile(FILENAME(renamedFile.GetFullPath()))) {
      wxMessageBox(errStr);
      return false;
   }

   wxPrintf(_("Renamed file: %s\n"), renamedFile.GetFullPath().c_str());

   // Go through our block files and see if any indeed point to
   // the file we're concerned about.  If so, point the block file
   // to the renamed file and when we're done, perform the rename.

   bool needToRename = false;
   wxBusyCursor busy;
   BlockHash::iterator it=blockFileHash.begin();
   while(it != blockFileHash.end()) {
      BlockFile *b = it->second;
      // don't worry, we don't rely on this cast unless IsAlias is true
      AliasBlockFile *ab = (AliasBlockFile*)b;

      if (b->IsAlias() && ab->GetAliasedFile() == fName) {
         needToRename = true;
         wxPrintf(_("Changing block %s\n"), b->GetFileName().GetFullName().c_str());
         ab->ChangeAliasedFile(renamedFile);
      }

      it++;
   }

   if (needToRename) {
      if (!wxRenameFile(FILENAME(fName.GetFullPath()),
                        FILENAME(renamedFile.GetFullPath()))) {
         // ACK!!! The renaming was unsuccessful!!!
         // (This shouldn't happen, since we tried creating a
         // file of this name and then deleted it just a
         // second earlier.)  But we'll handle this scenario
         // just in case!!!

         // Put things back where they were
         BlockHash::iterator it=blockFileHash.begin();
         while(it != blockFileHash.end()) {
            BlockFile *b = it->second;
            AliasBlockFile *ab = (AliasBlockFile*)b;

            if (b->IsAlias() && ab->GetAliasedFile() == renamedFile)
               ab->ChangeAliasedFile(fName);
            it++;
         }

         // Print error message and cancel the export
         wxMessageBox(errStr);
         return false;
      }

      aliasList.Delete(fName.GetFullPath());
      aliasList.Add(renamedFile.GetFullPath());
   }

   // Success!!!  Either we successfully renamed the file,
   // or we didn't need to!
   return true;
}

void DirManager::Ref()
{
   wxASSERT(mRef > 0); // MM: If mRef is smaller, it should have been deleted already
   ++mRef;
}

void DirManager::Deref()
{
   wxASSERT(mRef > 0); // MM: If mRef is smaller, it should have been deleted already
   
   --mRef;

   // MM: Automatically delete if refcount reaches zero
   if (mRef == 0)
   {
//      wxLogDebug(wxT("DirManager::Deref: Automatically deleting 'this'"));
      delete this;
   }
}

// check the Blockfiles against the disk state.  Missing Blockfile
// data is regenerated if possible orreplaced with silence, orphaned
// blockfiles are deleted.... but only after user confirm!  Note that
// even blockfiles not referenced by the current savefile (but locked
// by history) will be reflected in the blockFileHash, and that's a
// good thing; this is one reason why we use the hash and not the most
// recent savefile.

int DirManager::ProjectFSCK(bool forceerror)
{
      
   // get a rough guess of how many blockfiles will be found/processed
   // at each step by looking at the size of the blockfile hash
   int blockcount=blockFileHash.size();
   int ret=0;

   // enumerate *all* files in the project directory
   wxStringList fnameList;

   wxStringList orphanList;
   BlockHash    missingAliasList;
   BlockHash    missingAliasFiles;
   BlockHash    missingSummaryList;
   BlockHash    missingDataList;

   // this function is finally a misnomer
   rm_dash_rf_enumerate_prompt((projFull != wxT("")? projFull: mytemp),
                               &fnameList,wxEmptyString,1,0,blockcount,
                               _("Inspecting project file data..."));
   
   // enumerate orphaned blockfiles
   BlockHash diskFileHash;
   wxStringListNode *node = fnameList.GetFirst();
   while(node){
      wxFileName fullname(node->GetData());
      wxString basename=fullname.GetName();
      
      diskFileHash[basename.c_str()]=0; // just needs to be defined
      if(blockFileHash.find(basename) == blockFileHash.end()){
         // the blockfile on disk is orphaned
         orphanList.Add(fullname.GetFullPath());
         wxLogWarning(_("Orphaned blockfile: (%s)"),
                      fullname.GetFullPath().c_str());
      }
      node=node->GetNext();
   }
   
   // enumerate missing alias files
   BlockHash::iterator i=blockFileHash.begin();
   while(i != blockFileHash.end()) {
      wxString key=i->first;
      BlockFile *b=i->second;
      
      if(b->IsAlias()){
         wxFileName aliasfile=((AliasBlockFile *)b)->GetAliasedFile();
         if(aliasfile.GetFullPath()!=wxEmptyString && !wxFileExists(aliasfile.GetFullPath())){
            missingAliasList[key]=b;
            missingAliasFiles[aliasfile.GetFullPath().c_str()]=0; // simply must be defined
         }
      }
      i++;
   }
   
   i=missingAliasFiles.begin();
   while(i != missingAliasFiles.end()) {
      wxString key=i->first;
      wxLogWarning(_("Missing alias file: (%s)"),key.c_str());
      i++;
   }


   // enumerate missing summary blockfiles
   i=blockFileHash.begin();
   while(i != blockFileHash.end()) {
      wxString key=i->first;
      BlockFile *b=i->second;
      
      if(b->IsAlias()){
         /* don't look in hash; that might find files the user moved
            that the Blockfile abstraction can't find itself */
         wxFileName file=MakeBlockFilePath(key);
         file.SetName(key);
         file.SetExt(wxT("auf"));
         if(!wxFileExists(file.GetFullPath().c_str())){
            missingSummaryList[key]=b;
            wxLogWarning(_("Missing summary file: (%s.auf)"),
                         key.c_str());
         }
      }
      i++;
   }

   // enumerate missing data blockfiles
   i=blockFileHash.begin();
   while(i != blockFileHash.end()) {
      wxString key=i->first;
      BlockFile *b=i->second;

      if(!b->IsAlias()){
         wxFileName file=MakeBlockFilePath(key);
         file.SetName(key);
         file.SetExt(wxT("au"));
         if(!wxFileExists(file.GetFullPath().c_str())){
            missingDataList[key]=b;
            wxLogWarning(_("Missing data file: (%s.au)"),
                         key.c_str());
         }
      }
      i++;
   }
   
   // First, pop the log so the user can see what be up.
   if(forceerror ||
      !orphanList.IsEmpty() ||
      !missingAliasList.empty() ||
      !missingDataList.empty() ||
      !missingSummaryList.empty()){

      wxLogWarning(_("Project check found inconsistencies inspecting the loaded project data;\nclick 'Details' for a complete list of errors, or 'OK' to proceed to more options."));
      
      wxLog::GetActiveTarget()->Flush(); // Flush is both modal
      // (desired) and will clear the log (desired)
   }

   // report, take action
   if(!orphanList.IsEmpty()){

      wxString promptA =
         _("Project check found %d orphaned blockfile[s]. These files are\nunused and probably left over from a crash or some other bug.\nThey should be deleted to avoid disk contention.");
      wxString prompt;
      
      prompt.Printf(promptA,orphanList.GetCount());
      
      const wxChar *buttons[]={_("Delete orphaned files [safe and recommended]"),
                               _("Continue without deleting; silently work around the extra files"),
                               _("Close project immediately with no changes"),NULL};
      int action = ShowMultiDialog(prompt,
                                   _("Warning"),
                                   buttons);

      if(action==2)return (ret | FSCKstatus_CLOSEREQ);

      if(action==0){
         ret |= FSCKstatus_CHANGED;
         wxStringListNode *node=orphanList.GetFirst();
         while(node){
            wxString fullpath=node->GetData();
            wxRemoveFile(fullpath);
            node=node->GetNext();
         }
      }
   }


   // Deal with any missing aliases
   if(!missingAliasList.empty()){

      wxString promptA =
         _("Project check detected %d input file[s] being used in place\n('alias files') are now missing.  There is no way for Audacity\nto recover these files automatically; you may choose to\npermanently fill in silence for the missing files, temporarily\nfill in silence for this session only, or close the project now\nand try to restore the missing files by hand.");
      wxString prompt;
      
      prompt.Printf(promptA,missingAliasFiles.size());
      
      const wxChar *buttons[]={_("Replace missing data with silence [permanent upon save]"),
                               _("Temporarily replace missing files with silence [this session only]"),
                               _("Close project immediately with no further changes"),NULL};
      int action = ShowMultiDialog(prompt,
                                   _("Warning"),
                                   buttons);

      if(action==2)return (ret | FSCKstatus_CLOSEREQ);

      BlockHash::iterator i=missingAliasList.begin();
      while(i != missingAliasList.end()) {
         AliasBlockFile *b = (AliasBlockFile *)i->second; //this is
         //safe, we checked that it's an alias block file earlier
         
         if(action==0){
            // silence the blockfiles by yanking the filename
            wxFileName dummy;
            dummy.Clear();
            b->ChangeAliasedFile(dummy);
            b->Recover();
            ret |= FSCKstatus_CHANGED;
         }else if(action==1){
            // silence the log for this session
            b->SilenceAliasLog();
         }
         i++;
      }
   }

   // Summary regeneration must happen after alias checking.
   if(!missingSummaryList.empty()){

      wxString promptA =
         _("Project check detected %d missing summary file[s] (.auf).\nAudacity can fully regenerate these summary files from the\noriginal audio data in the project.");
      wxString prompt;
      
      prompt.Printf(promptA,missingSummaryList.size());
      
      const wxChar *buttons[]={_("Regenerate summary files [safe and recommended]"),
                               _("Fill in silence for missing display data [this session only]"),
                               _("Close project immediately with no further changes"),NULL};
      int action = ShowMultiDialog(prompt,
                                   _("Warning"),
                                   buttons);

      if(action==2)return (ret | FSCKstatus_CLOSEREQ);
      BlockHash::iterator i=missingSummaryList.begin();
      while(i != missingSummaryList.end()) {
         BlockFile *b = i->second;
         if(action==0){
            //regenerate from data
            b->Recover();
            ret |= FSCKstatus_CHANGED;
         }else if (action==1){
            b->SilenceLog();
         }
         i++;
      }
   }

   // Deal with any missing SimpleBlockFiles
   if(!missingDataList.empty()){

      wxString promptA =
         _("Project check detected %d missing audio data blockfile[s] (.au), \nprobably due to a bug, system crash or accidental deletion.\nThere is no way for Audacity to recover this lost data\nautomatically; you may choose to permanently fill in silence\nfor the missing data, temporarily fill in silence for this\nsession only, or close the project now and try to restore the\nmissing data by hand.");
      wxString prompt;
      
      prompt.Printf(promptA,missingDataList.size());
      
      const wxChar *buttons[]={_("Replace missing data with silence [permanent immediately]"),
                               _("Temporarily replace missing data with silence [this session only]"),
                               _("Close project immediately with no further changes"),NULL};
      int action = ShowMultiDialog(prompt,
                                   _("Warning"),
                                   buttons);
      
      if(action==2)return (ret | FSCKstatus_CLOSEREQ);
      
      BlockHash::iterator i=missingDataList.begin();
      while(i != missingDataList.end()) {
         BlockFile *b = i->second;
         if(action==0){
            //regenerate with zeroes
            b->Recover();
            ret |= FSCKstatus_CHANGED;
         }else if(action==1){
            b->SilenceLog();
         }
         i++;
      }
   }

   // clean up any empty directories
   fnameList.Clear();
   rm_dash_rf_enumerate_prompt((projFull != wxT("")? projFull: mytemp),
                               &fnameList,wxEmptyString,0,1,blockcount,
                               _("Cleaning up unused directories in project data..."));
   rm_dash_rf_execute(fnameList,0,0,1,0);


   return ret;
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
// arch-tag: 192b7dbe-6fef-49a8-b4f4-f11bce51d84f

