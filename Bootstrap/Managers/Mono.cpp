#include <string>
#include "Mono.h"
#include "Game.h"
#include "Hook.h"
#include "../Utils/Assertion.h"
#include "../Utils/Console/CommandLine.h"
#include "../Utils/Console/Debug.h"
#include "../Core.h"
#include "InternalCalls.h"
#include "BaseAssembly.h"
#include "Il2Cpp.h"
#include "../Utils/AnalyticsBlocker.h"
#include "../Utils/Console/Logger.h"
#include "../Utils/Helpers/ImportLibHelper.h"

#ifdef __ANDROID__
#include <dlfcn.h>
#include <stdio.h>
#include <sys/stat.h>
#endif

#ifdef _WIN32
const char* Mono::LibNames[] = { "mono", "mono-2.0-bdwgc", "mono-2.0-sgen", "mono-2.0-boehm" };
#elif defined(__ANDROID__)
const char* Mono::LibNames[] = { "monosgen-2.0" };
jclass Mono::jMonoDroidHelper = NULL;
jmethodID Mono::jLoadApplication = NULL;
#endif

const char* Mono::FolderNames[] = { "Mono", "MonoBleedingEdge", "MonoBleedingEdge.x86", "MonoBleedingEdge.x64" };
const char* Mono::PosixHelperName = "MonoPosixHelper";
char* Mono::BasePath = NULL;
char* Mono::ManagedPath = NULL;
char* Mono::ManagedPathMono = NULL;
char* Mono::ConfigPath = NULL;
char* Mono::ConfigPathMono = NULL;
char* Mono::MonoConfigPathMono = NULL;
Mono::Domain* Mono::domain = NULL;
bool Mono::IsOldMono = false;

#ifdef _WIN32
HMODULE Mono::Module = NULL;
HMODULE Mono::PosixHelper = NULL;
#elif defined(__ANDROID__)
void* Mono::Module = NULL;
void* Mono::PosixHelper = NULL;

// Patcher* Mono::Patches::mono_unity_get_unitytls_interface = NULL;
#endif

#pragma region MonoDeclare
#define MONODEF(fn) Mono::Exports::fn##_t Mono::Exports::fn = NULL;

MONODEF(mono_jit_init)
MONODEF(mono_jit_init_version)
MONODEF(mono_set_assemblies_path)
MONODEF(mono_assembly_setrootdir)
MONODEF(mono_set_config_dir)
MONODEF(mono_runtime_set_main_args)
MONODEF(mono_thread_set_main)
MONODEF(mono_thread_current)
MONODEF(mono_domain_set_config)
MONODEF(mono_add_internal_call)
MONODEF(mono_lookup_internal_call)
MONODEF(mono_runtime_invoke)
MONODEF(mono_method_get_name)
// MONODEF(mono_unity_get_unitytls_interface)
MONODEF(mono_domain_assembly_open)
MONODEF(mono_assembly_get_image)
MONODEF(mono_class_from_name)
MONODEF(mono_class_get_method_from_name)
MONODEF(mono_string_to_utf8)
MONODEF(mono_string_new)
MONODEF(mono_object_get_class)
MONODEF(mono_property_get_get_method)
MONODEF(mono_free)
MONODEF(g_free)

MONODEF(mono_raise_exception)
MONODEF(mono_get_exception_bad_image_format)
MONODEF(mono_image_open_full)
MONODEF(mono_image_open_from_data_full)
MONODEF(mono_image_close)
MONODEF(mono_image_get_table_rows)
MONODEF(mono_metadata_decode_table_row_col)
MONODEF(mono_array_addr_with_size)
MONODEF(mono_array_length)
MONODEF(mono_metadata_string_heap)
MONODEF(mono_class_get_name)

MONODEF(mono_trace_set_level_string)
MONODEF(mono_trace_set_mask_string)
MONODEF(mono_trace_set_log_handler)
MONODEF(mono_trace_set_print_handler)
MONODEF(mono_trace_set_printerr_handler)
MONODEF(mono_install_unhandled_exception_hook)
MONODEF(mono_print_unhandled_exception)
MONODEF(mono_dllmap_insert)

