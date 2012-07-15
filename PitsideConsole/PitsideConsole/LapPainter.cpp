#include "stdafx.h"
#include "LapPainter.h"
#include "LapData.h"
#include "ArtUI.h"

struct HIGHLIGHTDATA
{
public:
  const CDataChannel* m_pDataX;
  const CDataChannel* m_pDataY;
  POINT m_ptWindow;
  const CExtendedLap* m_pLap;
  DATA_CHANNEL m_eChannelY;

};

CLapPainter::CLapPainter(/*IUI* pUI, */ILapSupplier* pLapSupplier, int iSupplierId) : ArtOpenGLWindow(), /*m_pUI(pUI),*/ m_pLapSupplier(pLapSupplier), m_iSupplierId(iSupplierId)
{
}
CLapPainter::~CLapPainter()
{
}


void CLapPainter::OGL_Paint()
{
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );
  
  RECT rcClient;
  GetClientRect(OGL_GetHWnd(), &rcClient);
  
  LAPDISPLAYSTYLE eDisplayStyle = m_pLapSupplier->GetLapDisplayStyle(m_iSupplierId);
  const LAPSUPPLIEROPTIONS& sfLapOpts = m_pLapSupplier->GetDisplayOptions();
  switch(eDisplayStyle)
  {
  case LAPDISPLAYSTYLE_MAP:
    glViewport(0,0,RECT_WIDTH(&rcClient), RECT_HEIGHT(&rcClient));
    DrawLapLines(sfLapOpts);
    break;
  case LAPDISPLAYSTYLE_PLOT:
    DrawGeneralGraph(sfLapOpts, true);
    break;
  case LAPDISPLAYSTYLE_NOLAPS:
    // user doesn't have any laps selected, so we should tell them to select some
    DrawSelectLapsPrompt();
    break;
  }
  
	SwapBuffers( OGL_GetDC() );
}
  
void CLapPainter::DrawSelectLapsPrompt() const
{
  RECT rcClient;
  GetClientRect(OGL_GetHWnd(),&rcClient);

  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, RECT_WIDTH(&rcClient),0, RECT_HEIGHT(&rcClient),-1.0,1.0);

  DrawText(20.0, 20, "No laps selected.  Select some laps in the lap list");

  glPopMatrix();
}

void UpdateHighlightPointList(vector<HIGHLIGHTDATA>& lst, const CExtendedLap* pLap, GLdouble* rgModelviewMatrix, GLdouble* rgProjMatrix, GLint* rgViewport, float dTimeToHighlight, const CDataChannel* pDataX, const CDataChannel* pDataY)
{
  const double dPointX = pDataX->GetValue(dTimeToHighlight);
  const double dPointY = pDataY->GetValue(dTimeToHighlight);

  GLdouble winx,winy,winz;
  gluProject(dPointX, dPointY, 0, rgModelviewMatrix, rgProjMatrix, rgViewport, &winx, &winy, &winz);
  POINT pt;
  pt.x = (int)winx;
  pt.y = (int)winy;

  HIGHLIGHTDATA aHighlight;
  aHighlight.m_ptWindow = pt;
  aHighlight.m_pDataX = pDataX;
  aHighlight.m_pDataY = pDataY;
  aHighlight.m_pLap = pLap;
  aHighlight.m_eChannelY = pDataY->GetChannelType();
  lst.push_back(aHighlight);
}

