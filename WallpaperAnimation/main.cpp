//-------------------------------------------------------------------------------------------------
// Создание анимированных обоев на рабочий стол при помощи DX SDK и Windows Media Foundation
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
// Структуры
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT2 Tex;
 };

//--------------------------------------------------------------------------------------
// Глобальные переменные
//--------------------------------------------------------------------------------------
HINSTANCE                 g_hInst = NULL;           // Описание окна приложения
HWND                      g_hDesktopWnd = NULL;     // Дескриптор окна в которое мы рендерим
HWND                      g_hWnd = NULL;            // Собственный дескриптор приложения
HANDLE                    g_hMutex = NULL;          // Дескриптор мьютекса

D3D_DRIVER_TYPE           g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL         g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*             g_pd3dDevice = NULL;		    // Устройство (для создания объектов)
ID3D11DeviceContext*      g_pImmediateContext = NULL;	// Контекст устройства (рисование)
IDXGISwapChain*           g_pSwapChain = NULL;		    // Цепь связи (буфера с экраном)
ID3D11RenderTargetView*   g_pRenderTargetView = NULL;	// Объект заднего буфера
ID3D11VertexShader*       g_pVertexShader = NULL;		// Вершинный шейдер
ID3D11PixelShader*        g_pPixelShader = NULL;		// Пиксельный шейдер
ID3D11InputLayout*        g_pVertexLayout = NULL;		// Описание формата вершин
ID3D11Buffer*             g_pVertexBuffer = NULL;		// Буфер вершин
ID3D11ShaderResourceView* m_pTextureRV = NULL;
ID3D11SamplerState*       m_pSamplerLinear = NULL;

DWORD    flag         = NULL;
DWORD    pID          = NULL;
LONGLONG dwDuration   = NULL;
UINT     uiFlags      = NULL;
WCHAR    szFileName   [MAX_PATH];
char     path[256];

//--------------------------------------------------------------------------------------
// Глобальные переменные для Media Foundation
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
// Предварительные объявления функций
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );  // Создание окна
HRESULT InitDevice();	// Инициализация устройств DirectX
HRESULT InitGeometry();	// Инициализация шаблона ввода и буфера вершин
HRESULT InitDllToExplorer();
HRESULT ResetSwapChain(DWORD width, DWORD height);
void CleanupDevice();	// Удаление созданнных устройств DirectX
void Render();          // Функция рисования
HRESULT NextFrame();
HRESULT InitMFfromUrlVideo();
HRESULT CreateMediaSource(IMFMediaSource** ppSource);
BOOL InitMonitorsInfo();
BOOL CreateMutexForDll();
void ClearMFC();
void CheckSatusExplorer();

LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

BOOL CALLBACK EnumMonitorsProc(HMONITOR hMonitor, HDC, LPRECT rect, LPARAM lParam); // Функция окна

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
// Точка входа в программу. Инициализация всех объектов и вход в цикл сообщений.
// Свободное время используется для отрисовки сцены.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    MFStartup(MF_VERSION);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        

#ifdef DEBUG

    //Выделяем консоль 
    AllocConsole();
    FILE* pCout;
    freopen_s(&pCout, "conout$", "w", stdout);