MONODEF(mono_domain_get)
MONODEF(mono_melonloader_set_thread_checker)
MONODEF(mono_thread_attach)
MONODEF(mono_melonloader_thread_suspend_reload)

#undef MONODEF
#pragma endregion MonoDeclare

bool Mono::Initialize()
{
	Debug::Msg("Initializing Mono...");

	if (!SetupPaths())
	{
		Assertion::ThrowInternalFailure("Failed to Setup Mono Paths!");
		return false;
	}

//	jMonoDroidHelper = ::Core::Env->FindClass("com/melonloader/helpers/MonoDroidHelper");
//	if (jMonoDroidHelper == NULL)
//	{
//		Assertion::ThrowInternalFailure("Cannot find com/melonloader/helpers/MonoDroidHelper");
//		return false;
//	}
	
	Debug::Msg(("Mono::BasePath = " + std::string(BasePath)).c_str());
	Debug::Msg(("Mono::ManagedPath = " + std::string(ManagedPath)).c_str());
	Debug::Msg(("Mono::ConfigPath = " + std::string(ConfigPath)).c_str());
	IsOldMono = Core::FileExists((std::string(BasePath) + "\\mono.dll").c_str());
	Debug::Msg(("Mono::IsOldMono = " + std::string((IsOldMono ? "true" : "false"))).c_str());
	
	return true;
}

bool Mono::Load()
{
	for (int i = 0; i < (sizeof(LibNames) / sizeof(LibNames[0])); i++)
	{
#ifdef _WIN32
		Module = LoadLibraryA((std::string(BasePath) + "\\" + LibNames[i] + ".dll").c_str());
#elif defined (__ANDROID__)
		Debug::Msg((std::string("trying: ") + std::string("lib") + std::string(LibNames[i]) + ".so").c_str());
		Mono::Module = dlopen((std::string("lib") + std::string(LibNames[i]) + ".so").c_str(), RTLD_NOW & RTLD_NODELETE & RTLD_GLOBAL);
#endif
		
		if (Module != NULL)
		{
#ifndef __ANDROID__
			if (i == 0)
				IsOldMono = true;
#endif
			break;
		}
	}
	
	if (Module == NULL)
	{
		Assertion::ThrowInternalFailure("Failed to Load Mono Library!");
		return false;
	}

#ifdef _WIN32
	PosixHelper = LoadLibraryA((std::string(BasePath) + "\\" + Mono::PosixHelperName + ".dll").c_str());
#endif

#ifdef _WIN32
	if ((PosixHelper == NULL) && !IsOldMono)
	{
		Assertion::ThrowInternalFailure("Failed to Load Mono Posix Helper!");
		return false;
	}
#endif

	// const char* extraLibs[] = {
	// 	"libmonodroid.so"
	// };
	//
	// for (size_t i = 0; i < sizeof(extraLibs) / sizeof(extraLibs[0]); i++)
	// {
	// 	void* lib = dlopen(extraLibs[i], RTLD_NOW & RTLD_NODELETE & RTLD_GLOBAL);
	// 	if (lib == NULL)
	// 	{
	// 		Logger::Errorf("Failed to load %s", extraLibs[i]);
	// 		Assertion::ThrowInternalFailure("Failed to load library!");
	// 		return false;
	// 	}
	// }

	return Exports::Initialize();
}

