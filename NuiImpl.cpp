/************************************************************************
*                                                                       *
*   NuiImpl.cpp -- Implementation of CSkeletalViewerApp methods dealing *
*                  with NUI processing                                  *
*                                                                       *
* Copyright (c) Microsoft Corp. All rights reserved.                    *
*                                                                       *
* This code is licensed under the terms of the                          *
* Microsoft Kinect for Windows SDK (Beta)                               *
* License Agreement: http://kinectforwindows.org/KinectSDK-ToU          *
*                                                                       *
************************************************************************/

#include "stdafx.h"
#include "SkeletalViewer.h"
#include "resource.h"

static const COLORREF g_JointColorTable[NUI_SKELETON_POSITION_COUNT] = 
{
    RGB(169, 176, 155), // NUI_SKELETON_POSITION_HIP_CENTER
    RGB(169, 176, 155), // NUI_SKELETON_POSITION_SPINE
    RGB(168, 230, 29), // NUI_SKELETON_POSITION_SHOULDER_CENTER
    RGB(200, 0,   0), // NUI_SKELETON_POSITION_HEAD
    RGB(79,  84,  33), // NUI_SKELETON_POSITION_SHOULDER_LEFT
    RGB(84,  33,  42), // NUI_SKELETON_POSITION_ELBOW_LEFT
    RGB(255, 126, 0), // NUI_SKELETON_POSITION_WRIST_LEFT
    RGB(215,  86, 0), // NUI_SKELETON_POSITION_HAND_LEFT
    RGB(33,  79,  84), // NUI_SKELETON_POSITION_SHOULDER_RIGHT
    RGB(33,  33,  84), // NUI_SKELETON_POSITION_ELBOW_RIGHT
    RGB(77,  109, 243), // NUI_SKELETON_POSITION_WRIST_RIGHT
    RGB(37,   69, 243), // NUI_SKELETON_POSITION_HAND_RIGHT
    RGB(77,  109, 243), // NUI_SKELETON_POSITION_HIP_LEFT
    RGB(69,  33,  84), // NUI_SKELETON_POSITION_KNEE_LEFT
    RGB(229, 170, 122), // NUI_SKELETON_POSITION_ANKLE_LEFT
    RGB(255, 126, 0), // NUI_SKELETON_POSITION_FOOT_LEFT
    RGB(181, 165, 213), // NUI_SKELETON_POSITION_HIP_RIGHT
    RGB(71, 222,  76), // NUI_SKELETON_POSITION_KNEE_RIGHT
    RGB(245, 228, 156), // NUI_SKELETON_POSITION_ANKLE_RIGHT
    RGB(77,  109, 243) // NUI_SKELETON_POSITION_FOOT_RIGHT
};




void CSkeletalViewerApp::Nui_Zero()
{
    m_pNuiInstance = NULL;
    m_hNextDepthFrameEvent = NULL;
    m_hNextVideoFrameEvent = NULL;
    m_hNextSkeletonEvent = NULL;
    m_pDepthStreamHandle = NULL;
    m_pVideoStreamHandle = NULL;
    m_hThNuiProcess=NULL;
    m_hEvNuiProcessStop=NULL;
    ZeroMemory(m_Pen,sizeof(m_Pen));
    m_SkeletonDC = NULL;
    m_SkeletonOldObj = NULL;
    m_PensTotal = 6;
    ZeroMemory(m_Points,sizeof(m_Points));
    m_LastSkeletonFoundTime = -1;
    m_bScreenBlanked = false;
    m_FramesTotal = 0;
    m_LastFPStime = -1;
    m_LastFramesTotal = 0;
}

void CALLBACK CSkeletalViewerApp::Nui_StatusProc(const NuiStatusData *pStatusData)
{
    if (SUCCEEDED(pStatusData->hrStatus))
    {
        // Update UI
        ::PostMessageW(m_hWnd, WM_USER_UPDATE_COMBO, 0, 0);

        if (m_instanceId && 0 == wcscmp(pStatusData->instanceName, m_instanceId))
        {
            Nui_Init(m_instanceId);
        }
        else if (!m_pNuiInstance)
        {
            Nui_Init(0);
        }

    }
    else
    {
        if (0 == wcscmp(pStatusData->instanceName, m_instanceId))
        {
            Nui_UnInit();
            Nui_Zero();
        }
    }
}

