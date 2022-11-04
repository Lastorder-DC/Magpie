#include "pch.h"
#include "AppSettings.h"
#include "StrUtils.h"
#include "Win32Utils.h"
#include "Logger.h"
#include "HotkeyHelper.h"
#include "ScalingProfile.h"
#include "CommonSharedConstants.h"
#include <rapidjson/prettywriter.h>
#include "AutoStartHelper.h"
#include <Magpie.Core.h>
#include "ScalingModesService.h"
#include "JsonHelper.h"

using namespace Magpie::Core;


namespace winrt::Magpie::UI {

static constexpr uint32_t SETTINGS_VERSION = 0;

// 将热键存储为 uint32_t
// 不能存储为字符串，因为某些键有相同的名称，如句号和小键盘的点
static uint32_t EncodeHotkey(const HotkeySettings& hotkey) noexcept {
	uint32_t value = 0;
	value |= hotkey.code;
	if (hotkey.win) {
		value |= 0x100;
	}
	if (hotkey.ctrl) {
		value |= 0x200;
	}
	if (hotkey.alt) {
		value |= 0x400;
	}
	if (hotkey.shift) {
		value |= 0x800;
	}
	return value;
}

static void DecodeHotkey(uint32_t value, HotkeySettings& hotkey) noexcept {
	if (value > 0xfff) {
		return;
	}

	hotkey.code = value & 0xff;
	hotkey.win = value & 0x100;
	hotkey.ctrl = value & 0x200;
	hotkey.alt = value & 0x400;
	hotkey.shift = value & 0x800;
}

static void WriteScalingProfile(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, const ScalingProfile& scalingProfile) {
	writer.StartObject();
	if (!scalingProfile.name.empty()) {
		writer.Key("name");
		writer.String(StrUtils::UTF16ToUTF8(scalingProfile.name).c_str());
		writer.Key("packaged");
		writer.Bool(scalingProfile.isPackaged);
		writer.Key("pathRule");
		writer.String(StrUtils::UTF16ToUTF8(scalingProfile.pathRule).c_str());
		writer.Key("classNameRule");
		writer.String(StrUtils::UTF16ToUTF8(scalingProfile.classNameRule).c_str());
	}

	writer.Key("scalingMode");
	writer.Int(scalingProfile.scalingMode);
	writer.Key("captureMode");
	writer.Uint((uint32_t)scalingProfile.captureMode);
	writer.Key("multiMonitorUsage");
	writer.Uint((uint32_t)scalingProfile.multiMonitorUsage);
	writer.Key("graphicsAdapter");
	writer.Uint(scalingProfile.graphicsAdapter);

	writer.Key("disableWindowResizing");
	writer.Bool(scalingProfile.IsDisableWindowResizing());
	writer.Key("3DGameMode");
	writer.Bool(scalingProfile.Is3DGameMode());
	writer.Key("showFPS");
	writer.Bool(scalingProfile.IsShowFPS());
	writer.Key("VSync");
	writer.Bool(scalingProfile.IsVSync());
	writer.Key("tripleBuffering");
	writer.Bool(scalingProfile.IsTripleBuffering());
	writer.Key("reserveTitleBar");
	writer.Bool(scalingProfile.IsReserveTitleBar());
	writer.Key("adjustCursorSpeed");
	writer.Bool(scalingProfile.IsAdjustCursorSpeed());
	writer.Key("drawCursor");
	writer.Bool(scalingProfile.IsDrawCursor());
	writer.Key("disableDirectFlip");
	writer.Bool(scalingProfile.IsDisableDirectFlip());

	writer.Key("cursorScaling");
	writer.Uint((uint32_t)scalingProfile.cursorScaling);
	writer.Key("customCursorScaling");
	writer.Double(scalingProfile.customCursorScaling);
	writer.Key("cursorInterpolationMode");
	writer.Uint((uint32_t)scalingProfile.cursorInterpolationMode);

	writer.Key("croppingEnabled");
	writer.Bool(scalingProfile.isCroppingEnabled);
	writer.Key("cropping");
	writer.StartObject();
	writer.Key("left");
	writer.Double(scalingProfile.cropping.Left);
	writer.Key("top");
	writer.Double(scalingProfile.cropping.Top);
	writer.Key("right");
	writer.Double(scalingProfile.cropping.Right);
	writer.Key("bottom");
	writer.Double(scalingProfile.cropping.Bottom);
	writer.EndObject();

	writer.EndObject();
}

static bool LoadScalingProfile(
	const rapidjson::GenericObject<true, rapidjson::Value>& scalingConfigObj,
	ScalingProfile& scalingProfile,
	bool isDefault = false
) {
	if (!isDefault) {
		if (!JsonHelper::ReadString(scalingConfigObj, "name", scalingProfile.name, true)
			|| scalingProfile.name.empty()) {
			return false;
		}

		if (!JsonHelper::ReadBool(scalingConfigObj, "packaged", scalingProfile.isPackaged, true)) {
			return false;
		}

		if (!JsonHelper::ReadString(scalingConfigObj, "pathRule", scalingProfile.pathRule, true)
			|| scalingProfile.pathRule.empty()) {
			return false;
		}

		if (!JsonHelper::ReadString(scalingConfigObj, "classNameRule", scalingProfile.classNameRule, true)
			|| scalingProfile.classNameRule.empty()) {
			return false;
		}
	}

	JsonHelper::ReadInt(scalingConfigObj, "scalingMode", scalingProfile.scalingMode);
	JsonHelper::ReadUInt(scalingConfigObj, "captureMode", (uint32_t&)scalingProfile.captureMode);
	JsonHelper::ReadUInt(scalingConfigObj, "multiMonitorUsage", (uint32_t&)scalingProfile.multiMonitorUsage);
	JsonHelper::ReadUInt(scalingConfigObj, "graphicsAdapter", scalingProfile.graphicsAdapter);
	JsonHelper::ReadBoolFlag(scalingConfigObj, "disableWindowResizing", MagFlags::DisableDirectFlip, scalingProfile.flags);
	JsonHelper::ReadBoolFlag(scalingConfigObj, "3DGameMode", MagFlags::Is3DGameMode, scalingProfile.flags);
	JsonHelper::ReadBoolFlag(scalingConfigObj, "showFPS", MagFlags::ShowFPS, scalingProfile.flags);
	JsonHelper::ReadBoolFlag(scalingConfigObj, "VSync", MagFlags::VSync, scalingProfile.flags);
	JsonHelper::ReadBoolFlag(scalingConfigObj, "tripleBuffering", MagFlags::TripleBuffering, scalingProfile.flags);
	JsonHelper::ReadBoolFlag(scalingConfigObj, "reserveTitleBar", MagFlags::ReserveTitleBar, scalingProfile.flags);
	JsonHelper::ReadBoolFlag(scalingConfigObj, "adjustCursorSpeed", MagFlags::AdjustCursorSpeed, scalingProfile.flags);
	JsonHelper::ReadBoolFlag(scalingConfigObj, "drawCursor", MagFlags::DrawCursor, scalingProfile.flags);
	JsonHelper::ReadBoolFlag(scalingConfigObj, "disableDirectFlip", MagFlags::DisableDirectFlip, scalingProfile.flags);
	JsonHelper::ReadUInt(scalingConfigObj, "cursorScaling", (uint32_t&)scalingProfile.cursorScaling);
	JsonHelper::ReadFloat(scalingConfigObj, "customCursorScaling", scalingProfile.customCursorScaling);
	JsonHelper::ReadUInt(scalingConfigObj, "cursorInterpolationMode", (uint32_t&)scalingProfile.cursorInterpolationMode);

	JsonHelper::ReadBool(scalingConfigObj, "croppingEnabled", scalingProfile.isCroppingEnabled);

	auto croppingNode = scalingConfigObj.FindMember("cropping");
	if (croppingNode != scalingConfigObj.MemberEnd() && croppingNode->value.IsObject()) {
		const auto& croppingObj = croppingNode->value.GetObj();

		if (!JsonHelper::ReadFloat(croppingObj, "left", scalingProfile.cropping.Left, true)
			|| !JsonHelper::ReadFloat(croppingObj, "top", scalingProfile.cropping.Top, true)
			|| !JsonHelper::ReadFloat(croppingObj, "right", scalingProfile.cropping.Right, true)
			|| !JsonHelper::ReadFloat(croppingObj, "bottom", scalingProfile.cropping.Bottom, true))
		{
			scalingProfile.cropping = {};
		}
	}

	return true;
}

bool AppSettings::Initialize() {
	Logger& logger = Logger::Get();

	// 若程序所在目录存在配置文件则为便携模式
	_isPortableMode = Win32Utils::FileExists(
		StrUtils::ConcatW(CommonSharedConstants::CONFIG_DIR, CommonSharedConstants::CONFIG_NAME).c_str());
	_UpdateConfigPath();

	if (!Win32Utils::FileExists(_configPath.c_str())) {
		logger.Info("구성 파일이 존재하지 않습니다");
		// 只有不存在配置文件时才生成默认缩放模式
		_SetDefaultScalingModes();
		_SetDefaultHotkeys();
		return true;
	}

	std::string configText;
	if (!Win32Utils::ReadTextFile(_configPath.c_str(), configText)) {
		MessageBox(NULL, L"구성 파일을 읽을 수 없습니다", L"오류", MB_ICONERROR | MB_OK);
		logger.Error("구성 파일을 읽지 못했습니다.");
		return false;
	}

	if (configText.empty()) {
		Logger::Get().Info("구성 파일이 비어 있습니다");
		_SetDefaultHotkeys();
		return true;
	}

	rapidjson::Document doc;
	doc.ParseInsitu(configText.data());
	if (doc.HasParseError()) {
		Logger::Get().Error(fmt::format("구성을 구문 분석하지 못했습니다.\n\t에러 코드：{}", (int)doc.GetParseError()));
		MessageBox(NULL, L"구성 파일이 유효한 JSON이 아닙니다.", L"오류", MB_ICONERROR | MB_OK);
		return false;
	}

	if (!doc.IsObject()) {
		Logger::Get().Error("구성 파일 루트 요소가 개체가 아닙니다.");
		MessageBox(NULL, L"구성 파일을 구문 분석하지 못했습니다.", L"오류", MB_ICONERROR | MB_OK);
		return false;
	}

	auto root = ((const rapidjson::Document&)doc).GetObj();

	uint32_t settingsVersion = 0;
	// 不存在 version 字段则视为 0
	JsonHelper::ReadUInt(root, "version", settingsVersion);

	if (settingsVersion > SETTINGS_VERSION) {
		Logger::Get().Warn("알 수 없는 구성 파일 버전");
		if (_isPortableMode) {
			if (MessageBox(NULL, L"구성 파일은 이후 버전에서 가져온 것이며 올바르게 구문 분석되지 않을 수 있습니다.\n계속하려면 확인을 클릭하고 종료하려면 취소를 클릭하십시오.",
				L"오류", MB_ICONWARNING | MB_OKCANCEL) != IDOK
			) {
				return false;
			}
		} else {
			if (MessageBox(NULL, L"전역 구성 파일은 이후 버전에서 가져온 것이며 올바르게 구문 분석되지 않을 수 있습니다.\n계속하려면 확인을 클릭하고 포터블 모드를 활성화하려면 취소를 클릭하십시오.",
				L"오류", MB_ICONWARNING | MB_OKCANCEL) != IDOK
			) {
				IsPortableMode(true);
				_SetDefaultScalingModes();
				_SetDefaultHotkeys();
				return true;
			}
		}
	}

	_LoadSettings(root, settingsVersion);

	_SetDefaultHotkeys();
	return true;
}

bool AppSettings::Save() {
	if (!Win32Utils::CreateDir(_configDir)) {
		Logger::Get().Error("구성 폴더를 생성하지 못했습니다.");
		return false;
	}

	rapidjson::StringBuffer json;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(json);
	writer.StartObject();

	writer.Key("version");
	writer.Uint(SETTINGS_VERSION);

	writer.Key("theme");
	writer.Uint(_theme);

	if (HWND hwndMain = (HWND)Application::Current().as<App>().HwndMain()) {
		WINDOWPLACEMENT wp{};
		wp.length = sizeof(wp);
		if (GetWindowPlacement(hwndMain, &wp)) {
			_windowRect = {
				wp.rcNormalPosition.left,
				wp.rcNormalPosition.top,
				wp.rcNormalPosition.right - wp.rcNormalPosition.left,
				wp.rcNormalPosition.bottom - wp.rcNormalPosition.top
			};
			_isWindowMaximized = wp.showCmd == SW_MAXIMIZE;

		} else {
			Logger::Get().Win32Error("GetWindowPlacement 오류");
		}
	}

	writer.Key("windowPos");
	writer.StartObject();
	writer.Key("x");
	writer.Int(_windowRect.left);
	writer.Key("y");
	writer.Int(_windowRect.top);
	writer.Key("width");
	writer.Uint((uint32_t)_windowRect.right);
	writer.Key("height");
	writer.Uint((uint32_t)_windowRect.bottom);
	writer.Key("maximized");
	writer.Bool(_isWindowMaximized);
	writer.EndObject();

	writer.Key("hotkeys");
	writer.StartObject();
	writer.Key("scale");
	writer.Uint(EncodeHotkey(_hotkeys[(size_t)HotkeyAction::Scale]));
	writer.Key("overlay");
	writer.Uint(EncodeHotkey(_hotkeys[(size_t)HotkeyAction::Overlay]));
	writer.EndObject();

	writer.Key("autoRestore");
	writer.Bool(_isAutoRestore);
	writer.Key("downCount");
	writer.Uint(_downCount);
	writer.Key("debugMode");
	writer.Bool(_isDebugMode);
	writer.Key("disableEffectCache");
	writer.Bool(_isDisableEffectCache);
	writer.Key("saveEffectSources");
	writer.Bool(_isSaveEffectSources);
	writer.Key("warningsAreErrors");
	writer.Bool(_isWarningsAreErrors);
	writer.Key("simulateExclusiveFullscreen");
	writer.Bool(_isSimulateExclusiveFullscreen);
	writer.Key("alwaysRunAsElevated");
	writer.Bool(_isAlwaysRunAsElevated);
	writer.Key("showTrayIcon");
	writer.Bool(_isShowTrayIcon);
	writer.Key("inlineParams");
	writer.Bool(_isInlineParams);

	if (!_downscalingEffect.name.empty()) {
		writer.Key("downscalingEffect");
		writer.StartObject();
		writer.Key("name");
		writer.String(StrUtils::UTF16ToUTF8(_downscalingEffect.name).c_str());
		if (!_downscalingEffect.parameters.empty()) {
			writer.Key("parameters");
			writer.StartObject();
			for (const auto& [name, value] : _downscalingEffect.parameters) {
				writer.Key(StrUtils::UTF16ToUTF8(name).c_str());
				writer.Double(value);
			}
			writer.EndObject();
		}
		writer.EndObject();
	}

	ScalingModesService::Get().Export(writer);

	writer.Key("scalingProfiles");
	writer.StartArray();
	WriteScalingProfile(writer, _defaultScalingProfile);
	for (const ScalingProfile& rule : _scalingProfiles) {
		WriteScalingProfile(writer, rule);
	}
	writer.EndArray();

	writer.EndObject();

	if (!Win32Utils::WriteTextFile(_configPath.c_str(), { json.GetString(), json.GetLength() })) {
		Logger::Get().Error("구성을 저장하지 못했습니다.");
		return false;
	}

	return true;
}

void AppSettings::IsPortableMode(bool value) {
	if (_isPortableMode == value) {
		return;
	}

	if (!value) {
		// 关闭便携模式需删除本地配置文件
		// 不关心是否成功
		DeleteFile(StrUtils::ConcatW(_configDir, CommonSharedConstants::CONFIG_NAME).c_str());
	}

	Logger::Get().Info(value ? "포터블 모드가 켜져 있습니다." : "포터블 모드가 꺼져 있습니다.");

	_isPortableMode = value;
	_UpdateConfigPath();
}

void AppSettings::Theme(uint32_t value) {
	assert(value <= 2);

	if (_theme == value) {
		return;
	}

	_theme = value;
	_themeChangedEvent(value);
}

void AppSettings::SetHotkey(HotkeyAction action, const Magpie::UI::HotkeySettings& value) {
	if (_hotkeys[(size_t)action] == value) {
		return;
	}

	_hotkeys[(size_t)action] = value;
	Logger::Get().Info(fmt::format("단축키 {}가 {}로 변경됨", HotkeyHelper::ToString(action), StrUtils::UTF16ToUTF8(value.ToString())));
	_hotkeyChangedEvent(action);
}

void AppSettings::IsAutoRestore(bool value) noexcept {
	if (_isAutoRestore == value) {
		return;
	}

	_isAutoRestore = value;
	_isAutoRestoreChangedEvent(value);
}

void AppSettings::DownCount(uint32_t value) noexcept {
	if (_downCount == value) {
		return;
	}

	_downCount = value;
	_downCountChangedEvent(value);
}

void AppSettings::IsAlwaysRunAsElevated(bool value) noexcept {
	if (_isAlwaysRunAsElevated == value) {
		return;
	}

	_isAlwaysRunAsElevated = value;
	std::wstring arguments;
	if (AutoStartHelper::IsAutoStartEnabled(arguments)) {
		// 更新启动任务
		AutoStartHelper::EnableAutoStart(value, _isShowTrayIcon ? arguments.c_str() : nullptr);
	}
}

void AppSettings::IsShowTrayIcon(bool value) noexcept {
	if (_isShowTrayIcon == value) {
		return;
	}

	_isShowTrayIcon = value;
	_isShowTrayIconChangedEvent(value);
}

// 永远不会失败，遇到不合法的配置项则静默忽略
void AppSettings::_LoadSettings(const rapidjson::GenericObject<true, rapidjson::Value>& root, uint32_t /*version*/) {
	JsonHelper::ReadUInt(root, "theme", _theme);

	auto windowPosNode = root.FindMember("windowPos");
	if (windowPosNode != root.MemberEnd() && windowPosNode->value.IsObject()) {
		const auto& windowRectObj = windowPosNode->value.GetObj();

		int x = 0;
		int y = 0;
		if (JsonHelper::ReadInt(windowRectObj, "x", x, true)
			&& JsonHelper::ReadInt(windowRectObj, "y", y, true)) {
			_windowRect.left = x;
			_windowRect.top = y;
		}

		uint32_t width = 0;
		uint32_t height = 0;
		if (JsonHelper::ReadUInt(windowRectObj, "width", width, true)
			&& JsonHelper::ReadUInt(windowRectObj, "height", height, true)) {
			_windowRect.right = (LONG)width;
			_windowRect.bottom = (LONG)height;
		}

		JsonHelper::ReadBool(windowRectObj, "maximized", _isWindowMaximized);
	}

	auto hotkeysNode = root.FindMember("hotkeys");
	if (hotkeysNode != root.MemberEnd() && hotkeysNode->value.IsObject()) {
		const auto& hotkeysObj = hotkeysNode->value.GetObj();

		auto scaleNode = hotkeysObj.FindMember("scale");
		if (scaleNode != hotkeysObj.MemberEnd() && scaleNode->value.IsUint()) {
			DecodeHotkey(scaleNode->value.GetUint(), _hotkeys[(size_t)HotkeyAction::Scale]);
		}

		auto overlayNode = hotkeysObj.FindMember("overlay");
		if (overlayNode != hotkeysObj.MemberEnd() && overlayNode->value.IsUint()) {
			DecodeHotkey(overlayNode->value.GetUint(), _hotkeys[(size_t)HotkeyAction::Overlay]);
		}
	}

	JsonHelper::ReadBool(root, "autoRestore", _isAutoRestore);
	JsonHelper::ReadUInt(root, "downCount", _downCount);
	JsonHelper::ReadBool(root, "debugMode", _isDebugMode);
	JsonHelper::ReadBool(root, "disableEffectCache", _isDisableEffectCache);
	JsonHelper::ReadBool(root, "saveEffectSources", _isSaveEffectSources);
	JsonHelper::ReadBool(root, "warningsAreErrors", _isWarningsAreErrors);
	JsonHelper::ReadBool(root, "simulateExclusiveFullscreen", _isSimulateExclusiveFullscreen);
	JsonHelper::ReadBool(root, "alwaysRunAsElevated", _isAlwaysRunAsElevated);
	JsonHelper::ReadBool(root, "showTrayIcon", _isShowTrayIcon);
	JsonHelper::ReadBool(root, "inlineParams", _isInlineParams);

	auto downscalingEffectNode = root.FindMember("downscalingEffect");
	if (downscalingEffectNode != root.MemberEnd() && downscalingEffectNode->value.IsObject()) {
		auto downscalingEffectObj = downscalingEffectNode->value.GetObj();

		JsonHelper::ReadString(downscalingEffectObj, "name", _downscalingEffect.name);
		if (!_downscalingEffect.name.empty()) {
			auto parametersNode = downscalingEffectObj.FindMember("parameters");
			if (parametersNode != downscalingEffectObj.MemberEnd() && parametersNode->value.IsObject()) {
				auto paramsObj = parametersNode->value.GetObj();
				_downscalingEffect.parameters.reserve(paramsObj.MemberCount());
				for (const auto& param : paramsObj) {
					if (!param.value.IsNumber()) {
						continue;
					}

					std::wstring name = StrUtils::UTF8ToUTF16(param.name.GetString());
					_downscalingEffect.parameters[name] = param.value.GetFloat();
				}
			}
		}
	}

	ScalingModesService::Get().Import(root, false);

	auto scaleProfilesNode = root.FindMember("scalingProfiles");
	if (scaleProfilesNode != root.MemberEnd() && scaleProfilesNode->value.IsArray()) {
		const auto& scaleProfilesArray = scaleProfilesNode->value.GetArray();

		const rapidjson::SizeType size = scaleProfilesArray.Size();
		if (size > 0) {
			if (scaleProfilesArray[0].IsObject()) {
				// 解析默认缩放配置不会失败
				LoadScalingProfile(scaleProfilesArray[0].GetObj(), _defaultScalingProfile, true);
			}

			if (size > 1) {
				_scalingProfiles.reserve((size_t)size - 1);
				for (rapidjson::SizeType i = 1; i < size; ++i) {
					if (!scaleProfilesArray[i].IsObject()) {
						continue;
					}

					ScalingProfile& rule = _scalingProfiles.emplace_back();
					if (!LoadScalingProfile(scaleProfilesArray[i].GetObj(), rule)) {
						_scalingProfiles.pop_back();
						continue;
					}
				}
			}
		}
	}
}

void AppSettings::_SetDefaultHotkeys() {
	HotkeySettings& scaleHotkey = _hotkeys[(size_t)HotkeyAction::Scale];
	if (scaleHotkey.IsEmpty()) {
		scaleHotkey.win = true;
		scaleHotkey.shift = true;
		scaleHotkey.code = 'A';
	}

	HotkeySettings& overlayHotkey = _hotkeys[(size_t)HotkeyAction::Overlay];
	if (overlayHotkey.IsEmpty()) {
		overlayHotkey.win = true;
		overlayHotkey.shift = true;
		overlayHotkey.code = 'D';
	}
}

void AppSettings::_SetDefaultScalingModes() {
	_scalingModes.resize(7);

	// Lanczos
	{
		auto& lanczos = _scalingModes[0];
		lanczos.name = L"Lanczos";

		auto& lanczosEffect = lanczos.effects.emplace_back();
		lanczosEffect.name = L"Lanczos";
		lanczosEffect.scalingType = ::Magpie::Core::ScalingType::Fit;
	}
	// FSR
	{
		auto& fsr = _scalingModes[1];
		fsr.name = L"FSR";

		fsr.effects.resize(2);
		auto& easu = fsr.effects[0];
		easu.name = L"FSR\\FSR_EASU";
		easu.scalingType = ::Magpie::Core::ScalingType::Fit;
		auto& rcas = fsr.effects[1];
		rcas.name = L"FSR\\FSR_RCAS";
		rcas.parameters[L"sharpness"] = 0.87f;
	}
	// FSRCNNX
	{
		auto& fsrcnnx = _scalingModes[2];
		fsrcnnx.name = L"FSRCNNX";
		fsrcnnx.effects.emplace_back().name = L"FSRCNNX\\FSRCNNX";
	}
	// ACNet
	{
		auto& acnet = _scalingModes[3];
		acnet.name = L"ACNet";
		acnet.effects.emplace_back().name = L"ACNet";
	}
	// Anime4K
	{
		auto& anime4k = _scalingModes[4];
		anime4k.name = L"Anime4K";
		anime4k.effects.emplace_back().name = L"Anime4K\\Anime4K_Upscale_Denoise_L";
	}
	// CRT-Geom
	{
		auto& crtGeom = _scalingModes[5];
		crtGeom.name = L"CRT-Geom";

		auto& crtGeomEffect = crtGeom.effects.emplace_back();
		crtGeomEffect.name = L"CRT\\CRT_Geom";
		crtGeomEffect.scalingType = ::Magpie::Core::ScalingType::Fit;
		crtGeomEffect.parameters[L"curvature"] = 0.0f;
		crtGeomEffect.parameters[L"cornerSize"] = 0.001f;
		crtGeomEffect.parameters[L"CRTGamma"] = 1.5f;
		crtGeomEffect.parameters[L"monitorGamma"] = 2.2f;
		crtGeomEffect.parameters[L"interlace"] = 0.0f;
	}
	// Integer Scale 2x
	{
		auto& integer2x = _scalingModes[6];
		integer2x.name = L"Integer Scale 2x";

		auto& nearest = integer2x.effects.emplace_back();
		nearest.name = L"Nearest";
		nearest.scalingType = ::Magpie::Core::ScalingType::Normal;
		nearest.scale = { 2.0f,2.0f };
	}

	// 降采样效果默认为 Bicubic (B=0, C=0.5)
	_downscalingEffect.name = L"Bicubic";
	_downscalingEffect.parameters[L"paramB"] = 0.0f;
	_downscalingEffect.parameters[L"paramC"] = 0.5f;
}

void AppSettings::_UpdateConfigPath() noexcept {
	if (_isPortableMode) {
		wchar_t curDir[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, curDir);

		_configDir = curDir;
		if (_configDir.back() != L'\\') {
			_configDir.push_back(L'\\');
		}
		_configDir += CommonSharedConstants::CONFIG_DIR;
	} else {
		wchar_t localAppDataDir[MAX_PATH];
		HRESULT hr = SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppDataDir);
		if (SUCCEEDED(hr)) {
			_configDir = StrUtils::ConcatW(
				localAppDataDir,
				localAppDataDir[StrUtils::StrLen(localAppDataDir) - 1] == L'\\' ? L"Magpie\\" : L"\\Magpie\\",
				CommonSharedConstants::CONFIG_DIR
			);
		} else {
			Logger::Get().ComError("SHGetFolderPath 오류", hr);
			_configDir = CommonSharedConstants::CONFIG_DIR;
		}
	}

	_configPath = _configDir + CommonSharedConstants::CONFIG_NAME;

	// 确保 ConfigDir 存在
	Win32Utils::CreateDir(_configDir.c_str(), true);
}

}
