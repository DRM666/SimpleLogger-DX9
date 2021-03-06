﻿// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include <Windows.h>
#include <iostream>

#include <d3d9.h>
#include <d3dx9.h>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

#include <detours.h>
#pragma comment (lib, "detours.lib")

#define MLOGGER (LOGGERNUM[0] == iStride && LOGGERNUM[1] == NumVertices && LOGGERNUM[2] == primCount || LOGGERNUM[0] == iStride && LOGGERNUM[1] == 0 && LOGGERNUM[2] == 0 || LOGGERNUM[0] == iStride && LOGGERNUM[1] == NumVertices && LOGGERNUM[2] == 0 || LOGGERNUM[0] == iStride && LOGGERNUM[1] == 0 && LOGGERNUM[2] == primCount)

//Uncomment this to attempt to acquire VTable Pointer by signature scanning
//#define USESIG

typedef HRESULT(__stdcall * f_EndScene)(IDirect3DDevice9 * pDevice);

typedef HRESULT (__stdcall * f_DrawIndexedPrimitive)(
	LPDIRECT3DDEVICE9 pDevice,
	D3DPRIMITIVETYPE Type,
	INT              BaseVertexIndex,
	UINT             MinVertexIndex,
	UINT             NumVertices,
	UINT             startIndex,
	UINT             primCount
);

typedef HRESULT(__stdcall * f_Reset)(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pD3Dpp);

f_DrawIndexedPrimitive pTrp_DrawIndexedPrimitive;
f_EndScene pTrp_EndScene;

INT LOGGERNUM[3] = {0, 0, 0};
INT I;
INT SPEED;

ID3DXFont * pFontTahoma;

LPDIRECT3DVERTEXBUFFER9 ppStreamData;
UINT pOffset;
UINT iStride;

LPDIRECT3DTEXTURE9 texGreen;
LPDIRECT3DTEXTURE9 texRed;
IDirect3DPixelShader9 * shaderback;
IDirect3DPixelShader9 * shaderfront;

char buffer2[128];

/*
Credits: MPGH for sig and func
you can uncomment the above line #define USESIG to signature scan for the VTable instead
of creating a dummy device*/

#ifdef USESIG
DWORD_PTR * FindDevice(VOID)
{
	DWORD dwBase = (DWORD)GetModuleHandleA("d3d9.dll");

	for (int i = 0; i < 0x128000; i++)
	{
		if ((*(BYTE *)(dwBase + i + 0x00)) == 0xC7
			&& (*(BYTE *)(dwBase + i + 0x01)) == 0x06
			&& (*(BYTE *)(dwBase + i + 0x06)) == 0x89
			&& (*(BYTE *)(dwBase + i + 0x07)) == 0x86
			&& (*(BYTE *)(dwBase + i + 0x0C)) == 0x89
			&& (*(BYTE *)(dwBase + i + 0x0D)) == 0x86)

			return (DWORD_PTR *)(dwBase + i + 2);
	}
	return NULL;
}
#endif

/* Credits: unknowncheats.me
GenerateShader() generates texture takes a pointer to device pointer to shader object and a color values as float rgb*/

HRESULT GenerateShader(IDirect3DDevice9 *pD3Ddev, IDirect3DPixelShader9 **pShader, float r, float g, float b)
{
	char szShader[256];
	ID3DXBuffer *pShaderBuf = NULL;
	sprintf(szShader, "ps.1.1\ndef c0, %f, %f, %f, %f\nmov r0,c0", r, g, b, 1.0f);
	D3DXAssembleShader(szShader, sizeof(szShader), NULL, NULL, 0, &pShaderBuf, NULL);
	if (FAILED(pD3Ddev->CreatePixelShader((const DWORD*)pShaderBuf->GetBufferPointer(), pShader)))return E_FAIL;
	return D3D_OK;
}

/* Credits: unknowncheats.me
GenerateTexture() generates texture takes a pointer to device pointer to texture object and a color*/

HRESULT GenerateTexture(IDirect3DDevice9 *pD3Ddev, IDirect3DTexture9 **ppD3Dtex, DWORD colour32)
{
	if (FAILED(pD3Ddev->CreateTexture(8, 8, 1, 0, D3DFMT_A4R4G4B4, D3DPOOL_MANAGED, ppD3Dtex, NULL)))
		return E_FAIL;

	WORD colour16 = ((WORD)((colour32 >> 28) & 0xF) << 12)
		| (WORD)(((colour32 >> 20) & 0xF) << 8)
		| (WORD)(((colour32 >> 12) & 0xF) << 4)
		| (WORD)(((colour32 >> 4) & 0xF) << 0);

	D3DLOCKED_RECT d3dlr;
	(*ppD3Dtex)->LockRect(0, &d3dlr, 0, 0);
	WORD *pDst16 = (WORD*)d3dlr.pBits;

	for (int xy = 0; xy < 8 * 8; xy++)
		*pDst16++ = colour16;

	(*ppD3Dtex)->UnlockRect(0);

	return S_OK;
}