HRESULT CSkeletalViewerApp::Nui_Init(OLECHAR *instanceName)
{
    HRESULT hr = MSR_NuiCreateInstanceByName(instanceName, &m_pNuiInstance);

    if (FAILED(hr))
    {
        return hr;
    }

    if (m_instanceId)
    {
        ::SysFreeString(m_instanceId);
    }

    m_instanceId = m_pNuiInstance->NuiInstanceName();

    return Nui_Init();
}

HRESULT CSkeletalViewerApp::Nui_Init(int index)
{
    HRESULT hr = MSR_NuiCreateInstanceByIndex(index, &m_pNuiInstance);

    if (FAILED(hr))
    {
        return hr;
    }

    if (m_instanceId)
    {
        ::SysFreeString(m_instanceId);
    }

    m_instanceId = m_pNuiInstance->NuiInstanceName();

    return Nui_Init();
}

HRESULT CSkeletalViewerApp::Nui_Init()
{
    HRESULT                hr;
    RECT                rc;

    if (!m_pNuiInstance)
    {
        HRESULT hr = MSR_NuiCreateInstanceByIndex(0, &m_pNuiInstance);

        if (FAILED(hr))
        {
            return hr;
        }

        if (m_instanceId)
        {
            ::SysFreeString(m_instanceId);
        }

        m_instanceId = m_pNuiInstance->NuiInstanceName();
    }

    m_hNextDepthFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    m_hNextVideoFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    m_hNextSkeletonEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

    GetWindowRect(GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), &rc );
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

	// initiliaze RGB surface
	HDC hdc = GetDC(GetDlgItem( m_hWnd, IDC_VIDEOVIEW));
	width = rc.right - rc.left;
    height = rc.bottom - rc.top;
    m_VideoBMP = CreateCompatibleBitmap( hdc, width, height );
    m_VideoDC = CreateCompatibleDC( hdc );
    ::ReleaseDC(GetDlgItem(m_hWnd,IDC_VIDEOVIEW), hdc );
    m_VideoOldObj = SelectObject( m_VideoDC, m_VideoBMP );

	hr = m_DrawDepth.CreateDevice( GetDlgItem( m_hWnd, IDC_DEPTHVIEWER ) );
    if( FAILED( hr ) )
    {
        MessageBoxResource( m_hWnd,IDS_ERROR_D3DCREATE,MB_OK | MB_ICONHAND);
        return hr;
    }

    hr = m_DrawDepth.SetVideoType( 320, 240, 320 * 4 );
    if( FAILED( hr ) )
    {
        MessageBoxResource( m_hWnd,IDS_ERROR_D3DVIDEOTYPE,MB_OK | MB_ICONHAND);
        return hr;
    }

    DWORD nuiFlags = NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON |  NUI_INITIALIZE_FLAG_USES_COLOR;
    hr = m_pNuiInstance->NuiInitialize(nuiFlags);
    if (E_NUI_SKELETAL_ENGINE_BUSY == hr)
    {
        nuiFlags = NUI_INITIALIZE_FLAG_USES_DEPTH |  NUI_INITIALIZE_FLAG_USES_COLOR;
        hr = m_pNuiInstance->NuiInitialize(nuiFlags);
    }
    
    if( FAILED( hr ) )
    {
        MessageBoxResource(m_hWnd,IDS_ERROR_NUIINIT,MB_OK | MB_ICONHAND);
        return hr;
    }

    if (HasSkeletalEngine(m_pNuiInstance))
    {
        hr = m_pNuiInstance->NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, 0 );
        if( FAILED( hr ) )
        {
            MessageBoxResource(m_hWnd,IDS_ERROR_SKELETONTRACKING,MB_OK | MB_ICONHAND);
            return hr;
        }
    }

    hr = m_pNuiInstance->NuiImageStreamOpen(
        NUI_IMAGE_TYPE_COLOR,
        NUI_IMAGE_RESOLUTION_640x480,
        0,
        2,
        m_hNextVideoFrameEvent,
        &m_pVideoStreamHandle );
    if( FAILED( hr ) )
    {
        MessageBoxResource(m_hWnd,IDS_ERROR_VIDEOSTREAM,MB_OK | MB_ICONHAND);
        return hr;
    }

    hr = m_pNuiInstance->NuiImageStreamOpen(
        HasSkeletalEngine(m_pNuiInstance) ? NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX : NUI_IMAGE_TYPE_DEPTH,
        NUI_IMAGE_RESOLUTION_320x240,
        0,
        2,
        m_hNextDepthFrameEvent,
        &m_pDepthStreamHandle );
    if( FAILED( hr ) )
    {
        MessageBoxResource(m_hWnd,IDS_ERROR_DEPTHSTREAM,MB_OK | MB_ICONHAND);
        return hr;
    }

    // Start the Nui processing thread
    m_hEvNuiProcessStop=CreateEvent(NULL,FALSE,FALSE,NULL);
    m_hThNuiProcess=CreateThread(NULL,0,Nui_ProcessThread,this,0,NULL);

    return hr;
}



