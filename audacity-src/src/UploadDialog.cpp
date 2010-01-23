/**********************************************************************

  Audacity: A Digital Audio Editor

  UploadDialog.cpp

  Mike Underwood
  munderwood@bedheaddesign.com

  Much like the author, this code is crazy at times but it gets the job
  done.

**********************************************************************/
#include <fstream>
#include <math.h>
#include <wx/dialog.h>
#include <wx/html/htmlwin.h>
#include <wx/button.h>
#include <wx/dcclient.h>
#include <wx/gdicmn.h>
#include <wx/imaglist.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/statusbr.h>
#include <wx/string.h>
#include <wx/intl.h>
#include <wx/wfstream.h>
#include <wx/listctrl.h>
#include <wx/checkbox.h>
#include <wx/progdlg.h>
#include <wx/menuitem.h>
#include <wx/utils.h>
#include <wx/dir.h>
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "UploadDialog.h"
#include "Audacity.h"

//#include "../images/AudacityLogo.xpm"

#ifdef __WXMSW__
# define DLOG_HEIGHT 430
#else
# if defined(__WXMAC__) && ((wxMAJOR_VERSION == 2) && (wxMINOR_VERSION >= 5))
#  define DLOG_HEIGHT 430
# else
#  define DLOG_HEIGHT 400
# endif
#endif

// Icon images

static const char *file_xpm[] = {
/* columns rows colors chars-per-pixel */
"16 16 4 1",
"  c #040404",
". c #868686",
"X c #C0C0C0",
"o c gray100",
/* pixels */
"oo..........oooo",
"oo.ooooooooX.ooo",
"oo.ooooooooXo.oo",
"oo.oooooooo    o",
"oo.ooooooooooX o",
"oo.ooooooooooX o",
"oo.ooooooooooX o",
"oo.ooooooooooX o",
"oo.ooooooooooX o",
"oo.ooooooooooX o",
"oo.ooooooooooX o",
"oo.ooooooooooX o",
"oo.ooooooooooX o",
"oo.ooooooooooX o",
"oo.XXXXXXXXXXX o",
"oo             o"
};

static const char *folder_xpm[] = {
/* columns rows colors chars-per-pixel */
"16 16 15 1",
"  c gray30",
". c gray40",
"X c #996600",
"o c #CC6633",
"O c #CC9933",
"+ c #CCCC33",
"@ c #FFCC66",
"# c #868686",
"$ c #FFCC99",
"% c #FFFF99",
"& c gray80",
"* c #D7D7D7",
"= c #DDDDDD",
"- c #F1F1F1",
"; c gray100",
/* pixels */
";;;;;;;;;;;;;;;;",
";;;;;;;;;;;;;;;;",
";;OOOO=-;;;;;;;;",
";O;;;;O&-;;;;;;;",
"O;%%%%;OOOOOo=-;",
"O%%%%%%;;;;;X#=;",
"O%$OOOOOOOOOOOO=",
"O@O;;;;;;;;;$;X#",
"O@O%%%%%%%%%@%X.",
"O@O%%%%%%%%%@%X.",
"O@O%%%%%%%%%+%X.",
"O@O%@@@@@@@@O%X.",
"O@O%@@@@@@@@O%X.",
"=OOoXXXXXXXXXX #",
"-&#...........#&",
";-=***********=-"
};

static const char *mp3_xpm[] = {
/* columns rows colors chars-per-pixel */
"16 16 19 1",
"  c #040404",
". c gray11",
"X c #222222",
"o c gray20",
"O c gray26",
"+ c gray30",
"@ c gray40",
"# c #777777",
"$ c #818181",
"% c gray59",
"& c #A0A0A4",
"* c #B2B2B2",
"= c #C0C0C0",
"- c gray80",
"; c #D7D7D7",
": c gray89",
"> c #EAEAEA",
", c #F1F1F1",
"< c gray1",
/* pixels */
"<<<<<<<>=%@@<<<<",
"<<<-$+. .O##<<<<",
"<<<$@%=><<<&<<<<",
"<<<*<<<<<<<&<<<<",
"<<<*<<<<<<<&<<<<",
"<<<*<<<<<<<&<<<<",
"<<<*<<<<<<<&<<<<",
"<<<*<<<<<<<&<<<<",
"<<<*<<<<<<<&<<<<",
"<<<*<<<<<=$#<<<<",
"<<:*<<<<$  O<<<<",
";X O<<<<.  *<<<<",
"o  @<<<<;%;<<<<<",
"#X@,<<<<<<<<<<<<",
"<<<<<<<<<<<<<<<<",
"<<<<<<<<<<<<<<<<"
};

static const char *up_xpm[] = {
/* columns rows colors chars-per-pixel */
"16 16 25 1",
"  c #040404",
". c #0C0C0C",
"X c #161616",
"o c gray11",
"O c #222222",
"+ c gray16",
"@ c gray20",
"# c #393939",
"$ c gray26",
"% c gray30",
"& c #555555",
"* c gray40",
"= c #777777",
"- c #848484",
"; c gray59",
": c gray60",
"> c #A0A0A4",
", c #B2B2B2",
"< c #C0C0C0",
"1 c #D7D7D7",
"2 c #DDDDDD",
"3 c gray89",
"4 c #EAEAEA",
"5 c #F1F1F1",
"6 c gray1",
/* pixels */
"6666666666666666",
"6666666666666666",
"6666<<6666666666",
"6661$-6666666666",
"666: &5666666666",
"661#  :666666666",
"64&   O<66666666",
"6,=@ X#-36666336",
"526- X,436664:<6",
"666; .=66664*=56",
"666,o O:43,#+<66",
"6664&  o%%X :666",
"6666;     X-6666",
"66664-. .#>56666",
"6666666666666666",
"6666666666666666"
};

