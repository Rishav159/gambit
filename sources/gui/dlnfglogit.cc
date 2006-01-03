//
// $Source$
// $Date$
// $Revision$
//
// DESCRIPTION:
// Dialog for monitoring progress of logit equilibrium computation
//
// This file is part of Gambit
// Copyright (c) 2005, The Gambit Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//

#include <fstream>

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif  // WX_PRECOMP
#include <wx/txtstrm.h>
#include <wx/tokenzr.h>

#include "wx/sheet/sheet.h"
#include "wx/plotctrl/plotctrl.h"
#include "dlnfglogit.h"

//========================================================================
//                      class gbtLogitMixedList
//========================================================================

class gbtLogitMixedList : public wxSheet {
private:
  gbtGameDocument *m_doc;
  gbtList<double> m_lambdas;
  gbtList<gbtMixedProfile<double> > m_profiles;

  // Overriding wxSheet members for data access
  wxString GetCellValue(const wxSheetCoords &);
  wxSheetCellAttr GetAttr(const wxSheetCoords &p_coords, wxSheetAttr_Type) const;
  
  // Overriding wxSheet members to disable selection behavior
  bool SelectRow(int, bool = false, bool = false)
    { return false; }
  bool SelectRows(int, int, bool = false, bool = false)
    { return false; }
  bool SelectCol(int, bool = false, bool = false)
    { return false; }
  bool SelectCols(int, int, bool = false, bool = false)
    { return false; }
  bool SelectCell(const wxSheetCoords&, bool = false, bool = false)
    { return false; }
  bool SelectBlock(const wxSheetBlock&, bool = false, bool = false)
    { return false; }
  bool SelectAll(bool = false) { return false; }

  // Overriding wxSheet member to suppress drawing of cursor
  void DrawCursorCellHighlight(wxDC&, const wxSheetCellAttr &) { }

public:
  gbtLogitMixedList(wxWindow *p_parent, gbtGameDocument *p_doc);
  virtual ~gbtLogitMixedList();

  void AddProfile(const wxString &p_text, bool p_forceShow);

  const gbtList<double> &GetLambdas(void) const { return m_lambdas; }
  const gbtList<gbtMixedProfile<double> > &GetProfiles(void) const 
  { return m_profiles; }
};

gbtLogitMixedList::gbtLogitMixedList(wxWindow *p_parent, 
				     gbtGameDocument *p_doc) 
  : wxSheet(p_parent, -1), m_doc(p_doc)
{
  CreateGrid(0, 0);
  SetRowLabelWidth(40);
  SetColLabelHeight(25);
}

gbtLogitMixedList::~gbtLogitMixedList()
{ }

wxString gbtLogitMixedList::GetCellValue(const wxSheetCoords &p_coords)
{
  if (!m_doc->GetGame())  return wxT("");

  if (IsRowLabelCell(p_coords)) {
    return wxString::Format(wxT("%d"), p_coords.GetRow() + 1);
  }
  else if (IsColLabelCell(p_coords)) {
    if (p_coords.GetCol() == 0) {
      return wxT("Lambda");
    }
    else {
      int index = 1;
      for (int pl = 1; pl <= m_doc->GetGame()->NumPlayers(); pl++) {
	Gambit::GamePlayer player = m_doc->GetGame()->GetPlayer(pl);
	for (int st = 1; st <= player->NumStrategies(); st++) {
	  if (index++ == p_coords.GetCol()) {
	    return (wxString::Format(wxT("%d: "), pl) +
		    wxString(player->GetStrategy(st)->GetName().c_str(),
			    *wxConvCurrent));
	  }
	}
      }
      return wxT("");
    }
  }
  else if (IsCornerLabelCell(p_coords)) {
    return wxT("#");
  }

  if (p_coords.GetCol() == 0) {
    return wxString(ToText(m_lambdas[p_coords.GetRow()+1],
			   m_doc->GetStyle().NumDecimals()).c_str(),
		    *wxConvCurrent);
  }
  else {
    const gbtMixedProfile<double> &profile = m_profiles[p_coords.GetRow()+1];
    return wxString(ToText(profile[p_coords.GetCol()],
			   m_doc->GetStyle().NumDecimals()).c_str(), 
		    *wxConvCurrent);
  }
}


static wxColour GetPlayerColor(gbtGameDocument *p_doc, int p_index)
{
  if (!p_doc->GetGame())  return *wxBLACK;

  int index = 1;
  for (int pl = 1; pl <= p_doc->GetGame()->NumPlayers(); pl++) {
    Gambit::GamePlayer player = p_doc->GetGame()->GetPlayer(pl);
    for (int st = 1; st <= player->NumStrategies(); st++) {
      if (index++ == p_index) {
	return p_doc->GetStyle().GetPlayerColor(pl);
      }
    }
  }
  return *wxBLACK;
}

