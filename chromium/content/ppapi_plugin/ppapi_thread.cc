// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_thread.h"

#include <stddef.h>

#include <limits>

#include "base/command_line.h"
#include "base/cpu.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/child/browser_font_resource_trusted.h"
#include "content/child/child_discardable_shared_memory_manager.h"
#include "content/child/child_process.h"
#include "content/common/child_process_messages.h"
#include "content/common/sandbox_util.h"
#include "content/ppapi_plugin/broker_process_dispatcher.h"
#include "content/ppapi_plugin/plugin_process_dispatcher.h"
#include "content/ppapi_plugin/ppapi_blink_platform_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/sandbox_init.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ppapi/c/dev/ppp_network_state_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/interface_list.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "content/child/font_warmup_win.h"
#include "sandbox/win/src/sandbox.h"
#elif defined(OS_MACOSX)
#include "content/common/sandbox_init_mac.h"
#endif

#if defined(OS_WIN)
const char kWidevineCdmAdapterFileName[] = "widevinecdmadapter.dll";

extern sandbox::TargetServices* g_target_services;

// Used by EnumSystemLocales for warming up.
static BOOL CALLBACK EnumLocalesProc(LPTSTR lpLocaleString) {
  return TRUE;
}

static BOOL CALLBACK EnumLocalesProcEx(
    LPWSTR lpLocaleString,
    DWORD dwFlags,
    LPARAM lParam) {
  return TRUE;
}

// Warm up language subsystems before the sandbox is turned on.
static void WarmupWindowsLocales(const ppapi::PpapiPermissions& permissions) {
  ::GetUserDefaultLangID();
  ::GetUserDefaultLCID();

  if (permissions.HasPermission(ppapi::PERMISSION_FLASH)) {
    if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
      typedef BOOL (WINAPI *PfnEnumSystemLocalesEx)
          (LOCALE_ENUMPROCEX, DWORD, LPARAM, LPVOID);

      HMODULE handle_kern32 = GetModuleHandleW(L"Kernel32.dll");
      PfnEnumSystemLocalesEx enum_sys_locales_ex =
          reinterpret_cast<PfnEnumSystemLocalesEx>
              (GetProcAddress(handle_kern32, "EnumSystemLocalesEx"));

      enum_sys_locales_ex(EnumLocalesProcEx, LOCALE_WINDOWS, 0, 0);
    } else {
      EnumSystemLocalesW(EnumLocalesProc, LCID_INSTALLED);
    }
  }
}

#endif

namespace content {

typedef int32_t (*InitializeBrokerFunc)
    (PP_ConnectInstance_Func* connect_instance_func);

PpapiThread::PpapiThread(const base::CommandLine& command_line, bool is_broker)
    : is_broker_(is_broker),
      plugin_globals_(GetIOTaskRunner()),
      connect_instance_func_(NULL),
      local_pp_module_(base::RandInt(0, std::numeric_limits<PP_Module>::max())),
      next_plugin_dispatcher_id_(1) {
  plugin_globals_.SetPluginProxyDelegate(this);
  plugin_globals_.set_command_line(
      command_line.GetSwitchValueASCII(switches::kPpapiFlashArgs));

  blink_platform_impl_.reset(new PpapiBlinkPlatformImpl);
  blink::Platform::initialize(blink_platform_impl_.get());

  if (!is_broker_) {
    scoped_refptr<ppapi::proxy::PluginMessageFilter> plugin_filter(
        new ppapi::proxy::PluginMessageFilter(
            NULL, plugin_globals_.resource_reply_thread_registrar()));
    channel()->AddFilter(plugin_filter.get());
    plugin_globals_.RegisterResourceMessageFilters(plugin_filter.get());
  }

  // In single process, browser main loop set up the discardable memory
  // allocator.
  if (!command_line.HasSwitch(switches::kSingleProcess)) {
    base::DiscardableMemoryAllocator::SetInstance(
        ChildThreadImpl::discardable_shared_memory_manager());
  }
}

PpapiThread::~PpapiThread() {
}

void PpapiThread::Shutdown() {
  ChildThreadImpl::Shutdown();

  ppapi::proxy::PluginGlobals::Get()->ResetPluginProxyDelegate();
  if (plugin_entry_points_.shutdown_module)
    plugin_entry_points_.shutdown_module();
  blink_platform_impl_->Shutdown();
  blink::Platform::shutdown();
}

bool PpapiThread::Send(IPC::Message* msg) {
  // Allow access from multiple threads.
  if (base::MessageLoop::current() == message_loop())
    return ChildThreadImpl::Send(msg);

  return sync_message_filter()->Send(msg);
}

// Note that this function is called only for messages from the channel to the
// browser process. Messages from the renderer process are sent via a different
// channel that ends up at Dispatcher::OnMessageReceived.
bool PpapiThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiThread, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_LoadPlugin, OnLoadPlugin)
    IPC_MESSAGE_HANDLER(PpapiMsg_CreateChannel, OnCreateChannel)
    IPC_MESSAGE_HANDLER(PpapiMsg_SetNetworkState, OnSetNetworkState)
    IPC_MESSAGE_HANDLER(PpapiMsg_Crash, OnCrash)
    IPC_MESSAGE_HANDLER(PpapiMsg_Hang, OnHang)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PpapiThread::OnChannelConnected(int32_t peer_pid) {
  ChildThreadImpl::OnChannelConnected(peer_pid);
#if defined(OS_WIN)
  if (is_broker_)
    peer_handle_.Set(::OpenProcess(PROCESS_DUP_HANDLE, FALSE, peer_pid));
#endif
}

