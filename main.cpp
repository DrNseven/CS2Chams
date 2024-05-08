//D3D11 cs2 base
//compile in release mode

#pragma once
#include <Windows.h>
#include <vector>
#include <d3d11.h>
#include <dxgi.h>
#include <D3Dcompiler.h> //generateshader
#pragma comment(lib, "D3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment( lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib") //timeGetTime
#include "MinHook/include/MinHook.h" //detour x86&x64

//imgui
#include "ImGui\imgui.h"
#include "imgui\imgui_impl_win32.h"
#include "ImGui\imgui_impl_dx11.h"

//DX Includes
#include <DirectXMath.h>
using namespace DirectX;

#pragma warning( disable : 4244 )


typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(__stdcall *D3D11ResizeBuffersHook) (IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

typedef void(__stdcall *D3D11DrawIndexedHook) (ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
typedef void(__stdcall *D3D11DrawIndexedInstancedHook) (ID3D11DeviceContext* pContext, UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation);
typedef void(__stdcall *D3D11DrawHook) (ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartVertexLocation);


D3D11PresentHook phookD3D11Present = NULL;
D3D11ResizeBuffersHook phookD3D11ResizeBuffers = NULL;

D3D11DrawIndexedHook phookD3D11DrawIndexed = NULL;
D3D11DrawIndexedInstancedHook phookD3D11DrawIndexedInstanced = NULL;
D3D11DrawHook phookD3D11Draw = NULL;


ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;

DWORD_PTR* pSwapChainVtable = NULL;
DWORD_PTR* pContextVTable = NULL;
DWORD_PTR* pDeviceVTable = NULL;


#include "main.h" //helper funcs

//==========================================================================================================================

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC OWndProc = nullptr; //Pointer of the original window message handler

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (showmenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
	{
		ImGui::GetIO().MouseDrawCursor = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
		return true;
	}

	if (bInit)
		ImGui::GetIO().MouseDrawCursor = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);

	if (uMsg == WM_CLOSE)
	{
		//Unhook dx
		SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)OWndProc); //UnHookWindow
		TerminateProcess(GetCurrentProcess(), 0);
	}

	if (ImGui::GetIO().WantCaptureMouse)
	{
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
			return true;
		return false;
	}
	return CallWindowProc(OWndProc, hWnd, uMsg, wParam, lParam);
}

void InitImGuiD3D11()
{
	OWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);//HookWindow
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr;
	io.Fonts->AddFontDefault();

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(window);


	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(pDevice, pContext);

	bInit = TRUE;
}

//==========================================================================================================================

