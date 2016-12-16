
/*

  Copyright (C) 2009, Alexey Sudachen, https://goo.gl/lJNXya.

*/

#include "myafx.h"
#include "wx/splitter.h"
#include "wx/treectrl.h"
#include "wx/listctrl.h"
#include "wx/toolbar.h"
#include "wx/filedlg.h"
#include "wx/imagpng.h"
#include "wx/bmpbuttn.h"
#include "wx/dirdlg.h"
#include "wx/help.h"
#include "wx/dnd.h"

//#include "xpm/appicon_xpm.xpm"

#include "xpm/file.xpm"
#include "xpm/folder.xpm"
#include "xpm/new_xpm.xpm"
#include "xpm/open_xpm.xpm"
#include "xpm/save_xpm.xpm"
#include "xpm/pack_xpm.xpm"
#include "xpm/conf_xpm.xpm"

#include "xpm/subfold_xpm.xpm"
#include "xpm/addfold_xpm.xpm"
#include "xpm/delfold_xpm.xpm"
#include "xpm/addfile_xpm.xpm"
#include "xpm/delfile_xpm.xpm"
#include "xpm/edit_xpm.xpm"
#include "xpm/file_rename_xpm.xpm"
#include "xpm/fold_rename_xpm.xpm"


#include "xpm/help_xpm.xpm"
#include "xpm/info_xpm.xpm"
#include "xpm/save_exit_xpm.xpm"
#include "xpm/vline_xpm.xpm"
#include "xpm/activateG_xpm.xpm"
#include "xpm/activateR_xpm.xpm"

#include "mxf.h"

wxWindow *mainframe = 0;
  
#define wxFD_FILE_MUST_EXIT 0
MxfFolderPtr g_root_mxfFolder;
MxfFolderPtr g_curr_mxfFolder;

void Mainframe_Update_Tree_List(int expand_tree_pane);

void AddFolder(StringW rfold)
  {
    StringEfW ehf;
    ArrayT<StringW> names;
    DirList(rfold,L"*",names,DIRLIST_FILESONLY|DIRLIST_RECURSIVE);
    if ( names.Count() )
      {
        StringW folder;
        StringW f = GetBasenameOfPath(rfold) + "/";
        rfold = GetDirectoryOfPath(rfold);
        for ( int i = 0; i < names.Count(); ++i )
          names[i].Insert(0,+f);
        if ( QueryAddFiles(mainframe,names,folder) )
          {
            if ( folder ) folder += "/";
            int old_subcount = g_root_mxfFolder->SubCount();
            for ( int i = 0; i < names.Count(); ++i )
              {
                StringW path = JoinPath(rfold,names[i]);
                StringW left = GetDirectoryOfPath(names[i]);
                StringW right = GetBasenameOfPath(names[i]);
                path.Replace(L"/",L'\\');
                MxfAddFileIntoFolder(folder+left,right,path,ehf);
              }
            Mainframe_Update_Tree_List( !old_subcount && g_root_mxfFolder->SubCount() );
          }
      }
  }
   
void ShowHelpSection(StringParam name)
  {
    static wxHelpController *help = 0;
    if ( !help )
      {
        help = new wxHelpController();
        help->Initialize(+AtAppFolder(L"molebox.chm"));
      }
    
    //help->DisplayContents();
    help->DisplaySection(+name);
  }

struct MyFolderTreeItemData: wxTreeItemData
  {
    MxfFolderPtr mxff_;
    MyFolderTreeItemData(MxfFolderPtr mxff)
      : mxff_(mxff)
      {}
    MxfFolderPtr &MxF() { return mxff_; }
  };

template<class T>
  struct Panelized : wxPanel
    {
      T *el_;
      Panelized(wxWindow *parent) : wxPanel(parent), el_(0)
      {
        el_ = new T(this);
      }
    void Resize()
      {
        if (el_)
          {
            wxSize sz = GetClientSize();
            el_->SetSize(0,0,sz.x,sz.y);
          }
      }
    void OnSize(wxSizeEvent &e)
      {
        Resize();
      }
    T *Obj() { return el_; }
    DECLARE_EVENT_TABLE()
  };
#define BEGIN_PANELIZED_EVENT_TABLE(c) \
BEGIN_EVENT_TABLE(c, wxPanel)\
    EVT_SIZE(c::OnSize)\
END_EVENT_TABLE()\

