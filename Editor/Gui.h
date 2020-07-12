#ifndef _GUI_H_
#define _GUI_H_

#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>

using namespace std;

namespace UltraEd
{
    class Scene;

    class Gui
    {
    public:
        Gui(Scene *scene, HWND hWnd, IDirect3DDevice9 *device);
        ~Gui();
        void PrepareFrame();
        void RenderFrame();
        bool WantsMouse();
        void RebuildWith(function<void()> inner);

    private:
        void FileMenu();
        void ActorMenu();
        void ViewMenu();
        void GizmoMenu();

    private:
        Scene *m_scene;
        string m_buildOutput;
        bool m_moveBuildOutputToBottom;
    };
}

#endif
