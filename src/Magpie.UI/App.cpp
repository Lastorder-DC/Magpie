// Copyright (c) 2021 - present, Liu Xu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#include "pch.h"
#include "App.h"
#if __has_include("App.g.cpp")
#include "App.g.cpp"
#endif
#include "Win32Utils.h"
#include "Logger.h"
#include "HotkeyService.h"
#include "AppSettings.h"
#include "CommonSharedConstants.h"
#include "MagService.h"
#include <CoreWindow.h>
#include <Magpie.Core.h>
#include "EffectsService.h"

using namespace winrt;
using namespace Windows::UI::Xaml::Media;


namespace winrt::Magpie::UI::implementation {

App::App() {
	EffectsService::Get().StartInitialize();

	__super::Initialize();
	
	AddRef();
	m_inner.as<::IUnknown>()->Release();

	bool isWin11 = Win32Utils::GetOSBuild() >= 22000;
	if (!isWin11) {
		// Windows 10에서 DesktopWindowXamlSource 창 숨기기
		CoreWindow coreWindow = CoreWindow::GetForCurrentThread();
		if (coreWindow) {
			HWND hwndDWXS;
			coreWindow.as<ICoreWindowInterop>()->get_WindowHandle(&hwndDWXS);
			ShowWindow(hwndDWXS, SW_HIDE);
		}
	}

	// OS 버전에 따른 스타일 설정
	ResourceDictionary resource = Resources();

	// 운영 체제에 따라 아이콘 글꼴 선택
	resource.Insert(
		box_value(L"SymbolThemeFontFamily"),
		FontFamily(isWin11 ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets")
	);
}

App::~App() {
	Close();
}

void App::SaveSettings() {
	AppSettings::Get().Save();
}

StartUpOptions App::Initialize(int) {
	StartUpOptions result{};
	
	AppSettings& settings = AppSettings::Get();
	if (!settings.Initialize()) {
		result.IsError = true;
		return result;
	}

	result.IsError = false;
	const RECT& windowRect = settings.WindowRect();
	result.MainWndRect = {
		(float)windowRect.left,
		(float)windowRect.top,
		(float)windowRect.right,
		(float)windowRect.bottom
	};
	result.IsWndMaximized= settings.IsWindowMaximized();
	result.IsNeedElevated = settings.IsAlwaysRunAsElevated();

	HotkeyService::Get().Initialize();
	MagService::Get().Initialize();

	return result;
}

bool App::IsShowTrayIcon() const noexcept {
	return AppSettings::Get().IsShowTrayIcon();
}

event_token App::IsShowTrayIconChanged(EventHandler<bool> const& handler) {
	return AppSettings::Get().IsShowTrayIconChanged([handler(handler)](bool value) {
		handler(nullptr, value);
	});
}

void App::IsShowTrayIconChanged(event_token const& token) {
	AppSettings::Get().IsShowTrayIconChanged(token);
}

void App::HwndMain(uint64_t value) noexcept {
	if (_hwndMain == (HWND)value) {
		return;
	}

	_hwndMain = (HWND)value;
	_hwndMainChangedEvent(*this, value);
}

void App::MainPage(Magpie::UI::MainPage const& mainPage) noexcept {
	// 기본 창을 표시하기 전에 EffectsService가 초기화를 마칠 때까지 기다립니다.
	EffectsService::Get().WaitForInitialize();

	// MainPage에 대한 강력한 참조를 저장하지 마십시오.
	// MainPage에 대한 강력한 참조는 XAML Islands 내부에 유지되며 MainPage의 수명 주기는 예측할 수 없습니다.
	_mainPage = weak_ref(mainPage);
}

void App::RestartAsElevated() const noexcept {
	PostMessage(_hwndMain, CommonSharedConstants::WM_RESTART_AS_ELEVATED, 0, 0);
}

}