#endif // DEBUG

    //Создание именновоного мьютекса
    if (!CreateMutexForDll())
        return 0;

    //Поиск окна, в которое будет происходить рендер
    if (FAILED(FindWindowDestopWorker()))
        return 0;

    // Создание окна приложения
    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

    //Загрузка DLL в память процесса Explorer
    if (FAILED(InitDllToExplorer()))
        return 0;

    //Инициализация мониторов и ViewPorts
    if (!InitMonitorsInfo())
        return 0;

	// Создание объектов DirectX
    if( FAILED( InitDevice() ) )
    {
        CleanupDevice();
        return 0;
    }

	// Создание шейдеров и буфера вершин
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

    //Инициализация Media Foundation
    if (FAILED(InitMFfromUrlVideo()))
    {
        CleanupDevice();
        return 0;
    }

    // Главный цикл сообщений
    MSG msg = {0};
    while( WM_QUIT != msg.message )
    {
        if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else	// Если сообщений нет
        {
            NextFrame();       // Подготовка кадра
            Render();	       // Рисуем
            Sleep(dwDuration); // Усыпляем поток на время задержки кадра
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
// Регистрация класса и создание окна
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Регистрация класса
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

    // Создание окна
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
// Вызывается каждый раз, когда приложение получает системное сообщение
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

            // Пересоздание SwapChain и его буфер.
            if (FAILED(ResetSwapChain(DWORD(wParam), DWORD(lParam))))
            {
                MessageBoxW(NULL, L"Невозможно пересоздать SwapChain.", L"Ошибка", MB_OK);
                PostQuitMessage(0);
                break;
            }

            // Получаем новую информацию о разрешении мониторов,
            // и создаем новые ViewPorts.
            InitMonitorsInfo();

            break;
        }

        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Вспомогательная функция для компиляции шейдеров в D3DX11
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
// Создание устройства Direct3D (D3D Device), связующей цепи (Swap Chain) и
// контекста устройства (Immediate Context).
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_hDesktopWnd, &rc );

    UINT width = rc.right - rc.left;	// получаем ширину
    UINT height = rc.bottom - rc.top;	// и высоту окна

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

    // Тут мы создаем список поддерживаемых версий DirectX
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
	UINT numFeatureLevels = ARRAYSIZE( featureLevels );

	// Сейчас мы создадим устройства DirectX. Для начала заполним структуру,
	// которая описывает свойства переднего буфера и привязывает его к нашему окну.
    DXGI_SWAP_CHAIN_DESC sd;			// Структура, описывающая цепь связи (Swap Chain)
    ZeroMemory( &sd, sizeof( sd ) );	// очищаем ее
	sd.BufferCount = 1;					// у нас один буфер
    sd.BufferDesc.Width = width;		// ширина буфера
    sd.BufferDesc.Height = height;		// высота буфера
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	// формат пикселя в буфере
    sd.BufferDesc.RefreshRate.Numerator = 75;			// частота обновления экрана
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// назначение буфера - задний буфер
    sd.OutputWindow = g_hDesktopWnd;							// привязываем к нашему окну
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;						// не полноэкранный режим

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain( NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
                                            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
        if (SUCCEEDED(hr))  // Если устройства созданы успешно, то выходим из цикла
            break;
    }
    if (FAILED(hr))
        return hr;

    // Теперь создаем задний буфер. Обратите внимание, в SDK
    // RenderTargetOutput - это передний буфер, а RenderTargetView - задний.

	// Извлекаем описание заднего буфера
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&pBackBuffer );
    if (FAILED(hr))	return hr;

	// По полученному описанию создаем поверхность рисования
    hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &g_pRenderTargetView );
    pBackBuffer->Release();
    if (FAILED(hr))	return hr;

    // Подключаем объект заднего буфера к контексту устройства
    g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Создание буфера вершин, шейдеров (shaders) и описания формата вершин (input layout)