struct FoldersTree: wxTreeCtrl
  {
    FoldersTree(wxWindow *parent) 
      : wxTreeCtrl(parent,ctID_FOLDERSTREE,wxDefaultPosition,wxDefaultSize,
              wxTR_HAS_BUTTONS|wxTR_SINGLE|wxTR_FULL_ROW_HIGHLIGHT|wxSUNKEN_BORDER)
      {
        wxImageList *imglist = new wxImageList(16,16);
        imglist->Add(wxIcon(folder_xpm));
        imglist->Add(wxIcon(file_xpm));
        AssignImageList(imglist);
      }
    void Initialize() { ResetTree(); }
    void ResetTree_(MxfFolderPtr &mxff)
      {
        wxTreeItemId tid;
        if ( MxfFolder *parent = mxff->GetParent() )
          tid = AppendItem(parent->GetTid(),+mxff->GetName(), 0, 0, 0);
        else
          tid = AddRoot(L"VFSROOT", 0, 0, 0);
        mxff->SetTid(tid);
        SetItemData(tid,new MyFolderTreeItemData(mxff));
        for ( MxfFolderPtr* i = mxff->Begin(); i != mxff->End(); ++i )
          ResetTree_(*i);
      }
    void ResetTree()
      {
        DeleteAllItems();
        ResetTree_(g_root_mxfFolder);
        ExpandAll();
        SelectItem(g_root_mxfFolder->GetTid(),true);
      }
      
    void UpdateTree_(MxfFolderPtr &mxff)
      {
        wxTreeItemId tid;
        if ( !mxff->GetTid() ) 
          if ( MxfFolder *parent = mxff->GetParent() )
            {
              tid = AppendItem(parent->GetTid(),+mxff->GetName(), 0, 0, 0);
              mxff->SetTid(tid);
              SetItemData(tid,new MyFolderTreeItemData(mxff));
              for ( MxfFolder *p = parent; p; p = p->GetParent() )
                Expand(p->GetTid());
            }
        for ( MxfFolderPtr* i = mxff->Begin(); i != mxff->End(); ++i )
          UpdateTree_(*i);
      }
      
    void UpdateTree()
      {
        UpdateTree_(g_root_mxfFolder);
      }
      
    void OnSelChanged(wxTreeEvent &e)
      {
        wxTreeItemId tid = GetSelection();
        MxfFolderPtr mxff;
        if ( MyFolderTreeItemData *dat = (MyFolderTreeItemData *)GetItemData(tid) )
          {
            mxff = dat->MxF();
            if ( mxff != g_curr_mxfFolder )
              {
                if ( g_curr_mxfFolder )
                  g_curr_mxfFolder->UnselectAll();
                if ( mxff )          
                  {
                    g_curr_mxfFolder = mxff;
                    mxff->UnselectAll();
                  }
                e.Skip();
              }
          }
      }
    DECLARE_EVENT_TABLE()
  };
typedef Panelized<FoldersTree> PanelizedFoldersTree;
BEGIN_PANELIZED_EVENT_TABLE(PanelizedFoldersTree)
BEGIN_EVENT_TABLE(FoldersTree, wxTreeCtrl)
    EVT_TREE_SEL_CHANGED(ctID_FOLDERSTREE, FoldersTree::OnSelChanged)
END_EVENT_TABLE()
  
struct FilesList: wxListCtrl
  {
    FilesList(wxWindow *parent) 
      : wxListCtrl(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,
              wxLC_REPORT|wxSUNKEN_BORDER)
      {
        wxImageList *imglist = new wxImageList(16,16);
        imglist->Add(wxIcon(file_xpm));
        imglist->Add(wxIcon(file_xpm));
        AssignImageList(imglist,wxIMAGE_LIST_SMALL);
        InsertColumn(0, Tr(24,"File name"), wxLIST_FORMAT_LEFT, 200);
        InsertColumn(1, Tr(25,"File path on local Disk"), wxLIST_FORMAT_LEFT, 300);
      }
    
    void ResetList()
      {
        DeleteAllItems();
        MxfFolderPtr mxff = g_curr_mxfFolder;
        if ( mxff )
          {
            for ( int i = 0; i < mxff->FilesCount(); ++i )
              { 
                MxfFilePtr f = mxff->File(i);
                InsertItem(i, +f->GetName(), 0);
                SetItemPtrData(i,(wxUIntPtr)+f);
                SetItem(i,1,+f->GetDiskPath());
              }
          }
      }

    void SetSize(int x, int y, int width, int height)
      {
        wxListCtrl::SetSize(x,y,width,height);
        SetColumnWidth(1,width-GetColumnWidth(0)-10);
      }
      
    void GetSelectedFiles(ArrayT<MxfFilePtr> &files)
      {
        for ( int i = GetNextItem(-1,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED); i != -1; 
                  i = GetNextItem(i,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED) )
          {
            files.Push(RccRefe((MxfFile*)GetItemData(i)));
          }
      }
      
    DECLARE_EVENT_TABLE()
  };
typedef Panelized<FilesList> PanelizedFilesList;
BEGIN_PANELIZED_EVENT_TABLE(PanelizedFilesList)
BEGIN_EVENT_TABLE(FilesList, wxListCtrl)
END_EVENT_TABLE()

int Mainframe_QueryAddFiles(ArrayT<StringW> &,StringW &);

