//-------------------------------------------------------------------------------------------------
// �������� ������������� ����� �� ������� ���� ��� ������ DX SDK � Windows Media Foundation
//-------------------------------------------------------------------------------------------------
#include "std.h"

using namespace DirectX;
using namespace::std;

#define _WIN32_WINNT_WIN10 
#define WM_CLIENTRECTCHANGE 0x8001
#define dll_name  (CHAR*)"Hook2.dll"
#define urok_fx   (WCHAR*)L"animSh.fx"

typedef  void(__cdecl *proc)(HWND);

//--------------------------------------------------------------------------------------
// ���������
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT2 Tex;
 };

//--------------------------------------------------------------------------------------
// ���������� ����������
//--------------------------------------------------------------------------------------
HINSTANCE                 g_hInst = NULL;           // �������� ���� ����������
HWND                      g_hDesktopWnd = NULL;     // ���������� ���� � ������� �� ��������
HWND                      g_hWnd = NULL;            // ����������� ���������� ����������
HANDLE                    g_hMutex = NULL;          // ���������� ��������

D3D_DRIVER_TYPE           g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL         g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*             g_pd3dDevice = NULL;		    // ���������� (��� �������� ��������)
ID3D11DeviceContext*      g_pImmediateContext = NULL;	// �������� ���������� (���������)
IDXGISwapChain*           g_pSwapChain = NULL;		    // ���� ����� (������ � �������)
ID3D11RenderTargetView*   g_pRenderTargetView = NULL;	// ������ ������� ������
ID3D11VertexShader*       g_pVertexShader = NULL;		// ��������� ������
ID3D11PixelShader*        g_pPixelShader = NULL;		// ���������� ������
ID3D11InputLayout*        g_pVertexLayout = NULL;		// �������� ������� ������
ID3D11Buffer*             g_pVertexBuffer = NULL;		// ����� ������
ID3D11ShaderResourceView* m_pTextureRV = NULL;
ID3D11SamplerState*       m_pSamplerLinear = NULL;

DWORD    flag         = NULL;
DWORD    pID          = NULL;
LONGLONG dwDuration   = NULL;
UINT     uiFlags      = NULL;
WCHAR    szFileName   [MAX_PATH];
char     path[256];

//--------------------------------------------------------------------------------------
// ���������� ���������� ��� Media Foundation
//--------------------------------------------------------------------------------------
IMFSourceReader*                g_pReader = NULL;
IMFMediaSource*                 g_pSource = NULL;
ID3D11VideoProcessor*           pVideoProcessor = NULL;
ID3D11Texture2D*                pD3DVideoTexture = NULL;
ID3D11VideoDecoderOutputView*   pVideoDecoderOutputView = NULL;
ID3D11VideoContext*             pVideoContext = NULL;
ID3D11VideoDevice*              pVideoDevice = NULL;
ID3D11VideoProcessorEnumerator* pVideoEnum = NULL;
ID3D11VideoProcessorOutputView* pOutputView = NULL;
ID3D11VideoProcessorInputView*  pInputView = NULL;

#define MAX_MONITOR unsigned int mon = 3; 
vector <HMONITOR> m_vectAllMonitors = {};
vector <MONITORINFO> m_vectInfoMonitors = {};
vector <D3D11_VIEWPORT> m_vectViewPort = {};

//--------------------------------------------------------------------------------------
// ��������������� ���������� �������
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );  // �������� ����
HRESULT InitDevice();	// ������������� ��������� DirectX
HRESULT InitGeometry();	// ������������� ������� ����� � ������ ������
HRESULT InitDllToExplorer();
HRESULT ResetSwapChain(DWORD width, DWORD height);
void CleanupDevice();	// �������� ���������� ��������� DirectX
void Render();          // ������� ���������
HRESULT NextFrame();
HRESULT InitMFfromUrlVideo();
HRESULT CreateMediaSource(IMFMediaSource** ppSource);
BOOL InitMonitorsInfo();
BOOL CreateMutexForDll();
void ClearMFC();
void CheckSatusExplorer();

LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

