if(UNIX)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_search_module(SCDOC scdoc)
        if(SCDOC_FOUND)
            pkg_get_variable(SCDOC_SCDOC scdoc scdoc)
        endif()
    endif()
endif()

if(HORIZON_FORK)
    # Separate name, bundle id and data folder so the fork and Prism can be
    # installed side by side. Artwork is still Prism's: only the installed
    # names change, the input files keep their upstream names.
    set(Launcher_CommonName "HorizonPrismLauncher")
    set(Launcher_DisplayName "Horizon Prism Launcher")
    set(Launcher_AppID "dev.playhorizon.HorizonPrismLauncher")
    set(Launcher_Domain "playhorizon.dev")
    set(Launcher_Git "https://github.com/LuBart08/horizon-prism-launcher")
    set(Launcher_ENVName "HORIZON_PRISM")
    set(Launcher_Copyright "© 2026 horizon-prism contributors\\n© 2022-2026 Prism Launcher Contributors\\n© 2021-2022 PolyMC Contributors\\n© 2012-2021 MultiMC Contributors")
    set(Launcher_Copyright_Mac "© 2026 horizon-prism contributors, © 2022-2026 Prism Launcher Contributors, © 2021-2022 PolyMC Contributors and © 2012-2021 MultiMC Contributors")

    set(Launcher_Branding_Base "org.prismlauncher.PrismLauncher")
    set(Launcher_Binary_Base "prismlauncher")
    set(Launcher_Branding_Common "PrismLauncher")
else()
    set(Launcher_CommonName "PrismLauncher")
    set(Launcher_DisplayName "Prism Launcher")
    set(Launcher_AppID "org.prismlauncher.PrismLauncher")
    set(Launcher_Domain "prismlauncher.org")
    set(Launcher_Git "https://github.com/PrismLauncher/PrismLauncher")
    set(Launcher_ENVName "PRISMLAUNCHER")
    set(Launcher_Copyright "© 2022-2026 Prism Launcher Contributors\\n© 2021-2022 PolyMC Contributors\\n© 2012-2021 MultiMC Contributors")
    set(Launcher_Copyright_Mac "© 2022-2026 Prism Launcher Contributors, © 2021-2022 PolyMC Contributors and © 2012-2021 MultiMC Contributors")

    set(Launcher_Branding_Base "${Launcher_AppID}")
    set(Launcher_Binary_Base "${Launcher_APP_BINARY_NAME}")
    set(Launcher_Branding_Common "${Launcher_CommonName}")
endif()

set(Launcher_Name "${Launcher_CommonName}" PARENT_SCOPE)
set(Launcher_DisplayName "${Launcher_DisplayName}" PARENT_SCOPE)
set(Launcher_ENVName "${Launcher_ENVName}" PARENT_SCOPE)
set(Launcher_Domain "${Launcher_Domain}" PARENT_SCOPE)
set(Launcher_Git "${Launcher_Git}" PARENT_SCOPE)

set(Launcher_SVGFileName "${Launcher_AppID}.svg")
set(Launcher_Copyright "${Launcher_Copyright}" PARENT_SCOPE)
set(Launcher_Copyright_Mac "${Launcher_Copyright_Mac}" PARENT_SCOPE)
set(Launcher_UserAgent "${Launcher_CommonName}/${Launcher_VERSION_NAME}" PARENT_SCOPE)
set(Launcher_ConfigFile "${Launcher_APP_BINARY_NAME}.cfg" PARENT_SCOPE)
set(Launcher_AppID "${Launcher_AppID}" PARENT_SCOPE)
set(Launcher_SVGFileName "${Launcher_SVGFileName}" PARENT_SCOPE)