bool Mono::SetupPaths()
{
#ifdef _WIN32
	std::string MonoDir = std::string();
	for (int i = 0; i < (sizeof(FolderNames) / sizeof(FolderNames[0])); i++)
	{
		if (Game::IsIl2Cpp)
		{
			std::string str_melon = (std::string(Game::BasePath) + "\\MelonLoader\\Dependencies\\" + FolderNames[i]);
			if (Core::DirectoryExists(str_melon.c_str()))
			{
				MonoDir = str_melon;
				break;
			}
		}
		else
		{
			std::string str_base = (std::string(Game::BasePath) + "\\" + FolderNames[i]);
			if (Core::DirectoryExists(str_base.c_str()))
			{
				MonoDir = str_base;
				break;
			}
			std::string str_data = (std::string(Game::DataPath) + "\\" + FolderNames[i]);
			if (Core::DirectoryExists(str_data.c_str()))
			{
				MonoDir = str_data;
				break;
			}
		}
	}
	if (MonoDir.empty())
	{
		Assertion::ThrowInternalFailure("Failed to Find Mono Directory!");
		return false;
	}

	if (Game::IsIl2Cpp)
	{
		BasePath = new char[MonoDir.size() + 1];
		std::copy(MonoDir.begin(), MonoDir.end(), BasePath);
		BasePath[MonoDir.size()] = '\0';

		std::string ManagedPathStr = (std::string(Game::BasePath) + "\\MelonLoader\\Managed");
		ManagedPath = new char[ManagedPathStr.size() + 1];
		std::copy(ManagedPathStr.begin(), ManagedPathStr.end(), ManagedPath);
		ManagedPath[ManagedPathStr.size()] = '\0';

		std::string ConfigPathStr = (std::string(Game::DataPath) + "\\il2cpp_data\\etc");
		ConfigPath = new char[ConfigPathStr.size() + 1];
		std::copy(ConfigPathStr.begin(), ConfigPathStr.end(), ConfigPath);
		ConfigPath[ConfigPathStr.size()] = '\0';

		return true;
	}

	std::string BasePathStr = (MonoDir + "\\EmbedRuntime");
	if (!Core::DirectoryExists(BasePathStr.c_str()))
		BasePathStr = MonoDir;
	BasePath = new char[BasePathStr.size() + 1];
	std::copy(BasePathStr.begin(), BasePathStr.end(), BasePath);
	BasePath[BasePathStr.size()] = '\0';

	std::string ManagedPathStr = (std::string(Game::DataPath) + "\\Managed");
	ManagedPath = new char[ManagedPathStr.size() + 1];
	std::copy(ManagedPathStr.begin(), ManagedPathStr.end(), ManagedPath);
	ManagedPath[ManagedPathStr.size()] = '\0';

	std::string ConfigPathStr = (MonoDir + "\\etc");
	ConfigPath = new char[ConfigPathStr.size() + 1];
	std::copy(ConfigPathStr.begin(), ConfigPathStr.end(), ConfigPath);
	ConfigPath[ConfigPathStr.size()] = '\0';
#elif defined(__ANDROID__)
	if (Game::IsIl2Cpp)
	{
		std::string BasePathStr = (std::string(Game::BasePath) + "/melonloader/etc").c_str();
		BasePath = new char[BasePathStr.size() + 1];
		std::copy(BasePathStr.begin(), BasePathStr.end(), BasePath);
		BasePath[BasePathStr.size()] = '\0';

		std::string ManagedPathStr = (std::string(Mono::BasePath) + "/managed").c_str();
		ManagedPath = new char[ManagedPathStr.size() + 1];
		std::copy(ManagedPathStr.begin(), ManagedPathStr.end(), ManagedPath);
		ManagedPath[ManagedPathStr.size()] = '\0';
		
		std::string ConfigPathStr = (std::string(Game::DataPath) + "/il2cpp/etc").c_str();
		ConfigPath = new char[ConfigPathStr.size() + 1];
		std::copy(ConfigPathStr.begin(), ConfigPathStr.end(), ConfigPath);
		ConfigPath[ConfigPathStr.size()] = '\0';

		// TODO: REMOVE
		std::string BaseAsmPathStr = (BasePathStr + "/MelonLoader.dll").c_str();
		BaseAssembly::PathMono = new char[BaseAsmPathStr.size() + 1];
		std::copy(BaseAsmPathStr.begin(), BaseAsmPathStr.end(), BaseAssembly::PathMono);
		BaseAssembly::PathMono[BaseAsmPathStr.size()] = '\0';

		std::string PreloadPathStr = (BasePathStr + "/support/Preload.dll").c_str();
		BaseAssembly::PreloadPath = new char[PreloadPathStr.size() + 1];
		std::copy(PreloadPathStr.begin(), PreloadPathStr.end(), BaseAssembly::PreloadPath);
		BaseAssembly::PreloadPath[PreloadPathStr.size()] = '\0';

		return true;
	}

	Assertion::ThrowInternalFailure("(mono) NOT IMPLEMENTED");
	return false;
#endif
	
	return true;
}

