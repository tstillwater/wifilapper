#pragma once

#include "ArtUI.h"
#include "ArtTools.h"
#include "LapReceiver.h"

struct RACESELECT_RESULT
{
public:
  RACESELECT_RESULT()
  {
    iRaceId = -1;
    fCancelled = false;
  }
  int iRaceId;
  bool fCancelled;
};

class CRaceSelectDlg : public IUI
{
public:
  CRaceSelectDlg(ILapReceiver* pLapDB, RACESELECT_RESULT* pResults) : m_pResults(pResults) 
  {
    m_pLapDB = pLapDB;
  };

  virtual void NotifyChange(WPARAM wParam, LPARAM lParam) {DASSERT(FALSE);};
  virtual LRESULT DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  virtual DWORD GetDlgId() const {return IDD_SELECTRACE;}
private:
  RACESELECT_RESULT* m_pResults;
  ILapReceiver* m_pLapDB;
  ArtListBox sfListBox;
};