void CSkeletalViewerApp::Nui_UnInit( )
{

    ::SelectObject( m_VideoDC, m_VideoOldObj );
    DeleteDC( m_VideoDC );
    DeleteObject( m_VideoBMP );

    if( m_Pen[0] != NULL )
    {
        DeleteObject(m_Pen[0]);
        DeleteObject(m_Pen[1]);
        DeleteObject(m_Pen[2]);
        DeleteObject(m_Pen[3]);
        DeleteObject(m_Pen[4]);
        DeleteObject(m_Pen[5]);
        ZeroMemory(m_Pen,sizeof(m_Pen));
    }

    // Stop the Nui processing thread
    if(m_hEvNuiProcessStop!=NULL)
    {
        // Signal the thread
        SetEvent(m_hEvNuiProcessStop);

        // Wait for thread to stop
        if(m_hThNuiProcess!=NULL)
        {
            WaitForSingleObject(m_hThNuiProcess,INFINITE);
            CloseHandle(m_hThNuiProcess);
        }
        CloseHandle(m_hEvNuiProcessStop);
    }
 

    if (m_pNuiInstance)
    {
        m_pNuiInstance->NuiShutdown( );
    }
    if( m_hNextSkeletonEvent && ( m_hNextSkeletonEvent != INVALID_HANDLE_VALUE ) )
    {
        CloseHandle( m_hNextSkeletonEvent );
        m_hNextSkeletonEvent = NULL;
    }
    if( m_hNextDepthFrameEvent && ( m_hNextDepthFrameEvent != INVALID_HANDLE_VALUE ) )
    {
        CloseHandle( m_hNextDepthFrameEvent );
        m_hNextDepthFrameEvent = NULL;
    }
    if( m_hNextVideoFrameEvent && ( m_hNextVideoFrameEvent != INVALID_HANDLE_VALUE ) )
    {
        CloseHandle( m_hNextVideoFrameEvent );
        m_hNextVideoFrameEvent = NULL;
    }
    m_DrawDepth.DestroyDevice( );
   // m_DrawVideo.DestroyDevice( );
}



DWORD WINAPI CSkeletalViewerApp::Nui_ProcessThread(LPVOID pParam)
{
    CSkeletalViewerApp *pthis=(CSkeletalViewerApp *) pParam;
    return pthis->Nui_ProcessThread();
}

DWORD WINAPI CSkeletalViewerApp::Nui_ProcessThread()
{
    HANDLE                hEvents[4];
    int                    nEventIdx,t,dt;

    // Configure events to be listened on
    hEvents[0]= m_hEvNuiProcessStop;
    hEvents[1]= m_hNextDepthFrameEvent;
    hEvents[2]= m_hNextVideoFrameEvent;
    hEvents[3]= m_hNextSkeletonEvent;

#pragma warning(push)
#pragma warning(disable: 4127) // conditional expression is constant

    // Main thread loop
    while(1)
    {
        // Wait for an event to be signalled
        nEventIdx=WaitForMultipleObjects(sizeof(hEvents)/sizeof(hEvents[0]),hEvents,FALSE,100);

        // If the stop event, stop looping and exit
        if(nEventIdx==0)
            break;            

        // Perform FPS processing
        t = timeGetTime( );
        if( m_LastFPStime == -1 )
        {
            m_LastFPStime = t;
            m_LastFramesTotal = m_FramesTotal;
        }
        dt = t - m_LastFPStime;
        if( dt > 1000 )
        {
            m_LastFPStime = t;
            int FrameDelta = m_FramesTotal - m_LastFramesTotal;
            m_LastFramesTotal = m_FramesTotal;
            ::PostMessageW(m_hWnd, WM_USER_UPDATE_FPS, IDC_FPS, FrameDelta);
        }

        // Perform skeletal panel blanking
        if( m_LastSkeletonFoundTime == -1 )
            m_LastSkeletonFoundTime = t;
        dt = t - m_LastSkeletonFoundTime;
        if( dt > 250 )
        {
            if( !m_bScreenBlanked )
            {
                m_bScreenBlanked = true;
            }
        }
		
		/*POINT point3 = {40, 40}, point2 = {160, 200}, point1 = {320, 280};
				//DrawSweepingArc(point1, point2, point3);
				POINT point5 = {200, 200}, point6 = {40, 320};
				//DrawSweepingArc(point6, point5, point3);
				POINT point7 = {20, 100}, point8 = {100, 320};
				DrawSweepingArc(point8, point7, point3);
				POINT point9 = {240, 100}, point10 = {100, 320};
				DrawSweepingArc(point10, point9, point3);
				  Nui_DoDoubleBuffer(GetDlgItem(m_hWnd,IDC_SKELETALVIEW), m_SkeletonDC);*/
        // Process signal events
        switch(nEventIdx)
        {
            case 1:
                Nui_GotDepthAlert();
                m_FramesTotal++;
                break;

            case 2:
                Nui_GotVideoAlert();
                break;

            case 3:
                Nui_GotSkeletonAlert( );
				
                break;
        }
    }
#pragma warning(pop)

    return (0);
}

