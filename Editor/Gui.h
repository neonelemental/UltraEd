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
        ImGuiIO &IO();
        void RebuildWith(function<void()> inner);

    private:
        void FileMenu();
        void ActorMenu();
        void ViewMenu();
        void GizmoMenu();
        void BuildOutput();
        void SceneGraph();
        void OptionsModal();

    private:
        Scene *m_scene;
        string m_buildOutput;
        bool m_moveBuildOutputToBottom;
        int m_selectedActorIndex;
        int m_optionsModelOpen;
    };
}

#endif