void Mono::CreateDomain(const char* name)
{
	if (domain != NULL)
		return;

	Debug::Msg("Creating Mono Domain...");

	Exports::mono_set_assemblies_path(ManagedPath);
	Exports::mono_assembly_setrootdir(ManagedPath);

#ifdef __ANDROID__
//	Debug::Msg("Initializing mono via JNI");
//	if (!InitMonoJNI())
//	{
//		Assertion::ThrowInternalFailure("Failed to initialize mono in JNI");
//		Core::KillCurrentProcess();
//		return; // lol
//	}
#endif

#ifndef PORT_DISABLE
	if (!IsOldMono)
		Exports::mono_runtime_set_main_args(CommandLine::argc, CommandLine::argv);
#endif

#ifdef __ANDROID__
//	Debug::Msg("Finding JIT domain");
//	domain = Exports::mono_domain_get();
//	if (domain == NULL)
//	{
//		Assertion::ThrowInternalFailure("Failed to find mono domain");
//		Core::KillCurrentProcess();
//		return; // lol
//	}
    domain = Exports::mono_jit_init(name);
    Exports::mono_thread_set_main(Exports::mono_thread_current());
//    Exports::mono_thread_a
#else
	Debug::Msg("Jit init");
	domain = Exports::mono_jit_init(name);

	Debug::Msg("set thread");
	Exports::mono_thread_set_main(Exports::mono_thread_current());
#endif

// not needed in newer version of mono
#ifdef _WIN32
	if (!IsOldMono)
		Exports::mono_domain_set_config(domain, Game::BasePath, name);
#endif
}

void Mono::AddInternalCall(const char* name, void* method)
{
    Debug::Msgf("Adding: %s", name);
	Exports::mono_add_internal_call(name, method);
}

void Mono::Free(void* ptr)
{
	if (IsOldMono)
		Exports::g_free(ptr);
	else
		Exports::mono_free(ptr);
}

bool Mono::Exports::Initialize()
{
	Debug::Msg("Initializing Mono Exports...");
	
#pragma region MonoBind
#define MONODEF(fn) fn = (fn##_t) ImportLibHelper::GetExport(Module, #fn);

		MONODEF(mono_jit_init)
		MONODEF(mono_thread_set_main)
		MONODEF(mono_thread_current)
		MONODEF(mono_add_internal_call)
		MONODEF(mono_lookup_internal_call)
		MONODEF(mono_runtime_invoke)
		MONODEF(mono_method_get_name)
		MONODEF(mono_domain_assembly_open)
		MONODEF(mono_assembly_get_image)
		// MONODEF(mono_class_from_name_checked)
		MONODEF(mono_class_from_name)
		MONODEF(mono_class_get_method_from_name)
		MONODEF(mono_string_to_utf8)
		MONODEF(mono_string_new)
		MONODEF(mono_object_get_class)
		MONODEF(mono_property_get_get_method)

		MONODEF(mono_trace_set_level_string)
		MONODEF(mono_trace_set_mask_string)
		MONODEF(mono_trace_set_log_handler)
		MONODEF(mono_trace_set_print_handler)
		MONODEF(mono_trace_set_printerr_handler)
		MONODEF(mono_install_unhandled_exception_hook)
		MONODEF(mono_print_unhandled_exception)
		MONODEF(mono_dllmap_insert)
	
		MONODEF(mono_domain_get)


        MONODEF(mono_melonloader_set_thread_checker)
        MONODEF(mono_thread_attach)
        MONODEF(mono_melonloader_thread_suspend_reload)


		if (!IsOldMono)
		{
			MONODEF(mono_domain_set_config)
				// MONODEF(mono_unity_get_unitytls_interface)
				MONODEF(mono_free)
		}
		else
			MONODEF(g_free)

			if (Game::IsIl2Cpp)
			{
				MONODEF(mono_set_assemblies_path)
					MONODEF(mono_assembly_setrootdir)
					MONODEF(mono_set_config_dir)

					if (!IsOldMono)
						MONODEF(mono_runtime_set_main_args)

						MONODEF(mono_raise_exception)
						MONODEF(mono_get_exception_bad_image_format)
						MONODEF(mono_image_open_full)
						MONODEF(mono_image_open_from_data_full)
						MONODEF(mono_image_close)
						MONODEF(mono_image_get_table_rows)
						MONODEF(mono_metadata_decode_table_row_col)
						MONODEF(mono_array_addr_with_size)
						MONODEF(mono_array_length)
						MONODEF(mono_metadata_string_heap)
						MONODEF(mono_class_get_name)
			}
			else
				MONODEF(mono_jit_init_version)

#undef MONODEF
#pragma endregion MonoBind

	Debug::Msg("Binding Native Lib...");

    mono_dllmap_insert(NULL, "System.Native", NULL, "libmono-native.so", NULL);
    mono_dllmap_insert(NULL, "System.Net.Security.Native", NULL, "libmono-native.so", NULL);

	mono_dllmap_insert(NULL, "GameAssembly", NULL, "libil2cpp.so", NULL);
	mono_dllmap_insert(NULL, "haptic_library", NULL, "libbhaptics-native-lib.so", NULL);

	return Assertion::ShouldContinue;
}