void CSkeletalViewerApp::Nui_GotVideoAlert( )
{
    const NUI_IMAGE_FRAME * pImageFrame = NULL;

    HRESULT hr = m_pNuiInstance->NuiImageStreamGetNextFrame(
        m_pVideoStreamHandle,
        0,
        &pImageFrame );
    if( FAILED( hr ) )
    {
        return;
    }

    INuiFrameTexture * pTexture = pImageFrame->pFrameTexture;
    NUI_LOCKED_RECT LockedRect;
    pTexture->LockRect( 0, &LockedRect, NULL, 0 );
    if( LockedRect.Pitch != 0 )
    {

		RECT rc;
		GetWindowRect(GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), &rc );
		int width = rc.right - rc.left;
		int height = rc.bottom - rc.top;

		// map those bytes onto a bitmap??
		BYTE * pBuffer = (BYTE*) LockedRect.pBits;
		// will paint on this bitmap??
		BITMAP bmp;
		GetObject(m_VideoBMP, sizeof(BITMAP), &bmp);
		BITMAPINFOHEADER bi;
		bi.biSize = sizeof(BITMAPINFOHEADER);
		bi.biWidth = 640;
		bi.biHeight = -480;
		bi.biPlanes = 1;
		bi.biBitCount = 32;
		bi.biCompression = BI_RGB;
		bi.biClrUsed = 0;
		//bi.biSizeImage = ((((bi.biWidth * bi.biBitCount) + 31) & ~31) >> 3) * bi.biHeight;
		bi.biSizeImage = (bi.biWidth * bi.biBitCount) * bi.biHeight;
		SetDIBits(m_VideoDC, m_VideoBMP, 0, 480, pBuffer, (BITMAPINFO*)&bi, NULL);
		SelectObject(m_VideoDC, m_VideoBMP);
//      m_DrawVideo.DrawFrame( (BYTE*) pBuffer );
    }
    else
    {
        OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
    }

	if (m_bScreenBlanked) {
		Nui_DoDoubleBuffer(GetDlgItem(m_hWnd,IDC_VIDEOVIEW), m_VideoDC);
	}
	
    m_pNuiInstance->NuiImageStreamReleaseFrame( m_pVideoStreamHandle, pImageFrame );
}


void CSkeletalViewerApp::Nui_GotDepthAlert( )
{
    const NUI_IMAGE_FRAME * pImageFrame = NULL;

    HRESULT hr = m_pNuiInstance->NuiImageStreamGetNextFrame(
        m_pDepthStreamHandle,
        0,
        &pImageFrame );

    if( FAILED( hr ) )
    {
        return;
    }

    INuiFrameTexture * pTexture = pImageFrame->pFrameTexture;
    NUI_LOCKED_RECT LockedRect;
    pTexture->LockRect( 0, &LockedRect, NULL, 0 );
    if( LockedRect.Pitch != 0 )
    {
        BYTE * pBuffer = (BYTE*) LockedRect.pBits;

        // draw the bits to the bitmap
        RGBQUAD * rgbrun = m_rgbWk;
        USHORT * pBufferRun = (USHORT*) pBuffer;
        for( int y = 0 ; y < 240 ; y++ )
        {
            for( int x = 0 ; x < 320 ; x++ )
            {
                RGBQUAD quad = Nui_ShortToQuad_Depth( *pBufferRun );
                pBufferRun++;
                *rgbrun = quad;
                rgbrun++;
            }
        }

        m_DrawDepth.DrawFrame( (BYTE*) m_rgbWk );
    }
    else
    {
        OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
    }

    m_pNuiInstance->NuiImageStreamReleaseFrame( m_pDepthStreamHandle, pImageFrame );
}