// ----------------------------------------------------------------------------
// icons
// ----------------------------------------------------------------------------
BEGIN_EVENT_TABLE(UploadDialog, wxDialog)
   EVT_MENU(wxID_POPUP_DOWNLOAD, UploadDialog::OnPopupMenu)
   EVT_MENU(wxID_POPUP_DELETE, UploadDialog::OnPopupMenu)
   EVT_MENU(wxID_POPUP_RENAME, UploadDialog::OnPopupMenu)

   EVT_BUTTON(wxID_UPLOAD_FILE, UploadDialog::OnUploadFile)
   EVT_BUTTON(wxID_UPLOAD_DIR, UploadDialog::OnUploadDir)
   EVT_BUTTON(wxID_CREATEDIR, UploadDialog::OnCreateDir)
   EVT_BUTTON(wxID_OK, UploadDialog::OnOK)
   EVT_BUTTON(wxID_CONNECT, UploadDialog::OnConnect)
   EVT_BUTTON(wxID_DISCONNECT, UploadDialog::OnDisconnect)

   EVT_LIST_ITEM_ACTIVATED(wxID_FILEMANAGER, UploadDialog::OnActivateItem)
   EVT_LIST_ITEM_ACTIVATED(wxID_SITELIST, UploadDialog::OnActivateSite)
   EVT_LIST_ITEM_SELECTED(wxID_SITELIST, UploadDialog::OnSelectSite)
   EVT_LIST_ITEM_RIGHT_CLICK(wxID_FILEMANAGER, UploadDialog::OnListRightClick)

   EVT_BUTTON(wxID_DELSITE, UploadDialog::OnDeleteSite)
END_EVENT_TABLE()

IMPLEMENT_CLASS(UploadDialog, wxDialog)