void Mono::LogException(Mono::Object* exceptionObject, bool shouldThrow)
{
	Hooks::mono_unhandled_exception((Object*)exceptionObject, NULL);
}

#ifdef __ANDROID__
bool Mono::ApplyPatches()
{
	Debug::Msg("Applying patches for Mono");

	if (Debug::Enabled)
	{
		Exports::mono_trace_set_level_string("warning");
		Exports::mono_trace_set_mask_string("all");
		Debug::Msg("Enabled Mono Logging");
	}
	
//	Exports::mono_trace_set_log_handler(&Hooks::mono_log, NULL);
//	Exports::mono_trace_set_print_handler(&Hooks::mono_print);
//	Exports::mono_trace_set_printerr_handler(&Hooks::mono_printerr);

	Exports::mono_install_unhandled_exception_hook(&Hooks::mono_unhandled_exception, NULL);

	Debug::Msg("Defined Mono Handlers");

	// Patches::mono_unity_get_unitytls_interface = new Patcher((void**)&Exports::mono_unity_get_unitytls_interface, (void*)Hooks::mono_unity_get_unitytls_interface);

	// Patches::mono_unity_get_unitytls_interface->ApplyPatch();

    Exports::mono_melonloader_set_thread_checker(CheckThread);

	return true;
}

bool Mono::CheckPaths()
{
	char* Paths[] = {
		BasePath,
		ManagedPath,
		ConfigPath
	};

	for (auto& path : Paths)
	{
		if (!Core::DirectoryExists(path))
		{
			Logger::QuickLogf("Failed to load path (%s) because it doesn't exist.  Please restart the game after it loads.", LogType::Error, path);
			return false;
		}
	}

	return true;
}

bool Mono::InitMonoJNI()
{
	JNIEnv* env = Core::Env;
	Core::Bootstrap->AttachCurrentThread(&env, NULL);

	if (env == NULL)
	{
		Logger::QuickLog("Failed to attach ENV", LogType::Error);
		return false;
	}

	jclass jMonoDroidHelper = env->FindClass("com/melonloader/helpers/MonoDroidHelper");
	if (jMonoDroidHelper == NULL)
	{
		Assertion::ThrowInternalFailure("Cannot find com/melonloader/helpers/MonoDroidHelper");
		return false;
	}
	
	jmethodID jLoadApplication = env->GetStaticMethodID(jMonoDroidHelper, "LoadApplication", "()V");
	if (jLoadApplication == NULL)
	{
		Logger::QuickLog("Failed to find Method LoadApplication()V", LogType::Error);
		return false;
	}
	
	Debug::Msg("Starting Mono JNI");
	env->CallStaticVoidMethod(jMonoDroidHelper, jLoadApplication);
	return true;
}

