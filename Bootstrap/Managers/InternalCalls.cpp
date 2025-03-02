#include "InternalCalls.h"
#include "../Utils/Console/Debug.h"
#include "../Utils/Console/Logger.h"
#include "Game.h"
#include "Hook.h"
#include "../Utils/Assertion.h"
#include "../Core.h"

#include "Il2Cpp.h"
#include "../Utils/Helpers/ImportLibHelper.h"
#include "sys/mman.h"
#include "stdlib.h"
#include "../Utils/AssemblyUnhollower/XrefScannerBindings.h"
#include <android/log.h>
#include "BHapticsBridge.h"

#include <dlfcn.h>

#include "BaseAssembly.h"
#include "../Utils/Encoding.h"

bool InternalCalls::Initialized = false;

void InternalCalls::Initialize()
{
	Debug::Msg("Initializing Internal Calls...");
    MelonLogger::AddInternalCalls();
    MelonUtils::AddInternalCalls();
    MelonDebug::AddInternalCalls();
    SupportModules::AddInternalCalls();
    UnhollowerIl2Cpp::AddInternalCalls();
    BHaptics::AddInternalCalls();

    Mono::AddInternalCall("MelonLoader.Fixes.DateTimeOverride::GetLocalTimeZone", (void*)GetLocalTimeZone);

    Initialized = true;
}

#pragma region MelonLogger
void InternalCalls::MelonLogger::Internal_Msg(Console::Color meloncolor, Console::Color txtcolor, Mono::String* namesection, Mono::String* txt)
{
    auto nsStr = namesection != NULL ? Mono::Exports::mono_string_to_utf8(namesection) : NULL;
    auto txtStr = Mono::Exports::mono_string_to_utf8(txt);
    Logger::Internal_Msg(meloncolor, txtcolor, nsStr, txtStr);
    if (nsStr != NULL) Mono::Free(nsStr);
    Mono::Free(txtStr);
}

void InternalCalls::MelonLogger::Internal_PrintModName(Console::Color meloncolor, Console::Color authorcolor, Mono::String* name, Mono::String* author, Mono::String* version, Mono::String* id)
{
    auto nameStr = Mono::Exports::mono_string_to_utf8(name);
    auto nameStrOs = Encoding::Utf8ToOs(nameStr);
    Mono::Free(nameStr);

    auto versionStr = Mono::Exports::mono_string_to_utf8(version);
    auto versionStrOs = Encoding::Utf8ToOs(versionStr);
    Mono::Free(versionStr);

    char* idStrOs = NULL;
    if (id != NULL)
    {
        auto idStr = Mono::Exports::mono_string_to_utf8(id);
        idStrOs = Encoding::Utf8ToOs(idStr);
        Mono::Free(idStr);
    }

    char* authorStrOs = NULL;
    if (author != NULL)
    {
        auto authorStr = Mono::Exports::mono_string_to_utf8(author);
        authorStrOs = Encoding::Utf8ToOs(authorStr);
        Mono::Free(authorStr);
    }

    Logger::Internal_PrintModName(meloncolor, authorcolor, nameStrOs, authorStrOs, versionStrOs, idStrOs);

    delete[] nameStrOs;
    delete[] versionStrOs;
    if (idStrOs != NULL)
        delete[] idStrOs;
    if (authorStrOs != NULL)
        delete[] authorStrOs;
}

void InternalCalls::MelonLogger::Internal_Warning(Mono::String* namesection, Mono::String* txt)
{
    auto nsStr = namesection != NULL ? Mono::Exports::mono_string_to_utf8(namesection) : NULL;
    auto txtStr = Mono::Exports::mono_string_to_utf8(txt);
    Logger::Internal_Warning(nsStr, txtStr);
    if (nsStr != NULL) Mono::Free(nsStr);
    Mono::Free(txtStr);
}