void CSkeletalViewerApp::Nui_BlankSkeletonScreen(HWND hWnd)
{
    HDC hdc = GetDC( hWnd );
    RECT rct;
    GetClientRect(hWnd, &rct);
    int width = rct.right;
    int height = rct.bottom;
    PatBlt( hdc, 0, 0, width, height, BLACKNESS );
    ReleaseDC( hWnd, hdc );
}

void CSkeletalViewerApp::Nui_DrawSkeletonSegment(HDC dc, NUI_SKELETON_DATA * pSkel, int numJoints, ... )
{
    va_list vl;
    va_start(vl,numJoints);

    POINT segmentPositions[NUI_SKELETON_POSITION_COUNT];
    int segmentPositionsCount = 0;

    DWORD polylinePointCounts[NUI_SKELETON_POSITION_COUNT];
    int numPolylines = 0;
    int currentPointCount = 0;

    // Note the loop condition: We intentionally run one iteration beyond the
    // last element in the joint list, so we can properly end the final polyline.

    for (int iJoint = 0; iJoint <= numJoints; iJoint++)
    {
        if (iJoint < numJoints)
        {
            NUI_SKELETON_POSITION_INDEX jointIndex = va_arg(vl,NUI_SKELETON_POSITION_INDEX);

            if (pSkel->eSkeletonPositionTrackingState[jointIndex] != NUI_SKELETON_POSITION_NOT_TRACKED)
            {
                // This joint is tracked: add it to the array of segment positions.
            
                segmentPositions[segmentPositionsCount].x = m_Points[jointIndex].x;
                segmentPositions[segmentPositionsCount].y = m_Points[jointIndex].y;

				if (jointIndex == NUI_SKELETON_POSITION_KNEE_RIGHT) {
					DrawSweepingArc(dc, pSkel->SkeletonPositions, jointIndex);
				}

                segmentPositionsCount++;
                currentPointCount++;

                // Fully processed the current joint; move on to the next one

                continue;
            }
        }

        // If we fall through to here, we're either beyond the last joint, or
        // the current joint is not tracked: end the current polyline here.

        if (currentPointCount > 1)
        {
            // Current polyline already has at least two points: save the count.

            polylinePointCounts[numPolylines++] = currentPointCount;
        }
        else if (currentPointCount == 1)
        {
            // Current polyline has only one point: ignore it.

            segmentPositionsCount--;
        }
        currentPointCount = 0;
    }

#ifdef _DEBUG
    // We should end up with no more points in segmentPositions than the
    // original number of joints.

    assert(segmentPositionsCount <= numJoints);

    int totalPointCount = 0;
    for (int i = 0; i < numPolylines; i++)
    {
        // Each polyline should contain at least two points.
    
        assert(polylinePointCounts[i] > 1);

        totalPointCount += polylinePointCounts[i];
    }

    // Total number of points in all polylines should be the same as number
    // of points in segmentPositions.
    
    assert(totalPointCount == segmentPositionsCount);
#endif

    if (numPolylines > 0)
    {
        PolyPolyline(dc, segmentPositions, polylinePointCounts, numPolylines);
    }

    va_end(vl);
}

double CalcDist(POINT3D point1, POINT3D point2) {
	return sqrt(powf(point2.x - point1.x, 2.0) + powf(point2.y - point1.y, 2.0) + powf(point2.z - point1.z, 2.0));
}

double CalcSlope(POINT3D point1, POINT3D point2) {
	// determine starting point for angle relative to x-axis
	double xDiff = point2.x - point1.x;
	return xDiff != 0 ? -(point2.y - point1.y) / xDiff : 0;
}

INT CalcQuadrant(POINT3D reference, POINT3D point) {
	INT result = 0;
	BOOL xSmaller = point.x < reference.x;
	BOOL ySmaller = point.y < reference.y;
	if (xSmaller) {
		result = ySmaller ? 2 : 3;
	} else {
		result = ySmaller ? 1 : 4;
	}
	return result;
}

double CalcAngle(Vector4 point1, Vector4 point2, Vector4 point3) {
	double dotProduct = (point1.x - point2.x) * (point3.x - point2.x) + (point1.y - point2.y) * (point3.y - point2.y) + (point1.z - point2.z) * (point3.z - point2.z);
	double distA = CalcDist(point1, point2);
	double distB = CalcDist(point2, point3);
	double cosAlpha = dotProduct / (distA * distB);
	return acos(cosAlpha) * 180 / M_PI;
}