void DEBUGMSG(char* msg) {
	printf("[SimpleLogger]: %s\n", msg);
	return;
}

void DrawStringOutline(char* string, float x, float y, int r, int g, int b, int a, ID3DXFont* pFont) {
	RECT rPosition;

	rPosition.left = x + 1.0f;
	rPosition.top = y;

	pFont->DrawTextA(0, string, strlen(string), &rPosition, DT_NOCLIP, D3DCOLOR_RGBA(1, 1, 1, a));

	rPosition.left = x - 1.0f;
	rPosition.top = y;

	pFont->DrawTextA(0, string, strlen(string), &rPosition, DT_NOCLIP, D3DCOLOR_RGBA(1, 1, 1, a));

	rPosition.left = x;
	rPosition.top = y + 1.0f;

	pFont->DrawTextA(0, string, strlen(string), &rPosition, DT_NOCLIP, D3DCOLOR_RGBA(1, 1, 1, a));

	rPosition.left = x;
	rPosition.top = y - 1.0f;

	pFont->DrawTextA(0, string, strlen(string), &rPosition, DT_NOCLIP, D3DCOLOR_RGBA(1, 1, 1, a));

	rPosition.left = x;
	rPosition.top = y;

	pFont->DrawTextA(0, string, strlen(string), &rPosition, DT_NOCLIP, D3DCOLOR_RGBA(r, g, b, a));
}

void DrawString(char* string, float x, float y, int r, int g, int b, int a, ID3DXFont* pFont) {
	RECT rPosition;
	rPosition.left = x;
	rPosition.top = y;

	pFont->DrawTextA(0, string, strlen(string), &rPosition, DT_NOCLIP, D3DCOLOR_RGBA(r, g, b, a));
}

