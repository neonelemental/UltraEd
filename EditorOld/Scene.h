#pragma once

#include <array>
#include <map>
#include "deps/DXSDK/include/d3d8.h"
#include "deps/DXSDK/include/d3dx8.h"
#include "vendor/cJSON.h"
#include "View.h"
#include "Common.h"
#include "Debug.h"
#include "Gizmo.h"
#include "Grid.h"
#include "Model.h"
#include "Camera.h"
#include "Undo.h"

namespace UltraEd
{
    struct BuildFlag
    {
        enum Value { _, Run, Load };
    };

    class CScene : public CSavable
    {
    public:
        CScene();
        ~CScene();
        bool Create(HWND windowHandle);
        void Delete();
        void Duplicate();
        void FocusSelected();
        void SetScript(string script);
        string GetScript();
        void SetBackgroundColor(COLORREF color);
        COLORREF GetBackgroundColor();
        void Render();
        void Resize();       
        void OnMouseWheel(short zDelta);
        void OnNew(bool confirm = true);
        bool OnSave();
        void OnLoad();
        void OnAddCamera();
        void OnAddTexture();
        void OnDeleteTexture();
        void OnAddModel(ModelPreset::Value preset);
        void OnAddCollider(ColliderType::Value type);
        void OnDeleteCollider();
        void OnBuildROM(BuildFlag::Value flag);
        void Undo();
        void Redo();
        bool Pick(POINT mousePoint, CActor **selectedActor = NULL);
        void ReleaseResources(ModelRelease::Value type);
        void ScreenRaycast(POINT screenPoint, D3DXVECTOR3 *origin, D3DXVECTOR3 *dir);
        void SetViewType(ViewType::Value type);
        void SetGizmoModifier(GizmoModifierState::Value state);
        void SetGizmoSnapSize(float size);
        float GetGizmoSnapSize();
        CView *GetActiveView();
        bool ToggleMovementSpace();
        bool ToggleFillMode();
        bool ToggleSnapToGrid();
        void SelectActorById(GUID id, bool clearAll = true);
        void SelectAll();
        void UnselectAll();
        cJSON *Save();
        cJSON *PartialSave(cJSON *root);
        bool Load(cJSON *root);
        bool PartialLoad(cJSON *root);
        void SetDirty(bool value);
        bool Confirm();
        HWND GetWndHandle();
        vector<CActor *> GetActors();
        shared_ptr<CActor> GetActor(GUID id);
        void Delete(shared_ptr<CActor> actor);
        void RestoreActor(cJSON *item);
        void ResetViews();
        string GetStats();

    private:
        void CheckChanges();
        void CheckInput(const float deltaTime);
        void SetTitle(string title, bool store = true);
        void UpdateViewMatrix();
        void RefreshActorList();
        bool MouseInScene(const POINT &mousePoint);
        void WrapCursor();

    private:
        D3DLIGHT8 m_worldLight;
        D3DMATERIAL8 m_defaultMaterial;
        D3DMATERIAL8 m_selectedMaterial;
        D3DFILLMODE m_fillMode;
        CGizmo m_gizmo;
        CView m_views[4];
        IDirect3DDevice8 *m_device;
        IDirect3D8 *m_d3d8;
        D3DPRESENT_PARAMETERS m_d3dpp;
        map<GUID, shared_ptr<CActor>> m_actors;
        CGrid m_grid;
        vector<GUID> m_selectedActorIds;
        float m_mouseSmoothX, m_mouseSmoothY;
        ViewType::Value m_activeViewType;
        string m_sceneName;
        array<int, 3> m_backgroundColorRGB;
        CUndo m_undo;
    };
}