BOOL CALLBACK EnumMonitorsProc(HMONITOR hMonitor, HDC, LPRECT rect, LPARAM lParam); // ������� ����

BOOL CALLBACK enumWindowsProc(__in  HWND hWnd, __in  LPARAM lParam)
{

    HWND p = FindWindowExW(hWnd, NULL, L"SHELLDLL_DefView", NULL);

    if (p != NULL)
    {
        g_hDesktopWnd = FindWindowExW(NULL, hWnd, L"WorkerW", NULL);
    }

    return TRUE;
}

BOOL CALLBACK EnumMonitorsProc(HMONITOR hMonitor, HDC, LPRECT rect, LPARAM lParam)
{
    m_vectAllMonitors.push_back(hMonitor);
    return true;
}

BOOL GetFileOpen(WCHAR* pszFileName, DWORD cchFileName)
{
    pszFileName[0] = L'\0';

    OPENFILENAME ofn;
    RtlZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = L"*.mp4 Files\0*.mp4\0";
    ofn.lpstrFile = pszFileName;
    ofn.nMaxFile = cchFileName;
    ofn.lpstrTitle = L"Select an .mp4 file to display...";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    // Display the Open dialog box.
    return GetOpenFileName(&ofn);
}

HRESULT FindWindowDestopWorker()
{
 
    HWND hTempWnd = NULL;
    hTempWnd = FindWindowW(L"Progman", L"Program Manager");
    if (hTempWnd == NULL)
        return S_FALSE;
    if (SendMessageTimeout(hTempWnd, 0x052C, NULL, NULL, SMTO_NORMAL, 1000, NULL) == NULL)
        return S_FALSE;
    EnumWindows((WNDENUMPROC)enumWindowsProc, NULL);

#ifdef DEBUG
    cout << "HWND DESKTOP: " << g_hDesktopWnd << endl;
#endif // DEBUG

    return S_OK;
}

HRESULT ChangeCurrentPositionVideo()
{

    HRESULT hr = S_OK;

    PROPVARIANT var;
    PropVariantInit(&var);

    var.vt = VT_I8;
    var.hVal.QuadPart = 0;

    hr = g_pReader->SetCurrentPosition(
        MFP_POSITIONTYPE_100NS,
        var
    );

    PropVariantClear(&var);

    return hr;
}

BOOL isProcessRun(LPTSTR processName)
{
    HANDLE hSnap = NULL;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != NULL)
    {
        if (Process32First(hSnap, &pe32))
        {
            if (lstrcmp(pe32.szExeFile, processName) == 0 && (pe32.th32ProcessID == pID))
                return TRUE;
            while (Process32Next(hSnap, &pe32))
               if (lstrcmp(pe32.szExeFile, processName) == 0 && (pe32.th32ProcessID == pID))
                    return TRUE;
        }
    }
    CloseHandle(hSnap);
    return FALSE;
}

//--------------------------------------------------------------------------------------
// ����� ����� � ���������. ������������� ���� �������� � ���� � ���� ���������.
// ��������� ����� ������������ ��� ��������� �����.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    MFStartup(MF_VERSION);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        

#ifdef DEBUG

    //�������� ������� 
    AllocConsole();
    FILE* pCout;
    freopen_s(&pCout, "conout$", "w", stdout);

#endif // DEBUG

    //�������� ������������ ��������
    if (!CreateMutexForDll())
        return 0;

    //����� ����, � ������� ����� ����������� ������
    if (FAILED(FindWindowDestopWorker()))
        return 0;

    // �������� ���� ����������
    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

    //�������� DLL � ������ �������� Explorer
    if (FAILED(InitDllToExplorer()))
        return 0;

    //������������� ��������� � ViewPorts
    if (!InitMonitorsInfo())
        return 0;

	// �������� �������� DirectX
    if( FAILED( InitDevice() ) )
    {
        CleanupDevice();
        return 0;
    }

	// �������� �������� � ������ ������
    if( FAILED( InitGeometry() ) )
    {
        CleanupDevice();
        return 0;
    }

    if (!GetFileOpen(szFileName, ARRAYSIZE(szFileName)))
    {
        CleanupDevice();
        return 0;
    }

    //������������� Media Foundation
    if (FAILED(InitMFfromUrlVideo()))
    {
        CleanupDevice();
        return 0;
    }

    // ������� ���� ���������
    MSG msg = {0};
    while( WM_QUIT != msg.message )
    {
        if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else	// ���� ��������� ���
        {
            NextFrame();       // ���������� �����
            Render();	       // ������
            Sleep(dwDuration); // �������� ����� �� ����� �������� �����
        }
    }


    ReleaseMutex(g_hMutex);

