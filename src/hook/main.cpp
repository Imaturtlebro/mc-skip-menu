#include <windows.h>
#include <objbase.h>
#include <uiautomation.h>

// This DLL is injected into the Minecraft Bedrock UWP client.
// On load it starts a thread that, after a short delay, uses UI Automation
// to press "Play" on the main menu and launch the most recent world.
// No game internals / addresses needed - it drives the game's own UI.

static DWORD WINAPI AutoJoinThread(LPVOID);

static void SendClick(IUIAutomation* uia, IUIAutomationElement* el) {
    (void)uia;
    if (!el) return;
    IUnknown* punk = nullptr;
    if (SUCCEEDED(el->GetCurrentPattern(UIA_InvokePatternId, &punk)) && punk) {
        IUIAutomationInvokePattern* inv = nullptr;
        if (SUCCEEDED(punk->QueryInterface(__uuidof(IUIAutomationInvokePattern), (void**)&inv)) && inv) {
            inv->Invoke();
            inv->Release();
        }
        punk->Release();
        return;
    }
    // fallback: synthesize a click at the element's clickable point
    POINT pt = { 0, 0 };
    BOOL got = FALSE;
    if (SUCCEEDED(el->GetClickablePoint(&pt, &got)) && got) {
        DWORD x = static_cast<DWORD>((65535.0f * pt.x) / GetSystemMetrics(SM_CXSCREEN));
        DWORD y = static_cast<DWORD>((65535.0f * pt.y) / GetSystemMetrics(SM_CYSCREEN));
        mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTDOWN, x, y, 0, 0);
        mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTUP, x, y, 0, 0);
    }
}

static IUIAutomationElement* FindByName(IUIAutomation* uia, IUIAutomationElement* root, LPCWSTR name) {
    VARIANT v;
    v.vt = VT_BSTR;
    v.bstrVal = SysAllocString(name);
    if (!v.bstrVal) return nullptr;
    IUIAutomationCondition* cond = nullptr;
    HRESULT hr = uia->CreatePropertyCondition(UIA_NamePropertyId, v, &cond);
    VariantClear(&v);
    if (FAILED(hr) || !cond) return nullptr;

    IUIAutomationElement* found = nullptr;

    // prefer on screen elements: name match AND NOT offscreen
    IUIAutomationCondition* off = nullptr;
    VARIANT ov;
    ov.vt = VT_BOOL;
    ov.boolVal = VARIANT_TRUE;
    if (SUCCEEDED(uia->CreatePropertyCondition(UIA_IsOffscreenPropertyId, ov, &off))) {
        IUIAutomationCondition* notOff = nullptr;
        if (SUCCEEDED(uia->CreateNotCondition(off, &notOff))) {
            IUIAutomationCondition* andc = nullptr;
            if (SUCCEEDED(uia->CreateAndCondition(cond, notOff, &andc))) {
                root->FindFirst(TreeScope_Descendants, andc, &found);
                andc->Release();
            }
            notOff->Release();
        }
        off->Release();
    }
    if (!found) root->FindFirst(TreeScope_Descendants, cond, &found);
    cond->Release();
    return found;
}

static DWORD WINAPI AutoJoinThread(LPVOID) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // give the game time to reach the main menu
    Sleep(12000);

    IUIAutomation* uia = nullptr;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IUIAutomation, reinterpret_cast<void**>(&uia)))) {
        CoUninitialize();
        return 0;
    }

    // Step 1: find the Minecraft window
    IUIAutomationElement* desktop = nullptr;
    uia->GetRootElement(&desktop);

    IUIAutomationElement* win = nullptr;
    if (desktop) {
        VARIANT wv;
        wv.vt = VT_BSTR;
        wv.bstrVal = SysAllocString(L"Minecraft");
        if (wv.bstrVal) {
            IUIAutomationCondition* wc = nullptr;
            if (SUCCEEDED(uia->CreatePropertyCondition(UIA_NamePropertyId, wv, &wc))) {
                desktop->FindFirst(TreeScope_Children, wc, &win);
                wc->Release();
            }
            VariantClear(&wv);
        }
        desktop->Release();
    }

    if (win) {
        // Step 2: press "Play"
        IUIAutomationElement* play = FindByName(uia, win, L"Play");
        if (play) {
            SendClick(uia, play);
            play->Release();
            Sleep(4000);

            // Step 3: press "Play Selected World" (Bedrock keeps last world selected)
            IUIAutomationElement* psw = FindByName(uia, win, L"Play Selected World");
            if (psw) { SendClick(uia, psw); psw->Release(); }
        }
        win->Release();
    }

    uia->Release();
    CoUninitialize();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE t = CreateThread(nullptr, 0, AutoJoinThread, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