double AngleFromSlope(double slope) {
	double result = slope != 0 ? atan(slope) : M_PI/2;
	// convert to degrees
	return result * 180 / M_PI;
}

double CalcStartAngle(POINT3D point1, POINT3D point2, POINT3D point3) {
	INT quadrant1 = CalcQuadrant(point2, point1);
	INT quadrant2 = CalcQuadrant(point2, point3);
	INT chosenQuadrant;
	POINT3D firstPoint;
	// quadrant 4 is smaller than quadrant 1
	if (quadrant1 < quadrant2 || (quadrant2 == 1 && quadrant1 == 4)) {
		firstPoint = point1;
		chosenQuadrant = quadrant1;
	} else {
		firstPoint = point3;
		chosenQuadrant = quadrant2;
	}
	double slope = CalcSlope(point2, firstPoint);
	double angle = AngleFromSlope(slope);
	INT evenQuadrant = chosenQuadrant % 2;
	return angle + 90 * (chosenQuadrant - evenQuadrant);
}

void CSkeletalViewerApp::DrawSweepingArc(HDC dc, Vector4 *skeletonPositions, INT jointIndex) {
	INT OFFSET = 20;
	// darw arc, get coordinates in meters
	POINT3D point1 = skeletonPositions[jointIndex + 1];
	POINT3D point2 = skeletonPositions[jointIndex];
	POINT3D point3 = skeletonPositions[jointIndex - 1];

	double angle = CalcAngle(point1, point2, point3);

	// paint all this to screen
	point1 = m_Points[jointIndex + 1];
	point2 = m_Points[jointIndex];
	point3 = m_Points[jointIndex - 1];
	//double angle = acos((pow(distA, 2.0) + pow(distB, 2.0) - pow(distC, 2.0)) / (2 * distA * distB)); 
	//angle = angle * 180 / M_PI;
	double startAngle = CalcStartAngle(point1, point2, point3);
	
	POINT oldPosition;
	HPEN pen = CreatePen( PS_SOLID, 3, RGB(255, 0, 0) );
	HGDIOBJ oldObj = SelectObject(dc, pen);
	MoveToEx(dc, point1.x, point1.y, &oldPosition);
	#ifdef _DEBUG
	/*LineTo(dc, point2.x, point2.y);
	LineTo(dc, point3.x, point3.y);
	LineTo(dc, point1.x, point1.y);*/
	#endif
	MoveToEx(dc, point2.x, point2.y, NULL);
	pen = CreatePen( PS_SOLID, 3, RGB(255, 255, 0) );
	SelectObject(dc, pen);
	AngleArc(dc, point2.x, point2.y, OFFSET, startAngle, angle);
	LineTo(dc, point2.x, point2.y);
	MoveToEx(dc, oldPosition.x, oldPosition.y, NULL);

	CString text;
	// create function to draw angle
	text.Format(_T("Angle: %.2f�"), angle);
	int sizeOfString = (text.GetLength() + 1);
	LPTSTR lpsz = new TCHAR[ sizeOfString ];	
	_tcscpy_s(lpsz, sizeOfString, text);
	RECT rect = {30, 5, 5, 400};
	pen = CreatePen( PS_SOLID, 3, RGB(255, 120, 0) );
	SelectObject(dc, pen);
	DrawText(dc, lpsz, -1, &rect, DT_BOTTOM);
	delete lpsz;
	SelectObject(dc, oldObj);
}

void AddMarker( INT row, Vector4 vector, btk::Point::Values& traj) {
	traj.coeffRef(row, 0) = vector.x;
	traj.coeffRef(row, 1) = vector.y;
	traj.coeffRef(row, 2) = vector.z;
}

void AddAngle(INT row, btk::Point::Pointer point, NUI_SKELETON_DATA * pSkel, int joint1,
	int joint2, int joint3, std::string label ) {
	double angle = CalcAngle(pSkel->SkeletonPositions[joint1], 
		pSkel->SkeletonPositions[joint2], pSkel->SkeletonPositions[joint3]);
	if (angle != 0) {
		btk::Point::Values& traj = point->GetValues();
		traj.coeffRef(row, 0) = angle;
		traj.coeffRef(row, 1) = angle;
		traj.coeffRef(row, 2) = angle;
		point->SetLabel(label);
	}
}