#ifdef DEBUG
    if (pCout)  fclose(pCout);
    FreeConsole();
#endif // DEBUG

    ClearMFC();
    MFShutdown();
    CoUninitialize();
    CleanupDevice();
    return ( int )msg.wParam;
}

//--------------------------------------------------------------------------------------
// ����������� ������ � �������� ����
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // ����������� ������
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon( hInstance, NULL );
    wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"WallpaperAnimation";
    wcex.hIconSm = LoadIcon( wcex.hInstance, NULL);
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // �������� ����
    g_hInst = hInstance;
    RECT rc = { 0, 0, 1366, 768 };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	g_hWnd = CreateWindowW( L"WallpaperAnimation", L"WallpaperAnimation",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, g_hDesktopWnd, NULL, hInstance,
                           NULL );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, SW_HIDE);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// ���������� ������ ���, ����� ���������� �������� ��������� ���������
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch( message )
    {
        case WM_PAINT:
            hdc = BeginPaint( hWnd, &ps );
            EndPaint( hWnd, &ps );
            break;

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;

        case WM_CLIENTRECTCHANGE:
        {

#ifdef DEBUG
            cout << "DISPLAY_CHANGED" << endl;
            cout << "width: " << DWORD(wParam) << " height: " << DWORD(lParam) << endl;
#endif // DEBUG

            // ������������ SwapChain � ��� �����.
            if (FAILED(ResetSwapChain(DWORD(wParam), DWORD(lParam))))
            {
                MessageBoxW(NULL, L"���������� ����������� SwapChain.", L"������", MB_OK);
                PostQuitMessage(0);
                break;
            }

            // �������� ����� ���������� � ���������� ���������,
            // � ������� ����� ViewPorts.
            InitMonitorsInfo();

            break;
        }

        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// ��������������� ������� ��� ���������� �������� � D3DX11
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
    ID3DBlob* pErrorBlob;
    hr = D3DCompileFromFile( szFileName, NULL, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        if( pErrorBlob ) pErrorBlob->Release();
        return hr;
    }
    if( pErrorBlob ) pErrorBlob->Release();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// �������� ���������� Direct3D (D3D Device), ��������� ���� (Swap Chain) �
// ��������� ���������� (Immediate Context).
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_hDesktopWnd, &rc );

    UINT width = rc.right - rc.left;	// �������� ������
    UINT height = rc.bottom - rc.top;	// � ������ ����

    UINT createDeviceFlags = (0 * D3D11_CREATE_DEVICE_SINGLETHREADED) | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#ifdef _DEBUG
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE( driverTypes );

    // ��� �� ������� ������ �������������� ������ DirectX
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
	UINT numFeatureLevels = ARRAYSIZE( featureLevels );

	// ������ �� �������� ���������� DirectX. ��� ������ �������� ���������,
	// ������� ��������� �������� ��������� ������ � ����������� ��� � ������ ����.
    DXGI_SWAP_CHAIN_DESC sd;			// ���������, ����������� ���� ����� (Swap Chain)
    ZeroMemory( &sd, sizeof( sd ) );	// ������� ��
	sd.BufferCount = 1;					// � ��� ���� �����
    sd.BufferDesc.Width = width;		// ������ ������
    sd.BufferDesc.Height = height;		// ������ ������
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	// ������ ������� � ������
    sd.BufferDesc.RefreshRate.Numerator = 75;			// ������� ���������� ������
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// ���������� ������ - ������ �����
    sd.OutputWindow = g_hDesktopWnd;							// ����������� � ������ ����
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;						// �� ������������� �����

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain( NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
                                            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
        if (SUCCEEDED(hr))  // ���� ���������� ������� �������, �� ������� �� �����
            break;
    }
    if (FAILED(hr))
        return hr;

    // ������ ������� ������ �����. �������� ��������, � SDK
    // RenderTargetOutput - ��� �������� �����, � RenderTargetView - ������.

	// ��������� �������� ������� ������
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&pBackBuffer );
    if (FAILED(hr))	return hr;

	// �� ����������� �������� ������� ����������� ���������
    hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &g_pRenderTargetView );
    pBackBuffer->Release();
    if (FAILED(hr))	return hr;

    // ���������� ������ ������� ������ � ��������� ����������
    g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// �������� ������ ������, �������� (shaders) � �������� ������� ������ (input layout)