HRESULT __stdcall hk_DrawIndexedPrimitive(LPDIRECT3DDEVICE9 pDevice, D3DPRIMITIVETYPE Type, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) 
{
		//Create Resources
		if (!texGreen)
			GenerateTexture(pDevice, &texGreen, D3DCOLOR_ARGB(0, 0, 255, 0));

		if (!texRed)
			GenerateTexture(pDevice, &texRed, D3DCOLOR_ARGB(255, 255, 0, 0));


		//Log Model Stride
		if (pDevice->GetStreamSource(0, &ppStreamData, &pOffset, &iStride) == D3D_OK)
			ppStreamData->Release();

		//if (MLOGGER) { For doing no draw hacks like no hands
		//	return D3DERR_INVALIDCALL;
		//}


		if (MLOGGER) {
			//Chams Wallhack

			//Disable z-buffer while occluded
			pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

			//pDevice->SetTexture(0, texRed); You can uncomment this line and comment SetPixelShader if you would like to use the SetTexture method.
			pDevice->SetPixelShader(shaderback);

			pTrp_DrawIndexedPrimitive(pDevice, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

			//Re-enable z buffer
			pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

			//pDevice->SetTexture(0, texGreen); You can uncomment this line and comment SetPixelShader if you would like to use the SetTexture method.
			pDevice->SetPixelShader(shaderfront);

			//Model Logger
			if (GetAsyncKeyState(0x4C) & 1) {
				sprintf((char *)buffer2, "iStride == %d && NumVertices == %d && primCount == %d", (int)iStride, (int)(NumVertices), (int)(primCount));

				//Print log to console
				DEBUGMSG((char *)buffer2);
			}
		}

	return pTrp_DrawIndexedPrimitive(pDevice, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT __stdcall hk_EndScene(IDirect3DDevice9 * pDevice)
{
	//Create Resources
	if (!pFontTahoma)
		D3DXCreateFont(pDevice, 14, 0, 0, 0, false, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Tahoma", &pFontTahoma);


	if (!shaderback)
		GenerateShader(pDevice, &shaderback, 221.0f / 255.0f, 177.0f / 255.0f, 31.0f / 255.0f);


	if (!shaderfront)
		GenerateShader(pDevice, &shaderfront, 31.0f / 255.0f, 99.0f / 255.0f, 155.0f / 255.0f);

	//Drawing Area
	char buffer[128];

	//Draw Watermark
	DrawStringOutline((char *)"D3D9 Simple Logger - DREAM666", 5.0f, 5.0f, 109, 20, 22, 255, pFontTahoma);

	//Setup logger colors
	D3DXCOLOR clrSelected = { 0.0f, 255.0f, 0.0f, 255.0f };
	D3DXCOLOR clrText = { 140.0f, 36.0f, 160.0f, 255.0f };
	D3DXCOLOR clrTemp = { };

	//Draw Logger
	clrTemp = clrText; if (I == 0) clrTemp = clrSelected;
	sprintf((char *)buffer, "Stride: %d", (int)LOGGERNUM[0]);
	DrawStringOutline((char *)buffer, 5.0f, 18.0f, clrTemp.r, clrTemp.g, clrTemp.b, clrTemp.a, pFontTahoma);

	clrTemp = clrText; if (I == 1) clrTemp = clrSelected;
	sprintf((char *)buffer, "NumVertices: %d", (int)LOGGERNUM[1]);
	DrawStringOutline((char *)buffer, 5.0f, 31.0f, clrTemp.r, clrTemp.g, clrTemp.b, clrTemp.a, pFontTahoma);

	clrTemp = clrText; if (I == 2) clrTemp = clrSelected;
	sprintf((char *)buffer, "primCount: %d", (int)LOGGERNUM[2]);
	DrawStringOutline((char *)buffer, 5.0f, 41.0f, clrTemp.r, clrTemp.g, clrTemp.b, clrTemp.a, pFontTahoma);

	DrawStringOutline((char *)"Press \"END\" to unload module.", 5.0f, 56.0f, 66, 201, 62, 255, pFontTahoma);
	DrawStringOutline((char *)"Press \"L\" to print model log.", 5.0f, 69.0f, 66, 201, 62, 255, pFontTahoma);
	DrawStringOutline((char *)buffer2, 5.0f, 82.0f, 109, 20, 22, 255, pFontTahoma);

	if (pFontTahoma) {
		pFontTahoma->OnLostDevice();
		pFontTahoma->OnResetDevice();
	}

	//Return to original EndScene
	return pTrp_EndScene(pDevice);
}

void Hook(HMODULE hModule)
{

#ifdef _DEBUG
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
#endif

	//Create Pointer to DirectX Interface
	IDirect3D9* pD3D = NULL;
	
	while (!pD3D) {
		pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	}

	//Setting up parameters for device
	D3DPRESENT_PARAMETERS d3dParams = { 0 };

	d3dParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dParams.hDeviceWindow = GetForegroundWindow();
	d3dParams.Windowed = TRUE;

	//Create dummy device
	IDirect3DDevice9 * pDummyDevice = nullptr;
	HRESULT hResult = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dParams.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dParams, &pDummyDevice);
	
	//Check if device creation failed
	if (FAILED(hResult) || !pDummyDevice)
	{
#ifdef _DEBUG
		DEBUGMSG((char *)"Dummy device creation failed!");
		DEBUGMSG((char *)"Retry...");
#endif
		while (FAILED(hResult) || !pDummyDevice) {
			HRESULT hResult = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dParams.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dParams, &pDummyDevice);
		}

#ifdef _DEBUG
		DEBUGMSG((char *)"Dummy device created.");
#endif
	}

#ifdef USESIG
	DWORD_PTR * VtablePtr = FindDevice();
	DWORD_PTR * pVTable;
	*(DWORD_PTR *)&pVTable = *(DWORD_PTR *)VtablePtr;
#endif

	//Get VTable from dummy device
#ifndef USESIG
	//Dereference dummy device one level to the vtable
	void ** pVTable = *(void ** *)(pDummyDevice);
#endif

	//Hook Functions
	pTrp_EndScene = (f_EndScene)DetourFunction((PBYTE)pVTable[42], (PBYTE)hk_EndScene);
	pTrp_DrawIndexedPrimitive = (f_DrawIndexedPrimitive)DetourFunction((PBYTE)pVTable[82], (PBYTE)hk_DrawIndexedPrimitive);

#ifdef _DEBUG
	DEBUGMSG((char *)"Functions hooked!");
	DEBUGMSG((char *)"Releasing objects.");
#endif

	//Cleanup
	pDummyDevice->Release();
	pD3D->Release();

	while (!GetAsyncKeyState(VK_END)) {
		if (GetAsyncKeyState(VK_RIGHT)) {
			LOGGERNUM[I]++;
			if (SPEED < 84)
				SPEED++;
		}if (GetAsyncKeyState(VK_LEFT)) {
			LOGGERNUM[I]--;
			if (SPEED < 84)
				SPEED++;
		}if (GetAsyncKeyState(VK_UP)) {
			if (I > 0)
				I--;
		}if (GetAsyncKeyState(VK_DOWN)) {
			if (I < 2)
				I++;
		}

		if (!GetAsyncKeyState(VK_RIGHT) && !GetAsyncKeyState(VK_LEFT))
			SPEED = 0;

		Sleep(100 - SPEED);
	}

#ifdef _DEBUG
	DEBUGMSG((char *)"Unloading module.");
#endif

	//Remove Hooks
	DetourRemove((PBYTE)pTrp_EndScene, (PBYTE)hk_EndScene);
	DetourRemove((PBYTE)pTrp_DrawIndexedPrimitive, (PBYTE)hk_DrawIndexedPrimitive);

#ifdef _DEBUG
	DEBUGMSG((char *)"Hooks removed.");
#endif

	Sleep(500);

#ifdef _DEBUG
	fclose(stdin); fclose(stdout); fclose(stderr);
	FreeConsole();

#endif

	FreeLibraryAndExitThread((HMODULE)hModule, 0);

	return;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)(Hook), (HMODULE)hModule, 0, 0);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

