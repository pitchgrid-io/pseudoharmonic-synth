#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <algorithm>

// Simple file-backed preset store.  Presets are JSON text files ("*.phpreset")
// in the user application data directory; a small factory library is seeded on
// first run.  The processor builds/applies the JSON (params + mod config).
class PresetManager
{
public:
    PresetManager()
    {
        dir_ = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("PitchGrid")
                   .getChildFile("PseudoHarmonic")
                   .getChildFile("Presets");
        dir_.createDirectory();
        seedFactory();
    }

    // Names (without extension) of all presets, sorted.
    std::vector<std::string> list() const
    {
        std::vector<std::string> names;
        for (auto& f : dir_.findChildFiles(juce::File::findFiles, false, "*.phpreset"))
            names.push_back(f.getFileNameWithoutExtension().toStdString());
        std::sort(names.begin(), names.end());
        return names;
    }

    bool save(const std::string& name, const std::string& jsonText) const
    {
        if (name.empty()) return false;
        auto f = fileFor(name);
        return f.replaceWithText(juce::String(jsonText));
    }

    // Returns the JSON text of a preset, or empty string if not found.
    std::string load(const std::string& name) const
    {
        auto f = fileFor(name);
        return f.existsAsFile() ? f.loadFileAsString().toStdString() : std::string();
    }

    bool remove(const std::string& name) const
    {
        auto f = fileFor(name);
        return f.existsAsFile() && f.deleteFile();
    }

private:
    juce::File fileFor(const std::string& name) const
    {
        auto safe = juce::File::createLegalFileName(juce::String(name));
        return dir_.getChildFile(safe + ".phpreset");
    }

    void seedFactory();   // defined in .cpp — writes built-in presets if absent

    juce::File dir_;
};