//--------------------------------------------------------------------------------------
HRESULT InitGeometry()
{
	HRESULT hr = S_OK;

	// ���������� ���������� ������� �� �����
    ID3DBlob* pVSBlob = NULL; // ��������������� ������ - ������ ����� � ����������� ������
    hr = CompileShaderFromFile(urok_fx, "VS", "vs_4_0", &pVSBlob );
    if (FAILED(hr))
    {
        MessageBoxW( NULL, L"���������� �������������� ���� FX. ����������, ��������� ������ ��������� �� �����, ���������� ���� FX.", L"������", MB_OK);
        return hr;
    }

	// �������� ���������� �������
	hr = g_pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader );
	if( FAILED( hr ) )
	{	
		pVSBlob->Release();
        return hr;
	}

    // ����������� ������� ������
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		/* ������������� ���, ������������� ������, ������, �������� ���� (0-15), ����� ������ ������
		   � ������ ������, ����� ��������� ����� (�� �����), InstanceDataStepRate (�� �����) */
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
	UINT numElements = ARRAYSIZE( layout );

    // �������� ������� ������
	hr = g_pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
                                          pVSBlob->GetBufferSize(), &g_pVertexLayout );
	pVSBlob->Release();
	if (FAILED(hr)) return hr;

    // ����������� ������� ������
    g_pImmediateContext->IASetInputLayout( g_pVertexLayout );

	// ���������� ����������� ������� �� �����
	ID3DBlob* pPSBlob = NULL;
    hr = CompileShaderFromFile(urok_fx, "PS", "ps_4_0", &pPSBlob );
    if( FAILED( hr ) )
    {
        MessageBoxW( NULL, L"���������� �������������� ���� FX. ����������, ��������� ������ ��������� �� �����, ���������� ���� FX.", L"������", MB_OK );
        return hr;
    }

	// �������� ����������� �������
	hr = g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader );
	pPSBlob->Release();
	if (FAILED(hr)) return hr;

    // �������� ������ ������ (������ ������� ������������)
    SimpleVertex vertices[] = {
        { DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
        { DirectX::XMFLOAT3(1.0f,  1.0f,  1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
        { DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }
	};

	D3D11_BUFFER_DESC bd;	// ���������, ����������� ����������� �����
	ZeroMemory( &bd, sizeof(bd) );				// ������� ��
    bd.Usage = D3D11_USAGE_DEFAULT;	
    bd.ByteWidth = sizeof( SimpleVertex ) * 4;	// ������ ������
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// ��� ������ - ����� ������
	bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData; // ���������, ���������� ������ ������
	ZeroMemory( &InitData, sizeof(InitData) );	// ������� ��
    InitData.pSysMem = vertices;				// ��������� �� ���� 4 �������
	// ����� ������ g_pd3dDevice ������� ������ ������ ������
    hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pVertexBuffer );
	if (FAILED(hr)) return hr;

    // ��������� ������ ������
    UINT stride = sizeof( SimpleVertex );
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers( 0, 1, &g_pVertexBuffer, &stride, &offset );

    // ��������� ������� ��������� ������ � ������ (� ������ ������ - TRIANGLE LIST,
	// �. �. ����� 1-3 - ������ �����������, 4-6 - ������ � �. �. ������ ������ - TRIANGLE STRIP.
	// � ���� ������ ����� 1-3 - ������ �����������, 2-4 - ������, 3-5 - ������ � �. �.
    g_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear);
    if (FAILED(hr))
        return hr;



	return S_OK;
}