HRESULT __stdcall hookD3D11ResizeBuffers(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	ImGui_ImplDX11_InvalidateDeviceObjects();
	if (nullptr != mainRenderTargetViewD3D11) { mainRenderTargetViewD3D11->Release(); mainRenderTargetViewD3D11 = nullptr; }

	HRESULT toReturn = phookD3D11ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

	ImGui_ImplDX11_CreateDeviceObjects();

	return phookD3D11ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

//==========================================================================================================================

HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!initonce)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice)))
		{
			pDevice->GetImmediateContext(&pContext);
			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetViewD3D11);
			pBackBuffer->Release();
			//oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			InitImGuiD3D11();

			// Create Rasterize state
			D3D11_RASTERIZER_DESC Desc;
			Desc.AntialiasedLineEnable = 0;
			Desc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
			Desc.DepthBiasClamp = 0.f;
			Desc.ScissorEnable = FALSE;
			Desc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
			Desc.DepthBias = LONG_MIN; //-0x7FFFFFFF; //LONG_MAX; //INT_MAX;
			Desc.DepthClipEnable = TRUE;
			Desc.FrontCounterClockwise = FALSE;
			pDevice->CreateRasterizerState(&Desc, &CustomState);

			// Create depthstencil state
			D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
			depthStencilDesc.DepthEnable = FALSE; //TRUE;
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS; //D3D11_COMPARISON_ALWAYS;
			depthStencilDesc.StencilEnable = FALSE;
			depthStencilDesc.StencilReadMask = 0xFF;
			depthStencilDesc.StencilWriteMask = 0xFF;
			pDevice->CreateDepthStencilState(&depthStencilDesc, &DepthStencilState_FALSE);

			//load cfg settings
			LoadCfg();

			initonce = true;
		}
		else
			return phookD3D11Present(pSwapChain, SyncInterval, Flags);
	}

	//create shaders
	if (!sRed)
		GenerateShader(pDevice, &sRed, 1.0f, 0.0f, 0.0f); 

	if (!sRedDark)
		GenerateShader(pDevice, &sRedDark, 0.2f, 0.0f, 0.0f);

	if (!sGreen)
		GenerateShader(pDevice, &sGreen, 0.0f, 1.0f, 0.0f); 

	if (!sGreenDark)
		GenerateShader(pDevice, &sGreenDark, 0.0f, 0.2f, 0.0f);

	if (!sBlue)
		GenerateShader(pDevice, &sBlue, 0.0f, 0.0f, 1.0f); 

	if (!sYellow)
		GenerateShader(pDevice, &sYellow, 1.0f, 1.0f, 0.0f); 

	if (!sMagenta)
		GenerateShader(pDevice, &sMagenta, 1.0f, 0.0f, 1.0f); 

	if (!sGrey)
		GenerateShader(pDevice, &sGrey, 0.3f, 0.3f, 0.3f);

	//recreate rendertarget on reset
	if (mainRenderTargetViewD3D11 == NULL)
	{
		ID3D11Texture2D* pBackBuffer = NULL;
		pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
		pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetViewD3D11);
		pBackBuffer->Release();
	}

	//get imgui displaysize
	//const ImVec2 size = ImGui::GetIO().DisplaySize; //size.x size.y

	if (GetAsyncKeyState(VK_INSERT) & 1) {
		SaveCfg(); //save settings
		showmenu = !showmenu;
	}


	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (showmenu) 
	{

		ImGui::Begin("Hack Menu", &showmenu);
		//ImGui::Text("DisplaySize = %f,%f", io.DisplaySize.x, io.DisplaySize.y);		
		//ImGui::SetWindowSize({ 500, 500 }, ImGuiCond_Once);

		if (vWindowPos.x != 0.0f && vWindowPos.y != 0.0f && vWindowSize.x != 0.0f && vWindowSize.y != 0.0f && bSetPos)
		{
			ImGui::SetWindowPos(vWindowPos);
			ImGui::SetWindowSize(vWindowSize);
			bSetPos = false;
		}

		if (bSetPos == false)
		{
			vWindowPos = { ImGui::GetWindowPos().x, ImGui::GetWindowPos().y };
			vWindowSize = { ImGui::GetWindowSize().x, ImGui::GetWindowSize().y };
		}

		ImGui::Checkbox("Wallhack", &wallhack);
		ImGui::Checkbox("Chams", &chams);//universal
		ImGui::Checkbox("TeamChams", &teamchams);

		//ImGui::NewLine();
		//if (check_draw_result == 1)ImGui::Text("Draw called."); ImGui::SameLine();
		//if (check_drawindexed_result == 1)ImGui::Text("DrawIndexed called."); ImGui::SameLine();
		//if (check_drawindexedinstanced_result == 1)ImGui::Text("DrawIndexedInstanced called.");

		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();
	pContext->OMSetRenderTargets(1, &mainRenderTargetViewD3D11, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return phookD3D11Present(pSwapChain, SyncInterval, Flags);
}
//==========================================================================================================================

void __stdcall hookD3D11DrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	if (IndexCount > 0)
		check_drawindexed_result = 1;

    return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11DrawIndexedInstanced(ID3D11DeviceContext* pContext, UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	if (IndexCount > 0)
		check_drawindexedinstanced_result = 1;

	//get vertexbuffer
	ID3D11Buffer* veBuffer;
	UINT veWidth, Stride, veBufferOffset;
	D3D11_BUFFER_DESC veDesc;
	pContext->IAGetVertexBuffers(0, 1, &veBuffer, &Stride, &veBufferOffset);
	if (veBuffer) {
		veBuffer->GetDesc(&veDesc);
		veWidth = veDesc.ByteWidth;
	}
	if (NULL != veBuffer) {
		veBuffer->Release();
		veBuffer = NULL;
	}

	//get debugname (not all games)
	ID3D11PixelShader* pPixelShader = nullptr;
	pContext->PSGetShader(&pPixelShader, nullptr, nullptr);
	std::string debugName = GetDebugName(pPixelShader);
	if (pPixelShader)
	{
		pPixelShader->Release();
		// Get the shader resources bound to the pixel shader
		ID3D11ShaderResourceView* pSRV = nullptr;
		pContext->PSGetShaderResources(0, 1, &pSRV);
		if (pSRV)
		{
			ID3D11Resource* pResource = nullptr;
			pSRV->GetResource(&pResource);
			if (pResource)
			{
				std::string debugName = GetDebugName(pResource);
				pResource->Release();
			}
			pSRV->Release();
		}
	}
	
//log.txt sample
//DrawIndexedInstanced: Stride == 32 && IndexCount == 21495 && veWidth == 249376 && debugName == csgo_character.vfx_ps //cs2 models 
//DrawIndexedInstanced: Stride == 28 && IndexCount == 612 && veWidth == 58884 && debugName == csgo_character.vfx_ps //cs2 heads 
//DrawIndexedInstanced: Stride == 60 && IndexCount == 136128 && veWidth == 2220420 && debugName == csgo_weapon.vfx_ps //cs2 weapons 

#define T_Models (IndexCount == 3312 || IndexCount == 7218 || IndexCount == 8994 || IndexCount == 14184 || IndexCount == 19437)
#define CT_Models (IndexCount == 8160 || IndexCount == 8898 || IndexCount == 10242 || IndexCount == 21495) 

	//wallhack/chams
	if (wallhack == 1 || chams == 1) //if wallhack or chams option is enabled in menu
		if(debugName == "csgo_character.vfx_ps")//cs2 models
		{
			if (Stride != 28 && chams == 1)//body blue behind walls
			{
				pContext->PSGetShader(&oPixelShaderA, 0, 0);
				pContext->PSSetShader(sBlue, NULL, NULL);
			}

			if (T_Models && Stride != 28 && teamchams == 1)//use teamchams if this is enabled, Terrorists = red
			{
				pContext->PSGetShader(&oPixelShaderA, 0, 0);
				pContext->PSSetShader(sRedDark, NULL, NULL);
			}

			if (CT_Models && Stride != 28 && teamchams == 1)//use teamchams if this is enabled, CT = green
			{
				pContext->PSGetShader(&oPixelShaderA, 0, 0);
				pContext->PSSetShader(sGreenDark, NULL, NULL);
			}

			if (Stride == 28 && chams == 1) //heads grey behind walls
			{
				pContext->PSGetShader(&oPixelShaderA, 0, 0);
				pContext->PSSetShader(sGrey, NULL, NULL);
			}

			if (wallhack == 1)
			{
				pContext->OMGetDepthStencilState(&DepthStencilState_ORIG, &stencilRef); //get original
				pContext->OMSetDepthStencilState(DepthStencilState_FALSE, stencilRef); //depthstencil off

				//pContext->RSGetState(&OldState);
				//pContext->RSSetState(CustomState);
			}

			phookD3D11DrawIndexedInstanced(pContext, IndexCount, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);

			if (Stride != 28 && chams == 1)
			{
				pContext->PSGetShader(&oPixelShaderA, 0, 0);
				pContext->PSSetShader(sYellow, NULL, NULL); //yellow bodies when visible

				//pContext->PSSetShader(oPixelShaderA, NULL, NULL);
			}

			if (T_Models && Stride != 28 && teamchams == 1)//use teamchams if this is enabled, Terrorists = red
			{
				pContext->PSGetShader(&oPixelShaderA, 0, 0);
				pContext->PSSetShader(sRed, NULL, NULL);
			}

			if (CT_Models && Stride != 28 && teamchams == 1)//use teamchams if this is enabled, CT = green
			{
				pContext->PSGetShader(&oPixelShaderA, 0, 0);
				pContext->PSSetShader(sGreen, NULL, NULL);
			}

			if (Stride == 28 && chams == 1) //heads magenta when visible
			{
				pContext->PSGetShader(&oPixelShaderA, 0, 0);
				pContext->PSSetShader(sMagenta, NULL, NULL);
			}

			if (wallhack == 1)
			{
				pContext->OMSetDepthStencilState(DepthStencilState_ORIG, stencilRef); //depthstencil on

				//pContext->RSSetState(OldState);
			}
		}

	//log something
	//if (Stride == 32)
		//if (GetAsyncKeyState(VK_END) & 1) //press END to log to log.txt
			//Log("DrawIndexedInstanced: Stride == %d && IndexCount == %d && veWidth == %d && debugName == %s", Stride, IndexCount, veWidth, debugName.c_str());

	return phookD3D11DrawIndexedInstanced(pContext, IndexCount, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11Draw(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartVertexLocation)
{
	if (IndexCount > 0)
		check_draw_result = 1;

	return phookD3D11Draw(pContext, IndexCount, StartVertexLocation);
}

//==========================================================================================================================

LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){ return DefWindowProc(hwnd, uMsg, wParam, lParam); }
DWORD __stdcall InitializeHook(LPVOID)
{
	HMODULE hDXGIDLL = 0;
	do
	{
		hDXGIDLL = GetModuleHandle("dxgi.dll");
		Sleep(4000);
	} while (!hDXGIDLL);
	Sleep(100);

	//oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

    IDXGISwapChain* pSwapChain;

	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DXGIMsgProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
	RegisterClassExA(&wc);
	HWND hWnd = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

	D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
	D3D_FEATURE_LEVEL obtainedLevel;
	ID3D11Device* d3dDevice = nullptr;
	ID3D11DeviceContext* d3dContext = nullptr;

	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(scd));
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	scd.OutputWindow = hWnd;
	scd.SampleDesc.Count = 1; //1 disables multisampling
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scd.Windowed = ((GetWindowLongPtr(hWnd, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

	scd.BufferDesc.Width = 1;
	scd.BufferDesc.Height = 1;
	scd.BufferDesc.RefreshRate.Numerator = 0;
	scd.BufferDesc.RefreshRate.Denominator = 1;

	UINT createFlags = 0;
#ifdef _DEBUG
	//Debug text
	createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	IDXGISwapChain* d3dSwapChain = 0;

	if (FAILED(D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createFlags,
		requestedLevels,
		sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
		D3D11_SDK_VERSION,
		&scd,
		&pSwapChain,
		&pDevice,
		&obtainedLevel,
		&pContext)))
	{
		MessageBox(hWnd, "Failed to create DirectX Device and SwapChain!", "Error", MB_ICONERROR);
		return NULL;
	}

    pSwapChainVtable = (DWORD_PTR*)pSwapChain;
    pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];

    pContextVTable = (DWORD_PTR*)pContext;
    pContextVTable = (DWORD_PTR*)pContextVTable[0];

	pDeviceVTable = (DWORD_PTR*)pDevice;
	pDeviceVTable = (DWORD_PTR*)pDeviceVTable[0];

	if (MH_Initialize() != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pSwapChainVtable[8], hookD3D11Present, reinterpret_cast<void**>(&phookD3D11Present)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pSwapChainVtable[8]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pSwapChainVtable[13], hookD3D11ResizeBuffers, reinterpret_cast<void**>(&phookD3D11ResizeBuffers)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pSwapChainVtable[13]) != MH_OK) { return 1; }

	//if (MH_CreateHook((DWORD_PTR*)pContextVTable[12], hookD3D11DrawIndexed, reinterpret_cast<void**>(&phookD3D11DrawIndexed)) != MH_OK) { return 1; }
	//if (MH_EnableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }	
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[20], hookD3D11DrawIndexedInstanced, reinterpret_cast<void**>(&phookD3D11DrawIndexedInstanced)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[20]) != MH_OK) { return 1; }
	//if (MH_CreateHook((DWORD_PTR*)pContextVTable[13], hookD3D11Draw, reinterpret_cast<void**>(&phookD3D11Draw)) != MH_OK) { return 1; }
	//if (MH_EnableHook((DWORD_PTR*)pContextVTable[13]) != MH_OK) { return 1; }
	//DrawInstanced
	//DrawInstancedIndirect
	//DrawIndexedInstancedIndirect

	
    DWORD dwOld;
    VirtualProtect(phookD3D11Present, 2, PAGE_EXECUTE_READWRITE, &dwOld);

	while (true) {
		Sleep(10);
	}

	pDevice->Release();
	pContext->Release();
	pSwapChain->Release();

    return NULL;
}

//==========================================================================================================================

BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{ 
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH: // A process is loading the DLL.
		DisableThreadLibraryCalls(hModule);
		GetModuleFileName(hModule, dlldir, 512);
		for (size_t i = strlen(dlldir); i > 0; i--) { if (dlldir[i] == '\\') { dlldir[i + 1] = 0; break; } }
		CreateThread(NULL, 0, InitializeHook, NULL, 0, NULL);
		break;

	case DLL_PROCESS_DETACH: // A process unloads the DLL.
		if (MH_Uninitialize() != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pSwapChainVtable[8]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pSwapChainVtable[13]) != MH_OK) { return 1; }

		//if (MH_DisableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pContextVTable[20]) != MH_OK) { return 1; }
		//if (MH_DisableHook((DWORD_PTR*)pContextVTable[13]) != MH_OK) { return 1; }

		break;
	}
	return TRUE;
}