set(Launcher_Desktop "program_info/${Launcher_AppID}.desktop" PARENT_SCOPE)
set(Launcher_MIMEInfo "program_info/${Launcher_AppID}.xml" PARENT_SCOPE)
set(Launcher_MetaInfo "program_info/${Launcher_AppID}.metainfo.xml" PARENT_SCOPE)
set(Launcher_PNG_256 "program_info/${Launcher_Branding_Base}_256.png" PARENT_SCOPE)
set(Launcher_SVG "program_info/${Launcher_Branding_Base}.svg" PARENT_SCOPE)
set(Launcher_Branding_ICNS "program_info/${Launcher_Binary_Base}.icns" PARENT_SCOPE)
set(Launcher_Branding_MAC_ICON "program_info/${Launcher_Branding_Common}.icon" PARENT_SCOPE)
set(Launcher_Branding_ICO "program_info/${Launcher_APP_BINARY_NAME}.ico")
set(Launcher_Branding_ICO "${Launcher_Branding_ICO}" PARENT_SCOPE)
set(Launcher_Branding_WindowsRC "program_info/${Launcher_APP_BINARY_NAME}.rc" PARENT_SCOPE)
# main.cpp calls Q_INIT_RESOURCE(prismlauncher), so the Qt resource keeps its
# upstream file name even when the binary is renamed.
set(Launcher_Branding_LogoQRC "program_info/prismlauncher.qrc" PARENT_SCOPE)
if(HORIZON_FORK)
    set(Launcher_Authors "horizon-prism contributors & Prism Launcher Contributors")
else()
    set(Launcher_Authors "MultiMC & Prism Launcher Contributors")
endif()

set(Launcher_Portable_File "program_info/portable.txt" PARENT_SCOPE)

configure_file(${Launcher_Branding_Base}.desktop.in ${Launcher_AppID}.desktop)
configure_file(${Launcher_Branding_Base}.metainfo.xml.in ${Launcher_AppID}.metainfo.xml)
configure_file(${Launcher_Binary_Base}.rc.in ${Launcher_APP_BINARY_NAME}.rc @ONLY)
configure_file(${Launcher_Binary_Base}.qrc.in prismlauncher.qrc @ONLY)
configure_file(${Launcher_Binary_Base}.manifest.in ${Launcher_APP_BINARY_NAME}.manifest @ONLY)
configure_file(${Launcher_Binary_Base}.ico ${Launcher_APP_BINARY_NAME}.ico COPYONLY)
configure_file(${Launcher_Branding_Base}.svg ${Launcher_SVGFileName} COPYONLY)
configure_file(${Launcher_Branding_Base}.mime.xml ${Launcher_AppID}.xml COPYONLY)

if(MSVC)
    set(Launcher_MSVC_Redist_NSIS_Section [=[
!ifdef haveNScurl
Section "Visual Studio Runtime"
    Var /GLOBAL vc_redist_exe
    ${If} ${IsNativeARM64}
        StrCpy $vc_redist_exe "vc_redist.arm64.exe"
    ${Else}
        StrCpy $vc_redist_exe "vc_redist.x64.exe"
    ${EndIf}
    DetailPrint 'Downloading Microsoft Visual C++ Redistributable...'
    NScurl::http GET "https://aka.ms/vs/17/release/$vc_redist_exe" "$INSTDIR\vc_redist\$vc_redist_exe" /INSIST /CANCEL /Zone.Identifier /END
    Pop $0
    ${If} $0 == "OK"
        DetailPrint "Download successful"
        ExecWait "$INSTDIR\vc_redist\$vc_redist_exe /install /passive /norestart"
    ${Else}
        DetailPrint "Download failed with error $0"
    ${EndIf}
SectionEnd
!endif
]=])
endif()

configure_file(win_install.nsi.in win_install.nsi @ONLY)

if(SCDOC_FOUND)
    configure_file(${Launcher_Binary_Base}.6.scd.in ${Launcher_APP_BINARY_NAME}.6.scd @ONLY)

    set(in_scd "${CMAKE_CURRENT_BINARY_DIR}/${Launcher_APP_BINARY_NAME}.6.scd")
    set(out_man "${CMAKE_CURRENT_BINARY_DIR}/${Launcher_APP_BINARY_NAME}.6")
    add_custom_command(
        DEPENDS "${in_scd}"
        OUTPUT "${out_man}"
        COMMAND ${SCDOC_SCDOC} < "${in_scd}" > "${out_man}"
    )
    add_custom_target(man ALL DEPENDS ${out_man})
    set(Launcher_ManPage "program_info/${Launcher_APP_BINARY_NAME}.6" PARENT_SCOPE)
endif()