//--------------------------------------------------------------------------------------
// ������������ ���� ��������� ��������
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    // ������� �������� �������� ����������
    if( g_pImmediateContext ) g_pImmediateContext->ClearState();
	// ����� ������ �������
    if (m_pTextureRV) m_pTextureRV->Release();
    if (m_pSamplerLinear) m_pSamplerLinear->Release();
    if( g_pVertexBuffer ) g_pVertexBuffer->Release();
    if( g_pVertexLayout ) g_pVertexLayout->Release();
    if( g_pVertexShader ) g_pVertexShader->Release();
    if( g_pPixelShader ) g_pPixelShader->Release();
    if( g_pRenderTargetView ) g_pRenderTargetView->Release();
    if( g_pSwapChain ) g_pSwapChain->Release();
    if( g_pImmediateContext ) g_pImmediateContext->Release();
    if( g_pd3dDevice ) g_pd3dDevice->Release();
    if( g_hMutex ) CloseHandle(g_hMutex);

}

//--------------------------------------------------------------------------------------
// ��������� �����
//--------------------------------------------------------------------------------------
void Render()
{
    // �������� ������ �����
    float ClearColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f }; // �������, �������, �����, �����-�����
    g_pImmediateContext->ClearRenderTargetView( g_pRenderTargetView, ClearColor);
    
	// ���������� � ���������� ��������� �������
	g_pImmediateContext->VSSetShader( g_pVertexShader, NULL, 0 );
	g_pImmediateContext->PSSetShader( g_pPixelShader, NULL, 0 );

    // ���������� � ���������� ShaderResources
    g_pImmediateContext->PSSetShaderResources(0, 1, &m_pTextureRV);
    
    g_pImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinear);
    
    //������ � ������ ViewPort �� �����������, ������ ViewPort ������� ��������.
    for (int i(0); i < m_vectViewPort.size(); i++)
    {

        g_pImmediateContext->RSSetViewports(1u, &m_vectViewPort[i]);

        g_pImmediateContext->Draw(4, 0);

    }

    // ������� � �������� ����� (�� �����) ����������, ������������ � ������ ������.
    g_pSwapChain->Present( 0, 0 );
}

//--------------------------------------------------------------------------------------
// ������������� �����
//--------------------------------------------------------------------------------------
HRESULT InitMFfromUrlVideo()
{
    HRESULT hr = S_OK;

    //������� ���������� D3D ��� �� ���������� �������������� ������ MF, ����� �������� undefined behavior
    const CComQIPtr<ID3D10Multithread> pMultithread = g_pd3dDevice;
    pMultithread->SetMultithreadProtected(TRUE);

    //�������� ��������� ��������� MF
    UINT token;
    CComPtr<IMFDXGIDeviceManager> manager;
    hr = MFCreateDXGIDeviceManager(&token, &manager);

    //����������� ���� D3D ���������� � ��������� ���������
    hr = manager->ResetDevice(g_pd3dDevice, token);

    //������� �������� SourceReader
    CComPtr<IMFAttributes> attributes;
    hr = MFCreateAttributes(&attributes, 3);
    hr = attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, manager);  //��������� ��������
    //hr = attributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, FALSE);
    hr = attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    hr = attributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);

    //������� SourceReader � ��������� �����, ������� h264 �������
    if (FAILED(MFCreateSourceReaderFromURL(szFileName, attributes, &g_pReader)))
    {
        MessageBoxW(NULL, L"���������� ������� ����, ����������� ������ ��� ���� �� �����.", L"������", MB_OK);
        return E_INVALIDARG;
    }


    //����������� �������� �������� �������� � SourceReader
    CComPtr<IMFMediaType> output_type;
    hr = MFCreateMediaType(&output_type);
    hr = output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    hr = output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    //hr = attributes->SetUINT32(MF_MT_AVG_BITRATE, 30000000);
    //hr = MFSetAttributeRatio(attributes, MF_MT_FRAME_RATE, 60, 1);

    hr = g_pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, output_type);

    //������� ����� ����������
    hr = g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pVideoDevice));

    if (FAILED(hr))
    {
        return hr;
    }

    // �������� ��������
    D3D11_TEXTURE2D_DESC desc = {};
    ZeroMemory(&desc, sizeof(desc));

    desc.Width = 1920;
    desc.Height = 1080;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    desc.MiscFlags = D3D11_USAGE_DEFAULT;

    hr = g_pd3dDevice->CreateTexture2D(&desc, nullptr, &pD3DVideoTexture);

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC dc = {};
    ZeroMemory(&dc, sizeof(dc));

    dc.DecodeProfile = D3D11_DECODER_PROFILE_H264_VLD_NOFGT;
    dc.Texture2D.ArraySlice = 0;
    dc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;

    if (FAILED(hr))
    {
        return hr;
    }

    //� ��������� D3D View ��������
    pVideoDevice->CreateVideoDecoderOutputView((ID3D11Resource*)pD3DVideoTexture, &dc, &pVideoDecoderOutputView);
    hr = g_pImmediateContext->QueryInterface(IID_PPV_ARGS(&pVideoContext));

    return hr;
}