void InternalCalls::MelonLogger::Internal_Error(Mono::String* namesection, Mono::String* txt)
{
    auto nsStr = namesection != NULL ? Mono::Exports::mono_string_to_utf8(namesection) : NULL;
    auto txtStr = Mono::Exports::mono_string_to_utf8(txt);
    Logger::Internal_Error(nsStr, txtStr);
    if (nsStr != NULL) Mono::Free(nsStr);
    Mono::Free(txtStr);
}

void InternalCalls::MelonLogger::ThrowInternalFailure(Mono::String* msg)
{
    auto str = Mono::Exports::mono_string_to_utf8(msg);
    Assertion::ThrowInternalFailure(str);
    Mono::Free(str);
}

void InternalCalls::MelonLogger::WriteSpacer() { Logger::WriteSpacer(); }
void InternalCalls::MelonLogger::Flush() { 
#ifndef PORT_DISABLE
    Logger::Flush(); Console::Flush(); 
#endif
}
void InternalCalls::MelonLogger::AddInternalCalls()
{
    Mono::AddInternalCall("MelonLoader.MelonLogger::Internal_PrintModName", (void*)Internal_PrintModName);
    Mono::AddInternalCall("MelonLoader.MelonLogger::Internal_Msg", (void*)Internal_Msg);
    Mono::AddInternalCall("MelonLoader.MelonLogger::Internal_Warning", (void*)Internal_Warning);
    Mono::AddInternalCall("MelonLoader.MelonLogger::Internal_Error", (void*)Internal_Error);
    Mono::AddInternalCall("MelonLoader.MelonLogger::ThrowInternalFailure", (void*)ThrowInternalFailure);
    Mono::AddInternalCall("MelonLoader.MelonLogger::WriteSpacer", (void*)WriteSpacer);
    Mono::AddInternalCall("MelonLoader.MelonLogger::Flush", (void*)Flush);
}
#pragma endregion

#pragma region MelonUtils
bool InternalCalls::MelonUtils::IsGame32Bit()
{
#ifdef _WIN64
    return false;
#else
    return true;
#endif
}
bool InternalCalls::MelonUtils::IsGameIl2Cpp() { return Game::IsIl2Cpp; }
bool InternalCalls::MelonUtils::IsOldMono() { return Mono::IsOldMono; }
Mono::String* InternalCalls::MelonUtils::GetApplicationPath() { return Mono::Exports::mono_string_new(Mono::domain, Game::ApplicationPath); }
Mono::String* InternalCalls::MelonUtils::GetGamePackage() { return Mono::Exports::mono_string_new(Mono::domain, Game::Package); }
Mono::String* InternalCalls::MelonUtils::GetGameName() { return Mono::Exports::mono_string_new(Mono::domain, Game::Name); }
Mono::String* InternalCalls::MelonUtils::GetGameDeveloper() { return Mono::Exports::mono_string_new(Mono::domain, Game::Developer); }
Mono::String* InternalCalls::MelonUtils::GetGameDirectory() { return Mono::Exports::mono_string_new(Mono::domain, Game::BasePath); }
Mono::String* InternalCalls::MelonUtils::GetGameDataDirectory() { return Mono::Exports::mono_string_new(Mono::domain, Game::DataPath); }
Mono::String* InternalCalls::MelonUtils::GetUnityVersion() { return Mono::Exports::mono_string_new(Mono::domain, Game::UnityVersion); }
Mono::String* InternalCalls::MelonUtils::GetManagedDirectory() { return Mono::Exports::mono_string_new(Mono::domain, Mono::ManagedPath); }
Mono::String* InternalCalls::MelonUtils::GetMainAssemblyLoc() { return Mono::Exports::mono_string_new(Mono::domain, Il2Cpp::LibPath); }
#ifndef PORT_DISABLE
Mono::String* InternalCalls::MelonUtils::GetHashCode() { return Mono::Exports::mono_string_new(Mono::domain, HashCode::Hash.c_str()); }
#else 
Mono::String* InternalCalls::MelonUtils::GetHashCode() { return Mono::Exports::mono_string_new(Mono::domain, "Placeholder Hash"); }
#endif
void InternalCalls::MelonUtils::SCT(Mono::String* title)
{
#ifndef PORT_DISABLE
    if (title == NULL) return;
    auto str = Mono::Exports::mono_string_to_utf8(title);
    Console::SetTitle(str);
    Mono::Free(str);
#else 
    return;
#endif
}
Mono::String* InternalCalls::MelonUtils::GetFileProductName(Mono::String* filepath)
{
    char* filepathstr = Mono::Exports::mono_string_to_utf8(filepath);
    if (filepathstr == NULL)
        return NULL;
    const char* info = Core::GetFileInfoProductName(filepathstr);
    Mono::Free(filepathstr);
    if (info == NULL)
        return NULL;
    return Mono::Exports::mono_string_new(Mono::domain, info);
}