void CLapPainter::DrawGeneralGraph(const LAPSUPPLIEROPTIONS& sfLapOpts, bool fHighlightXAxis)
{
  vector<CExtendedLap*> lstLaps = m_pLapSupplier->GetLapsToShow();

  DATA_CHANNEL eX;
  eX = DATA_CHANNEL_DISTANCE;
  set<DATA_CHANNEL> setY;
  map<DATA_CHANNEL,float> mapMinY;
  map<DATA_CHANNEL,float> mapMaxY;
  float dMaxX = -1e30;
  float dMinX = 1e30;

  { // figuring out bounds and getting matrices all set up
    
    for(int x = 0;x < lstLaps.size(); x++)
    {
      const CDataChannel* pDataX = m_pLapSupplier->GetXChannel(lstLaps[x]->GetLap()->GetLapId());
      vector<const CDataChannel*> lstDataY = m_pLapSupplier->GetYChannels(lstLaps[x]->GetLap()->GetLapId());
      for(int y = 0; y < lstDataY.size(); y++)
      {
        const CDataChannel* pChannel = lstDataY[y];
        const DATA_CHANNEL eType = pChannel->GetChannelType();
        if(mapMinY.find(eType) == mapMinY.end())
        {
          mapMinY[eType] = pChannel->GetMin();
          mapMaxY[eType] = pChannel->GetMax();
        }
        else
        {
          mapMinY[eType] = min(pChannel->GetMin(),mapMinY[eType]);
          mapMaxY[eType] = max(pChannel->GetMax(),mapMaxY[eType]);
        }
        setY.insert(eType);
      }
      if(pDataX)
      {
        dMaxX = max(dMaxX, pDataX->GetMax());
        dMinX = min(dMinX, pDataX->GetMin());

        eX = pDataX->GetChannelType();
      }
    }
  }
  
  if(setY.size() <= 0)
  {
    DrawSelectLapsPrompt();
    return;
  }
  
  RECT rcSpot;
  int iSegmentHeight=0;
  {
    RECT rcClient;
    GetClientRect(OGL_GetHWnd(), &rcClient);
  
    iSegmentHeight = RECT_HEIGHT(&rcClient) / setY.size();
    rcSpot.left = 0;
    rcSpot.top = 0;
    rcSpot.right = RECT_WIDTH(&rcClient);
    rcSpot.bottom = iSegmentHeight;
  }
  int iPos = 0;
  for(set<DATA_CHANNEL>::iterator i = setY.begin(); i != setY.end(); i++)
  {
    vector<HIGHLIGHTDATA> lstMousePointsToDraw;
    glViewport(rcSpot.left,rcSpot.top,RECT_WIDTH(&rcSpot),RECT_HEIGHT(&rcSpot));

    // now we have the bounds of all the laps we've looked at, so let's draw them
    glPushMatrix();
    glLoadIdentity();
    glScalef(0.95f, 0.70f, 0.95f);
    glOrtho(dMinX, dMaxX,mapMinY[*i], mapMaxY[*i],-1.0,1.0);

    // draw line guides on the background
    for(float flLine = m_pLapSupplier->GetGuideStart(*i, mapMinY[*i], mapMaxY[*i]) + m_pLapSupplier->GetGuideStep(*i, mapMinY[*i], mapMaxY[*i]); flLine < mapMaxY[*i]; flLine += m_pLapSupplier->GetGuideStep(*i, mapMinY[*i], mapMaxY[*i]))
    {
      glColor3d(1.0,1.0,1.0);
      glBegin(GL_LINE_STRIP);
      glVertex2f(dMinX,flLine);
      glVertex2f(dMaxX,flLine);
      glEnd();
      
      glColor3d(0.7,0.7,0.7);
      char szText[256];
      GetChannelString(*i, sfLapOpts.eUnitPreference, flLine, szText, NUMCHARS(szText));
      DrawText(dMinX, flLine, szText);
    }

    GLdouble rgModelviewMatrix[16];
    GLdouble rgProjMatrix[16];
    GLint rgViewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, rgModelviewMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, rgProjMatrix);
    glGetIntegerv(GL_VIEWPORT, rgViewport);

    Vector2D ptHighlight; // the (x,y) coords in unit-space that we want to highlight.  Example: for a speed-distance graph, x would be in distance units, y in velocities.
    POINT ptMouse;
    if(GetMouse(&ptMouse) && m_pLapSupplier->IsHighlightSource(m_iSupplierId))
    {
      // the mouse is in our window... we make our own highlighter, ignoring anything that got sent to us
      GLdouble dX,dY,dZ;
      gluUnProject(ptMouse.x,ptMouse.y,0,rgModelviewMatrix,rgProjMatrix,rgViewport,&dX,&dY,&dZ);
      ptHighlight = V2D(dX,0);
    }

    for(int x = 0; x < lstLaps.size(); x++)
    {
      const CExtendedLap* pLap = lstLaps[x];
      const CDataChannel* pDataX = m_pLapSupplier->GetXChannel(pLap->GetLap()->GetLapId());
      const CDataChannel* pDataY = m_pLapSupplier->GetChannel(pLap->GetLap()->GetLapId(), *i);

      if(pDataX && pDataY)
      {
        // tracking what we want to highlight
        float dBestLength = -1;
        float dTimeToHighlight = -1;
        const vector<DataPoint>& lstPointsX = pDataX->GetData();
        const vector<DataPoint>& lstPointsY = pDataY->GetData();


        srand((int)pLap);
        const float r = RandDouble()/2 + 0.5;
        const float g = RandDouble()/2 + 0.5;
        const float b = RandDouble()/2 + 0.5;
        glColor3d( r, g, b ); 

        if(sfLapOpts.fDrawLines)
        {
          glBegin(GL_LINE_STRIP);
        }
        else
        {
          glPointSize(5.0f);
          glBegin(GL_POINTS);
        }

        vector<DataPoint>::const_iterator iX = lstPointsX.begin();
        vector<DataPoint>::const_iterator iY = lstPointsY.begin();
        while(iX != lstPointsX.end() && iY != lstPointsY.end())
        {
          float dX;
          float dY;
          const DataPoint& ptX = *iX;
          const DataPoint& ptY = *iY;
          int iTimeUsed = 0;
          if(ptX.iTimeMs < ptY.iTimeMs)
          {
            iTimeUsed = ptX.iTimeMs;
            dX = ptX.flValue;
            dY = pDataY->GetValue(ptX.iTimeMs, iY);
            iX++;
          }
          else if(ptX.iTimeMs > ptY.iTimeMs)
          {
            iTimeUsed = ptY.iTimeMs;
            dX = pDataX->GetValue(ptY.iTimeMs, iX);
            dY = ptY.flValue;
            iY++;
          }
          else
          {
            iTimeUsed = ptY.iTimeMs;
            DASSERT(ptX.iTimeMs == ptY.iTimeMs);
            dX = ptX.flValue;
            dY = ptY.flValue;
            iX++;
            iY++;
          }
          glVertex2f(dX,dY);

          // if we're a highlight source, try to figure out the closest point for this lap
          if(m_pLapSupplier->IsHighlightSource(m_iSupplierId))
          {
            Vector2D vPt = V2D(dX,0);
            Vector2D vDiff = vPt - ptHighlight;
            if(vDiff.Length() < dBestLength || dBestLength < 0)
            {
              dBestLength = vDiff.Length();
              dTimeToHighlight = iTimeUsed;
            }
          }
        }
		    glEnd();
        // for each lap, draw an indicator of the closest thing to the mouse
        if(!m_pLapSupplier->IsHighlightSource(m_iSupplierId))
        {
          // if we're not a source, use the given time to highlight
          dTimeToHighlight = m_pLapSupplier->GetLapHighlightTime(pLap);
        }
        else
        {
          m_pLapSupplier->SetLapHighlightTime(pLap, (int)dTimeToHighlight);
        }
        UpdateHighlightPointList(lstMousePointsToDraw, pLap, rgModelviewMatrix, rgProjMatrix, rgViewport, dTimeToHighlight, pDataX, pDataY);
      }
    } // end lap loop

    if(lstMousePointsToDraw.size() > 0)
    {
      glPushMatrix();
      glLoadIdentity();
      glOrtho(0, RECT_WIDTH(&rcSpot),0, RECT_HEIGHT(&rcSpot),-1.0,1.0);


      for(int x = 0; x < lstMousePointsToDraw.size(); x++)
      {
        const CExtendedLap* pLap = lstMousePointsToDraw[x].m_pLap;
        const POINT& ptWindow = lstMousePointsToDraw[x].m_ptWindow;
        const CDataChannel* pDataX = lstMousePointsToDraw[x].m_pDataX;
        const CDataChannel* pDataY = lstMousePointsToDraw[x].m_pDataY;

        srand((int)pLap);
        const float r = RandDouble()/2 + 0.5;
        const float g = RandDouble()/2 + 0.5;
        const float b = RandDouble()/2 + 0.5;
        glColor3d( r, g, b ); 
      
        // if we're the main screen, we want to draw some text data for each point
        TCHAR szLapName[256];
        pLap->GetString(szLapName, NUMCHARS(szLapName));

        float dTimeToHighlight = m_pLapSupplier->GetLapHighlightTime(pLap);

        char szYString[256];
        GetChannelString(lstMousePointsToDraw[x].m_eChannelY, sfLapOpts.eUnitPreference, pDataY->GetValue(dTimeToHighlight), szYString, NUMCHARS(szYString));

        char szXString[256];
        GetChannelString(eX, sfLapOpts.eUnitPreference, pDataX->GetValue(dTimeToHighlight), szXString, NUMCHARS(szXString));

        char szText[256];
        sprintf(szText, "%S - %s @ %s", szLapName, szYString, szXString);

        DrawText(0.0,(x+1)*GetWindowFontSize(),szText);

        // we also want to draw a highlighted square
        DrawGLFilledSquare(ptWindow.x, ptWindow.y, 5);
      }
      glPopMatrix();
      glPopMatrix();
    }
    rcSpot.top += iSegmentHeight;
    rcSpot.bottom += iSegmentHeight;
  } // end y-channel data channel loop

	
}
struct MAPHIGHLIGHT
{
  const CExtendedLap* pLap;
  POINT pt;
};