//--------------------------------------------------------------------------------------
HRESULT InitGeometry()
{
	HRESULT hr = S_OK;

	// Компиляция вершинного шейдера из файла
    ID3DBlob* pVSBlob = NULL; // Вспомогательный объект - просто место в оперативной памяти
    hr = CompileShaderFromFile(urok_fx, "VS", "vs_4_0", &pVSBlob );
    if (FAILED(hr))
    {
        MessageBoxW( NULL, L"Невозможно скомпилировать файл FX. Пожалуйста, запустите данную программу из папки, содержащей файл FX.", L"Ошибка", MB_OK);
        return hr;
    }

	// Создание вершинного шейдера
	hr = g_pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader );
	if( FAILED( hr ) )
	{	
		pVSBlob->Release();
        return hr;
	}

    // Определение шаблона вершин
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		/* семантическое имя, семантический индекс, размер, входящий слот (0-15), адрес начала данных
		   в буфере вершин, класс входящего слота (не важно), InstanceDataStepRate (не важно) */
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
	UINT numElements = ARRAYSIZE( layout );

    // Создание шаблона вершин
	hr = g_pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
                                          pVSBlob->GetBufferSize(), &g_pVertexLayout );
	pVSBlob->Release();
	if (FAILED(hr)) return hr;

    // Подключение шаблона вершин
    g_pImmediateContext->IASetInputLayout( g_pVertexLayout );

	// Компиляция пиксельного шейдера из файла
	ID3DBlob* pPSBlob = NULL;
    hr = CompileShaderFromFile(urok_fx, "PS", "ps_4_0", &pPSBlob );
    if( FAILED( hr ) )
    {
        MessageBoxW( NULL, L"Невозможно скомпилировать файл FX. Пожалуйста, запустите данную программу из папки, содержащей файл FX.", L"Ошибка", MB_OK );
        return hr;
    }

	// Создание пиксельного шейдера
	hr = g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader );
	pPSBlob->Release();
	if (FAILED(hr)) return hr;

    // Создание буфера вершин (четыре вершины треугольника)
    SimpleVertex vertices[] = {
        { DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
        { DirectX::XMFLOAT3(1.0f,  1.0f,  1.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
        { DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }
	};

	D3D11_BUFFER_DESC bd;	// Структура, описывающая создаваемый буфер
	ZeroMemory( &bd, sizeof(bd) );				// очищаем ее
    bd.Usage = D3D11_USAGE_DEFAULT;	
    bd.ByteWidth = sizeof( SimpleVertex ) * 4;	// размер буфера
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// тип буфера - буфер вершин
	bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData; // Структура, содержащая данные буфера
	ZeroMemory( &InitData, sizeof(InitData) );	// очищаем ее
    InitData.pSysMem = vertices;				// указатель на наши 4 вершины
	// Вызов метода g_pd3dDevice создаст объект буфера вершин
    hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pVertexBuffer );
	if (FAILED(hr)) return hr;

    // Установка буфера вершин
    UINT stride = sizeof( SimpleVertex );
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers( 0, 1, &g_pVertexBuffer, &stride, &offset );

    // Установка способа отрисовки вершин в буфере (в данном случае - TRIANGLE LIST,
	// т. е. точки 1-3 - первый треугольник, 4-6 - второй и т. д. Другой способ - TRIANGLE STRIP.
	// В этом случае точки 1-3 - первый треугольник, 2-4 - второй, 3-5 - третий и т. д.
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
// Освобождение всех созданных объектов
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    // Сначала отключим контекст устройства
    if( g_pImmediateContext ) g_pImmediateContext->ClearState();
	// Потом удалим объекты
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
// Рисование кадра
//--------------------------------------------------------------------------------------
void Render()
{
    // Очистить задний буфер
    float ClearColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f }; // красный, зеленый, синий, альфа-канал
    g_pImmediateContext->ClearRenderTargetView( g_pRenderTargetView, ClearColor);
    
	// Подключить к устройству рисования шейдеры
	g_pImmediateContext->VSSetShader( g_pVertexShader, NULL, 0 );
	g_pImmediateContext->PSSetShader( g_pPixelShader, NULL, 0 );

    // Подключить к устройству ShaderResources
    g_pImmediateContext->PSSetShaderResources(0, 1, &m_pTextureRV);
    
    g_pImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinear);
    
    //Рисуем в каждый ViewPort по отдельности, каждый ViewPort каждому монитору.
    for (int i(0); i < m_vectViewPort.size(); i++)
    {

        g_pImmediateContext->RSSetViewports(1u, &m_vectViewPort[i]);

        g_pImmediateContext->Draw(4, 0);

    }

    // Вывести в передний буфер (на экран) информацию, нарисованную в заднем буфере.
    g_pSwapChain->Present( 0, 0 );
}

//--------------------------------------------------------------------------------------
// Инициализация видео
//--------------------------------------------------------------------------------------
HRESULT InitMFfromUrlVideo()
{
    HRESULT hr = S_OK;

    //Говорим библиотеке D3D что мы используем мультипоточные вызовы MF, чтобы избежать undefined behavior
    const CComQIPtr<ID3D10Multithread> pMultithread = g_pd3dDevice;
    pMultithread->SetMultithreadProtected(TRUE);

    //Создание Менеджера Устройств MF
    UINT token;
    CComPtr<IMFDXGIDeviceManager> manager;
    hr = MFCreateDXGIDeviceManager(&token, &manager);

    //Привязываем наше D3D Устройство к Менеджеру Устройств
    hr = manager->ResetDevice(g_pd3dDevice, token);

    //Создаем свойства SourceReader
    CComPtr<IMFAttributes> attributes;
    hr = MFCreateAttributes(&attributes, 3);
    hr = attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, manager);  //указываем менеджер
    //hr = attributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, FALSE);
    hr = attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    hr = attributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);

    //Создаем SourceReader и загружаем видео, зажатое h264 кодеком
    if (FAILED(MFCreateSourceReaderFromURL(szFileName, attributes, &g_pReader)))
    {
        MessageBoxW(NULL, L"Невозможно открыть файл, некоректный формат или путь до файла.", L"Ошибка", MB_OK);
        return E_INVALIDARG;
    }


    //Настраиваем выходной медиатип декодера в SourceReader
    CComPtr<IMFMediaType> output_type;
    hr = MFCreateMediaType(&output_type);
    hr = output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    hr = output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    //hr = attributes->SetUINT32(MF_MT_AVG_BITRATE, 30000000);
    //hr = MFSetAttributeRatio(attributes, MF_MT_FRAME_RATE, 60, 1);

    hr = g_pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, output_type);

    //Создаем Видео Устройство
    hr = g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pVideoDevice));

    if (FAILED(hr))
    {
        return hr;
    }

    // Создание текстуры
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

    //И описываем D3D View текстуры
    pVideoDevice->CreateVideoDecoderOutputView((ID3D11Resource*)pD3DVideoTexture, &dc, &pVideoDecoderOutputView);
    hr = g_pImmediateContext->QueryInterface(IID_PPV_ARGS(&pVideoContext));

    return hr;
}