void CSkeletalViewerApp::Nui_DrawSkeleton(HDC dc, bool bBlank, NUI_SKELETON_DATA * pSkel, HWND hWnd, int WhichSkeletonColor )
{
    HGDIOBJ hOldObj = SelectObject(dc,m_Pen[WhichSkeletonColor % m_PensTotal]);
    
    RECT rct;
    GetClientRect(hWnd, &rct);
    int width = rct.right;
    int height = rct.bottom;

    if( m_Pen[0] == NULL )
    {
        m_Pen[0] = CreatePen( PS_SOLID, width / 80, RGB(255, 0, 0) );
        m_Pen[1] = CreatePen( PS_SOLID, width / 80, RGB( 0, 255, 0 ) );
        m_Pen[2] = CreatePen( PS_SOLID, width / 80, RGB( 64, 255, 255 ) );
        m_Pen[3] = CreatePen( PS_SOLID, width / 80, RGB(255, 255, 64 ) );
        m_Pen[4] = CreatePen( PS_SOLID, width / 80, RGB( 255, 64, 255 ) );
        m_Pen[5] = CreatePen( PS_SOLID, width / 80, RGB( 128, 128, 255 ) );
    }

    if( bBlank )
    {
		//PatBlt( dc, 0, 0, width, height, BLACKNESS );
    }

    int scaleX = 320; //scaling up to image coordinates
    int scaleY = 240;
    float fx=0,fy=0;
	USHORT fz;
    int i;
	LONG color_x, color_y;
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
    {
        NuiTransformSkeletonToDepthImageF( pSkel->SkeletonPositions[i], &fx, &fy, &fz );
		m_Points[i].x = (int) ( fx * scaleX + 0.5f );
		m_Points[i].y = (int) ( fy * scaleY + 0.5f );
		// transform skeleton coordinates to RGB space
		NuiImageGetColorPixelCoordinatesFromDepthPixel(NUI_IMAGE_RESOLUTION_640x480,0,m_Points[i].x,m_Points[i].y,fz,&color_x,&color_y);
		// if recording, write coordinates to disk
		m_Points[i].x = color_x;
		m_Points[i].y = color_y;
    }

    SelectObject(dc,m_Pen[WhichSkeletonColor%m_PensTotal]);
    
    Nui_DrawSkeletonSegment(dc,pSkel,4,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HEAD);
    Nui_DrawSkeletonSegment(dc,pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);
    Nui_DrawSkeletonSegment(dc,pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);
    Nui_DrawSkeletonSegment(dc,pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);
    Nui_DrawSkeletonSegment(dc,pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);
    
    // Draw the joints in a different color
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT ; i++)
    {
        if (pSkel->eSkeletonPositionTrackingState[i] != NUI_SKELETON_POSITION_NOT_TRACKED)
        {
            HPEN hJointPen;
        
            hJointPen=CreatePen(PS_SOLID,9, g_JointColorTable[i]);
            hOldObj=SelectObject(dc,hJointPen);

            MoveToEx( dc, m_Points[i].x, m_Points[i].y, NULL );
            LineTo( dc, m_Points[i].x, m_Points[i].y );

            SelectObject( dc, hOldObj );
            DeleteObject(hJointPen);
        }
    }

    return;
}

void CSkeletalViewerApp::Nui_DoDoubleBuffer(HWND hWnd,HDC hDC)
{
    RECT rct;
    GetClientRect(hWnd, &rct);
    int width = rct.right;
    int height = rct.bottom;

    HDC hdc = GetDC( hWnd );

    BitBlt( hdc, 0, 0, width, height, hDC, 0, 0, SRCCOPY );

    ReleaseDC( hWnd, hdc );

}

void CSkeletalViewerApp::Nui_WriteToFile(NUI_SKELETON_DATA * pSkel) {
	if (m_bRecording && m_pWriter) {
		m_pAcquisition->ResizeFrameNumber(m_RecordedFrames);
		for (INT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++) {
			if ( pSkel->eSkeletonPositionTrackingState[i] != NUI_SKELETON_POSITION_NOT_TRACKED) {
				btk::Point::Pointer point = m_pAcquisition->GetPoint(i);
				//point->SetFrameNumber(m_RecordedFrames);
				btk::Point::Values& traj = point->GetValues();
				AddMarker(m_RecordedFrames - 1, pSkel->SkeletonPositions[i], traj);
			}
		 }
		 btk::Point::Pointer point = m_pAcquisition->GetPoint(NUI_SKELETON_POSITION_COUNT);
		 AddAngle(m_RecordedFrames - 1, point, pSkel, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, "Left Knee Angle");
		 point = m_pAcquisition->GetPoint(NUI_SKELETON_POSITION_COUNT + 1);
		 AddAngle(m_RecordedFrames - 1, point, pSkel, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, "Right Knee Angle");
		 point = m_pAcquisition->GetPoint(NUI_SKELETON_POSITION_COUNT + 2);
		 AddAngle(m_RecordedFrames - 1, point, pSkel, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT, "Left Foot Angle");
		 point = m_pAcquisition->GetPoint(NUI_SKELETON_POSITION_COUNT + 3);
		 AddAngle(m_RecordedFrames - 1, point, pSkel, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT, "Right Foot Angle");
		 m_RecordedFrames++;
	}

}