//--------------------------------------------------------------------------------------
// ���������� �����
//--------------------------------------------------------------------------------------
HRESULT NextFrame()
{
    HRESULT hr = S_OK;

    CComPtr<IMFSample> pSample;
    hr = g_pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, 0, &flag, 0, &pSample);
    if (FAILED(hr))
    {
        return hr;
    }

    if (flag & MF_SOURCE_READERF_ENDOFSTREAM || pSample == nullptr)
    {
        if(FAILED(ChangeCurrentPositionVideo()))
            SendMessage(g_hWnd, WM_DESTROY, NULL, NULL);

        return S_FALSE;
    }
       
    hr = pSample->GetSampleDuration(&dwDuration);

    if (dwDuration)
    {
        dwDuration = dwDuration / 10000;
    }

    CComPtr<IMFMediaBuffer> pBuffer;
    hr = pSample->GetBufferByIndex(0, &pBuffer);

    if (FAILED(hr))
    {
        return hr;
    }

    CComPtr<IMFDXGIBuffer> pDXGIBuffer;
    hr = pBuffer->QueryInterface(IID_PPV_ARGS(&pDXGIBuffer));
    if (FAILED(hr))
    {
        return hr;
    }

    CComPtr<ID3D11Texture2D> pTexture2D;
    hr = pDXGIBuffer->GetResource(IID_PPV_ARGS(&pTexture2D));
    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_TEXTURE2D_DESC desc_2;
    pTexture2D->GetDesc(&desc_2);

    static bool ef = { false };

    if(ef == false) 
    {
    
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd;
        ZeroMemory(&cd, sizeof(cd));

        cd.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
        cd.InputWidth = desc_2.Width;
        cd.InputHeight = desc_2.Height;
        cd.OutputWidth = desc_2.Width;
        cd.OutputHeight = desc_2.Height;
        cd.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

        hr = pVideoDevice->CreateVideoProcessorEnumerator(&cd, &pVideoEnum);
        hr = pVideoEnum->CheckVideoProcessorFormat(DXGI_FORMAT_B8G8R8A8_UNORM, &uiFlags);
        hr = pVideoDevice->CreateVideoProcessor(pVideoEnum, 0, &pVideoProcessor);

        if (FAILED(hr))
        {
            return hr;
        }

        ef = true;
    }

    
    if (pSample != nullptr)pSample.Release();
    if (pBuffer != nullptr)pBuffer.Release();
    if(pOutputView != nullptr) pOutputView->Release();
    if (pInputView != nullptr)pInputView->Release();
    if (m_pTextureRV != nullptr)m_pTextureRV->Release();


    hr = MFCreateSample(&pSample);

    if (FAILED(hr))
    {
        return hr;
    }

    hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), pD3DVideoTexture, 0, FALSE, &pBuffer);
    hr = pSample->AddBuffer(pBuffer);

    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
    ZeroMemory(&OutputViewDesc, sizeof(OutputViewDesc));

    OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    OutputViewDesc.Texture2D.MipSlice = 0;
    OutputViewDesc.Texture2DArray.MipSlice = 0;
    OutputViewDesc.Texture2DArray.FirstArraySlice = 0;

    hr = pVideoDevice->CreateVideoProcessorOutputView(pD3DVideoTexture, pVideoEnum, &OutputViewDesc, &pOutputView);

    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC ivd;
    ZeroMemory(&ivd, sizeof(ivd));

    ivd.FourCC = 0;
    ivd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    ivd.Texture2D.MipSlice = 0;
    ivd.Texture2D.ArraySlice = 0;

    hr = pVideoDevice->CreateVideoProcessorInputView(pTexture2D, pVideoEnum, &ivd, &pInputView);

    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_VIDEO_PROCESSOR_STREAM sd;
    ZeroMemory(&sd, sizeof(sd));


    sd.Enable = TRUE;
    sd.OutputIndex = 0;
    sd.InputFrameOrField = 0;
    sd.PastFrames = 0;
    sd.FutureFrames = 0;
    sd.ppPastSurfaces = NULL;
    sd.ppFutureSurfaces = NULL;
    sd.pInputSurface = pInputView;
    sd.ppPastSurfacesRight = NULL;
    sd.ppFutureSurfacesRight = NULL;

    hr = pVideoContext->VideoProcessorBlt(pVideoProcessor, pOutputView, 0, 1, &sd);

    if (FAILED(hr))
    {
        return hr;
    }

    hr = g_pd3dDevice->CreateShaderResourceView(pD3DVideoTexture, nullptr, &m_pTextureRV);


    if (pTexture2D != nullptr) pTexture2D.Release();

    return hr;

}

