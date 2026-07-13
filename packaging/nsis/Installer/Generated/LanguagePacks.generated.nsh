; Языковые компоненты FictionBook Editor Next.
; Сгенерировано из localization/language-packs.json.
; Не редактируйте вручную: запускайте tools/localization/export-nsis-language-pack-plan.ps1.

; Fallback-язык: en-US
; Текущие языки интерфейса установщика: en-US, ru-RU, uk-UA

SectionGroup /e $(LanguagePacksGroup) LanguagePacksGroup_id

  Section "English (en-US)" LanguagePack_en_US
    ; required=True; defaultInstall=True; installerLanguage=English
    SectionIn RO
    SetOutPath "$INSTDIR\en-US"
    File /nonfatal "${INPUTDIR}\en-US\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\en-US"
    File /nonfatal /r "${INPUTDIR}\Lang\en-US\*.*"
  SectionEnd

  Section "Русский (ru-RU)" LanguagePack_ru_RU
    ; required=False; defaultInstall=True; installerLanguage=Russian
    SetOutPath "$INSTDIR"
    File /nonfatal "${INPUTDIR}\res_rus.dll"
    SetOutPath "$INSTDIR\ru-RU"
    File /nonfatal "${INPUTDIR}\ru-RU\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\ru-RU"
    File /nonfatal /r "${INPUTDIR}\Lang\ru-RU\*.*"
  SectionEnd

  Section "Українська (uk-UA)" LanguagePack_uk_UA
    ; required=False; defaultInstall=True; installerLanguage=Ukrainian
    SetOutPath "$INSTDIR"
    File /nonfatal "${INPUTDIR}\res_ukr.dll"
    SetOutPath "$INSTDIR\uk-UA"
    File /nonfatal "${INPUTDIR}\uk-UA\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\uk-UA"
    File /nonfatal /r "${INPUTDIR}\Lang\uk-UA\*.*"
  SectionEnd

  Section /o "Deutsch (de-DE)" LanguagePack_de_DE
    ; required=False; defaultInstall=False; installerLanguage=
    SetOutPath "$INSTDIR\de-DE"
    File /nonfatal "${INPUTDIR}\de-DE\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\de-DE"
    File /nonfatal /r "${INPUTDIR}\Lang\de-DE\*.*"
  SectionEnd

  Section /o "Français (fr-FR)" LanguagePack_fr_FR
    ; required=False; defaultInstall=False; installerLanguage=
    SetOutPath "$INSTDIR\fr-FR"
    File /nonfatal "${INPUTDIR}\fr-FR\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\fr-FR"
    File /nonfatal /r "${INPUTDIR}\Lang\fr-FR\*.*"
  SectionEnd

  Section /o "Español (es-ES)" LanguagePack_es_ES
    ; required=False; defaultInstall=False; installerLanguage=
    SetOutPath "$INSTDIR\es-ES"
    File /nonfatal "${INPUTDIR}\es-ES\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\es-ES"
    File /nonfatal /r "${INPUTDIR}\Lang\es-ES\*.*"
  SectionEnd

  Section /o "Italiano (it-IT)" LanguagePack_it_IT
    ; required=False; defaultInstall=False; installerLanguage=
    SetOutPath "$INSTDIR\it-IT"
    File /nonfatal "${INPUTDIR}\it-IT\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\it-IT"
    File /nonfatal /r "${INPUTDIR}\Lang\it-IT\*.*"
  SectionEnd

  Section /o "Polski (pl-PL)" LanguagePack_pl_PL
    ; required=False; defaultInstall=False; installerLanguage=
    SetOutPath "$INSTDIR\pl-PL"
    File /nonfatal "${INPUTDIR}\pl-PL\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\pl-PL"
    File /nonfatal /r "${INPUTDIR}\Lang\pl-PL\*.*"
  SectionEnd

  Section /o "Português (pt-PT)" LanguagePack_pt_PT
    ; required=False; defaultInstall=False; installerLanguage=
    SetOutPath "$INSTDIR\pt-PT"
    File /nonfatal "${INPUTDIR}\pt-PT\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\pt-PT"
    File /nonfatal /r "${INPUTDIR}\Lang\pt-PT\*.*"
  SectionEnd

  Section /o "Nederlands (nl-NL)" LanguagePack_nl_NL
    ; required=False; defaultInstall=False; installerLanguage=
    SetOutPath "$INSTDIR\nl-NL"
    File /nonfatal "${INPUTDIR}\nl-NL\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\nl-NL"
    File /nonfatal /r "${INPUTDIR}\Lang\nl-NL\*.*"
  SectionEnd

  Section /o "Čeština (cs-CZ)" LanguagePack_cs_CZ
    ; required=False; defaultInstall=False; installerLanguage=
    SetOutPath "$INSTDIR\cs-CZ"
    File /nonfatal "${INPUTDIR}\cs-CZ\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\cs-CZ"
    File /nonfatal /r "${INPUTDIR}\Lang\cs-CZ\*.*"
  SectionEnd

  Section /o "Български (bg-BG)" LanguagePack_bg_BG
    ; required=False; defaultInstall=False; installerLanguage=
    SetOutPath "$INSTDIR\bg-BG"
    File /nonfatal "${INPUTDIR}\bg-BG\FBVVerbResources.dll.mui"
    SetOutPath "$INSTDIR\Lang\bg-BG"
    File /nonfatal /r "${INPUTDIR}\Lang\bg-BG\*.*"
  SectionEnd

SectionGroupEnd