void InternalCalls::MelonUtils::GetStaticSettings(StaticSettings::Settings_t &settings)
{
    memcpy(&settings, &StaticSettings::Settings, sizeof(StaticSettings::Settings_t));
}

void InternalCalls::MelonUtils::AddInternalCalls()
{
    Mono::AddInternalCall("MelonLoader.MelonUtils::IsGame32Bit", (void*)IsGame32Bit);
    Mono::AddInternalCall("MelonLoader.MelonUtils::IsGameIl2Cpp", (void*)IsGameIl2Cpp);
    Mono::AddInternalCall("MelonLoader.MelonUtils::IsOldMono", (void*)IsOldMono);
    Mono::AddInternalCall("MelonLoader.MelonUtils::GetApplicationPath", (void*)GetApplicationPath);
    Mono::AddInternalCall("MelonLoader.MelonUtils::GetGameDataDirectory", (void*)GetGameDataDirectory);
    Mono::AddInternalCall("MelonLoader.MelonUtils::GetMainAssemblyLoc", (void*)GetMainAssemblyLoc);
    Mono::AddInternalCall("MelonLoader.MelonUtils::GetUnityVersion", (void*)GetUnityVersion);
    Mono::AddInternalCall("MelonLoader.MelonUtils::GetManagedDirectory", (void*)GetManagedDirectory);
    Mono::AddInternalCall("MelonLoader.MelonUtils::SetConsoleTitle", (void*)SCT);
    Mono::AddInternalCall("MelonLoader.MelonUtils::GetFileProductName", (void*)GetFileProductName);
    Mono::AddInternalCall("MelonLoader.MelonUtils::NativeHookAttach", (void*)Hook::Attach);
    Mono::AddInternalCall("MelonLoader.MelonUtils::NativeHookDetach", (void*)Hook::Detach);

    Mono::AddInternalCall("MelonLoader.MelonUtils::Internal_GetGamePackage", (void*)GetGamePackage);
    Mono::AddInternalCall("MelonLoader.MelonUtils::Internal_GetGameName", (void*)GetGameName);
    Mono::AddInternalCall("MelonLoader.MelonUtils::Internal_GetGameDeveloper", (void*)GetGameDeveloper);
    Mono::AddInternalCall("MelonLoader.MelonUtils::Internal_GetGameDirectory", (void*)GetGameDirectory);
    Mono::AddInternalCall("MelonLoader.MelonUtils::Internal_GetHashCode", (void*)GetHashCode);
    
    Mono::AddInternalCall("MelonLoader.MelonUtils::GetStaticSettings", (void*)GetStaticSettings);
}
#pragma endregion

#pragma region MelonDebug
void InternalCalls::MelonDebug::Internal_Msg(Console::Color meloncolor, Console::Color txtcolor, Mono::String* namesection, Mono::String* txt)
{
    auto nsStr = namesection != NULL ? Mono::Exports::mono_string_to_utf8(namesection) : NULL;
    auto txtStr = Mono::Exports::mono_string_to_utf8(txt);
    Debug::Internal_Msg(meloncolor, txtcolor, nsStr, txtStr);
    if (nsStr != NULL) Mono::Free(nsStr);
    Mono::Free(txtStr);
}
void InternalCalls::MelonDebug::AddInternalCalls()
{
    Mono::AddInternalCall("MelonLoader.MelonDebug::Internal_Msg", (void*)Internal_Msg);
}
#pragma endregion