//--------------------------------------------------------------------------------------
// ������������ ���� ��������� �������� Windows Media Foundation
//--------------------------------------------------------------------------------------
void ClearMFC()
{
    

    if (g_pReader) g_pReader->Release();
    if (pVideoProcessor) pVideoProcessor->Release();
    if (pD3DVideoTexture) pD3DVideoTexture->Release();
    if (pVideoDecoderOutputView) pVideoDecoderOutputView->Release();
    if (pVideoContext) pVideoContext->Release();
    if (pVideoDevice) pVideoDevice->Release();
    if (pVideoEnum) pVideoEnum->Release();
   
    return;
}

//--------------------------------------------------------------------------------------
// ������������� ���������
//--------------------------------------------------------------------------------------
BOOL InitMonitorsInfo()
{
    m_vectAllMonitors.clear();
    m_vectInfoMonitors.clear();
    m_vectViewPort.clear();

    EnumDisplayMonitors(
        NULL,
        NULL,
        EnumMonitorsProc,
        NULL);

    if (m_vectAllMonitors.empty())
    {
        return FALSE;
    }

    for (int i(0); i < m_vectAllMonitors.size(); i++)
    {
        MONITORINFO monic1;

        ZeroMemory(&monic1, sizeof(monic1));
        monic1.cbSize = sizeof(monic1);

        GetMonitorInfo(m_vectAllMonitors[i], &monic1);

        m_vectInfoMonitors.push_back(monic1);
    }

    for (int i(0); i < m_vectInfoMonitors.size(); i++)
    {
        D3D11_VIEWPORT vp;
        ZeroMemory(&vp, sizeof(vp));

        vp.Width = m_vectInfoMonitors[i].rcWork.right - m_vectInfoMonitors[i].rcWork.left;
        vp.Height = m_vectInfoMonitors[i].rcWork.bottom - m_vectInfoMonitors[i].rcWork.top;
        vp.MinDepth = 0;
        vp.MaxDepth = 1;

        POINT p = {};
        p.x = m_vectInfoMonitors[i].rcWork.left;
        p.y = m_vectInfoMonitors[i].rcWork.top;
        ScreenToClient(g_hDesktopWnd, &p);

        vp.TopLeftX = p.x;
        vp.TopLeftY = p.y;

        m_vectViewPort.push_back(vp);
    }

    return TRUE;
}

