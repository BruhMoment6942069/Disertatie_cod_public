//
// Created by Andrei on 13.07.2026.
//

#include "CameraEnumerator.h"
#include <windows.h>
#include <dshow.h>

QStringList CameraEnumerator::listCameras() {
    QStringList names;
    HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    ICreateDevEnum* devEnum = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, reinterpret_cast<void**>(&devEnum));
    if (SUCCEEDED(hr)) {
        IEnumMoniker* enumMoniker = nullptr;
        hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMoniker, 0);

        if (hr == S_OK) {
            IMoniker* moniker = nullptr;
            while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
                IPropertyBag* propBag = nullptr;
                if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag, reinterpret_cast<void**>(&propBag)))) {
                    VARIANT nameVar;
                    VariantInit(&nameVar);
                    // Prefix the index. Two identical cameras report the SAME FriendlyName,
                    // and the index is what actually gets handed to VideoCapture - showing it
                    // is the only way to tell the entries apart in the combo.
                    const QString slot = QString("[%1] ").arg(names.size());
                    if (SUCCEEDED(propBag->Read(L"FriendlyName", &nameVar,nullptr))) {
                        names.append(slot + QString::fromWCharArray(nameVar .bstrVal));
                    } else {
                        names.append(slot + QStringLiteral("Unknown camera"));
                    }
                    VariantClear(&nameVar);
                    propBag->Release();
                }
                moniker->Release();
            }
            enumMoniker->Release();
        }
        devEnum->Release();   // was leaked; this runs on every combo populate
    }
    // NOTE: this return used to sit inside the `if (SUCCEEDED(hr))` block, so a failed
    // CoCreateInstance fell off the end of a non-void function - undefined behaviour on a
    // path that only shows up when DirectShow is unavailable. Now every path returns.
    if (initHr == S_OK || initHr == S_FALSE) {
        CoUninitialize();
    }
    return names;
}