#pragma region SupportModules
void InternalCalls::SupportModules::SetDefaultConsoleTitleWithGameName(Mono::String* GameVersion) { 
#ifndef PORT_DISABLE
    Console::SetDefaultTitleWithGameName(GameVersion != NULL ? Mono::Exports::mono_string_to_utf8(GameVersion) : NULL);
#endif
}
void InternalCalls::SupportModules::AddInternalCalls()
{
    Mono::AddInternalCall("MelonLoader.Support.Preload::GetManagedDirectory", (void*)MelonUtils::GetManagedDirectory);
    Mono::AddInternalCall("MelonLoader.Support.Main::SetDefaultConsoleTitleWithGameName", (void*)SetDefaultConsoleTitleWithGameName);
}
#pragma endregion

#pragma region UnhollowerIl2Cpp
void InternalCalls::UnhollowerIl2Cpp::AddInternalCalls()
{
    Mono::AddInternalCall("UnhollowerRuntimeLib.ClassInjector::GetProcAddress", (void*)GetProcAddress);
    Mono::AddInternalCall("UnhollowerRuntimeLib.ClassInjector::LoadLibrary", (void*)LoadLibrary);

    Mono::AddInternalCall("UnhollowerRuntimeLib.XrefScans.CSHelper::GetAsmLoc", (void*)GetAsmLoc);
    Mono::AddInternalCall("UnhollowerRuntimeLib.XrefScans.CSHelper::CleanupDisasm_Native", (void*)CleanupDisasm);

    Mono::AddInternalCall("UnhollowerRuntimeLib.XrefScans.XrefScanner::XrefScanImpl_Native", (void*)XrefScannerBindings::XrefScanner::XrefScanImplNative);

    Mono::AddInternalCall("UnhollowerRuntimeLib.XrefScans.XrefScannerLowLevel::JumpTargetsImpl_Native", (void*)XrefScannerBindings::XrefScannerLowLevel::JumpTargetsImpl);

    Mono::AddInternalCall("UnhollowerRuntimeLib.XrefScans.XrefScanUtilFinder::FindLastRcxReadAddressBeforeCallTo_Native", (void*)XrefScannerBindings::XrefScanUtilFinder::FindLastRcxReadAddressBeforeCallTo);
    Mono::AddInternalCall("UnhollowerRuntimeLib.XrefScans.XrefScanUtilFinder::FindByteWriteTargetRightAfterCallTo_Native", (void*)XrefScannerBindings::XrefScanUtilFinder::FindByteWriteTargetRightAfterCallTo);
}

void* InternalCalls::UnhollowerIl2Cpp::GetProcAddress(void* hModule, Mono::String* procName)
{
    char* parsedSym = Mono::Exports::mono_string_to_utf8(procName);
    void* res = dlsym(hModule, parsedSym);
    Mono::Free(parsedSym);
    return res;
}

void* InternalCalls::UnhollowerIl2Cpp::LoadLibrary(Mono::String* lpFileName)
{
    char* parsedLib = Mono::Exports::mono_string_to_utf8(lpFileName);
    //Debug::Msg(parsedLib);
    if (strcmp(parsedLib, "GameAssembly.dll") == 0
       || strcmp(parsedLib, "libil2cpp.so") == 0)
        return Il2Cpp::Handle;

    return dlopen(parsedLib, RTLD_NOW | RTLD_GLOBAL);
}

void* InternalCalls::UnhollowerIl2Cpp::GetAsmLoc()
{
    return Il2Cpp::MemLoc;
}

void InternalCalls::UnhollowerIl2Cpp::CleanupDisasm()
{
    Debug::Msg("CleanupDisasm not implemented");
}
#pragma endregion

#pragma region bHaptics