base::SingleThreadTaskRunner* PpapiThread::GetIPCTaskRunner() {
  return ChildProcess::current()->io_task_runner();
}

base::WaitableEvent* PpapiThread::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

IPC::PlatformFileForTransit PpapiThread::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId peer_pid,
    bool should_close_source) {
#if defined(OS_WIN)
  if (peer_handle_.IsValid()) {
    DCHECK(is_broker_);
    return IPC::GetPlatformFileForTransit(handle, should_close_source);
  }
#endif

  DCHECK(peer_pid != base::kNullProcessId);
  return BrokerGetFileHandleForProcess(handle, peer_pid, should_close_source);
}

base::SharedMemoryHandle PpapiThread::ShareSharedMemoryHandleWithRemote(
    const base::SharedMemoryHandle& handle,
    base::ProcessId remote_pid) {
  DCHECK(remote_pid != base::kNullProcessId);
  return base::SharedMemory::DuplicateHandle(handle);
}

std::set<PP_Instance>* PpapiThread::GetGloballySeenInstanceIDSet() {
  return &globally_seen_instance_ids_;
}

IPC::Sender* PpapiThread::GetBrowserSender() {
  return this;
}

std::string PpapiThread::GetUILanguage() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->GetSwitchValueASCII(switches::kLang);
}

void PpapiThread::PreCacheFontForFlash(const void* logfontw) {
#if defined(OS_WIN)
  ChildThreadImpl::PreCacheFont(*static_cast<const LOGFONTW*>(logfontw));
#endif
}

void PpapiThread::SetActiveURL(const std::string& url) {
  GetContentClient()->SetActiveURL(GURL(url));
}

PP_Resource PpapiThread::CreateBrowserFont(
    ppapi::proxy::Connection connection,
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description& desc,
    const ppapi::Preferences& prefs) {
  if (!BrowserFontResource_Trusted::IsPPFontDescriptionValid(desc))
    return 0;
  return (new BrowserFontResource_Trusted(
        connection, instance, desc, prefs))->GetReference();
}

uint32_t PpapiThread::Register(
    ppapi::proxy::PluginDispatcher* plugin_dispatcher) {
  if (!plugin_dispatcher ||
      plugin_dispatchers_.size() >= std::numeric_limits<uint32_t>::max()) {
    return 0;
  }

  uint32_t id = 0;
  do {
    // Although it is unlikely, make sure that we won't cause any trouble when
    // the counter overflows.
    id = next_plugin_dispatcher_id_++;
  } while (id == 0 ||
           plugin_dispatchers_.find(id) != plugin_dispatchers_.end());
  plugin_dispatchers_[id] = plugin_dispatcher;
  return id;
}

