
/*

  Copyright (C) 2009, Alexey Sudachen, https://goo.gl/lJNXya.

*/

#include "myafx.h"
#include "wx/aboutdlg.h"
#include "wx/sizer.h"
#include "wx/html/htmlwin.h"

time_t _BUILD_TIME = POSIXBUILDTIME;

struct AboutDialog : wxDialog
  {
    AboutDialog(wxWindow *parent) : wxDialog(parent,wxID_ANY,L"About",wxDefaultPosition,wxDefaultSize,wxCAPTION)
      {
      }
      
    void CreateControls()
      {
        wxBoxSizer *topszr = new wxBoxSizer(wxVERTICAL);
        wxPanel *pan = new wxPanel(this);
        wxHtmlWindow *html = new wxHtmlWindow(pan,wxID_ANY,wxDefaultPosition,wxSize(520,480),wxHW_SCROLLBAR_NEVER|wxSUNKEN_BORDER);
        html->LoadPage(+AtAppFolder("data/about.htm"));  
        StringW limits;
        DataStreamPtr ds;
                
        ds = DataSource->Open(AtAppFolder("data/about.t"));
        if (!ds) ds = DataSource->Open(AtAppFolder("molebox.SRC/data/about.t"));
        if ( ds )
          {     
            BufferT<wchar_t> b; 
            ds->UtfReadTextEx(b);
            b.Push(0);
            static StringW buildtime_tmpl = "%BUILDTIME%";
            auto dt = DateTime::FromPOSIXtime(_BUILD_TIME);
            static StringW buildtime_val  = _S*L"%d %s %d" %dt.Day() %dt.Smon() %dt.Year();
            b.Replace(+buildtime_tmpl,buildtime_tmpl.Length(),+buildtime_val,buildtime_val.Length()/*,towlower*/);
            html->AppendToPage(+b);
          }
        html->SetSize( html->GetInternalRepresentation()->GetWidth(),
                       html->GetInternalRepresentation()->GetHeight());
        html->SetBorders(0);
        pan->SetSize(html->GetSize());
        topszr->Add(pan, 0, wxALL, 10);
        wxSizer *buttons = CreateButtonSizer(wxOK);
        topszr->Add( buttons, 0, wxALIGN_CENTER_HORIZONTAL|wxBOTTOM, 10 );
        SetSizer(topszr);
        topszr->Fit(this);
        wxSize sz = GetSize();
        wxSize psz = GetParent()->GetSize();
        wxPoint xy = GetParent()->GetPosition();
        SetPosition(wxPoint(xy.x+(psz.x-sz.x)/2,xy.y+(psz.y-sz.y)/2));
        //SetClientSize(topszr->ComputeFittingClientSize(this));
      }

    DECLARE_EVENT_TABLE()
  };

BEGIN_EVENT_TABLE( AboutDialog, wxDialog )
END_EVENT_TABLE()

void ShowAboutInfo(wxWindow *parent)
  {
    AboutDialog dialog(parent);
    dialog.CreateControls();
    dialog.ShowModal();
  }
