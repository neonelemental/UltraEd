#ifndef _GUI_H_
#define _GUI_H_

#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>
#include "Actor.h"
#include <ImGui/Plugins/TextEditor.h>

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
        void EditMenu();
        void ActorMenu();
        void ViewMenu();
        void GizmoMenu();
        void BuildOutput();
        void SceneGraph();
        void ActorProperties();
        void OptionsModal();
        void SceneSettingsModal();
        void ContextMenu();
        void ScriptEditor();

    private:
        Scene *m_scene;
        TextEditor m_textEditor;
        string m_buildOutput;
        bool m_moveBuildOutputToBottom;
        bool m_openContextMenu;
        bool m_textEditorOpen;
        int m_selectedActorIndex;
        int m_optionsModalOpen;
        int m_sceneSettingsModalOpen;
        Actor *m_selectedActor;
    };
}

#endif