void PpapiThread::Unregister(uint32_t plugin_dispatcher_id) {
  plugin_dispatchers_.erase(plugin_dispatcher_id);
}

void PpapiThread::OnLoadPlugin(const base::FilePath& path,
                               const ppapi::PpapiPermissions& permissions) {
  // In case of crashes, the crash dump doesn't indicate which plugin
  // it came from.
  base::debug::SetCrashKeyValue("ppapi_path", path.MaybeAsASCII());

  SavePluginName(path);

  // This must be set before calling into the plugin so it can get the
  // interfaces it has permission for.
  ppapi::proxy::InterfaceList::SetProcessGlobalPermissions(permissions);
  permissions_ = permissions;

  // Trusted Pepper plugins may be "internal", i.e. built-in to the browser
  // binary.  If we're being asked to load such a plugin (e.g. the Chromoting
  // client) then fetch the entry points from the embedder, rather than a DLL.
  std::vector<PepperPluginInfo> plugins;
  GetContentClient()->AddPepperPlugins(&plugins);
  for (size_t i = 0; i < plugins.size(); ++i) {
    if (plugins[i].is_internal && plugins[i].path == path) {
      // An internal plugin is being loaded, so fetch the entry points.
      plugin_entry_points_ = plugins[i].internal_entry_points;
    }
  }

  // If the plugin isn't internal then load it from |path|.
  base::ScopedNativeLibrary library;
  if (plugin_entry_points_.initialize_module == NULL) {
    // Load the plugin from the specified library.
    base::NativeLibraryLoadError error;
    base::TimeDelta load_time;
    {
      TRACE_EVENT1("ppapi", "PpapiThread::LoadPlugin", "path",
                   path.MaybeAsASCII());

      base::TimeTicks start = base::TimeTicks::Now();
      library.Reset(base::LoadNativeLibrary(path, &error));
      load_time = base::TimeTicks::Now() - start;
    }

    if (!library.is_valid()) {
      LOG(ERROR) << "Failed to load Pepper module from " << path.value()
                 << " (error: " << error.ToString() << ")";
      if (!base::PathExists(path)) {
        ReportLoadResult(path, FILE_MISSING);
        return;
      }
      ReportLoadResult(path, LOAD_FAILED);
      // Report detailed reason for load failure.
      ReportLoadErrorCode(path, error);
      return;
    }

    // Only report load time for success loads.
    ReportLoadTime(path, load_time);

    // Get the GetInterface function (required).
    plugin_entry_points_.get_interface =
        reinterpret_cast<PP_GetInterface_Func>(
            library.GetFunctionPointer("PPP_GetInterface"));
    if (!plugin_entry_points_.get_interface) {
      LOG(WARNING) << "No PPP_GetInterface in plugin library";
      ReportLoadResult(path, ENTRY_POINT_MISSING);
      return;
    }

    // The ShutdownModule/ShutdownBroker function is optional.
    plugin_entry_points_.shutdown_module =
        is_broker_ ?
        reinterpret_cast<PP_ShutdownModule_Func>(
            library.GetFunctionPointer("PPP_ShutdownBroker")) :
        reinterpret_cast<PP_ShutdownModule_Func>(
            library.GetFunctionPointer("PPP_ShutdownModule"));

    if (!is_broker_) {
      // Get the InitializeModule function (required for non-broker code).
      plugin_entry_points_.initialize_module =
          reinterpret_cast<PP_InitializeModule_Func>(
              library.GetFunctionPointer("PPP_InitializeModule"));
      if (!plugin_entry_points_.initialize_module) {
        LOG(WARNING) << "No PPP_InitializeModule in plugin library";
        ReportLoadResult(path, ENTRY_POINT_MISSING);
        return;
      }
    }
  }

#if defined(OS_WIN)
  // If code subsequently tries to exit using abort(), force a crash (since
  // otherwise these would be silent terminations and fly under the radar).
  base::win::SetAbortBehaviorForCrashReporting();

  // Once we lower the token the sandbox is locked down and no new modules
  // can be loaded. TODO(cpu): consider changing to the loading style of
  // regular plugins.
  if (g_target_services) {
    // Let Flash and Widevine CDM adapter load DXVA before lockdown on Vista+.
    if (permissions.HasPermission(ppapi::PERMISSION_FLASH) ||
        path.BaseName().MaybeAsASCII() == kWidevineCdmAdapterFileName) {
      if (base::win::OSInfo::GetInstance()->version() >=
          base::win::VERSION_VISTA) {
        LoadLibraryA("dxva2.dll");
      }
    }

    if (permissions.HasPermission(ppapi::PERMISSION_FLASH)) {
      if (base::win::OSInfo::GetInstance()->version() >=
          base::win::VERSION_WIN7) {
        base::CPU cpu;
        if (cpu.vendor_name() == "AuthenticAMD") {
          // The AMD crypto acceleration is only AMD Bulldozer and above.
#if defined(_WIN64)
          LoadLibraryA("amdhcp64.dll");
#else
          LoadLibraryA("amdhcp32.dll");
#endif
        }
      }
    }

    // Cause advapi32 to load before the sandbox is turned on.
    unsigned int dummy_rand;
    rand_s(&dummy_rand);

    WarmupWindowsLocales(permissions);

    if (!base::win::IsUser32AndGdi32Available() &&
        permissions.HasPermission(ppapi::PERMISSION_FLASH)) {
      PatchGdiFontEnumeration(path);
    }

    g_target_services->LowerToken();
  }
#endif

  if (is_broker_) {
    // Get the InitializeBroker function (required).
    InitializeBrokerFunc init_broker =
        reinterpret_cast<InitializeBrokerFunc>(
            library.GetFunctionPointer("PPP_InitializeBroker"));
    if (!init_broker) {
      LOG(WARNING) << "No PPP_InitializeBroker in plugin library";
      ReportLoadResult(path, ENTRY_POINT_MISSING);
      return;
    }

    int32_t init_error = init_broker(&connect_instance_func_);
    if (init_error != PP_OK) {
      LOG(WARNING) << "InitBroker failed with error " << init_error;
      ReportLoadResult(path, INIT_FAILED);
      return;
    }
    if (!connect_instance_func_) {
      LOG(WARNING) << "InitBroker did not provide PP_ConnectInstance_Func";
      ReportLoadResult(path, INIT_FAILED);
      return;
    }
  } else {
#if defined(OS_MACOSX)
    // We need to do this after getting |PPP_GetInterface()| (or presumably
    // doing something nontrivial with the library), else the sandbox
    // intercedes.
    CHECK(InitializeSandbox());
#endif

    int32_t init_error = plugin_entry_points_.initialize_module(
        local_pp_module_,
        &ppapi::proxy::PluginDispatcher::GetBrowserInterface);
    if (init_error != PP_OK) {
      LOG(WARNING) << "InitModule failed with error " << init_error;
      ReportLoadResult(path, INIT_FAILED);
      return;
    }
  }

  // Initialization succeeded, so keep the plugin DLL loaded.
  library_.Reset(library.Release());

  ReportLoadResult(path, LOAD_SUCCESS);
}