wxSheetCellAttr gbtLogitMixedList::GetAttr(const wxSheetCoords &p_coords, 
					     wxSheetAttr_Type) const
{
  if (IsRowLabelCell(p_coords)) {
    wxSheetCellAttr attr(GetSheetRefData()->m_defaultRowLabelAttr);
    attr.SetFont(wxFont(10, wxSWISS, wxNORMAL, wxBOLD));
    attr.SetAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
    attr.SetOrientation(wxHORIZONTAL);
    attr.SetReadOnly(true);
    return attr;
  }
  else if (IsColLabelCell(p_coords)) {
    wxSheetCellAttr attr(GetSheetRefData()->m_defaultColLabelAttr);
    attr.SetFont(wxFont(10, wxSWISS, wxNORMAL, wxBOLD));
    attr.SetAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
    attr.SetOrientation(wxHORIZONTAL);
    attr.SetReadOnly(true);
    attr.SetForegroundColour(GetPlayerColor(m_doc, p_coords.GetCol()));
    return attr;
  }
  else if (IsCornerLabelCell(p_coords)) {
    return GetSheetRefData()->m_defaultCornerLabelAttr;
  }

  wxSheetCellAttr attr(GetSheetRefData()->m_defaultGridCellAttr);
  attr.SetFont(wxFont(10, wxSWISS, wxNORMAL, wxNORMAL));
  attr.SetAlignment(wxALIGN_RIGHT, wxALIGN_CENTER);
  attr.SetOrientation(wxHORIZONTAL);
  attr.SetForegroundColour(GetPlayerColor(m_doc, p_coords.GetCol()));
  attr.SetReadOnly(true);
  return attr;
}


void gbtLogitMixedList::AddProfile(const wxString &p_text,
				   bool p_forceShow)
{
  if (GetNumberCols() == 0) {
    AppendCols(m_doc->GetGame()->MixedProfileLength() + 1);
  }

  gbtMixedProfile<double> profile(m_doc->GetGame());

  wxStringTokenizer tok(p_text, wxT(","));

  m_lambdas.Append((double) ToNumber(std::string((const char *) tok.GetNextToken().mb_str())));

  for (int i = 1; i <= profile.Length(); i++) {
    profile[i] = ToNumber(std::string((const char *) tok.GetNextToken().mb_str()));
  }

  m_profiles.Append(profile);
  if (p_forceShow || m_profiles.Length() - GetNumberRows() > 20) {
    AppendRows(m_profiles.Length() - GetNumberRows());
    MakeCellVisible(wxSheetCoords(GetNumberRows() - 1, 0));
  }

  // Lambda tends to get large, so this column usually needs resized
  AutoSizeCol(0);
}


//========================================================================
//                      class gbtLogitPlotCtrl
//========================================================================

class gbtLogitPlotCtrl : public wxPlotCtrl {
private:
  gbtGameDocument *m_doc;

  /// Overriding x (lambda) axis labeling
  void CalcXAxisTickPositions(void);
  double LambdaToX(double p_lambda)
  { return p_lambda / (1.0 + p_lambda); }
  double XToLambda(double p_x)
  { return p_x / (1.0 - p_x); }
  
public:
  gbtLogitPlotCtrl(wxWindow *p_parent, gbtGameDocument *p_doc);

  void SetProfiles(const gbtList<double> &,
		   const gbtList<gbtMixedProfile<double> > &);
};

gbtLogitPlotCtrl::gbtLogitPlotCtrl(wxWindow *p_parent, 
				   gbtGameDocument *p_doc)
  : wxPlotCtrl(p_parent), m_doc(p_doc)
{
  SetAxisLabelColour(*wxBLUE);
  wxFont labelFont(8, wxSWISS, wxNORMAL, wxBOLD);
  SetAxisLabelFont(labelFont);
  SetAxisColour(*wxBLUE);
  SetAxisFont(labelFont);

  // SetAxisFont resets the width of the y axis labels, assuming
  // a fairly long label.
  int x=6, y=12, descent=0, leading=0;
  GetTextExtent(wxT("0.88"), &x, &y, &descent, &leading, &labelFont);
  m_y_axis_text_width = x + leading;

  SetXAxisLabel(wxT("Lambda"));
  SetShowXAxisLabel(true);
  SetYAxisLabel(wxT("Probability"));
  SetShowYAxisLabel(true);

  SetShowKey(true);

  m_xAxisTick_step = 0.2;
  SetViewRect(wxRect2DDouble(0, 0, 1, 1));
} 