void CSkeletalViewerApp::Nui_GotSkeletonAlert( )
{
    NUI_SKELETON_FRAME SkeletonFrame;

    bool bFoundSkeleton = false;

    if( SUCCEEDED(m_pNuiInstance->NuiSkeletonGetNextFrame( 0, &SkeletonFrame )) )
    {
        for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
        {
            if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
            {
                bFoundSkeleton = true;
            }
        }
    }

    // no skeletons!
    //
    if( !bFoundSkeleton )
    {
        return;
    }

    // smooth out the skeleton data
    m_pNuiInstance->NuiTransformSmooth(&SkeletonFrame,NULL);

    // we found a skeleton, re-start the timer
    m_bScreenBlanked = false;
    m_LastSkeletonFoundTime = -1;

    // draw each skeleton color according to the slot within they are found.
    //
    bool bBlank = true;
    for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
    {
        // Show skeleton only if it is tracked, and the center-shoulder joint is at least inferred.
        if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED &&
            SkeletonFrame.SkeletonData[i].eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER] != NUI_SKELETON_POSITION_NOT_TRACKED)
        {
            Nui_DrawSkeleton( m_VideoDC, bBlank, &SkeletonFrame.SkeletonData[i], GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), i );
			// just track first skeleton only
			Nui_WriteToFile(  &SkeletonFrame.SkeletonData[i] );
		
			RECT rc;
			GetWindowRect(GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), &rc );
			int width = rc.right - rc.left;
			int height = rc.bottom - rc.top;
			// draw skeleton over rgb bitmap
			//BitBlt(m_VideoDC, 0, 0, width, height, m_SkeletonDC, 0, 0, SRCPAINT);

            bBlank = false;
        }
    }

    //Nui_DoDoubleBuffer(GetDlgItem(m_hWnd,IDC_SKELETALVIEW), m_SkeletonDC);
	Nui_DoDoubleBuffer(GetDlgItem(m_hWnd,IDC_VIDEOVIEW), m_VideoDC);
	// get complete bitmap and write to avi file
	if (m_bRecording && m_pAviFile) {
		HGDIOBJ result = SelectObject(m_VideoDC, m_VideoBMP); 
		m_pAviFile->AppendNewFrame(m_VideoBMP);
	}
}



RGBQUAD CSkeletalViewerApp::Nui_ShortToQuad_Depth( USHORT s )
{
    bool hasPlayerData = HasSkeletalEngine(m_pNuiInstance);
    USHORT RealDepth = hasPlayerData ? (s & 0xfff8) >> 3 : s & 0xffff;
    USHORT Player = hasPlayerData ? s & 7 : 0;

    // transform 13-bit depth information into an 8-bit intensity appropriate
    // for display (we disregard information in most significant bit)
    BYTE l = 255 - (BYTE)(256*RealDepth/0x0fff);

    RGBQUAD q;
    q.rgbRed = q.rgbBlue = q.rgbGreen = 0;

    switch( Player )
    {
    case 0:
        q.rgbRed = l / 2;
        q.rgbBlue = l / 2;
        q.rgbGreen = l / 2;
        break;
    case 1:
        q.rgbRed = l;
        break;
    case 2:
        q.rgbGreen = l;
        break;
    case 3:
        q.rgbRed = l / 4;
        q.rgbGreen = l;
        q.rgbBlue = l;
        break;
    case 4:
        q.rgbRed = l;
        q.rgbGreen = l;
        q.rgbBlue = l / 4;
        break;
    case 5:
        q.rgbRed = l;
        q.rgbGreen = l / 4;
        q.rgbBlue = l;
        break;
    case 6:
        q.rgbRed = l / 2;
        q.rgbGreen = l / 2;
        q.rgbBlue = l;
        break;
    case 7:
        q.rgbRed = 255 - ( l / 2 );
        q.rgbGreen = 255 - ( l / 2 );
        q.rgbBlue = 255 - ( l / 2 );
    }

    return q;
}