void PpapiThread::OnCreateChannel(base::ProcessId renderer_pid,
                                  int renderer_child_id,
                                  bool incognito) {
  IPC::ChannelHandle channel_handle;

  if (!plugin_entry_points_.get_interface ||  // Plugin couldn't be loaded.
      !SetupRendererChannel(renderer_pid, renderer_child_id, incognito,
                            &channel_handle)) {
    Send(new PpapiHostMsg_ChannelCreated(IPC::ChannelHandle()));
    return;
  }

  Send(new PpapiHostMsg_ChannelCreated(channel_handle));
}

void PpapiThread::OnSetNetworkState(bool online) {
  // Note the browser-process side shouldn't send us these messages in the
  // first unless the plugin has dev permissions, so we don't need to check
  // again here. We don't want random plugins depending on this dev interface.
  if (!plugin_entry_points_.get_interface)
    return;
  const PPP_NetworkState_Dev* ns = static_cast<const PPP_NetworkState_Dev*>(
      plugin_entry_points_.get_interface(PPP_NETWORK_STATE_DEV_INTERFACE));
  if (ns)
    ns->SetOnLine(PP_FromBool(online));
}

void PpapiThread::OnCrash() {
  // Intentionally crash upon the request of the browser.
  //
  // Linker's ICF feature may merge this function with other functions with the
  // same definition and it may confuse the crash report processing system.
  static int static_variable_to_make_this_function_unique = 0;
  base::debug::Alias(&static_variable_to_make_this_function_unique);

  volatile int* null_pointer = nullptr;
  *null_pointer = 0;
}