struct FilesDnD : wxFileDropTarget
  {
    FilesDnD(FilesList *owner) { owner_ = owner; }
 
    virtual bool OnDropFiles(wxCoord x, wxCoord y,
                             const wxArrayString& filenames)
      {
        size_t n = filenames.GetCount();
        ArrayT<StringW> files;
        ArrayT<StringW> dirs;
        for ( int i = 0; i < n; ++i ) 
          {
            if ( GetFileAttributesW(filenames[i].c_str()) & FILE_ATTRIBUTE_DIRECTORY )
                dirs.Push(filenames[i].c_str().AsWChar());
            else 
                files.Push(filenames[i].c_str().AsWChar());
          }
        if ( files.Count() )
          {
            StringEfW ehf;
            ArrayT<StringW> names;
            for ( int i = 0; i < files.Count(); ++i )
              {
                names.Push(GetBasenameOfPath(files[i]));
              }
            StringW folder;
            if ( QueryAddFiles(mainframe,names,folder) )
              {
                for ( int i = 0; i < names.Count(); ++i )
                  MxfAddFileIntoFolder(folder,names[i],files[i],ehf);
                owner_->ResetList();
              }
          }
        if ( dirs.Count() )
          {
            for ( int i = 0; i < dirs.Count(); ++i )
              {
                AddFolder(dirs[i]);
              }
          }
        return true;
      }
 
    FilesList *owner_;
  };

wchar_t startdir[1024] = {0};

