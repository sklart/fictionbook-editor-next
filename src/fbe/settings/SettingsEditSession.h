#pragma once
#include "../Settings.h"
#include "../RuntimeLocalization.h"

class SettingsEditSession
{
    CSettings& m_settings;
    struct Value { CString key; CProperty property; };
    std::vector<Value> m_values;
    std::vector<WordsItem> m_words;
    CString m_nbspChar, m_oldNbsp;
    bool m_restart, m_previousSuppression, m_accepted = false;
public:
    explicit SettingsEditSession(CSettings& settings) : m_settings(settings), m_words(settings.m_words),
        m_nbspChar(settings.m_nbsp_char), m_oldNbsp(settings.m_old_nbsp),
        m_restart(settings.m_need_restart), m_previousSuppression(settings.m_saveSuppressed)
    {
        std::vector<CString> keys; settings.GetProperties(keys);
        for(const CString& key : keys)
        {
            CProperty value;
            if(settings.GetPropertyValue(key, value) && (value.GetType() == Simple || value.GetType() == SimpleList))
                m_values.push_back({ key, value });
        }
        settings.m_saveSuppressed = true;
    }
    bool HasChanges()
    {
        for(auto& saved : m_values)
        {
            CProperty current; m_settings.GetPropertyValue(saved.key, current);
            if(current.GetStringValue() != saved.property.GetStringValue() ||
                current.GetStringList() != saved.property.GetStringList()) return true;
        }
        return false;
    }
    void Accept() { m_accepted = true; }
    ~SettingsEditSession()
    {
        if(!m_accepted)
        {
            for(auto& saved : m_values) m_settings.SetPropertyValue(saved.key, saved.property);
            m_settings.m_words = m_words;
            m_settings.m_nbsp_char = m_nbspChar;
            m_settings.m_old_nbsp = m_oldNbsp;
            m_settings.m_need_restart = m_restart;
            FbePublishRuntimeLocaleName(m_settings.GetInterfaceLocaleName());
            FbeResetRuntimeLocalization();
            ThemeManager::SetSelectedTheme(m_settings.GetInterfaceTheme());
            ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
        }
        m_settings.m_saveSuppressed = m_previousSuppression;
    }
    SettingsEditSession(const SettingsEditSession&) = delete;
    SettingsEditSession& operator=(const SettingsEditSession&) = delete;
};