void InternalCalls::BHaptics::AddInternalCalls()
{
    // player
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::Start", (void*)BHapticsBridge::InternalCalls::start);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::Stop", (void*)BHapticsBridge::InternalCalls::stop);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::TurnOff", (void*)BHapticsBridge::InternalCalls::turnOff);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::TurnOffAll", (void*)BHapticsBridge::InternalCalls::turnOffAll);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::RegisterProject", (void*)BHapticsBridge::InternalCalls::registerProject);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::RegisterProjectReflected", (void*)BHapticsBridge::InternalCalls::registerProjectReflected);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::SubmitRegistered", (void*)BHapticsBridge::InternalCalls::submitRegisteredWithOption);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::SubmitRegisteredWithTime", (void*)BHapticsBridge::InternalCalls::submitRegisteredWithTime);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::IsRegistered", (void*)BHapticsBridge::InternalCalls::isRegistered);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::IsPlaying", (void*)BHapticsBridge::InternalCalls::isPlaying);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::IsAnythingPlaying", (void*)BHapticsBridge::InternalCalls::isAnythingPlaying);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::Internal_SubmitDot", (void*)BHapticsBridge::InternalCalls::submitDotArray);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::Internal_SubmitPath", (void*)BHapticsBridge::InternalCalls::submitPathArray);
    Mono::AddInternalCall("MelonLoader.bHaptics_NativeLibrary::Internal_GetPositionStatus", (void*)BHapticsBridge::InternalCalls::getPositionStatus);
}

#pragma endregion

Mono::String* InternalCalls::GetLocalTimeZone()
{
    auto env = Core::GetEnv();

    jclass jCore = env->FindClass("java/util/TimeZone");
    if (jCore == NULL)
    {
        Logger::QuickLog("Failed to find class java.util.TimeZone", LogType::Error);
        return Mono::Exports::mono_string_new(Mono::domain, "");
    }

    jmethodID mid = env->GetStaticMethodID(jCore, "getDefault", "()Ljava/util/TimeZone;");
    if (mid == NULL)
    {
        Logger::QuickLog("Failed to find method java.util.TimeZone.getDefault()", LogType::Error);
        return Mono::Exports::mono_string_new(Mono::domain, "");
    }

    jobject jObj = env->CallStaticObjectMethod(jCore, mid);
    if (jObj == NULL)
    {
        Logger::QuickLog("Failed to invoke java.util.TimeZone.getDefault()", LogType::Error);
        return Mono::Exports::mono_string_new(Mono::domain, "");
    }

    jmethodID mid2 = env->GetMethodID(jCore, "getID", "()Ljava/lang/String;");
    if (mid2 == NULL)
    {
        Logger::QuickLog("Failed to find method java.util.TimeZone.getID()", LogType::Error);
        return Mono::Exports::mono_string_new(Mono::domain, "");
    }

    jobject jObj2 = env->CallObjectMethod(jObj, mid2);
    if (jObj2 == NULL)
    {
        Logger::QuickLog("Failed to invoke java.util.TimeZone.getId()", LogType::Error);
        return Mono::Exports::mono_string_new(Mono::domain, "");
    }

    jstring jStr = (jstring)jObj2;
    const jclass stringClass = env->GetObjectClass(jStr);
    const jmethodID getBytes = env->GetMethodID(stringClass, "getBytes", "(Ljava/lang/String;)[B");
    const jbyteArray stringJbytes = (jbyteArray) env->CallObjectMethod(jStr, getBytes, env->NewStringUTF("UTF-8"));

    size_t length = (size_t) env->GetArrayLength(stringJbytes);
    jbyte* pBytes = env->GetByteArrayElements(stringJbytes, NULL);

    std::string ret = std::string((char *)pBytes, length);
    env->ReleaseByteArrayElements(stringJbytes, pBytes, JNI_ABORT);

    env->DeleteLocalRef(stringJbytes);
    env->DeleteLocalRef(stringClass);

    return Mono::Exports::mono_string_new(Mono::domain, ret.c_str());
}
