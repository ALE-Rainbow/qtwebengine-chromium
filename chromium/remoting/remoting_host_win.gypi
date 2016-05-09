# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'remoting_host_installer_win_roots': [
      'host/installer/win/',
    ],
    'remoting_host_installer_win_files': [
      'host/installer/win/chromoting.wxs',
      'host/installer/win/parameters.json',
    ],
  },  # end of 'variables'

  'targets': [
    {
      # GN version: //remoting:remoting_breakpad_tester
      'target_name': 'remoting_breakpad_tester',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        'remoting_host',
      ],
      'sources': [
        'tools/breakpad_tester_win.cc',
      ],
    },  # end of target 'remoting_breakpad_tester'
    {
      # GN version: //remoting/host:remoting_lib_idl
      'target_name': 'remoting_lib_idl',
      'type': 'static_library',
      'variables': {
        'clang_warning_flags': [
          # MIDL generates code like "#endif !_MIDL_USE_GUIDDEF_"
          '-Wno-extra-tokens',
        ],
      },
      'sources': [
        'host/win/chromoting_lib_idl.templ',
        '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.h',
        '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.idl',
        '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib_i.c',
      ],
      # This target exports a hard dependency because dependent targets may
      # include chromoting_lib.h, a generated header.
      'hard_dependency': 1,
      'msvs_settings': {
        'VCMIDLTool': {
          'OutputDirectory': '<(SHARED_INTERMEDIATE_DIR)/remoting/host',
        },
      },
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'rules': [
        {
          # GN version: //remoting/host:generate_idl
          'rule_name': 'generate_idl',
          'extension': 'templ',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.idl',
          ],
          'action': [
            'python', '<(version_py_path)',
            '-e', "DAEMON_CONTROLLER_CLSID='<(daemon_controller_clsid)'",
            '-e', "RDP_DESKTOP_SESSION_CLSID='<(rdp_desktop_session_clsid)'",
            '<(RULE_INPUT_PATH)',
            '<@(_outputs)',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generating <@(_outputs)',
        },
      ],
    },  # end of target 'remoting_lib_idl'

    # remoting_lib_ps builds the proxy/stub code generated by MIDL (see
    # remoting_lib_idl).
    {
      # GN version: //remoting/host:remoting_lib_ps
      'target_name': 'remoting_lib_ps',
      'type': 'static_library',
      'defines': [
        # Prepend 'Ps' to the MIDL-generated routines. This includes
        # DllGetClassObject, DllCanUnloadNow, DllRegisterServer,
        # DllUnregisterServer, and DllMain.
        'ENTRY_PREFIX=Ps',
        'REGISTER_PROXY_DLL',
      ],
      'dependencies': [
        'remoting_lib_idl',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.dlldata.c',
        '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib_p.c',
      ],
      'variables': {
        'clang_warning_flags': [
          # MIDL generated code has a habit of omitting optional braces.
          '-Wno-missing-braces',
          # Source files generated by the MIDL compiler trigger warnings with
          # -Wincompatible-pointer-types enabled.
          '-Wno-incompatible-pointer-types',
          # Generated code contains unused variables.
          '-Wno-unused-variable',
          # PROXYFILE_LIST_START is an extern with initializer.
          '-Wno-extern-initializer',
        ],
      },
    },  # end of target 'remoting_lib_ps'

    # Regenerates 'chromoting_lib.rc' (used to embed 'chromoting_lib.tlb'
    # into remoting_core.dll's resources) every time
    # 'chromoting_lib_idl.templ' changes. Making remoting_core depend on
    # both this and 'remoting_lib_idl' targets ensures that the resorces
    # are rebuilt every time the type library is updated. GYP alone is
    # not smart enough to figure out this dependency on its own.
    {
      # We do not need remoting_lib_rc target anymore in GN, generate_idl and
      # remoting_lib_idl take care of chromoting_lib_idl.templ change.
      'target_name': 'remoting_lib_rc',
      'type': 'none',
      'sources': [
        'host/win/chromoting_lib_idl.templ',
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'rules': [
        {
          'rule_name': 'generate_rc',
          'extension': 'templ',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.rc',
          ],
          'action': [
            'echo 1 typelib "remoting/host/chromoting_lib.tlb" > <@(_outputs)',
          ],
          'message': 'Generating <@(_outputs)',
        },
      ],
    },  # end of target 'remoting_lib_rc'
    # The only difference between |remoting_console.exe| and
    # |remoting_host.exe| is that the former is a console application.
    # |remoting_console.exe| is used for debugging purposes.
    {
      # GN version: //remoting/host:remoting_console
      'target_name': 'remoting_console',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'BINARY=BINARY_HOST_ME2ME',
      ],
      'dependencies': [
        'remoting_core',
        'remoting_windows_resources',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
        'host/win/entry_point.cc',
      ],
      'msvs_settings': {
        'VCManifestTool': {
          'AdditionalManifestFiles': [
            'host/win/dpi_aware.manifest',
          ],
        },
        'VCLinkerTool': {
          'EntryPointSymbol': 'HostEntryPoint',
          'IgnoreAllDefaultLibraries': 'true',
          'SubSystem': '1', # /SUBSYSTEM:CONSOLE
        },
      },
    },  # end of target 'remoting_console'
    {
      # GN version: //remoting/host:remoting_core
      'target_name': 'remoting_core',
      'type': 'shared_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines' : [
        '_ATL_APARTMENT_THREADED',
        '_ATL_CSTRING_EXPLICIT_CONSTRUCTORS',
        '_ATL_NO_AUTOMATIC_NAMESPACE',
        '_ATL_NO_EXCEPTIONS',
        'BINARY=BINARY_CORE',
        'DAEMON_CONTROLLER_CLSID="{<(daemon_controller_clsid)}"',
        'RDP_DESKTOP_SESSION_CLSID="{<(rdp_desktop_session_clsid)}"',
        'HOST_IMPLEMENTATION',
        'ISOLATION_AWARE_ENABLED=1',
        'STRICT',
        'VERSION=<(version_full)',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_static',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        'remoting_base',
        'remoting_breakpad',
        'remoting_host',
        'remoting_host_setup_base',
        'remoting_it2me_host_static',
        'remoting_lib_idl',
        'remoting_lib_ps',
        'remoting_lib_rc',
        'remoting_me2me_host_static',
        'remoting_native_messaging_base',
        'remoting_protocol',
        'remoting_windows_resources',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/remoting/core.rc',
        '<(SHARED_INTERMEDIATE_DIR)/remoting/host/chromoting_lib.rc',
        '<(SHARED_INTERMEDIATE_DIR)/remoting/host/remoting_host_messages.rc',
        '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
        'host/desktop_process_main.cc',
        'host/host_main.cc',
        'host/host_main.h',
        'host/it2me/it2me_native_messaging_host_main.cc',
        'host/it2me/it2me_native_messaging_host_main.h',
        'host/security_key/remote_security_key_main.cc',
        'host/security_key/remote_security_key_main.h',
        'host/setup/me2me_native_messaging_host_main.cc',
        'host/setup/me2me_native_messaging_host_main.h',
        'host/win/chromoting_module.cc',
        'host/win/chromoting_module.h',
        'host/win/core.cc',
        'host/win/core_resource.h',
        'host/win/host_service.cc',
        'host/win/host_service.h',
        'host/win/omaha.cc',
        'host/win/omaha.h',
        'host/win/rdp_desktop_session.cc',
        'host/win/rdp_desktop_session.h',
        'host/win/unprivileged_process_delegate.cc',
        'host/win/unprivileged_process_delegate.h',
        'host/win/wts_session_process_delegate.cc',
        'host/win/wts_session_process_delegate.h',
        'host/worker_process_ipc_delegate.h',
      ],
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'true',
          'AdditionalManifestFiles': [
            'host/win/common-controls.manifest',
          ],
        },
        'VCLinkerTool': {
          'AdditionalDependencies': [
            'comctl32.lib',
            'rpcns4.lib',
            'rpcrt4.lib',
            'uuid.lib',
            'wtsapi32.lib',
          ],
          'AdditionalOptions': [
            # Export the proxy/stub entry points. Note that the generated
            # routines have 'Ps' prefix to avoid conflicts with our own
            # DllMain().
            '/EXPORT:DllGetClassObject=PsDllGetClassObject,PRIVATE',
            '/EXPORT:DllCanUnloadNow=PsDllCanUnloadNow,PRIVATE',
            '/EXPORT:DllRegisterServer=PsDllRegisterServer,PRIVATE',
            '/EXPORT:DllUnregisterServer=PsDllUnregisterServer,PRIVATE',
          ],
        },
        'conditions': [
          ['clang==1', {
            # atlapp.h contains a global "using namespace WTL;".
            # TODO: Remove once remoting/host/verify_config_window_win.h no
            # longer depends on atlapp.h, http://crbug.com/5027
            'VCCLCompilerTool': {
              'AdditionalOptions': ['-Wno-header-hygiene'],
            },
          }],
        ],
      },
    },  # end of target 'remoting_core'
    {
      # GN version: //remoting/host:remoting_desktop
      'target_name': 'remoting_desktop',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'BINARY=BINARY_DESKTOP',
      ],
      'dependencies': [
        'remoting_core',
        'remoting_windows_resources',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
        'host/win/entry_point.cc',
      ],
      'msvs_settings': {
        'VCManifestTool': {
          'AdditionalManifestFiles': [
            'host/win/dpi_aware.manifest',
          ],
        },
        'VCLinkerTool': {
          'EnableUAC': 'true',
          # Add 'level="requireAdministrator" uiAccess="true"' to
          # the manifest only for the official builds because it requires
          # the binary to be signed to work.
          'conditions': [
            ['buildtype == "Official"', {
              'UACExecutionLevel': 2,
              'UACUIAccess': 'true',
            }],
          ],
          'EntryPointSymbol': 'HostEntryPoint',
          'IgnoreAllDefaultLibraries': 'true',
          'SubSystem': '2', # /SUBSYSTEM:WINDOWS
        },
      },
    },  # end of target 'remoting_desktop'
    {
      # GN version: //remoting/host:remoting_me2me_host
      'target_name': 'remoting_me2me_host',
      'product_name': 'remoting_host',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'BINARY=BINARY_HOST_ME2ME',
      ],
      'dependencies': [
        'remoting_core',
        'remoting_windows_resources',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
        'host/win/entry_point.cc',
      ],
      'msvs_settings': {
        'VCManifestTool': {
          'AdditionalManifestFiles': [
            'host/win/dpi_aware.manifest',
          ],
        },
        'VCLinkerTool': {
          'EntryPointSymbol': 'HostEntryPoint',
          'IgnoreAllDefaultLibraries': 'true',
          'OutputFile': '$(OutDir)\\remoting_host.exe',
          'SubSystem': '2', # /SUBSYSTEM:WINDOWS
        },
      },
    },  # end of target 'remoting_me2me_host'
    {
      # GN version: //remoting/host:remote_security_key
      'target_name': 'remote_security_key',
      'type': 'executable',
      'product_name': 'remote_security_key',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines' : [
        'BINARY=BINARY_REMOTE_SECURITY_KEY',
      ],
      'dependencies': [
        'remoting_core',
        'remoting_windows_resources',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
        'host/security_key/remote_security_key_entry_point.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'IgnoreAllDefaultLibraries': 'true',
          'SubSystem': '1', # /SUBSYSTEM:CONSOLE
        },
      },
    },  # end of target 'remote_security_key'
    {
      # GN version: //remoting/host:remoting_me2me_native_messaging_host
      'target_name': 'remoting_me2me_native_messaging_host',
      'type': 'executable',
      'product_name': 'remoting_native_messaging_host',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines' : [
        'BINARY=BINARY_NATIVE_MESSAGING_HOST',
      ],
      'dependencies': [
        'remoting_core',
        'remoting_windows_resources',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
        'host/setup/me2me_native_messaging_host_entry_point.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'IgnoreAllDefaultLibraries': 'true',
          'SubSystem': '1', # /SUBSYSTEM:CONSOLE
        },
      },
    },  # end of target 'remoting_me2me_native_messaging_host'
    {
      # GN version: //remoting/host/it2me:remoting_assistance_host
      'target_name': 'remoting_it2me_native_messaging_host',
      'type': 'executable',
      'product_name': 'remote_assistance_host',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_core',
        'remoting_windows_resources',
      ],
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/remoting/version.rc',
        'host/it2me/it2me_native_messaging_host_entry_point.cc',
      ],
      'defines' : [
        'BINARY=BINARY_REMOTE_ASSISTANCE_HOST',
      ],
      'msvs_settings': {
        'VCManifestTool': {
          'EmbedManifest': 'true',
          'AdditionalManifestFiles': [
            'host/win/common-controls.manifest',
            'host/win/dpi_aware.manifest',
          ],
        },
        'VCLinkerTool': {
          'IgnoreAllDefaultLibraries': 'true',
          'SubSystem': '1', # /SUBSYSTEM:CONSOLE
          'AdditionalDependencies': [
            'comctl32.lib',
          ],
        },
      },
    },  # end of target 'remoting_it2me_native_messaging_host'
    {
      # GN version: //remoting/host:messages
      'target_name': 'remoting_host_messages',
      'type': 'none',
      'dependencies': [
        'remoting_resources',
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'sources': [
        'host/win/host_messages.mc.jinja2'
      ],
      'rules': [
        {
          'rule_name': 'localize',
          'extension': 'jinja2',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/host/remoting_host_messages.mc',
          ],
          'action': [
            'python', '<(remoting_localize_path)',
            '--locale_dir', '<(webapp_locale_dir)',
            '--template', '<(RULE_INPUT_PATH)',
            '--output', '<@(_outputs)',
            '--encoding', 'utf-16',
            '<@(remoting_locales)',
          ],
          'message': 'Localizing the event log messages'
        },
      ],
    },  # end of target 'remoting_host_messages'

    # Generates localized resources for the Windows binaries.
    # The substitution strings are taken from:
    #   - build/util/LASTCHANGE - the last source code revision. There is
    #       no explicit dependency on this file to avoid rebuilding the host
    #       after unrelated changes.
    #   - chrome/VERSION - the major, build & patch versions.
    #   - remoting/VERSION - the chromoting patch version (and overrides
    #       for chrome/VERSION).
    #   - translated webapp strings
    {
      # GN version: //remoting/host:remoting_windows_resources
      'target_name': 'remoting_windows_resources',
      'type': 'none',
      'dependencies': [
        'remoting_resources',
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'sources': [
        'host/win/core.rc.jinja2',
        'host/win/version.rc.jinja2',
      ],
      'rules': [
        {
          'rule_name': 'version',
          'extension': 'jinja2',
          'variables': {
            'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
          },
          'inputs': [
            '<(chrome_version_path)',
            '<(remoting_version_path)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/remoting/<(RULE_INPUT_ROOT)',
          ],
          'action': [
            'python', '<(remoting_localize_path)',
            '--variables', '<(chrome_version_path)',
            # |remoting_version_path| must be after |chrome_version_path|
            # because it can contain overrides for the version numbers.
            '--variables', '<(remoting_version_path)',
            '--variables', '<(lastchange_path)',
            '--locale_dir', '<(webapp_locale_dir)',
            '--template', '<(RULE_INPUT_PATH)',
            '--output', '<@(_outputs)',
            '--encoding', 'utf-16',
            '<@(remoting_locales)',
          ],
          'message': 'Localizing the version information'
        },
      ],
    },  # end of target 'remoting_windows_resources'
  ],  # end of 'targets'

  'conditions': [
    # The host installation is generated only if WiX is available. If
    # component build is used the produced installation will not work due to
    # missing DLLs. We build it anyway to make sure the GYP scripts are executed
    # by the bots.
    ['wix_exists == "True" and sas_dll_exists == "True"', {
      'targets': [
        {
          'target_name': 'remoting_host_installation',
          'type': 'none',
          'dependencies': [
            'remoting_me2me_host_archive',
          ],
          'sources': [
            '<(PRODUCT_DIR)/remoting-me2me-host-<(OS).zip',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/chromoting.msi',
          ],
          'rules': [
            {
              'rule_name': 'zip2msi',
              'extension': 'zip',
              'inputs': [
                'tools/zip2msi.py',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/chromoting.msi',
              ],
              'action': [
                'python', 'tools/zip2msi.py',
                '--wix_path', '<(wix_path)',
                '--intermediate_dir', '<(INTERMEDIATE_DIR)/installation',
                '--target_arch', '<(target_arch)',
                '<(RULE_INPUT_PATH)',
                '<@(_outputs)',
              ],
              'message': 'Generating <@(_outputs)',
            },
          ],
        },  # end of target 'remoting_host_installation'

        {
          'target_name': 'remoting_me2me_host_archive',
          'type': 'none',
          'dependencies': [
            '<(icu_gyp_path):icudata',
            'remote_security_key',
            'remoting_core',
            'remoting_desktop',
            'remoting_it2me_native_messaging_host',
            'remoting_me2me_host',
            'remoting_me2me_native_messaging_host',
            'remoting_native_messaging_manifests',
          ],
          'compiled_inputs': [
            '<(PRODUCT_DIR)/remote_assistance_host.exe',
            '<(PRODUCT_DIR)/remote_security_key.exe',
            '<(PRODUCT_DIR)/remoting_core.dll',
            '<(PRODUCT_DIR)/remoting_desktop.exe',
            '<(PRODUCT_DIR)/remoting_host.exe',
            '<(PRODUCT_DIR)/remoting_native_messaging_host.exe',
          ],
          'compiled_inputs_dst': [
            'files/remote_assistance_host.exe',
            'files/remote_security_key.exe',
            'files/remoting_core.dll',
            'files/remoting_desktop.exe',
            'files/remoting_host.exe',
            'files/remoting_native_messaging_host.exe',
          ],
          'conditions': [
            ['buildtype == "Official"', {
              'defs': [
                'OFFICIAL_BUILD=1',
              ],
            }, {  # else buildtype != "Official"
              'defs': [
                'OFFICIAL_BUILD=0',
              ],
            }],
          ],
          'defs': [
            'BRANDING=<(branding)',
            'DAEMON_CONTROLLER_CLSID={<(daemon_controller_clsid)}',
            'RDP_DESKTOP_SESSION_CLSID={<(rdp_desktop_session_clsid)}',
            'VERSION=<(version_full)',
          ],
          'generated_files': [
            '<@(_compiled_inputs)',
            '<(sas_dll_path)/sas.dll',
            '<(SHARED_INTERMEDIATE_DIR)/remoting/CREDITS.txt',
            '<(PRODUCT_DIR)/remoting/com.google.chrome.remote_assistance.json',
            '<(PRODUCT_DIR)/remoting/com.google.chrome.remote_desktop.json',
            'resources/chromoting.ico',
            '<(PRODUCT_DIR)/icudtl.dat',
          ],
          'generated_files_dst': [
            '<@(_compiled_inputs_dst)',
            'files/sas.dll',
            'files/CREDITS.txt',
            'files/com.google.chrome.remote_assistance.json',
            'files/com.google.chrome.remote_desktop.json',
            'files/chromoting.ico',
            'files/icudtl.dat',
          ],
          'zip_path': '<(PRODUCT_DIR)/remoting-me2me-host-<(OS).zip',
          'outputs': [
            '<(_zip_path)',
          ],
          'actions': [
            {
              'action_name': 'Zip installer files for signing',
              'temp_dir': '<(INTERMEDIATE_DIR)/installation',
              'source_files': [
                '<@(remoting_host_installer_win_files)',
              ],
              'inputs': [
                '<@(_compiled_inputs)',
                '<(sas_dll_path)/sas.dll',
                '<@(_source_files)',
                'host/installer/build-installer-archive.py',
                'resources/chromoting.ico',
              ],
              'outputs': [
                '<(_zip_path)',
              ],
              'action': [
                'python', 'host/installer/build-installer-archive.py',
                '<(_temp_dir)',
                '<(_zip_path)',
                '--source-file-roots', '<@(remoting_host_installer_win_roots)',
                '--source-files', '<@(_source_files)',
                '--generated-files', '<@(_generated_files)',
                '--generated-files-dst', '<@(_generated_files_dst)',
                '--defs', '<@(_defs)',
              ],
            },
          ],  # actions
        }, # end of target 'remoting_me2me_host_archive'
      ],  # end of 'targets'
    }, {
      # Dummy targets for when Wix is not available.
      'targets': [
        {
          'target_name': 'remoting_host_installation',
          'type': 'none',
        },

        {
          'target_name': 'remoting_me2me_host_archive',
          'type': 'none',
        },
      ],  # end of 'targets'
    }],  # 'wix_exists == "True" and sas_dll_exists == "True"'

  ],  # end of 'conditions'
}