bool Mono::CheckThread(pthread_t tid) {
    Debug::Msgf("checking: %p", tid);

    size_t size;
    auto threads = Il2Cpp::Exports::il2cpp_thread_get_all_attached_threads(&size);

    for (size_t i = 0; i < size; i++) {
        Debug::Msgf("Il2Cpp Thread: [%p]", threads[i]->internal_thread->tid);
        if (threads[i]->internal_thread->tid == tid)
            return false;
    }

    return true;
}

#endif

#pragma region Hooks

void* Mono::Hooks::mono_unity_get_unitytls_interface() { return Il2Cpp::UnityTLSInterfaceStruct; }

void Mono::Hooks::mono_print(const char* string, mono_bool is_stdout)
{
	if (is_stdout)
	{
		if (Debug::Enabled)
			Debug::Msg("Skip writing STDOUT msg");

		return;
	}

	Logger::Internal_Msg(Console::Color::Gray, Console::Color::Gray, "Mono", string);
}

void Mono::Hooks::mono_printerr(const char* string, mono_bool is_stdout)
{
	if (is_stdout)
	{
		if (Debug::Enabled)
			Debug::Msg("Skip writing STDOUT msg");

		return;
	}

	Logger::Internal_Msg(Console::Color::Red, Console::Color::Red, "Mono Error", string);
}

void Mono::Hooks::mono_log(const char* log_domain, const char* log_level, const char* message, mono_bool fatal, void* user_data)
{
	Console::Color color = fatal ? Console::Gray : Console::Red;
	//Logger::Internal_DirectWrite(Console::Color::Gray, fatal ? LogLevel::Error : LogLevel::Info, prefixes, sizeof(prefixes) / sizeof(prefixes[0]), message);
    Logger::Internal_Msg(color, color, "Mono", message);
}

void Mono::Hooks::mono_unhandled_exception(Object* exceptionObject, void* user_data)
{
	if (exceptionObject == NULL)
		return;

	Exports::mono_print_unhandled_exception(exceptionObject);
}

#ifdef _WIN32
Mono::Domain* Mono::Hooks::mono_jit_init_version(const char* name, const char* version)
{
	if (!Debug::Enabled)
		Console::SetHandles();
	
	Debug::Msg("Detaching Hook from mono_jit_init_version...");
	Hook::Detach(&(LPVOID&)Exports::mono_jit_init_version, mono_jit_init_version);
	
	Debug::Msg("Creating Mono Domain...");
	domain = Exports::mono_jit_init_version(name, version);
	Exports::mono_thread_set_main(Exports::mono_thread_current());
	if (!IsOldMono)
		Exports::mono_domain_set_config(domain, Game::BasePath, name);
	InternalCalls::Initialize();
	if (BaseAssembly::Initialize())
	{
		Debug::Msg("Attaching Hook to mono_runtime_invoke...");
		Hook::Attach(&(LPVOID&)Exports::mono_runtime_invoke, mono_runtime_invoke);
	}
	return domain;
}

Mono::Object* Mono::Hooks::mono_runtime_invoke(Method* method, Object* obj, void** params, Object** exec)
{
	const char* method_name = Exports::mono_method_get_name(method);
	if ((strstr(method_name, "Internal_ActiveSceneChanged") != NULL) || (strstr(method_name, "UnityEngine.ISerializationCallbackReceiver.OnAfterDeserialize") != NULL))
	{
		Debug::Msg("Detaching Hook from mono_runtime_invoke...");
		Hook::Detach(&(LPVOID&)Exports::mono_runtime_invoke, mono_runtime_invoke);
		BaseAssembly::Start();
	}
	return Exports::mono_runtime_invoke(method, obj, params, exec);
}
#endif
#pragma endregion Hooks