struct MyMainFrame: wxFrame
  {
    FoldersTree  *folders_;
    FilesList    *files_;
    wxSplitterWindow *splitter_;
    wxBitmapButton   *activate_button_;

    void SetupActivateButton(wxToolBar *tbar)
      {
        // do nothing now
      }
    
    wxPanel *CreateControls(wxWindow *parent)
      {
        wxPanel *pan = new wxPanel(parent);
        wxBoxSizer *h0 = new wxBoxSizer(wxHORIZONTAL);
        wxBoxSizer *h1 = new wxBoxSizer(wxHORIZONTAL);
        wxBoxSizer *v0 = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer *topszr = new wxStaticBoxSizer(new wxStaticBox(pan,wxID_ANY,L""),wxHORIZONTAL);
        //topszr->Add(new wxBitmapButton(pan,wxID_OPEN,wxBitmap(open_xpm),wxDefaultPosition, wxSize(48,48)), 0, wxALIGN_TOP, 0 );
        //topszr->Add(new wxBitmapButton(pan,wxID_SAVE,wxBitmap(save_xpm),wxDefaultPosition, wxSize(48,48)), 0, wxALIGN_TOP, 0 );
        //topszr->Add(new wxBitmapButton(pan,myID_PKGCONFIGURE,wxBitmap(conf_xpm),wxDefaultPosition, wxSize(48,48)), 0, wxALIGN_TOP, 0 );
        //topszr->Add(new wxBitmapButton(pan,myID_PKGDOPACK,wxBitmap(pack_xpm),wxDefaultPosition, wxSize(48,48)), 0, wxALIGN_TOP, 0 );
        wxTextCtrl *t0;
        h0->Add(new wxStaticText(pan,wxID_ANY,L"Exe-FILE",wxDefaultPosition,wxSize(50,-1),wxALIGN_RIGHT), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
        h0->Add(t0 = new wxTextCtrl(pan,ctID_INPUTTEXT3,wxEmptyString,wxDefaultPosition,wxDefaultSize,wxTE_LEFT), 1, wxRIGHT|wxGROW|wxALIGN_CENTER_VERTICAL, 3);
        t0->Enable(false);
        v0->Add(h0, 1, wxALL|wxGROW|wxALIGN_LEFT, 1 );
        wxTextCtrl *t1;
        wxTextCtrl *t2;
        wxButton *b0;
        h1->Add(new wxStaticText(pan,wxID_ANY,L"Exe-CRC",wxDefaultPosition,wxSize(50,-1),wxALIGN_RIGHT), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
        h1->Add(t1 = new wxTextCtrl(pan,ctID_INPUTTEXT3,wxEmptyString,wxDefaultPosition,wxSize(80,-1),wxTE_LEFT),0,wxRIGHT|wxALIGN_CENTER_VERTICAL,0);
        h1->Add(new wxStaticText(pan,wxID_ANY,L"Packing Token",wxDefaultPosition,wxSize(90,-1),wxALIGN_RIGHT), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
        h1->Add(t2 = new wxTextCtrl(pan,ctID_INPUTTEXT3,wxEmptyString,wxDefaultPosition,wxDefaultSize,wxTE_LEFT),1,wxRIGHT|wxGROW|wxALIGN_CENTER_VERTICAL,8);
        h1->Add(new wxButton(pan,ctID_BROWSE_SOURCE,L"What is it?",wxDefaultPosition,wxSize(60,-1)), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
        h1->Add(b0 = new wxButton(pan,ctID_BROWSE_SOURCE,L"Get One",wxDefaultPosition,wxSize(60,-1)), 0, wxRIGHT|wxALIGN_CENTER_VERTICAL, 5);
        v0->Add(h1, 1, wxALL|wxGROW|wxALIGN_LEFT, 1 );
        t1->Enable(false);
        t1->SetValue(L"unknown");
        t2->Enable(false);
        b0->Enable(false);
        topszr->Add(v0, 1, wxALL|wxGROW|wxALIGN_TOP, 2 );
        pan->SetSizer(topszr);
        topszr->Fit(parent);
        return pan;
      }
    
    MyMainFrame(StringParam title) : wxFrame(0, wxID_ANY, +title)
      {
        activate_button_ = 0;
        //SetIcon(wxIcon(appicon_xpm));
        //static wxIcon ico;
        //ico.SetHICON(LoadIconW(GetModuleHandle(0),L"WINICON"));
        //SetIcon(ico);
        SetClassLongPtr((HWND)GetHandle(),GCL_HICON,(LONG)LoadIconW(GetModuleHandle(0),L"APPICON"));
//        SetClassLongPtr((HWND)GetHandle(),GCL_HICON,(LONG)LoadIconW(GetModuleHandle(0),L"WINICON"));
        SetClassLongPtr((HWND)GetHandle(),GCL_HICONSM,0);
        wxMenu *file_menu = new wxMenu;
        wxMenu *help_menu = new wxMenu;
        wxMenu *tool_menu = new wxMenu;
        wxMenu *edit_menu = new wxMenu;
        wxMenuBar *mbar = new wxMenuBar;

        edit_menu->Append(myID_ADDFOLDER,Tr(26,"Add f&older ..."),Tr(27,"Add folder content recusively"));
        edit_menu->Append(myID_ADDFILES,Tr(28,"Add f&iles ..."),Tr(29,"Add files to current virtual folder"));
        edit_menu->Append(myID_DELFOLDER,Tr(30,"Del fol&der ..."),Tr(31,"Remove selected folder from vritual fs"));
        edit_menu->Append(myID_DELFILES,Tr(32,"Del fil&es ..."),Tr(33,"Remove selected files from virtual fs"));
        //edit_menu->Append(myID_EDIT,Tr(34,"Edi&t ..."),Tr(35,"Edit file(s) or folder identity"));
        tool_menu->Append(myID_PKGDOPACK,Tr(36,"&Pack ..."),Tr(37,"Pack files to one package"));
        tool_menu->Append(myID_PKGCONFIGURE,Tr(38,"&Configure ..."),Tr(39,"Configure options of packing process"));
        //help_menu->Append(wxID_HELP,L"&Help ...",L"Show program help");
        help_menu->Append(wxID_ABOUT,Tr(40,"&About ..."),Tr(41,"Show about dialog"));
        //help_menu->AppendSeparator();
        //help_menu->Append(wxID_HELP,Tr(0,"Molebox &Help"),Tr(0,"Show Help Content"));
        file_menu->Append(wxID_NEW,Tr(44,"&New"),Tr(45,"New Config Script"));
        file_menu->Append(wxID_OPEN,Tr(46,"&Open"),Tr(47,"Load Config Script"));
        file_menu->Append(wxID_SAVE,Tr(48,"&Save"),Tr(49,"Save Config Script"));
        file_menu->Append(wxID_SAVEAS,Tr(50,"Save &As ..."),Tr(51,"Save Config Script As ..."));
        file_menu->AppendSeparator();
        file_menu->Append(wxID_EXIT,Tr(52,"E&xit"),Tr(53,"Quit this program"));
        file_menu->Append(myID_SAVEnEXIT,Tr(54,"Save and &Exit"),Tr(55,"Save Config Script and Quit this program"));
        mbar->Append(file_menu,Tr(56,"&File"));
        mbar->Append(tool_menu,Tr(57,"&Tool"));
        mbar->Append(edit_menu,Tr(58,"&Edit"));
        mbar->Append(help_menu,Tr(59,"&Help"));
        SetMenuBar(mbar);
        
        ///*
        wxToolBar *tbar = new wxToolBar(this,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxTB_HORIZONTAL|wxNO_BORDER);
        //tbar->AddTool(wxID_NEW,wxBitmap(new_xpm), L"Make New Package");
        tbar->SetToolSeparation(32);
        tbar->AddTool(wxID_OPEN,"",wxBitmap(open_xpm),Tr(60,"Load Package Config form MXF file"));
        tbar->AddTool(wxID_SAVE,"",wxBitmap(save_xpm),Tr(61,"Save Package Config to MXF file"));
        //tbar->AddTool(myID_SPLITTER1,wxBitmap(vline_xpm),(wchar_t*)0);
        tbar->AddTool(myID_PKGCONFIGURE,"",wxBitmap(conf_xpm),Tr(62,"Configure Package"));
        tbar->AddTool(myID_PKGDOPACK,"",wxBitmap(pack_xpm),Tr(63,"Pack Package"));
        tbar->AddTool(myID_SPLITTER2,"",wxBitmap(vline_xpm),(wchar_t*)0);
        tbar->AddTool(myID_NEWSUBFOLDER,"",wxBitmap(subfold_xpm),Tr(64,"New Subfolder"));
        tbar->AddTool(myID_ADDFOLDER,"",wxBitmap(addfold_xpm),Tr(65,"Add Folder Recursive"));
        tbar->AddTool(myID_DELFOLDER,"",wxBitmap(delfold_xpm),Tr(66,"Delete Folder"));
        tbar->AddTool(myID_FOLDRENAME,"",wxBitmap(fold_rename_xpm),Tr(67,"Rename/Move Folder"));
        tbar->AddTool(myID_ADDFILES,"",wxBitmap(addfile_xpm),Tr(68,"Add Files"));
        tbar->AddTool(myID_DELFILES,"",wxBitmap(delfile_xpm),Tr(69,"Delete Files"));
        tbar->AddTool(myID_FILERENAME,"",wxBitmap(file_rename_xpm),Tr(70,"Rename/Move File"));
        tbar->AddTool(myID_SPLITTER3,"",wxBitmap(vline_xpm),(wchar_t*)0);
        tbar->AddTool(wxID_HELP,"",wxBitmap(help_xpm),L"Open Help");
        //tbar->AddTool(ctID_HELP,wxBitmap(info_xpm),Tr(0,"Help"));
        //tbar->AddTool(wxID_ABOUT,wxBitmap(info_xpm),Tr(71,"About This Program"));
        tbar->AddTool(myID_SAVEnEXIT,"",wxBitmap(save_exit_xpm),Tr(72,"Save and Exit"));
        SetupActivateButton(tbar);
        tbar->Realize();
        SetToolBar(tbar);
        
        tbar->EnableTool(myID_SPLITTER1,false);
        tbar->EnableTool(myID_SPLITTER2,false);
        tbar->EnableTool(myID_SPLITTER3,false);
        //* /

        CreateStatusBar(1);
        SetStatusText(Tr(73,"Welcome to Molebox Ultra!"));
        
        wxPanel *pan = new wxPanel(this);
        wxBoxSizer *topszr = new wxBoxSizer(wxVERTICAL);
        splitter_ = new wxSplitterWindow(pan,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxSP_3D);
        //topszr->Add(CreateControls(pan), 0, wxGROW|wxLEFT|wxALIGN_LEFT|wxRIGHT|wxBOTTOM, 5 );
        topszr->Add(splitter_, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxALIGN_LEFT|wxGROW, 5 );
        pan->SetSizer(topszr);
        wxPanel *right_pan = new wxPanel(splitter_);
        wxPanel *left_pan = new wxPanel(splitter_);
        PanelizedFoldersTree *left = new PanelizedFoldersTree(left_pan);
        folders_ = left->Obj();
        folders_->Initialize();
        PanelizedFilesList *right = new PanelizedFilesList(right_pan);
        files_ = right->Obj();
        wxBoxSizer *leftszr = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer *rightszr = new wxBoxSizer(wxVERTICAL);
        //wxBoxSizer *btns = new wxBoxSizer(wxHORIZONTAL);
        //wxPanel *btnspan = left_pan;
        //rightbtns->Add(new wxBitmapButton(right_pan,wxID_OPEN,wxBitmap(open_xpm),wxDefaultPosition, wxSize(34,34)), 0, wxALIGN_TOP, 0 );
        //btns->Add(new wxBitmapButton(btnspan,myID_NEWSUBFOLDER,wxBitmap(subfold_xpm),wxDefaultPosition, wxSize(34,34)), 0, wxALIGN_TOP, 0 );
        //btns->Add(new wxBitmapButton(btnspan,myID_ADDFOLDER,wxBitmap(addfold_xpm),wxDefaultPosition, wxSize(34,34)), 0, wxALIGN_TOP, 0 );
        //btns->Add(new wxBitmapButton(btnspan,myID_DELFOLDER,wxBitmap(delfold_xpm),wxDefaultPosition, wxSize(34,34)), 0, wxALIGN_TOP, 0 );
        //btns->Add(new wxBitmapButton(btnspan,myID_FOLDRENAME,wxBitmap(fold_rename_xpm),wxDefaultPosition, wxSize(34,34)), 0, wxALIGN_TOP, 0 );
        //btns->Add(new wxBitmapButton(btnspan,myID_ADDFILES,wxBitmap(addfile_xpm),wxDefaultPosition, wxSize(34,34)), 0, wxALIGN_TOP, 0 );
        //btns->Add(new wxBitmapButton(btnspan,myID_DELFILES,wxBitmap(delfile_xpm),wxDefaultPosition, wxSize(34,34)), 0, wxALIGN_TOP, 0 );
        //btns->Add(new wxBitmapButton(btnspan,myID_FILERENAME,wxBitmap(file_rename_xpm),wxDefaultPosition, wxSize(34,34)), 0, wxALIGN_TOP, 0 );
        
        rightszr->Add( right, 1 , wxGROW ); 
        //rightszr->Add( btns, 0 , 0 ); 
        leftszr->Add( left, 1 , wxGROW ); 
        //leftszr->Add( btns, 0 , 0 ); 
        
        right_pan->SetSizer(rightszr);
        rightszr->Fit(right_pan);
        left_pan->SetSizer(leftszr);
        leftszr->Fit(left_pan);
        splitter_->SplitVertically(left_pan,right_pan);
        splitter_->SetMinimumPaneSize(60);
        SetSize(wxSize(776,480+48));
        splitter_->SetSashPosition(100);
        
        files_->SetDropTarget(new FilesDnD(files_));
      }
            
    void OnQuit(wxCommandEvent &event)
      {
        Close();
      }
      
    void OnClose(wxCloseEvent &event)
      {
        //__asm int 3;
        if ( g_mxf_changed )
          if ( !AreYouSure(this,
            //L"Molebox Config Script has been changed.\n\nAre you sure to exit without saving?\n",
            Tr(74,"Molebox Config Script has been changed.\n\nAre you sure to exit without saving?\n"),
            Tr(75,"Exit without saving")) )
            return;
        event.Skip();
      }
      
    void OnNew(wxCommandEvent &event)
      {
        //__asm int 3;
        if ( g_mxf_changed )
          if ( !AreYouSure(this,
           Tr(145,"Package configuration has been modified.\n\nDiscard?\n"),
            Tr(146,"Discard package configuration")) )
            return;
        ClearMxfOpts();
        g_current_mxf_file_name = "";
        g_curr_mxfFolder = g_root_mxfFolder = RccPtr(new MxfFolder(0,"VFSROOT"));
        files_->DeleteAllItems();
        folders_->ResetTree();
        splitter_->SetSashPosition(100);
      }
      
    void OnAbout(wxCommandEvent &event)
      {
        ShowAboutInfo(this);
      }

    void OpenScript(pwide_t path)
      {
        ClearMxfOpts();
        g_root_mxfFolder = RccPtr(new MxfFolder(0,"VFSROOT"));
        g_curr_mxfFolder = MxfFolderPtr(0);
        StringEfW ehf = StringEfW();
        StringW mxfpath = path;
        if ( !ReadMxfConfig(mxfpath,ehf) )
          {
            DisplayError(this,
                      0|_S*Tr(78,"Could not read config script\n'%s'\n\n%s") %mxfpath %+ehf,
                      Tr(79,"Molebox Config Script file loading Error")
                      );
            g_curr_mxfFolder = g_root_mxfFolder = RccPtr(new MxfFolder(0,"VFSROOT"));
            ClearMxfOpts();
          }
        else
          g_current_mxf_file_name = mxfpath;
        files_->DeleteAllItems();
        folders_->ResetTree();                        
        if ( g_root_mxfFolder->SubCount() )
          splitter_->SetSashPosition(263);
        else
          splitter_->SetSashPosition(100);
      }
      
    void OnOpen(wxCommandEvent &event)
      {      
        wxString caption    = Tr(76,"Load Molebox Config Script");
        wxString wildcard   = Tr(77,"Molebox Config Script (*.mxb)|*.mxb");
        wxString defaultDir = wxEmptyString;//g_last_operated_directory;
        wxString defaultFilename = wxEmptyString;
        wxFileDialog dialog(this, caption, defaultDir, defaultFilename,wildcard, wxFD_OPEN|wxFD_FILE_MUST_EXIT);
        if (dialog.ShowModal() == wxID_OK)
          OpenScript(dialog.GetPath().c_str());
      }
    
    void SaveMXF(StringParam path)
      {
        StringEfW ehf = StringEfW();
        if ( !WriteMxfConfig(path,ehf) )
          {
            DisplayError(this,
                      0|_S*Tr(80,"Could not write config script\n'%s'\n\n%s") %+path %+ehf,
                      Tr(81,"Molebox Config Script file saving Error")
                      );
          }
        else
          {
            g_current_mxf_file_name = +path;
            g_mxf_changed = false;
          }
      }
    
    void OnSave(wxCommandEvent &event)
      {
        if ( !g_current_mxf_file_name )
          OnSaveAs(event);
        else
          SaveMXF(g_current_mxf_file_name);
      }
      
    void OnSaveAndExit(wxCommandEvent &event)
      {
        if ( g_mxf_changed )
          OnSave(event);
        Close();
      }
      
    void OnSaveAs(wxCommandEvent &event)
      {
        wxString caption    = Tr(82,"Save Molebox Config Script");
        wxString wildcard   = Tr(83,"Molebox Config Script (*.mxb)|*.mxb");
        wxString defaultDir = wxEmptyString;
        wxString defaultFilename = wxEmptyString;
        wxFileDialog dialog(this, caption, defaultDir, defaultFilename,wildcard, wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() == wxID_OK)
          SaveMXF(dialog.GetPath().c_str().AsWChar());
      }

    void OnTreeSelChanged(wxTreeEvent &e)
      {
        files_->ResetList();
      }
      
    void OnActivate(wxCommandEvent &event)
      {
        // do nothing now
      }
      
    void OnConfigure(wxCommandEvent &event)
      {
        ShowConfigDialog(this);
      }
      
    void OnPack(wxCommandEvent &event)
      {
        if ( !g_mxf_target )
          {
            DisplayError(this,Tr(113,"You should configure packing target before pack"),Tr(114,"Error"));
            OnConfigure(event);
            goto l;
          }
        if ( g_mxf_target && g_mxf_is_executable && !g_mxf_source )
          {
            DisplayError(this,Tr(115,"You should configure exectable source before pack"),Tr(116,"Error"));
            OnConfigure(event);
            goto l;
          }
        DoPackingProcess(this);
      l:;
      }
      
    void OnNewSubfolder(wxCommandEvent &event)
      {
        StringW subfolder;
        StringEfW ehf;
        int old_subcount = g_root_mxfFolder->SubCount();
        if ( QueryNewSubfolder(this,subfolder) )
          if ( !MxfNewSubfolder(subfolder,ehf) )
            DisplayError(this,
              0|_S*Tr(84,"Could not create folder '%s'\n\n%s")%+subfolder%+ehf,
              Tr(85,"Subfolder creation Error"));
          else
            {
              folders_->UpdateTree();
              if ( !old_subcount && g_root_mxfFolder->SubCount() )
                splitter_->SetSashPosition(263);
            }
      }

    void OnAddFiles(wxCommandEvent &event)
      {
        wxString caption    = Tr(86,"Add Files");
        wxString wildcard   = Tr(87,"All Files (*.*)|*.*");
        wxString defaultDir = wxEmptyString;//g_last_operated_directory;
        wxString defaultFilename = wxEmptyString;
        StringEfW ehf;
        wxFileDialog dialog(this, caption, defaultDir, defaultFilename,wildcard, wxFD_OPEN|wxFD_FILE_MUST_EXIT|wxFD_MULTIPLE);
        if (dialog.ShowModal() == wxID_OK)
          {
            wxArrayString wxsa;
            dialog.GetPaths(wxsa);
            ArrayT<StringW> paths;
            ArrayT<StringW> names;
            for ( int i = 0; i < wxsa.Count(); ++i )
              {
                paths.Push(wxsa[i].c_str().AsWChar());
                names.Push(GetBasenameOfPath(paths.Last()));
              }
            StringW folder;
            if ( QueryAddFiles(this,names,folder) )
              {
                for ( int i = 0; i < names.Count(); ++i )
                  MxfAddFileIntoFolder(folder,names[i],paths[i],ehf);
                files_->ResetList();
              }
          }     
      }
      
    void OnAddFolder(wxCommandEvent &event)
      {
        wxString caption    = Tr(88,"Add Folder Recursively");
        wxString defaultDir = wxEmptyString;//g_last_operated_directory;
        StringEfW ehf;
        wxDirDialog dialog(this, caption, defaultDir, wxDD_DEFAULT_STYLE);
        if (dialog.ShowModal() == wxID_OK)
          {
            StringW rfold = dialog.GetPath().c_str();
            AddFolder(rfold);
          }
      }
      
    void OnDeleteFiles(wxCommandEvent &e)
      {
        ArrayT<MxfFilePtr> mxff;
        files_->GetSelectedFiles(mxff);
        if ( mxff.Count() == 0 )
          DisplayError(this,Tr(89,"There are no selected files!"),Tr(90,"Could not Delete Files"));
        else if ( AreYouSure(this,Tr(91,"Are you sure you want to delete slected files?"),Tr(92,"Delete Files")) )
          {
            
            for ( int i = 0; i < mxff.Count(); ++i )
              mxff[i]->Remove();
            files_->ResetList();
          }
      }
      
    void OnDeleteFolder(wxCommandEvent &e)
      {
        MxfFolderPtr fold = g_curr_mxfFolder;
        if ( !fold )
          DisplayError(this,Tr(93,"There are no selected folder!"),Tr(94,"Could not Delete Folder"));
        else if ( AreYouSure(this,Tr(95,"Are you sure you want to delete slected folder and all its content?"),Tr(96,"Delete Folder")) )
          {
            fold->Remove();
            folders_->ResetTree();
          }
      }

    void OnFoldRename(wxCommandEvent &e)
      {
        MxfFolderPtr fold = g_curr_mxfFolder;
        StringW targfold;
        StringEfW ehf;
        StringW name = fold->GetName();
        if ( !fold )
          DisplayError(this,Tr(97,"There are no selected folder!"),Tr(98,"Could not Rename/Move Folder"));
        if ( fold == g_root_mxfFolder )
          DisplayError(this,Tr(99,"VFSROOT could not be renamed or moved!"),Tr(100,"Could not Rename/Move Folder"));
        else if ( RenameMove(this,targfold,name,true,true) )
          {
            //__asm int 3;
            MxfFolderPtr tg =  MxfEnsureSubfolder(targfold,ehf);
            if ( +tg != fold->GetParent() || name != fold->name_ )
              {
                fold->Remove();
                fold->name_ = name;
                tg->Push(fold);
                folders_->ResetTree();
              }
          }
      }
      
    void OnFileRename(wxCommandEvent &e)
      {
        StringEfW ehf;
        ArrayT<MxfFilePtr> mxff;
        files_->GetSelectedFiles(mxff);
        MxfFolderPtr fold = g_curr_mxfFolder;
        StringW targfold;
        StringW name;
        if ( mxff.Count() == 1 ) name = mxff[0]->GetName();
        if ( mxff.Count() == 0 )
          DisplayError(this,Tr(101,"There are no selected files!"),Tr(102,"Could not Rename/Move Files"));
        else if ( RenameMove(this,targfold,name,false,(mxff.Count() == 1)) )
          {
            MxfFolderPtr tg =  MxfEnsureSubfolder(targfold,ehf);
            if ( +tg != +fold || name != fold->name_ )
              {
                for ( int i = 0; i < mxff.Count(); ++i )
                  mxff[i]->Remove(); 
                if ( mxff.Count() == 1 ) 
                  mxff[0]->pkgname_ = name;
                for ( int i = 0; i < mxff.Count(); ++i )
                  tg->Push(mxff[i]); 
                files_->ResetList();
              }
          }
      }
      
    void OnHelp(wxCommandEvent &e)
      {
        ShowHelpSection(L"overview.htm");
      }
      
    void OnMainWindowHelp(wxCommandEvent &e)
      {
        ShowHelpSection(L"using.htm");
      }

    DECLARE_EVENT_TABLE()
  };

void Mainframe_Update_Tree_List(int expand_tree_pane)
  {
    MyMainFrame *fr = (MyMainFrame*)mainframe;
    
    if ( expand_tree_pane )
      fr->splitter_->SetSashPosition(263);
    fr->folders_->UpdateTree();
    fr->files_->ResetList();
  }

BEGIN_EVENT_TABLE(MyMainFrame, wxFrame)
    EVT_MENU(wxID_ABOUT, MyMainFrame::OnAbout)
    EVT_MENU(wxID_EXIT,  MyMainFrame::OnQuit)
    EVT_CLOSE(MyMainFrame::OnClose)
    EVT_MENU(wxID_OPEN,  MyMainFrame::OnOpen)
    EVT_BUTTON(wxID_OPEN,  MyMainFrame::OnOpen)
    EVT_MENU(wxID_SAVE,  MyMainFrame::OnSave)
    EVT_BUTTON(wxID_SAVE,  MyMainFrame::OnSave)
    EVT_MENU(wxID_SAVEAS,  MyMainFrame::OnSaveAs)
    EVT_MENU(myID_ACTIVATE,  MyMainFrame::OnActivate)
    EVT_MENU(wxID_HELP, MyMainFrame::OnHelp)
    EVT_MENU(ctID_HELP, MyMainFrame::OnMainWindowHelp)
    EVT_BUTTON(myID_ACTIVATE,  MyMainFrame::OnActivate)
    EVT_MENU(myID_PKGCONFIGURE, MyMainFrame::OnConfigure)
    EVT_BUTTON(myID_PKGCONFIGURE, MyMainFrame::OnConfigure)
    EVT_TREE_SEL_CHANGED(ctID_FOLDERSTREE, MyMainFrame::OnTreeSelChanged)
    EVT_MENU(myID_PKGDOPACK, MyMainFrame::OnPack)
    EVT_BUTTON(myID_PKGDOPACK, MyMainFrame::OnPack)
    EVT_MENU(myID_NEWSUBFOLDER, MyMainFrame::OnNewSubfolder)
    EVT_MENU(myID_ADDFILES, MyMainFrame::OnAddFiles)
    EVT_MENU(myID_ADDFOLDER, MyMainFrame::OnAddFolder)
    EVT_MENU(myID_DELFILES, MyMainFrame::OnDeleteFiles)
    EVT_MENU(myID_DELFOLDER, MyMainFrame::OnDeleteFolder)
    EVT_MENU(myID_FOLDRENAME, MyMainFrame::OnFoldRename)
    EVT_MENU(myID_FILERENAME, MyMainFrame::OnFileRename)
    EVT_MENU(myID_SAVEnEXIT, MyMainFrame::OnSaveAndExit)
    EVT_MENU(wxID_NEW, MyMainFrame::OnNew)
END_EVENT_TABLE()

struct MyWxApp: wxApp
  {
    virtual bool OnInit()
      {
        GetCurrentDirectory(sizeof(startdir)/sizeof(startdir[0])-1,startdir);
        GetModuleVersion(Hinstance->GetModulePath(),&_MAJOR_VERSION,&_MINOR_VERSION,&_BUILD_NUMBER);
        wxImage::AddHandler(new wxPNGHandler);

        g_curr_mxfFolder = g_root_mxfFolder = RccPtr(new MxfFolder(0,"VFSROOT"));
        ClearMxfOpts();
        
        //LoadLibraryA("itss.dll");
        MyMainFrame *frame = new MyMainFrame(0|_S*Tr(103,"The Molebox SVS %d.%d") %_MAJOR_VERSION %_BUILD_NUMBER);
        mainframe = frame;
        frame->Show(true);
        bool config_passed = false;
    
        CargsPtrW cargs = ParseCmdL((pwide_t)GetCommandLineW());
        if ( cargs && cargs->args.Count() )
          {
            frame->OpenScript(+cargs->args[0]);
            config_passed = true;
          }
              
        if ( !config_passed )
          {
            //ShowConfigDialog(frame);
          }
    
        return true;
      }
    virtual ~MyWxApp()
      {
      }
  };

DECLARE_APP(MyWxApp)
IMPLEMENT_APP(MyWxApp)