//
// This differs from the wxPlotWindow original only by the use of
// XToLambda() to construct the tick labels.
//
void gbtLogitPlotCtrl::CalcXAxisTickPositions(void)
{
  double current = ceil(m_viewRect.GetLeft() / m_xAxisTick_step) * m_xAxisTick_step;
  m_xAxisTicks.Clear();
  m_xAxisTickLabels.Clear();
  int i, x, windowWidth = GetPlotAreaRect().width;
  for (i=0; i<m_xAxisTick_count; i++) {
    if (!IsFinite(current, wxT("axis label is not finite"))) return;
                
    x = GetClientCoordFromPlotX( current );
            
    if ((x >= -1) && (x < windowWidth+2)) {
      m_xAxisTicks.Add(x);
      m_xAxisTickLabels.Add(wxString::Format(m_xAxisTickFormat.c_str(), 
					     XToLambda(current)));
    }

    current += m_xAxisTick_step;
  }
}

void gbtLogitPlotCtrl::SetProfiles(const gbtList<double> &p_lambdas,
				   const gbtList<gbtMixedProfile<double> > &p_profiles)
{
  if (p_lambdas.Length() == 0)  return;

  wxBitmap bitmap(1, 1);
  
  for (int pl = 1; pl <= m_doc->GetGame()->NumPlayers(); pl++) {
    Gambit::GamePlayer player = m_doc->GetGame()->GetPlayer(pl);
    for (int st = 1; st <= player->NumStrategies(); st++) {
      wxPlotData *curve = new wxPlotData(p_lambdas.Length());
      curve->SetFilename(wxString::Format(wxT("%d:%d"), pl, st));
    
      for (int i = 0; i < p_lambdas.Length(); i++) {
	curve->SetValue(i, LambdaToX(p_lambdas[i+1]), 
			p_profiles[i+1](pl, st));
      }

      curve->SetPen(wxPLOTPEN_NORMAL, 
		    wxPen(m_doc->GetStyle().GetPlayerColor(pl), 1, wxSOLID));
      curve->SetSymbol(bitmap);

      AddCurve(curve);
    }
  }
}

//========================================================================
//                      class gbtLogitMixedDialog
//========================================================================

const int GBT_ID_TIMER = 1000;
const int GBT_ID_PROCESS = 1001;

BEGIN_EVENT_TABLE(gbtLogitMixedDialog, wxDialog)
  EVT_END_PROCESS(GBT_ID_PROCESS, gbtLogitMixedDialog::OnEndProcess)
  EVT_IDLE(gbtLogitMixedDialog::OnIdle)
  EVT_TIMER(GBT_ID_TIMER, gbtLogitMixedDialog::OnTimer)
  EVT_BUTTON(wxID_SAVE, gbtLogitMixedDialog::OnSave)
END_EVENT_TABLE()

#include "bitmaps/stop.xpm"

gbtLogitMixedDialog::gbtLogitMixedDialog(wxWindow *p_parent, 
					 gbtGameDocument *p_doc)
  : wxDialog(p_parent, -1, wxT("Compute quantal response equilibria"),
	     wxDefaultPosition),
    m_doc(p_doc), m_process(0), m_timer(this, GBT_ID_TIMER)
{
  wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

  wxBoxSizer *startSizer = new wxBoxSizer(wxHORIZONTAL);

  m_statusText = new wxStaticText(this, wxID_STATIC,
				  wxT("The computation is currently in progress."));
  m_statusText->SetForegroundColour(*wxBLUE);
  startSizer->Add(m_statusText, 0, wxALL | wxALIGN_CENTER, 5);

  m_stopButton = new wxBitmapButton(this, wxID_CANCEL, wxBitmap(stop_xpm));
  m_stopButton->SetToolTip(_("Stop the computation"));
  startSizer->Add(m_stopButton, 0, wxALL | wxALIGN_CENTER, 5);
  Connect(wxID_CANCEL, wxEVT_COMMAND_BUTTON_CLICKED,
	  wxCommandEventHandler(gbtLogitMixedDialog::OnStop));

  sizer->Add(startSizer, 0, wxALL | wxALIGN_CENTER, 5);

  wxBoxSizer *midSizer = new wxBoxSizer(wxHORIZONTAL);
  m_mixedList = new gbtLogitMixedList(this, m_doc);
  m_mixedList->SetSizeHints(wxSize(300, 400));
  midSizer->Add(m_mixedList, 0, wxALL | wxALIGN_CENTER, 5);
  
  m_plot = new gbtLogitPlotCtrl(this, m_doc);
  m_plot->SetSizeHints(wxSize(400, 400));
  midSizer->Add(m_plot, 0, wxALL | wxALIGN_CENTER, 5);

  sizer->Add(midSizer, 0, wxALL | wxALIGN_CENTER, 5);

  wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
  m_saveButton = new wxButton(this, wxID_SAVE, 
			      wxT("Save correspondence to .csv file"));
  m_saveButton->Enable(false);
  buttonSizer->Add(m_saveButton, 0, wxALL | wxALIGN_CENTER, 5);
  m_okButton = new wxButton(this, wxID_OK, wxT("OK"));
  buttonSizer->Add(m_okButton, 0, wxALL | wxALIGN_CENTER, 5);
  m_okButton->Enable(false);

  sizer->Add(buttonSizer, 0, wxALL | wxALIGN_RIGHT, 5);

  SetSizer(sizer);
  sizer->Fit(this);
  sizer->SetSizeHints(this);
  Layout();
  CenterOnParent();
  Start();
}

