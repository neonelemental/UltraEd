#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include "actor.h"
#include "settings.h"
#include "shlwapi.h"

using namespace std;

namespace UltraEd
{
    class CBuild
    {
    public:
        static bool Start(vector<CActor*> actors, HWND hWnd);
        static bool Run();
        static bool Load();
        static bool WriteSpecFile(vector<CActor*> actors);
        static bool WriteSegmentsFile(vector<CActor*> actors, map<string, string> *resourceCache);
        static bool WriteActorsFile(vector<CActor*> actors, const map<string, string> &resourceCache);
        static bool WriteCollisionFile(vector<CActor*> actors);
        static bool WriteScriptsFile(vector<CActor*> actors);
        static bool WriteMappingsFile(vector<CActor*> actors);

    private:
        static bool Compile(HWND hWnd);
        static string GetPathFor(string name);
    };
}
