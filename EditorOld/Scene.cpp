#include "Build.h"
#include "Scene.h"
#include "FileIO.h"
#include "Dialog.h"
#include "Util.h"
#include "ResourceExt.h"
#include "BoxCollider.h"
#include "SphereCollider.h"

namespace UltraEd
{
    CScene::CScene() :
        m_worldLight(),
        m_defaultMaterial(),
        m_selectedMaterial(),
        m_fillMode(D3DFILL_SOLID),
        m_gizmo(),
        m_views(),
        m_device(0),
        m_d3d8(0),
        m_d3dpp(),
        m_actors(),
        m_grid(),
        m_selectedActorIds(),
        m_mouseSmoothX(0),
        m_mouseSmoothY(0),
        m_activeViewType(ViewType::Perspective),
        m_sceneName(),
        m_backgroundColorRGB(),
        m_undo(this)
    {
        m_defaultMaterial.Diffuse.r = m_defaultMaterial.Ambient.r = 1.0f;
        m_defaultMaterial.Diffuse.g = m_defaultMaterial.Ambient.g = 1.0f;
        m_defaultMaterial.Diffuse.b = m_defaultMaterial.Ambient.b = 1.0f;
        m_defaultMaterial.Diffuse.a = m_defaultMaterial.Ambient.a = 1.0f;

        m_selectedMaterial.Ambient.r = m_selectedMaterial.Emissive.r = 0.0f;
        m_selectedMaterial.Ambient.g = m_selectedMaterial.Emissive.g = 1.0f;
        m_selectedMaterial.Ambient.b = m_selectedMaterial.Emissive.b = 0.0f;
        m_selectedMaterial.Ambient.a = m_selectedMaterial.Emissive.a = 1.0f;

        m_worldLight.Type = D3DLIGHT_DIRECTIONAL;
        m_worldLight.Diffuse.r = 1.0f;
        m_worldLight.Diffuse.g = 1.0f;
        m_worldLight.Diffuse.b = 1.0f;
        m_worldLight.Direction = D3DXVECTOR3(0, 0, 1);
    }

    CScene::~CScene()
    {
        ReleaseResources(ModelRelease::AllResources);
        if (m_device) m_device->Release();
        if (m_d3d8) m_d3d8->Release();
    }

    bool CScene::Create(HWND windowHandle)
    {
        if ((m_d3d8 = Direct3DCreate8(D3D_SDK_VERSION)) == NULL) return false;

        D3DDISPLAYMODE d3ddm;
        if (FAILED(m_d3d8->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm))) return false;

        m_d3dpp.Windowed = TRUE;
        m_d3dpp.SwapEffect = D3DSWAPEFFECT_COPY_VSYNC;
        m_d3dpp.BackBufferFormat = d3ddm.Format;
        m_d3dpp.EnableAutoDepthStencil = TRUE;
        m_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