void CLapPainter::DrawLapLines(const LAPSUPPLIEROPTIONS& sfLapOpts)
{
  FLOATRECT rcAllLaps = m_pLapSupplier->GetAllLapsBounds();

  RECT rcClient;
  GetClientRect(OGL_GetHWnd(), &rcClient);
  double dClientAspect = ((double)RECT_WIDTH(&rcClient)) / ((double)RECT_HEIGHT(&rcClient));
  double dMapAspect = abs(RECT_WIDTH(&rcAllLaps)) / abs(RECT_HEIGHT(&rcAllLaps));


  if(dClientAspect > dMapAspect)
  {
    // the window is wider than the map is.  we'll want to widen the map space we show appropriately
    const double dCentreX = (rcAllLaps.left + rcAllLaps.right)/2;
    rcAllLaps.left -= dCentreX;
    rcAllLaps.right -= dCentreX;
    rcAllLaps.left *= (dClientAspect / dMapAspect);
    rcAllLaps.right *= (dClientAspect / dMapAspect);
    rcAllLaps.left += dCentreX;
    rcAllLaps.right += dCentreX;
  }
  else
  {
    // the window is taller than the map points are.  we'll want to tallen map bounds
    const double dCentreY = (rcAllLaps.top + rcAllLaps.bottom)/2;
    rcAllLaps.top -= dCentreY;
    rcAllLaps.bottom -= dCentreY;
    rcAllLaps.top *= (dMapAspect / dClientAspect);
    rcAllLaps.bottom *= (dMapAspect / dClientAspect);
    rcAllLaps.top += dCentreY;
    rcAllLaps.bottom += dCentreY;

  }
  // we have now determined the bounds of the thing we're going to draw
	glPushMatrix();
  glLoadIdentity();
  glOrtho(rcAllLaps.right, rcAllLaps.left,rcAllLaps.top, rcAllLaps.bottom,-1.0,1.0);
  //glOrtho(rcAllLaps.left, rcAllLaps.right,rcAllLaps.bottom,rcAllLaps.top, -1.0,1.0);
  
  GLdouble rgModelviewMatrix[16];
  GLdouble rgProjMatrix[16];
  GLint rgViewport[4];

  {
    glGetDoublev(GL_MODELVIEW_MATRIX, rgModelviewMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, rgProjMatrix);
    glGetIntegerv(GL_VIEWPORT, rgViewport);
  
    const double dCenterX = (rcAllLaps.left + rcAllLaps.right)/2;
    const double dCenterY = (rcAllLaps.top + rcAllLaps.bottom)/2;
    double dScaleAmt = pow(1.05,sfLapOpts.iZoomLevels);
    
    GLdouble dXShift,dYShift,dZ;
    // project the window shift stuff so we know how far to translate the view
    gluUnProject(-sfLapOpts.flWindowShiftX/dScaleAmt,-sfLapOpts.flWindowShiftY/dScaleAmt,0,rgModelviewMatrix,rgProjMatrix,rgViewport,&dXShift,&dYShift,&dZ);
    
    const double dTranslateShiftX = dCenterX;
    const double dTranslateShiftY = dCenterY;
    glTranslated(dTranslateShiftX,dTranslateShiftY,0);
    glRotated(180.0, 0.0, 0.0, 1.0);
    glScaled(dScaleAmt,dScaleAmt,dScaleAmt);
    glTranslated(-dTranslateShiftX,-dTranslateShiftY,0);

    glTranslated(dXShift-rcAllLaps.right,dYShift-rcAllLaps.top,0);

  }


  // now having shifted, let's get our new model matrices
  glGetDoublev(GL_MODELVIEW_MATRIX, rgModelviewMatrix);
  glGetDoublev(GL_PROJECTION_MATRIX, rgProjMatrix);
  glGetIntegerv(GL_VIEWPORT, rgViewport);

  POINT ptMouse;
  Vector2D vHighlight;
  if(GetMouse(&ptMouse) && m_pLapSupplier->IsHighlightSource(m_iSupplierId))
  {
    // the mouse is in our window... we make our own highlighter
    GLdouble dX,dY,dZ;

    gluUnProject(ptMouse.x,ptMouse.y,0,rgModelviewMatrix,rgProjMatrix,rgViewport,&dX,&dY,&dZ);
    vHighlight = V2D((float)dX,(float)dY);
    
  }
  
  vector<MAPHIGHLIGHT> lstMousePointsToDraw;
  vector<CExtendedLap*> lstLaps = m_pLapSupplier->GetLapsToShow();
  for(int x = 0; x < lstLaps.size(); x++)
  {
    CExtendedLap* pLap = lstLaps[x];
    srand((int)pLap);

    float dBestLength = -1;
    float dTimeToHighlight = -1;
    TimePoint2D ptBest;

    if(sfLapOpts.fDrawLines)
    {
      glBegin(GL_LINE_STRIP);
    }
    else
    {
      glPointSize(5.0f);
      glBegin(GL_POINTS);
    }
    const float r = RandDouble();
    const float g = RandDouble();
    const float b = RandDouble();
    glColor3d( r, g, b ); 
    const vector<TimePoint2D>& lstPoints = pLap->GetPoints();
    for(int x = 0; x< lstPoints.size(); x++)
    {
      const TimePoint2D& p = lstPoints[x];
      glVertex2f(p.flX,p.flY);

      // if we're a highlight source, try to figure out the closest point for this lap
      if(m_pLapSupplier->IsHighlightSource(m_iSupplierId))
      {
        Vector2D vPt = V2D(p.flX,p.flY);
        Vector2D vDiff = vPt - vHighlight;
        if(vDiff.Length() < dBestLength || dBestLength < 0)
        {
          dBestLength = vDiff.Length();
          dTimeToHighlight = p.iTime;
          ptBest = p;
        }
      }
      else
      {
        int iTime = m_pLapSupplier->GetLapHighlightTime(pLap);
        if(abs(p.iTime - iTime) < dBestLength || dBestLength < 0)
        {
          dBestLength = abs(p.iTime - iTime);
          ptBest = p;
          dTimeToHighlight = iTime;
        }
      }
    }

    // for each lap, draw an indicator of the closest thing to the mouse
    if(!m_pLapSupplier->IsHighlightSource(m_iSupplierId))
    {
      // if we're not a source, use the given time to highlight
      dTimeToHighlight = m_pLapSupplier->GetLapHighlightTime(pLap);
    }
    else
    {
      m_pLapSupplier->SetLapHighlightTime(pLap, (int)dTimeToHighlight);
    }

    // project from unit-space (the map) to window-space so we know where we need to draw our highlight-box
    {
      GLdouble winx,winy,winz;
      gluProject(ptBest.flX, ptBest.flY, 0, rgModelviewMatrix, rgProjMatrix, rgViewport, &winx, &winy, &winz);
      MAPHIGHLIGHT mapPt;
      mapPt.pt.x = (int)winx;
      mapPt.pt.y = (int)winy;
      mapPt.pLap = pLap;

      lstMousePointsToDraw.push_back(mapPt);
    }
		glEnd();

  }

  // draw the start-finish lines
  if(lstLaps.size() > 0)
  {
    const CExtendedLap* pLastLap = lstLaps[lstLaps.size()-1];
    const StartFinish* pSF = pLastLap->GetLap()->GetSF();
    for(int x = 0;x < 3; x++)
    {
      Vector2D pt1 = pSF[x].GetPt1();
      Vector2D pt2 = pSF[x].GetPt2();
      glBegin(GL_LINE_STRIP);
      glColor3d(1.0,0.0,0.0);
      glVertex2f(pt1.m_v[0],pt1.m_v[1]);
      glVertex2f(pt2.m_v[0],pt2.m_v[1]);
      glEnd();

      glColor3d(1.0,1.0,1.0);
      LPCSTR lpszText = "";
      if(x == 0) lpszText = "S1";
      if(x == 1) lpszText = "S2";
      if(x == 2) lpszText = "S/F";
      DrawText(pt1.m_v[0],pt1.m_v[1], lpszText);
      DrawText(pt2.m_v[0],pt2.m_v[1], lpszText);
    }
  }


	glPopMatrix(); // popping us out of map-coords space.

  if(lstMousePointsToDraw.size() > 0)
  {
    RECT rcClient;
    GetClientRect(OGL_GetHWnd(),&rcClient);

    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, RECT_WIDTH(&rcClient),0, RECT_HEIGHT(&rcClient),-1.0,1.0);

    for(int x = 0; x < lstMousePointsToDraw.size(); x++)
    {
      const CExtendedLap* pLap = lstMousePointsToDraw[x].pLap;
      const POINT& ptWindow = lstMousePointsToDraw[x].pt;

      srand((int)pLap);
      const float r = RandDouble();
      const float g = RandDouble();
      const float b = RandDouble();
      glColor3d( r, g, b ); 
      
      // we also want to draw a highlighted square
      DrawGLFilledSquare(ptWindow.x, ptWindow.y, 5);
    }
    glPopMatrix(); // pop us from window space back to the identity
  }
}