void PpapiThread::OnHang() {
  // Intentionally hang upon the request of the browser.
  for (;;)
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
}

bool PpapiThread::SetupRendererChannel(base::ProcessId renderer_pid,
                                       int renderer_child_id,
                                       bool incognito,
                                       IPC::ChannelHandle* handle) {
  DCHECK(is_broker_ == (connect_instance_func_ != NULL));
  IPC::ChannelHandle plugin_handle;
  plugin_handle.name = IPC::Channel::GenerateVerifiedChannelID(
      base::StringPrintf(
          "%d.r%d", base::GetCurrentProcId(), renderer_child_id));

  ppapi::proxy::ProxyChannel* dispatcher = NULL;
  bool init_result = false;
  if (is_broker_) {
    BrokerProcessDispatcher* broker_dispatcher =
        new BrokerProcessDispatcher(plugin_entry_points_.get_interface,
                                    connect_instance_func_);
    init_result = broker_dispatcher->InitBrokerWithChannel(this,
                                                           renderer_pid,
                                                           plugin_handle,
                                                           false);
    dispatcher = broker_dispatcher;
  } else {
    PluginProcessDispatcher* plugin_dispatcher =
        new PluginProcessDispatcher(plugin_entry_points_.get_interface,
                                    permissions_,
                                    incognito);
    init_result = plugin_dispatcher->InitPluginWithChannel(this,
                                                           renderer_pid,
                                                           plugin_handle,
                                                           false);
    dispatcher = plugin_dispatcher;
  }

  if (!init_result) {
    delete dispatcher;
    return false;
  }

  handle->name = plugin_handle.name;
#if defined(OS_POSIX)
  // On POSIX, transfer ownership of the renderer-side (client) FD.
  // This ensures this process will be notified when it is closed even if a
  // connection is not established.
  handle->socket = base::FileDescriptor(dispatcher->TakeRendererFD());
  if (handle->socket.fd == -1)
    return false;
#endif

  // From here, the dispatcher will manage its own lifetime according to the
  // lifetime of the attached channel.
  return true;
}

void PpapiThread::SavePluginName(const base::FilePath& path) {
  ppapi::proxy::PluginGlobals::Get()->set_plugin_name(
      path.BaseName().AsUTF8Unsafe());
}

static std::string GetHistogramName(bool is_broker,
                                    const std::string& metric_name,
                                    const base::FilePath& path) {
  return std::string("Plugin.Ppapi") + (is_broker ? "Broker" : "Plugin") +
         metric_name + "_" + path.BaseName().MaybeAsASCII();
}

void PpapiThread::ReportLoadResult(const base::FilePath& path,
                                   LoadResult result) {
  DCHECK_LT(result, LOAD_RESULT_MAX);

  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::LinearHistogram::FactoryGet(
          GetHistogramName(is_broker_, "LoadResult", path),
          1,
          LOAD_RESULT_MAX,
          LOAD_RESULT_MAX + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->Add(result);
}

void PpapiThread::ReportLoadErrorCode(
    const base::FilePath& path,
    const base::NativeLibraryLoadError& error) {
// Only report load error code on Windows because that's the only platform that
// has a numerical error value.
#if defined(OS_WIN)
  // For sparse histograms, we can use the macro, as it does not incorporate a
  // static.
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      GetHistogramName(is_broker_, "LoadErrorCode", path), error.code);
#endif
}

void PpapiThread::ReportLoadTime(const base::FilePath& path,
                                 const base::TimeDelta load_time) {
  // Note: This leaks memory, which is expected behavior.
  base::HistogramBase* histogram =
      base::Histogram::FactoryTimeGet(
          GetHistogramName(is_broker_, "LoadTime", path),
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromSeconds(10),
          50,
          base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->AddTime(load_time);
}

}  // namespace content