//--------------------------------------------------------------------------------------
// �������� ����������� DLL � ������� Explorer(���������) 
//--------------------------------------------------------------------------------------
HRESULT InitDllToExplorer()
{
    GetWindowThreadProcessId(g_hDesktopWnd, &pID);

#ifdef DEBUG
    cout << "pID = " << pID << endl;
#endif // DEBUG

    if(GetFullPathNameA(dll_name, sizeof(path), path, NULL) == NULL)
    {
        MessageBoxW(NULL, L"���������� ������� ���� �� ������� ������������.", L"������", MB_ERR_INVALID_CHARS);
        return E_INVALIDARG;
    }

    if (!Inject(pID, path))
    {
        return E_INVALIDARG;
    }

    HINSTANCE hLib = LoadLibraryA(path);

#ifdef DEBUG
    cout << "hLib: " << hLib << endl;
#endif 

    if (hLib)
    {
        proc proc_addr = (proc)GetProcAddress(hLib, "set");

        if (proc_addr)
        {
            proc_addr(g_hWnd);
        }

        //��������� ����������
        FreeLibrary(hLib);
    }
    else
    {
        return E_INVALIDARG;
    }

    CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(CheckSatusExplorer), NULL, NULL, NULL);

    return S_OK;
}

//----------------------------------------------------------------------------------------
// ������� ������� ������� ��� ������������� ������ ����� ����� ����������, � ��� ��������
// DLL �� �������� Explorer, ����� �������� ���������� �����������.
//----------------------------------------------------------------------------------------
BOOL CreateMutexForDll()
{
    //������� �������
    g_hMutex = CreateMutex(NULL, FALSE, L"WallpaperAnimationWin10");

    if (g_hMutex == NULL)
    {
        return FALSE;
    }
    else
    {
        //���� �� 20 ���������� �������� ������ � ��������, ������ ������� ����������, �� ���������� �� �������.
        DWORD err = WaitForSingleObject(g_hMutex, 20);

        if (GetLastError() == ERROR_ALREADY_EXISTS && (err == WAIT_TIMEOUT))
        {

            // ����� ����� �� ���������� ��� ������� � ����������� ���� ���� ��� �� �����, ����������� ����������� ������ ����
#ifdef DEBUG
            cout << "Mutex already exist, app is running..." << endl;
#endif // DEBUG

            return FALSE;
        }

    }

    //������� ������� �� ��� ����� ������ ����������.
    WaitForSingleObject(g_hMutex, INFINITE);

    return TRUE;
}

//--------------------------------------------------------------------------------------
// ��������� �� �������������� �� ������� Explorer, ���� �� �� ��������� ����������.
//--------------------------------------------------------------------------------------
void CheckSatusExplorer()
{
    // ������ 2 ������� ��������� �� �������������� �� Explorer,
    // ���� ��, �� ��������� ����������, ���������� ���� ��� ��������� 
    // ����������, ��� ���������� ����������� � ������ ��� �� �������,
    // ��������� ���������� ����������.

    for (;; Sleep(2000))
    {
        if (!(isProcessRun((WCHAR*)L"explorer.exe"))) break;
    }

    SendMessage(g_hWnd, WM_DESTROY, NULL, NULL);

    return;
}

//--------------------------------------------------------------------------------------
// ����������� SwapChain, ������ ����� SwapChain.
//--------------------------------------------------------------------------------------
HRESULT ResetSwapChain(DWORD width, DWORD height)
{
    HRESULT hr_t = S_OK;

    g_pRenderTargetView->Release();

    // ��� ��������� ���������� ������(��), ����� ����������� SwapChain � ��� ����� �������������,
    // ��� ��� ����� ��� ��������� ������ ����������.
    if (FAILED(g_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, NULL))) 
        return hr_t;

    ID3D11Texture2D* pBackBuffer = NULL;
    hr_t = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr_t))	return hr_t;

    // �� ����������� �������� ������� ����������� ���������
    hr_t = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr_t))	return hr_t;

    // ���������� ������ ������� ������ � ��������� ����������
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

    return hr_t;
}