//--------------------------------------------------------------------------------------
// Подготовка кадра
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
// Освобождение всех созданных объектов Windows Media Foundation
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
// Инициализация мониторов
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
// Загрузка собственной DLL в процесс Explorer(Проводник) 
//--------------------------------------------------------------------------------------
HRESULT InitDllToExplorer()
{
    GetWindowThreadProcessId(g_hDesktopWnd, &pID);

#ifdef DEBUG
    cout << "pID = " << pID << endl;
#endif // DEBUG

    if(GetFullPathNameA(dll_name, sizeof(path), path, NULL) == NULL)
    {
        MessageBoxW(NULL, L"Невозможно открыть файл по данному расположению.", L"Ошибка", MB_ERR_INVALID_CHARS);
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

        //Выгружаем библиотеку
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
// Создаем именной мьютекс для существования только одной копии приложения, и для выгрузки
// DLL из процесса Explorer, когда основное приложение закрывается.
//----------------------------------------------------------------------------------------
BOOL CreateMutexForDll()
{
    //Создаем мьютекс
    g_hMutex = CreateMutex(NULL, FALSE, L"WallpaperAnimationWin10");

    if (g_hMutex == NULL)
    {
        return FALSE;
    }
    else
    {
        //Если за 20 милисекунд получили доступ к мьютексу, значит мьютекс существует, но приложение не открыто.
        DWORD err = WaitForSingleObject(g_hMutex, 20);

        if (GetLastError() == ERROR_ALREADY_EXISTS && (err == WAIT_TIMEOUT))
        {

            // Иначе такое же приложение уже открыто и дублировать само себя оно не будет, воизбежание перерисовки самого себя
#ifdef DEBUG
            cout << "Mutex already exist, app is running..." << endl;
#endif // DEBUG

            return FALSE;
        }

    }

    //Занимем мьютекс на все время работы приложения.
    WaitForSingleObject(g_hMutex, INFINITE);

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Проверяем не перезапустился ли процесс Explorer, если да то закрываем приложение.
//--------------------------------------------------------------------------------------
void CheckSatusExplorer()
{
    // Каждые 2 секунды проверяем не перезапустился ли Explorer,
    // если да, то закрываем приложение, вследствии того что произошел 
    // перезапуск, все полученные дескрипторы и хэндлы уже не валидны,
    // требуется перезапуск приложения.

    for (;; Sleep(2000))
    {
        if (!(isProcessRun((WCHAR*)L"explorer.exe"))) break;
    }

    SendMessage(g_hWnd, WM_DESTROY, NULL, NULL);

    return;
}

//--------------------------------------------------------------------------------------
// Пересоздаем SwapChain, задний буфер SwapChain.
//--------------------------------------------------------------------------------------
HRESULT ResetSwapChain(DWORD width, DWORD height)
{
    HRESULT hr_t = S_OK;

    g_pRenderTargetView->Release();

    // При изменение разрешения экрана(ов), нужно пересоздать SwapChain и его буфер соответсвенно,
    // все это нужно для коректной работы приложения.
    if (FAILED(g_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, NULL))) 
        return hr_t;

    ID3D11Texture2D* pBackBuffer = NULL;
    hr_t = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr_t))	return hr_t;

    // По полученному описанию создаем поверхность рисования
    hr_t = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr_t))	return hr_t;

    // Подключаем объект заднего буфера к контексту устройства
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

    return hr_t;
}