UploadDialog::UploadDialog(wxWindow * parent)
:  wxDialog(parent, -1, _("Audacity FTP"),
         wxDefaultPosition, wxSize(640, 480), wxDEFAULT_DIALOG_STYLE | wxWANTS_CHARS)
{

    dirnameList = new wxArrayString();
    dirpermList = new wxArrayString();
    dirsizeList = new wxArrayString();
    filenameList = new wxArrayString();
    filepermList = new wxArrayString();
    filesizeList = new wxArrayString();
    displayNames = new wxArrayString();
    displayPerm  = new wxArrayString();
    displaySizes  = new wxArrayString();
    ftpList = new wxArrayString();

    icons = new wxImageList(16, 16, false, 2);
    wxIcon *folderIcon = new wxIcon(folder_xpm);
    wxIcon *fileIcon = new wxIcon(file_xpm);
    wxIcon *mp3Icon = new wxIcon(mp3_xpm);
    wxIcon *upIcon = new wxIcon(up_xpm);
    icons->Add(*folderIcon);
    icons->Add(*fileIcon);
    icons->Add(*mp3Icon);
    icons->Add(*upIcon);
    
    wxFlexGridSizer *topSizer = new wxFlexGridSizer(2, 1);  
    wxStaticBoxSizer *connectionBox = new wxStaticBoxSizer(new wxStaticBox(this, -1, _("FTP Connection"), wxDefaultPosition, wxDefaultSize, 0, wxT("")), wxVERTICAL);   
    wxStaticBoxSizer *fileBox = new wxStaticBoxSizer(new wxStaticBox(this, -1, _("File Manager"), wxDefaultPosition, wxDefaultSize, 0, wxT("")), wxVERTICAL);
    wxStaticBoxSizer *siteBox = new wxStaticBoxSizer(new wxStaticBox(this, -1, _("Site Manager"), wxDefaultPosition, wxDefaultSize, 0, wxT("")), wxVERTICAL);

    wxFlexGridSizer *connectionSizer = new wxFlexGridSizer(2, 4);
    wxBoxSizer *leftSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *rightSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *fileButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *siteButtonSizer = new wxBoxSizer(wxHORIZONTAL);

    txtFtpName = new wxTextCtrl(this, wxID_FTPNAME, wxT(""), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT(""));
    txtFtpHost = new wxTextCtrl(this, wxID_FTPHOST, wxT(""), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT(""));
    txtFtpUser = new wxTextCtrl(this, wxID_FTPUSER, wxT(""), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT(""));
    txtFtpPass = new wxTextCtrl(this, wxID_FTPPASS, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD, wxDefaultValidator, wxT(""));
    btnConnect = new wxButton(this, wxID_CONNECT, _("&Connect"), wxDefaultPosition, wxDefaultSize);
    btnDisconnect = new wxButton(this, wxID_DISCONNECT, _("&Disconnect"), wxDefaultPosition, wxDefaultSize);
    btnDisconnect->Enable(false);

    btnUploadFile = new wxButton(this, wxID_UPLOAD_FILE, _("Upload &File"), wxDefaultPosition, wxDefaultSize);
    btnUploadFile->Enable(false);
    btnUploadDir = new wxButton(this, wxID_UPLOAD_DIR, _("Upload F&older"), wxDefaultPosition, wxDefaultSize);
    btnUploadDir->Enable(false);
    btnCreateDir = new wxButton(this, wxID_CREATEDIR, _("N&ew Folder"), wxDefaultPosition, wxDefaultSize);
    btnCreateDir->Enable(false);

    btnDelSite = new wxButton(this, wxID_DELSITE, _("&Remove Site"), wxDefaultPosition, wxDefaultSize);

    fileManager = new wxListView(this, wxID_FILEMANAGER, wxDefaultPosition, wxSize(350, 350), wxLC_REPORT, wxDefaultValidator, wxT(""));
    fileManager->AssignImageList(icons, wxIMAGE_LIST_SMALL);
    //fileManager->Show(false);
    fileManager->InsertColumn(0, _("name"));
    fileManager->InsertColumn(1, _("size (bytes)"));
    fileManager->InsertColumn(2, _("permissions"));

    siteList = new wxListView(this, wxID_SITELIST, wxDefaultPosition, wxSize(200, 215), wxLC_REPORT | wxLC_NO_HEADER, wxDefaultValidator, wxT(""));
    siteList->InsertColumn(0, wxT(""));

    connectionSizer->Add(new wxStaticText(this, -1, _("Name:"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT, wxT("")));
    connectionSizer->Add(txtFtpName);
    connectionSizer->Add(new wxStaticText(this, -1, _("Host:"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT, wxT("")));
    connectionSizer->Add(txtFtpHost);
    connectionSizer->Add(new wxStaticText(this, -1, _("Username:"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT, wxT("")));
    connectionSizer->Add(txtFtpUser);
    connectionSizer->Add(new wxStaticText(this, -1, _("Password:"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT, wxT("")));
    connectionSizer->Add(txtFtpPass);
    connectionSizer->Add(btnConnect, 0, wxALIGN_LEFT | wxALL, 5);
    connectionSizer->Add(btnDisconnect, 0, wxALIGN_LEFT | wxALL, 5);

    connectionBox->Add(connectionSizer, 0, wxALIGN_CENTER);
    connectionBox->Add(200, 0);

    fileButtonSizer->Add(btnUploadFile, 0, wxALL, 5);
    fileButtonSizer->Add(btnUploadDir, 0, wxALL, 5);
    fileButtonSizer->Add(btnCreateDir, 0, wxALL, 5);

    //siteButtonSizer->Add(btnNewSite, 0, wxALL, 5);
    siteButtonSizer->Add(btnDelSite, 0, wxALL, 5);

    fileBox->Add(fileManager, 0, wxALIGN_CENTER);
    fileBox->Add(fileButtonSizer, 0, wxALIGN_CENTER);
    //fileBox->Add(300, 0);

    siteBox->Add(siteList, 0, wxALIGN_CENTER);
    siteBox->Add(siteButtonSizer, 0, wxALIGN_LEFT);

    leftSizer->Add(connectionBox);
    leftSizer->Add(siteBox, 0, wxTOP, 10);
    rightSizer->Add(fileBox);

    topSizer->Add(leftSizer, 0, wxALL, 10);
    topSizer->Add(rightSizer, 0, wxTOP | wxBOTTOM | wxRIGHT, 10);

    SetSizerAndFit(topSizer);

    // populate siteList
    UpdateSiteList();

}

UploadDialog::~UploadDialog()
{
    
}

void UploadDialog::OnConnect(wxCommandEvent & WXUNUSED(event))
{

    wxString system;

    SetTitle(_("Connecting..."));
    SetCursor(wxCURSOR_WAIT);

    //int connectionExists = -1;

    ftp = new wxFTP();
    ftp->SetUser(txtFtpUser->GetValue());
    ftp->SetPassword(txtFtpPass->GetValue());
    
    if (ftp->Connect(txtFtpHost->GetValue()))
    {

        // check which system ftp is running on
        ftp->SendCommand(wxT("SYST"));
        system = ftp->GetLastResult();
        system = system.Lower();

        //wxMessageBox(system, _T("FTP Status"), wxOK | wxICON_INFORMATION, this);

        if (system.Find(wxT("unix"))==-1 && system.Find(wxT("windows"))==-1)
        {
            wxString msg;
            msg.Printf(_("Unknown FTP Server type: %s\n\nOnly Unix based and Windows systems are supported/tested at this time."), (const wxChar*)system);
            wxMessageBox(msg, _("Error"), wxOK | wxICON_INFORMATION, this);
        }

        btnConnect->Enable(false);
        btnDisconnect->Enable(true);
        btnUploadFile->Enable(true);
        btnUploadDir->Enable(true);
        btnCreateDir->Enable(true);
        btnDelSite->Enable(false);

        fileManager->Show(true);
        siteList->Show(false);

        txtFtpName->Enable(false);
        txtFtpHost->Enable(false);
        txtFtpUser->Enable(false);
        txtFtpPass->Enable(false);

        if (txtFtpName->GetValue().Trim() == wxT(""))
            txtFtpName->SetValue(_("New Site"));

        // save login into to connections file
        if (SaveFtpSite(txtFtpName->GetValue(), txtFtpHost->GetValue(), txtFtpUser->GetValue(), txtFtpPass->GetValue()))
        {
            // add site to site list
            long tmp = siteList->InsertItem(siteList->GetItemCount(), txtFtpName->GetValue(), 0);
            siteList->SetItemData(tmp, 0);
        }

        SetTitle(_("Connected to: ") + txtFtpHost->GetValue());
    
        RefreshFiles();
        
    }

    else
    {
        wxMessageBox(_("Connection cannot be established"), _("FTP Status"), wxOK | wxICON_INFORMATION, this);
        delete ftp;
    }

    SetCursor(wxCURSOR_ARROW);

}

void UploadDialog::OnDisconnect(wxCommandEvent & WXUNUSED(event))
{

    delete ftp;

    btnDisconnect->Enable(false);
    btnConnect->Enable(true);
    btnUploadFile->Enable(false);
    btnUploadDir->Enable(false);
    btnCreateDir->Enable(false);
    btnDelSite->Enable(true);

    fileManager->Show(false);
    fileManager->DeleteAllItems();
    siteList->Show(true);

    txtFtpName->Enable(true);
    txtFtpHost->Enable(true);
    txtFtpUser->Enable(true);
    txtFtpPass->Enable(true);

    SetTitle(_("No Connection"));
    fileManager->Show(false);

    LoadFtpSiteList();
}


void UploadDialog::OnUploadFile(wxCommandEvent & WXUNUSED(event))
{
    unsigned int count;
    wxString name;
    wxString path;
    wxArrayString files;

    abort = false;

    wxFileDialog *open = new wxFileDialog(this, _("Choose file(s) to upload"),
                                          ::wxGetCwd(), wxT(""),
                                          _("All files (*.*)|*.*"),
                                          wxOPEN | wxMULTIPLE); 
    
    if (open->ShowModal()==wxID_OK)
    {
        SetCursor(wxCURSOR_WAIT);

        // get the file(s) the user selected
        open->GetPaths(files);

        for (count = 0; count < files.GetCount(); count++)
        {   
            if (!abort)
            {
                // remove path from filename
                int index = files.Item(count).Find(wxT('\\'), true);
                wxString name = files.Item(count).Mid(index+1, files.Item(count).Length() - index);
                wxString path = ftp->Pwd()+wxT("/")+name.MakeLower();
            

                // upload the file
                UploadFile(files.Item(count), path.MakeLower());
            }
        }

        RefreshFiles();

        SetCursor(wxCURSOR_ARROW);

    }
}

void UploadDialog::OnUploadDir(wxCommandEvent & WXUNUSED(event))
{

    wxDirDialog *open = new wxDirDialog(this, _("Choose folder to upload"), wxT("")); 
    int result = open->ShowModal();

    abort = false;

    if (result ==  wxID_OK)
    {
        SetCursor(wxCURSOR_WAIT);
        UploadDir(open->GetPath(), wxT(""));
        SetCursor(wxCURSOR_ARROW);
    }
}

void UploadDialog::OnListRightClick(wxListEvent &event)
{
    
    listIndex = event.m_itemIndex;

    wxMenu *menu = new wxMenu();
    wxMenuItem *del = new wxMenuItem(menu, wxID_POPUP_DELETE, _("Delete"), _("Delete file(s) from server)"));
    wxMenuItem *rn  = new wxMenuItem(menu, wxID_POPUP_RENAME, _("Rename"), _("Rename file"));
    wxMenuItem *dl  = new wxMenuItem(menu, wxID_POPUP_DOWNLOAD, _("Download"), _("Download this file(s) to your computer"));

    wxMenuItem *sep  = new wxMenuItem(menu, -1, wxT(""), wxT(""));

    menu->Insert(0, dl);
    menu->Insert(1, sep);
    menu->Insert(2, del);
    menu->Insert(3, rn);

    fileManager->GetItemPosition(event.m_itemIndex ,mousePos);
    PopupMenu(menu, mousePos.x + 250, mousePos.y + 50);
}

void UploadDialog::OnActivateSite (wxListEvent &event)
{



}

void UploadDialog::OnPopupMenu (wxCommandEvent & event)
{

    if (event.GetId() == wxID_POPUP_DOWNLOAD)
    {
        abort = false;

        if (fileManager->GetSelectedItemCount() > 1)
        {
            SetCursor(wxCURSOR_WAIT);
            DownloadMultipleItems();
            SetCursor(wxCURSOR_ARROW);
        }

        else
        {
            SetCursor(wxCURSOR_WAIT);
            wxString item = fileManager->GetItemText(listIndex);

            if (displayPerm->Item(listIndex).GetChar(0)=='d')
                DownloadItem(item, true);    
            else
                DownloadItem(item, false);
            
            SetCursor(wxCURSOR_ARROW);
        }
    }

    if (event.GetId() == wxID_POPUP_DELETE)
    {
        

        if (fileManager->GetSelectedItemCount() > 1)
        {

            int result = wxMessageBox(_("Delete selected items?"), _("Confirm"), wxYES_NO, NULL);

            if (result == wxYES)
            {
                SetCursor(wxCURSOR_WAIT);

                long item = -1;
                wxArrayString *selectList = new wxArrayString();
                wxArrayString *selectPerm = new wxArrayString();

                item = fileManager->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
                while (item != -1)
                {
                    // add selected item to list
                    selectList->Add(displayNames->Item(item));
                    selectPerm->Add(displayPerm->Item(item));

                    // get next item
                    item = fileManager->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
                }

                unsigned int count;
                for (count = 0; count < selectList->GetCount(); count++)
                {
                    if (selectList->Item(count) != wxT(".."))
                    {
                        // if directory
                        if (selectPerm->Item(count).GetChar(0) == wxT('d'))
                        {
                            progress = new wxProgressDialog(_("Deleting..."), wxT(""), 100, this);
                            progress->SetCursor(wxCURSOR_ARROWWAIT);

                            deleteFileList = new wxArrayString();
                            deleteDirList = new wxArrayString();

                            GetDeleteList(selectList->Item(count));
                            RemoveItems(deleteFileList, deleteDirList);

                            deleteFileList->Clear();
                            deleteDirList->Clear();

                            progress->Destroy();
                        }

                        else
                            ftp->RmFile(selectList->Item(count));
                                                    
                    }
                }

                selectList->Clear();
                selectPerm->Clear();

                SetCursor(wxCURSOR_ARROW);
            }

            RefreshFiles();
        }
        

        else
        {

            wxString confirm;
            confirm.Printf(_("Delete %s?"),
                           displayNames->Item(listIndex).c_str());
            
            int result = wxMessageBox(confirm, _("Confirm"), wxYES_NO, NULL);

            if (result == wxYES)
            {
                SetCursor(wxCURSOR_WAIT);

                    // if directory
                    if (displayPerm->Item(listIndex).GetChar(0) == 'd')
                    {
                        // create progress bar
                        progress = new wxProgressDialog(_("Delete"), wxT(""), 100, this);
                        progress->SetCursor(wxCURSOR_ARROWWAIT);

                        deleteFileList = new wxArrayString();
                        deleteDirList = new wxArrayString();    

                        GetDeleteList(displayNames->Item(listIndex));
                        RemoveItems(deleteFileList, deleteDirList);

                        deleteFileList->Clear();
                        deleteDirList->Clear();

                        progress->Destroy();

                    }
                    else
                    {
                        //DeleteItem(displayNames->Item(listIndex), false);
                        ftp->RmFile(displayNames->Item(listIndex));
                    }
                SetCursor(wxCURSOR_ARROW);
            }

            RefreshFiles();
    
        }
    }

    if (event.GetId() == wxID_POPUP_RENAME)
    {
        wxString result = wxGetTextFromUser(_("Please enter the new file/folder name:"), _("Rename"), fileManager->GetItemText(listIndex), this);

        if (result != wxT(""))
        {
            result.Replace(wxT(" "), wxT("_"), true);

            if (ftp->Rename(fileManager->GetItemText(listIndex).MakeLower(), result))
                RefreshFiles();
            else
                wxMessageBox(_("Error renaming file, check permissions."), _("FTP Status"), wxOK, NULL);
        }
    }

}

void UploadDialog::GetDeleteList (wxString src)
{
    unsigned int count;
    
        ftp->ChDir(src);
        GetDirContents();
        
        for (count = 0; count < displayNames->GetCount(); count++)
        {
            if (displayPerm->Item(count).GetChar(0) == wxT('d'))
            {
                GetDeleteList(displayNames->Item(count));
            }
            else
            {
                if (displayNames->Item(count) != wxT(".."))
                {
                    deleteFileList->Add(ftp->Pwd()+wxT("/")+displayNames->Item(count));
                }
            }

            if (progress)
            {
                int percent = (int)(((float)count / (float)displayNames->GetCount())*100.0);
                progress->Update(percent, _("Preparing to delete"));
            }           

        }

        deleteDirList->Add(ftp->Pwd());

        ftp->ChDir(wxT(".."));
        GetDirContents();

}

void UploadDialog::RemoveItems(wxArrayString *files, wxArrayString *dirs)
{
    unsigned int count;
    int percent;

    for (count = 0; count < files->GetCount(); count++)
    {
        ftp->RmFile(files->Item(count));
        
        percent = (int)(((float)count / (float)files->GetCount())*100.0);
        progress->Update(percent, _("Deleting files"));
    }
    
    for (count = 0; count < dirs->GetCount(); count++)
    {
        ftp->RmDir(dirs->Item(count));

        percent = (int)(((float)count / (float)dirs->GetCount())*100.0);
        progress->Update(percent, _("Deleting directories"));

    }

}

void UploadDialog::GetDirContents (void)
{
    unsigned int count;
    wxString filename;
    wxString buf;
    wxString size;

    wxArrayString *rawList = new wxArrayString();

    dirnameList->Empty();
    dirpermList->Empty();
    dirsizeList->Empty();
    filenameList->Empty();
    filepermList->Empty();
    filesizeList->Empty();
    displayNames->Empty();
    displayPerm->Empty();
    displaySizes->Empty();

    if (ftp->GetDirList(*rawList))
    {   
        
        // get Directory list
        for (count = 0; count < rawList->GetCount(); count++)
        {                   

            filename = rawList->Item(count);

            wxArrayString *info = new wxArrayString();
            ExtractListData(filename, info);    
    
            if (info->Item(3) == wxT("yes")) 
            {
                dirnameList->Add(info->Item(0));
                dirpermList->Add(info->Item(1));
                dirsizeList->Add(info->Item(2));
            }
                
        }

        // get File list    
        for (count = 0; count < rawList->GetCount(); count++)
        {                   

            filename = rawList->Item(count);

            wxArrayString *info = new wxArrayString();
            ExtractListData(filename, info);
            
            if (info->Item(3) == wxT("no"))  
            {
                filenameList->Add(info->Item(0));
                filepermList->Add(info->Item(1));
                filesizeList->Add(info->Item(2));
            }
            
        }

        dirnameList->Insert(wxT(".."), 0);
        dirpermList->Insert(wxT("up"), 0);
        dirsizeList->Insert(wxT("-"), 0);


        // add directories and files
        for (count = 0; count < dirnameList->GetCount(); count++)   
        {
            displayNames->Add(dirnameList->Item(count));
            displayPerm->Add(dirpermList->Item(count));
            displaySizes->Add(dirsizeList->Item(count));
        }

        for (count = 0; count < filenameList->GetCount(); count++)  
        {
            if (!filenameList->Item(count).IsNumber())
            {
                displayNames->Add(filenameList->Item(count));
                displayPerm->Add(filepermList->Item(count));
                displaySizes->Add(filesizeList->Item(count));
            }
        }
    }
    else
        wxMessageBox(_("Unable to retrieve directory contents."), _("FTP Status"), wxOK | wxICON_INFORMATION, this);


}

void UploadDialog::OnActivateItem (wxListEvent &event)
{

if (ftp->ChDir(displayNames->Item(event.m_itemIndex)))
{
    RefreshFiles();
}



}

void UploadDialog::RefreshFiles(void)
{
    unsigned int count;
    long tmp;

    // get directory listing and save it
    GetDirContents();
    
    // clear filemanager
    fileManager->DeleteAllItems();

        for (count = 0; count < displayNames->GetCount(); count++)
        {
            if (displayPerm->Item(count)[0] == wxT('d') || displayNames->Item(count) == wxT(".."))
            {
                if (displayNames->Item(count) == wxT(".."))
                    tmp = fileManager->InsertItem(count, displayNames->Item(count), 3);
                else
                    tmp = fileManager->InsertItem(count, displayNames->Item(count), 0);

            }
            else
            {
                if (displayNames->Item(count).Mid(displayNames->Item(count).Length()-4, 4) == wxT(".mp3"))
                    tmp = fileManager->InsertItem(count, displayNames->Item(count), 2);
                else
                    tmp = fileManager->InsertItem(count, displayNames->Item(count), 1);
            
            }

            fileManager->SetItemData(tmp, count);

            // filesize
            if (displayPerm->Item(count)[0] == wxT('d') || displayNames->Item(count) == wxT(".."))
                fileManager->SetItem(count, 1, wxT("-"));
            else
                fileManager->SetItem(count, 1, displaySizes->Item(count));

            // file permissions
            fileManager->SetItem(count, 2, displayPerm->Item(count));
        }

        // show
        fileManager->Show(true);
    


}


// This function extracts the data from a ftp list string
void UploadDialog::ExtractListData (wxString string, wxArrayString *results)
{
    int index;
    wxString permissions;
    wxString name;
    wxString size;
    wxString isDir = wxT("no");

    // trim string
    string.Trim();

    
    // extract data left to right

    // extract permissions
    index = string.Find(' ');
    permissions = string.Mid(0, index);
    string = string.Mid(index+1, string.Length());      
    string.Trim(false);
    
    // junk it
    index = string.Find(' ');
    string = string.Mid(index+1, string.Length());      
    string.Trim(false);

    // junk it
    index = string.Find(' ');
    string = string.Mid(index+1, string.Length());      
    string.Trim(false);

    // junk it
    index = string.Find(' ');
    string = string.Mid(index+1, string.Length());      
    string.Trim(false);

    // extract filesize
    index = string.Find(' ');
    size = string.Mid(0, index);
    string = string.Mid(index+1, string.Length());      
    string.Trim(false);

    // junk it
    index = string.Find(' ');
    string = string.Mid(index+1, string.Length());      
    string.Trim(false);

    // junk it
    index = string.Find(' ');
    string = string.Mid(index+1, string.Length());      
    string.Trim(false);

    // junk it
    index = string.Find(' ');
    string = string.Mid(index+1, string.Length());      
    string.Trim(false);

    // extract filename
    name = string.Mid(0, string.Length());
    string.Trim();
    string.Trim(false);

    if (permissions[0] == wxT('d'))
        isDir = wxT("yes");
    else
        isDir = wxT("no");

    results->Add(name);
    results->Add(permissions);
    results->Add(size);
    results->Add(isDir);

}

void UploadDialog::DownloadFile (wxString src, wxString dest)
{

    wxInputStream *in;
    wxFileOutputStream *out;
    size_t size;
    char *chunk;
    int iterations;
    int count;

    if (!abort)
    {
        in = ftp->GetInputStream(src);

        if ( !in )
        {
                wxLogError(_("Could not get file: ")+src);
        }
        else
        {
                
            size = in->GetLength();

            if (size > CHUNKSIZE)
                chunk = new char[CHUNKSIZE];
            else
                chunk = new char[size];

            progress = new wxProgressDialog(_("Download Progress"), wxT(""), 100, this, wxPD_CAN_ABORT);
            progress->SetCursor(wxCURSOR_ARROWWAIT);    

            // create output file
            out = new wxFileOutputStream(dest);

            if (size > 0)
            {
                // find number of iterations needed
                if (size > CHUNKSIZE)
                    iterations = size / CHUNKSIZE;
                else
                    iterations = 1;
                
                for (count = 0; count < iterations; count++)
                {
                    if (!abort)
                    {
                        if (size > CHUNKSIZE)
                        {
                            in->Read(chunk, CHUNKSIZE);
                            out->Write(chunk, CHUNKSIZE);
                        }
                        else
                        {
                            in->Read(chunk, size);
                            out->Write(chunk, size);
                        }
                    }

                    int percent = (int)(((float)count / (float)iterations)*100.0);
                        
                    if (!progress->Update(percent, src))
                    {
                        //wxMessageBox("ABORT", _T("FTP Status"), wxOK | wxICON_INFORMATION, NULL);

                        abort = true;
                        break;
                    }
                        
                }

                if (!abort)
                {
                    int remSize = size - (CHUNKSIZE * iterations);

                    if (remSize > 0)
                    {

                        char *endChunk = new char[remSize];

                        in->Read(endChunk, remSize);
                        out->Write(endChunk, remSize);

                        delete [] endChunk;
                    }
                }
            }

            delete in;
            delete out;
            delete [] chunk;

            if (abort)
            {
                wxRemoveFile(dest);         
                ftp->Reconnect();
            }

            progress->Destroy();
        }       
    }
}

void UploadDialog::UploadFile(wxString src, wxString dest)
{
    wxFileInputStream *in_stream;
    size_t size;
    char *chunk;
    int iterations;
    int count;

    in_stream = new wxFileInputStream(src);

    size = in_stream->GetLength();
        
    if (size > CHUNKSIZE)
        chunk = new char[CHUNKSIZE];
    else
        chunk = new char[size];

    // create progress bar
    progress = new wxProgressDialog(_("Upload Progress"), wxT(""), 100, this, wxPD_CAN_ABORT);
    progress->SetCursor(wxCURSOR_ARROWWAIT);

    // find number of iterations needed
    if (size > CHUNKSIZE)
        iterations = size / CHUNKSIZE;
    else
        iterations = 1;
        
    // create output file on server
    dest.Replace(wxT(" "), wxT("_"), true);
    dest = dest.MakeLower();
        
    wxOutputStream *out_stream = ftp->GetOutputStream(dest);    

    if (size > 0)
        {
            for (count = 0; count < iterations; count++)
            {
                if (size > CHUNKSIZE)
                {
                    in_stream->Read(chunk, CHUNKSIZE);
                    out_stream->Write(chunk, CHUNKSIZE);
                }
                else
                {
                    in_stream->Read(chunk, size);
                    out_stream->Write(chunk, size);
                }

                int percent = (int)(((float)count / (float)iterations)*100.0);
                    
                // result is false, user clicked abort
                if(!progress->Update(percent, dest))
                {
                    abort = true;
                        break;
                }
                    
            }
    
            if (!abort)
            {
                int remSize = size - (CHUNKSIZE * iterations);

                if (remSize > 0)
                {

                    char *endChunk = new char[remSize];

                    in_stream->Read(endChunk, remSize);
                    out_stream->Write(endChunk, remSize);

                    delete [] endChunk;
                }
            }

        }

    delete [] chunk;
    delete in_stream;
    delete out_stream;

    // if aborted, delete incomplete file
    if (abort)
        ftp->RmFile(dest);

    progress->Destroy();
}

void UploadDialog::UploadDir (wxString src, wxString dest)
{

    wxDir *dir = new wxDir();
    wxArrayString *files = new wxArrayString();
    wxArrayString *dirs = new wxArrayString();
    wxString debug;
    wxString name;
    bool unique;
    int index;
    unsigned int count, count2;

    // get list of directories
    dir->GetAllFiles(src, files, wxT(""), wxDIR_FILES | wxDIR_DIRS);

    // extract directories from filenames
    for (count = files->GetCount()-1; count > 0; count--)
    {

        // remove filename
        index = files->Item(count).Find(wxT('\\'), true);
        name = files->Item(count).Mid(0, index);

        // remove root dir
        index = name.Find(wxT('\\'));
        name = name.Mid(index, name.Length() - index);
        
        // replace '\' with '/'
        name.Replace(wxT("\\"), wxT("/"), true);
        name = name.MakeLower();

        unique = true;
        for (count2 = 0; count2 < dirs->GetCount(); count2++)
        {
            if (name == dirs->Item(count2))
            {
                unique = false;
                break;
            }
        }

        if (unique)
            dirs->Add(name);
        
    }

    // create directories on server
    for (count = 0; count < dirs->GetCount(); count++)
    {
        wxString dirname = ftp->Pwd()+dirs->Item(count);
        ftp->MkDir(dirname.MakeLower());
    }

    // upload files
    for (count = 0; count < files->GetCount(); count++)
    {
        if (!abort)
        {
            index = files->Item(count).Find(wxT('\\'));
            name = files->Item(count).Mid(index, files->Item(count).Length() - index);

            name.Replace(wxT("\\"), wxT("/"), true);
            name = ftp->Pwd()+name;
            
            UploadFile(files->Item(count), name);
        }
    }

    RefreshFiles();
}

void UploadDialog::DownloadDir (wxString src, wxString dest)
{
    if (!abort)
    {
    wxString dirName;

    if (dest.Last() == wxT('\\'))
        dirName = dest+src;
    else
        dirName = dest+wxT("\\")+src;

    // create directory on users computer
    if (!wxDirExists(dirName))
        wxMkdir(dirName);
    
    // change to src directory
    if (ftp->ChDir(src))
    {
        GetDirContents();
        
        // loop through each item and download
        unsigned int count;
        for (count = 0; count < displayNames->GetCount(); count++)
        {
            if (!abort)
            {
                bool isDir = false;
                wxString destName;

                if (displayNames->Item(count) != wxT(".."))
                {
                    if (displayPerm->Item(count).GetChar(0) == wxT('d'))
                        isDir = true;

                    //destName = dirName+"\\"+displayNames->Item(count);

                    DownloadItem(displayNames->Item(count), dirName, isDir, true);
                }
            }
        }

        // switch back to parent
        ftp->ChDir(wxT(".."));
        GetDirContents();

    }
    else
        wxMessageBox(_("Cannot copy folder: ")+src, _("Error"), wxOK, NULL);

    }


}

void UploadDialog::DownloadItem (wxString &src, wxString &dest,
                                 bool dir, bool multi)
{
    if (abort)
        return;

    if (dir)
    {
        if (multi)
        {
            DownloadDir(src, dest);
        }

        else
        {
            wxDirDialog *saveDir = new wxDirDialog(this, _("Download Folder"), wxT(""));
            int result = saveDir->ShowModal();

            if (result == wxID_OK)
                DownloadDir(src, saveDir->GetPath());
        }

    }
    else
    {
        // if part of a multi file download, don't ask what to save file as...
        // just save to dest argument
        if (multi)
        {
            DownloadFile(src, dest+wxT("\\")+src);
        }
        else
        {
            wxFileDialog *saveFile = new wxFileDialog(this, _("Download File"),
                                                      wxT(""), src,
                                                      _("All Files (*.*)|*.*"), wxSAVE);
            int result = saveFile->ShowModal();
    
            if (result == wxID_OK)
                DownloadFile(src, saveFile->GetPath());
        }
    }
}

void UploadDialog::DownloadItem (wxString &src, bool dir)
{
    if (abort)
        return;

    if (dir)
    {
        
        wxDirDialog *saveDir = new wxDirDialog(this, _("Download Folder"), wxT(""));
        int result = saveDir->ShowModal();

        if (result == wxID_OK)
            DownloadDir(src, saveDir->GetPath());

    }
    else
    {

        wxFileDialog *saveFile = new wxFileDialog(this, _("Download File"), wxT(""), src, _("All Files (*.*)|*.*"), wxSAVE);
        int result = saveFile->ShowModal();
    
        if (result == wxID_OK)
            DownloadFile(src, saveFile->GetPath());
        
    }
}

void UploadDialog::DownloadMultipleItems (void)
{

    long item = -1;

    // get save directory
    wxDirDialog *saveDir = new wxDirDialog(this, _("Download Files"), wxT(""));
    saveDir->ShowModal();

    // download selected items
    item = fileManager->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    while (item != -1)
    {
        // if directory
        if (displayPerm->Item(item).GetChar(0) == wxT('d'))
        {
            //DownloadItem(fileManager->GetItemText(listIndex), saveDir->GetPath(), true, true);    
            DownloadDir(displayNames->Item(item), saveDir->GetPath()+wxT("\\"));
    
        }
        else
        {
            DownloadFile(displayNames->Item(item), saveDir->GetPath()+wxT("\\")+displayNames->Item(item));
        }

        // get next item
        item = fileManager->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

    }

}

void UploadDialog::OnCreateDir(wxCommandEvent &event)
{

    wxString result = wxGetTextFromUser(_("Please enter new folder name:"), _("New Folder"), wxT(""), this);

    if (result != wxT(""))
    {
        result.Replace(wxT(" "), wxT("_"), true);

        if (ftp->MkDir(result.MakeLower()))
            RefreshFiles();
    }

}

void UploadDialog::OnNewSite (wxCommandEvent &event)
{
    int count;
    int index;
    long tmp;
    wxListItem listItem;
    wxString newSite = _("New Site");
    
    index = siteList->GetItemCount();

    tmp = siteList->InsertItem(index, newSite, 0);
    siteList->SetItemData(tmp, 0);

    // make sure all items are deselected
    for (count = 0; count < siteList->GetItemCount(); count++)
        siteList->SetItemState(count, 0, wxLIST_STATE_SELECTED);
    
    // select added item
    siteList->SetItemState(tmp, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

    txtFtpName->SetValue(newSite);

    ftpIndex = tmp;
}

void UploadDialog::OnDeleteSite (wxCommandEvent &event)
{

    wxString debug;

//  debug.Printf("%d", index);
//  wxMessageBox(debug, _T("FTP Status"), wxOK | wxICON_INFORMATION, this);
    
    wxTextFile *file = new wxTextFile(wxT("connections.ini"));

    if (file->Exists())
    {
        file->Open();

        file->RemoveLine(ftpIndex);
        file->RemoveLine(ftpIndex);
        file->RemoveLine(ftpIndex);
        file->RemoveLine(ftpIndex); 

        file->Write();
        file->Close();

        siteList->DeleteItem(ftpIndex);
    }

    UpdateSiteList();

}

void UploadDialog::OnFtpChange (wxCommandEvent &event)
{

    siteList->SetItemText(ftpIndex, txtFtpName->GetValue());
}

void UploadDialog::OnSelectSite (wxListEvent &event)
{

    currentFtp = event.m_itemIndex;
    ftpIndex = event.m_itemIndex;

    txtFtpName->SetValue(ftpList->Item((ftpIndex * 4)+0));
    txtFtpHost->SetValue(ftpList->Item((ftpIndex * 4)+1));
    txtFtpUser->SetValue(ftpList->Item((ftpIndex * 4)+2));
    txtFtpPass->SetValue(ftpList->Item((ftpIndex * 4)+3));

    //wxString debug;
    //debug.Printf("ftpIndex %d", ftpIndex);
    //wxMessageBox(debug, _T("FTP Status"), wxOK | wxICON_INFORMATION, NULL);


}

bool UploadDialog::SaveFtpSite(wxString name, wxString host, wxString user, wxString pass)
{

    wxTextFile *file = new wxTextFile(wxT("connections.ini"));

    if (file->Exists())
        file->Open();
    else
    {
        file->Create();
        file->Open();
    }

    // see if this ftp is already in our file
    unsigned int count = 0;
    int found = -1;

    while (count < file->GetLineCount())
    {
    
        if (file->GetLine(count) == name)
        {
            found = count;
            break;
        }

        count += 4;
    }

    // add new site
    if (found == -1)
    {
        file->AddLine(name);
        file->AddLine(host);
        file->AddLine(user);
        file->AddLine(pass);
        
        file->Write();
        file->Close();

        return true;

    }
    else
    {
        file->RemoveLine(found);
        file->RemoveLine(found);
        file->RemoveLine(found);
        file->RemoveLine(found);

        file->InsertLine(pass, found);
        file->InsertLine(user, found);
        file->InsertLine(host, found);
        file->InsertLine(name, found);
        
        file->Write();
        file->Close();

        return false;
    }




}

void UploadDialog::LoadFtpSiteList (void)
{
    wxTextFile *file = new wxTextFile(wxT("connections.ini"));
    unsigned int count;

    if (file->Exists())
    {
        file->Open();
    
        ftpList->Empty();
    
        for (count = 0; count < file->GetLineCount(); count++)
            ftpList->Add(file->GetLine(count));         
    
        file->Close();
    }
        
}

void UploadDialog::UpdateSiteList (void)
{
    unsigned int count;
    unsigned int insert_count;
    long tmp;

    LoadFtpSiteList();


    count = 0;
    insert_count = 0;
    
    siteList->DeleteAllItems();

    while (count < ftpList->GetCount())
    {
        //wxMessageBox(ftpList->Item(count), _T("Error"), wxOK, NULL);

        tmp = siteList->InsertItem(insert_count, ftpList->Item(count), 0);
        siteList->SetItemData(tmp, insert_count);

        count += 4;
        insert_count++;
    }

}

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 4
// indent-tabs-mode: nil
// End:
//
// vim: et sts=4 sw=4