        if (FAILED(m_d3d8->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, windowHandle,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &m_d3dpp, &m_device))) return false;

        // Setup the new scene.
        OnNew();
        Resize();

        return true;
    }

    void CScene::OnNew(bool confirm)
    {
        if (confirm && !Confirm()) return;

        SetTitle("Untitled");
        UnselectAll();
        ReleaseResources(ModelRelease::AllResources);
        m_actors.clear();
        m_undo.Reset();
        ResetViews();
        RefreshActorList();
        m_backgroundColorRGB[0] = m_backgroundColorRGB[1] = m_backgroundColorRGB[2] = 0;
        m_gizmo.SetSnapSize(0.5f);
        SetDirty(false);
    }

    bool CScene::OnSave()
    {
        string savedName;
        if (CFileIO::Save(this, savedName))
        {
            SetTitle(savedName);
            SetDirty(false);
            return true;
        }

        return false;
    }

    void CScene::OnLoad()
    {
        if (!Confirm()) return;

        cJSON *root = NULL;
        string loadedName;
        if (CFileIO::Load(&root, loadedName))
        {
            OnNew(false);
            SetTitle(loadedName);
            Load(root);
            cJSON_Delete(root);
            RefreshActorList();
        }
    }

    void CScene::OnBuildROM(BuildFlag::Value flag)
    {
        if (CBuild::Start(this))
        {
            if (flag & BuildFlag::Run)
            {
                CBuild::Run();
            }
            else if (flag & BuildFlag::Load)
            {
                CBuild::Load(GetWndHandle());
            }
        }
        else
        {
            MessageBox(NULL, "The ROM build has failed. Make sure the build tools have been installed.", "Error", MB_OK);
        }
    }

    void CScene::OnAddModel(ModelPreset::Value preset)
    {
        shared_ptr<CModel> model = NULL;

        switch (preset)
        {
            case ModelPreset::Custom:
            {
                string file;
                if (CDialog::Open("Add Model",
                    "3D Studio (*.3ds)\0*.3ds\0Blender (*.blend)\0*.blend\0Autodesk (*.fbx)\0*.fbx\0"
                    "Collada (*.dae)\0*.dae\0DirectX (*.x)\0*.x\0Stl (*.stl)\0*.stl\0"
                    "VRML (*.wrl)\0*.wrl\0Wavefront (*.obj)\0*.obj", file))
                {
                    model = make_shared<CModel>(file.c_str());
                    m_actors[model->GetId()] = model;
                    model->SetName(string("Actor ").append(to_string(m_actors.size())));
                    m_undo.AddActor("Model", model->GetId());                    
                }
                break;
            }
            case ModelPreset::Pumpkin:
            {
                model = make_shared<CModel>("presets/pumpkin.fbx");
                m_actors[model->GetId()] = model;
                model->SetName(string("Pumpkin ").append(to_string(m_actors.size())));
                model->SetTexture(m_device, "presets/pumpkin.png");
                m_undo.AddActor("Pumpkin", model->GetId());
                break;
            }
        }
        
        if (model != NULL)
        {
            SelectActorById(model->GetId());
            RefreshActorList();
        }
    }

    void CScene::OnAddCamera()
    {
        auto newCamera = make_shared<CCamera>();
        m_actors[newCamera->GetId()] = newCamera;
        newCamera->SetName(string("Camera ").append(to_string(m_actors.size())));
        m_undo.AddActor("Camera", newCamera->GetId());

        SelectActorById(newCamera->GetId());
        RefreshActorList();
    }

    void CScene::OnAddCollider(ColliderType::Value type)
    {
        if (m_selectedActorIds.empty())
        {
            MessageBox(NULL, "An object must be selected first.", "Error", MB_OK);
        }

        GUID groupId = CUtil::NewGuid();

        for (const auto &selectedActorId : m_selectedActorIds)
        {
            m_undo.ChangeActor("Add Collider", selectedActorId, groupId);

            if (type == ColliderType::Box)
            {
                m_actors[selectedActorId]->SetCollider(new CBoxCollider(m_actors[selectedActorId]->GetVertices()));
            }
            else
            {
                m_actors[selectedActorId]->SetCollider(new CSphereCollider(m_actors[selectedActorId]->GetVertices()));
            }
        }
    }

    void CScene::OnDeleteCollider()
    {
        if (m_selectedActorIds.empty())
        {
            MessageBox(NULL, "An object must be selected first.", "Error", MB_OK);
        }

        GUID groupId = CUtil::NewGuid();

        for (const auto &selectedActorId : m_selectedActorIds)
        {
            m_undo.ChangeActor("Delete Collider", selectedActorId, groupId);

            m_actors[selectedActorId]->SetCollider(NULL);
        }
    }

    void CScene::OnAddTexture()
    {
        string file;

        if (m_selectedActorIds.empty())
        {
            MessageBox(NULL, "An actor must be selected first.", "Error", MB_OK);
            return;
        }

        if (CDialog::Open("Select a texture",
            "PNG (*.png)\0*.png\0JPEG (*.jpg)\0*.jpg\0BMP (*.bmp)\0*.bmp\0TGA (*.tga)\0*.tga", file))
        {
            GUID groupId = CUtil::NewGuid();

            for (const auto &selectedActorId : m_selectedActorIds)
            {
                if (m_actors[selectedActorId]->GetType() != ActorType::Model) continue;

                m_undo.ChangeActor("Add Texture", selectedActorId, groupId);

                if (!dynamic_cast<CModel *>(m_actors[selectedActorId].get())->SetTexture(m_device, file.c_str()))
                {
                    MessageBox(NULL, "Texture could not be loaded.", "Error", MB_OK);
                }
            }
        }
    }

    void CScene::OnDeleteTexture()
    {
        if (m_selectedActorIds.empty())
        {
            MessageBox(NULL, "An actor must be selected first.", "Error", MB_OK);
            return;
        }

        GUID groupId = CUtil::NewGuid();

        for (const auto &selectedActorId : m_selectedActorIds)
        {
            if (m_actors[selectedActorId]->GetType() != ActorType::Model) continue;

            m_undo.ChangeActor("Delete Texture", selectedActorId, groupId);

            dynamic_cast<CModel *>(m_actors[selectedActorId].get())->DeleteTexture();
        }
    }

    bool CScene::Pick(POINT mousePoint, CActor **selectedActor)
    {
        D3DXVECTOR3 orig, dir;
        ScreenRaycast(mousePoint, &orig, &dir);
        bool gizmoSelected = m_gizmo.Select(orig, dir);
        float closestDist = FLT_MAX;

        // When just selecting the gizmo don't check any actors.
        if (gizmoSelected && !m_selectedActorIds.empty()) return true;

        // Check all actors to see which poly might have been picked.
        for (const auto &actor : m_actors)
        {
            // Only choose the closest actors to the view.
            float pickDist = 0;
            if (actor.second->Pick(orig, dir, &pickDist) && pickDist < closestDist)
            {
                closestDist = pickDist;

                SelectActorById(actor.first, !(GetAsyncKeyState(VK_SHIFT) & 0x8000));

                if (selectedActor != NULL)
                    *selectedActor = actor.second.get();
            }
        }

        RefreshActorList();
        if (closestDist != FLT_MAX) return true;
        if (!gizmoSelected) UnselectAll();
        return false;
    }

    void CScene::CheckInput(const float deltaTime)
    {
        POINT mousePoint;
        GetCursorPos(&mousePoint);
        ScreenToClient(GetWndHandle(), &mousePoint);
        CView *view = GetActiveView();

        static POINT prevMousePoint = mousePoint;
        static DWORD prevTick = GetTickCount();
        static bool prevInScene = false;
        static bool prevGizmo = false;
        static GUID groupId = CUtil::NewGuid();

        const float smoothingModifier = 20.0f;
        const float mouseSpeedModifier = 0.55f;
        const bool mouseReady = GetTickCount() - prevTick < 100;

        // Only accept input when mouse in scene or when pressed mouse leaves scene.
        if (!prevInScene && !(prevInScene = MouseInScene(mousePoint))) return;

        WrapCursor();

        if (GetAsyncKeyState(VK_LBUTTON) && !m_selectedActorIds.empty())
        {
            D3DXVECTOR3 rayOrigin, rayDir;
            ScreenRaycast(mousePoint, &rayOrigin, &rayDir);
            if (prevGizmo || (prevGizmo = m_gizmo.Select(rayOrigin, rayDir)))
            {
                GUID lastSelectedActorId = m_selectedActorIds.back();
                for (const auto &selectedActorId : m_selectedActorIds)
                {
                    auto action = m_undo.PotentialChangeActor(m_gizmo.GetModifierName(), selectedActorId, groupId);

                    if (m_gizmo.Update(GetActiveView(), rayOrigin, rayDir, m_actors[selectedActorId].get(),
                        m_actors[lastSelectedActorId].get()))
                    {
                        action();
                    }
                }
            }
        }
        else if (GetAsyncKeyState(VK_RBUTTON) && m_activeViewType == ViewType::Perspective && mouseReady)
        {
            if (GetAsyncKeyState('W')) view->Walk(4.0f * deltaTime);
            if (GetAsyncKeyState('S')) view->Walk(-4.0f * deltaTime);
            if (GetAsyncKeyState('A')) view->Strafe(-4.0f * deltaTime);
            if (GetAsyncKeyState('D')) view->Strafe(4.0f * deltaTime);

            m_mouseSmoothX = CUtil::Lerp(deltaTime * smoothingModifier, m_mouseSmoothX, (FLOAT)(mousePoint.x - prevMousePoint.x));
            m_mouseSmoothY = CUtil::Lerp(deltaTime * smoothingModifier, m_mouseSmoothY, (FLOAT)(mousePoint.y - prevMousePoint.y));

            view->Yaw(m_mouseSmoothX * mouseSpeedModifier * deltaTime);
            view->Pitch(m_mouseSmoothY * mouseSpeedModifier * deltaTime);
        }
        else if (GetAsyncKeyState(VK_MBUTTON) && mouseReady)
        {
            m_mouseSmoothX = CUtil::Lerp(deltaTime * smoothingModifier, m_mouseSmoothX, (FLOAT)(prevMousePoint.x - mousePoint.x));
            m_mouseSmoothY = CUtil::Lerp(deltaTime * smoothingModifier, m_mouseSmoothY, (FLOAT)(mousePoint.y - prevMousePoint.y));

            view->Strafe(m_mouseSmoothX * deltaTime);
            view->Fly(m_mouseSmoothY * deltaTime);
        }
        else
        {
            // Reset smoothing values for new mouse view movement.
            m_mouseSmoothX = m_mouseSmoothY = 0;
            prevInScene = prevGizmo = false;
            groupId = CUtil::NewGuid();
            m_gizmo.Reset();
        }

        // Remember the last position so we know how much to move the view.
        prevMousePoint = mousePoint;
        prevTick = GetTickCount();
    }

    void CScene::Resize()
    {
        if (m_device)
        {
            ReleaseResources(ModelRelease::VertexBufferOnly);
            m_device->Reset(&m_d3dpp);
            UpdateViewMatrix();
        }
    }

    void CScene::UpdateViewMatrix()
    {
        D3DXMATRIX viewMat;
        RECT rect;
        GetClientRect(GetWndHandle(), &rect);
        const float aspect = (float)rect.right / (float)rect.bottom;

        if (m_activeViewType == ViewType::Perspective)
        {
            const float fov = D3DX_PI / 2.0f;
            D3DXMatrixPerspectiveFovLH(&viewMat, fov, aspect, 0.1f, 1000.0f);
        }
        else
        {
            const float zoom = GetActiveView()->GetZoom();
            D3DXMatrixOrthoLH(&viewMat, zoom * aspect, zoom, -1000.0f, 1000.0f);
        }

        m_device->SetTransform(D3DTS_PROJECTION, &viewMat);
    }

    void CScene::Render()
    {
        // Calculate the frame rendering speed.
        static double lastTime = (double)timeGetTime();
        const double currentTime = (double)timeGetTime();
        const float deltaTime = (float)(currentTime - lastTime) * 0.001f;

        CheckInput(deltaTime);
        CheckChanges();

        if (m_device)
        {
            ID3DXMatrixStack *stack;
            if (!SUCCEEDED(D3DXCreateMatrixStack(0, &stack))) return;
            stack->LoadMatrix(&GetActiveView()->GetViewMatrix());

            m_device->SetTransform(D3DTS_WORLD, stack->GetTop());
            m_device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                D3DCOLOR_XRGB(m_backgroundColorRGB[0], m_backgroundColorRGB[1], m_backgroundColorRGB[2]), 1.0f, 0);
            m_device->SetLight(0, &m_worldLight);
            m_device->LightEnable(0, TRUE);

            if (!SUCCEEDED(m_device->BeginScene())) return;

            m_grid.Render(m_device);
            CDebug::Instance()->Render(m_device);

            // Render all actors with selected fill mode.
            m_device->SetMaterial(&m_defaultMaterial);
            m_device->SetRenderState(D3DRS_ZENABLE, TRUE);
            m_device->SetRenderState(D3DRS_FILLMODE, m_fillMode);

            for (const auto &actor : m_actors)
            {
                actor.second->Render(m_device, stack);
            }

            if (!m_selectedActorIds.empty())
            {
                // Highlight the selected actor.
                m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
                for (const auto &selectedActorId : m_selectedActorIds)
                {
                    m_device->SetMaterial(&m_selectedMaterial);
                    m_actors[selectedActorId]->Render(m_device, stack);
                }

                // Draw the gizmo on "top" of all objects in scene.
                m_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
                m_device->SetRenderState(D3DRS_ZENABLE, FALSE);
                m_gizmo.Render(m_device, stack, GetActiveView());
            }

            m_device->EndScene();
            m_device->Present(NULL, NULL, NULL, NULL);
            stack->Release();
        }

        lastTime = currentTime;
    }

    void CScene::CheckChanges()
    {
        SetDirty(this->IsDirty());

        for (const auto &actor : m_actors)
        {
            if (actor.second->IsDirty())
            {
                SetDirty(true);
                break;
            }
        }
    }

    void CScene::OnMouseWheel(short zDelta)
    {
        GetActiveView()->SingleStep(zDelta);
        UpdateViewMatrix();
    }

    void CScene::ScreenRaycast(POINT screenPoint, D3DXVECTOR3 *origin, D3DXVECTOR3 *dir)
    {
        CView *view = GetActiveView();

        D3DVIEWPORT8 viewport;
        m_device->GetViewport(&viewport);

        D3DXMATRIX matProj;
        m_device->GetTransform(D3DTS_PROJECTION, &matProj);

        D3DXMATRIX matWorld;
        D3DXMatrixIdentity(&matWorld);

        D3DXVECTOR3 v1;
        D3DXVECTOR3 start = D3DXVECTOR3((FLOAT)screenPoint.x, (FLOAT)screenPoint.y, 0.0f);
        D3DXVec3Unproject(&v1, &start, &viewport, &matProj, &view->GetViewMatrix(), &matWorld);

        D3DXVECTOR3 v2;
        D3DXVECTOR3 end = D3DXVECTOR3((FLOAT)screenPoint.x, (FLOAT)screenPoint.y, 1.0f);
        D3DXVec3Unproject(&v2, &end, &viewport, &matProj, &view->GetViewMatrix(), &matWorld);

        *origin = v1;
        D3DXVec3Normalize(dir, &(v2 - v1));
    }

    CView *CScene::GetActiveView()
    {
        return &m_views[m_activeViewType];
    }

    void CScene::SetViewType(ViewType::Value type)
    {
        m_activeViewType = type;
        UpdateViewMatrix();
    }

    void CScene::ResetViews()
    {
        for (int i = 0; i < 4; i++)
        {
            m_views[i].Reset();
            switch (i)
            {
                case ViewType::Perspective:
                    m_views[i].Fly(2);
                    m_views[i].Walk(-5);
                    m_views[i].SetViewType(ViewType::Perspective);
                    break;
                case ViewType::Top:
                    m_views[i].Fly(12);
                    m_views[i].Pitch(D3DX_PI / 2);
                    m_views[i].SetViewType(ViewType::Top);
                    break;
                case ViewType::Left:
                    m_views[i].Yaw(D3DX_PI / 2);
                    m_views[i].Walk(-12);
                    m_views[i].SetViewType(ViewType::Left);
                    break;
                case ViewType::Front:
                    m_views[i].Yaw(D3DX_PI);
                    m_views[i].Walk(-12);
                    m_views[i].SetViewType(ViewType::Front);
                    break;
            }
        }
    }

    string CScene::GetStats()
    {
        size_t vertCount = 0;
        for (const auto &actor : m_actors)
        {
            if (actor.second->GetType() == ActorType::Model)
            {
                vertCount += actor.second->GetVertices().size();
            }
        }
        return string("Actors:").append(to_string(m_actors.size()))
            .append(" | Tris:").append(to_string(vertCount / 3));
    }

    void CScene::SetGizmoModifier(GizmoModifierState::Value state)
    {
        m_gizmo.SetModifier(state);
    }

    void CScene::SetGizmoSnapSize(float size)
    {
        m_gizmo.SetSnapSize(size);
    }

    float CScene::GetGizmoSnapSize()
    {
        return m_gizmo.GetSnapSize();
    }

    bool CScene::ToggleMovementSpace()
    {
        if (!m_selectedActorIds.empty())
        {
            return m_gizmo.ToggleSpace(m_actors[m_selectedActorIds.back()].get());
        }
        return false;
    }

    bool CScene::ToggleFillMode()
    {
        if (m_fillMode == D3DFILL_SOLID)
        {
            m_fillMode = D3DFILL_WIREFRAME;
            return false;
        }

        m_fillMode = D3DFILL_SOLID;
        return true;
    }

    void CScene::ReleaseResources(ModelRelease::Value type)
    {
        m_grid.Release();
        m_gizmo.Release();
        CDebug::Instance()->Release();
        for (const auto &actor : m_actors)
        {
            if (auto model = dynamic_cast<CModel *>(actor.second.get()))
            {
                model->Release(type);
            }
            else
            {
                actor.second->Release();
            }
        }
    }

    void CScene::Delete()
    {
        // Copy selected ids since loop modifies the master selected actor id vector.
        GUID groupId = CUtil::NewGuid();
        auto selectedActorIds = m_selectedActorIds;
        for (const auto &selectedActorId : selectedActorIds)
        {
            m_undo.DeleteActor("Actor", selectedActorId, groupId);
            Delete(m_actors[selectedActorId]);
            SetDirty(true);
        }

        RefreshActorList();
    }

    void CScene::Delete(shared_ptr<CActor> actor)
    {
        if (auto model = dynamic_cast<CModel *>(actor.get()))
        {
            model->Release(ModelRelease::AllResources);
        }
        else
        {
            actor->Release();
        }

        // Unselect actor if selected.
        auto it = find(m_selectedActorIds.begin(), m_selectedActorIds.end(), actor->GetId());
        if (it != m_selectedActorIds.end()) m_selectedActorIds.erase(it);

        m_actors.erase(actor->GetId());
    }

    void CScene::Duplicate()
    {
        GUID groupId = CUtil::NewGuid();

        for (const auto &selectedActorId : m_selectedActorIds)
        {
            switch (m_actors[selectedActorId]->GetType())
            {
                case ActorType::Model:
                {
                    auto model = make_shared<CModel>(*dynamic_cast<CModel *>(m_actors[selectedActorId].get()));
                    string texturePath = model->GetResources()["textureDataPath"];
                    model->SetTexture(m_device, texturePath.c_str());
                    m_actors[model->GetId()] = model;
                    m_undo.AddActor("Model", model->GetId(), groupId);
                    break;
                }
                case ActorType::Camera:
                {
                    auto camera = make_shared<CCamera>(*dynamic_cast<CCamera *>(m_actors[selectedActorId].get()));
                    m_actors[camera->GetId()] = camera;
                    m_undo.AddActor("Camera", camera->GetId(), groupId);
                    break;
                }
            }
        }

        RefreshActorList();
    }

    void CScene::FocusSelected()
    {
        if (m_selectedActorIds.size() > 0)
        {
            auto selectedActor = m_actors[m_selectedActorIds[0]];
            GetActiveView()->SetPosition(selectedActor->GetPosition() + (GetActiveView()->GetForward() * -2.5f));
        }
    }

    void CScene::SetScript(string script)
    {
        if (!m_selectedActorIds.empty())
        {
            m_undo.ChangeActor("Script Change", m_selectedActorIds[0]);
            m_actors[m_selectedActorIds[0]]->SetScript(script);
        }
    }

    string CScene::GetScript()
    {
        if (!m_selectedActorIds.empty())
        {
            return m_actors[m_selectedActorIds[0]]->GetScript();
        }
        return string("");
    }

    void CScene::SetBackgroundColor(COLORREF color)
    {
        m_undo.ChangeScene("Background Color");

        Dirty([&] {
            m_backgroundColorRGB[0] = GetRValue(color);
            m_backgroundColorRGB[1] = GetGValue(color);
            m_backgroundColorRGB[2] = GetBValue(color);
        }, &m_backgroundColorRGB);
    }

    COLORREF CScene::GetBackgroundColor()
    {
        return RGB(m_backgroundColorRGB[0], m_backgroundColorRGB[1], m_backgroundColorRGB[2]);
    }

    HWND CScene::GetWndHandle()
    {
        D3DDEVICE_CREATION_PARAMETERS params;
        if (m_device && SUCCEEDED(m_device->GetCreationParameters(&params)))
        {
            return params.hFocusWindow;
        }
        return NULL;
    }

    vector<CActor *> CScene::GetActors()
    {
        vector<CActor *> actors;
        for (const auto &actor : m_actors)
        {
            actors.push_back(actor.second.get());
        }
        return actors;
    }

    shared_ptr<CActor> CScene::GetActor(GUID id)
    {
        if (m_actors.find(id) != m_actors.end())
            return m_actors[id];
        return NULL;
    }

    bool CScene::ToggleSnapToGrid()
    {
        return m_gizmo.ToggleSnapping();
    }

    void CScene::RefreshActorList()
    {
        SendMessage(GetWndHandle(), WM_COMMAND, TV_CLEAR_ACTORS, 0);
        for (const auto &actor : m_actors)
        {
            SendMessage(GetWndHandle(), WM_COMMAND, TV_ADD_ACTOR, (LPARAM)actor.second.get());
        }
        for (const auto &selectedActorId : m_selectedActorIds)
        {
            SendMessage(GetWndHandle(), WM_COMMAND, TV_SELECT_ACTOR, (LPARAM)m_actors[selectedActorId].get());
        }
    }

    void CScene::SelectActorById(GUID id, bool clearAll)
    {
        if (clearAll) UnselectAll();

        auto it = find(m_selectedActorIds.begin(), m_selectedActorIds.end(), id);
        if (it != m_selectedActorIds.end())
        {
            // Unselect actor when already selected and clicked on again.
            m_selectedActorIds.erase(it);

            // Select previous selected actor if any available.
            if (!m_selectedActorIds.empty())
                m_gizmo.Update(m_actors[m_selectedActorIds.back()].get());
        }
        else
        {
            // Add to selection and move gizmo to its location.
            m_selectedActorIds.push_back(id);
            m_gizmo.Update(m_actors[id].get());
        }
    }

    void CScene::SelectAll()
    {
        UnselectAll();
        for (const auto &actor : m_actors)
        {
            SelectActorById(actor.first, false);
        }
    }

    void CScene::UnselectAll()
    {
        m_selectedActorIds.clear();
    }

    void CScene::SetTitle(string title, bool store)
    {
        HWND parentWnd = GetParent(GetWndHandle());
        if (parentWnd != NULL)
        {
            if (store) m_sceneName = title;
            title.append(" - ").append(APP_NAME);
            SetWindowText(parentWnd, title.c_str());
        }
    }

    void CScene::SetDirty(bool value)
    {
        CSavable::SetDirty(value);

        HWND parentWnd = GetParent(GetWndHandle());
        if (parentWnd != NULL)
        {
            string newSceneName(m_sceneName);
            if (value)
            {
                newSceneName.append("*");
            }
            SetTitle(newSceneName, false);
        }
    }

    bool CScene::Confirm()
    {
        if (IsDirty())
        {
            int choice = MessageBox(NULL, "Would you like to save your changes?", "Are you sure?", MB_YESNOCANCEL | MB_ICONQUESTION);
            switch (choice)
            {
                case IDCANCEL:
                    return false;
                case IDYES:
                    if (!OnSave()) return false;
            }
        }
        return true;
    }

    bool CScene::MouseInScene(const POINT &mousePoint)
    {
        if (GetActiveWindow() != GetParent(GetWndHandle())) return false;

        RECT rect = { 0 };
        GetClientRect(GetWndHandle(), &rect);
        return mousePoint.x > 0 && mousePoint.x < rect.right
            && mousePoint.y > 0 && mousePoint.y < rect.bottom;
    }

    void CScene::WrapCursor()
    {
        const int screenX = GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1;
        const int screenY = GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1;

        POINT mousePoint;
        GetCursorPos(&mousePoint);

        if (mousePoint.x >= screenX)
            SetCursorPos(1, mousePoint.y);
        else if (mousePoint.x < 1)
            SetCursorPos(screenX - 1, mousePoint.y);
        else if (mousePoint.y >= screenY)
            SetCursorPos(mousePoint.x, 1);
        else if (mousePoint.y < 1)
            SetCursorPos(mousePoint.x, screenY - 1);
    }

    void CScene::Undo()
    {
        m_undo.Undo();
        RefreshActorList();
    }

    void CScene::Redo()
    {
        m_undo.Redo();
        RefreshActorList();
    }

    cJSON *CScene::Save()
    {
        char buffer[128];
        cJSON *scene = cJSON_CreateObject();

        cJSON *viewArray = cJSON_CreateArray();
        cJSON_AddItemToObject(scene, "views", viewArray);
        for (int i = 0; i < 4; i++)
        {
            cJSON_AddItemToArray(viewArray, m_views[i].Save());
        }

        sprintf(buffer, "%i", (int)GetActiveView()->GetType());
        cJSON_AddStringToObject(scene, "active_view", buffer);

        cJSON *actorArray = cJSON_CreateArray();
        cJSON_AddItemToObject(scene, "actors", actorArray);
        for (const auto &actor : m_actors)
        {
            cJSON_AddItemToArray(actorArray, actor.second->Save());
        }

        PartialSave(scene);

        return scene;
    }

    cJSON *CScene::PartialSave(cJSON *root)
    {
        char buffer[128];
        cJSON *scene = NULL;

        if (root == NULL)
            scene = cJSON_CreateObject();
        else
            scene = root;

        sprintf(buffer, "%i %i %i", m_backgroundColorRGB[0], m_backgroundColorRGB[1],
            m_backgroundColorRGB[2]);
        cJSON_AddStringToObject(scene, "background_color", buffer);

        sprintf(buffer, "%f", m_gizmo.GetSnapSize());
        cJSON_AddStringToObject(scene, "gizmo_snap_size", buffer);

        return scene;
    }

    bool CScene::Load(cJSON *root)
    {
        // Restore editor views.
        int count = 0;
        cJSON *views = cJSON_GetObjectItem(root, "views");
        cJSON *viewItem = NULL;
        cJSON_ArrayForEach(viewItem, views)
        {
            m_views[count++].Load(viewItem);
        }

        // Set the active view.
        ViewType::Value viewType;
        cJSON *activeView = cJSON_GetObjectItem(root, "active_view");
        sscanf(activeView->valuestring, "%i", &viewType);
        SetViewType(viewType);

        // Restore saved actors.
        cJSON *actors = cJSON_GetObjectItem(root, "actors");
        cJSON *actor = NULL;
        cJSON_ArrayForEach(actor, actors)
        {
            RestoreActor(actor);
        }

        PartialLoad(root);

        return true;
    }

    bool CScene::PartialLoad(cJSON *root)
    {
        // Set the background color.
        cJSON *backgroundColor = cJSON_GetObjectItem(root, "background_color");
        sscanf(backgroundColor->valuestring, "%i %i %i", &m_backgroundColorRGB[0],
            &m_backgroundColorRGB[1], &m_backgroundColorRGB[2]);

        // Set gizmo snap size.
        float snapSize;
        cJSON *gizmoSnapSize = cJSON_GetObjectItem(root, "gizmo_snap_size");
        sscanf(gizmoSnapSize->valuestring, "%f", &snapSize);
        SetGizmoSnapSize(snapSize);

        return true;
    }

    void CScene::RestoreActor(cJSON *item)
    {
        // Avoid creation of new actor objects when restoring.
        auto existingActor = GetActor(CActor::GetId(item));
        switch (CActor::GetType(item))
        {
            case ActorType::Model:
            {
                auto model = existingActor ? static_pointer_cast<CModel>(existingActor) : make_shared<CModel>();
                model->Load(item, m_device);
                if (!existingActor) m_actors[model->GetId()] = model;
                break;
            }
            case ActorType::Camera:
            {
                auto camera = existingActor ? static_pointer_cast<CCamera>(existingActor) : make_shared<CCamera>();
                camera->Load(item);
                if (!existingActor) m_actors[camera->GetId()] = camera;
                break;
            }
        }
    }
}