void gbtLogitMixedDialog::Start(void)
{
  m_doc->BuildNfg();

  m_process = new wxProcess(this, GBT_ID_PROCESS);
  m_process->Redirect();

  m_pid = wxExecute(wxT("gambit-nfg-logit"), wxEXEC_ASYNC, m_process);
  
  std::ostringstream s;
  m_doc->GetGame()->WriteNfgFile(s);
  wxString str(wxString(s.str().c_str(), *wxConvCurrent));
  
  // It is possible that the whole string won't write on one go, so
  // we should take this possibility into account.  If the write doesn't
  // complete the whole way, we take a 100-millisecond siesta and try
  // again.  (This seems to primarily be an issue with -- you guessed it --
  // Windows!)
  while (str.length() > 0) {
    wxTextOutputStream os(*m_process->GetOutputStream());

    // It appears that (at least with mingw) the string itself contains
    // only '\n' for newlines.  If we don't SetMode here, these get
    // converted to '\r\n' sequences, and so the number of characters
    // LastWrite() returns does not match the number of characters in
    // our string.  Setting this explicitly solves this problem.
    os.SetMode(wxEOL_UNIX);
    os.WriteString(str);
    str.Remove(0, m_process->GetOutputStream()->LastWrite());
    wxMilliSleep(100);
  }
  m_process->CloseOutput();

  m_timer.Start(1000, false);
}

void gbtLogitMixedDialog::OnIdle(wxIdleEvent &p_event)
{
  if (!m_process)  return;

  if (m_process->IsInputAvailable()) {
    wxTextInputStream tis(*m_process->GetInputStream());

    wxString msg;
    msg << tis.ReadLine();
    m_mixedList->AddProfile(msg, false);
    m_output += msg;
    m_output += wxT("\n");

    p_event.RequestMore();
  }
  else {
    m_timer.Start(1000, false);
  }
}

void gbtLogitMixedDialog::OnTimer(wxTimerEvent &p_event)
{
  wxWakeUpIdle();
}

void gbtLogitMixedDialog::OnEndProcess(wxProcessEvent &p_event)
{
  m_stopButton->Enable(false);
  m_timer.Stop();

  while (m_process->IsInputAvailable()) {
    wxTextInputStream tis(*m_process->GetInputStream());

    wxString msg;
    msg << tis.ReadLine();

    if (msg != wxT("")) {
      m_mixedList->AddProfile(msg, true);
      m_output += msg;
      m_output += wxT("\n");
    }
  }

  if (p_event.GetExitCode() == 0) {
    m_statusText->SetLabel(wxT("The computation has completed."));
    m_statusText->SetForegroundColour(wxColour(0, 192, 0));
  }
  else {
    m_statusText->SetLabel(wxT("The computation ended abnormally."));
    m_statusText->SetForegroundColour(*wxRED);
  }

  m_okButton->Enable(true);
  m_saveButton->Enable(true);
  m_plot->SetProfiles(m_mixedList->GetLambdas(), m_mixedList->GetProfiles());
}

void gbtLogitMixedDialog::OnStop(wxCommandEvent &)
{
  // Per the wxWidgets wiki, under Windows, programs that run
  // without a console window don't respond to the more polite
  // SIGTERM, so instead we must be rude and SIGKILL it.
  m_stopButton->Enable(false);

#ifdef __WXMSW__
  wxProcess::Kill(m_pid, wxSIGKILL);
#else
  wxProcess::Kill(m_pid, wxSIGTERM);
#endif  // __WXMSW__
}

void gbtLogitMixedDialog::OnSave(wxCommandEvent &)
{
  wxFileDialog dialog(this, _("Choose file"), wxT(""), wxT(""),
		      wxT("CSV files (*.csv)|*.csv|"
			  "All files (*.*)|*.*"),
		      wxSAVE | wxOVERWRITE_PROMPT);

  if (dialog.ShowModal() == wxID_OK) {
    std::ofstream file((const char *) dialog.GetPath().mb_str());
    file << ((const char *) m_output.mb_str());
  }
}
